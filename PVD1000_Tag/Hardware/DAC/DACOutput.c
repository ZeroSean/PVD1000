#include "DACOutput.h"

void DAC1_Init(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	DAC_InitTypeDef DAC_InitType;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//使能PA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);		//使能DAC时钟
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;				//端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;			//模拟输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//初始化GPIOA
	GPIO_SetBits(GPIOA, GPIO_Pin_4);						///PA.4 输出高
	
	DAC_InitType.DAC_Trigger = DAC_Trigger_None;				//不使用触发功能
	DAC_InitType.DAC_WaveGeneration = DAC_WaveGeneration_None;	//不使用波形发生
	DAC_InitType.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;
	DAC_InitType.DAC_OutputBuffer = DAC_OutputBuffer_Disable;	//DAC1输出缓存关
	DAC_Init(DAC_Channel_1, &DAC_InitType);		//初始化DAC通道1
	
	DAC_Cmd(DAC_Channel_1, ENABLE);			//使能DAC1
	DAC_SetChannel1Data(DAC_Align_12b_R, 0);	//12位右对其，设置DAC初始值
	
	setAGCValue(980);
}


void setAGCValue(u16 value) {
	double temp = value;
	u16 dat = 0;
	temp /= 1000.0;
	temp = temp * 4096.0 / 3.3;
	dat = temp / 1;
	DAC_SetChannel1Data(DAC_Align_12b_R, (dat & 0xfff));
}

