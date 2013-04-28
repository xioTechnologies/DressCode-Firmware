/*
    Delay.c
    Author: Seb Madgwick
*/

//------------------------------------------------------------------------------
// Includes

#include <p24Fxxxx.h>

//------------------------------------------------------------------------------
// Functions

void Delay(unsigned int milliseconds) {
    T5CON = 0x8000;     // enable TMR5 with 1:1 prescalar
    while (milliseconds--) {
        TMR5 = 0;
        while (TMR5 < 16000);   // wait 1 ms at 16 MIPS
    }
}

//------------------------------------------------------------------------------
// End of file
