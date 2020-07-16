#include "deca_spi.h"
#include "deca_device_api.h"

#include <string.h>
#include "host_usart.h"

/*Function: openspi()
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(void) {
	return 0;
}

/*Function: closespi()
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void) {
	return 0;
}

/*Function: writetospi()
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 */
//#pragma GCC optimize ("O3")
int writetospi(uint16 headerLength, const uint8 *headerBuffer, uint32 bodylength, const uint8 *bodyBuffer) {
    //decaIrqStatus_t  stat ;
    //stat = decamutexon() ;
	/* Blocking: Check whether previous transfer has been finished */
	//while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
	
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_RESET);	//Put chip select line low
	
	DWM_WriteByte((uint8_t *)&headerBuffer[0], headerLength);
	DWM_WriteByte((uint8_t *)&bodyBuffer[0], bodylength);
	
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_SET);	//Put chip select line high
	
    //decamutexoff(stat) ;
	return 0;
}

/*Function: readfromspi()
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns -1 if there was an error
 */
int readfromspi(uint16 headerLength, const uint8 *headerBuffer, uint32 readlength, uint8 *readBuffer) {
	uint8_t spi_TmpBuffer[BUFFLEN];
	uint16_t size = readlength + headerLength;
	uint16_t index = 0;
	
	//decaIrqStatus_t  stat ;
   // stat = decamutexon() ;
	
	//check param is or isn¡®t valid
	assert_param( (headerLength+readlength) < BUFFLEN );
   
	/* Blocking: Check whether previous transfer has been finished */
	//while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_RESET);	//Put chip select line low
	while(size > readlength) {
		spi_TmpBuffer[index++] = DWM_ReadWriteByte(*headerBuffer);
		headerBuffer++;
		size--;
		
	}
	while(size > 0) {
		spi_TmpBuffer[index++] = DWM_ReadWriteByte(0xff);
		size--;
	}
	GPIO_WriteBit(DW_SPI_GPIO, DW_NSS, Bit_SET);	//Put chip select line high
	
	memcpy((uint8_t*)readBuffer , (uint8_t*)&spi_TmpBuffer[headerLength], readlength);
	
    //decamutexoff(stat) ;
	
	return 0;
}
