#include "stm32f10x.h"
#include "bsp_ili9341_lcd.h"
#include "bsp_xpt2046_lcd.h"
#include "./flash/bsp_spi_flash.h"
#include "util.h"
#include "process.h"
#include "Delay.h"
#include "AD9833.h"

// Constants & Variable for State Machine
#define SEPERATE 0
#define ADJUST   1
#define STABLE   2
int state = SEPERATE;

/* Phisical Connections:
 * PC1 -> A'
 * PC2 -> B'
 * PC3 -> C
 * PC4 -> NC
 */

// Phase A - A' increase => A left, A' right; need to add f_A'
// Phase B - B' decrease => B right, B' left; need to suntract f_B'

char str[50] = "Hello World!";

u8 sampleReady = 0;

int AIndex;
int BIndex;

extern ChannelInfo A_Feedback;
extern ChannelInfo B_Feedback;
extern ChannelInfo C_Mixed;

void separate(void);
void adjust(void);

int main(void)
{				
	ILI9341_Init();
	ILI9341_GramScan(6);
	LCD_SetColors(WHITE, BLACK);
	ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
	ADCx_Init();
	
	
	while(1)
	{
		if(sampleReady)
		{
			switch(state)
			{
			case SEPERATE:
				separate();
				sprintf(str, "A: %d, %.2fkHz, %.2fV", A_Feedback.wave, AIndex*0.500, C_Mixed.OUTPUT_MAG[AIndex]);
				ILI9341_DispString_EN(16, 160, str);
				sprintf(str, "B: %d, %.2fkHz, %.2fV", B_Feedback.wave, BIndex*0.500, C_Mixed.OUTPUT_MAG[BIndex]);
				ILI9341_DispString_EN(16, 176, str);
				state = ADJUST;
				ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
				break;
			case ADJUST:
				adjust();
				sprintf(str, "A: %d, %.2fkHz, %.2fV", A_Feedback.wave, AIndex*0.500, C_Mixed.OUTPUT_MAG[AIndex]);
				ILI9341_DispString_EN(16, 0, str);
				sprintf(str, "B: %d, %.2fkHz, %.2fV", B_Feedback.wave, BIndex*0.500, C_Mixed.OUTPUT_MAG[BIndex]);
				ILI9341_DispString_EN(16, 16, str);
				break;
			case STABLE:
				sprintf(str, "A: %d, %.2fkHz, %.2fV", A_Feedback.wave, AIndex*0.500, C_Mixed.OUTPUT_MAG[AIndex]);
				ILI9341_DispString_EN(16, 0, str);
				sprintf(str, "B: %d, %.2fkHz, %.2fV", B_Feedback.wave, BIndex*0.500, C_Mixed.OUTPUT_MAG[BIndex]);
				ILI9341_DispString_EN(16, 16, str);
				sprintf(str, "Stablized.");
				ILI9341_DispString_EN(16, 32, str);
				break;
			default:
				break;
			}
			
			sampleReady = 0;
			DMA_Cmd(DMA1_Channel1, ENABLE);
			ADC_Cmd(ADC1, ENABLE);
		}
	}
}

void separate(void)
{
	// Seperate based on the Amplitude of the base wave.
	// Here's a base wave: Vpp > 0.5V
	// Sine:               Vpp = 1.0V, Vpp(base) = 1.00V;
	// Triangle:           Vpp = 1.0V, Vpp(base) = 0.81V, Vpp(h2) = 0.09V;
	static int label = 1;
	if(label)
	{
		AIndex = C_Mixed.baseIndex;
		BIndex = 0;
		for(int i = C_Mixed.baseIndex+10; i < NPT/2; i+=10)
		{
			if(C_Mixed.OUTPUT_MAG[i] > 0.5/2)
			{
				BIndex = i;
				break;
			}
		}
		if(C_Mixed.OUTPUT_MAG[C_Mixed.baseIndex] > 0.9/2)
		{
			A_Feedback.wave = Sine;
			if(C_Mixed.OUTPUT_MAG[BIndex] > 0.9/2)
					B_Feedback.wave = Sine;
				else
					B_Feedback.wave = Triangle;
		}
		else
		{
			A_Feedback.wave = Triangle;
			if(BIndex == AIndex * 3 || BIndex == AIndex * 5)
			{
				if(C_Mixed.OUTPUT_MAG[BIndex] > 0.9/2)
					B_Feedback.wave = Sine;
				else
					B_Feedback.wave = Triangle;
			}
			else
			{
				if(C_Mixed.OUTPUT_MAG[BIndex] > 0.9/2)
					B_Feedback.wave = Sine;
				else
					B_Feedback.wave = Triangle;
			}
		}
		AD9833_generate(0, AIndex*500, (A_Feedback.wave == Sine)?AD9833_OUT_SINUS:AD9833_OUT_TRIANGLE, AD9833_1);
		AD9833_generate(0, BIndex*500, (B_Feedback.wave == Sine)?AD9833_OUT_SINUS:AD9833_OUT_TRIANGLE, AD9833_2);
		label = 0;
	}
}


void adjust(void)
{
	// use 3 samples to identify the change of phase
	static float freqA;
	static float freqB;
	static int sampleCount = 0;
	static float biasA[10];
	static float biasB[10];
	static u8 label = 0;
	float ref = 0;
	if(freqA == 0 && freqB == 0)
	{
		freqA = 500 * AIndex;
		freqB = 500 * BIndex;
	}
	if(sampleCount < 10)
	{
		biasA[sampleCount] = PhaseBias32(C_Mixed.OUT[AIndex], A_Feedback.OUT[AIndex]);
		biasB[sampleCount] = PhaseBias32(C_Mixed.OUT[BIndex], B_Feedback.OUT[BIndex]);
		sampleCount++;
	}
	else if(sampleCount == 10)
	{
		sampleCount = 0;
		// Deal with A
		ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
		ref = 0;
		for(int i = 0; i < 10-1; i++)
		{
			ref += PhaseBiasAngle(biasA[i+1], biasA[i]);
		}
		ref /= 9;
		sprintf(str, "Ref A: %.2f", ref);
		ILI9341_DispString_EN(16, 50, str);
		if((label & 0x01) == 0)
		{
			if(ref < -10)
				freqA += 0.3;
			else if(ref < -1.5)
				freqA += 0.08;
			else if(ref < 1.5)
				label |= 0x01;
			else if(ref < 10)
				freqA -= 0.08;
			else
				freqA -= 0.3;
		}
		
		// Deal with B'
		ref = 0;
		for(int i = 0; i < 10-1; i++)
		{
			ref += PhaseBiasAngle(biasB[i+1], biasB[i]);
		}
		ref /= 9;
		sprintf(str, "Ref B: %.2f", ref);
		ILI9341_DispString_EN(16, 70, str);
		if((label & 0x02) == 0)
		{
			if(ref < -10)
				freqB += 0.3;
			else if(ref < -1.5)
				freqB += 0.08;
			else if(ref < 1.5)
				label |= 0x02;
			else if(ref < 10)
				freqB -= 0.08;
			else
				freqB -= 0.3;
		}
		
		if((label & 0x01) == 0x03)
			state = STABLE;
		
		AD9833_generate(0, freqA, (A_Feedback.wave == Sine)?AD9833_OUT_SINUS:AD9833_OUT_TRIANGLE, AD9833_1);
		AD9833_generate(0, freqB, (B_Feedback.wave == Sine)?AD9833_OUT_SINUS:AD9833_OUT_TRIANGLE, AD9833_2);
		Delay_ms(20);
	}
	else
		;
	
}


