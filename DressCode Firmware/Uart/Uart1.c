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
volatile unsigned char uart1RxBufIn = 0;
volatile unsigned char uart1RxBufOut = 0;
volatile int uart1RxBufOverrun = 0;
volatile char uart1TxBuf[256];
volatile unsigned char uart1TxBufIn = 0;
volatile unsigned char uart1TxBufOut = 0;
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
    U1MODEbits.UARTEN = 1;      // UART1 enabled
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
    while(U1STAbits.URXDA) {    // repeat while data available
        uart1RxBuf[uart1RxBufIn] = U1RXREG; // fetch data from buffer
        uart1RxBufIn++;
        if(uart1RxBufIn == uart1RxBufOut) { // check for FIFO overrun
            uart1RxBufOverrun = 1;
        }
    }
    _U1RXIF = 0;    // data received immediately before clearing UxRXIF will be unhandled, URXDA should be polled to set UxRXIF
}

void __attribute__((interrupt, auto_psv))_U1TXInterrupt(void) {
    _U1TXIE = 0;    // disable interrupt to avoid nested interrupt
    _U1TXIF = 0;    // clear interrupt flag
    //do {
        if(uart1TxBufOut == uart1TxBufIn) { // if FIFO empty
            return;
        }
        U1TXREG = uart1TxBuf[uart1TxBufOut];    // send data from FIFO
        uart1TxBufOut++;
        uart1TxBufCount--;
    //} while(!U1STAbits.UTXBF);  // repeat while buffer not full
    _U1TXIE = 1;    // re-enable interrupt
}

//------------------------------------------------------------------------------
// End of file
