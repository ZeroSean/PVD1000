#include "netproto.h"


#define NET_NUM		2	//UART2_FD and UART3_FD(internet and bluetooth)

/*temp static gateway id*/
u8 gw_id[GATEWAYID_LENGTH] = {0x41, 0x44, 0x30, 0x39, 0x30, 0x31};

typedef struct LocalNetStruct{
	u8 nextState;
	u8 fd;
	u32 sendDuraTime;
	u32 sendNetdataTime;
	u32 sendHeartPacketTime;
	u32 sendHeartPacketDuraTime;
	char* (*read)(char*, u16*);
	char (*send)(char*, u16);
	void (*freeBuf)(char *);
	u16 (*getSendCount)(void);
	u8 (*checkState)(void);
	char (*otherParse)(struct LocalNetStruct*, char*, u16);
	NET_MSG_S netmsg;
}LocalNetS;

LocalNetS localNetList[NET_NUM];


typedef struct {
	u16 length;	//数据长度
	u8 data[MAX_DATALENGTH];
}PosNetBufStr;

//为避免不同设备共用同一个缓冲，此处应该对不同设备分配不同的缓冲，
//但目前为节省内存，暂时公用一个缓冲,使用时需要小心，以免不同设备对其更改破坏数据有效性
PosNetBufStr posNetBuf;

void addTagInfo(u16 addr, u8 *pos, u8 alarm, uint16 elecValue, u8 seqNum) {
	u16 offset = posNetBuf.length;
	u8 i = 0;
	
	if((offset + 21) <= MAX_DATALENGTH) {
		for(i = 0; i < 4; ++i) {
			posNetBuf.data[offset+ i] = 0;
		}
		posNetBuf.data[offset+ i + 0] = (addr >> 8) & 0xff;
		posNetBuf.data[offset+ i + 1] = addr & 0xff;
		offset += 6;
		for(i = 0; i < 12; ++i) {
			posNetBuf.data[offset+ i] = pos[i];
		}
		offset += 12;
		posNetBuf.data[offset] = alarm;
		posNetBuf.data[offset+1] = (elecValue >> 8) & 0xff;
		posNetBuf.data[offset+2] = elecValue & 0xff;
		posNetBuf.length = offset + 3;
		posNetBuf.data[TAG_NUM_OFFSET] += 1;
	} else {
		//说明缓冲空闲空间不足，忽略该数据
	}
	debug_printf("add %x seq %d--%d\n", addr, seqNum, posNetBuf.data[TAG_NUM_OFFSET]);
	return;
}

void cpy_from_UWBBuf_to_PosNetBuf(uint8 num, uint8* dat) {
	uint8 i = 0;
	u16 addr = 0;
	uint16 elecValue;
	for(i = 0; i < num; ++i) {
		if((posNetBuf.length + 21) > MAX_DATALENGTH)	return;//20
		
		ByteOrder_Inverse_Int(&(dat[2]), NULL, 3);
		addr = dat[0] + (dat[1] << 8);
		elecValue = dat[15] + (dat[16] << 8);
		
		addTagInfo(addr, &(dat[2]), dat[14], elecValue, dat[17]);

		dat += 18;
	}
}

void clearPosNetBuf() {
	posNetBuf.data[TAG_NUM_OFFSET] = 0;
	posNetBuf.length = TAG_NUM_OFFSET + TAG_NUM_LENGTH;
}

//char netpro_sendreq(NET_MSG_S *netmsg, u8 ctrlcode);							//send require data
//char netpro_send(LocalNetS *localNet, u8 ctrlcode, u8 *data, uint16_t length);	//directly send
//char netpro_write(u8 ctrlcode, u8 *data, uint16_t length);	//write data to buffter until buffer withou space and then send it
//char netpro_parse(LocalNetS *localNet);										//parse the receive data from gateway server

//static char * read(u8 fd, char *rbuf, u16 *length) {
//	if(fd == UART2_FD) {
//		return DMA_UART_read(rbuf, length);
//	} else if(fd == UART3_FD) {
//		return DMA_UART3_read(rbuf, length);
//	}
//	return NULL;
//}

///*0 for succ, 1 for send error, 2 for fd not supported*/
//static char send(u8 fd, char *netmsg, u16 length) {
//	if(fd == UART2_FD) {
//		return DMA_UART_Send(netmsg, length);
//	} else if(fd == UART3_FD) {
//		return DMA_UART3_Send(netmsg, length);
//	}
//	return 2;
//}

