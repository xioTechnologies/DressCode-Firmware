/*
    AudioIn.c
    Author: Seb Madgwick, Vincent Akkermans

	Conversion is 14 TAD for 12 bit results
	14 TAD per 1 sample
	FOSC = 8 MHz (always internal)
	PLL is on, 4x
	FCY = 4 * 8 MHz / 2 = 16 MHz 
	TCY = 1/FCY = 62.5 ns

	TAD = 1/AD_CLK = sample period
	TAD = TCY * (ADCS + 1)
	TAD = TCY * (63 + 1) = 4 uS (for ADCS == 63)
	AD_CLK = 250 kHz

	SR = 1 / ((14 + SAMC) * TAD) = 17857.14
	SR = 250000 / (14 + SAMC)
    SR = 250000 / (14 + 17) = 8064.516129032

	TODO:
	- implement onset detection
	- if using SF for onset detection, reduce sample rate of LPF?


	Please note:
	Dynamically allocated memory is not freed anywhere as it will be used throughout the 
	applications running.
*/

//------------------------------------------------------------------------------
// Includes

#include "AudioIn.h"
#include "Q15utils/Q15utils.h"
#include <p24Fxxxx.h>
#include <math.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
// Definitions

#define PI 3.141592f
#define TWO_PI 6.283185f

#define SR 8064.516f
#define GAIN_ADJUSTMENT_SAMPLES 8065 // don't touch the gain for this many samples after updating it
#define ONSET_WINDOW_SIZE 5

#define ADC_ADCS 63
#define ADC_SAMC 17

typedef enum {
    GAIN_1,
    GAIN_4,
    GAIN_16,
    GAIN_25,
    GAIN_64,
    GAIN_100,
    GAIN_256,
    GAIN_1024,
    INVALID
} PreampGain;

struct Envelope {
	_Q15 attackCoeff;
	_Q15 releaseCoeff;
	_Q15 envelope;
};

typedef struct OnePoleFilter {
	_Q15 a0;
	_Q15 y;
	_Q15 b1;
} OnePoleFilter;

/* TODO: code to set coefficients
typedef struct TwoPoleFilter {
	_Q15 a0;
	_Q15 y;
	_Q15 y2;
	_Q15 b1;
	_Q15 b2;
} TwoPoleFilter;
*/

typedef struct Goertzel {
	unsigned int n, i; // n is blocksize, i is index in the current block
	_Q16 q0, q1, q2;
	_Q16 c;
	_Q16 magnitude;
	_Q16 magScaler;
} Goertzel;

// scaler is the value by which the accumulated magnitude of all the bins is multiplied.
// scaler = 1 / size
struct GoertzelArray {
	struct Goertzel *gs;
	unsigned int size;
	int ready;
	_Q16 scaler; 
};

struct SpectralFluxOnsetDetector {
	GoertzelArray *ga;
	_Q16 *prevMagnitudes;
	_Q16 *fluxes;
	unsigned int i; // last index written in.
	int ready;
};


//------------------------------------------------------------------------------
// Variables

GoertzelArray lowG;
GoertzelArray midG;
GoertzelArray hiG;

_Q15 signal;

SpectralFluxOnsetDetector lowSF;
SpectralFluxOnsetDetector midSF;
SpectralFluxOnsetDetector hiSF;

static Envelope gainEnv;
Envelope env;
/*
static Envelope lpfEnv;
static Envelope hpfEnv;
*/
static OnePoleFilter biasLPF;
/*
static OnePoleFilter lpf;
static OnePoleFilter hpf;
*/
static PreampGain preampGain = GAIN_1;
static unsigned int gainAdjustmentSamples = 0;

static _Q16 q16one;
static _Q16 q16minOne;

static int readyP = 0;

//------------------------------------------------------------------------------
// Function declarations

static void setHardwareGain();
static _Q15 getEnvelopeCoefficient(unsigned int ms);
static void setEnvelope(Envelope *env, unsigned int attack, unsigned int release);
static void updateEnvelope(Envelope *env, _Q15 inAbs); 
static float getFilterX(float fc);
static void setLPF(OnePoleFilter *filter, float fc);
static void updateOnePoleFilter(OnePoleFilter *filter, _Q15 x);
//static void updateTwoPoleFilter(TwoPoleFilter *filter, _Q15 x);
static void setGoertzel(Goertzel *g, unsigned int blockSize, float frequency);
static int updateGoertzel(Goertzel *g, _Q16 x);
static void setGoertzelArray(GoertzelArray *ga, unsigned int blockSize, float startFrequency, float endFrequency);
static int updateGoertzelArray(GoertzelArray *ga, _Q15 x);
static void setSpectralFlux(SpectralFluxOnsetDetector *sf, GoertzelArray *ga);
static int updateSpectralFlux(SpectralFluxOnsetDetector *sf, _Q15 x);

