#ifndef ADC_H_INCLUDE
#define ADC_H_INCLUDE
#include "stm32f10x.h"
#define NPT 1024

void ADC1_Init(int ADC1_Channelx);
void DualADC_Init(void);

#endif
