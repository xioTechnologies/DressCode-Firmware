/*
    main.c
    Author: Seb Madgwick

    Device:
    PIC24FV16KA304

    Compiler:
    MPLAB C30 v3.31

    Peripherals used:
    Timer 5                 Delay.c
    OC 1-3                  Leds.c
    SPI 2                   AudioIn.c
    UART 1                  Uart2.c
    ADC 1                   AudioIn.c

    Interrupt priorities (nesting enabled):
    7. ADC 1
    6. UART 2 TX and RX
    5.
    4.
    3.
    2.
    1.
*/

//------------------------------------------------------------------------------
// Includes

#include "AudioIn/AudioIn.h"
#include "Delay/Delay.h"
//#include "Fixed.h"
#include "Leds/Leds.h"
#include <p24Fxxxx.h>
#include <stdlib.h>
#include <libq.h>
//#include "Uart/Uart2.h"

//------------------------------------------------------------------------------
// Configuration bits

_FOSC(OSCIOFNC_OFF)      // CLKO output disabled
_FOSCSEL(FNOSC_FRCPLL)   // Fast RC Oscillator with Postscaler and PLL Module (FRCDIV+PLL)
//_FPOR(MCLRE_OFF)       // RA5 input pin enabled, MCLR disabled
//_FICD(ICS_PGx1)        // EMUC/EMUD share PGC3/PGD3
_FICD(ICS_PGx3)          // EMUC/EMUD share PGC3/PGD3
_FWDT(FWDTEN_OFF)

//------------------------------------------------------------------------------
// Function declarations

static void InitMain(void);

//------------------------------------------------------------------------------
// Functions

int main(void) {

    // Init PIC24
    InitMain();

    // Init modules
	LedsInit();
	SPI1Init();
    AudioInit();
    //Uart2Init(UART_BAUD_250000, 0);

    // Main loop
    while(1) {
        // Update LEDs
        LedsUpdate();
    }
}

static void InitMain(void) {

	/*
    // Disable analogue Inputs
    ANSA = 0x0003;  // RA0 is Vref+, RA1 is AN1
    ANSB = 0x0000;

    // Enable pull-ups
    _CN28PUE = 1;   // RB14 is SDI2
    _CN5PUE = 1;    // RB1 is U2RX

    // Set port latches
    _LATA9 = 1;    // SS1 idle

    // Set port directions
    _TRISC5 = 0;    // RC5 is SCK2
    _TRISC4 = 0;    // RC4 is SDO2
    _TRISA9 = 0;    // RA9 is SS2
    _TRISB0 = 0;    // RB0 is U2TX
    _TRISB7 = 0;    // RB7 is OC1
    _TRISC8 = 0;    // RC8 is OC2
    _TRISA10 = 0;   // RA10 is OC3

    // Setup oscillator for 4 MIPS
    CLKDIVbits.RCDIV = 0b010;   // 2 MHz (divide-by-4)
	*/

    // Disable analogue Inputs
    ANSA = 0x0003;  // RA0 is Vref+, RA1 is AN1 -- same on 301
    ANSB = 0x0000;

    // Enable pull-ups
    //_CN12PUE = 1;	// RB14 is SDI1		//_CN28PUE = 1;   // RB14 is SDI2
    // NO SERIAL // _CN5PUE = 1;    // RB1 is U2RX

    // Set port directions
    TRISBbits.TRISB12 = 0;   // RB12 = SCK1     //_TRISC5 = 0;    // RC5 is SCK2
    TRISBbits.TRISB13 = 0;   // RB13 = SDO1     //_TRISC4 = 0;    // RC4 is SDO2
    TRISBbits.TRISB15 = 0;   // RB15 = SS1      //_TRISA9 = 0;    // RA9 is SS2

    // NO SERIAL //_TRISB0 = 0;    // RB0 is U2TX

    TRISBbits.TRISB7 = 0;    // RB7 is OC1
    TRISBbits.TRISB0 = 0;    // RB0 is OC2		//_TRISC8 = 0;    // RC8 is OC2
    TRISBbits.TRISB1 = 0;	 // RB1 is OC3		//_TRISA10 = 0;   // RA10 is OC3

    // Setup oscillator for 16 MIPS, 32 MHz
    CLKDIVbits.RCDIV = 0;   
}

//------------------------------------------------------------------------------
// End of file