//------------------------------------------------------------------------------
// Functions

void SPI1Init(void) {
    //config SPI1
    SPI1STATbits.SPIEN 		= 0;	// disable SPI port
    SPI1STATbits.SPISIDL 	= 0; 	// Continue module operation in Idle mode

    SPI1BUF 				= 0;   	// clear SPI buffer

    IFS0bits.SPI1IF 		= 0;	// clear interrupt flag
    IEC0bits.SPI1IE 		= 0;	// disable interrupt

    SPI1CON1bits.DISSCK		= 0;	// Internal SPIx clock is enabled
    SPI1CON1bits.DISSDO		= 0;	// SDOx pin is controlled by the module
    SPI1CON1bits.MODE16 	= 1;	// set in 16-bit mode, clear in 8-bit mode
    SPI1CON1bits.SMP		= 0;	// Input data sampled at middle of data output time
    SPI1CON1bits.CKP 		= 0;	// CKP and CKE is subject to change ...
    SPI1CON1bits.CKE 		= 1;	// ... based on your communication mode.
	SPI1CON1bits.MSTEN 		= 1; 	// 1 =  Master mode; 0 =  Slave mode
	SPI1CON1bits.SPRE 		= 4; 	// Secondary Prescaler = 4:1
	SPI1CON1bits.PPRE 		= 2; 	// Primary Prescaler = 4:1

    SPI1CON2 				= 0;	// non-framed mode

	PORTBbits.RB15			= 1;
	TRISBbits.TRISB15		= 0;	// set SS as output

    SPI1STATbits.SPIEN 		= 1; 	// enable SPI port, clear status
}


void AudioInit(void) {
	
	q16one = _Q16ftoi(1.0f);
	q16minOne = _Q16ftoi(-1.0f);

    // Setup ADC
	AD1CON1bits.MODE12 = 1;
	AD1CON1bits.FORM = 3;
    AD1CON1bits.SSRC = 0b0111;  // Internal counter ends sampling and starts conversion (auto-convert)
    AD1CON1bits.ASAM = 1;       // Sampling begins immediately after the last conversion; SAMP bit is auto-set
    AD1CON2bits.PVCFG = 1;   	// External VREF+
	AD1CON2bits.NVCFG = 0;		// VREF- is Vss
	AD1CON2bits.CSCNA = 0;
    AD1CON2bits.SMPI = 0;       // Interrupts at the completion of the conversion for each 16th sample
	AD1CON2bits.ALTS = 0;
	AD1CON2bits.BUFREGEN = 1;
    AD1CON3bits.SAMC = ADC_SAMC;
    AD1CON3bits.ADCS = ADC_ADCS;
    AD1CHSbits.CH0SA = 1;       // Sample A Channel 0 Positive Input = AN1
    _AD1IP = 7;                 // set interrupt priority
    _AD1IF = 0;                 // clear interrupt flag
    _AD1IE = 1;                 // enable interrupt

	setEnvelope(&gainEnv, 100, 1000);
	setEnvelope(&env, 20, 500);
	/*
	setEnvelope(&lpfEnv, 20, 500);
	setEnvelope(&hpfEnv, 20, 500);

	setLPF(&lpf, 70.0f);
	setHPF(&hpf, 300.0f);
	*/
	setLPF(&biasLPF, 5.0f);

	// These arrays together make up 10 Goertzels, which should take up 100 bytes on the stack.
	setGoertzelArray(&lowG, 100, 0.0f, 80.0f); // 80Hz updates and 0 Hz and 80 Hz bins.
	setGoertzelArray(&midG, 100, 160.0f, 250.0f); // voice frequency range (female): 80Hz updates and bins at 160 Hz and 240Hz
	setGoertzelArray(&hiG, 100, 320.f, 800.0f); // catch high frequencies

	setSpectralFlux(&lowSF, &lowG);
	setSpectralFlux(&midSF, &midG);
	setSpectralFlux(&hiSF, &hiG);

    // Ensure default preamp gain
	setHardwareGain();

	AD1CON1bits.ADON = 1;       // A/D Converter module is operating

}

// N.B. This resets the flag!
int AU_readyP(void) {
	static int old;
	old = readyP;
	readyP = 0;
	return old;
}

int GA_readyP(GoertzelArray *ga) {
	return ga->ready;
}

_Q16 GA_getMagnitude(GoertzelArray *ga) {
	ga->ready = 0;
	_Q16 acc = 0;
	unsigned int i;
	for(i=0; i<ga->size; i++) {
		//acc = _Q16mac(ga->scaler, ga->gs[i].magnitude, acc);
		acc = ga->gs[i].magnitude + acc;
	}
	return acc;
}

