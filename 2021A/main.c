#include "stm32f10x.h"
#include "./lcd/bsp_ili9341_lcd.h"
#include "./lcd/bsp_xpt2046_lcd.h"
#include "./flash/bsp_spi_flash.h"
#include "palette.h"
#include <string.h>
#include "stm32_dsp.h"
#include <math.h>
#include "adc.h"
#include "bsp_xpt2046_lcd.h"

//#define MAX(a, b, c) (((a)>(b))?((a>c)?(a):(c)):(((b)>(c))?(b):(c)))
#define SQUARE(a) ((a)*(a))

char str[50] = "Hello World!";
u32 IN[NPT];
u32 OUT[NPT];
float OUTPUT_MAG[NPT/2];

float frequency;
float sampleRateP;
extern int prescaler;

// For Estimating Base Frequency and THD
int baseIndex = -1;
float baseMag = 0.0;
float THD = 0.0;
int verified = 0;

int main(void)
{				
	// Initialize LCD, ADC(including TIM, DMA)
	ILI9341_Init();
	ILI9341_GramScan(6);
	LCD_SetColors(WHITE, BLACK);
	ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
	ADC1_Init();
	
	while ( 1 )
	{
		
	}
}

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

float sinc(float x)
{
	if(x == 0)	return 1;
	else		return sin(PI*x)/(PI*x);
}

void calcMag(void)
{
	int i;
	cr4_fft_1024_stm32(OUT, IN, NPT);
	for(i = 0; i < NPT/2; i++)
	{
		float real = (float)(((signed)OUT[i] / (1<<16))) / 4096.0f*3.3;
		float imag = (float)(((signed)(OUT[i] << 16) / (1<<16))) / 4096.0f*3.3;
		OUTPUT_MAG[i] = sqrt(SQUARE(real) + SQUARE(imag)) * (i?2:1);
		//if(OUTPUT_MAG[i] < 0.007)	OUTPUT_MAG[i] = 0;
		if(i != 0 && OUTPUT_MAG[i] > baseMag)
		{
			baseIndex = i;
			baseMag = OUTPUT_MAG[i];
		}
	}
	// Refined Method to measure Base Frequency
	// Accuracy improved DRAMATICALLY!!!
	
	for(i = 1; i <= 5; i++)
	{
		int i1 = findMax(OUTPUT_MAG+baseIndex*i, 1);
		int i2 = findMax(OUTPUT_MAG+baseIndex*i+i1, 2);
		OUTPUT_MAG[i*baseIndex] = OUTPUT_MAG[i*baseIndex+i1]
				/ sinc(OUTPUT_MAG[baseIndex*i+i1+i2]/(OUTPUT_MAG[baseIndex*i+i1]+OUTPUT_MAG[baseIndex*i+i1+i2]));
	}
	/*
	for(i = 0; i < NPT/2; i++)
	{
		if(i == baseIndex)
			continue;
		OUTPUT_MAG[i] -= 0.005 * OUTPUT_MAG[baseIndex];
		OUTPUT_MAG[i] = OUTPUT_MAG[i] > 0 ? OUTPUT_MAG[i] : 0;
	}
	OUTPUT_MAG[baseIndex] *= 0.995;
	*/
}

// Plan A: Equivalent Sample

void optFreq(void)
{
	if(!verified)
	{
		frequency = (float)((int)(sampleRateP * baseIndex + 150) / 1000) * 1000;
		if(frequency < 31000)
		{
			/*
			if(baseIndex < 4)		prescaler = 350;	// Base:  100, Max:  51.2k, MaxBase:  10.6k
			else if(baseIndex < 13)	prescaler = 175;	// Base:  200, Max: 102.4k, MaxBase:  20.6k
			else					prescaler =  70;	// Base:  500, Max: 256.0k, MaxBase:  51.2k
			*/
			prescaler = 72000000.0 / 2 / frequency / 64.0;
		}
		else
		{
			// sampleRate = 72000000 / 2 / prescalor = frequency / (1 + 1 / 64) = frequency * 64 / 65
			prescaler = 72000000.0 / 2 / frequency / 64.0 * 65.0;
		}
		baseIndex = -1;
		baseMag = 0.0;
		sampleRateP = 72000000.0 / NPT / 2.0 / prescaler;
		TIM_PrescalerConfig(TIM2, prescaler, TIM_PSCReloadMode_Update);
		verified = 1;
		
	}
	
}

