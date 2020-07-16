#ifndef __MISCPORT_H_
#define __MISCPORT_H_

/****************************************
*    misc external hardware IO configuration£ºled,key
****************************************/

#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"


/*led*/
#define	LED_STATUS1 PCout(8)	//  Led1
#define	LED_STATUS2 PCout(9)	//  Led2
#define	LED_STATUS3 PCout(10)	//  Led3
#define	LED_STATUS4 PCout(11)	//  Led4

#define LED_1		GPIO_Pin_8
#define LED_2		GPIO_Pin_9
#define LED_3		GPIO_Pin_10
#define LED_4		GPIO_Pin_11
#define LED_GPIO	GPIOC

#define SW_1 		GPIO_Pin_0
#define SW_2 		GPIO_Pin_1
#define SW_3 		GPIO_Pin_2
#define SW_4 		GPIO_Pin_3
#define SW_GPIO		GPIOC

#define BOOT1		GPIO_Pin_2
#define BOOT1_GPIO	GPIOB

#define SW_ON	(1)
#define SW_OFF	(0)

#define BUZZER			PCout(12)
#define BUZZER_PIN		GPIO_Pin_12
#define BUZZER_GPIO		GPIOC


typedef enum {
	LED1,
	LED2,
	LED3,
	LED4,
	LED_ALL,
	LEDn
}led_t;

void Led_Init(void);  //led initial;
void SWKey_Init(void);	//SWkey initial
void BOOT1_Init(void);
void led_on(led_t led);
void led_off(led_t led);
int switch_is_on(uint16_t GPIOpin);
int boot1_is_on(void);
int boot1_is_low(void);

void BUZZER_Init(void);
void BUZZER_On(uint32_t time);
void BUZZER_Off(void);
void BUZZER_Run(void);

void warning_led_On(uint32_t time);
void warning_led_Run(void);

#endif
