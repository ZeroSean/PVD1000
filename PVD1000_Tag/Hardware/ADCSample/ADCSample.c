#include "ADCSample.h"

#define ADC1_DR_Address    ((u32)0x40012400+0x4c)

#define ADC_BUFFER_SIZE		(715 * 2)	//�������ڣ�50hz��,������ʱ���ܳ���Ϊ40ms
__IO uint16_t ADC_ConvertedValue[ADC_BUFFER_SIZE];

u16 electronicValue = 0;
int transE = 0;
u16 elevtronicThreshold = 1000;

/*ADC GPIO PB1---ADC1 channel 9*/

/*
 * ��������ADC1_GPIO_Config
 * ����  ��ʹ��ADC1����ʼ��PA.0
 * ����  : ��
 * ���  ����
 * ����  ���ڲ�����
 */
 void ADC1_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	/* Enable ADC1 and GPIOB clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);//����ADCʱ��
	
	/* Configure PA0  as analog input */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				// PB1,����ʱ������������
}


/* ��������ADC1_Mode_Config
 * ����  ������ADC1�Ĺ���ģʽΪMDAģʽ
 * ����  : ��
 * ���  ����
 * ����  ���ڲ�����
 */
void ADC1_Mode_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	
	/* DMA channel1 configuration */
	DMA_DeInit(DMA1_Channel1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;	 //ADC��ַ
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_ConvertedValue[0];//�ڴ��ַ
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = ADC_BUFFER_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//�����ַ�̶�
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;  //�ڴ��ַ����
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//����
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		//ѭ������
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	
	/* Enable DMA channel1 */
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	/* ADC1 configuration */
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	//����ADCģʽ
	ADC_InitStructure.ADC_ScanConvMode = DISABLE ; 	 //��ֹɨ��ģʽ��ɨ��ģʽ���ڶ�ͨ���ɼ�
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	//��������ת��ģʽ������ͣ�ؽ���ADCת��
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//��ʹ���ⲿ����ת��
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 	//�ɼ������Ҷ���
	ADC_InitStructure.ADC_NbrOfChannel = 1;	 	//Ҫת����ͨ����Ŀ1
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/*����ADCʱ�ӣ�ΪPCLK2��8��Ƶ����9MHz*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div8); 
	/*����ADC1��ͨ��9Ϊ239.5���������ڣ�����Ϊ1 */ 
	/*sample time: Tall = (12.5 + Tperiod)/fs*/
	/*sample time: Tall = 252 / 9Mhz = 28us*/
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
	
	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);
	
	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);
	
	/*��λУ׼�Ĵ��� */   
	ADC_ResetCalibration(ADC1);
	/*�ȴ�У׼�Ĵ�����λ��� */
	while(ADC_GetResetCalibrationStatus(ADC1));
	
	/* ADCУ׼ */
	ADC_StartCalibration(ADC1);
	/* �ȴ�У׼���*/
	while(ADC_GetCalibrationStatus(ADC1));
	
	/* ����û�в����ⲿ����������ʹ���������ADCת�� */ 
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
		//��������Чֵ, LSM = 3.3 / 4096, ��תΪ10LSM����Ϊ��3300 / 4096����ʱƽ���ۼӻᷢ�����
		dtemp = temp*33/4096.0 - avgVol / 100.0;
		rmsval += (dtemp * dtemp);
	}
	rmsval = 100 * sqrt(rmsval / ADC_BUFFER_SIZE);
	
	preAMN = AMN[index];
	//�Զ����ݲ���ֵ�����Ŵ���
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
