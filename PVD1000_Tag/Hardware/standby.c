#include "standby.h"


//1:������0������
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
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);	//ʹ��PWR����ʱ��
	//PWR_WakeUpPinCmd(ENABLE);	//ʹ�ܹܽŻ��ѹ���
	PWR_EnterSTANDBYMode();		//�������
	
}

void Sys_Enter_Standby(void) {
	//��dwm1000����˯��
	dwt_configuresleep((DWT_PRESRV_SLEEP|DWT_CONFIG|DWT_TANDV), (DWT_WAKE_WK|DWT_WAKE_CS|DWT_SLP_EN));
	dwt_entersleep();
	
	SysTick_Delay(50);
	
	RCC_APB2PeriphResetCmd(0xffffffff, DISABLE);	//��λ����IO��
	Sys_Standby();
}
