#include "anchors.h"

#define VERSION_MASK	0x0f

struct {
	u8 version;	//基站版本号
	u8 count;
	anchor_pos ancPos[MAX_ANCPOS_NUM];
}anchor_pos_list;

/*return: 
 * 2:no anc list
 * 1:no found anc, 
 * 0:ok
*/
u8 getAncPosition(u16 addr, int32_t *pos) {
	u8 i = 0;
	u8 low1 = addr & 0xff, low2 = (addr >> 8) & 0xff;
	if(anchor_pos_list.version == 0) {
		return 2;
	}
	for(i = 0; i < anchor_pos_list.count; ++i) {
		if((low1 == anchor_pos_list.ancPos[i].id[5]) && (low2 == anchor_pos_list.ancPos[i].id[4])) {
			memcpy(pos, anchor_pos_list.ancPos[i].pos, 12);
			//pos[0] = anchor_pos_list.ancPos[i].pos[0];
			return 0;
		}
	}
	return 1;
}

//计算两个点间的距离(基于二维，若后期标签的第三维坐标精度提升，可加上第三维)
int calculateDistance(int32_t *pos1, int32_t *pos2) {
	int dis = (pos1[0] - pos2[0]) * (pos1[0] - pos2[0]) + (pos1[1] - pos2[1]) * (pos1[1] - pos2[1]);
	return (int)(sqrt(dis) / 1);
}

void updateVersionOfAncs(u8 version) {
	if(version & VERSION_MASK) {
		anchor_pos_list.version = version & VERSION_MASK;
		return;
	}
	anchor_pos_list.version = (anchor_pos_list.version + 1) & VERSION_MASK;//控制版本号在1~15之间
	if(anchor_pos_list.version == 0) {
		anchor_pos_list.version = 1;//版本号0有特殊含义：该数据无效
	}
}

u8 getVersionOfAncs(void) {
	if(anchor_pos_list.version == 0) {
		updateVersionOfAncs(0);
	}
	
	if(anchor_pos_list.count == 0) {
		return 0;
	}
	//若正在下载数据，则暂时返回版本0
	if(hasDownloadTask(DOWNLOAD_TASK_TYPE_ANCLIST)) {
		return 0;
	}
	return (anchor_pos_list.version & VERSION_MASK);
}


//通过蓝牙设置基站坐标，只能设置4个基站
u8 setAncListPosition(void *ptr) {
	char *data = (char *)ptr;
	int x[4], y[4], z[4];
	int i = 0;
	if((data[0] == 's' || data[0] == 'S') && (data[1] == 'a' || data[1] == 'A') && data[2] == ':') {
		clearAncPosList();
	
		sscanf((data+3), "(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)", &x[0], &y[0], &z[0], &x[1], &y[1], &z[1], &x[2], &y[2], &z[2], &x[3], &y[3], &z[3]);
	
		anchor_pos_list.count = 4;
		for(i = 0; i < 4; ++i) {
			anchor_pos_list.ancPos[i].id[ANCHOR_ID_SIZE - 1] = i;
			anchor_pos_list.ancPos[i].pos[0] = x[i];
			anchor_pos_list.ancPos[i].pos[1] = y[i];
			anchor_pos_list.ancPos[i].pos[2] = z[i];
		}	
		downloadAncListReg();
		
		printf("rec:(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)\n", x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2], x[3], y[3], z[3]);
		
		updateVersionOfAncs(0);
		printf("update anchors version:%d!\n", getVersionOfAncs());
		return 0;
	}
	return 1;
}

void ancPosListInitTest(void) {
	anchor_pos_list.count = 4;
	anchor_pos_list.ancPos[0].id[ANCHOR_ID_SIZE - 1] = 0;
	anchor_pos_list.ancPos[0].pos[0] = -160;
	anchor_pos_list.ancPos[0].pos[1] = 7100;
	anchor_pos_list.ancPos[0].pos[2] = 1300;
	//sa:(-160,7100,1300),(0,0,1320),(15500,0,1830),(8050,-100,2280)
	//sa:(-160,7100,1300),(0,0,1320),(14700,-100,1300),(8050,0,2900)
	//sa:(-160,7100,1300),(8050,0,500),(14700,-100,1300),(8050,0,3000)
	anchor_pos_list.ancPos[1].id[ANCHOR_ID_SIZE - 1] = 1;
	anchor_pos_list.ancPos[1].pos[0] = 8050;
	anchor_pos_list.ancPos[1].pos[1] = 0;
	anchor_pos_list.ancPos[1].pos[2] = 500;
	
	anchor_pos_list.ancPos[2].id[ANCHOR_ID_SIZE - 1] = 2;
	anchor_pos_list.ancPos[2].pos[0] = 14700;
	anchor_pos_list.ancPos[2].pos[1] = -100;
	anchor_pos_list.ancPos[2].pos[2] = 1300;
	
	anchor_pos_list.ancPos[3].id[ANCHOR_ID_SIZE - 1] = 3;
	anchor_pos_list.ancPos[3].pos[0] = 8050;
	anchor_pos_list.ancPos[3].pos[1] = 0;
	anchor_pos_list.ancPos[3].pos[2] = 3000;
	
	updateVersionOfAncs(0);
}

