#ifndef AE_MATH_H
#define AE_MATH_H

#include <math.h>

#ifndef _MSC_VER
#include "arm_math.h"
#endif

#define AE_PI       3.14159265358979323846f

#define EPS (1e-7f)

#define AE_CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

#ifndef _MSC_VER
#define AE_SSAT16(x) __SSAT((int32_t)(x), 16)
#define AE_SSAT24(x) __SSAT((int32_t)(x), 24)
#else
#define AE_SSAT16(x) AE_CLAMP(x,-32768,32767)
#define AE_SSAT24(x) AE_CLAMP(x,-16777216,16777215)
#endif

#define AE_ABS(x) ((x) > 0 ? (x) : (-(x)))

#define AE_FLOOR(x) (floorf(x))

#define AE_ROUND(x) (roundf(x))

#define AE_INT(x) ((int)(x))

// deal with x > -1 && x < 0
#define AE_SIGN(x) ((AE_INT(x) == 0 && (x) < 0) ? "-" : "")

#define AE_FRAC(x) ((int)(((x) > 0 ? ((x) - AE_INT(x)) : (AE_INT(x) - (x))) * 1000000))

#define AE_MIN(a,b) ((a) < (b) ? (a) : (b))

#define AE_MAX(a,b) ((a) > (b) ? (a) : (b))

#define SQUARE(x) ((x) * (x))

#define DB2LIN(x) (powf(10.f, (x) / 20.f))

#ifdef VQE_SIMULATE
#define AE_SIN(x) sinf(x)
#define AE_COS(x) cosf(x)
#else
#define AE_SIN(x) arm_sin_f32(x)
#define AE_COS(x) arm_cos_f32(x)
#endif

int ae_gcd(int u, int v);

int ipow(int base, int exp);

float ipowf(float base, int exp);

float pow_int(float base, int exp);

float expint(int n, float x);

float sqrt_approx(float z);

#endif