//static void freeBuf(u8 fd, char *ptr) {
//	if(fd == UART2_FD) {
//		DMA_UART_freeRecBuf(ptr);
//	} else if(fd == UART3_FD) {
//		DMA_UART3_freeRecBuf(ptr);
//	}
//}

//static uint16_t getSendCount(u8 fd) {
//	if(fd == UART2_FD) {
//		return DMA_UART_getSendCount();
//	} else if(fd == UART3_FD) {
//		return DMA_UART3_getSendCount();
//	}
//	return 0;
//}
	
void registerNetPro(LocalNetS *localNet, char* (*read)(char*, u16*), char (*send)(char*, u16),
	void (*freeBuf)(char *), u16 (*getSendCount)(void), u8 (*checkState)(void), char (*otherParse)(LocalNetS *localNet, char*, u16)) {
	localNet->read = read;
	localNet->send = send;
	localNet->freeBuf = freeBuf;
	localNet->getSendCount = getSendCount;
	localNet->checkState = checkState;
	localNet->otherParse = otherParse;
}

static const char bluetoothCommandHeader[6] = "AT-AB ";	
static const char bluetoothCommandConnetUp[12] = "ConnectionUp";
static const char bluetoothCommandConnetDown[14] = "ConnectionDown";
static char strCompare(const char* str1, const char* str2, u16 size) {
	u16 i = 0;
	for(i = 0; i < size; ++i) {
		if(str1[i] != str2[i]) {
			return (str1[i] > str2[i] ? 1 : -1);
		}
	}
	return 0;
}

char bluetoothParse(LocalNetS *localNet, char* buf, u16 length) {
	if((buf[0] == 'S' || buf[0] == 's') && (setAncListPosition(buf) == 0)) {
		return 0;
	} else if((length > 6) && (strCompare(bluetoothCommandHeader, buf, 6) == 0)){
		if((length >= 18) && (strCompare(bluetoothCommandConnetUp, &buf[6], 12) == 0)) {
			localNet->nextState = NC_POWERLINK;
			localNet->sendNetdataTime = portGetTickCnt();
			localNet->sendDuraTime = NEXT_SEND_DURATION;//100ms
		} else if((length >= 20) && (strCompare(bluetoothCommandConnetDown, &buf[6], 14) == 0)) {
			localNet->nextState = NC_WAITING_CONNECT;
			localNet->sendDuraTime = RESEND_DURATION * 10;
		}
		return 0;
	}
	return -1;
}

/*init the paras in front of the programs*/
void netpro_init(void) {
	u8 i, j;
	for(j = 0; j < NET_NUM; ++j) {
		localNetList[j].netmsg.fcode = START_CODE;
		for(i = 0; i < GATEWAYID_LENGTH; i++) {
			localNetList[j].netmsg.gatewayID[i] = gw_id[i];
		}
		localNetList[j].netmsg.endcode = END_CODE;
		localNetList[j].sendDuraTime = NEXT_SEND_DURATION;
		localNetList[j].sendNetdataTime = portGetTickCnt();
		localNetList[j].sendHeartPacketTime = portGetTickCnt();
		localNetList[j].sendHeartPacketDuraTime = HEART_DURATION;
	}
	/*can based on the hardware state or net state to close/open the fd*/
	localNetList[0].fd = UART2_FD;	
	localNetList[1].fd = UART3_FD;
	localNetList[0].nextState = NC_POWERLINK;
	localNetList[1].nextState = NC_WAITING_CONNECT;
	localNetList[1].sendHeartPacketDuraTime = 0xffffffff;//蓝牙不发生心跳
//	localNetList[0].checkState = getZLSNState;
//	localNetList[1].checkState = getBLEState;
	
	registerNetPro(&localNetList[0], DMA_UART_read, DMA_UART_Send, DMA_UART_freeRecBuf, DMA_UART_getSendCount, getZLSNState, NULL);
	registerNetPro(&localNetList[1], DMA_UART3_read, DMA_UART3_Send, DMA_UART3_freeRecBuf, DMA_UART3_getSendCount, getBLEState, bluetoothParse);
	
	clearPosNetBuf();
	return;
}

/*
 *based on the hardware state or net state to close/open the fd
*currently: fd only support UART2_FD and UART3_FD
*/
u8 netpro_open(u8 fd) {
	u8 i = 0;
	for(i = 0; i < NET_NUM; i++) {
		if(localNetList[i].fd == FD_CLOSE || localNetList[i].fd == fd) {
			localNetList[i].fd = fd;
			localNetList[i].nextState = NC_POWERLINK;
			localNetList[i].sendDuraTime = NEXT_SEND_DURATION;//10ms
			localNetList[i].sendNetdataTime = portGetTickCnt();
			return 0;	//succ
		}
	}
	return 1; //mean fd not supported
}

