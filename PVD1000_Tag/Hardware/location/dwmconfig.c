#include "dwmconfig.h"

static volatile uint32_t signalResetDone;

void DWM_Init(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB| RCC_APB2Periph_GPIOC |RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	GPIO_InitStructure.GPIO_Pin = DW_SCK | DW_MISO | DW_MOSI;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(DW_SPI_GPIO, &GPIO_InitStructure);
	
	//NSS
	GPIO_InitStructure.GPIO_Pin = DW_NSS;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(DW_SPI_GPIO, &GPIO_InitStructure);
	
	//DW_RESET
	GPIO_InitStructure.GPIO_Pin = DW_RESET;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(DW_RESET_GPIO, &GPIO_InitStructure);
	
	//DW_WUP
	GPIO_InitStructure.GPIO_Pin = DW_WUP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(DW_WUP_GPIO, &GPIO_InitStructure);
	
	//DW_IRQ
	GPIO_InitStructure.GPIO_Pin = DW_IRQ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(DW_IRQ_GPIO, &GPIO_InitStructure);
	
	//DW_PHA, DW_POL
	GPIO_InitStructure.GPIO_Pin = DW_PHA | DW_POL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(DW_P_GPIO, &GPIO_InitStructure);
	
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;//2lines 2direction
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		//nss signal by software control
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;//4--9Mhz, 2--18MHz
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 10;
	SPI_Init(SPI2, &SPI_InitStructure);
	
	//IRQn interrupet
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource5);
	EXTI_InitStructure.EXTI_Line = EXTI_Line5;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;//4
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;//0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	SPI_SSOutputCmd(SPI2, ENABLE); /*Enable SPI1.NSS as a GPIO*/
	SPI_Cmd(SPI2, ENABLE);		//enable spi
	/*
	void SPI_I2S_SendData(SPI_TypeDef *SPIx, uint16_t Data)
	uint16_t SPI_I2S_ReceiveData(SPI_TypeDef *SPIx)
	*/
	//DWM_ReadWriteByte(0xff);
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_SET);
	GPIO_WriteBit(DW_WUP_GPIO, DW_WUP, Bit_SET);
	
	GPIO_WriteBit(DW_P_GPIO, DW_PHA | DW_POL, Bit_RESET);
	
	//GPIO_WriteBit(DW_RESET_GPIO, DW_RESET, Bit_SET);
	
	DWM_EnableEXT_IRQ();
	return; 
}

void DWM_SetSpeed(u8 SPI_BaudRatePrescaler) {
	assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));
	SPI2->CR1 &= 0xFFC7;
	SPI2->CR1 |= SPI_BaudRatePrescaler;
	SPI_Cmd(SPI2, ENABLE);		//enable spi
}

/*
setup the DW_RESET pin mode
0 - output open controller mode
!0- input mode with connected EXTI0 IRQ
*/
void DWM_RSTnIRQ_setup(int enable) {
	//GPIO¶Ë¿ÚÉèÖÃ
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	if(enable) {
		//PB0
		GPIO_InitStructure.GPIO_Pin = DW_RESET;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	//
		GPIO_Init(DW_RESET_GPIO, &GPIO_InitStructure);
		
		//enable GPIO used as reset for interrupt
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
		EXTI_InitStructure.EXTI_Line = DW_RESET_IRQLINE;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);
	
		NVIC_InitStructure.NVIC_IRQChannel = DW_RESET_IRQn;		//pin #0 -> EXTI #0
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;//4
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;//0
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
		
		NVIC_EnableIRQ(DW_RESET_IRQn);
		
	} else {
		NVIC_DisableIRQ(DW_RESET_IRQn);
		
		//put the pin back to tri-state...as
		//output open-drain (not active)
		GPIO_InitStructure.GPIO_Pin = DW_RESET;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
		GPIO_Init(DW_RESET_GPIO, &GPIO_InitStructure);
		GPIO_WriteBit(DW_RESET_GPIO, DW_RESET, Bit_SET); 
	}
	return;
}

/*
DWM_RESET pin has 2 functions
In general it is output, but it also can be used to reset the digital part of DWM1000 by driving this pin low.
Note, the DWM_RESET  pin should not be driven high externally.
*/
void DWM_reset(void) {
	//GPIO IO
	GPIO_InitTypeDef GPIO_InitStructure;
	
	//enable GPIO used for DWM1000 reset as open controller output
	GPIO_InitStructure.GPIO_Pin = DW_RESET;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(DW_RESET_GPIO, &GPIO_InitStructure);
	
	//driven the RSTn pin low
	GPIO_WriteBit(DW_RESET_GPIO, DW_RESET, Bit_RESET);
	
	delay_us(1);
	
	//put the pin back to output open-drain (no active)
	DWM_RSTnIRQ_setup(0);
	
	delay_ms(2);
	
	return;
}

/*
"slow" waking up of DW1000 using DW_NSS only
*/
void DWM_wakeup(void) {
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_RESET);
	delay_ms(2);
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_SET);
	delay_ms(7);	//wait 7ms for DW1000 XTAL to stabilise
	return;	
}

/*
waking up of DW1000 using DW_NSS and DW_RESET pins;
The DW_RESET signalling that the DW1000 is in the INIT state.
the total fast wakeup takes ~2.2ms and depends on crystal startup time
*/

