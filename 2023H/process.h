#ifndef PROCESS_H_INCLUDE
#define PROCESS_H_INCLUDE

#include "stm32f10x.h"
#include "adc.h"

void calcMag(ChannelInfo *info);
void showInfo(ChannelInfo *info);
void plotPeriod(ChannelInfo *info);


#endif
