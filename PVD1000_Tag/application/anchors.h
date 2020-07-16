#ifndef __ANCHORS_H_
#define __ANCHORS_H_

#include "downloadJob.h"
#include "stmflash.h"
#include <math.h>

#include "dma_uart3.h"

#define ANCHOR_ID_SIZE	6
#define MAX_ANCPOS_NUM	112		//2k block, (2048 - 1) / sizeof(anchor_pos) = 113

typedef struct {
	u8 id[ANCHOR_ID_SIZE];
	int32_t	pos[3];		//x, y, z mm
}anchor_pos;


anchor_pos * getFreeNextAncPosPtr(void);
char getAncPos(anchor_pos *anc, u8 index);
u8 getAncPosListCount(void);
void clearAncPosList(void);
void initAncPosList(void);
u16 getPosBufferSize(void);
u16 readAnchorsList(u8 *dest, u16 size, u16 offset);
u16 writeAnchorsList(u8 *src, u16 size, u16 offset);

void downloadAncListReg(void);
void requestAncList(void);

/*save anchors info into stmflash, based on block*/
/*each block is 2k, so each block can save 113 anchors info, we set 112, remain space for other info*/
void StoreAncInfo(void);
u8 LoadAncInfo(void);

u8 getVersionOfAncs(void);
void updateVersionOfAncs(u8 version);

/*test code*/
void ancPosListInitTest(void);
void ancPosListPrint(u8 n);

u8 setAncListPosition(void *ptr);
u8 getAncPosition(u16 addr, int32_t *pos);
int calculateDistance(int32_t *pos1, int32_t *pos2);

#endif