void DWM_wakeup_fast(void) {
	#define WAKEUP_TMR_MS	(7)
	
	uint32_t x = 0;
	
	DWM_RSTnIRQ_setup(0);		//disable RSTn IRQ
	delay_us(10);
	signalResetDone = 0;		//signalResetDone connected to RST_IRQ
	DWM_RSTnIRQ_setup(1);		//enable RSTn IRQ
	delay_us(10);
	DWM_SPI_clear_chip_select();//NSS(CS) low
	delay_us(10);
	
	//need to poll to check when the dw1000 is in the IDLE, the CPLL interrupt is no reliable
	//when RSTn goes high the DW1000 is in INIT, it will enter IDLE after PLL lock(~ in 5 us)
	while((signalResetDone == 0) && x < WAKEUP_TMR_MS) {
		delay_ms(1);	//if possible, it would be better to change delay_ms() to a ClockStick()
		x++;	//when DW1000 will switch to an IDLE state RSTn pin will high
	}
	DWM_RSTnIRQ_setup(0);		//disable RSTn IRQ
	DWM_SPI_set_chip_select();	//NSS(CS) high
	
	//it takes ~35us in total for the DW1000 to lock the PLL, download AON and go to IDLE state;
	delay_ms(1);
	return;
}

/*
set 2.25MHz, spi is clocked from 72MHz
*/
void DWM_set_slowrate(void) {
	DWM_SetSpeed(SPI_BaudRatePrescaler_32);
	return;
}

/*
set 18Mhz, spi is clocked from 72MHz
*/
void DWM_set_fastrate(void) {
	DWM_SetSpeed(SPI_BaudRatePrescaler_4);
	return;
}

uint8_t DWM_ReadWriteByte(uint8_t TxData) {
	u16 retry = 0;
	//waiting send buffer is empty
	//LED_STATUS3 = 1;
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {
		retry++;
		if(retry > 800) return 0;
	}
	//LED_STATUS3 = 0;
	SPI_I2S_SendData(SPI2, TxData);
	retry = 0;
	//LED_STATUS4 = 1;
	//waiting receive finished
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {
		retry++;
		if(retry > 800) return 0;
	}
	//LED_STATUS4 = 0;
	return SPI_I2S_ReceiveData(SPI2);
}

int DWM_ReadByte(uint8_t *rxData, u16 length) {
	u8 retry = 0;
	while(length > 0) {
		//waiting receive finished
		retry = 0;
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {
			retry++;
			if(retry > 200) return -1;
		}
		*rxData++ = SPI_I2S_ReceiveData(SPI2);
		length--;
	}
	
	return 0;
}

int DWM_WriteByte(uint8_t *txData, u16 length) {
//	u8 retry = 0;
	while(length > 0) {
		//waiting send buffer is empty
//		retry = 0;
//		while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
//			retry++;
//			if(retry > 200) return -1;
//		}
//		SPI_I2S_SendData(SPI1, *txData++);
		
		DWM_ReadWriteByte(*txData++);
		length--;		
	}
	
	return 0;
}

/*
disable DW_IRQ pin IRQ
in current implementation it disables all IRQ from line5:9
*/
void DWM_DisableEXT_IRQ(void) {
	NVIC_DisableIRQ(DW_IRQ_LINE);
}

/*
enable DW_IRQ pin IRQ
in current implementation it disables all IRQ from line5:9
*/
void DWM_EnableEXT_IRQ(void) {
	NVIC_EnableIRQ(DW_IRQ_LINE);
}

/*
checks whether the specified EXTI line is enabled or not.
EXTI line: external interrupt line x where x(0...19)
return the enable state of EXTI_Line (SET or RESET)

note: NVIC->ISER is Interrupt Set Enable Register, 
ISER[0] read/set 0~31 interrupt enable status
ISER[1] read/set 32~63 interrupt enable status
...
*/
ITStatus EXTI_GetITEnStatus(uint32_t x) {
	return ((NVIC->ISER[x >> 5] & (1 << (x & 0x1F))) == RESET) ? RESET:SET;
}


/*
get DW_IRQ pin IRQ status
*/
int DWM_GetEXT_IRQStatus(void) {
	return EXTI_GetITEnStatus(DW_IRQ_LINE);
}

/*
rget DW_IRQ input pin state
*/
u8 DWM_CheckEXT_IRQ(void) {
	return GPIO_ReadInputDataBit(DW_IRQ_GPIO, DW_IRQ);
}

/*
main call-back for processing of DW1000 IRQ
it re-enter the IRQ routing and processed all events.
after processing of all events, DW1000 will clear the IRQ line.
*/
void DWM_process_irq(void) {
	//while DW1000 IRQ line active
	while(DWM_CheckEXT_IRQ() != 0) {
		dwt_isr();
	}
	return;
}

/*
IRQ call-back for all EXTI configured lines.
i.e. DW_RESET pin and DW_IRQn pin
*/
void GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == DW_RESET) {
		signalResetDone = 1;
	} else if(GPIO_Pin == DW_IRQ) {
		//process dwm irq
		//while DW1000 IRQ line active
		while(DWM_CheckEXT_IRQ() != 0) {
			dwt_isr();
		}
	} else {
		return;
	}
	return;
}

	 
void EXTI9_5_IRQHandler(void) {
	//process dwm irq
	//while DW1000 IRQ line active
	//printf("EXTI9_5_IRQHandler............\n");
	if(EXTI_GetFlagStatus(EXTI_Line5) != RESET) {
		EXTI_ClearITPendingBit(EXTI_Line5);//clear interrupt flag bit
		while(DWM_CheckEXT_IRQ() != 0) {
			dwt_isr();
			LED_STATUS2 = LED_STATUS2?0:1;
			//printf("dwt_isr............\n");
		}
	}
	
}

void EXTI0_IRQHandler(void) {
	//printf("EXTI0_IRQHandler............\n");
	if(EXTI_GetFlagStatus(DW_RESET_IRQLINE) != RESET) {
		EXTI_ClearITPendingBit(DW_RESET_IRQLINE);//clear interrupt flag bit
		signalResetDone = 1;
	}
}


