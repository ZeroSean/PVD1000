#include "zlsnio.h"

void ZLSN_Init(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO, ENABLE);
	//zlsn input io
	GPIO_InitStructure.GPIO_Pin = LINK_100M_PIN | LINK_TCP_PIN | WIFI_LINK_PIN | WIFI_WORK_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(ZLSN_INPUT_GPIO, &GPIO_InitStructure);
	
	//zlsn output io
	GPIO_InitStructure.GPIO_Pin = ZLSN_DEF_PIN | ZLSN_RST_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(ZLSN_OUTPUT_GPIO, &GPIO_InitStructure);

	GPIO_WriteBit(ZLSN_OUTPUT_GPIO, ZLSN_DEF_PIN | ZLSN_RST_PIN, Bit_SET);
}

void ZLSN_Reset(void) {
	ZLSN_RST_OUT = 0;
	delay_ms(8);
	ZLSN_RST_OUT = 1;
}

void ZLSN_DefaultConfig(u8 enable) {
	static uint32_t timer = 0;
	if(enable >= 1) {
		ZLSN_DEF_OUT = 0;
		timer = portGetTickCnt();
	}
	
	if(portGetTickCnt() > timer + 1500) {
		ZLSN_DEF_OUT = 1;
		timer = 0;
	}
}

int ZLSN_is_link(uint16_t GPIOpin) {
	return ((GPIO_ReadInputDataBit(ZLSN_INPUT_GPIO, GPIOpin)) ? (0) : (1));
}

u8 getZLSNState(void) {
	u8 state = 0;
	state = ZLSN_is_link(WIFI_LINK_PIN)
			| (ZLSN_is_link(LINK_100M_PIN) << 1)
			| (ZLSN_is_link(LINK_TCP_PIN) << 2);
	return state;
}
