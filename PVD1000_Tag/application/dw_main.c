#include "instance.h"
#include "deca_types.h"
#include "deca_spi.h"
#include "stdio.h"

#include "dwmconfig.h"
#include "stmflash.h"
#include "ADCSample.h"

uint32 inittestapplication(uint8 s1switch);

#define SWS1_SHF_MODE 0x02	//short frame mode (6.81M)
#define SWS1_CH5_MODE 0x04	//channel 5 mode
#define SWS1_ANC_MODE 0x08  //anchor mode
#define SWS1_ADD_MODE 0xf0  //anchor/tag address

uint8 s1switch = 0;
int instance_anchaddr = 0;
int dr_mode = 0;
int chan, tagaddr, ancaddr;
int instance_mode = ANCHOR;

uint32 printDebugTWRReports = 0;
uint32 sendTWRReports = 0;
uint32 sendNetData = 0;

uint32 halfSecondTimer = 0;			//0.5s timer
uint32 twoSecondTimer = 0;			//2.0s timer
uint32 fiveSecondTimer = 0;			//5.0s timer

uint8	sendTWRRawReports = 1;

//#define NETWORK_SEND_MAXLENGTH	1024
//struct network{
//	char *recbuf;
//	u16 reclength;
//	char sendbuf[NETWORK_SEND_MAXLENGTH];
//	u16 sendlength;
//}network_buf;

//Configuration for DecaRangeRTLS TREK Modes (4 default use cases selected by the switch S1 [2,3] on EVB1000, indexed 0 to 3 )
instanceConfig_t chConfig[4] ={
                    //mode 1 - S1: 2 off, 3 off
                    {
                        4,//2,            	// channel
                        20,//4,             // preambleCode(20-80m, 17-40m, 9-24m,4-11m )
                        DWT_PRF_64M,    	// prf		(DWT_PRF_16M)
                        DWT_BR_110K,        // datarate
                        DWT_PLEN_1024,   	// preambleLength
                        DWT_PAC32,          // pacSize
                        1,                  // non-standard SFD
                        (1025 + 64 - 32)    // SFD timeout
                    },
                    //mode 2 - S1: 2 on, 3 off
                    {
						4,//2,            	// channel
						20,//4,             // preambleCode
						DWT_PRF_64M,		//DWT_PRF_16M,   		// prf
						DWT_BR_6M8,        	// datarate
						DWT_PLEN_128,   	// preambleLength
						DWT_PAC8,           // pacSize
						1,//0,                  // non-standard SFD
						(129 + 8 - 8)       // SFD timeout
                    },
                    //mode 3 - S1: 2 off, 3 on
                    {
						5,             		// channel
						3,              	// preambleCode
						DWT_PRF_16M,    	// prf
						DWT_BR_110K,        // datarate
						DWT_PLEN_1024,   	// preambleLength
						DWT_PAC32,          // pacSize
						1,                  // non-standard SFD
						(1025 + 64 - 32)    // SFD timeout
                    },
                    //mode 4 - S1: 2 on, 3 on
                    {
						5,            		// channel
						3,             		// preambleCode
						DWT_PRF_16M,   		// prf
						DWT_BR_6M8,        	// datarate
						DWT_PLEN_128,   	// preambleLength
						DWT_PAC8,           // pacSize
						0,                  // non-standard SFD
						(129 + 8 - 8)       // SFD timeout
                    }
};

//Slot and Superframe Configuration
sfConfig_t sfConfig[4] ={
/*
	uint16 BCNslotDuration_ms;
	uint16 BCNslot_num;
	uint16 SVCslotDuration_ms;
	uint16 SVCslot_num;
	uint16 twrSlotDuration_ms;
	uint16 twrSlots_num;
	uint16 sfPeriod_ms;
	uint16 grpPollTx2RespTxDly_us;	
*/
	//{10, 16, 20, 2, 50, 15, 1000, 20000},	//110k, channel 2
	{4, 16, 4, 2, 35, 15, 600, 20000},	//110k, channel 2
	{3, 16, 3, 2, 10, 15, 204, 5000},	//6.81M, channel 2
	{10, 16, 20, 2, 50, 15, 1000, 20000},	//110k, channel 5
	{1,  16, 2,  2, 5,  15, 100,  2000}	//6.81M, channel 5
};

void setUniqueAddress16(uint16 instAddress) {
	STMFLASH_Write(FLASH_FACTORY_ADDR + FLASH_ID_OFFSET, &instAddress, 1);
}

uint16 getUniqueAddress16(void) {
	uint16 id = 0xffff;
	STMFLASH_Read(FLASH_FACTORY_ADDR + FLASH_ID_OFFSET, &id, 1);
	
	return id;
}