void ancPosListPrint(u8 n) {
	int i = 0;
	printf("Anchors Num: %d\n", anchor_pos_list.count);
	for(i = 0; i < anchor_pos_list.count; i++) {
		printf("Anchors %d (%x-%x)---(%d, %d, %d)\n", i, anchor_pos_list.ancPos[i].id[ANCHOR_ID_SIZE - 2], anchor_pos_list.ancPos[i].id[ANCHOR_ID_SIZE - 1], anchor_pos_list.ancPos[i].pos[0], \
													anchor_pos_list.ancPos[i].pos[1], anchor_pos_list.ancPos[i].pos[2]);
	} 
	return;
}

anchor_pos * getFreeNextAncPosPtr(void) {
	if(anchor_pos_list.count >= MAX_ANCPOS_NUM) {
		return NULL;
	}
	return &anchor_pos_list.ancPos[anchor_pos_list.count++];
}

char getAncPos(anchor_pos *anc, u8 index) {
	u8 i = 0;
	u8 *dest = (u8 *)anc;
	u8 *src = NULL;
	if(index >= anchor_pos_list.count) {
		return -1;
	}
	dest = (u8 *)&anchor_pos_list.ancPos[index];
	for(i = 0; i < sizeof(anchor_pos); i++) {
		dest[i] = src[i];
	}
	return 0;
}

u8 getAncPosListCount(void) {
	return anchor_pos_list.count;
}

void clearAncPosList(void) {
	anchor_pos_list.count = 0;
	anchor_pos_list.version = 0;
	memset(anchor_pos_list.ancPos, 0, sizeof(anchor_pos) * MAX_ANCPOS_NUM);
}

void initAncPosList(void) {
	anchor_pos_list.version = 0;
	clearAncPosList();
}

u16 getPosBufferSize(void) {
	return (anchor_pos_list.count * sizeof(anchor_pos) + 1);
}

//using for download the anchors list data to tags
u16 readAnchorsList(u8 *dest, u16 size, u16 offset) {
	u8 *src = (u8 *)&anchor_pos_list.ancPos;
	u16 i = 0;
	u16 readByte = 0;
	if(size == 0) {
		error_printf("invalid argument: size = 0\n");
		return 0;
	}
	
	if(offset == 0) {
		*(dest++) = getVersionOfAncs();
		size--;
		offset++;
		readByte++;
	}
	for(i = offset; size > 0 && i < (anchor_pos_list.count * sizeof(anchor_pos) + 1); i++, size--) {
		dest[i - offset] = src[i - 1];
		readByte++;
	}
	return readByte;  //(i - offset);
}

//using for download the anchors list data to tags
u16 writeAnchorsList(u8 *src, u16 size, u16 offset) {
	u8 *dest = (u8 *)&anchor_pos_list.ancPos;
	u16 i = 0;
	u8 count = 0;
	
	if(size == 0) {
		error_printf("invalid argument: size = 0\n");
		return offset;
	}
	if(offset == 0) {
		updateVersionOfAncs(*(src++));
		size--;
		offset++;
	}
	for(i = offset; size > 0 && i < (MAX_ANCPOS_NUM * sizeof(anchor_pos) + 1); i++, size--) {
		dest[i - 1] = src[i - offset];
	}
	
	count = (i - 1)/ sizeof(anchor_pos);
	//bug:若未下载前的数量比下载后的数量大，那么count不会更新，把判断条件去掉
	/*if(count > anchor_pos_list.count) {
		anchor_pos_list.count = count;
	}*/
	anchor_pos_list.count = count;
	return i;
}

void downloadAncListReg(void) {
	creatDownloadTask(DOWNLOAD_TASK_TYPE_ANCLIST | UPLOAD_TYPE_MASK, getPosBufferSize, readAnchorsList, writeAnchorsList);
}

void requestAncList(void) {
	creatDownloadTask(DOWNLOAD_TASK_TYPE_ANCLIST, getPosBufferSize, readAnchorsList, writeAnchorsList);
}

/*save anchors info into stmflash, based on block*/
/*each block is 2k, so each block can save 113 anchors info, we set 112, remain space for other info*/
void StoreAncInfo(void) {
	u16 size = 4;
	u16 offset = 0;
	STMFLASH_Write(FLASH_ANCINFO_ADDR + offset, (u16 *)&anchor_pos_list, size / 2);
	offset += size;
	
	size = sizeof(anchor_pos) * anchor_pos_list.count;
	STMFLASH_Write(FLASH_ANCINFO_ADDR + offset, (u16 *)anchor_pos_list.ancPos, size / 2);
	offset += size;
}


/*load anchors info from stmflash, based on block*/
/*each block is 2k, so each block can save 113 anchors info, we set 112, remain space for other info*/
u8 LoadAncInfo(void) {
	u16 size = 4;
	u16 offset = 0;
	STMFLASH_Read(FLASH_ANCINFO_ADDR + offset, (u16 *)&anchor_pos_list, size / 2);
	offset += size;
	
	if(anchor_pos_list.version == 0xff) {
		clearAncPosList();
		return 2;
	}
	
	size = sizeof(anchor_pos) * anchor_pos_list.count;
	STMFLASH_Read(FLASH_ANCINFO_ADDR + offset, (u16 *)anchor_pos_list.ancPos, size / 2);
	offset += size;
	return 0;
}

