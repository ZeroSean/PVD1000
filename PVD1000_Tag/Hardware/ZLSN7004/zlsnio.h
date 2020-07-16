#ifndef __ZLSN_IO_H_
#define __ZLSN_IO_H_

#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"

/*input io*/
#define LINK_100M_PIN		GPIO_Pin_9
#define LINK_TCP_PIN		GPIO_Pin_8
#define WIFI_LINK_PIN		GPIO_Pin_7
#define WIFI_WORK_PIN		GPIO_Pin_6
#define ZLSN_INPUT_GPIO		GPIOB

/*ouput io*/
#define ZLSN_DEF_PIN		GPIO_Pin_11		//4
#define ZLSN_RST_PIN		GPIO_Pin_10		//5
#define ZLSN_OUTPUT_GPIO	GPIOC

#define	ZLSN_DEF_OUT 		PCout(4)
#define	ZLSN_RST_OUT 		PCout(5)

/*data trans io*/
/*below ios  already be configured in dma_usart2 file */
#define ZLSN_RTS_PIN		GPIO_Pin_0
#define ZLSN_CTS_PIN		GPIO_Pin_1
#define ZLSN_RX_PIN			GPIO_Pin_2
#define ZLSN_TX_PIN			GPIO_Pin_3
#define ZLSN_UART_GPIO		GPIOA


/*net link state*/
#define LINK_WIFI	0x01
#define LINK_100M	0x02
#define LINK_TCP	0x04

void ZLSN_Init(void);
void ZLSN_Reset(void);
void ZLSN_DefaultConfig(u8 enable);

u8 getZLSNState(void);


#endif
