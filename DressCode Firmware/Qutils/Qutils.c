/*
    Qutils.c
    Author: Vincent Akkermans
*/

#define _Q15mpy(a, b) ((_Q15)(((signed long)a) * b) >> 15)

// 'chops' off the integer part
#define _Q16toQ15(f) ((_Q15)(((f & 0x80000000) >> 16) | ((f & 0x0000FFFF) >> 1)))
#define _Q15toQ16(f) ((_Q16)((((signed long)f & 0x8000) << 16) | ((f & 0x7FFF) << 1)))
#define _Q16abs(f) 	 (f >= 0x80000000 ? _Q16neg(f) : f)
