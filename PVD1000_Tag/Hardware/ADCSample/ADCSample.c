#include "ADCSample.h"

#define ADC1_DR_Address    ((u32)0x40012400+0x4c)

#define ADC_BUFFER_SIZE		(715 * 2)	//俩个周期（50hz）,即采样时间总长度为40ms
__IO uint16_t ADC_ConvertedValue[ADC_BUFFER_SIZE];

u16 electronicValue = 0;
int transE = 0;
u16 elevtronicThreshold = 1000;

/*ADC GPIO PB1---ADC1 channel 9*/

/*
 * 函数名：ADC1_GPIO_Config
 * 描述  ：使能ADC1，初始化PA.0
 * 输入  : 无
 * 输出  ：无
 * 调用  ：内部调用
 */
 void ADC1_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	/* Enable ADC1 and GPIOB clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);//开启ADC时钟
	
	/* Configure PA0  as analog input */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				// PB1,输入时不用设置速率
}


/* 函数名：ADC1_Mode_Config
 * 描述  ：配置ADC1的工作模式为MDA模式
 * 输入  : 无
 * 输出  ：无
 * 调用  ：内部调用
 */
void ADC1_Mode_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	
	/* DMA channel1 configuration */
	DMA_DeInit(DMA1_Channel1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;	 //ADC地址
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_ConvertedValue[0];//内存地址
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = ADC_BUFFER_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设地址固定
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;  //内存地址递增
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//半字
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		//循环传输
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	
	/* Enable DMA channel1 */
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	/* ADC1 configuration */
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	//独立ADC模式
	ADC_InitStructure.ADC_ScanConvMode = DISABLE ; 	 //禁止扫描模式，扫描模式用于多通道采集
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	//开启连续转换模式，即不停地进行ADC转换
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//不使用外部触发转换
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 	//采集数据右对齐
	ADC_InitStructure.ADC_NbrOfChannel = 1;	 	//要转换的通道数目1
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/*配置ADC时钟，为PCLK2的8分频，即9MHz*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div8); 
	/*配置ADC1的通道9为239.5个采样周期，序列为1 */ 
	/*sample time: Tall = (12.5 + Tperiod)/fs*/
	/*sample time: Tall = 252 / 9Mhz = 28us*/
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
	
	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);
	
	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);
	
	/*复位校准寄存器 */   
	ADC_ResetCalibration(ADC1);
	/*等待校准寄存器复位完成 */
	while(ADC_GetResetCalibrationStatus(ADC1));
	
	/* ADC校准 */
	ADC_StartCalibration(ADC1);
	/* 等待校准完成*/
	while(ADC_GetCalibrationStatus(ADC1));
	
	/* 由于没有采用外部触发，所以使用软件触发ADC转换 */ 
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}


void ADC1_Init(void)
{
	ADC1_GPIO_Config();
	ADC1_Mode_Config();
}

int GetAngle(void)
{
	int value = 0;
	value = (ADC_ConvertedValue[0]&0x0fff)*3300/4096;
	return value;
}

#define VCTL_NUM	8
u16 CalSinPara() {
	u16 maxval = 0, minval = 0xffff, avgval = 0;
	u32 sumval = 0;
	double rmsval = 0, dtemp = 0;
	double avgVol = 0, maxVol = 0;
	u16 i = 0, temp = 0;
	static u16 Vol[VCTL_NUM] = {980, 829, 679, 520, 369, 281, 130, 0};
	static u8  AMN[VCTL_NUM] = {1,   2,   4,   6,   12,  25,  50,  100};
	static u8  index = 0, printCount = 0;
	u8 preAMN = 0;
	
	ADC_SoftwareStartConvCmd(ADC1, DISABLE);
	
	for(i = 0; i < ADC_BUFFER_SIZE; i++) {
		temp = ADC_ConvertedValue[i]&0x0fff;
		if(temp > maxval) {
			maxval = temp;
		}
		if(temp < minval) {
			minval = temp;
		}
		sumval += temp;
	}
	avgval = sumval / ADC_BUFFER_SIZE;
	avgVol = avgval * 3300 / 4096.0;
	
	maxVol = maxval * 3300.0 / 4096.0 - avgVol;
	
	for(i = 0; i < ADC_BUFFER_SIZE; ++i) {
		temp = ADC_ConvertedValue[i]&0x0fff;
		//计算真有效值, LSM = 3.3 / 4096, 先转为10LSM，因为以3300 / 4096计算时平方累加会发生溢出
		dtemp = temp*33/4096.0 - avgVol / 100.0;
		rmsval += (dtemp * dtemp);
	}
	rmsval = 100 * sqrt(rmsval / ADC_BUFFER_SIZE);
	
	preAMN = AMN[index];
	//自动根据采样值调整放大倍数
	if(rmsval > 1000.0) {
		if(index < (VCTL_NUM - 1)) {
			index += 1;
			setAGCValue(Vol[index]);
		}
	} else if(rmsval < 10.0){
		if(index > 0) {
			index -= 1;
			setAGCValue(Vol[index]);
		}
	}
	rmsval = rmsval * preAMN;
	maxVol = maxVol * preAMN;
	
	transE = (7.903 * rmsval - 11.935) / 1;
	if(transE < 0) {
		transE = 0;
	}
	
	electronicValue = transE / 1;
	
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	
	//printf("av:%.1f ma:%.1f rm:%.1f, N:%d\n", avgVol, maxVol, rmsval, AMN[index]);
	printCount = (printCount + 1) & 0x07;
	if(printCount == 0)	debug_printf("E:%d V/m, rms:%.1f, N:%d\n", transE, rmsval, preAMN);
	return maxval;
}

u16 getElectronicValue(void) {
	return electronicValue;
}

u8 setElectronicThreshold(void* dat) {
	char *data = (char *)dat;
	if((data[0] == 'E' || data[0] == 'e') && data[1] == ':') {
		sscanf((data+2), "%hu", &elevtronicThreshold);
		printf("rec:E:%d\n", elevtronicThreshold);
		return 0;
	}
	return 1;
}

u8 checkElecValue(void) {
	if(elevtronicThreshold < transE) {
		return 1;
	}
	return 0;
}
