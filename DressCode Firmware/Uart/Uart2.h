/*
    Uart2.h
    Author: Seb Madgwick
*/

#ifndef Uart2_h
#define Uart2_h

//------------------------------------------------------------------------------
// Includes

#include <p24Fxxxx.h>
#include "UartBauds.h"

//------------------------------------------------------------------------------
// Variable declarations

extern volatile char uart2RxBuf[256];
extern volatile unsigned char uart2RxBufInPos;
extern volatile unsigned char uart2RxBufOutPos;
extern volatile int uart2RxBufOverrun;
extern volatile char uart2TxBuf[256];
extern volatile unsigned char uart2TxBufInPos;
extern volatile unsigned char uart2TxBufOutPos;
extern volatile unsigned char uart2TxBufCount;

//------------------------------------------------------------------------------
// Function declarations

void Uart2Init(const UartBaud baud, const int flowControlEnabled);
void Uart2PutString(const char* str);

//------------------------------------------------------------------------------
// Macros

#define Uart2IsGetReady() (uart2RxBufInPos - uart2RxBufOutPos)
#define Uart2IsPutReady() (255 - uart2TxBufCount)
#define Uart2GetChar() uart2RxBuf[uart2RxBufOutPos++]
#define Uart2PutChar(c) {                       \
    uart2TxBuf[uart2TxBufInPos] = c;            \
    uart2TxBufCount++;                          \
    uart2TxBufInPos++;                          \
    if(!_U2TXIE) {                              \
        _U2TXIF = 1;                            \
        _U2TXIE = 1;                            \
    }                                           \
}
#define Uart2FlushRxBuf() { uart2RxBufOutPos = uart2RxBufInPos; uart2RxBufOverrun = 0; }
#define Uart2FlushTxBuf() { uart2TxBufOutPos = uart2TxBufInPos; uart2TxBufCount = 0; }
#define Uart2TxIsIdle() (_U2TXIE != 0)

#endif

//------------------------------------------------------------------------------
// End of file
