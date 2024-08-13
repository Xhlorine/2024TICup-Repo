#include "stm32f10x.h"
#include <stdio.h>
#include "bsp_ili9341_lcd.h"
#include "bsp_xpt2046_lcd.h"
#include "bsp_usart.h"
#include "palette.h"
#include "AD9833.h"
#include "delay.h"
#include "process.h"

#define NEAR(a, b, c) (ABS((a)-(b))<(c))

#define POINTS 40
#define COEF_CI (1/105000.0f)
#define COEF_VI (1/56.0f)
#define COEF_VO (5.1f)

// Current In:  mag / 105k (A)
#define CurrentIn  (results[1].magnitude * COEF_CI)
// Voltage In:  mag / 56   (V)
#define VoltageIn  (results[0].magnitude * COEF_VI)
// Voltage Out: mag * 5.1  (V)
#define VoltageOut (results[2].magnitude * COEF_VO)
// Voltage Out: (2.6 - sample) * 5.1
#define VoltageOutDirect ((2.6 - results[2].direct) * COEF_VO)

char str[50] = "Hello World!";
float AD9833_Frequency = 1000;
extern Detector_state screenState;
extern int prescaler;

float VIs[NPT];
char errors[][15] = {"No Error", "C1 Open", "R1 Open", "C2 Open", "R4 Open", "C3 Open", "C3 Double", "C2 Double", "C1 Double", "R3 Short", "R3 Open", "R2 Open", "R4 Short", "R1/2 Short", "R2 Short"};
/*
 * results[0]: Voltage in  -> PC1
 * results[1]: Current in  -> PC2
 * results[2]: Voltage out -> PC3
 */
ChannelFeedback results[3];
float currentFrequency;
float ResistanceIn;
float ResistanceOut;
float Gain1kHz;
float Gain100Hz;
float Gain200kHz;
float Phase10Hz;
float UpperCutoffFreq;
float Gains[POINTS];

void Detect_Init(void);
void refineSampleRate(float frequency);
float calcResistanceIn(void);
float calcResistanceOut(void);
float calcGain(float frequency);
float plotFreq_Gain(int reCalc);
int detectError(int reCalc);


void showInfo(const ChannelFeedback *feedback, int position)
{
	// Base Frequency
	sprintf(str, "BaseF: %.2fkHz", feedback->frequency/1000.0);
	ILI9341_DispString_EN(120, 70*position, str);
	// Direct Component
	sprintf(str, "Direct: %.2f", feedback->direct);
	ILI9341_DispString_EN(120, 70*position+16, str);
	// Harmonic Component
	sprintf(str, "Base: %.3f", feedback->magnitude);
	ILI9341_DispString_EN(120, 70*position+32, str);
	// Harmonic Phase Angle
	sprintf(str, "Phase: %.3f", feedback->phase);
	ILI9341_DispString_EN(120, 70*position+48, str);
}

int main(void)
{
	Detector_state lastState  = STARTUP_0;
	int error = 0;
	CTR_GPIO_Init();
	ILI9341_Init();
	XPT2046_Init();
	ILI9341_GramScan(3);
	ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
	Detect_Init();
	GPIO_WriteBit(GPIOC, GPIO_Pin_8, Bit_RESET);
	Palette_Init(LCD_SCAN_MODE);
	AD9833_generate(AD9833_Frequency, AD9833_OUT_SINUS);
	
	while(1)
	{
		switch(screenState)
		{
		case STARTUP_0:
			break;
		case STARTUP_1:
			break;
		case PARA_DETECTOR_0: // ResistanceIn
			if(lastState != screenState)
				ResistanceIn = calcResistanceIn();
			sprintf(str, "%.2f Ohm", ResistanceIn);
			ILI9341_DispString_EN(140, 10, str);
			break;
		case PARA_DETECTOR_1: // ResistanceOut
			if(lastState != screenState)
				ResistanceOut = calcResistanceOut();
			sprintf(str, "%.1f Ohm", ResistanceOut);
			ILI9341_DispString_EN(140, 50, str);
			break;
		case PARA_DETECTOR_2: // 1kHz Gain
			if(lastState != screenState)
				Gain1kHz = calcGain(1000);
			sprintf(str, "%.1f V/V", Gain1kHz);
			ILI9341_DispString_EN(140, 90, str);
			break;
		case PARA_DETECTOR_3: // Frequency - Gain Plot
			if(lastState != screenState)
				UpperCutoffFreq = plotFreq_Gain(1);
			else
				plotFreq_Gain(0);
			sprintf(str, "f_H: %.2fkHz", UpperCutoffFreq / 1000.0);
			ILI9341_DispString_EN(140, 210, str);
			break;
		case PARA_DETECTOR_4:
			break;
		case ERROR_DETECTOR_0: // Error Detecting
			if(lastState == screenState)
				error = detectError(0);
			else
				error = detectError(1);
			ILI9341_DispString_EN(16, 32, errors[error]);
			break;
		case ERROR_DETECTOR_1:
			break;
		default:
			break;
		}
		lastState = screenState;
		XPT2046_TouchEvenHandler();
	}
}

