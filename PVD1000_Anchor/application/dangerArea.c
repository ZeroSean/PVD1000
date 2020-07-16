#include "dangerArea.h"

#define VERSION_MASK	0x0f

areaList_t areaListHead;

/*init the areaListHead base data*/
void dangerAreaInit(void) {
	areaListHead.count = 0;
	areaListHead.size = 0;
	areaListHead.version = 0;
	areaListHead.area = NULL;
	return;
}

/*free the malloc memory, reset the areaListHead*/
void dangerAreaClear(void) {
	u8 i = 0;
	if(areaListHead.area != NULL) {
		for(i = 0; i < areaListHead.count; i++) {
			if(areaListHead.area[i].poslist != NULL) {
				free(areaListHead.area[i].poslist);
				areaListHead.area[i].poslist = NULL;
			}
		}
		free(areaListHead.area);
		areaListHead.area = NULL;
	}
	areaListHead.count = 0;
	areaListHead.size = 0;
	return;
}

/*set block count and malloc the memory*/
u8 setAreaBlockCount(u8 count) {
	u8 i = 0;
	dangerAreaClear();
	areaListHead.count = count;
	if(count == 0) {
		return 0;
	}
	areaListHead.size = 1;
	areaListHead.area = (danArea_t *)malloc(sizeof(danArea_t) * count);
	if(areaListHead.area == NULL) {
		dangerAreaClear();
		return 1;	//malloc error, may be no memory
	}
	
	for(i = 0; i < count; i++) {
		areaListHead.area[i].size = 0;
		areaListHead.area[i].poslist = NULL;
	}
	//updateVersionOfArea(0);	
	return 0; //success
}

/*write area data, each one block, the data come from network*/
/*src carry points coordinates, length is the size of the buffer src
*Note: the src data must belong one area block and must be intact.*/
u8 addAreaBlock(u8 *id, u8 *src, u16 length) {
	u8 i = 0;
	u16 j = 0;
	u16 poscount = 0;
	area_pos_t *ptr = (area_pos_t *)src;
	for(i = 0; i < areaListHead.count; i++) {
		if(areaListHead.area[i].size == 0) {
			if(length % 12 != 0) {
				return 2;		//length error, must be devide by 3
			}
			poscount = length / 12; //calculate the points num
			if(areaListHead.area[i].poslist == NULL) {
				areaListHead.area[i].poslist = (area_pos_t *)malloc(sizeof(area_pos_t) * poscount);
				if(areaListHead.area[i].poslist == NULL) {
					return 1;	//malloc error, may be no memory
				}
			}
			
			//copy point coordinates
			areaListHead.area[i].size = poscount;
			for(j = 0; j  < poscount; j++) {
				areaListHead.area[i].poslist[j].x = ptr[j].x;
				areaListHead.area[i].poslist[j].y = ptr[j].y;
				areaListHead.area[i].poslist[j].z = ptr[j].z;
			}
			//copy block id
			for(j = 0; j < DAN_AREA_ID_SIZE; j++) {
				areaListHead.area[i].id[j] = id[j]; 
			}
			areaListHead.size += (sizeof(area_pos_t) * poscount + DAN_AREA_ID_SIZE + 2);
			return 0;
		}
	}
	return 3;	//error: bound of block count
}

u16 getAreaDataSize(void) {
	u8 i = 0;
//	if(areaListHead.size != 0) {
//		return areaListHead.size;
//	}
	//计算区域块所占有的内存大小，不包括链表指针占用的空间
	areaListHead.size = 2 + areaListHead.count * (2 + DAN_AREA_ID_SIZE);
	for(i = 0; i < areaListHead.count; i++) {
		areaListHead.size += areaListHead.area[i].size * sizeof(area_pos_t);
	}
	return areaListHead.size;
}

/*because area data too long and use pointer, so here build our data fomat
 *if offset < maxoff, mean this will be first data paket, we send the data struction information for receiver,
 *			and the receirver will build the data struction base on the information
 *if offset >= maxoff, then send the position data of each block
 *format: blockCount(1 byte), id1, size1....idn, sizen, areadata1, ... areadatan 
 */
