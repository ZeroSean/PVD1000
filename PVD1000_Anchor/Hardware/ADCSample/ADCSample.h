#ifndef _ADCSAMPLE_H_
#define _ADCSAMPLE_H_

#include "stm32f10x.h"
#include "host_usart.h"

/*ADC sampling program code, ADC port is PB1, channel is ADC1 channel9*/

void ADC1_GPIO_Config(void);		//IO口初始化
void ADC1_Mode_Config(void);		//AD配置
void ADC1_Init(void);						//AD采集初始化
int GetAngle(void);


#endif
