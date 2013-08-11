/*
    Analysis.c
    Author: Vincent Akkermans
*/

//------------------------------------------------------------------------------
// Includes

#include "Analysis.h"
#include "Qutils/Qutils.h"
#include <math.h>
#include <stdlib.h>
#include <p24Fxxxx.h>
#include <libq.h>


//------------------------------------------------------------------------------
// Definitions

#define SR 16129.03224f 	// TODO: check this is right, //moved to 16 mips
#define ONSET_WINDOW_SIZE 5

//#define PI 3.141592f
//#define TWO_PI 6.283185f

/*
typedef struct Envelope {
	_Q16 attackCoeff;
	_Q16 releaseCoeff;
	_Q16 envelope;
} Envelope;

typedef struct OnePoleFilter {
	_Q16 a0;
	_Q16 y;
	_Q16 b1;
} OnePoleFilter;

typedef struct TwoPoleFilter {
	_Q16 a0;
	_Q16 y;
	_Q16 y2;
	_Q16 b1;
	_Q16 b2;
} TwoPoleFilter;
*/

typedef struct Goertzel {
	unsigned char n; // blocksize
	unsigned char i; // index in the current block
	_Q16 q0, q1, q2;
	_Q16 c;
	_Q16 magnitude;
	_Q16 magScaler;
} Goertzel;

// scaler is the value by which the accumulated magnitude of all the bins is multiplied.
// scaler = 1 / size
typedef struct GoertzelArray {
	struct Goertzel *gs;
	unsigned char size;
	unsigned char ready;
	_Q16 scaler; 
} GoertzelArray;

typedef struct SpectralFluxOnsetDetector {
	GoertzelArray *ga;
	_Q16 *prevMagnitudes;
	_Q16 *fluxes;
	unsigned char i; // last index written in.
	unsigned char ready;
} SpectralFluxOnsetDetector;


//------------------------------------------------------------------------------
// Variables

static GoertzelArray lowG;
static GoertzelArray midG;
static GoertzelArray hiG;

static SpectralFluxOnsetDetector lowSF;
static SpectralFluxOnsetDetector midSF;
static SpectralFluxOnsetDetector hiSF;

//static Envelope longEnv;
//static Envelope shortEnv;

//static _Q16 q16one;
//static _Q16 q16negOne;

//------------------------------------------------------------------------------
// Function declarations

/*
static _Q16 ENV_calculateCoefficient(unsigned int ms);
static void ENV_init(Envelope *env, unsigned int attack, unsigned int release);
static void ENV_update(Envelope *env, _Q16 x); 
static _Q16 ENV_getAmplitude(Envelope *e);
*/

/*
static float F1P_calculateX(float fc);
static void  F1P_initLPF(OnePoleFilter *filter, float fc);
static void  F1P_update(OnePoleFilter *filter, _Q16 x);
static void  F2P_update(TwoPoleFilter *filter, _Q16 x);
*/

static void GOE_init(Goertzel *g, unsigned int blockSize, float frequency);
static unsigned char  GOE_update(Goertzel *g, _Q16 x);

static void GA_init(GoertzelArray *ga, unsigned int blockSize, float startFrequency, float endFrequency);
static unsigned char  GA_update(GoertzelArray *ga, _Q16 x);
//static int  GA_readyP(GoertzelArray *ga);
static _Q16 GA_getMagnitude(GoertzelArray *ga);

static void SF_init(SpectralFluxOnsetDetector *sf, GoertzelArray *ga);
static unsigned char  SF_update(SpectralFluxOnsetDetector *sf, _Q16 x);
//static int  SF_readyP(SpectralFluxOnsetDetector *sf);
//static _Q16 SF_getFlux(SpectralFluxOnsetDetector *sf);
static unsigned char  SF_getOnset(SpectralFluxOnsetDetector *sf);


//------------------------------------------------------------------------------
// Functions

void AnalysisInit(void) {
	// initialise one and negative one.
	// TODO: turn into definition
	//q16one = _Q16ftoi(1.0f);
	//q16negOne = _Q16ftoi(-1.0f);

	//ENV_init(&longEnv, 1000, 1000);
	//ENV_init(&shortEnv, 20, 500);
	
	// These arrays together make up 11 Goertzels, make sure 
	// there is enough memory available on the stack.
	GA_init(&lowG, 200, 0.0f, 80.0f); 		// 80Hz updates and 0 Hz and 80 Hz bins.
	GA_init(&midG, 200, 160.0f, 250.0f); 	// voice frequency range (female): 80Hz updates and bins at 160 Hz and 240Hz
	GA_init(&hiG, 200, 320.f, 800.0f); 	// catch high frequencies

	SF_init(&lowSF, &lowG);
	SF_init(&midSF, &midG);
	SF_init(&hiSF, &hiG);
}

