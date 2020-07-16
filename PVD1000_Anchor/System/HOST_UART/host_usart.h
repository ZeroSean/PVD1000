#ifndef __HOSTUSART_H
#define	__HOSTUSART_H

#include "stm32f10x.h"
#include <stdio.h>
#include "miscport.h"

#define PC_UART_BUF_LENGTH	140

struct PC_DAT {
	u8 buf[PC_UART_BUF_LENGTH];
	u8 pre,next;
};
extern struct PC_DAT pc_dat;

void USART1_Init(u32 bound);
void USART1_Send_Byte(uint16_t Data);
void USART1_Send_Int(int value);
void USART1_Send_String(unsigned char *str ,unsigned int length);

#define USART_ERROR		1	//错误
#define USART_DEBUG 	1	//打印调试
#define USART_WARN		1	//打印警告
#define USART_INFO		1	//打印一般信息

#if USART_ERROR
#define error_printf	printf
#else
#define error_printf(...)
#endif

#if USART_DEBUG
#define debug_printf	printf
#else
#define debug_printf(...)
#endif

#if USART_WARN
#define warn_printf	printf
#else
#define warn_printf(...)
#endif

#if USART_INFO
#define info_printf	printf
#else
#define info_printf(...)
#endif

#endif 
