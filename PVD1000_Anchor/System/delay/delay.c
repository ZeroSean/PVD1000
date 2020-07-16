#include "delay.h"
	 
//static u8  fac_us=0;//us延时倍乘数
//static u16 fac_ms=0;//ms延时倍乘数
////初始化延迟函数
////SYSTICK的时钟固定为HCLK时钟的1/8
////SYSCLK:系统时钟
//void delay_init(void)
//{
////	SysTick->CTRL&=0xfffffffb;//bit2清空,选择外部时钟  HCLK/8
//	u8 SYSCLK=72;
////	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);	//选择外部时钟  HCLK/8
//	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);	//选择时钟  HCLK
//	fac_us=SYSCLK/8;		    
//	fac_ms=(u16)fac_us*1000;
//}								    
////延时nms
////注意nms的范围
////SysTick->LOAD为24位寄存器,所以,最大延时为:
////nms<=0xffffff*8*1000/SYSCLK
////SYSCLK单位为Hz,nms单位为ms
////对72M条件下,nms<=1864 
//void delay_ms(u16 nms)
//{	 		  	  
//	u32 temp;		   
//	SysTick->LOAD=(u32)nms*fac_ms;//时间加载(SysTick->LOAD为24bit)
//	SysTick->VAL =0x00;           //清空计数器
//	SysTick->CTRL=0x01 ;          //开始倒数  
//	do
//	{
//		temp=SysTick->CTRL;
//	}
//	while(temp&0x01&&!(temp&(1<<16)));//等待时间到达   
//	SysTick->CTRL=0x00;       //关闭计数器
//	SysTick->VAL =0X00;       //清空计数器	  	    
//}   
////延时nus
////nus为要延时的us数.		    								   
//void delay_us(u32 nus)
//{		
//	u32 temp;	    	 
//	SysTick->LOAD=nus*fac_us; //时间加载	  		 
//	SysTick->VAL=0x00;        //清空计数器
//	SysTick->CTRL=0x01 ;      //开始倒数 	 
//	do
//	{
//		temp=SysTick->CTRL;
//	}
//	while(temp&0x01&&!(temp&(1<<16)));//等待时间到达   
//	SysTick->CTRL=0x00;       //关闭计数器
//	SysTick->VAL =0X00;       //清空计数器	 
//}

/*Init SysTick timer*/
void SysTick_Init(void) 
{
	/* SystemFrequency / 1000    1ms中断一次
	 * SystemFrequency / 100000	 10us中断一次
	 * SystemFrequency / 1000000 1us中断一次
	 */
	//default NVIC priority 15, you can change it
	if(SysTick_Config(SystemCoreClock / 100000)) {
		/*capture error*/
		while(1);
	}
	NVIC_SetPriority (SysTick_IRQn, 6);
	//close systick timer
	//SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}

static volatile u32 timingCounter = 0;

void SysTick_IncTick(void)
{
	timingCounter++;
}

uint32_t portGetTickCnt(void)
{
	//return HAL_GetTick();
	return (timingCounter / 100);
}

uint32_t portGetTickCnt_10us(void)
{
	//return HAL_GetTick();
	return timingCounter;
}

//判断time_10us是否到达，因为计数器会溢出，所以这里根据差值判断计数器溢出与否
u8 SysTick_TimeIsOn_10us(uint32_t time_10us) {
	uint32_t tickstart = portGetTickCnt_10us();
	if(tickstart >= time_10us) {
		return 1;	//计时到达
	} else {
		uint32_t delta = time_10us - tickstart;
		if(delta >= 0x7fffffff) {
			return 1;//当差值大于32位的一半时，可以认为计数器溢出了，仍未计时到达
		}
	}
	return 0;
}

//注意：该算法的前提是portGetTickCnt()在pretime的时间后面
u32 SysTick_TimeDelta_10us(u32 pretime_10us) {
	uint32_t tickstart = portGetTickCnt_10us();
	if(tickstart >= pretime_10us) {
		return (tickstart - pretime_10us);
	} else {
		return (0xffffffff - tickstart + pretime_10us);
	}
}

u32 SysTick_TimeDelta(u32 pretime) {
	return SysTick_TimeDelta_10us(pretime * 100) / 100;
}

void SysTick_Delay(volatile uint32_t delay) 
{
	uint32_t tickstart = portGetTickCnt();
	//printf("delay start ticks   %d\n", tickstart);
	while(SysTick_TimeDelta(tickstart) < delay) {
		//printf("systick handler   %d\n", portGetTickCnt());
	}
}

void delay_us_nop(uint32_t usec)
{
	int i, j;
	for(i=0; i<usec; i++) {
		for(j=0; j<2; j++) {
			__NOP();
			__NOP();
		}
	}
}

clock_s local_clock;
u32 lastTimingCounter;
char SysTick_SetClock(u8 *ptr, u8 length) {
	if(length < 6) return -1;
	lastTimingCounter = timingCounter;
	local_clock.year 	= ptr[0];
	local_clock.month 	= ptr[1];
	local_clock.day 	= ptr[2];
	local_clock.hour 	= ptr[3];
	local_clock.mini 	= ptr[4];
	local_clock.second 	= ptr[5];
	local_clock.ms	    = 0;
	return 0;
}

char SysTick_GetClock(u8 *ptr, u8 length) {
	if(length < 6) return -1;
	ptr[0] = local_clock.year;
	ptr[1] = local_clock.month;
	ptr[2] = local_clock.day;
	ptr[3] = local_clock.hour;
	ptr[4] = local_clock.mini;
	ptr[5] = local_clock.second;
	return 0;
}

void SysTick_ClockUpdate(void) {
						//	1	2	3	4	5	6	7	8	9	10	11	12
	static u8 months[13] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	u16 year = local_clock.year + CLOCK_START_YEAR;
	//u8 leapYear = 0;
	u8 daysOfMon = months[local_clock.month - 1];
	u16 dt = SysTick_TimeDelta(lastTimingCounter);
	
	if(dt == 0) return;
	local_clock.ms += dt;
	lastTimingCounter = portGetTickCnt();
	//second
	local_clock.second += (local_clock.ms / 1000);
	local_clock.ms = (local_clock.ms % 1000);
	//minit
	local_clock.mini += (local_clock.second / 60);
	local_clock.second = (local_clock.second % 60);
	//hour
	local_clock.hour += (local_clock.mini / 60);
	local_clock.mini = (local_clock.mini % 60);	//min； 0~59
	
	if(local_clock.hour < 24) return;
	
	//day
	if(local_clock.hour >= 24) {
		local_clock.day += 1;
		local_clock.hour -= 24;	//hour； 0~23
	}
	
	//month
	if((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
		//Is Leap Year
		//leapYear = 1;
		if(local_clock.month == 2) {
			daysOfMon = 29;
		}
	}
	if(local_clock.day > daysOfMon) {
		local_clock.month += 1;
		local_clock.day -= daysOfMon;	//day； 1~31
	}
	
	//year
	if(local_clock.month > 12) {
		local_clock.year += 1;		
		local_clock.month -= 12;		//month； 1~12
	}
}