u16 readAreaData(u8 *dest, u16 size, u16 offset) {
	u16 i = 0;
	u8 j = 0, maxcout;
	u16 start = 0, idx = 0;
	
	//计算管理所需结构的大小（不包括指针占有空间），必须先获取这个大小，接收方才能构建出管理结构，
	u16 maxoff = areaListHead.count * (DAN_AREA_ID_SIZE + 2) + 2;
	u16 readbytes = 0;
	
	if(size == 0) {
		error_printf("Invalid argument: size is zero\n");
		return 0;
	}
	
	if(areaListHead.size == 0 || areaListHead.area == NULL) {
		return 0;
	}
	if(offset == 0) {
		if(size < 2) {
			error_printf("Invalid argument: size less than 2\n");
			return 0;
		}
		readbytes = 2;
		dest[0] = getVersionOfArea();
		dest[1] = areaListHead.count;
		size -= 2;
		dest += 2;
		offset = 2;
	}
	if(offset < maxoff) {//注意，此处不能加等号=，不然会导致读错误
		//先把管理结构信息发送过去
		start = (offset - 2) / (DAN_AREA_ID_SIZE + 2);
		maxcout = size / (DAN_AREA_ID_SIZE + 2);
		for(i = start; i < areaListHead.count && i < (maxcout + start); i++) {
			idx = (i - start) * (DAN_AREA_ID_SIZE + 2);
			for(j = 0; j < DAN_AREA_ID_SIZE; j++) {
				dest[idx + j] = areaListHead.area[i].id[j];
			}
			dest[idx + DAN_AREA_ID_SIZE] = areaListHead.area[i].size & 0xff;
			dest[idx + DAN_AREA_ID_SIZE + 1] = (areaListHead.area[i].size >> 8) & 0xff;
			readbytes += (DAN_AREA_ID_SIZE + 2);
		}
	} else {
		//然后把真正的点坐标发送过去
		u8 *src = NULL;
		start = offset - maxoff;
		for(j = 0; j < areaListHead.count; j++) {
			if(start < areaListHead.area[j].size * sizeof(area_pos_t)) {
				break;
			}
			start -= areaListHead.area[j].size * sizeof(area_pos_t);
		}
		if(j == areaListHead.count)	return readbytes;
		while(1) {
			src = (u8 *)areaListHead.area[j].poslist;
			for(i = 0; (i + start)< areaListHead.area[j].size * sizeof(area_pos_t) && (i + readbytes) < size; i++) {
				dest[readbytes + i] = src[i + start];
			}
			readbytes += i;
			if(++j == areaListHead.count || readbytes == size) {
				break;
			}
			start = 0;
		}
	}
	return readbytes;
}

u16 writeAreaData(u8 *src, u16 size, u16 offset) {
	u16 i = 0;
	u8 j = 0, maxcout;
	u16 start = 0, idx = 0;
	//写的原理和上面读的原理类似，先计算管理信息大小
	u16 maxoff = areaListHead.count * (DAN_AREA_ID_SIZE + 2) + 2;
	u16 writebytes = 0;
	
	if(size == 0) return offset;
	if(offset == 0) {
		if(size < 2) {
			error_printf("Invalid argument: size less than 2\n");
			return 0;
		}
		if(setAreaBlockCount(src[1]) != 0)	return 0;
		updateVersionOfArea(src[0]);
		offset = 2;
		size -= 2;
		src += 2;
		writebytes = 2;
	}
	if(offset < maxoff) {//注意，此处不能加等号=，不然会导致读错误
		//复制管理信息
		start = (offset - 2) / (DAN_AREA_ID_SIZE + 2);
		maxcout = size / (DAN_AREA_ID_SIZE + 2);
		for(i = start; i < areaListHead.count && i < (maxcout + start); i++) {
			idx = (i - start) * (DAN_AREA_ID_SIZE + 2);
			for(j = 0; j < DAN_AREA_ID_SIZE; j++) {
				areaListHead.area[i].id[j] = src[idx + j];
			}
			areaListHead.area[i].size = (src[idx + DAN_AREA_ID_SIZE + 1] << 8) | src[idx + DAN_AREA_ID_SIZE];
			areaListHead.area[i].poslist = (area_pos_t *)malloc(sizeof(area_pos_t) * areaListHead.area[i].size);
			if(areaListHead.area[i].poslist == NULL) {
				dangerAreaClear();
				return 0;
			}
			offset += (DAN_AREA_ID_SIZE + 2);
		}
	} else {
		//然后是真正的点坐标
		u8 *dest = NULL;
		start = offset - maxoff;
		for(j = 0; j < areaListHead.count; j++) {
			if(start < areaListHead.area[j].size * sizeof(area_pos_t)) {
				break;
			}
			start -= areaListHead.area[j].size * sizeof(area_pos_t);
		}
		if(j == areaListHead.count)	return offset;
		while(1) {
			dest = (u8 *)areaListHead.area[j].poslist;
			for(i = 0; (i + start)< areaListHead.area[j].size * sizeof(area_pos_t) && (i + writebytes) < size; i++) {
				dest[i + start] = src[writebytes + i];
			}
			writebytes += i;
			offset += i;
			if(++j == areaListHead.count || writebytes == size) {
				break;
			}
			start = 0;
		}
		areaListHead.size = 0;
	}
	return offset;
}


void downloadAreaDataReg(void) {
	creatDownloadTask(DOWNLOAD_TASK_TYPE_DANAREA | UPLOAD_TYPE_MASK, getAreaDataSize, readAreaData, writeAreaData);
}

void requestAreaData(void) {
	creatDownloadTask(DOWNLOAD_TASK_TYPE_DANAREA, getAreaDataSize, readAreaData, writeAreaData);
}

void updateVersionOfArea(u8 version) {
	if(version & VERSION_MASK) {
		areaListHead.version = version & VERSION_MASK;
		return;
	}
	
	areaListHead.version = (areaListHead.version + 1) & VERSION_MASK;//控制版本号在1~15之间
	if(areaListHead.version == 0) {
		areaListHead.version = 1;//版本号0有特殊含义：该数据无效
	}
}

