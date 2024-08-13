#include "util.h"

// Sinc
float sinc(float x)
{
	if(x == 0)	return 1;
	else		return sin(PI*x)/(PI*x);
}

// Basic calculations for Complex numbers
float Real(u32 comp)
{
	return ((signed)comp / 65536) / 4096.0 * 3.3;
}

float Imag(u32 comp)
{
	return ((signed)(comp<<16) / 65536) / 4096.0 * 3.3;
}

float Phase(float real, float imag)
{
	if(real > 0)
	{
		return 180*atan(imag/real)/PI;
	}
	else if(real < 0)
	{
		if(imag >= 0)
			return 180*atan(imag/real)/PI+180;
		else
			return 180*atan(imag/real)/PI-180;
	}
	else
	{
		if(imag > 0)
			return 90;
		else if(imag < 0)
			return -90;
		else
			return 0;
	}
}

float Phase32(u32 comp)
{
	return Phase(Real(comp), Imag(comp));
}

float PhaseBias(float r1, float i1, float r2, float i2)
{
	return Phase(r1*r2+i1*i2, -r1*i2+r2*i1);
}

float PhaseBiasAngle(float p1, float p2)
{
	return Mod(p1-p2+180, 360)-180;
}

float PhaseBias32(u32 comp1, u32 comp2)
{
	return PhaseBias(Real(comp1), Imag(comp1), Real(comp2), Imag(comp2));
}

// Calculation "%" for float numbers
float Mod(float num, float base)
{
	base = ABS(base);
	while(num < 0 || num > base)
	{
		if(num > base)
			num -= base;
		if(num < 0)
			num += base;
	}
	return num;
}

