#ifndef PROCESS_H_INCLUDE
#define PROCESS_H_INCLUDE

#include "stm32f10x.h"
#include "stm32_dsp.h"
#include "delay.h"
#include "adc.h"
#include "util.h"

typedef struct _ChannelFeedback {
	float frequency;
	float direct;
	float magnitude;
	float phase;
} ChannelFeedback;

void GetChannelxFeedback(int ADC1_Channelx, ChannelFeedback * const result);
float GetDualChannelFeedback(ChannelFeedback * const feedbacks);
float GetChannelxMag(int ADC1_Channelx, ChannelFeedback * const result);
void GetDualChannelMags(ChannelFeedback * const feedbacks);
void CTR_GPIO_Init(void);

#endif
