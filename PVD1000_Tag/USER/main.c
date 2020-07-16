#include "stm32f10x.h"
#include "miscport.h"
#include "ADCSample.h"
#include "DACOutput.h"
#include "dwmconfig.h"
#include "timer3.h"
#include "delay.h"
#include "standby.h"

#include "deca_regs.h"
#include "deca_device_api.h"
#include "deca_param_types.h"
#include "instance.h"

#include "stmflash.h"

extern int dw_main(void);

/* 
mian function
 */
int main(void)
{
	uint32 devid = 0;
	
	NVIC_Configuration(); 	//NVIC:4,0	
	Led_Init();				//Led IO pin configuration
	SWKey_Init();
	BUZZER_Init();
	SysTick_Init();			//replace the delay_init()
	DWM_Init();				//NVIC:(4,0), (5,0)
	
	USART1_Init(115200);	//uart initialise£¨115200£¨8£¨1£¨no ,NVIC:(1,0)
	
	if(check_standby()) {
		Sys_Enter_Standby();
	}
	
	DAC1_Init();
	ADC1_Init();			//ADC sample configuration

	
//	dangerAreaDataInit_Test();
//	while(1) {
//		int32_t x,y,z;
//		x = 500, y = 0;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = -250, y = -500;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = -500, y = 0;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = 1250, y = 0;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = 50, y = 600;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = -250, y = 0;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = 250, y = -1200;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));
//		x = -550, y = 700;
//		printf("Point(%d, %d, %d):%x\n", x, y, z, isInDangerAreas(x, y, z));

//		return 0;
//	}
	
	dangerAreaInit();
	initAncPosList();
	
//	StoreAreaInfo();
//	StoreAncInfo();

	if(LoadAreaInfo() != 0) {
		dangerAreaInit();
		error_printf("error:Load Danger Area Info error, version:%d!\n", getVersionOfArea());
	} else {
		printf("Success to load Area datas, version:%d!\n", getVersionOfArea());
	}
	
	//dangerAreaDataInit_Test();  //≤‚ ‘Œ£œ’«¯”Ú‘§æØ
	
	if(LoadAncInfo() != 0) {
		initAncPosList();
		error_printf("error:Load Anchors Info error, version:%d!\n", getVersionOfAncs());
	} else {
		ancPosListPrint(0);
		printf("Success to load Anchors Info, version:%d!\n", getVersionOfAncs());
	}
	
	DWM_wakeup();
	
	printf("Init finished!\n");
	DWM_RSTnIRQ_setup(0);
	dw_main();
	
	DWM_RSTnIRQ_setup(0);
	printf("Failed to location loop, now, will into test loop\n");
	dwt_softreset();
  	while(1) {
		//test code, normally can not be here
		LED_STATUS1 = LED_STATUS1?0:1;		
		delay_ms(800);
		devid = dwt_readdevid();
		printf("dev id: %lX\n", devid);
		delay_ms(300);	
	}
}




