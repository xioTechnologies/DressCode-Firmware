/*
    UartBauds.h
    Author: Seb Madgwick

    UxBRG register values calculated for FCY = 4 MIPS and BRGH = 1.
*/

#ifndef Uartbauds_h
#define Uartbauds_h

//------------------------------------------------------------------------------
// Definitions

enum UartBaud {
    UART_BAUD_9600      = 103,
    UART_BAUD_115200    = 8,
    UART_BAUD_250000    = 3
};

#endif

//------------------------------------------------------------------------------
// End of file