void refineSampleRate(float frequency)
{
	
	if(frequency < 1000)
	{
		prescaler = 72000000 / frequency / 128;
	}
	else if(frequency > 100000)
	{
		prescaler = 35;
	}
	else
	{
		prescaler = 176;
	}
	currentFrequency = frequency;
	TIM_PrescalerConfig(TIM2, prescaler, TIM_PSCReloadMode_Update);
}


float calcPhaseBias(float frequency)
{
	float sum = 0;
	refineSampleRate(frequency);
	AD9833_generate(frequency, AD9833_OUT_SINUS);
	for(int i = 0; i < 8; i++)
	{
		sum += GetDualChannelFeedback(results);
	}
	return sum / 8;
}


float calcResistanceIn(void)
{
	int i;
	float sum = 0.0;
	refineSampleRate(1000);
	AD9833_generate(1000, AD9833_OUT_SINUS);
	delay_ms(50);
	for(i = 0; i < 8; i++)
	{
		GetDualChannelFeedback(results);
		if(CurrentIn == 0)
			return 99999999;
		sum += VoltageIn / CurrentIn;
	}
	return sum / 8;
}


float calcResistanceOut(void)
{
	float VoltageOut_ON;
	float VoltageOut_OFF;
	int i, j;
	float sum = 0;
	refineSampleRate(1000);
	AD9833_generate(1000, AD9833_OUT_SINUS);
	for(i = 0; i < 5; i++)
	{
		float sum1 = 0;
		for(j = 0; j < 4; j++)
		{
			GetChannelxFeedback(13, results + 2);
			VoltageOut_OFF = VoltageOut;
			GPIO_WriteBit(GPIOC, GPIO_Pin_8, Bit_SET);
			delay_ms(20);
			GetChannelxFeedback(13, results + 2);
			VoltageOut_ON = VoltageOut;
			GPIO_WriteBit(GPIOC, GPIO_Pin_8, Bit_RESET);
			delay_ms(20);
			if(ABS(VoltageOut_ON) < 0.002)
				return -1;
			sum1 += VoltageOut_OFF/VoltageOut_ON;
					//102000*(5/(6-VoltageOut_OFF/VoltageOut_ON)-1);
					//(VoltageOut_OFF/VoltageOut_ON - 1)*20000;
					//1000*(5100/(51-VoltageOut_OFF/VoltageOut_ON)-102);
		}
		sum1 /= 4;
		sum += (sum1-1)*20000;
		//16.7 * 102000 * (sum1 - 1)/(102 - 16.7*sum1);
		sum1 = 0;
	}
	sum /= 5;
	return sum;
		
}

float calcGain(float frequency)
{
	int i;
	float sum = 0.0;
	refineSampleRate(frequency);
	AD9833_generate(frequency, AD9833_OUT_SINUS);
	delay_ms(20);
	for(i = 0; i < 8; i++)
	{
		GetChannelxFeedback(11, results);
		GetChannelxFeedback(13, results + 2);
		sum += VoltageOut / VoltageIn;
	}
	return sum / 8;
}


#define DENSITY 30
#define XSTART 150.0f
#define XEND 300.0f
#define YSTART 20.0f
#define YEND 184.0f
#define WIDTH (float)(XEND - XSTART)
#define HEIGHT (float)(YEND - YSTART)

