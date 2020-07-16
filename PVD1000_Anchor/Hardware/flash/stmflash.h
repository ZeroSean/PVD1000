#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "sys.h"  


/*based on user demand configuration*/
#define STM32_FLASH_SIZE 512 	 		//STM32 Flash size(uint is k)
#define STM32_FLASH_WREN 1              //Flash write Enable(0:diable, 1:enable)

#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH starting address

//flash address range: 0x08000000~0x0807ffff
/*structure: {deviceid(6bytes), }*/
#define FLASH_FACTORY_ADDR		0X08070000 		//0x08070000~0x080707ff, size = 2k
#define FLASH_ID_OFFSET			0


/*structure: {devicenum(2bytes), deviceid(6byte), coordinate(12bytes)}*/
#define FLASH_ANCINFO_ADDR		0x08070800		//0x08070800~0x080717ff, size = 4k, 4k / 28byte anchors info

/*structure: {dangerAreaBlockNul(2bytes), block1, block2.....}
block struct{pointsNum(2bytes), flag(1bytes), coordinate1(12bytes), coordinate2(12bytes), ....}
*/
#define FLASH_DAGAREA_ADDR		0x08072000		//0x08072000~0x0807ffff, size = 56k, danger area info


u16 STMFLASH_ReadHalfWord(u32 faddr);		  						//read half word(2 byte size) 

void STMFLASH_Write(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite);		//on address WrireAddr starting write 
void STMFLASH_Read(u32 ReadAddr,u16 *pBuffer,u16 NumToRead);   		//on address WrireAddr starting read 


void Test_Write(u32 WriteAddr,u16 WriteData);								   
#endif

















