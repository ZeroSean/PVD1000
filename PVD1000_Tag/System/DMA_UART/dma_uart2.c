#include "dma_uart2.h"

#define _USART				USART2
#define _DMA_CHANNEL_REC	DMA1_Channel6
#define _DMA_CHANNEL_SND	DMA1_Channel7
#define _DMA_FLAG_SEND_TC	DMA1_FLAG_TC7
#define _USART_IRQ			USART2_IRQn

#define _USART_TX_PIN		ZLSN_RX_PIN
#define _USART_RX_PIN		ZLSN_TX_PIN
#define _USART_RTS_PIN		ZLSN_CTS_PIN
#define _USART_CTS_PIN		ZLSN_RTS_PIN
#define _USART_GPIO			ZLSN_UART_GPIO

#define _DMA_SENDBUF_LENGTH	1024
#define _DMA_RECBUF_LENGTH	1024

#define _RECDOUBLEBUF_FINISH		0x01	//双缓存接收完成标志
#define _RECDOUBLEBUF_ONE		0x02	//双缓存1标志位，0为空，1为忙
#define _RECDOUBLEBUF_TWO		0x04	//双缓存2标志位，同上
#define _RECDOUBLEBUF_MASK		0x06	//缓存选择

/*接收双缓存区*/
static struct doubleBuf{
	char doubleBuf[2][_DMA_RECBUF_LENGTH];
	u16 dataSize[2];
	u8 state;	//receive state
	u8 pre, next;
}recBuf;

static struct{
	char buf[_DMA_SENDBUF_LENGTH];
	u16 length;

}sendBuf;

static u16 sendingLength = 0;

/* UART DMA接收设置 */
void DMA_USART_setRecBuf(char *buf, u16 bufersize) {
	DMA_InitTypeDef DMA_InitStructure;
	
	DMA_InitStructure.DMA_BufferSize	= bufersize;
	DMA_InitStructure.DMA_DIR			= DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_M2M			= DMA_M2M_Disable;
	DMA_InitStructure.DMA_MemoryBaseAddr= (uint32_t)buf;
	DMA_InitStructure.DMA_MemoryDataSize= DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_MemoryInc		= DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode			= DMA_Mode_Normal;
	DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t)&_USART->DR;
	DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_PeripheralInc			= DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_Priority				= DMA_Priority_VeryHigh;
	//DMA_DeInit(_DMA_CHANNEL_REC);
	DMA_Init(_DMA_CHANNEL_REC, &DMA_InitStructure);
	DMA_Cmd(_DMA_CHANNEL_REC, ENABLE);
}

/*UART DMA发送设置*/
void DMA_USART_setSendBuf(char *buf, u16 bufersize) {
	DMA_InitTypeDef DMA_InitStructure;
	
	DMA_InitStructure.DMA_BufferSize	= bufersize;
	DMA_InitStructure.DMA_DIR			= DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_M2M			= DMA_M2M_Disable;
	DMA_InitStructure.DMA_MemoryBaseAddr= (uint32_t)buf;
	DMA_InitStructure.DMA_MemoryDataSize= DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_MemoryInc		= DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode			= DMA_Mode_Normal;
	DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t)&_USART->DR;
	DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_PeripheralInc			= DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_Priority				= DMA_Priority_High;
	//DMA_DeInit(_DMA_CHANNEL_SND);
	DMA_Init(_DMA_CHANNEL_SND, &DMA_InitStructure);
	DMA_Cmd(_DMA_CHANNEL_SND, ENABLE);
}

void DMA_UART_Init() {
	//GPIO端口设置
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO, ENABLE);
	//USART2_TX   PA.2
	GPIO_InitStructure.GPIO_Pin = _USART_TX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(_USART_GPIO, &GPIO_InitStructure);

	//USART2_RX	  PA.3
	GPIO_InitStructure.GPIO_Pin = _USART_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(_USART_GPIO, &GPIO_InitStructure); 

	//USART 初始化设置 
	USART_InitStructure.USART_BaudRate = 115200;//一般设置为9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(_USART, &USART_InitStructure);	

	NVIC_InitStructure.NVIC_IRQChannel = _USART_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器USART1
	
	USART_ITConfig(_USART, USART_IT_TC, DISABLE);		//close send inter
	USART_ITConfig(_USART, USART_IT_RXNE, DISABLE);		//clsoe rec  inter
	USART_ITConfig(_USART, USART_IT_IDLE, ENABLE);		//open idle inter
	
	USART_DMACmd(_USART, USART_DMAReq_Tx, ENABLE);		//enable dma sned 
	USART_DMACmd(_USART, USART_DMAReq_Rx, ENABLE);		//enable dma rece
	USART_Cmd(_USART, ENABLE);
	
	recBuf.next = recBuf.pre = recBuf.state = 0;
	sendingLength = 1;
	DMA_USART_setSendBuf((char *)&recBuf.state, 1);
	DMA_USART_setRecBuf(&recBuf.doubleBuf[recBuf.next][0], _DMA_RECBUF_LENGTH);
}

