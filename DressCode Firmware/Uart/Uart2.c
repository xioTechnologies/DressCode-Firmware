/*
    Uart2.c
    Author: Seb Madgwick

    Modified to ensure that the UART TX Buffer is never filled as per Errata.
    See: "PIC24FV32KA304 Family Silicon Errata and Data Sheet Clarification"
*/

//------------------------------------------------------------------------------
// Includes

#include <p24Fxxxx.h>
#include "Uart2.h"

//------------------------------------------------------------------------------
// Variables

volatile char uart2RxBuf[256];
volatile unsigned char uart2RxBufInPos = 0;
volatile unsigned char uart2RxBufOutPos = 0;
volatile int uart2RxBufOverrun = 0;
volatile char uart2TxBuf[256];
volatile unsigned char uart2TxBufInPos = 0;
volatile unsigned char uart2TxBufOutPos = 0;
volatile unsigned char uart2TxBufCount = 0;

//------------------------------------------------------------------------------
// Functions

void Uart2Init(const UartBaud baud, const int flowControlEnabled) {
    U2MODE = 0x0000;    // ensure default register states
    U2STA = 0x0000;
    U2BRG = baud;       // set baud rate
    if(flowControlEnabled) {
        U2MODEbits.UEN = 0b10;  // UxTX, UxRX, UxCTS and UxRTS pins are enabled and used
    }
    else {
        U2MODEbits.UEN = 0b00;  // UxTX and UxRX pins are enabled and used; UxCTS, UxRTS and BCLKx pins are controlled by port latches
    }
    U2MODEbits.UARTEN = 1;      // Uart2 enabled
    U2MODEbits.BRGH = 1;        // high speed mode
    U2STAbits.UTXISEL1 = 0b01;  // interrupt when TX FIFO is empty
    U2STAbits.UTXEN = 1;        // transmit enabled
    Uart2FlushRxBuf();          // flush buffers
    Uart2FlushTxBuf();
    _U2RXIP = 6;    // set RX interrupt priority
    _U2TXIP = 6;    // set TX interrupt priority
    _U2RXIF = 0;    // clear RX interrupt flag
    _U2RXIE = 1;    // RX interrupt enabled
}

void Uart2PutString(const char* str) {
    while(*str != '\0') {
        Uart2PutChar(*str++);
    }
}

//------------------------------------------------------------------------------
// Functions - ISRs

void __attribute__((interrupt, auto_psv))_U2RXInterrupt(void) {
    do {
        uart2RxBuf[uart2RxBufInPos] = U2RXREG;  // fetch data from buffer
        uart2RxBufInPos++;
        if(uart2RxBufInPos == uart2RxBufOutPos) {   // check for FIFO overrun
            uart2RxBufOverrun = 1;
        }
    } while(U2STAbits.URXDA);   // repeat while data available
    _U2RXIF = 0;
}

void __attribute__((interrupt, auto_psv))_U2TXInterrupt(void) {
    _U2TXIE = 0;    // disable interrupt to avoid nested interrupt
    _U2TXIF = 0;    // clear interrupt flag
    //do {
        if(uart2TxBufOutPos == uart2TxBufInPos) {   // if FIFO empty
            return;
        }
        U2TXREG = uart2TxBuf[uart2TxBufOutPos]; // send data from FIFO
        uart2TxBufOutPos++;
        uart2TxBufCount--;
    //} while(!U2STAbits.UTXBF);  // repeat while buffer not full
    _U2TXIE = 1;    // re-enable interrupt
}

//------------------------------------------------------------------------------
// End of file
