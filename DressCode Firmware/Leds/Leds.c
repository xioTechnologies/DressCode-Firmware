/*
    Leds.c
    Author: Seb Madgwick
*/

//------------------------------------------------------------------------------
// Includes

#include "Leds.h"
#include "AudioIn/AudioIn.h"
#include "Q15utils/Q15utils.h"
#include <p24Fxxxx.h>

//------------------------------------------------------------------------------
// Definitions

extern struct GoertzelArray lowG;
extern struct GoertzelArray midG;
extern struct GoertzelArray hiG;
extern struct Envelope env;
extern _Q15 signal;
extern struct SpectralFluxOnsetDetector lowSF;
extern struct SpectralFluxOnsetDetector midSF;
extern struct SpectralFluxOnsetDetector hiSF;

//------------------------------------------------------------------------------
// Functions

void LedsInit(void) {

	OC1CON1bits.OCM		= 0; 		// Output compare channel is disabled
	OC1R				= 0;		// Initialize Compare Register with 0% duty cycle
	OC1RS				= 0xFF;
	OC1CON1bits.OCSIDL	= 0;		// Output capture will continue to operate in CPU Idle mode
	OC1CON1bits.OCFLT	= 0;		// No PWM Fault condition has occurred (this bit is only used when OCM<2:0> = 111)
	OC1CON1bits.OCTSEL	= 1;		// Timer3 is the clock source for output Compare
	OC1CON1bits.OCM		= 0b110;	// PWM mode on OC, Fault pin disabled

	OC2CON1bits.OCM		= 0; 		// Output compare channel is disabled
	OC2R				= 0;		// Initialize Compare Register with 0% duty cycle
	OC2RS				= 0xFF;
	OC2CON1bits.OCSIDL	= 0;		// Output capture will continue to operate in CPU Idle mode
	OC2CON1bits.OCFLT	= 0;		// No PWM Fault condition has occurred (this bit is only used when OCM<2:0> = 111)
	OC2CON1bits.OCTSEL	= 1;		// Timer3 is the clock source for output Compare
	OC2CON1bits.OCM		= 0b110;	// PWM mode on OC, Fault pin disabled

	OC3CON1bits.OCM		= 0; 		// Output compare channel is disabled
	OC3R				= 0;		// Initialize Compare Register with 0% duty cycle
	OC3RS				= 0xFF;
	OC3CON1bits.OCSIDL	= 0;		// Output capture will continue to operate in CPU Idle mode
	OC3CON1bits.OCFLT	= 0;		// No PWM Fault condition has occurred (this bit is only used when OCM<2:0> = 111)
	OC3CON1bits.OCTSEL	= 1;		// Timer3 is the clock source for output Compare
	OC3CON1bits.OCM		= 0b110;	// PWM mode on OC, Fault pin disabled

	PR3					= 0xFF;	    // Initialize PR3 with 0xff = 0d65536 as PWM cycle
	IFS0bits.T3IF		= 0;		// Clear Output Compare interrupt flag
	IEC0bits.T3IE		= 0;		// Enable Output Compare interrupts
	T3CONbits.TON		= 1;		// Start Timer3 with assumed settings
}

void LedsUpdate() {

	static _Q16 lowM, midM, hiM;
	static int lowOnset, midOnset, hiOnset;
	static _Q15 gain;

	if(AU_readyP()) {
		gain = ENV_getMagnitude(&env);
	
		if(GA_readyP(&lowG)) {
			lowM = GA_getMagnitude(&lowG);
		}
	
		if(GA_readyP(&midG)) {
			midM = GA_getMagnitude(&midG);
		}
		
		if(GA_readyP(&hiG)) {
			hiM = GA_getMagnitude(&hiG);
		}
	
		if(SF_readyP(&lowSF)) {
			lowOnset = SF_getOnset(&lowSF);
		}
		if(SF_readyP(&midSF)) {
			midOnset = SF_getOnset(&midSF);
		}
		if(SF_readyP(&hiSF)) {
			hiOnset = SF_getOnset(&hiSF);
		}
	
		/*
		OC3R = gain >> 8;
		OC2R = midOnset ? 32000 : 0;
		OC1R = _Q16toQ15(hiM) >> 8;
		*/


		OC1R = _Q16toQ15(lowM) >> 7;
		OC2R = _Q16toQ15(midM) >> 7;
		OC3R = _Q16toQ15(hiM) >> 7;
		
		
	}
}

//------------------------------------------------------------------------------
// End of file