u8 netpro_close(u8 fd) {
	u8 i = 0;
	for(i = 0; i < NET_NUM; i++) {
		if(localNetList[i].fd == fd) {
			localNetList[i].fd = FD_CLOSE;
			localNetList[i].nextState = NC_POWERLINK;
			return 0;	//succ
		}
	}
	return 1; //mean fd not supported
}

void err_print(char state) {
	if(state == 0) {
		return;
	}
	switch(state) {
		case ERR_TMOUT:
			printf("ERR_TMOUT\n");
			break;
		case ERR_UNDCODE:
			printf("ERR_UNDCODE\n");
			break;
		case ERR_BFOVER:
			printf("ERR_BFOVER\n");
			break;
		case ERR_NOSPACE:
			printf("ERR_NOSPACE\n");
			break;
		case ERR_RECLENGTH:
			printf("ERR_RECLENGTH\n");
			break;
		case ERR_FCODE:
			printf("ERR_FCODE\n");
			break;
		case ERR_CHECKCODE:
			printf("ERR_CHECKCODE\n");
			break;
		case ERR_PROCESS:
			printf("ERR_PROCESS\n");
			break;
		case ERR_LENGTH:
			printf("ERR_LENGTH\n");
			break;
		case ERR_TOOSHORT:
			printf("ERR_TOOSHORT\n");
			break;
		default:
			printf("UNDEFINED ERROR!\n");
			break;
	}
	return;
}

u8 netpro_calCheckCode(u8 *data, uint16_t length) {
	uint16_t i;
	u8 checkcode = 0;
	for(i = 0; i < length; i++) {
		checkcode += data[i];
	}
	return checkcode;
}

uint16_t netpro_arraData(NET_MSG_S *netmsg, u8 *data, uint16_t length) {
	netmsg->checkcode = netpro_calCheckCode(data, length);
	data[length] = netmsg->checkcode;
	data[length + 1] = END_CODE;
	return (MIN_STATELENGTH + length - 3 + 2);
}

/*directly send*/
char netpro_send(LocalNetS *localNet, u8 ctrlcode, u8 *data, uint16_t length) {
	uint16_t i;
	uint16_t freelength = localNet->getSendCount();
	if(length > MAX_DATALENGTH) {
		//data too long, overflow the buffer 
		printf("ERR_BFOVER\n");
		return ERR_BFOVER;	
	}
	
//	if(length > (freelength - MIN_STATELENGTH)) {
//		//data send too quickly, old data no be send, without space to put new data
//		printf("ERR_NOSPACE\n");
//		return ERR_NOSPACE;	
//	}
	
	localNet->netmsg.ctrl = ctrlcode;
	localNet->netmsg.length[0] = (length >> 8) & 0xff;
	localNet->netmsg.length[1] = length & 0xff;
	if(data != NULL) {
		for(i = 0; i < length; i++) {
			localNet->netmsg.data[i] = data[i];
		}
	}
	length = netpro_arraData(&(localNet->netmsg), (u8 *)&(localNet->netmsg.ctrl), (CHECKCODE_PRESIZE + length));
	return localNet->send((char *)&(localNet->netmsg), length);
}

/*send require data*/
char netpro_sendreq(LocalNetS *localNet, u8 ctrlcode) {
	uint16_t length = 0;
	char state = 0;
	switch(ctrlcode) {
		case NC_POWERLINK:
			localNet->netmsg.data[0] = (ANC_VERSION >> 8) & 0xff;
			localNet->netmsg.data[1] = ANC_VERSION & 0xff;
			length = 2;
		
			state = netpro_send(localNet, NC_POWERLINK, NULL, length);
			break;
		case NC_SYNCLOCK:
			state = netpro_send(localNet, NC_SYNCLOCK, NULL, 0);
			break;
		default:
			state = ERR_UNDCODE;	//invalid ctrl code
			break;
	}
	//printf("state: %d\n", state);
	return state;
}

/*write data to buffter until buffer withou space and then send it*/
char netpro_write(u8 ctrlcode, u8 *data, uint16_t length) {
	return 0;
}

