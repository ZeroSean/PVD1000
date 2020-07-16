#ifndef __DMA_UART_H_
#define __DMA_UART_H_

#include "stm32f10x.h"
#include "host_usart.h"
#include "zlsnio.h"

/*************************************************************************************************
��װUART+DMA������ֻ�������������ݣ�
���ջ���Ϊ˫�����������û��������ڴ棬��ֱ�Ӷ�ȡ˫���������ݣ��ڴ��ڼ䣬����ȡ�Ļ������������ڽ������ݣ�������ʹ�������Ҫ�����ͷ�(DMA_UART_freeRecBuf)
�����޻�����ƣ�ֱ�Ӵ��û����ݴ洢�����ͣ������ڷ������֮ǰ����������û����ݴ洢�����ݣ�
���ڴ���㣬�ɽ����ͺ������ĳɰ�ȫ�ģ�
	���������û�����copyһ�����ڷ��ͣ������������ܻ����ͷ����ٶ�
18-8-27
auth: scutzero
**************************************************************************************************/

void DMA_UART_Init(void);

u16	DMA_UART_getSendCount(void);
char DMA_UART_Send(char* sbuf, u16 length);
char * DMA_UART_read(char *rbuf, u16 *length);
void DMA_UART_freeRecBuf(char *rbuf);

char DMA_UART_copysnd(void *buf, u16 length);

#endif