//Configure instance tag/anchor/etc... addresses
void addressconfigure(uint8 s1switch, uint8 mode) {
    uint16 instAddress;
    instance_anchaddr = (s1switch & SWS1_ADD_MODE) >> 4;
	
	//可以在此处设置并保存id，正式运行后，最好屏蔽该行
	setUniqueAddress16(5);
	//读取id
	instance_anchaddr = getUniqueAddress16();
    
	if(mode == ANCHOR) {
		instAddress = instance_anchaddr;
	} else {
		instAddress = instance_anchaddr | TAG_ADDR_FLAG_BIT;//标签地址最高位置1
	}
	
	//STMFLASH_Read(FLASH_FACTORY_ADDR, &instAddress, 1);
	instance_set_16bit_address(instAddress);
	info_printf("Address:%x\n", instAddress);
}

//returns the use case / operational mode
int decarangingmode(uint8 s1switch) {
    int mode = 0;
    if(s1switch & SWS1_SHF_MODE) {	//0x02
        mode = 1;
    }
    if(s1switch & SWS1_CH5_MODE) {	//0x04
        mode = mode + 2;
    }
    return mode;
}

uint32 inittestapplication(uint8 s1switch) {
    uint32 devID ;
    int result;

    DWM_set_slowrate();

    //this is called here to wake up the device (i.e. if it was in sleep mode before the restart)
    devID = instance_readdeviceid() ;
    if(DWT_DEVICE_ID != devID) {	//if the read of device ID fails, the DW1000 could be asleep
    	DWM_wakeup();
		
        devID = instance_readdeviceid() ;
        // SPI not working or Unsupported Device ID
        if(DWT_DEVICE_ID != devID)	return(-1);
            
        //clear the sleep bit - so that after the hard reset below the DW does not go into sleep
        dwt_softreset();
    }

    DWM_reset();	//reset the DW1000 by driving the RSTn line low

    if((s1switch & SWS1_ANC_MODE) == 0) {
        instance_mode = TAG;
		info_printf("Tag....");
    } else {
        instance_mode = ANCHOR;
		info_printf("Anc....");
    }

    result = instance_init(instance_mode);  // Set this instance mode (tag/anchor)
    if(0 > result) return(-1) ; 			// Some failure has occurred

    DWM_set_fastrate();
	
    devID = instance_readdeviceid();
    if (DWT_DEVICE_ID != devID) {  	// Means it is NOT DW1000 device
        return(-1) ;	// SPI not working or Unsupported Device ID
    }

    addressconfigure(s1switch, instance_mode) ; // set up default 16-bit address

	// get mode selection (index) this has 4 values see chConfig struct initialiser for details.
	dr_mode = decarangingmode(s1switch);
	chan = chConfig[dr_mode].channelNumber;
	instance_config(&chConfig[dr_mode], &sfConfig[dr_mode]);	// Set operating channel etc
    return devID;
}

void configure_continuous_txspectrum_mode(uint8 s1switch) {
	info_printf("Continuous TX %s%d\n", (s1switch & SWS1_SHF_MODE) ? "S" : "L", chan);
	info_printf("Spectrum Test   \n");
	//configure DW1000 into Continuous TX mode
	instance_starttxtest(0x1000);
	//measure the power
	//Spectrum Analyser set:
	//FREQ to be channel default e.g. 3.9936 GHz for channel 2
	//SPAN to 1GHz
	//SWEEP TIME 1s
	//RBW and VBW 1MHz
	//measure channel power

	//user has to reset the board to exit mode
	while(1) {
		delay_ms(2);
	}
}

