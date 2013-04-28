/*
    Fixed.h
    Author: Seb Madgwick

    Q15.16 Fixed point library
*/

#ifndef Fixed_h
#define Fixed_h

//------------------------------------------------------------------------------
// Definitions

typedef long Fixed;

#define FIXED_MUL(a, b) (Fixed)(((long long)(a) * (long long)(b)) >> 16)
#define FIXED_FROM_FLOAT(x) (Fixed)((x) * (float)((long)1 << 16))
#define FIXED_TO_FLOAT(x) ((float)(x) / (float)((long)1 << 16))
#define FIXED_FROM_INT(x) (Fixed)((Fixed)(x) << 16)
#define FIXED_TO_INT(x) (int)((x) >> 16)

#endif

//------------------------------------------------------------------------------
// End of file

