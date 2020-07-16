#include "downloadJob.h"

download_t *downloadReqQueueHead = NULL;	//下载请求节点队列
download_t *downloadRespQueueHead = NULL;	//下载应答节点队列

download_t* findDownload(download_t* queueHead, u8 type) {
	while(queueHead != NULL) {
		if(queueHead->type == type) {
			return queueHead;
		}
		queueHead = queueHead->next;
	}
	return NULL;
}

u8 hasDownloadTask(u8 type) {
	download_t* task = NULL;
	if(type & UPLOAD_TYPE_MASK) {
		task = findDownload(downloadRespQueueHead, type);
		return (task == NULL ? 0 : 1);
	} else {
		task = findDownload(downloadReqQueueHead, type);
		return (task == NULL ? 0 : 1);
	}
}

//根据type类型将任务插入下载请求节点或下载应答节点队列中
void creatDownloadTask(u8 type, u16 (*dataSizeFun)(void), 
	u16 (*readDataFun)(u8 *, u16, u16), u16 (*writeDataFun)(u8 *, u16, u16)) {
	download_t **ptr = NULL;
	if(type & UPLOAD_TYPE_MASK) {
		ptr = &downloadRespQueueHead;
	} else {
		ptr = &downloadReqQueueHead;
	}
	
	while(*ptr != NULL) {
		if((*ptr)->type == type) {
			return;	//download task already exist;
		}
		ptr = &((*ptr)->next);
	}
	*ptr = (download_t *)malloc(sizeof(download_t));
	if(ptr == NULL) {
		error_printf("error:download task malloc error!\n");
		return;
	}
	(*ptr)->type = type;
	(*ptr)->curlength = 0;
	(*ptr)->length = 0;
	(*ptr)->state = 1; //no start
	(*ptr)->dataSizeFun = dataSizeFun;
	(*ptr)->writeDataFun = writeDataFun;
	(*ptr)->readDataFun = readDataFun;
	(*ptr)->next = NULL;
	return;
}

u8 updateDownload(u8 type, u16 curlength, u16 length) {
	download_t *ptr = NULL;
	if((downloadReqQueueHead == NULL) || (downloadReqQueueHead->type != type)) {
		return 0;
	}
	downloadReqQueueHead->curlength = curlength;
	downloadReqQueueHead->length = length;
	if(curlength >= length) {
		downloadReqQueueHead->state = 3; //finished
		ptr = downloadReqQueueHead;
		downloadReqQueueHead = downloadReqQueueHead->next;
		free(ptr);
		return 3;	//下载任务完成
	} else {
		downloadReqQueueHead->state = 2; //downloading
		return 2;
	}
	//return;
}

download_t *getDownloadTask(void) {
	return downloadReqQueueHead;
}

u8 downloadTaskIsEmpty(void) {
	if(downloadReqQueueHead == NULL) {
		return 1;
	}
	return 0;
}

u16 downloadGetSize(u8 type) {
	download_t *ptr = NULL;	//从应答队列寻找
	//下载请求队列中存在该任务，则说明该节点数据还没有完成更新，暂时不读取数据
	if(findDownload(downloadReqQueueHead, (type&(~UPLOAD_TYPE_MASK))) != NULL)	return 1;//标识暂时数据被锁住
	
	ptr = findDownload(downloadRespQueueHead, (type|UPLOAD_TYPE_MASK));
	if(ptr == NULL) {
		return 0;	//没有注册下载
	}
	return ptr->dataSizeFun();
}

u16 downloadRead(u8 type, u8 *dest, u16 size, u16 offset) {
	download_t *ptr = NULL;//从应答队列寻找
	//下载请求队列中存在该任务，则说明该节点数据还没有完成更新，暂时不读取数据
	if(findDownload(downloadReqQueueHead, (type&(~UPLOAD_TYPE_MASK))) != NULL)	return 0;
	
	ptr = findDownload(downloadRespQueueHead, (type|UPLOAD_TYPE_MASK));
	if(ptr == NULL) {
		return 0;
	}
	return ptr->readDataFun(dest, size, offset);
}


u16 downloadWrite(u8 type, u8 *src, u16 size, u16 offset) {
	download_t *ptr = findDownload(downloadReqQueueHead, (type&(~UPLOAD_TYPE_MASK)));//从请求队列寻找
	if(ptr == NULL) {
		return 0;
	}
	return ptr->writeDataFun(src, size, offset);
}

u8 getDownloadTaskType(void) {
	download_t *ptr = NULL;
	while(1) {
		if(downloadReqQueueHead == NULL) {
			return 0;	//no task
		}
		if(downloadReqQueueHead->state == 0 || downloadReqQueueHead->state == 3) {
			ptr = downloadReqQueueHead;
			downloadReqQueueHead = downloadReqQueueHead->next;
			free(ptr);
		} else {
			return downloadReqQueueHead->type;
		}
	}
	//return 0xff; //undefined  error
}

u16 getDownloadCurLength(void) {
	if(downloadReqQueueHead == NULL) {
		return 0xffff;
	}
	return downloadReqQueueHead->curlength;
}
