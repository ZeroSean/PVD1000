#include "netproto.h"


#define NET_NUM		2	//UART2_FD and UART3_FD(internet and bluetooth)

/*temp static gateway id*/
u8 gw_id[GATEWAYID_LENGTH] = {0x41, 0x44, 0x30, 0x39, 0x30, 0x31};

typedef struct {
	u8 nextState;
	u8 fd;
	u32 sendDuraTime;
	u32 sendNetdataTime;
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

void addTagInfo(u16 addr, u8 *pos, u8 alarm, u8 seqNum) {
	u16 offset = posNetBuf.length;
	u8 i = 0;
	
	if((offset + 20) < MAX_DATALENGTH) {
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
		posNetBuf.data[offset+1] = seqNum;
		posNetBuf.length = offset + 2;
		posNetBuf.data[TAG_NUM_OFFSET] += 1;
	} else {
		//说明缓冲空闲空间不足，忽略该数据
	}
	debug_printf("add %x seq %d--%d\n", addr, seqNum, posNetBuf.data[TAG_NUM_OFFSET]);
	return;
}

void cpy_from_UWBBuf_to_PosNetBuf(uint8 num, uint8* dat) {
	uint8 i = 0;
	for(i = 0; i < num; ++i) {
		if((posNetBuf.length + 20) > MAX_DATALENGTH)	return;
		ByteOrder_Inverse_Int(&(dat[2]), NULL, 3);
		memcpy(&(posNetBuf.data[posNetBuf.length + 4]), dat, 16);
		posNetBuf.length += 20;
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

static char * read(u8 fd, char *rbuf, u16 *length) {
	if(fd == UART2_FD) {
		return DMA_UART_read(rbuf, length);
	} else if(fd == UART3_FD) {
		return DMA_UART3_read(rbuf, length);
	}
	return NULL;
}

/*0 for succ, 1 for send error, 2 for fd not supported*/
static char send(u8 fd, char *netmsg, u16 length) {
	if(fd == UART2_FD) {
		return DMA_UART_Send(netmsg, length);
	} else if(fd == UART3_FD) {
		return DMA_UART3_Send(netmsg, length);
	}
	return 2;
}

static void freeBuf(u8 fd, char *ptr) {
	if(fd == UART2_FD) {
		DMA_UART_freeRecBuf(ptr);
	} else if(fd == UART3_FD) {
		DMA_UART3_freeRecBuf(ptr);
	}
}

static uint16_t getSendCount(u8 fd) {
	if(fd == UART2_FD) {
		return DMA_UART_getSendCount();
	} else if(fd == UART3_FD) {
		return DMA_UART3_getSendCount();
	}
	return 0;
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
	
		localNetList[j].nextState = NC_POWERLINK;
		localNetList[j].sendDuraTime = RESEND_DURATION;
		localNetList[j].sendNetdataTime = portGetTickCnt();
	}
	
	/*can based on the hardware state or net state to close/open the fd*/
	localNetList[0].fd = UART2_FD;	
	localNetList[1].fd = UART3_FD;
	
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
			localNetList[i].sendDuraTime = RESEND_DURATION;
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
			error_printf("ERR_TMOUT\n");
			break;
		case ERR_UNDCODE:
			error_printf("ERR_UNDCODE\n");
			break;
		case ERR_BFOVER:
			error_printf("ERR_BFOVER\n");
			break;
		case ERR_NOSPACE:
			error_printf("ERR_NOSPACE\n");
			break;
		case ERR_RECLENGTH:
			error_printf("ERR_RECLENGTH\n");
			break;
		case ERR_FCODE:
			error_printf("ERR_FCODE\n");
			break;
		case ERR_CHECKCODE:
			error_printf("ERR_CHECKCODE\n");
			break;
		case ERR_PROCESS:
			error_printf("ERR_PROCESS\n");
			break;
		case ERR_LENGTH:
			error_printf("ERR_LENGTH\n");
			break;
		case ERR_TOOSHORT:
			error_printf("ERR_TOOSHORT\n");
			break;
		default:
			error_printf("UNDEFINED ERROR!\n");
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
	uint16_t freelength = getSendCount(localNet->fd);
	if(length > MAX_DATALENGTH) {
		//data too long, overflow the buffer 
		error_printf("ERR_BFOVER\n");
		return ERR_BFOVER;	
	}
	
//	if(length > (freelength - MIN_STATELENGTH)) {
//		//data send too quickly, old data no be send, without space to put new data
//		error_printf("ERR_NOSPACE\n");
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
	return send(localNet->fd, (char *)&(localNet->netmsg), length);
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
	//debug_printf("state: %d\n", state);
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
	u16 reclength = 0;
	u16 priindex = 0; //test
	if((recbuf = read(localNet->fd, 0, &reclength)) == NULL) {
		return 0;
	}
	debug_printf("rec data!\n");
	for(priindex = 0; priindex < reclength; priindex++) {
		debug_printf("%c", recbuf[priindex]);
	}
	debug_printf("\n\n");
	
	if(reclength < MIN_PROTOLENGTH) {
		error_printf("data size(%d) < MIN_PROTOLENGTH(%d)\n", reclength, MIN_PROTOLENGTH);
		freeBuf(localNet->fd, recbuf);
		return ERR_TOOSHORT;
	}
	recmsg = (NET_MSG_S *)recbuf;
	if(recmsg->fcode != START_CODE) {
		error_printf("Start code error(%d)\n", recmsg->fcode);
		freeBuf(localNet->fd, recbuf);
		return ERR_FCODE;
	}
	if(netpro_calCheckCode(&recmsg->ctrl, reclength - CHECKCODE_STARTIN - 2) != recbuf[reclength - 2]) {
		error_printf("check code: %d--%d\n", netpro_calCheckCode(&recmsg->ctrl, reclength - CHECKCODE_STARTIN - 2), recbuf[reclength - 2]);
		freeBuf(localNet->fd, recbuf);
		return ERR_CHECKCODE;
	}
	switch(recmsg->ctrl) {
		case NC_POWERLINK:
			localNet->nextState = NC_SYNCLOCK;
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
					freeBuf(localNet->fd, recbuf);
					result = 0;
					netpro_send(localNet, NC_DANGERAREA, &result, 1);
					return ERR_PROCESS;
				}
				for(i = 0; i < recmsg->data[0]; i++) {
					count = (recmsg->data[index + 6] << 8) | recmsg->data[index + 7];
					//printf("point count: %d-%d-%d\n", count, recmsg->data[index + 6], recmsg->data[index + 7]);
					ByteOrder_Inverse_Int(&recmsg->data[index + 8], NULL, count * 3);
					if(addAreaBlock(&recmsg->data[index], &recmsg->data[index + 8], count * 12) != 0) {
						freeBuf(localNet->fd, recbuf);
						result = 0;
						netpro_send(localNet, NC_DANGERAREA, &result, 1);
						error_printf("updata area error!\n");
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
					freeBuf(localNet->fd, recbuf);
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
	debug_printf("receive data type %d \n", recmsg->ctrl);
	freeBuf(localNet->fd, recbuf);
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
	
	if((localNet->sendNetdataTime + localNet->sendDuraTime) < portGetTickCnt()) {
		switch(localNet->nextState) {
			case NC_POWERLINK:
			case NC_SYNCLOCK:
				netpro_sendreq(localNet, localNet->nextState);
				debug_printf("sending data type %d\n", localNet->nextState);
				break;
			case NC_DANGERAREA:
			case NC_ANCINFO:
				localNet->nextState = NC_TAGINFO;
				break;
			case NC_TAGINFO:
				if(posNetBuf.data[TAG_NUM_OFFSET]) {
					// offset:	0					6				7				13					25				26
					//net pro: timestamp(6 bytes), tagNum(1 byte), tagID(6 bytes), position(12 bytes), warn(1 byte), flag(1 byte)
					SysTick_GetClock(posNetBuf.data, TIMESTAMP_LENGTH);
					netpro_send(localNet, NC_TAGINFO, posNetBuf.data, posNetBuf.length);
					debug_printf("sending data type %d\n", localNet->nextState);
					clearPosNetBuf();
				}
				localNet->sendDuraTime = SENDPOS_DURATION;				
				break;
			default:
				localNet->nextState = NC_TAGINFO;
				break;
		}
		localNet->sendNetdataTime = portGetTickCnt();
	}
}

void netpro_run(void) {
	u8 i = 0;
	for(i = 0; i < NET_NUM; ++i) {
		if(localNetList[i].fd != FD_CLOSE) {
			netpro_run2( (localNetList + i) );
		}
	}
}


