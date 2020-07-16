#ifndef _TIMER3_H_
#define _TIMER3_H_

#include "stm32f10x.h"
#include "host_usart.h"
#include "miscport.h"
#include "ADCSample.h"

#include "deca_device_api.h"

/*
		使用说明：
			初始化：
				TIM3_NVIC_Configuration(); 
				TIM3_Configuration(); 	
			中断函数：
				void TIM3_IRQHandler(void)
			 定时器初始化：
			    void Timer3_Init(void)  
*/
extern volatile u32 timer3_counter; //定时器 1ms计数

void TIM3_NVIC_Configuration(void);      //定时器中断配置函数 
void TIM3_Configuration(void);           //定时器IO配置函数
void Timer3_Init(void);	                 //定时器初始化

#endif