/* 发送长度为 length的数据，成功return 0,失败return 1 */
char DMA_UART_Send(char* sbuf, u16 length) {
	u16 time = 0;
	while(!DMA_GetFlagStatus(_DMA_FLAG_SEND_TC)) {
		if((time++) > 10000) return 1;	//error, time out
	}
	DMA_Cmd(_DMA_CHANNEL_SND, DISABLE);
	DMA_ClearFlag(_DMA_FLAG_SEND_TC);
	DMA_USART_setSendBuf(sbuf, length);
	sendingLength = length;
	//DMA_SetCurrDataCounter(_DMA_CHANNEL_SND, length);
	//DMA_Cmd(_DMA_CHANNEL_SND, ENABLE);
	return 0;
}

u16	DMA_UART_getSendCount(void) {
	return (sendingLength - DMA_GetCurrDataCounter(_DMA_CHANNEL_SND));
}

void USART2_IRQHandler(void) {
	u16 temp;
	if(USART_GetITStatus(_USART, USART_IT_IDLE) != RESET) {
		temp = _USART->SR;
		temp = _USART->DR;
		DMA_Cmd(_DMA_CHANNEL_REC, DISABLE);
		
		recBuf.dataSize[recBuf.next] = _DMA_RECBUF_LENGTH - DMA_GetCurrDataCounter(_DMA_CHANNEL_REC);
		recBuf.state |= _RECDOUBLEBUF_FINISH;
		recBuf.state |= 0x01 << (recBuf.next + 1);
		if((++recBuf.next) >= 2) {
			recBuf.next = 0;
		}
		
		/*if there buf is free, then config it*/
		if((recBuf.state & _RECDOUBLEBUF_MASK) != _RECDOUBLEBUF_MASK) {
			DMA_USART_setRecBuf(&recBuf.doubleBuf[recBuf.next][0], _DMA_RECBUF_LENGTH);
		}	
	} else {
		USART_ReceiveData(_USART);
		USART_ClearFlag(_USART, USART_FLAG_ORE);
	}
}

/* if user directly read inside buf data, then need free it after unuse it */
void DMA_UART_freeRecBuf(char *ptr) {
	if(ptr == &recBuf.doubleBuf[0][0]) {
		if((recBuf.state & _RECDOUBLEBUF_MASK) == _RECDOUBLEBUF_MASK) {
			/*receiver don't open, now there is free buffer, config it*/
			recBuf.next = 0;
			recBuf.pre = 0;
			DMA_USART_setRecBuf(&recBuf.doubleBuf[recBuf.next][0], _DMA_RECBUF_LENGTH);
		}
		/*clear buffer state bit*/
		recBuf.state &= ~_RECDOUBLEBUF_ONE;
	} else if(ptr == &recBuf.doubleBuf[1][0]) {
		if((recBuf.state & _RECDOUBLEBUF_MASK) == _RECDOUBLEBUF_MASK) {
			/*receiver don't open, now there is free buffer, config it*/
			recBuf.next = 1;	
			recBuf.pre = 1;			
			DMA_USART_setRecBuf(&recBuf.doubleBuf[recBuf.next][0], _DMA_RECBUF_LENGTH);
		}
		/*clear buffer state bit*/
		recBuf.state &= ~_RECDOUBLEBUF_TWO;
	}
}

/*if rbuf is NULL, then directly return inside buffer, 
otherwise, copy data from inside buffer to rbuf, and then free inside buffer*/
char * DMA_UART_read(char *rbuf, u16 *length) {
	char *ptr;
	u16 i = 0, len = 0;
	/*there new data, then read it*/
	if(recBuf.state & _RECDOUBLEBUF_FINISH){
		ptr = &recBuf.doubleBuf[recBuf.pre][0];
		len = recBuf.dataSize[recBuf.pre];
		
		if((++recBuf.pre) >= 2) {
			recBuf.pre = 0;
		}
		if(recBuf.next == recBuf.pre) {
			/*mean without new data, reset RECDOUBLEBUF_FINISH flag bit*/
			recBuf.state &= ~_RECDOUBLEBUF_FINISH;
		}
		
		if(rbuf != 0) {
			/*copy data from inside buffer to rbuf*/
			for(i=0; (i < *length) && (i < len); i++) {
				rbuf[i] = ptr[i];
			}
			*length = i;
			/*free inside buffer*/
			DMA_UART_freeRecBuf(ptr);
			return rbuf;
		}
		*length = len;
		return ptr;
		
	} else {
		/*without new data*/
		*length = 0;
		return NULL;
	}
}

char DMA_UART_copysnd(void *buf, u16 length) {
	u16 i = 0;
	char *ptr = (char *)buf;
	for(i = 0; i < length; i++) {
		sendBuf.buf[i] = ptr[i];
	}
	return DMA_UART_Send(sendBuf.buf, length);
}