int dw_main(void) {
    int rx = 0;
    int toggle = 0;

    led_off(LED_ALL); 		//turn off all the LEDs
	
    DWM_DisableEXT_IRQ(); 	//disable DW1000 IRQ until we configure the application

	//设置设备唯一地址
	//setUniqueAddress16(0x00);
	
	s1switch =switch_is_on(SW_4) << 1					//short frame mode(6.81M)
    		| 0 << 2									//channel 5 mode
    		| 0 << 3									//anchor mode
    		| switch_is_on(SW_1) << 4					//anchor/tag address A1
		    | switch_is_on(SW_2) << 5					//anchor/tag address A2
    		| switch_is_on(SW_3) << 6					//anchor/tag address A3
    		| 0 << 7;									//anchor/tag address A4
	
	info_printf("ID:%x\n", s1switch);
 
	if(inittestapplication(s1switch) == (uint32)-1) {
		led_on(LED_ALL); //to display error....
		error_printf("ERROR INIT FAIL! \n");
		return 0; //error
	}

    DWM_EnableEXT_IRQ(); //enable ScenSor IRQ before starting
    // main loop
    while(1) {
    	instance_data_t* inst = instance_get_local_structure_ptr(0);

    	int monitor_local = inst->monitor ;
    	int txdiff = (portGetTickCnt() - inst->timeofTx);

        instance_mode = instance_get_role();
        if(instance_mode == TAG) {
    		tag_run();
    	} else {
    		anch_run();
    	}

        //if delayed TX scheduled but did not happen after expected time then it has failed... (has to be < slot period)
        //if anchor just go into RX and wait for next message from tags/anchors
        //if tag handle as a timeout
        if((monitor_local == 1) && ( txdiff > inst->TWRslotDuration_ms))
        {
        	int an = 0;
        	uint32 tdly ;
        	uint32 reg1, reg2;

        	reg1 = dwt_read32bitoffsetreg(0x0f, 0x1);
        	reg2 = dwt_read32bitoffsetreg(0x019, 0x1);
        	tdly = dwt_read32bitoffsetreg(0x0a, 0x1);
        	printf("T%08x %08x time %08x %08x", (unsigned int) reg2, (unsigned int) reg1, 
					(unsigned int) dwt_read32bitoffsetreg(0x06, 0x1), (unsigned int) tdly);

            inst->wait4ack = 0;

        	if(instance_mode == TAG)
			{
        		tag_process_rx_event(inst, 0);
			}
			else
			{
				dwt_forcetrxoff();	//this will clear all events
				inst->testAppState = TA_RXE_WAIT ; //enable the RX
			}
        	inst->monitor = 0;
        }

        rx = instance_newrange();

        //if there is a new ranging report received or a new range has been calculated, then prepare data
        //to output over USB - Virtual COM port, and update the LCD
        if(rx != TOF_REPORT_NUL)
        {
        	int l = 0, r= 0, aaddr, taddr;
        	int rangeTime, valid;
            uint16 txa, rxa;

            //send the new range information to LCD and/or USB
            aaddr = instance_newrangeancadd() & 0xf;
			
#if (SEND_DATA_SUPPORT == 1) //this is set in the port.h file
            l = instance_get_lcount() & 0xFFFF;
            if(instance_mode == TAG)
            {
            	r = instance_get_rnum();
            }
            else
            {
            	r = instance_get_rnuma(taddr);
            }
            txa =  instance_get_txantdly();
            rxa =  instance_get_rxantdly();
            valid = instance_validranges();

			if(sendTWRReports + 1000 <= portGetTickCnt()) 
			{
				if(rx == TOF_REPORT_T2A)
				{
					// anchorID tagID range rangeraw countofranges rangenum rangetime txantdly rxantdly address
					printf("mc %02x %08x %08x %08x %08x %04x %02x %08x %c%d:%d\r\n", valid, instance_get_idist_mm(0), 
									instance_get_idist_mm(1), instance_get_idist_mm(2), instance_get_idist_mm(3), 
									l, r, rangeTime, (instance_mode == TAG)?'t':'a', taddr, aaddr);
					if(sendTWRRawReports == 1)
					{
						printf("mr %02x %08x %08x %08x %08x %04x %02x %04x%04x %c%d:%d\r\n", valid, instance_get_idistraw_mm(0), 
									instance_get_idistraw_mm(1), instance_get_idistraw_mm(2), instance_get_idistraw_mm(3),
									l, r, txa, rxa, (instance_mode == TAG)?'t':'a', taddr, aaddr);
					}
				}
				//update the send time
				sendTWRReports = portGetTickCnt();
			}
			instance_cleardisttableall();
#endif
        } //if new range present
			
		//if rec new data from host, then parse it
		dataParse();
		BUZZER_Run();
		warning_led_Run();
		
		/*time chip*/
		if((halfSecondTimer + 500) <= portGetTickCnt()) {
			halfSecondTimer = portGetTickCnt();
			//LED_STATUS4 = !LED_STATUS4;
			LED_STATUS3 = !LED_STATUS3;
			if(instance_is_busy(40) == 0) {
				CalSinPara();   //大约耗费时间67ms
				if(checkElecValue()) {
					//BUZZER_On(1000);
				}
			}
		}
		
		if((twoSecondTimer + 2000) <= portGetTickCnt()) {
			twoSecondTimer = portGetTickCnt();

		}
		
		if((fiveSecondTimer + 5000) <= portGetTickCnt()) {
			fiveSecondTimer = portGetTickCnt();
		}
    }
    //return 0;
}

