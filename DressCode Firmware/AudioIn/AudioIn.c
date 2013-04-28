/*
    AudioIn.c
    Author: Seb Madgwick

    Sample rate:
    = MIPS / ( (ADCS + 1) * (12 + SAMC) * N )
    = 16000000 / ( (7 + 1) * (14 + 17) * N)
    = 16.13 kHz

    SPI clock = 8 MHz

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

#define CS_PIN          _LATA9
#define TWO_PI_T        (6.283185f * (1.0f / 16000.0f)) // 2 * PI * sample period
#define HP_FILTER_FREQ  7.32f   // Hz
#define ENVELOPE_FREQ   0.1f    // Hz
#define P2P_TARGET      1024    // auto gain peak-to-peak target

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
    AD1CON2bits.SMPI = 3;       // Interrupts at the completion of the conversion for each sample
    AD1CON3bits.SAMC = 16;      // Auto-Sample Time = 16 TAD
    AD1CON3bits.ADCS = 7;       // 8 * TCY = TAD
    AD1CHSbits.CH0SA = 1;       // Sample A Channel 0 Positive Input = AN1
    _AD1IP = 7;                 // set interrupt priority
    _AD1IF = 0;                 // clear interrupt flag
    _AD1IE = 1;                 // enable interrupt
    AD1CON1bits.ADON = 1;       // A/D Converter module is operating

    // Setup SPI
    SPI2STATbits.SISEL = 0b101; // Interrupt when the last bit is shifted out of SPIxSR; now the transmit is complete
    SPI2CON1bits.MODE16 = 1;    // Communication is word-wide (16 bits)
    SPI2CON1bits.CKE = 1;       // Serial output data changes on transition from active clock state to Idle clock state (see bit 6)
    SPI2CON1bits.MSTEN = 1;     // Master mode
    SPI2CON1bits.SPRE = 0b110;  // Secondary prescale 2:1
    SPI2CON1bits.PPRE = 0b11;   // Primary prescale 1:1
    SPI2STATbits.SPIEN = 1;     // Enables module and configures SCKx, SDOx, SDIx and SSx as serial port pins

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
    adc >>= 2;     // divide by 4

    // High-pass filter
    Fixed signal = FIXED_FROM_INT(adc) - bias;
    bias += FIXED_MUL(signal, FIXED_FROM_FLOAT(HP_FILTER_FREQ * TWO_PI_T));

    // Envelope follower
    if(signal > envelope) {
        if(signal > 0) {
            envelope = signal;
        }
    }
    envelope -= FIXED_MUL(envelope, FIXED_FROM_FLOAT(ENVELOPE_FREQ * TWO_PI_T));

    // Adjust auto-gain
    sampleValue = FIXED_MUL(signal, swGain);
    Fixed error = FIXED_MUL(envelope, swGain) - FIXED_FROM_INT(P2P_TARGET);
    gain -= FIXED_MUL(error, FIXED_FROM_FLOAT(ENVELOPE_FREQ * TWO_PI_T));   // proportional feedback

    // Apply gain as combination of preamp gain and software gain
    if(gain >= FIXED_FROM_INT(1024)) {
        setPreampGain(GAIN_1024);
        swGain = gain >> 10;
    }
    else if(gain >= FIXED_FROM_INT(256)) {
        setPreampGain(GAIN_256);
        swGain = gain >> 8;
    }
    else if(gain >= FIXED_FROM_INT(64)) {
        setPreampGain(GAIN_64);
        swGain = gain >> 8;
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
        _SPI2IF = 0;        // clear flag
        SPI2BUF = 0x4000 | preampGain;
        while(!_SPI2IF);    // wait for transmit to complete
        CS_PIN = 1;         // chip select idle
        asm volatile("MOV SPI2BUF, W0");    // discard received data
    }
}

//------------------------------------------------------------------------------
// End of file
