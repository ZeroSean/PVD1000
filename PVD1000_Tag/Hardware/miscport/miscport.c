#include "miscport.h"

uint32_t buzzer_time = 0;
uint32_t warning_led_time = 0;

/*********************************************** 
configuration led hardware IO
 ***********************************************/
void Led_Init()
{
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO, ENABLE);
	//Led---PC6,PC7,PC8,PC9
	GPIO_InitStructure.GPIO_Pin = LED_1 | LED_2 | LED_3 | LED_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(LED_GPIO, &GPIO_InitStructure);
	
	return; 
}

void BUZZER_Init(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO, ENABLE);
	//Led---PC6,PC7,PC8,PC9
	GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(BUZZER_GPIO, &GPIO_InitStructure);
	
	return; 
}

void BUZZER_On(uint32_t time) {
	buzzer_time = time + portGetTickCnt();
	BUZZER = 1;
	return;
}

void BUZZER_Off(void) {
	BUZZER = 0;
	return;
}

void BUZZER_Run(void) {
	if(buzzer_time <= portGetTickCnt()) {
		BUZZER = 0;
	}
	return;
}

//Ìæ´ú·äÃùÆ÷Ïì£¬ÒòÎª²âÊÔÖÐ·äÃùÆ÷Ì«³³ÁË
void warning_led_On(uint32_t time) {
	warning_led_time = time + portGetTickCnt();
	led_on(LED4);
	return;
}

void warning_led_Run(void) {
	if(warning_led_time <= portGetTickCnt()) {
		led_off(LED4);
	}
	return;
}


void BOOT1_Init(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO, ENABLE);
	//Led---PC6,PC7,PC8,PC9
	GPIO_InitStructure.GPIO_Pin = BOOT1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(BOOT1_GPIO, &GPIO_InitStructure);
	
	return; 
}

void SWKey_Init(void){
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO, ENABLE);
	//Key: SW3-8: PC0-PC5
	GPIO_InitStructure.GPIO_Pin = SW_1 | SW_2 | SW_3 | SW_4 ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(SW_GPIO, &GPIO_InitStructure);
}

void led_on(led_t led) {
	switch(led) {
		case LED1:
			LED_STATUS1 = 0;
			break;
		case LED2:
			LED_STATUS2 = 0;
			break;
		case LED3:
			LED_STATUS3 = 0;
			break;
		case LED4:
			LED_STATUS4 = 0;
			break;
		case LED_ALL:
			LED_STATUS1 = 0;
			LED_STATUS2 = 0;
			LED_STATUS3 = 0;
			LED_STATUS4 = 0;
			break;
		default:
			//do nothing
			break;	
	}
}

void led_off(led_t led) {
	switch(led) {
		case LED1:
			LED_STATUS1 = 1;
			break;
		case LED2:
			LED_STATUS2 = 1;
			break;
		case LED3:
			LED_STATUS3 = 1;
			break;
		case LED4:
			LED_STATUS4 = 1;
			break;
		case LED_ALL:
			LED_STATUS1 = 1;
			LED_STATUS2 = 1;
			LED_STATUS3 = 1;
			LED_STATUS4 = 1;
			break;
		default:
			//do nothing
			break;	
	}
}

/*
check the switch status,
when switch(SW) is on, the pin is high
return 1 if ON and 0 for OFF
*/
int switch_is_on(uint16_t GPIOpin) { 
	return ((GPIO_ReadInputDataBit(SW_GPIO, GPIOpin)) ? (1) : (0));
}

/*
check the BOOT1 pin status
return 1 if ON and 0 for OFF
*/
int boot1_is_on(void) {
	return ((GPIO_ReadInputDataBit(BOOT1_GPIO, BOOT1)) ? (0) : (1));
}


/*
check the BOOT1 pin status
return 1 if ON and 0 for OFF
*/
int boot1_is_low(void) {
	return ((GPIO_ReadInputDataBit(BOOT1_GPIO, BOOT1)) ? (0) : (1));
}



