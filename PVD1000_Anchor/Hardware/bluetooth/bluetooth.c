#include "bluetooth.h"


void BLE_Init(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO, ENABLE);
	//bluetooth power pin
	GPIO_InitStructure.GPIO_Pin = BLE_POWER_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(BLE_POWER_GPIO, &GPIO_InitStructure);
	
	BLE_POWER = 0;
}


void BLE_Power(u8 on) {
	BLE_POWER = on > 0 ? 0 : 1;
}

u8 getBLEState(void) {
	return ((BLE_POWER == 0) ? 1 : 0);
}