float plotFreq_Gain(int reCalc)
{
	int i, j;
	static float maxGain = 0.0;
	float maxGainFreqlog = 0.0;
	float upperCutoffFreq = 0.0;
	float upperCutoffGain = 0.0;
	float Freqslog[POINTS];
	float frequency;
	if(reCalc == 0)
		return maxGain;
	if(reCalc == 1)
	{
		maxGain = 0.0;
		for(i = 0; i < POINTS; i++)
		{
			Freqslog[i] = 2 + 3.3*i/(POINTS-1);
		}
		for(i = 0; i < POINTS; i++)
		{
			frequency = pow(10, Freqslog[i]);
			// Consider apply equavilent sample to refine the performance?
			if(frequency  < 10000)			prescaler = 703;
			else if(frequency < 30000)		prescaler = 351;
			else if(frequency < 1600000)	prescaler = 70;
			else							prescaler = 35;
			Gains[i] = calcGain(frequency);
			if(Gains[i] > maxGain)
			{
				maxGainFreqlog = Freqslog[i];
				maxGain = Gains[i];
			}
		}
	}
	// x-axis(upper & lower)
	for(i = XSTART; i < XEND; i++)
	{
		ILI9341_SetPointPixel(i, YSTART);
		ILI9341_SetPointPixel(i, YEND);
	}
	// y-axis(left & right)
	for(i = YSTART; i < YEND; i++)
	{
		ILI9341_SetPointPixel(XSTART, i);
		ILI9341_SetPointPixel(XEND, i);
	}
	// Axes labels
	ILI9341_DispString_EN(XSTART+0.5*WIDTH-8, YEND+1, "Log");
	ILI9341_DispString_EN(XSTART, YEND+1, "100");
	ILI9341_DispString_EN(XEND-8*4, YEND+1, "200k");
	ILI9341_DispString_EN(XSTART, YSTART-16, "Linear");
	ILI9341_DispString_EN(XSTART-8, YEND-16, "0");
	sprintf(str, "%.0f", maxGain);
	ILI9341_DispString_EN(XSTART-3*8, YEND-HEIGHT/1.1, str);
	sprintf(str, "%.0f", maxGain*0.707);
	ILI9341_DispString_EN(XSTART-3*8, YEND-HEIGHT/1.1*0.707, str);
	for(i = 0; i < POINTS; i++)
	{
		// Relative Coordination
		float x = i * WIDTH / (POINTS - 1);
		float y = Gains[i] / (maxGain * 1.1) * HEIGHT;
		if(i < POINTS - 1)
		{
			float yNext = Gains[i+1] / (maxGain * 1.1) * HEIGHT;
			for(j = 0; j < DENSITY; j++)
			{
				float gain = (Gains[i+1] - Gains[i]) * j / DENSITY + Gains[i];
				ILI9341_SetPointPixel(XSTART + x + j*WIDTH/POINTS/DENSITY, YEND - (y + j * (yNext - y) / DENSITY));
				// Find the upper cut-off frequency: closest to 0.707 * maxGain
				if(ABS(maxGain*0.707 - gain) < ABS(maxGain*0.707 - upperCutoffGain) && Freqslog[i] > maxGainFreqlog)
				{
					upperCutoffFreq = pow(10, Freqslog[i] + 3.69897*j/DENSITY/(POINTS-1));
					upperCutoffGain = gain;
				}
			}
		}
		ILI9341_SetPointPixel(XSTART + x, YEND - y);
		ILI9341_SetPointPixel(XSTART + x+1, YEND - y);
		ILI9341_SetPointPixel(XSTART + x, YEND - y+1);
		ILI9341_SetPointPixel(XSTART + x-1, YEND - y);
		ILI9341_SetPointPixel(XSTART + x, YEND - y-1);
		ILI9341_SetPointPixel(XSTART + x+1, YEND - y+1);
		ILI9341_SetPointPixel(XSTART + x-1, YEND - y+1);
		ILI9341_SetPointPixel(XSTART + x-1, YEND - y-1);
		ILI9341_SetPointPixel(XSTART + x+1, YEND - y-1);
	}
	return upperCutoffFreq;
}


