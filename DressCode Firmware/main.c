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
#include "Fixed.h"
#include "Analysis/Analysis.h"
#include "Leds/Leds.h"
#include <p24Fxxxx.h>
#include <stdlib.h>
#include <libq.h>
//#include "Uart/Uart2.h"

//------------------------------------------------------------------------------
// Configuration bits

_FOSC(OSCIOFNC_OFF)     // CLKO output disabled
_FOSCSEL(FNOSC_FRCPLL)  // Fast RC Oscillator with Postscaler and PLL Module (FRCDIV+PLL)
_FWDT(FWDTEN_OFF)       // WDT disabled in hardware; SWDTEN bit disabled
//_FPOR(MCLRE_OFF)        // RA5 input pin enabled, MCLR disabled
_FICD(ICS_PGx1)         // EMUC/EMUD share PGC3/PGD3

//------------------------------------------------------------------------------
// Function declarations

static void InitMain(void);

//------------------------------------------------------------------------------
// Functions

int main(void) {

    // Init PIC24
    InitMain();

    // Init modules
	AnalysisInit();
    AudioInInit();
    //Uart2Init(UART_BAUD_250000, 0);
    LedsInit();

    // Main loop
    while(1) {
        if(AudioInIsGetReady()) {
			AnalysisUpdate(AudioInGet());

            // Update LEDs
            //LedsUpdate(audioSample);

            // Print audio sample
			/*
            Uart2RxTasks();
            if(Uart2IsPutReady() >= 6) {
                static const char asciiDigits[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
                int i = FIXED_TO_INT(audioSample);
                div_t n;
                int print = 0;
                if(i < 0) {
                    Uart2PutChar('-');
                    i = -i;
                }
                if(i >= 10000) {
                    n = div(i, 10000);
                    Uart2PutChar(asciiDigits[n.quot]);
                    i = n.rem;
                    print = 1;
                }
                if(i >= 1000 || print) {
                    n = div(i, 1000);
                    Uart2PutChar(asciiDigits[n.quot]);
                    i = n.rem;
                    print = 1;
                }
                if(i >= 100 || print) {
                    n = div(i, 100);
                    Uart2PutChar(asciiDigits[n.quot]);
                    i = n.rem;
                    print = 1;
                }
                if(i >= 10 || print) {
                    n = div(i, 10);
                    Uart2PutChar(asciiDigits[n.quot]);
                    i = n.rem;
                }
                Uart2PutChar(asciiDigits[i]);
                Uart2PutChar('\r');
            }
			*/
        }
    }
}

static void InitMain(void) {

    // Disable analogue Inputs
    ANSA = 0x0003;  // RA0 is Vref+, RA1 is AN1
    ANSB = 0x0000;
    ANSC = 0x0000;

    // Enable pull-ups
    _CN28PUE = 1;   // RB14 is SDI2
    _CN5PUE = 1;    // RB1 is U2RX
    _CN32PUE = 1;   // RC0 is STAT (battery charging status)

    // Set port latches
    _LATA9 = 1;     // SS1 idle

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
}

//------------------------------------------------------------------------------
// End of file
