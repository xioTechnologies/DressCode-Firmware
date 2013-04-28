/*
    AudioIn.h
    Author: Seb Madgwick
*/

#ifndef AudioIn_h
#define AudioIn_h

//------------------------------------------------------------------------------
// Includes

#include "Fixed.h"

//------------------------------------------------------------------------------
// Function declarations

void AudioInInit(void);
int AudioInIsGetReady(void);
Fixed AudioInGet(void);

#endif

//------------------------------------------------------------------------------
// End of file