_Q15 ENV_getMagnitude(Envelope *e) {
	return e->envelope;
}

int SF_readyP(SpectralFluxOnsetDetector *sf) {
	return sf->ready;
}

_Q16 SF_getFlux(SpectralFluxOnsetDetector *sf) {
	// sf->ready = 0; // N.B. don't need to set ready flag, done in update
	return sf->fluxes[sf->i];
}

// TODO: simply return result of boolean comparison.
int SF_getOnset(SpectralFluxOnsetDetector *sf) {
	unsigned int checks=0, i=0, onsetIndex;
	// we want to check whether 2 windows ago we had an onset.
	// keep onsetIndex % ONSET_WINDOW_SIZE
	onsetIndex = (sf->i + (ONSET_WINDOW_SIZE / 2 + 1));
	if(onsetIndex >= ONSET_WINDOW_SIZE) {
		onsetIndex -= ONSET_WINDOW_SIZE;
	}
	for(;i<ONSET_WINDOW_SIZE;i++) {
		checks += sf->fluxes[onsetIndex] >= sf->fluxes[i];
	}
	checks = checks == ONSET_WINDOW_SIZE; // TODO: skip this after debugging
	return checks;
}


//------------------------------------------------------------------------------
// Functions - ISRs

void __attribute__((interrupt, auto_psv))_ADC1Interrupt(void) {
	static _Q15 adc;
	static _Q15 abs;

    // Get ADC result
	adc = ADC1BUF1;
	
	signal = _Q15sub(adc, 0x4000);
	updateOnePoleFilter(&biasLPF, signal);
	signal = _Q15sub(signal, biasLPF.y); // inverting the filter

	abs = _Q15abs(signal);

	updateEnvelope(&env, abs);
	updateEnvelope(&gainEnv, abs);

	/*
	updateOnePoleFilter(&lpf, signal);
	updateOnePoleFilter(&hpf, signal);

	updateEnvelope(&lpfEnv, _Q15abs(lpf.y));
	updateEnvelope(&hpfEnv, _Q15abs(hpf.y));

	updateGoertzelArray(&lowG, signal);
	updateGoertzelArray(&midG, signal);
	updateGoertzelArray(&hiG, signal);
	*/

	updateSpectralFlux(&lowSF, signal);
	updateSpectralFlux(&midSF, signal);
	updateSpectralFlux(&hiSF, signal);

	setHardwareGain();

	readyP = 1;
    _AD1IF = 0;     // clear interrupt flag
}


static void setHardwareGain() {	
	if(gainAdjustmentSamples > 0) {
		gainAdjustmentSamples--;
		return;
	} 
	else {
		gainAdjustmentSamples = GAIN_ADJUSTMENT_SAMPLES;
	}

	if(gainEnv.envelope < 1024) {
		if(preampGain != GAIN_1024)
			preampGain++;
	}
	else if(gainEnv.envelope > 8192) {
		if(preampGain != GAIN_1)
			preampGain--;
	}
	// we're good
	else
		return;

   	LATBbits.LATB15 = 0; // assert chip select
	unsigned int gain = preampGain;
	unsigned int data = 0b0100000000000000 | gain;
    SPI1BUF = data;
    while(!SPI1STATbits.SPIRBF); // wait for transmit to complete by checking receive
	int i = SPI1BUF;
  	LATBbits.LATB15 = 1; // chip select idle

	return;
}

static _Q15 getEnvelopeCoefficient(unsigned int ms) {
	return _Q15ftoi(powf(0.01f, 1.0f/(((float)ms) * SR * 0.001f)));
}

static void setEnvelope(Envelope *env, unsigned int attack, unsigned int release) {
	env->attackCoeff = getEnvelopeCoefficient(attack);
	env->releaseCoeff = getEnvelopeCoefficient(release);
	env->envelope = 0;
}

static void updateEnvelope(Envelope *env, _Q15 inAbs) {
	if(inAbs > env->envelope)
		env->envelope = _Q15add(
							_Q15mpy(env->attackCoeff,
									_Q15sub(env->envelope, inAbs)),
							inAbs);
	else
		env->envelope = _Q15add(
							_Q15mpy(env->releaseCoeff,
									_Q15sub(env->envelope, inAbs)),
							inAbs);
}

static float getFilterX(float fc) {
	return expf((-2.0f * PI * fc) / SR);
}

static void setLPF(OnePoleFilter *filter, float fc) {
	float x = getFilterX(fc);
	filter->a0 = _Q15ftoi(1.0f - x);
	filter->b1 = _Q15ftoi(-x);
	filter->y = 0;
}

