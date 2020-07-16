#include "stm32f10x.h"
#include "miscport.h"
#include "ADCSample.h"
#include "dwmconfig.h"
#include "timer3.h"
#include "delay.h"

#include "deca_regs.h"
#include "deca_device_api.h"
#include "deca_param_types.h"
#include "instance.h"

#include "zlsnio.h"
#include "bluetooth.h"
#include "dma_uart2.h"
#include "dma_uart3.h"
#include "netproto.h"
#include "stmflash.h"
#include "wdg.h"

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
	ZLSN_Init();
	BLE_Init();
	SysTick_Init();			//replace the delay_init()
	USART1_Init(115200);	//uart initialise，115200，8，1，no ,NVIC:(1,0)
	DMA_UART_Init();		//uart initialise，115200，8，1，no,NVIC:(2,0)
	DMA_UART3_Init();		//uart initialise，115200，8，1，no,NVIC:(2,1)
	
	//Timer3_Init();		//timer initialise，clock unit is 10ms
	DWM_Init();				//NVIC:(4,0), (5,0)
	netpro_init();//---------------暂时屏蔽
	
	dangerAreaInit();
	initAncPosList();
	
//	StoreAreaInfo();
//	StoreAncInfo();

	if(LoadAreaInfo() != 0) {
		dangerAreaInit();
		error_printf("error:Load Danger Area Info error, version:%d!\n", getVersionOfArea());
	} else {
		downloadAreaDataReg();
		printf("Register Area datas Download task, version:%d!\n", getVersionOfArea());
	}
	//downloadAreaDataReg();
	
	if(LoadAncInfo() != 0) {
		initAncPosList();
		error_printf("error:Load Anchors Info error, version:%d!\n", getVersionOfAncs());
	} else {
		downloadAncListReg();
		ancPosListPrint(0);
		printf("Register Anchors Info Download task, version:%d!\n", getVersionOfAncs());
	}
	//downloadAncListReg();
	
/**********************flash test code**********************/
//	dangerAreaDataInit_Test();
//	dangerAreaDataPrint_Test();
//	printf("隔开\r\n");
//	StoreAreaInfo();
//	if(LoadAreaInfo() != 0) {
//		printf("Load Danger Area Info error!\n");
//	}
//	dangerAreaDataPrint_Test();
//	
//	ancPosListInitTest();
//	ancPosListPrint(4);
//	printf("隔开\r\n");
//	StoreAncInfo();
//	if(LoadAncInfo() != 0) {
//		printf("Load Anchors Info error!\n");
//	}
//	ancPosListPrint(4);
//	while(1);
/**********************end:flash test code**********************/
	IWDG_Init(5,1250); //128分频，计数值1250，喂狗最大间隔4s
	
	info_printf("Init finished!\n");
	DWM_RSTnIRQ_setup(0);
	dw_main();
	
	/*normally can't be here*/
	DWM_RSTnIRQ_setup(0);
	error_printf("Failed to location loop, now, will into test loop\n");
	dwt_softreset();
  	while(1) {
		//test code, normally can not be here
		LED_STATUS3 = LED_STATUS3?0:1;		
		delay_ms(800);
		devid = dwt_readdevid();
		info_printf("dev id: %lX\n", devid);
		delay_ms(300);	
	}
}




