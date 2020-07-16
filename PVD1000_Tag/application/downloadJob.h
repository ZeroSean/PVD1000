#ifndef __DOWNLOAD_JOB_H_
#define __DOWNLOAD_JOB_H_

#include <stdio.h>
#include <stdlib.h>
#include "stm32f10x.h"

#include "host_usart.h"

//bit[7]置1，则任务可作为下载请求回应节点；
//bit[7]置0，则任务为下载数据请求节点
#define DOWNLOAD_TASK_TYPE_ANCLIST	0x01
#define DOWNLOAD_TASK_TYPE_DANAREA	0x02

#define UPLOAD_TYPE_MASK			0x80	
//#define TYPE_MASK					0x7f

//download structure
typedef struct download_str{
	u8 type;
	u16 curlength;
	u16 length;
	u8 state;	//0--free, 1--no start, 2--downloading, 3--finished
	u16 (*dataSizeFun)(void);
	u16 (*readDataFun)(u8 *, u16, u16);
	u16 (*writeDataFun)(u8 *, u16, u16);
	struct download_str *next;
}download_t;

/*downloading task*/
void creatDownloadTask(u8 type, u16 (*dataSizeFun)(void), 
	u16 (*readDataFun)(u8 *, u16, u16),u16 (*writeDataFun)(u8 *, u16, u16));
u8 updateDownload(u8 type, u16 curlength, u16 length);

u16 downloadGetSize(u8 type);
u16 downloadRead(u8 type, u8 *dest, u16 size, u16 offset);
u16 downloadWrite(u8 type, u8 *src, u16 size, u16 offset);

u8 getDownloadTaskType(void);
u16 getDownloadCurLength(void);
download_t *getDownloadTask(void);
u8 downloadTaskIsEmpty(void);

u8 hasDownloadTask(u8 type);



#endif
