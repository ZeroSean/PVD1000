#include "timer3.h"

/*********************************************** 
 * ��������TIM3_NVIC_Configuration
 * ��  ��: TIM3�ж����ȼ�����
 * ��  �룺��
 * ��  ��: ��
 ***********************************************/
void TIM3_NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure; 
    
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);  		    //ѡ���жϷ���1��3λ��ռ���ȼ���1λ��Ӧ���ȼ�
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;	  	 	    //ѡ��TIM3���ж�ͨ��
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;	//��ռʽ�ж����ȼ�����Ϊ
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;			//��Ӧʽ�ж����ȼ�����Ϊ 
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//ʹ���ж�  
    NVIC_Init(&NVIC_InitStructure);		                   	    //��ʼ��NVIC	
}

/*********************************************** 
 * ��������TIM3_Configuration
 * ��  ��: TIM3����    �ж�����Ϊ1ms
 * ��  �룺��
 * ��  ��: ��
 ***********************************************/
void TIM3_Configuration(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 , ENABLE);
	TIM_DeInit(TIM3);

	//�����ڲ�ʱ�Ӹ�TIM3�ṩʱ��Դ 
	TIM_InternalClockConfig(TIM3); 
	//�Զ���װ�ؼĴ������ڵ�ֵ(����ֵ) 	 10ms	
	/* �ۼ� TIM_Period��Ƶ�ʺ����һ�����»����ж� */
	TIM_TimeBaseStructure.TIM_Period=10000;			
	//ʱ��Ԥ��Ƶ�� 72M/72    
	TIM_TimeBaseStructure.TIM_Prescaler= (72 - 1);
	//������Ƶ 	
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	//���ϼ���ģʽ 	
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; 
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_ClearFlag(TIM3, TIM_FLAG_Update);	  	 //�������жϱ�־ 
	TIM_ARRPreloadConfig(TIM3, DISABLE);    	 //��ֹARRԤװ�ػ�����
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);	 //����ʱ��3�ж�

	TIM_Cmd(TIM3, ENABLE);	//����ʱ��   
		
}
/*********************************************** 
 * ��������Timer3_Init
 * ��  ��: TIM3��ʱ1ms��ʼ��
 * ��  �룺��
 * ��  ��: ��
 ***********************************************/
void Timer3_Init(void)
{
	TIM3_Configuration();
	TIM3_NVIC_Configuration();
}


//ע�⣺������stm32f103x_it.c�ļ����жϺ�����ͻ
volatile u32 timer3_counter = 0; // ms ��ʱ����  volatile��ֹ�༭�������Ż��������޷�����

/*********************************************** 
 * ��������TIM3_IRQHandler
 * ��  ��: TIM3��ʱ1ms�жϴ������
 * ��  �룺��
 * ��  ��: ��
 ***********************************************/
void TIM3_IRQHandler(void)
{ 
	 //��ʱ10ms�ж�
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {		
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);  //����жϱ�־ 
		timer3_counter++;
		if(timer3_counter >= 100) {
			timer3_counter = 0;
			LED_STATUS1 = LED_STATUS1?0:1;		
		}
	}	
}

