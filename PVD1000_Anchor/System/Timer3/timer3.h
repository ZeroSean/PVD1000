#ifndef _TIMER3_H_
#define _TIMER3_H_

#include "stm32f10x.h"
#include "host_usart.h"
#include "miscport.h"
#include "ADCSample.h"

#include "deca_device_api.h"

/*
		ʹ��˵����
			��ʼ����
				TIM3_NVIC_Configuration(); 
				TIM3_Configuration(); 	
			�жϺ�����
				void TIM3_IRQHandler(void)
			 ��ʱ����ʼ����
			    void Timer3_Init(void)  
*/
extern volatile u32 timer3_counter; //��ʱ�� 1ms����

void TIM3_NVIC_Configuration(void);      //��ʱ���ж����ú��� 
void TIM3_Configuration(void);           //��ʱ��IO���ú���
void Timer3_Init(void);	                 //��ʱ����ʼ��

#endif
