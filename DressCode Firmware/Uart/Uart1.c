/*
    Uart1.c
    Author: Seb Madgwick

    Modified to ensure that the UART TX Buffer is never filled as per Errata.
    See: "PIC24FV32KA304 Family Silicon Errata and Data Sheet Clarification"
*/

//------------------------------------------------------------------------------
// Includes

#include <p24Fxxxx.h>
#include "Uart1.h"

//------------------------------------------------------------------------------
// Variables

volatile char uart1RxBuf[256];
volatile unsigned char uart1RxBufInPos = 0;
volatile unsigned char uart1RxBufOutPos = 0;
volatile int uart1RxBufOverrun = 0;
volatile char uart1TxBuf[256];
volatile unsigned char uart1TxBufInPos = 0;
volatile unsigned char uart1TxBufOutPos = 0;
volatile unsigned char uart1TxBufCount = 0;

//------------------------------------------------------------------------------
// Functions

void Uart1Init(const UartBaud baud, const int flowControlEnabled) {
    U1MODE = 0x0000;    // ensure default register states
    U1STA = 0x0000;
    U1BRG = baud;       // set baud rate
    if(flowControlEnabled) {
        U1MODEbits.UEN = 0b10;  // UxTX, UxRX, UxCTS and UxRTS pins are enabled and used
    }
    else {
        U1MODEbits.UEN = 0b00;  // UxTX and UxRX pins are enabled and used; UxCTS, UxRTS and BCLKx pins are controlled by port latches
    }
    U1MODEbits.UARTEN = 1;      // Uart1 enabled
    U1MODEbits.BRGH = 1;        // high speed mode
    U1STAbits.UTXISEL1 = 0b01;  // interrupt when TX FIFO is empty
    U1STAbits.UTXEN = 1;        // transmit enabled
    Uart1FlushRxBuf();          // flush buffers
    Uart1FlushTxBuf();
    _U1RXIP = 6;    // set RX interrupt priority
    _U1TXIP = 6;    // set TX interrupt priority
    _U1RXIF = 0;    // clear RX interrupt flag
    _U1RXIE = 1;    // RX interrupt enabled
}

void Uart1PutString(const char* str) {
    while(*str != '\0') {
        Uart1PutChar(*str++);
    }
}

//------------------------------------------------------------------------------
// Functions - ISRs

void __attribute__((interrupt, auto_psv))_U1RXInterrupt(void) {
    do {
        uart1RxBuf[uart1RxBufInPos] = U1RXREG;  // fetch data from buffer
        uart1RxBufInPos++;
        if(uart1RxBufInPos == uart1RxBufOutPos) {   // check for FIFO overrun
            uart1RxBufOverrun = 1;
        }
    } while(U1STAbits.URXDA);   // repeat while data available
    _U1RXIF = 0;
}

void __attribute__((interrupt, auto_psv))_U1TXInterrupt(void) {
    _U1TXIE = 0;    // disable interrupt to avoid nested interrupt
    _U1TXIF = 0;    // clear interrupt flag
    //do {
        if(uart1TxBufOutPos == uart1TxBufInPos) {   // if FIFO empty
            return;
        }
        U1TXREG = uart1TxBuf[uart1TxBufOutPos]; // send data from FIFO
        uart1TxBufOutPos++;
        uart1TxBufCount--;
    //} while(!U1STAbits.UTXBF);  // repeat while buffer not full
    _U1TXIE = 1;    // re-enable interrupt
}

//------------------------------------------------------------------------------
// End of file
