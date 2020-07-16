/******************** ***************************       
 * Ӳ�����ӣ�------------------------
 *          | PA9  - USART1(Tx)      |
 *          | PA10 - USART1(Rx)      |
 *           -----------------------
************************************************/
#include "host_usart.h"
//#include <stdarg.h>
//#include "math.h"

struct PC_DAT pc_dat;
/*
 * ��������USART1_Init
 * ����  ��USART1 GPIO ����,����ģʽ���á�115200 8-N-1
 * ����  ����
 * ���  : ��
 * ����  ���ⲿ����
 */
void USART1_Init(u32 bound)
{
	//GPIO�˿�����
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO|RCC_APB2Periph_USART1, ENABLE);
	//USART1_TX   PA.9
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//USART1_RX	  PA.10
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);  

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQͨ��ʹ��
	NVIC_Init(&NVIC_InitStructure);	//����NVIC_InitStruct��ָ���Ĳ�����ʼ������NVIC�Ĵ���USART1

	//USART ��ʼ������ 
	USART_InitStructure.USART_BaudRate = bound;//һ������Ϊ9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//�����ж�
	USART_Cmd(USART1, ENABLE);                    //ʹ�ܴ��� 
	
	pc_dat.next = 0;
	pc_dat.pre = 0;
}

/*
 * ��������fputc
 * ����  ���ض���c�⺯��printf��USART1
 * ����  ����
 * ���  ����
 * ����  ����printf����
 */
int fputc(int ch, FILE *f)
{
	/* ��Printf���ݷ������� */
	USART_SendData(USART1, (unsigned char) ch);
	//	while (!(USART1->SR & USART_FLAG_TXE));
	while( USART_GetFlagStatus(USART1,USART_FLAG_TC)!= SET);	
	return (ch);
}

//=================������ͨ�շ�����============================================================
//=============================================================================================

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
** ��������: USART1_Send_Byte
** ��������: ���ڷ���һ���ַ�
** ����������Data Ҫ���͵�����
::::::::::::::::::::::::::::::::*/
void USART1_Send_Byte(uint16_t Data)
{ 
	while(!USART_GetFlagStatus(USART1,USART_FLAG_TXE));	  //USART_GetFlagStatus���õ�����״̬λ
														  //USART_FLAG_TXE:���ͼĴ���Ϊ�� 1��Ϊ�գ�0��æ״̬
	USART_SendData(USART1,Data);						  //����һ���ַ�
}

/*********************************************************************************************************
** �������� ������һ�����ȵ��ַ���
** ��ڲ��� ��str���������ַ����ĵ�ַ
**            length�������ַ����ĳ���
** ���ڲ��� ����
*********************************************************************************************************/
void USART1_Send_String(unsigned char *str ,unsigned int length)	
{
	while(length!=0)
	{
		USART1_Send_Byte(*str++);
		length --;
	}
}

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
** ��������: USART1_Send_Byte
** ��������: ��������ʽ��ʾ����ΧΪ4λ������
:::::::::::::::::::::::::::::::::*/
void USART1_Send_Int(int value)
{
	if(value<0)//�����ж�
	{
		value = -value;	
		USART1_Send_Byte('-');
	}
	USART1_Send_Byte(value/1000+'0');
	USART1_Send_Byte(value%1000/100+'0');
	USART1_Send_Byte(value%100/10+'0');
	USART1_Send_Byte(value%10+'0');
	USART1_Send_Byte(' ');
}

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
** ��������: USART1_IRQHandler
** ��������: �����жϺ���
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

void USART1_IRQHandler()
{ 
	u8 ReceiveDate;
		 
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) 	//��ȡ�����жϱ�־λUSART_IT_RXNE 													
	{
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);	//����жϱ�־λ
		ReceiveDate=USART_ReceiveData(USART1);			//�����ַ���������
		//USART1_Send_Byte(ReceiveDate);
		pc_dat.buf[pc_dat.next] = ReceiveDate;
		pc_dat.next++;
		if(pc_dat.next == PC_UART_BUF_LENGTH) {
			pc_dat.next = 0;
		}
	}
}

