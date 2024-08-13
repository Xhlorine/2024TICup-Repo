#ifndef UTIL_H_INCLUDE
#define UTIL_H_INCLUDE
#include "stm32f10x.h"
#include <math.h>
#define PI 3.14159265358979
#define SQUARE(a) ((a)*(a))
#define LENGTH(a, b) sqrt(SQUARE(a)+SQUARE(b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define ABS(a) (((a) > 0) ? (a) : (-(a)))

float sinc(float x);
float Real(u32 comp);
float Imag(u32 comp);
float Phase(float real, float imag);
float Phase32(u32 comp);
float PhaseBias(float r1, float i1, float r2, float i2);
float PhaseBiasAngle(float phase1, float phase2);
float PhaseBias32(u32 comp1, u32 comp2);
float Mod(float num, float base);


#endif