/*parse the receive data from gateway server*/
char netpro_parse(LocalNetS *localNet) {
	NET_MSG_S *recmsg = NULL;
	char *recbuf = NULL;
	char result = 0;
	u16 reclength = 0;
	//u16 priindex = 0; //test
	if((recbuf = localNet->read(0, &reclength)) == NULL) {
		return 0;
	}
	printf("[%d]rec data[%d]!\n", localNet->fd, reclength);
//	for(priindex = 0; priindex < reclength; priindex++) {
//		printf("%c", recbuf[priindex]);
//	}
//	printf("\n\n");
	
	if(reclength < MIN_PROTOLENGTH) {
		if((localNet->otherParse == NULL) || (localNet->otherParse(localNet, recbuf, reclength) != 0)) {
			printf("data size(%d) < MIN_PROTOLENGTH(%d)\n", reclength, MIN_PROTOLENGTH);
			result = ERR_TOOSHORT;
		}
		localNet->freeBuf(recbuf);
		return result;
	}
	
	recmsg = (NET_MSG_S *)recbuf;
	if(recmsg->fcode != START_CODE) {
		if((localNet->otherParse == NULL) || (localNet->otherParse(localNet, recbuf, reclength) != 0)) {
			printf("Start code error(%d)\n", recmsg->fcode);
			result = ERR_FCODE;
		}
		localNet->freeBuf(recbuf);
		return result;
	}
	if(netpro_calCheckCode(&recmsg->ctrl, reclength - CHECKCODE_STARTIN - 2) != recbuf[reclength - 2]) {
		printf("check code: %d--%d\n", netpro_calCheckCode(&recmsg->ctrl, reclength - CHECKCODE_STARTIN - 2), recbuf[reclength - 2]);
		localNet->freeBuf(recbuf);
		return ERR_CHECKCODE;
	}
	switch(recmsg->ctrl) {
		case NC_POWERLINK:
			localNet->nextState = NC_SYNCLOCK;
			localNet->sendDuraTime = NEXT_SEND_DURATION;
			break;
		case NC_SYNCLOCK:
			localNet->nextState = NC_TAGINFO;
			localNet->sendDuraTime = SENDPOS_DURATION;
			SysTick_SetClock(recmsg->data, 6);
			break;
		case NC_DANGERAREA:
			{
				u8 result = 1;
				u8 i = 0;
				u16 count = 0;
				u16 index = 1;
				//u16 length = (recmsg->length[0] << 8) | recmsg->length[1];
				if(setAreaBlockCount(recmsg->data[0]) != 0) {
					localNet->freeBuf(recbuf);
					result = 0;
					netpro_send(localNet, NC_DANGERAREA, &result, 1);
					return ERR_PROCESS;
				}
				for(i = 0; i < recmsg->data[0]; i++) {
					count = (recmsg->data[index + 6] << 8) | recmsg->data[index + 7];
					//printf("point count: %d-%d-%d\n", count, recmsg->data[index + 6], recmsg->data[index + 7]);
					ByteOrder_Inverse_Int(&recmsg->data[index + 8], NULL, count * 3);
					if(addAreaBlock(&recmsg->data[index], &recmsg->data[index + 8], count * 12) != 0) {
						localNet->freeBuf(recbuf);
						result = 0;
						netpro_send(localNet, NC_DANGERAREA, &result, 1);
						printf("updata area error!\n");
						return ERR_PROCESS;
					}
					index += (count * 12 + 8);
				}
				downloadAreaDataReg();	//create download task
				updateVersionOfArea(0);
				netpro_send(localNet, NC_DANGERAREA, &result, 1);
				localNet->nextState = NC_TAGINFO;
				dangerAreaDataPrint_Test();
				localNet->sendDuraTime = SENDPOS_DURATION;
			}
			break;
		case NC_ANCINFO:
			{
				anchor_pos *ancpos = NULL;
				u8 i = 0, j = 0;
				u8 result = 0;
				u16 length = (recmsg->length[0] << 8) | recmsg->length[1];
				if(length != (recmsg->data[0] * 18 + 1)) {
					localNet->freeBuf(recbuf);
					return ERR_LENGTH;
				}
				clearAncPosList();
				for(i = 0; i < recmsg->data[0]; i++) {
					ancpos = getFreeNextAncPosPtr();
					if(ancpos == NULL) {
						break;
					}
					for(j = 0; j < ANCHOR_ID_SIZE; j++) {
						ancpos->id[j] = recmsg->data[i * 18 + 1 + j ];
					}
					ByteOrder_Inverse_Int(&recmsg->data[i * 18 + 1 + ANCHOR_ID_SIZE], (u8 *)&ancpos->pos[0], 3);
				}
				
				if(i == recmsg->data[0]) {
					result = 1;
				}
				netpro_send(localNet, NC_ANCINFO, &result, 1);
				downloadAncListReg();
				updateVersionOfAncs(0);
				ancPosListPrint(0);				
			}
			localNet->nextState = NC_TAGINFO;
			localNet->sendDuraTime = SENDPOS_DURATION;
			break;
		case NC_TAGINFO:
			localNet->nextState = NC_TAGINFO;
			localNet->sendDuraTime = SENDPOS_DURATION;
			//ignore
			break;
		default:
			break;
	}
	//printf("receive data type %d \n", recmsg->ctrl);
	localNet->freeBuf(recbuf);
	return 0;
}