u8 getVersionOfArea(void) {
	if(areaListHead.version == 0) {
		updateVersionOfArea(0);
	}
	if(areaListHead.count == 0 || areaListHead.area == NULL) {
		return 0;
	}
	//如果当前数据正在下载，则暂时返回版本0
	if(hasDownloadTask(DOWNLOAD_TASK_TYPE_DANAREA)) {
		return 0;
	}
	return (areaListHead.version & VERSION_MASK);
}


/*test code*/
u8 dangerAreaTest_id[6];
area_pos_t dangerAreaTest_pos[10];
void dangerAreaDataInit_Test(void) {
	u8 i = 0, j = 0;
	setAreaBlockCount(10);
	for(i = 0; i < 10; i++) {
		for(j = 0; j < 6; j++) {
			dangerAreaTest_id[j] = (i + 1) * 10 + (j + 1);
		}
		for(j = 0; j < 10; j++) {
			dangerAreaTest_pos[j].x = (i + 1) * 1100 + (j + 1) * 10 + 1;
			dangerAreaTest_pos[j].y = (i + 1) * 1100 + (j + 1) * 10 + 2;
			dangerAreaTest_pos[j].z = (i + 1) * 1100 + (j + 1) * 10 + 3;
		}
		addAreaBlock(dangerAreaTest_id, (u8 *)dangerAreaTest_pos, 120);
	}
}


void dangerAreaDataPrint_Test(void) {
	u8 i = 0;
	u8 j = 0;
	printf("***************danger area test*******************\n");
	printf("Danger Area count:	%d\n", areaListHead.count);
	printf("Danger Area bytes:	%d\n", getAreaDataSize());
	for(i = 0; i < areaListHead.count; i++) {
		printf("%d area id: %d-%d-%d-%d-%d-%d\n", i+1, areaListHead.area[i].id[0], areaListHead.area[i].id[1], areaListHead.area[i].id[2],
													areaListHead.area[i].id[3], areaListHead.area[i].id[4], areaListHead.area[i].id[5]);
		printf("point count: %d\n", areaListHead.area[i].size);
		printf("points: ");
		for(j = 0; j < areaListHead.area[i].size; j++) {
			printf("(%d,%d,%d) ", areaListHead.area[i].poslist[j].x, areaListHead.area[i].poslist[j].y, areaListHead.area[i].poslist[j].z);
		}
		printf("\n");
	}
	printf("********************test end**********************\n");
}

/*save danger area info into stmflash, based on block*/
/*store struct: areaList_t, danArea_t1. danArea_t2, danArea_t3, ...
area_pos_t1, area_pos_t2, ...*/
/*注：写最好改成校验写，以防数据写入错误造成麻烦*/
void StoreAreaInfo(void) {
	u16 size = sizeof(areaList_t);
	u16 offset = 0;
	u8 i = 0;
	STMFLASH_Write(FLASH_DAGAREA_ADDR + offset, (u16 *)&areaListHead, size / 2);
	offset += size ;

	size = sizeof(danArea_t) * areaListHead.count;
	STMFLASH_Write(FLASH_DAGAREA_ADDR + offset, (u16 *)areaListHead.area, size / 2);
	offset += size ;

	for(i = 0; i < areaListHead.count; i++) {
		size = sizeof(area_pos_t) * areaListHead.area[i].size;
		STMFLASH_Write(FLASH_DAGAREA_ADDR + offset, (u16 *)areaListHead.area[i].poslist, size / 2);
		offset += size ;
	}
}


u8 LoadAreaInfo(void) {
	u16 size = sizeof(areaList_t);
	u16 offset = 0;
	u8 i = 0;

	dangerAreaClear();//先把之前的空间清空
	STMFLASH_Read(FLASH_DAGAREA_ADDR + offset, (u16 *)&areaListHead, size / 2);
	offset += size ;
	areaListHead.area = NULL;

	if(areaListHead.version == 0xff) {
		dangerAreaClear();
		return 2;
	}

	size = sizeof(danArea_t) * areaListHead.count;
	areaListHead.area = (danArea_t *)malloc(size);
	if(areaListHead.area == NULL) {
		dangerAreaClear();
		return 1;	//malloc error, may be no memory
	}
	STMFLASH_Read(FLASH_DAGAREA_ADDR + offset, (u16 *)areaListHead.area, size / 2);
	offset += size ;
	
	for(i = 0; i < areaListHead.count; i++) {
		areaListHead.area[i].poslist = NULL;//将指针清空
	}
	
	for(i = 0; i < areaListHead.count; i++) {
		size = sizeof(area_pos_t) * areaListHead.area[i].size;
		areaListHead.area[i].poslist = (area_pos_t *)malloc(size);
		if(areaListHead.area[i].poslist == NULL) {
			dangerAreaClear();
			return 1;	//malloc error, may be no memory
		}
		STMFLASH_Read(FLASH_DAGAREA_ADDR + offset, (u16 *)areaListHead.area[i].poslist, size / 2);
		offset += size ;
	}
	dangerAreaDataPrint_Test();
	return 0;
}