void AnalysisUpdate(Fixed sample) {
	// bring back sample to [-1, 1]
	static _Q16 x;
	x = _Q16mac(sample, 2, 0);

	//ENV_update(&shortEnv, x);
	//ENV_update(&longEnv, x);

	SF_update(&lowSF, x);
	SF_update(&midSF, x);
	SF_update(&hiSF, x);

	OC1R = _Q16toQ15(GA_getMagnitude(lowSF.ga));
	OC2R = SF_getOnset(&midSF) ? 32000 : 0;
	OC3R = _Q16toQ15(GA_getMagnitude(hiSF.ga));
}

/*
static _Q16 ENV_getAmplitude(Envelope *e) {
	return e->envelope;
}
*/

/*
static int GA_readyP(GoertzelArray *ga) {
	return ga->ready;
}
*/

static _Q16 GA_getMagnitude(GoertzelArray *ga) {
	ga->ready = 0;
	_Q16 acc = 0;
	unsigned char i;
	for(i=0; i<ga->size; i++) {
		//acc = _Q16mac(ga->scaler, ga->gs[i].magnitude, acc);
		acc = ga->gs[i].magnitude + acc;
	}
	return acc;
}

/*
static int SF_readyP(SpectralFluxOnsetDetector *sf) {
	return sf->ready;
}
*/

/*
static _Q16 SF_getFlux(SpectralFluxOnsetDetector *sf) {
	// sf->ready = 0; // N.B. don't need to set ready flag, done in update
	return sf->fluxes[sf->i];
}
*/

static unsigned char SF_getOnset(SpectralFluxOnsetDetector *sf) {
	unsigned char checks=0, i=0, onsetIndex;
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
// Functions - static

/*
static _Q16 ENV_calculateCoefficient(unsigned int ms) {
	return _Q16ftoi(powf(0.01f, 1.0f/(((float)ms) * SR * 0.001f)));
}

static void ENV_init(Envelope *env, unsigned int attack, unsigned int release) {
	env->attackCoeff = ENV_calculateCoefficient(attack);
	env->releaseCoeff = ENV_calculateCoefficient(release);
	env->envelope = 0;
}

static void ENV_update(Envelope *env, _Q16 x) {
	_Q16 inAbs = _Q16abs(x);
	if(inAbs > env->envelope)
		env->envelope = inAbs + _Q16mac(env->attackCoeff, (env->envelope - inAbs), 0);
	else
		env->envelope = inAbs + _Q16mac(env->releaseCoeff, (env->envelope - inAbs), 0);
}

static float F1P_calculateX(float fc) {
	return expf((-2.0f * PI * fc) / SR);
}

static void F1P_initLPF(OnePoleFilter *filter, float fc) {
	float x = F1P_calculateX(fc);
	filter->a0 = _Q16ftoi(1.0f - x);
	filter->b1 = _Q16ftoi(-x);
	filter->y = 0;
}

static void F1P_update(OnePoleFilter *filter, _Q16 x) {
	filter->y = _Q16mac(filter->a0, x, 0) - _Q16mac(filter->b1, filter->y, 0);
}

static void F2P_update(TwoPoleFilter *filter, _Q16 x) {
	static _Q16 yTmp;
	yTmp = filter->y;
	filter->y = (_Q16mac(filter->a0, x, 0) - _Q16mac(filter->b1, filter->y, 0)) 
				- _Q16mac(filter->b2, filter->y2, 0);
	filter->y2 = yTmp;
}
*/

static void GOE_init(Goertzel *g, unsigned int blockSize, float frequency) {
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
static unsigned char GOE_update(Goertzel *g, _Q16 x) {
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

static void GA_init(GoertzelArray *ga, 
					unsigned int blockSize, 
					float startFrequency, 
					float endFrequency) {
	float binWidth = SR / ((float)blockSize);
	ga->size = 1 + (int)floorf((endFrequency - startFrequency) / binWidth); 
	ga->gs = (Goertzel*)malloc(ga->size * sizeof(Goertzel));
	unsigned char i;
	for(i=0; i<ga->size; i++) {
		GOE_init(&ga->gs[i], blockSize, startFrequency + (((float)i) * binWidth));
	}
	float scaler = 1.0f / ((float)ga->size);
	ga->scaler = _Q16ftoi(scaler);
}

static unsigned char GA_update(GoertzelArray *ga, _Q16 x) {
	unsigned char i;
	for(i=0; i<ga->size; i++) {
		ga->ready = GOE_update(ga->gs+i, x);
	}
	return ga->ready;
}

void SF_init(SpectralFluxOnsetDetector *sf, GoertzelArray *ga) {
	sf->ga = ga;
	sf->prevMagnitudes = calloc(ga->size, sizeof(_Q16));
	sf->fluxes = calloc(ONSET_WINDOW_SIZE, sizeof(_Q16));
	sf->ready = 0;
}

static unsigned char SF_update(SpectralFluxOnsetDetector *sf, _Q16 x) {
	sf->ready = GA_update(sf->ga, x);
	// SF is ready, because goertzel array is ready, because goertzels are ready
	if(sf->ready) {
		sf->i++;
		if(sf->i >= ONSET_WINDOW_SIZE) sf->i -= ONSET_WINDOW_SIZE;
		unsigned char j;
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
