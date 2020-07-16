#ifndef __DANGER_AREA_H_
#define __DANGER_AREA_H_

#include <stdio.h>
#include <stdlib.h>
#include "downloadJob.h"
#include "stmflash.h"

#define DAN_AREA_ID_SIZE	6

typedef struct{
	int32_t x;
	int32_t y;
	int32_t z;
}area_pos_t;

typedef struct {
	u8 id[DAN_AREA_ID_SIZE];
	uint16_t size;
	area_pos_t *poslist;
}danArea_t;
 
typedef struct {
	u8 version;			//版本号（危险区域）
	u8 count;			//区域块数量
	u16 size;			//记录整个区域管理所用的内存大小，下载数据需要用到
	danArea_t *area;	//区域块链表
}areaList_t;


void dangerAreaInit(void);
void dangerAreaClear(void);
u8 setAreaBlockCount(u8 count);
u8 addAreaBlock(u8 *id, u8 *src, u16 length);

u16 getAreaDataSize(void);
u16 readAreaData(u8 *dest, u16 size, u16 offset);
u16 writeAreaData(u8 *src, u16 size, u16 offset);

u8 getVersionOfArea(void);
void updateVersionOfArea(u8 version);

/*save danger area info into stmflash, based on block*/
void StoreAreaInfo(void);
u8 LoadAreaInfo(void);

void downloadAreaDataReg(void);
void requestAreaData(void);

/*test code*/
void dangerAreaDataInit_Test(void);
void dangerAreaDataPrint_Test(void);

#endif