/*ByteOrder_Inverse:
 *	big order trans to small order
 *	small order trans to big order*/
void ByteOrder_Inverse_Int(u8 *src, u8 *dest, u16 count) {
	u8 i, j;
	if(src == NULL)	return;
	if(dest == NULL) {
		u8 temp = 0;
		for(i = 0; i < count; i++) {
			for(j = 0; j < 2; j++) {
				temp = src[4 * i + j];
				src[4 * i + j] = src[4 * i + 4 - 1 - j];
				src[4 * i + 4 - 1 - j] = temp;
			}
		}
	} else {
		for(i = 0; i < count; i++) {
			for(j = 0; j < 4; j++) {
				dest[4 * i + j] = src[4 * i + 4 - 1 - j];
			}
		}
	}
}

void netpro_run2(LocalNetS *localNet) {
	char state = 0;
	//static uint8_t testSendNum = 5;
	state = netpro_parse(localNet);
	err_print(state);
	
	if(SysTick_TimeDelta(localNet->sendNetdataTime) >= localNet->sendDuraTime) {
		switch(localNet->nextState) {
			case NC_POWERLINK:
			case NC_SYNCLOCK:
				netpro_sendreq(localNet, localNet->nextState);
				printf("[%d]sending data type %d\n", localNet->fd, localNet->nextState);
				localNet->sendDuraTime = RESEND_DURATION;
				break;
			case NC_DANGERAREA:
			case NC_ANCINFO:
				localNet->nextState = NC_TAGINFO;
				break;
			case NC_TAGINFO:
				if(posNetBuf.data[TAG_NUM_OFFSET]) {
					// offset:	0					6				7				13					25				26
					//net pro: timestamp(6 bytes), tagNum(1 byte), tagID(6 bytes), position(12 bytes), warn(1 byte), flag(1 byte)
					//warn(1 byte):bit[1-0]指明告警状态0不报警，bit[0]高压报警,bit[1]区域告警
					//			   bit[7-4]指明定位状态,bit[7]0无效，1只收到一个基站，2为2个基站，3为三个基站，4个...
					SysTick_GetClock(posNetBuf.data, TIMESTAMP_LENGTH);
					netpro_send(localNet, NC_TAGINFO, posNetBuf.data, posNetBuf.length);
					printf("[%d]sending data type %d\n", localNet->fd, localNet->nextState);
					clearPosNetBuf();
				}
				localNet->sendDuraTime = SENDPOS_DURATION;				
				break;
			case NC_WAITING_CONNECT:
				break;
			default:
				localNet->nextState = NC_POWERLINK;
				break;
		}
		localNet->sendNetdataTime = portGetTickCnt();
	} else if(localNet->nextState != NC_POWERLINK) {
		if(SysTick_TimeDelta(localNet->sendHeartPacketTime) >= localNet->sendHeartPacketDuraTime) {
			localNet->sendHeartPacketTime = portGetTickCnt();
			SysTick_GetClock(posNetBuf.data, TIMESTAMP_LENGTH);
			netpro_send(localNet, NC_HEART_PACKET, posNetBuf.data, TIMESTAMP_LENGTH);
		}
	}
}

void netpro_run(void) {
	u8 i = 0;
	for(i = 0; i < NET_NUM; ++i) {
		if((localNetList[i].fd != FD_CLOSE) && (localNetList[i].checkState())) {
			netpro_run2( (localNetList + i) );
			
		}
	}
}

u8 isGateway(void) {
	u8 i = 0;
	for(i = 0; i < NET_NUM; ++i) {
		//只有具备以太网模块的基站才被认为是网关
		if((localNetList[i].fd == UART2_FD) && (localNetList[i].checkState())) {
			return 1;
		}
	}
	
	return 0;
}


