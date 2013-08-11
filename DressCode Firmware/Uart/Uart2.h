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
extern volatile unsigned char uart2RxBufIn;
extern volatile unsigned char uart2RxBufOut;
extern volatile int uart2RxBufOverrun;
extern volatile char uart2TxBuf[256];
extern volatile unsigned char uart2TxBufIn;
extern volatile unsigned char uart2TxBufOut;
extern volatile unsigned char uart2TxBufCount;

//------------------------------------------------------------------------------
// Function declarations

void Uart2Init(const UartBaud baud, const int flowControlEnabled);
void Uart2PutString(const char* str);

//------------------------------------------------------------------------------
// Macros

#define Uart2IsGetReady() (uart2RxBufIn - uart2RxBufOut)
#define Uart2IsPutReady() (255 - uart2TxBufCount)
#define Uart2GetChar() uart2RxBuf[uart2RxBufOut++]
#define Uart2PutChar(c) {           \
    uart2TxBuf[uart2TxBufIn] = c;   \
    uart2TxBufCount++;              \
    uart2TxBufIn++;                 \
    if(!_U2TXIE) {                  \
        _U2TXIF = 1;                \
        _U2TXIE = 1;                \
    }                               \
}
#define Uart2FlushRxBuf() { uart2RxBufOut = uart2RxBufIn; uart2RxBufOverrun = 0; }
#define Uart2FlushTxBuf() { uart2TxBufOut = uart2TxBufIn; uart2TxBufCount = 0; }
#define Uart2RxTasks() { if(U2STAbits.URXDA) { _U2RXIF = 1; } U2STAbits.OERR = 0; }
#define Uart2TxIsIdle() (_U2TXIE != 0)

#endif

//------------------------------------------------------------------------------
// End of file
