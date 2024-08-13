#include "process.h"
#include <stdio.h>
#include <math.h>
#include "stm32_dsp.h"
#include "bsp_ili9341_lcd.h"


static char str[50];
u32 IN3[NPT*2];
ChannelInfo A_Feedback;
ChannelInfo B_Feedback;
ChannelInfo C_Mixed;

extern u8 sampleReady;

// To address unaligned frequency components
int findMax(float *nums, int rank)
{
	int index;
	if(rank == 1)
	{
		if(*nums > *(nums + 1))				index = 0;
		else								index = 1;
		if(*(nums - 1) > *(nums + index))	index = -1;
		else								;
	}
	else if(rank == 2)
	{
		index = (*(nums + 1) > *(nums - 1)) ? 1 : -1;
	}
	return index;
}

void calcMag(ChannelInfo *info)
{
	int i;
	float baseMag = 0.0;
	cr4_fft_1024_stm32(info->OUT, info->IN, NPT);
	for(i = 0; i < NPT/2; i++)
	{
		float real = (float)(((signed)info->OUT[i] / (1<<16))) / 4096.0f*3.3;
		float imag = (float)(((signed)(info->OUT[i] << 16) / (1<<16))) / 4096.0f*3.3;
		info->OUTPUT_MAG[i] = sqrt(SQUARE(real) + SQUARE(imag)) * (i?2:1);
		if(info != &C_Mixed)
		{
			if(i != 0 && info->OUTPUT_MAG[i] > baseMag)
			{
				info->baseIndex = i;
				baseMag = info->OUTPUT_MAG[i];
			}
		}
		info->frequency = info->baseIndex * 500.0;
	}
	// Refined Method to measure Base Frequency
	// Accuracy improved DRAMATICALLY!!!
	if(info != &C_Mixed)
		for(i = 1; i <= 5; i++)
		{
			if(info->baseIndex < 3 || info->baseIndex * i > NPT / 2)
				break;
			int i1 = findMax(info->OUTPUT_MAG+info->baseIndex*i, 1);
			int i2 = findMax(info->OUTPUT_MAG+info->baseIndex*i+i1, 2);
			info->OUTPUT_MAG[i*info->baseIndex] = info->OUTPUT_MAG[i*info->baseIndex+i1]
				/ sinc(info->OUTPUT_MAG[info->baseIndex*i+i1+i2]/(info->OUTPUT_MAG[info->baseIndex*i+i1]+info->OUTPUT_MAG[info->baseIndex*i+i1+i2]));
		}
	else
	{
		for(i = 0; 5 * i < NPT/2; i++)
		{
			int i1 = findMax(info->OUTPUT_MAG+5*i, 1);
			int i2 = findMax(info->OUTPUT_MAG+5*i+i1, 2);
			info->OUTPUT_MAG[i*5] = info->OUTPUT_MAG[i*5+i1]
				/ sinc(info->OUTPUT_MAG[5*i+i1+i2]/(info->OUTPUT_MAG[5*i+i1]+info->OUTPUT_MAG[5*i+i1+i2]));
			if(i != 0 && info->baseIndex <= 0 && info->OUTPUT_MAG[5*i] > 0.35)
			{
				info->baseIndex = 5*i;
				baseMag = info->OUTPUT_MAG[5*i];
			}
		}
	}
	
}


void showInfo(ChannelInfo *info)
{
	int i;
	ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
	// Base Frequency
	sprintf(str, "Base Frequency: %.1fkHz", info->frequency/1000);
	ILI9341_DispString_EN(16, 0, str);
	// Direct Component
	sprintf(str, "Direct: %.4fV", info->OUTPUT_MAG[0]);
	ILI9341_DispString_EN(16, 48, str);
	// Harmonic Components
	for(i = 1; i <= 5; i++)
	{
		if(i == 1)
			sprintf(str, "Base: %.3fV", info->OUTPUT_MAG[i*info->baseIndex]);
		else
			sprintf(str, "Harmonic%d: %.3fV", i, info->OUTPUT_MAG[i*info->baseIndex]);
		ILI9341_DispString_EN(16, 16*(i+3), str);
	}
}

void plotPeriod(ChannelInfo *info)
{
	int i, j;
	u16 x, y;
	float points = NPT / info->baseIndex + 1;
	float width = (LCD_X_LENGTH-10)/points;
	// x-axis(upper & lower)
	for(i = 5; i < LCD_X_LENGTH-5; i++)
	{
		ILI9341_SetPointPixel(i, 0.9*LCD_Y_LENGTH);
		ILI9341_SetPointPixel(i, 0.5*LCD_Y_LENGTH);
	}
	// y-axis(left & right)
	for(i = LCD_Y_LENGTH*0.5; i < LCD_Y_LENGTH*0.9; i++)
	{
		ILI9341_SetPointPixel(5, i);
		ILI9341_SetPointPixel(LCD_X_LENGTH-5, i);
	}
	// Linear Interpolation
	for(i = 0; i <= points; i++)
	{
		x = (u16)(5+width*i);
		y = (u16)(LCD_Y_LENGTH*0.9-(info->IN[i+1]/4096.0*LCD_Y_LENGTH*0.4));
		if(i < points)
		{
			float yNext = (u16)(LCD_Y_LENGTH*0.9-(info->IN[i+2]/4096.0*LCD_Y_LENGTH*0.4));
			for(j = 0; j < 12; j++)
				ILI9341_SetPointPixel(x+j*width/12.0, y+(yNext-y)/12.0*j);
		}
		ILI9341_SetPointPixel(x, y);
		ILI9341_SetPointPixel(x+1, y);
		ILI9341_SetPointPixel(x, y+1);
		ILI9341_SetPointPixel(x-1, y);
		ILI9341_SetPointPixel(x, y-1);
		ILI9341_SetPointPixel(x+1, y+1);
		ILI9341_SetPointPixel(x-1, y+1);
		ILI9341_SetPointPixel(x-1, y-1);
		ILI9341_SetPointPixel(x+1, y-1);
	}
}

void DMA1_Channel1_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_IT_TC1))
	{
		DMA_Cmd(DMA1_Channel1, DISABLE);
		ADC_Cmd(ADC1, DISABLE);
		for(int i = 0; i < NPT; i++)
		{
			A_Feedback.IN[i] = IN3[2*i+0] << 16;
			B_Feedback.IN[i] = IN3[2*i+0] & 0xffff0000;
			C_Mixed.IN[i]    = IN3[2*i+1] << 16;
		}
		calcMag(&A_Feedback);
		calcMag(&B_Feedback);
		calcMag(&C_Mixed);
		sampleReady = 1;
		DMA_ClearITPendingBit(DMA1_IT_TC1);
		ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
	}
}

float GetHarmonicMag(ChannelInfo *info, int harmonic)
{
	return info->OUTPUT_MAG[info->baseIndex*harmonic];
}

float GetHarmonicPhase(ChannelInfo *info, int harmonic)
{
	return Phase32(info->OUT[info->baseIndex*harmonic]);
}
