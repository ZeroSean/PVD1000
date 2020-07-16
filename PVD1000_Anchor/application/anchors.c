#include "anchors.h"

#define VERSION_MASK	0x0f

struct {
	u8 version;	//基站版本号
	u8 count;
	anchor_pos ancPos[MAX_ANCPOS_NUM];
}anchor_pos_list;

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
		
		u3_printf_str("rec:(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)\n", x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2], x[3], y[3], z[3]);
		printf("rec:(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)\n", x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2], x[3], y[3], z[3]);
		
		updateVersionOfAncs(0);
		printf("update anchors version:%d!\n", getVersionOfAncs());
		return 0;
	}
	return 1;
}

void ancPosListInitTest(void) {
	int i = 0;
	u8  ID[7] ={0, 	   1, 	 2,    3, 	 4,    5, 	  6};
	int X[7] = {11168, 1175, 5384, 780,  8200, 15874, 8609};
	int Y[7] = {6530,  7110, 7567, 350,  10,   12753, 15117};
	int Z[7] = {-1348, -1686,-2100,-1731,-1030,-1764, -1733};
	anchor_pos_list.count = 7;
	for(i = 0; i < anchor_pos_list.count; ++i) {
		anchor_pos_list.ancPos[i].id[ANCHOR_ID_SIZE - 1] = ID[i];
		anchor_pos_list.ancPos[i].pos[0] = X[i];
		anchor_pos_list.ancPos[i].pos[1] = Y[i];
		anchor_pos_list.ancPos[i].pos[2] = Z[i];
	}
	updateVersionOfAncs(0);
}

void ancPosListPrint(u8 n) {
	int i = 0;
	printf("Anchors Num: %d, version:%d\n", anchor_pos_list.count, getVersionOfAncs());
	for(i = 0; i < anchor_pos_list.count; i++) {
		printf("Anchors %d (%x)---(%d, %d, %d)\n", i, anchor_pos_list.ancPos[i].id[ANCHOR_ID_SIZE - 1], anchor_pos_list.ancPos[i].pos[0], \
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
	if(size == 0) return 0;
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
//返回下一次写的偏移量
u16 writeAnchorsList(u8 *src, u16 size, u16 offset) {
	u8 *dest = (u8 *)&anchor_pos_list.ancPos;
	u16 i = 0;
	u8 count = 0;
	if(size == 0) {
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
	
	count = (i - 1) / sizeof(anchor_pos);
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


