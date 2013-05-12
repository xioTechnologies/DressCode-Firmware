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
extern volatile unsigned char uart1RxBufInPos;
extern volatile unsigned char uart1RxBufOutPos;
extern volatile int uart1RxBufOverrun;
extern volatile char uart1TxBuf[256];
extern volatile unsigned char uart1TxBufInPos;
extern volatile unsigned char uart1TxBufOutPos;
extern volatile unsigned char uart1TxBufCount;

//------------------------------------------------------------------------------
// Function declarations

void Uart1Init(const UartBaud baud, const int flowControlEnabled);
void Uart1PutString(const char* str);

//------------------------------------------------------------------------------
// Macros

#define Uart1IsGetReady() (uart1RxBufInPos - uart1RxBufOutPos)
#define Uart1IsPutReady() (255 - uart1TxBufCount)
#define Uart1GetChar() uart1RxBuf[uart1RxBufOutPos++]
#define Uart1PutChar(c) {               \
    uart1TxBuf[uart1TxBufInPos] = c;    \
    uart1TxBufCount++;                  \
    uart1TxBufInPos++;                  \
    if(!_U1TXIE) {                      \
        _U1TXIF = 1;                    \
        _U1TXIE = 1;                    \
    }                                   \
}
#define Uart1FlushRxBuf() { uart1RxBufOutPos = uart1RxBufInPos; uart1RxBufOverrun = 0; }
#define Uart1FlushTxBuf() { uart1TxBufOutPos = uart1TxBufInPos; uart1TxBufCount = 0; }
#define Uart1RxTasks() { if(U1STAbits.URXDA) _U1RXIF = 1; }
#define Uart1TxIsIdle() (_U1TXIE != 0)

#endif

//------------------------------------------------------------------------------
// End of file
