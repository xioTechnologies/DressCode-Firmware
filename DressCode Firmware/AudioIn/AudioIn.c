/*
    AudioIn.c
    Author: Seb Madgwick

    Sample rate:
    = MIPS / ( (ADCS + 1) * (12 + SAMC) * N )
    = 4000000 / ( (1 + 1) * (14 + 17) * 16)
    = 4.032 kHz

    SPI clock = 4 MHz

    ENVELOPE_FREQ and AUTO_GAIN_FREQ combine to create second order dynamics.
    The time constant of AUTO_GAIN_FREQ must be sufficiently lower than that of
    ENVELOPE_FREQ to ensure closed-loop stability.

    P2P_TARGET must be less than ADC maximum value else clipping will not cause
    the auto gain to decrease.  An optimal value is as high as possible to
    maximise resolution and preamp gain.  The ideal value is therefore half of
    ADC range because this is highest valid base 2 value.
*/

//------------------------------------------------------------------------------
// Includes

#include "AudioIn.h"
#include "Fixed.h"
#include <p24Fxxxx.h>

//------------------------------------------------------------------------------
// Definitions

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

#define CS_PIN          _LATB15
#define SPI_DEV			1
#define TWO_PI_T        (6.283185f * (1.0f / 4032.0f))  // 2 * PI * sample period
#define HP_FILTER_FREQ  7.32f   // Hz
#define ENVELOPE_FREQ   7.32f   // Hz
#define AUTO_GAIN_FREQ  0.05f   // Hz
#define P2P_TARGET      1024    // auto gain peak-to-peak target


#if SPI_DEV == 1
#define SPIXBUF		SPI1BUF
#define SPIXIF			_SPI1IF
#define SPIXASM			"MOV SPI1BUF, W0"
#define SPIXSTATbits	SPI1STATbits
#define SPIXCON1bits	SPI1CON1bits
#endif

#if SPI_DEV == 2
#define SPIXBUF			SPI2BUF
#define SPIXIF			_SPI2IF
#define SPIXASM			"MOV SPI2BUF, W0"
#define SPIXSTATbits	SPI2STATbits
#define SPIXCON1bits	SPI2CON1bits
#endif

//------------------------------------------------------------------------------
// Variables

static int isGetReady = 0;
static Fixed sampleValue;

//------------------------------------------------------------------------------
// Function declarations

static void setPreampGain(const PreampGain preampGain);

//------------------------------------------------------------------------------
// Functions

void AudioInInit(void) {

    // Setup ADC
    AD1CON1 |= 0x0400;          // 12-bit A/D operation
    AD1CON1bits.SSRC = 0b0111;  // Internal counter ends sampling and starts conversion (auto-convert)
    AD1CON1bits.ASAM = 1;       // Sampling begins immediately after the last conversion; SAMP bit is auto-set
    AD1CON2bits.PVCFG = 0b01;   // External VREF+
    AD1CON2bits.SMPI = 15;      // Interrupts at the completion of the conversion for each 16th sample
    AD1CON3bits.SAMC = 17;      // Auto-Sample Time = 17 TAD
    AD1CON3bits.ADCS = 1;       // 2 * TCY = TAD
    AD1CHSbits.CH0SA = 1;       // Sample A Channel 0 Positive Input = AN1
    _AD1IP = 7;                 // set interrupt priority
    _AD1IF = 0;                 // clear interrupt flag
    _AD1IE = 1;                 // enable interrupt
    AD1CON1bits.ADON = 1;       // A/D Converter module is operating

    // Setup SPI
    SPIXSTATbits.SISEL = 0b101; // Interrupt when the last bit is shifted out of SPIxSR; now the transmit is complete
    SPIXCON1bits.MODE16 = 1;    // Communication is word-wide (16 bits)
    SPIXCON1bits.CKE = 1;       // Serial output data changes on transition from active clock state to Idle clock state (see bit 6)
    SPIXCON1bits.MSTEN = 1;     // Master mode
    SPIXCON1bits.SPRE = 0b111;  // Secondary prescale 1:1
    SPIXCON1bits.PPRE = 0b11;   // Primary prescale 1:1
    SPIXSTATbits.SPIEN = 1;     // Enables module and configures SCKx, SDOx, SDIx and SSx as serial port pins

    // Ensure default preamp gain
    setPreampGain(GAIN_1);
}