static void updateOnePoleFilter(OnePoleFilter *filter, _Q15 x) {
	filter->y = _Q15sub(_Q15mpy(filter->a0, x), _Q15mpy(filter->b1, filter->y));
}

/*
static void updateTwoPoleFilter(TwoPoleFilter *filter, _Q15 x) {
	static _Q15 yTmp;
	yTmp = filter->y;
	filter->y = _Q15sub(_Q15sub(_Q15mpy(filter->a0, x), 
						        _Q15mpy(filter->b1, filter->y)),
							 _Q15mpy(filter->b2, filter->y2));
	filter->y2 = yTmp;
}
*/

// We use Q15.16 here because sometimes the intermediary values (e.g. coeff) go above 1.0
static void setGoertzel(Goertzel *g, unsigned int blockSize, float frequency) {
	float k = 0.5f * ((((float)blockSize) * frequency)/SR);
	float w = (TWO_PI / (float)blockSize) * k;
	float coeff = 2.0f * cosf(w);
	g->q0 = g->q1 = g->q2 = 0;
	g->c = _Q16ftoi(coeff);
	g->i = 0;
	g->n = blockSize;
	g->magnitude = 0;
	g->magScaler = _Q16reciprocalQ16(_Q16ftoi((float)blockSize));
}

// returns 1 when the algorithm has completed the window, 0 otherwise.
static int updateGoertzel(Goertzel *g, _Q16 x) {
	g->q0 = (_Q16mac(g->c, g->q1, 0) - g->q2) + x;
	g->q2 = g->q1;
	g->q1 = g->q0;
	g->i++;
	if(g->i == g->n) {
		// Note that instead of sqrt, pow is used -> pow(x, 1/2) == sqrt(x) 
		// magnitudes are normalised by dividing by blockSize.
		g->magnitude = _Q16mac(_Q16power(_Q16mac(g->q1, g->q1, 0) + 
												 _Q16mac(g->q2, g->q2, 0) - 
						  						 _Q16mac(_Q16mac(g->q1, g->q2, 0), 
								 						 g->c, 0),
								 		 32768),
							   g->magScaler, 0);
		g->q0 = g->q1 = g->q2 = 0;
		g->i = 0;
		return 1;
	} else {
		return 0;
	}
}

static void setGoertzelArray(GoertzelArray *ga, unsigned int blockSize, float startFrequency, float endFrequency) {
	float binWidth = SR / ((float)blockSize);
	ga->size = 1 + (int)floorf((endFrequency - startFrequency) / binWidth); 
	ga->gs = (Goertzel*)malloc(ga->size * sizeof(Goertzel)); // allocate memory for bins x Goertzel structs
	unsigned int i;
	for(i=0; i<ga->size; i++) {
		setGoertzel(&ga->gs[i], blockSize, startFrequency + (((float)i) * binWidth));
	}
	float scaler = 1.0f / ((float)ga->size);
	ga->scaler = _Q15ftoi(scaler);
}

static int updateGoertzelArray(GoertzelArray *ga, _Q15 x) {
	_Q16 x16 = _Q15toQ16(x);
	unsigned int i;
	for(i=0; i<ga->size; i++) {
		ga->ready = updateGoertzel(ga->gs+i, x16);
	}
	return ga->ready;
}

void setSpectralFlux(SpectralFluxOnsetDetector *sf, GoertzelArray *ga) {
	sf->ga = ga;
	sf->prevMagnitudes = calloc(ga->size, sizeof(_Q16));
	sf->fluxes = calloc(ONSET_WINDOW_SIZE, sizeof(_Q16));
	sf->ready = 0;
}

static int updateSpectralFlux(SpectralFluxOnsetDetector *sf, _Q15 x) {
	sf->ready = updateGoertzelArray(sf->ga, x);
	// SF is ready, because goertzel array is ready, because goertzels are ready
	if(sf->ready) {
		sf->i++;
		if(sf->i >= ONSET_WINDOW_SIZE) sf->i -= ONSET_WINDOW_SIZE;
		unsigned int j;
		sf->fluxes[sf->i] = 0;
		_Q16 d;
		for(j=0; j<sf->ga->size; j++) {
			d = sf->prevMagnitudes[j] - sf->ga->gs[j].magnitude;
			sf->fluxes[sf->i] = _Q16mac(d + _Q16abs(d), 32768, sf->fluxes[sf->i]);
			sf->prevMagnitudes[j] = sf->ga->gs[j].magnitude;
		}
	}
	return sf->ready;
}

//------------------------------------------------------------------------------
// End of file
