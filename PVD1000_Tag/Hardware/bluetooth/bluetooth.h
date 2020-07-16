#ifndef __BLUETOOTH_H_
#define __BLUETOOTH_H_

#include "stm32f10x.h"
#include "sys.h"

/*control io*/
#define BLE_POWER_PIN		GPIO_Pin_12
#define BLE_POWER_GPIO		GPIOB

#define	BLE_POWER 			PBout(12)

/*data trans ios*/
/*note!!!: below io already be configured in dma_uart3.x files*/
#define BLE_RXD_PIN			GPIO_Pin_10
#define BLE_TXD_PIN			GPIO_Pin_11
#define BLE_RTS_PIN			GPIO_Pin_13
#define BLE_CTS_PIN			GPIO_Pin_14
#define BLE_UART_GPIO		GPIOB


void BLE_Init(void);
void BLE_Power(u8 on);

#endif
