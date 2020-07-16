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
			}
		}
		free(areaListHead.area);
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
	//¼ÆËãÇøÓò¿éËùÕ¼ÓĞµÄÄÚ´æ´óĞ¡£¬²»°üÀ¨Á´±íÖ¸ÕëÕ¼ÓÃµÄ¿Õ¼ä
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
	//¼ÆËã¹ÜÀíËùĞè½á¹¹µÄ´óĞ¡£¨²»°üÀ¨Ö¸ÕëÕ¼ÓĞ¿Õ¼ä£©£¬±ØĞëÏÈ»ñÈ¡Õâ¸ö´óĞ¡£¬½ÓÊÕ·½²ÅÄÜ¹¹½¨³ö¹ÜÀí½á¹¹£¬
	u16 maxoff = areaListHead.count * (DAN_AREA_ID_SIZE + 2) + 2;
	u16 readbytes = 0;
	if(size == 0) {
		error_printf("invalid argument: size = 0\n");
		return 0;
	}
	if(areaListHead.size == 0 || areaListHead.area == NULL) {
		return 0;
	}
	if(offset == 0) {
		if(size < 2) {
			error_printf("invalid argument: size < 2\n");
			return 0;
		}
		readbytes = 2;
		dest[0] = getVersionOfArea();
		dest[1] = areaListHead.count;
		size -= 2;
		dest += 2;
		offset = 2;
	}
	if(offset < maxoff) {//×¢Òâ£¬´Ë´¦²»ÄÜ¼ÓµÈºÅ=£¬²»È»»áµ¼ÖÂ¶Á´íÎó
		//ÏÈ°Ñ¹ÜÀí½á¹¹ĞÅÏ¢·¢ËÍ¹ıÈ¥
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
		//È»ºó°ÑÕæÕıµÄµã×ø±ê·¢ËÍ¹ıÈ¥
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
	//Ğ´µÄÔ­ÀíºÍÉÏÃæ¶ÁµÄÔ­ÀíÀàËÆ£¬ÏÈ¼ÆËã¹ÜÀíĞÅÏ¢´óĞ¡
	u16 maxoff = areaListHead.count * (DAN_AREA_ID_SIZE + 2) + 2;
	u16 writebytes = 0;
	if(size == 0) {
		error_printf("invalid argument: size = 0\n");
		return offset;
	}
	if(offset == 0) {
		if(size < 2) {
			error_printf("invalid argument: size < 2\n");
			return offset;
		}
		if(setAreaBlockCount(src[1]) != 0)	return 0;
		updateVersionOfArea(src[0]);
		offset = 2;
		size -= 2;
		src += 2;
		writebytes = 2;
	}
	if(offset < maxoff) {//×¢Òâ£¬´Ë´¦²»ÄÜ¼ÓµÈºÅ=£¬²»È»»áµ¼ÖÂ¶Á´íÎó
		//¸´ÖÆ¹ÜÀíĞÅÏ¢
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
		//È»ºóÊÇÕæÕıµÄµã×ø±ê
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
	
	areaListHead.version = (areaListHead.version + 1) & VERSION_MASK;//¿ØÖÆ°æ±¾ºÅÔÚ1~15Ö®¼ä
	if(areaListHead.version == 0) {
		areaListHead.version = 1;//°æ±¾ºÅ0ÓĞÌØÊâº¬Òå£º¸ÃÊı¾İÎŞĞ§
	}
}

u8 getVersionOfArea(void) {
	if(areaListHead.version == 0) {
		updateVersionOfArea(0);
	}
	
	if(areaListHead.count == 0 || areaListHead.area == NULL) {
		return 0;
	}
	//Èç¹ûµ±Ç°Êı¾İÕıÔÚÏÂÔØ£¬ÔòÔİÊ±·µ»Ø°æ±¾0
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
	setAreaBlockCount(1); //10
	for(i = 0; i < 1; i++) {
		for(j = 0; j < 6; j++) {
			dangerAreaTest_id[j] = (i + 1) * 10 + (j + 1);
		}
//		for(j = 0; j < 10; j++) {
//			dangerAreaTest_pos[j].x = (i + 1) * 1100 + (j + 1) * 10 + 1;
//			dangerAreaTest_pos[j].y = (i + 1) * 1100 + (j + 1) * 10 + 2;
//			dangerAreaTest_pos[j].z = (i + 1) * 1100 + (j + 1) * 10 + 3;
//		}
		
		//addAreaBlock(dangerAreaTest_id, (u8 *)dangerAreaTest_pos, 120);
		
		dangerAreaTest_pos[0].x = -400;
		dangerAreaTest_pos[0].y = 11180;
		dangerAreaTest_pos[0].z = 1030;
		
		dangerAreaTest_pos[1].x = 7110;
		dangerAreaTest_pos[1].y = 8140;
		dangerAreaTest_pos[1].z = 1570;
		
		dangerAreaTest_pos[2].x = 7030;
		dangerAreaTest_pos[2].y = 15809;
		dangerAreaTest_pos[2].z = 1730;
		
		dangerAreaTest_pos[3].x = -3020;
		dangerAreaTest_pos[3].y = 15809;
		dangerAreaTest_pos[3].z = 1730;
		
		addAreaBlock(dangerAreaTest_id, (u8 *)dangerAreaTest_pos, 12 * 4);
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
/*×¢£ºĞ´×îºÃ¸Ä³ÉĞ£ÑéĞ´£¬ÒÔ·ÀÊı¾İĞ´Èë´íÎóÔì³ÉÂé·³*/
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

	dangerAreaClear();//ÏÈ°ÑÖ®Ç°µÄ¿Õ¼äÇå¿Õ
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
		areaListHead.area[i].poslist = NULL;//½«Ö¸ÕëÇå¿Õ
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
	return 0;
}

/*
*·µ»Ø£ºbit[0]:	0--²»ÔÚÇøÓòÄÚ
				1--ÔÚÇøÓòÄÚ
		bit[1]--±êÊ¶³¬¸ß
		bit[2]--±êÊ¶ÔÚ¶à±ßĞÎÄÚ
*/
#define DANGER_FLAG		0x00
#define EXCEED_HIGH		0x04
#define INSIDE_AREA		0x02
u8 isInDangerAreas(int32_t x, int32_t y, int32_t z) {
	u16 crossingNum = 0;
	int32_t *p1 = 0;
	int32_t *p2 = 0;
	u8 i = 0;	//ÇøÓòÏÂ±ê
	u16 idx = 0, jdx = 0;//¶¥µãÏÂ±ê
	area_pos_t pos;
	area_pos_t *plist = 0;
	double slope = 0;
	
	if(areaListHead.area == NULL)	return 0;//ËµÃ÷Ã»ÓĞÎ£ÏÕÇøÓò
	
	pos.x = x;
	pos.y = y;
	pos.z = z;
	for(i = 0; i < areaListHead.count; ++i) {
		if(areaListHead.area[i].poslist == NULL)	continue;//ËµÃ÷¸ÃÖ¸Õë´íÎó
		if(areaListHead.area[i].size == 1) {
			//ÅĞ¶ÏÊÇ·ñ³¬¸ß£
			p1 = (int32_t *)&pos;
			p2 = (int32_t *)&areaListHead.area[i].poslist[0];
			for(idx = 0; idx < 3; ++idx) {
				if(p2[idx] != 0 && p1[idx] > p2[idx]) {
					return (DANGER_FLAG | EXCEED_HIGH);
				}
			}
		} else if(areaListHead.area[i].size >= 3){
			//ÅĞ¶ÏÊÇ·ñÔÚ¶à±ßĞÎÄÚ
			plist = areaListHead.area[i].poslist;
			crossingNum = 0;
			for(idx = 0, jdx = areaListHead.area[i].size - 1; idx < areaListHead.area[i].size; jdx = idx++) {
				if(plist[idx].x != plist[jdx].x) {
					if((plist[jdx].x <= x && x < plist[idx].x) || (plist[idx].x <= x && x < plist[jdx].x)) {
						slope = (double)(plist[idx].y - plist[jdx].y) / (1.0 * (plist[idx].x - plist[jdx].x));
						slope = slope * (x - plist[jdx].x) + plist[jdx].y - y;
						if(slope > 0.0)	crossingNum++;
					}
				}
			}
			if((crossingNum&0x01) == 0x01) {
				return (DANGER_FLAG | INSIDE_AREA);
			}
		}
	}
	return 0;
}