int AudioInIsGetReady(void) {
    return isGetReady;
}

Fixed AudioInGet(void) {
    isGetReady = 0;
    return sampleValue;
}

//------------------------------------------------------------------------------
// Functions - ISRs

void __attribute__((interrupt, auto_psv))_ADC1Interrupt(void) {
    unsigned int adc;
    static Fixed bias = FIXED_FROM_FLOAT(2048.0f);
    static Fixed envelope = 0;
    static Fixed gain = FIXED_FROM_FLOAT(1.0f / 2048.0f);
    static Fixed swGain = FIXED_FROM_INT(1);

    // Get ADC result
    adc = ADC1BUF0;
    adc += ADC1BUF1;
    adc += ADC1BUF2;
    adc += ADC1BUF3;
    adc += ADC1BUF4;
    adc += ADC1BUF5;
    adc += ADC1BUF6;
    adc += ADC1BUF7;
    adc += ADC1BUF8;
    adc += ADC1BUF9;
    adc += ADC1BUF10;
    adc += ADC1BUF11;
    adc += ADC1BUF12;
    adc += ADC1BUF13;
    adc += ADC1BUF14;
    adc += ADC1BUF15;
    adc >>= 4;     // divide by 16

    // High-pass filter
    Fixed signal = FIXED_FROM_INT(adc) - bias;
    bias += FIXED_MUL(signal, FIXED_FROM_FLOAT(HP_FILTER_FREQ * TWO_PI_T));

    // Apply auto gain
    signal = FIXED_MUL(signal, swGain);
    sampleValue = signal;

    // Envelope follower
    if(signal > envelope) {
        if(signal > 0) {
            envelope = signal;
        }
    }
    envelope -= FIXED_MUL(envelope, FIXED_FROM_FLOAT(ENVELOPE_FREQ * TWO_PI_T));

    // Adjust auto gain
    Fixed error = envelope - FIXED_FROM_INT(P2P_TARGET);
    gain -= FIXED_MUL(error, FIXED_FROM_FLOAT(AUTO_GAIN_FREQ * TWO_PI_T));  // proportional feedback

    // Apply gain as combination of preamp gain and software gain
    if(gain >= FIXED_FROM_INT(1024)) {
        setPreampGain(GAIN_1024);
        swGain = gain >> 9;     // should be 10 but 9 provides correct behaviour when tested, I don't know why
    }
    else if(gain >= FIXED_FROM_INT(256)) {
        setPreampGain(GAIN_256);
        swGain = gain >> 8;
    }
    else if(gain >= FIXED_FROM_INT(64)) {
        setPreampGain(GAIN_64);
        swGain = gain >> 6;
    }
    else if(gain >= FIXED_FROM_INT(16)) {
        setPreampGain(GAIN_16);
        swGain = gain >> 4;
    }
    else if(gain >= FIXED_FROM_INT(4)) {
        setPreampGain(GAIN_4);
        swGain = gain >> 2;
    }
    else {
        setPreampGain(GAIN_1);
        swGain = FIXED_FROM_INT(1);
    }

    isGetReady = 1; // set 'data ready' flag
    _AD1IF = 0;     // clear interrupt flag
}

static void setPreampGain(const PreampGain preampGain) {
    static PreampGain currentPreampGain = INVALID;
    if(preampGain != currentPreampGain) {
        currentPreampGain = preampGain;
        CS_PIN = 0;         // assert chip select
        SPIXIF = 0;        // clear flag
        SPIXBUF = 0x4000 | preampGain;
        while(!SPIXIF);    // wait for transmit to complete
        CS_PIN = 1;         // chip select idle
        asm volatile(SPIXASM);    // discard received data
    }
}

//------------------------------------------------------------------------------
// End of file
