/*
    Uart1.h
    Author: Seb Madgwick
*/

#ifndef Uart1_h
#define Uart1_h

//------------------------------------------------------------------------------
// Includes

#include <p24Fxxxx.h>
#include "UartBauds.h"

//------------------------------------------------------------------------------
// Variable declarations

extern volatile char uart1RxBuf[256];
extern volatile unsigned char uart1RxBufIn;
extern volatile unsigned char uart1RxBufOut;
extern volatile int uart1RxBufOverrun;
extern volatile char uart1TxBuf[256];
extern volatile unsigned char uart1TxBufIn;
extern volatile unsigned char uart1TxBufOut;
extern volatile unsigned char uart1TxBufCount;

//------------------------------------------------------------------------------
// Function declarations

void Uart1Init(const UartBaud baud, const int flowControlEnabled);
void Uart1PutString(const char* str);

//------------------------------------------------------------------------------
// Macros

#define Uart1IsGetReady() (uart1RxBufIn - uart1RxBufOut)
#define Uart1IsPutReady() (255 - uart1TxBufCount)
#define Uart1GetChar() uart1RxBuf[uart1RxBufOut++]
#define Uart1PutChar(c) {           \
    uart1TxBuf[uart1TxBufIn] = c;   \
    uart1TxBufCount++;              \
    uart1TxBufIn++;                 \
    if(!_U1TXIE) {                  \
        _U1TXIF = 1;                \
        _U1TXIE = 1;                \
    }                               \
}
#define Uart1FlushRxBuf() { uart1RxBufOut = uart1RxBufIn; uart1RxBufOverrun = 0; }
#define Uart1FlushTxBuf() { uart1TxBufOut = uart1TxBufIn; uart1TxBufCount = 0; }
#define Uart1RxTasks() { if(U1STAbits.URXDA) { _U1RXIF = 1; } U1STAbits.OERR = 0; }
#define Uart1TxIsIdle() (_U1TXIE != 0)

#endif

//------------------------------------------------------------------------------
// End of file
