#ifndef __DMA_UART3_H_
#define __DMA_UART3_H_

#include "stdarg.h"
#include "string.h"

#include "stm32f10x.h"
#include "host_usart.h"
#include "bluetooth.h"

/*************************************************************************************************
封装UART+DMA程序，其只负责发生接收数据，
接收缓存为双缓存区，若用户不分配内存，则将直接读取双缓存区数据，在此期间，被读取的缓存区不能用于接收数据，缓冲区使用完后需要自行释放(DMA_UART_freeRecBuf)
发送无缓存机制，直接从用户数据存储区发送，所以在发送完成之前，避免更改用户数据存储区内容，
若内存充足，可将发送函数更改成安全的；
	方法：将用户数据copy一份用于发送，但这样做可能会拉低发送速度
18-8-27
auth: scutzero
note: 注意define不要和其他dma_uart*.h文件中的冲突
**************************************************************************************************/

void DMA_UART3_Init(void);
u16	DMA_UART3_getSendCount(void);
char DMA_UART3_Send(char* sbuf, u16 length);
char * DMA_UART3_read(char *rbuf, u16 *length);
void DMA_UART3_freeRecBuf(char *rbuf);
char DMA_UART3_copysnd(void *buf, u16 length);

void u3_printf_str(char* fmt, ...);

#endif
