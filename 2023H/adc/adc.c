#include "adc.h"

extern u32 IN3[];
int prescaler = 70;

// Enable PC for ADCs
static void ADCx_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// Enable TIM2_CC2 for ADC1&2
static void ADCx_TIM_Config(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_InternalClockConfig(TIM2);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 2 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = prescaler - 1;
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

// Enable ADC1_Channel_11 & ADC2_Channel_12, ADC3_Channel13
static void ADCx_Mode_Config(void) 
{ 
	ADC_InitTypeDef ADC_InitStructure; 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
	ADC_InitStructure.ADC_Mode = ADC_Mode_RegSimult; 
	ADC_InitStructure.ADC_ScanConvMode = ENABLE; 
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; 
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 
	ADC_InitStructure.ADC_NbrOfChannel = 2; 
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_Init(ADC2, &ADC_InitStructure);
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div4); 
	ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_1Cycles5);	// A'
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 2, ADC_SampleTime_1Cycles5);	// C
	ADC_RegularChannelConfig(ADC2, ADC_Channel_12, 1, ADC_SampleTime_1Cycles5);	// B'
	ADC_RegularChannelConfig(ADC2, ADC_Channel_14, 2, ADC_SampleTime_1Cycles5);	// NC
	ADC_DMACmd(ADC1, ENABLE);
	ADC_DMACmd(ADC2, ENABLE);
	ADC_ExternalTrigConvCmd(ADC1, ENABLE);
	ADC_ExternalTrigConvCmd(ADC2, ENABLE);
	ADC_Cmd(ADC1, ENABLE); 
	ADC_Cmd(ADC2, ENABLE); 
	ADC_ResetCalibration(ADC1); 
	while (ADC_GetResetCalibrationStatus(ADC1)); 
	ADC_StartCalibration(ADC1); 
	while (ADC_GetCalibrationStatus(ADC1));
	ADC_ResetCalibration(ADC2); 
	while (ADC_GetResetCalibrationStatus(ADC2)); 
	ADC_StartCalibration(ADC2); 
	while (ADC_GetCalibrationStatus(ADC2));
} 

// Enable DMA1_Channel1 for transfering from ADC1 to memory
static void ADCx_DMA_Config(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_BufferSize = NPT*2;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&(ADC1->DR);
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)(IN3);
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel1, DMA1_IT_TC1, ENABLE);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	NVIC_InitTypeDef NVIC_InitStructure;						
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;				
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;			
	NVIC_Init(&NVIC_InitStructure);		
	
	DMA_Cmd(DMA1_Channel1, ENABLE);
}


void ADCx_Init(void)
{
	ADCx_GPIO_Config();
	ADCx_TIM_Config();
	ADCx_DMA_Config();
	ADCx_Mode_Config();
}
