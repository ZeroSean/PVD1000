#ifndef __DWMCONFIG_H_
#define __DWMCONFIG_H_

#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"

#include "miscport.h"

#include "deca_device_api.h"

#define	DW_SPI_CS		PAout(4)	//chip select

#define DW_NSS			GPIO_Pin_4
#define DW_SCK			GPIO_Pin_5
#define DW_MISO			GPIO_Pin_6
#define DW_MOSI			GPIO_Pin_7
#define DW_SPI_GPIO		GPIOA

#define DW_RESET		GPIO_Pin_0	//0ÔÝÊ±ÆÁ±Î
#define DW_RESET_GPIO	GPIOB		//AÔÝÊ±ÆÁ±Î
#define DW_RESET_PortSource		GPIO_PortSourceGPIOB
#define DW_RESET_PinSource		GPIO_PinSource0

#define DW_WUP			GPIO_Pin_1	//0ÔÝÊ±ÆÁ±Î
#define DW_WUP_GPIO		GPIOB

#define DW_IRQ			GPIO_Pin_5
#define DW_IRQ_GPIO		GPIOB
#define DW_IRQ_NUM		EXTI9_5_IRQn
#define DW_IRQ_LINE		EXTI9_5_IRQn

/* 
Clock polarity: without important impact
POL=0, serial sync clock idle state is low voltage, while POL=1, is high voltage
Clock phase: select two type different transform protocol
PHA=0, when fist edge (up or down) of serial sync clock sample data,
PHA=1, when second edge (up or down) of serial sync clock sample data,
*/
//unused!
#define DW_PHA			GPIO_Pin_5		//11
#define DW_POL			GPIO_Pin_4		//10
#define DW_P_GPIO		GPIOC

/*NSS pin control*/
#define DWM_SPI_set_chip_select()	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_SET)
#define DWM_SPI_clear_chip_select()	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_RESET)


void DWM_Init(void);
void DWM_RSTnIRQ_setup(int enable);
void DWM_reset(void);
void DWM_wakeup(void);
void DWM_wakeup_fast(void);
void DWM_set_slowrate(void);
void DWM_set_fastrate(void);

void DWM_DisableEXT_IRQ(void);
void DWM_EnableEXT_IRQ(void);
int DWM_GetEXT_IRQStatus(void);
u8 DWM_CheckEXT_IRQ(void);

void DWM_process_irq(void);

uint8_t DWM_ReadWriteByte(uint8_t TxData);
int DWM_ReadByte(uint8_t *rxData, u16 length);
int DWM_WriteByte(uint8_t *txData, u16 length);

#endif
