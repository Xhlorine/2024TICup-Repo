#include "adc.h"

extern u32 IN[];
extern float sampleRateP;
int prescaler = 35;

// Enable PC1 for ADC1_Channel_11
static void ADC1_GPIO_Config(void) 
{
	GPIO_InitTypeDef GPIO_InitStructure; 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; 
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// Enable TIM2_CC2 for ADC triggering
static void ADC1_TIM2_Config(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_InternalClockConfig(TIM2);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 2 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = prescaler - 1;
	sampleRateP = 72000000.0 / NPT / (TIM_TimeBaseInitStructure.TIM_Period + 1) / (TIM_TimeBaseInitStructure.TIM_Prescaler + 1);
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 1;
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);
	TIM_CCxCmd(TIM2, TIM_Channel_2, TIM_CCx_Enable);
	TIM_Cmd(TIM2, ENABLE);
	
	TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
}

// Enable ADC1_Channel_11
static void ADC1_Mode_Config(void) 
{ 
	ADC_InitTypeDef ADC_InitStructure; 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); 
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent; 
	ADC_InitStructure.ADC_ScanConvMode = DISABLE; 
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; 
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 
	ADC_InitStructure.ADC_NbrOfChannel = 1; 
	ADC_Init(ADC1, &ADC_InitStructure); 
	RCC_ADCCLKConfig(RCC_PCLK2_Div4); 
	ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_1Cycles5);
	ADC_DMACmd(ADC1, ENABLE);
	ADC_ExternalTrigConvCmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE); 
	ADC_ResetCalibration(ADC1); 
	while (ADC_GetResetCalibrationStatus(ADC1)); 
	ADC_StartCalibration(ADC1); 
	while (ADC_GetCalibrationStatus(ADC1));
} 

// Enable DMA1_Channel1 for transfering from ADC1 to memory
static void ADC1_DMA1_Config(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_BufferSize = NPT;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&(ADC1->DR);
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)IN;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	NVIC_InitTypeDef NVIC_InitStructure;						
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;				
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;			
	NVIC_Init(&NVIC_InitStructure);	
}


void ADC1_Init(void)
{
	ADC1_GPIO_Config();
	ADC1_TIM2_Config();
	ADC1_DMA1_Config();
	ADC1_Mode_Config();
}
