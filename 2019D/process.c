#include "process.h"

u32 IN[NPT];
u32 OUT[NPT];
float OUTPUT_MAG[NPT/2];

float sampleRateP;
extern int prescaler;
extern float currentFrequency;

int label = 0;

// For Estimating Base Frequency 
int baseIndex = 0;
float baseMag = 0.0;


void CTR_GPIO_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
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

// Use lower 16 bits only
float calcMagOnly(u32 array[NPT])
{
	int i, j, k;
	float maxs[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float mins[11] = {4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096};
	float maxAverage, minAverage;
	for(i = 0; i < NPT; i++)
	{
		for(j = 0; j < 11; j++)
		{
			if(array[i] > maxs[j])
			{
				for(k = 10; k > j; k--)
				{
					maxs[k] = maxs[k-1];
				}
				maxs[j] = array[i];
				break;
			}
		}
		for(j = 0; j < 11; j++)
		{
			if(array[i] < mins[j])
			{
				for(k = 10; k > j; k--)
				{
					mins[k] = mins[k-1];
				}
				mins[j] = array[i];
				break;
			}
		}
	}
	for(i = 1; i < 11; i++)
	{
		maxAverage += maxs[i];
		minAverage += mins[i];
	}
	maxAverage /= 10;
	minAverage /= 10;
	return maxAverage - minAverage;
}

//void calcMag(void)
//{
//	int i;
//	cr4_fft_1024_stm32(OUT, IN, NPT);
//	for(i = 0; i < NPT/2; i++)
//	{
//		float real = (float)(((signed)OUT[i] / (1<<16))) / 4096.0f*3.3;
//		float imag = (float)(((signed)(OUT[i] << 16) / (1<<16))) / 4096.0f*3.3;
//		OUTPUT_MAG[i] = sqrt(SQUARE(real) + SQUARE(imag)) * (i?2:1);
//		if(i != 0 && OUTPUT_MAG[i] > baseMag)
//		{
//			baseIndex = i;
//			baseMag = OUTPUT_MAG[i];
//		}
//	}
//	// Refined Method to measure Base Frequency
//	// Accuracy improved DRAMATICALLY!!!
//	for(i = 1; i <= 5; i++)
//	{
//		int i1 = findMax(OUTPUT_MAG+baseIndex*i, 1);
//		int i2 = findMax(OUTPUT_MAG+baseIndex*i+i1, 2);
//		OUTPUT_MAG[i*baseIndex] = OUTPUT_MAG[i*baseIndex+i1]
//				/ sinc(OUTPUT_MAG[baseIndex*i+i1+i2]/(OUTPUT_MAG[baseIndex*i+i1]+OUTPUT_MAG[baseIndex*i+i1+i2]));
//	}
//}

void calcMag(void)
{
	int i;
	float real;
	float imag;
	baseIndex = 0;
	baseMag = 0;
	for(i = 0; i < NPT/2; i++)
	{
		real = Real(OUT[i]);
		imag = Imag(OUT[i]);
		OUTPUT_MAG[i] = sqrt(SQUARE(real) + SQUARE(imag)) * (i?2:1);
		if(i != 0 && OUTPUT_MAG[i] > baseMag)
		{
			baseIndex = i;
			baseMag = OUTPUT_MAG[i];
		}
	}
	if(currentFrequency < 110000)
		baseIndex = (int)(currentFrequency / (72000000.0/2/prescaler/1024) + 0.5);
	// Refined Method to measure Base Frequency
	// Accuracy improved DRAMATICALLY!!!
	
	if(baseIndex >= 3 && baseIndex <= NPT/2-3)
	{
		int i1 = findMax(OUTPUT_MAG+baseIndex, 1);
		int i2 = findMax(OUTPUT_MAG+baseIndex+i1, 2);
		OUTPUT_MAG[baseIndex] = OUTPUT_MAG[baseIndex+i1]
				/ sinc(OUTPUT_MAG[baseIndex+i1+i2]/(OUTPUT_MAG[baseIndex+i1]+OUTPUT_MAG[baseIndex+i1+i2]));
	}
	
}

void GetChannelxFeedback(int ADC1_Channelx, ChannelFeedback * const result)
{
	label = 0;
	ADC1_Init(ADC1_Channelx);
	while(1)
	{
		delay_ms(50);
		if(label == 1)
		{
			cr4_fft_1024_stm32(OUT, IN, NPT);
			calcMag();
 			*result = (ChannelFeedback){
				sampleRateP*baseIndex,
				OUTPUT_MAG[0],
				OUTPUT_MAG[baseIndex],
				Phase(Real(OUT[baseIndex]), Imag(OUT[baseIndex]))
			};
			return;
		}
	}
}
/*

float GetChannelxMag(int ADC1_Channelx, ChannelFeedback * const result)
{
	label = 0;
	ADC1_Init(ADC1_Channelx);
	while(1)
	{
		delay_ms(50);
		if(label == 1)
		{
			*result = (ChannelFeedback){
				1000,
				0,
				calcMagOnly(IN),
				0
			};
			return calcMagOnly(IN);
		}
	}
}

void GetDualChannelMags(ChannelFeedback * const feedbacks)
{
	int i;
	label = 0;
	u32 IN2[NPT];
	u32 *IN1 = IN;
	DualADC_Init();
	// feedbacks[0] -> ADC1_Channel11 -> PC1 -> IN1
	// feedbacks[1] -> ADC2_Channel12 -> PC2 -> IN2
	while(1)
	{
		delay_ms(50);
		if(label == 1)
		{
			for(i = 0; i < NPT; i++)
			{
				IN2[i] = IN[i] >> 16;
				IN1[i] = IN[i] & 0x0000ffff;
			}
			feedbacks[0].frequency = feedbacks[1].frequency = 1000;
			feedbacks[0].magnitude = calcMagOnly(IN1);
			feedbacks[1].magnitude = calcMagOnly(IN2);
		}
	}
}

*/
float GetDualChannelFeedback(ChannelFeedback * const feedbacks)
{
	int i;
	label = 0;
	u32 IN1[NPT];
	u32 *IN2 = IN;
	u32 base1, base2;
	DualADC_Init();
	// feedbacks[0] -> ADC1_Channel11 -> PC1
	// feedbacks[1] -> ADC2_Channel12 -> PC2
	while(1)
	{
		delay_ms(50);
		if(label == 1)
		{
			for(i = 0; i < NPT; i++)
			{
				IN1[i] = IN[i] << 16;
				IN2[i] = IN[i] & 0xffff0000;
			}
			cr4_fft_1024_stm32(OUT, IN1, NPT);
			calcMag();
			base1 = OUT[baseIndex];
			if(feedbacks)
				feedbacks[0] = (ChannelFeedback){
					sampleRateP * baseIndex,
					OUTPUT_MAG[0],
					OUTPUT_MAG[baseIndex],
					Phase(Real(OUT[baseIndex]), Imag(OUT[baseIndex]))
				};
			cr4_fft_1024_stm32(OUT, IN2, NPT);
			calcMag();
			base2 = OUT[baseIndex];
			if(feedbacks)
				feedbacks[1] = (ChannelFeedback){
					sampleRateP * baseIndex,
					OUTPUT_MAG[0],
					OUTPUT_MAG[baseIndex],
					Phase(Real(OUT[baseIndex]), Imag(OUT[baseIndex]))
				};
			return PhaseBias32(base1, base2);
		}
	}
}

void DMA1_Channel1_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA_IT_TC))
	{
		DMA_Cmd(DMA1_Channel1, DISABLE);
		ADC_Cmd(ADC1, DISABLE);
		ADC_Cmd(ADC2, DISABLE);
		label = 1;
		DMA_ClearITPendingBit(DMA_IT_TC);
	}
}

