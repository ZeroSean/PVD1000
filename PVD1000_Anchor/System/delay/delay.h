#ifndef __DELAY_H
#define __DELAY_H 			   
#include "stm32f10x.h"
#include "host_usart.h"

//void delay_init(void);
//void delay_ms(u16 nms);
//void delay_us(u32 nus);

#define CLOCK_START_YEAR		2000

typedef struct {
	u8 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 mini;
	u8 second;
	u16 ms;
}clock_s;

#define delay_ms(delay) SysTick_Delay(delay)
#define delay_us(usec) delay_us_nop(usec)

void SysTick_Init(void);
void SysTick_Delay(volatile uint32_t delay);
void SysTick_IncTick(void);
uint32_t portGetTickCnt(void);
uint32_t portGetTickCnt_10us(void);

u8 SysTick_TimeIsOn_10us(uint32_t time_10us);
u32 SysTick_TimeDelta_10us(u32 pretime_10us);
u32 SysTick_TimeDelta(u32 pretime);

void delay_us_nop(uint32_t usec);

char SysTick_SetClock(u8 *ptr, u8 length);
char SysTick_GetClock(u8 *ptr, u8 length);
void SysTick_ClockUpdate(void);

#endif





