void Detect_Init(void)
{
	ILI9341_DispString_EN(16, 32, "Initializing.");
	refineSampleRate(1000);
	AD9833_generate(1000, AD9833_OUT_SINUS);
	ResistanceIn = calcResistanceIn();
	ILI9341_DispString_EN(16, 32, "Initializing..");
	Gain1kHz = calcGain(1000);
	Gain200kHz = calcGain(200000);
	Gain100Hz = calcGain(100);
	ILI9341_DispString_EN(16, 32, "Initializing...");
	Phase10Hz = calcPhaseBias(10);
}


int detectError(int reCalc)
{
	// Fisrt of all, guarantee all the parameters needed are ready
	static int ans = 0;
	if(reCalc == 0)
		return ans;
	ILI9341_DispString_EN(16, 16, "Detecting.");
	float RinError = calcResistanceIn();
	float Gain1kHzError = calcGain(1000);
	float VoDirectError = VoltageOutDirect;
	if(RinError > 10 * ResistanceIn)
	{
		// C1 Open 1
		ans = 1;
	}
	else if(RinError > 6.5 * ResistanceIn)
	{
		if(NEAR(VoDirectError, 11, 0.70))
		{
			// R1 Open 1
			ans = 2;
		}
	}
	else if(RinError > 3.5 * ResistanceIn)
	{
		if(NEAR(VoDirectError, 11, 0.70))
		{
			// R4 Open 1
			ans = 4;
		}
		else if(NEAR(VoDirectError, 6.8, 0.80))
		{
			// C2 Open 1
			ans = 3;
		}
		else
			goto ERROR;
	}
	else if(NEAR(RinError/ResistanceIn, 1, 0.2))
	{
		float phaseB = calcPhaseBias(10);
//		sprintf(str, "%.2f", Phase10Hz - phaseB);
//		ILI9341_DispString_EN(16, 128, str);
		ILI9341_DispString_EN(16, 16, "Detecting..");
		if(phaseB < Phase10Hz - 4)
		{
			// C1 Double
			ans = 8;
		}
		else
		{
			float Gain200kHzError = calcGain(200000);
			
//			ILI9341_DispString_EN(16, 96, str);
//			sprintf(str, "Gain200k: %f", Gain200kHzError/Gain200kHz);
			if(NEAR(VoDirectError, 12, 0.5))
			{
				// R3 Short 1
				ans = 9;
			}
			else if(Gain200kHzError/Gain200kHz > 1.2)
			{
				// C3 Open 1
				ans = 5;
			}
			else if(Gain200kHzError/Gain200kHz < 0.7)
			{
				// C3 Double
				ans = 6;
			}
			else
			{
				ILI9341_DispString_EN(16, 16, "Detecting...");
				float Gain100HzError = calcGain(100);
				
//				ILI9341_DispString_EN(16, 80, str);
//				sprintf(str, "Gain100: %f", Gain100HzError/Gain100Hz);
				if(Gain100HzError/Gain100Hz > 1.4)
				{
					// C2 Double
					ans = 7;
				}
				else
					goto ERROR;
			}
		}
	}
	else if(RinError > ResistanceIn * 0.088)
	{
		if(VoDirectError < 1)
		{
			// R3 Open 1
			ans = 10;
		}
	}
	else if(RinError < ResistanceIn * 0.088)
	{
		if(NEAR(VoDirectError, 3.5, 1))
		{
			// R2 Open 1
			ans = 11;
		}
		else if(VoDirectError < 0.5)
		{
			// R4 Short 1
			ans = 12;
		}
		else if(NEAR(VoDirectError, 10.9, 0.4))
		{
			// R1 Short 1
			ans = 13;
		}
		else if(NEAR(VoDirectError, 11.4, 0.7))
		{
			// R2 Short 1
			ans = 14;
		}
		else
			goto ERROR;
	}
	else
	{
		// ERROR: NO ERROR
		ERROR:
		{
			ans = 0;
		}
	}
//	sprintf(str, "Rin: %f", RinError/ResistanceIn);
//	ILI9341_DispString_EN(16, 48, str);
	sprintf(str, "VoD: %f", VoDirectError);
	ILI9341_DispString_EN(16, 64, str);
//	sprintf(str, "Gain1k: %f", Gain1kHzError/Gain1kHz);
//	ILI9341_DispString_EN(16, 112, str);
	if(!ans)
		ILI9341_Clear(16, 32, 12*8, 16);
	return ans;
	
}
