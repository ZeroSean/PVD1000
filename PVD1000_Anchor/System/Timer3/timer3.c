#include "timer3.h"

/*********************************************** 
 * 函数名：TIM3_NVIC_Configuration
 * 描  述: TIM3中断优先级配置
 * 输  入：无
 * 输  出: 无
 ***********************************************/
void TIM3_NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure; 
    
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);  		    //选择中断分组1：3位抢占优先级，1位响应优先级
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;	  	 	    //选择TIM3的中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;	//抢占式中断优先级设置为
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;			//响应式中断优先级设置为 
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//使能中断  
    NVIC_Init(&NVIC_InitStructure);		                   	    //初始化NVIC	
}

/*********************************************** 
 * 函数名：TIM3_Configuration
 * 描  述: TIM3配置    中断周期为1ms
 * 输  入：无
 * 输  出: 无
 ***********************************************/
void TIM3_Configuration(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 , ENABLE);
	TIM_DeInit(TIM3);

	//采用内部时钟给TIM3提供时钟源 
	TIM_InternalClockConfig(TIM3); 
	//自动重装载寄存器周期的值(计数值) 	 10ms	
	/* 累计 TIM_Period个频率后产生一个更新或者中断 */
	TIM_TimeBaseStructure.TIM_Period=10000;			
	//时钟预分频数 72M/72    
	TIM_TimeBaseStructure.TIM_Prescaler= (72 - 1);
	//采样分频 	
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	//向上计数模式 	
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; 
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_ClearFlag(TIM3, TIM_FLAG_Update);	  	 //清除溢出中断标志 
	TIM_ARRPreloadConfig(TIM3, DISABLE);    	 //禁止ARR预装载缓冲器
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);	 //允许定时器3中断

	TIM_Cmd(TIM3, ENABLE);	//开启时钟   
		
}
/*********************************************** 
 * 函数名：Timer3_Init
 * 描  述: TIM3定时1ms初始化
 * 输  入：无
 * 输  出: 无
 ***********************************************/
void Timer3_Init(void)
{
	TIM3_Configuration();
	TIM3_NVIC_Configuration();
}


//注意：不能与stm32f103x_it.c文件的中断函数冲突
volatile u32 timer3_counter = 0; // ms 计时变量  volatile防止编辑器主动优化，导致无法计数

/*********************************************** 
 * 函数名：TIM3_IRQHandler
 * 描  述: TIM3定时1ms中断处理程序
 * 输  入：无
 * 输  出: 无
 ***********************************************/
void TIM3_IRQHandler(void)
{ 
	 //定时10ms中断
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {		
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);  //清除中断标志 
		timer3_counter++;
		if(timer3_counter >= 100) {
			timer3_counter = 0;
			LED_STATUS1 = LED_STATUS1?0:1;		
		}
	}	
}

