#include "standby.h"


//1:待机，0：工作
u16 check_standby(void) {
	u16 paras = 0, result = 0;
	STMFLASH_Read(FLASH_FACTORY_ADDR, &paras, 1);
	if(paras != 0x5555) {
		paras = 0x5555;
	} else {
		paras = ~paras;
		result = 1;
	}
	STMFLASH_Write(FLASH_FACTORY_ADDR, &paras, 1);
	return result;
}

void Sys_Standby(void) {
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);	//使能PWR外设时钟
	//PWR_WakeUpPinCmd(ENABLE);	//使能管脚唤醒功能
	PWR_EnterSTANDBYMode();		//进入待机
	
}

void Sys_Enter_Standby(void) {
	//让dwm1000进入睡眠
	dwt_configuresleep((DWT_PRESRV_SLEEP|DWT_CONFIG|DWT_TANDV), (DWT_WAKE_WK|DWT_WAKE_CS|DWT_SLP_EN));
	dwt_entersleep();
	
	SysTick_Delay(50);
	
	RCC_APB2PeriphResetCmd(0xffffffff, DISABLE);	//复位所有IO口
	Sys_Standby();
}
