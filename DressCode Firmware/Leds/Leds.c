/*
    Leds.c
    Author: Seb Madgwick
*/

//------------------------------------------------------------------------------
// Includes

#include "Fixed.h"
#include "Leds.h"
#include <p24Fxxxx.h>

//------------------------------------------------------------------------------
// Definitions

#define TWO_PI_T        (6.283185f * (1.0f / 4000.0f))  // 2 * PI * sample period
#define ENVELOPE_FREQ   1.0f    // Hz
#define LED1_THRESH     3000    // threshold relative to envelope
#define LED2_THRESH     2000
#define LED3_THRESH     1000
#define LED1_OFF_RATE   50      // arbitrary units
#define LED2_OFF_RATE   100
#define LED3_OFF_RATE   200

//------------------------------------------------------------------------------
// Functions

void LedsInit(void) {

    // Setup Timer 3
    PR3 = 0xFFFF;       // set period register for 61 Hz at 4 MIPS for 16-bit resolution
    T3CONbits.TON = 1;  // start timer

    // Setup Output Compare 1,2,3 as PWM
    OC1CON1bits.OCTSEL = 0b001;     // output Compare x Timer is Timer 3
    OC2CON1bits.OCTSEL = 0b001;
    OC3CON1bits.OCTSEL = 0b001;
    OC1CON1bits.OCM = 0b110;        // edge-Aligned PWM mode on OCx
    OC2CON1bits.OCM = 0b110;
    OC3CON1bits.OCM = 0b110;
    OC1CON2bits.SYNCSEL = 0b01101;  // Trigger/Synchronization Source is Timer 3
    OC2CON2bits.SYNCSEL = 0b01101;
    OC3CON2bits.SYNCSEL = 0b01101;
}

void LedsUpdate(Fixed audioSample) {
    static Fixed envelope = 0;
    static unsigned led1 = 0;
    static unsigned led2 = 0;
    static unsigned led3 = 0;

   // Envelope follower
    if(audioSample > envelope) {
        if(audioSample > 0) {
            envelope = audioSample;
        }
    }
    envelope -= FIXED_MUL(envelope, FIXED_FROM_FLOAT(ENVELOPE_FREQ * TWO_PI_T));

    // Turn on LEDs according to thresholds
    if(envelope > FIXED_FROM_INT(LED1_THRESH)) {
        led1 = 65535;
    }
    if(envelope > FIXED_FROM_INT(LED2_THRESH)) {
        led2 = 65535;
    }
    if(envelope > FIXED_FROM_INT(LED3_THRESH)) {
        led3 = 65535;
    }

    // Turn off LEDs at defined rate
    led1 = led1 > LED1_OFF_RATE ? led1 - LED1_OFF_RATE : 0;
    led2 = led2 > LED2_OFF_RATE ? led2 - LED2_OFF_RATE : 0;
    led3 = led3 > LED3_OFF_RATE ? led3 - LED3_OFF_RATE : 0;

    // Set PWM duty cycles
    OC1R = led1;
    OC2R = led2;
    OC3R = led3;
}

//------------------------------------------------------------------------------
// End of file
