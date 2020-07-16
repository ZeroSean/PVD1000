/******************** ***************************       
 * 硬件连接：------------------------
 *          | PA9  - USART1(Tx)      |
 *          | PA10 - USART1(Rx)      |
 *           -----------------------
************************************************/
#include "host_usart.h"
//#include <stdarg.h>
//#include "math.h"

struct PC_DAT pc_dat;
/*
 * 函数名：USART1_Init
 * 描述  ：USART1 GPIO 配置,工作模式配置。115200 8-N-1
 * 输入  ：无
 * 输出  : 无
 * 调用  ：外部调用
 */
void USART1_Init(u32 bound)
{
	//GPIO端口设置
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
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器USART1

	//USART 初始化设置 
	USART_InitStructure.USART_BaudRate = bound;//一般设置为9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//开启中断
	USART_Cmd(USART1, ENABLE);                    //使能串口 
	
	pc_dat.next = 0;
	pc_dat.pre = 0;
}

/*
 * 函数名：fputc
 * 描述  ：重定向c库函数printf到USART1
 * 输入  ：无
 * 输出  ：无
 * 调用  ：由printf调用
 */
int fputc(int ch, FILE *f)
{
	/* 将Printf内容发往串口 */
	USART_SendData(USART1, (unsigned char) ch);
	//	while (!(USART1->SR & USART_FLAG_TXE));
	while( USART_GetFlagStatus(USART1,USART_FLAG_TC)!= SET);	
	return (ch);
}

//=================串口普通收发功能============================================================
//=============================================================================================

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
** 函数名称: USART1_Send_Byte
** 功能描述: 串口发送一个字符
** 参数描述：Data 要发送的数据
::::::::::::::::::::::::::::::::*/
void USART1_Send_Byte(uint16_t Data)
{ 
	while(!USART_GetFlagStatus(USART1,USART_FLAG_TXE));	  //USART_GetFlagStatus：得到发送状态位
														  //USART_FLAG_TXE:发送寄存器为空 1：为空；0：忙状态
	USART_SendData(USART1,Data);						  //发送一个字符
}

/*********************************************************************************************************
** 函数功能 ：发送一定长度的字符串
** 入口参数 ：str：待发送字符串的地址
**            length：发送字符串的长度
** 出口参数 ：无
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
** 函数名称: USART1_Send_Byte
** 功能描述: 以整数形式显示，范围为4位进制数
:::::::::::::::::::::::::::::::::*/
void USART1_Send_Int(int value)
{
	if(value<0)//正负判断
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
** 函数名称: USART1_IRQHandler
** 功能描述: 串口中断函数
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

void USART1_IRQHandler()
{ 
	u8 ReceiveDate;
		 
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) 	//读取接收中断标志位USART_IT_RXNE 													
	{
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);	//清楚中断标志位
		ReceiveDate=USART_ReceiveData(USART1);			//接收字符存入数组
		//USART1_Send_Byte(ReceiveDate);
		pc_dat.buf[pc_dat.next] = ReceiveDate;
		pc_dat.next++;
		if(pc_dat.next == PC_UART_BUF_LENGTH) {
			pc_dat.next = 0;
		}
	}
}

