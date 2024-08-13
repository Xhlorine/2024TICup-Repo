#ifndef ADC_H_INCLUDE
#define ADC_H_INCLUDE
#include "stm32f10x.h"
#include "util.h"
#define NPT 1024

typedef enum _WaveForm {Mixed, Sine, Triangle} WaveForm;

typedef struct _ChannelInfo {
	u32 IN[NPT];
	u32 OUT[NPT];
	float OUTPUT_MAG[NPT];
	int baseIndex;
	float frequency;
	WaveForm wave;
} ChannelInfo;

void ADCx_Init(void);

#endif