/*
void optFreq(void)
{
	if(!verified)
	{
		if(baseIndex < 4)		prescaler = 350;	// Base:  100, Max:  51.2k, MaxBase:  10.6k
		else if(baseIndex < 13)	prescaler = 175;	// Base:  200, Max: 102.4k, MaxBase:  20.6k
		else if(baseIndex < 40)	prescaler =  70;	// Base:  500, Max: 256.0k, MaxBase:  51.2k
		else if(baseIndex < 70)	prescaler =  44;	// Base:  800, Max: 409.6k, MaxBase:  81.9k
		else					prescaler =  35;	// Base: 1004, Max: 514.0k, MaxBase: 102.8k (Initial)
		baseIndex = -1;
		baseMag = 0.0;
		sampleRateP = 72000000.0 / NPT / 2.0 / prescaler;
		TIM_PrescalerConfig(TIM2, prescaler, TIM_PSCReloadMode_Update);
		verified = 1;
	}
}
*/

void calcTHD(void)
{
	static int count = 0;
	static float sum = 0.0;
	float thd = 0.0;
	int i;
	if(!verified)
	{
		// Do not save data before stablization!
		count = 0;
		sum = 0.0;
	}
	for(i = 2; i <= 5; i++)
		thd += SQUARE(OUTPUT_MAG[i*baseIndex]);
	thd = sqrt(thd) / OUTPUT_MAG[baseIndex] * 100;
	if(thd > 0 && thd < 50)
	{
		sum += thd;
		count++;
	}
	if(count > 5)
		verified = 2;
	THD = sum / count;
}

void showInfo(void)
{
	static int count = 0;
	static float relativeHarmonic[6];
	int i;
	ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
	// Base Frequency
	sprintf(str, "Base Frequency: %.2fkHz", frequency/1000/*sampleRateP*baseIndex/1000.0/(1+sampleRateP*baseIndex/5000000)*/); // A mild correction
	ILI9341_DispString_EN(16, 0, str);
	// THD
	sprintf(str, "THD: %.2f", THD);
	ILI9341_DispString_EN(16, 16, str);
	// Direct Component(ununified)
	sprintf(str, "Direct: %.4fV", OUTPUT_MAG[0]);
	ILI9341_DispString_EN(16, 48, str);
	// Harmonic Components(unified)
	count++;
	for(i = 1; i <= 5; i++)
	{
		if(i == 1)
			sprintf(str, "Base: %.3f", OUTPUT_MAG[i*baseIndex]/*OUTPUT_MAG[baseIndex]*/);
		else
		{
			relativeHarmonic[i] += OUTPUT_MAG[i*baseIndex]/OUTPUT_MAG[baseIndex];
			sprintf(str, "Harmonic%d: %.3f", i, OUTPUT_MAG[i*baseIndex]/OUTPUT_MAG[baseIndex]);
		}
		ILI9341_DispString_EN(16, 16*(i+3), str);
	}
}

void plotPeriod(void)
{
	int i, j;
	u16 x, y;
	float points = NPT / baseIndex + 1;
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
	for(i = 0; i <= points; i++)
	{
		x = (u16)(5+width*i);
		y = (u16)(LCD_Y_LENGTH*0.9-(IN[i+1]/4096.0*LCD_Y_LENGTH*0.4));
		if(i < points)
		{
			float yNext = (u16)(LCD_Y_LENGTH*0.9-(IN[i+2]/4096.0*LCD_Y_LENGTH*0.4));
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
	static int refresher = 0;
	if(DMA_GetITStatus(DMA_IT_TC))
	{
		DMA_Cmd(DMA1_Channel1, DISABLE);
		calcMag();
		calcTHD();
		optFreq();
		if(verified == 2 && refresher % (int)(sampleRateP/10) == 1)
		{
			showInfo();
			plotPeriod();
		}
		refresher = (refresher + 1) % (int)(sampleRateP/10);
		DMA_ClearITPendingBit(DMA_IT_TC);
		DMA_Cmd(DMA1_Channel1, ENABLE);
	}
}


