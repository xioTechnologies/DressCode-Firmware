/*
    AudioIn.h
    Author: Seb Madgwick
*/

#ifndef AudioIn_h
#define AudioIn_h

//------------------------------------------------------------------------------
// Includes

#include <libq.h>

//------------------------------------------------------------------------------
// Function declarations

void SPI1Init(void);
void AudioInit(void);

typedef struct GoertzelArray GoertzelArray;
typedef struct Envelope Envelope;
typedef struct SpectralFluxOnsetDetector SpectralFluxOnsetDetector;

int AU_readyP(void);
int GA_readyP(GoertzelArray *ga);
_Q16 GA_getMagnitude(GoertzelArray *ga);

_Q15 ENV_getMagnitude(Envelope *e);

int SF_readyP(SpectralFluxOnsetDetector *sf);
_Q16 SF_getFlux(SpectralFluxOnsetDetector *sf);
int SF_getOnset(SpectralFluxOnsetDetector *sf);

#endif

//------------------------------------------------------------------------------
// End of file

