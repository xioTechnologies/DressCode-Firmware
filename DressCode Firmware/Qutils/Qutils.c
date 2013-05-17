/*
    Qutils.c
    Author: Vincent Akkermans
*/

#include "Qutils.h"

_Q15 _Q15mpy(_Q15 a, _Q15 b) {
	_Q15 result = (((signed long)a) * b) >> 15;
	return result;
}

// 'chops' off the integer part
_Q15 _Q16toQ15(_Q16 f) {
	return (_Q15)(((f & 0x80000000) >> 16) | ((f & 0x0000FFFF) >> 1));
}

_Q16 _Q15toQ16(_Q15 f) {
	return (_Q16)((((signed long)f & 0x8000) << 16) | ((f & 0x7FFF) << 1));
}

_Q16 _Q16abs(_Q16 f) {
	return f >= 0x80000000 ? _Q16neg(f) : f;
}