#include "deca_device_api.h"
#include "deca_spi.h"
#include "deca_regs.h"

#include "instance.h"

// ----------------------------------------------------------
//      Data Definitions
// ----------------------------------------------------------
#define ONE_POS_INFO_SIZE		(18)
#define POS_UWB_BUF_MAX_SIZE	(4 * ONE_POS_INFO_SIZE)
typedef struct {
	u16 length;
	u8 num;
	//addr(2 bytes), xyz(12 bytes), alarm(1 byte), seqNum(1 byte), elecValue(2 bytes) = 18 bytes
	u8 data[POS_UWB_BUF_MAX_SIZE];
}PosUWBBufStr;

PosUWBBufStr posUWBBuf = {0, 0};

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// NOTE: the maximum RX timeout is ~ 65ms
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// ----------------------------------------------------------
// Functions
// ----------------------------------------------------------
void addTagPos_ToBuf(uint16 addr, uint8* pos, u8 alarm, u8 seqNum, uint16 elecValue) {
	uint16 offset = posUWBBuf.length;
	if((offset + ONE_POS_INFO_SIZE) > POS_UWB_BUF_MAX_SIZE)	return;	//缓冲装不下，忽略
	posUWBBuf.data[offset + 0] = addr & 0xff;
	posUWBBuf.data[offset + 1] = (addr >> 8) & 0xff;
	memcpy(&posUWBBuf.data[offset + 2], pos, 12);
	posUWBBuf.data[offset + 14] = alarm;
	posUWBBuf.data[offset + 15] = seqNum;
	posUWBBuf.data[offset + 16] = elecValue & 0xff;
	posUWBBuf.data[offset + 17] = (elecValue >> 8) & 0xff;
	posUWBBuf.length = offset + 18;
}

//返回剩余多少个标签数据没有拷贝, 是否考虑覆盖缓冲中已存在的标签信息？？？？
uint8 cpy_to_PosBuf(uint8 num, uint8 *src) {
	int8 leftNum = 4 - posUWBBuf.num;
	if(leftNum <= 0) {
		leftNum = 0;
	}
	if(num < leftNum) {
		leftNum = num;
	}
	if(leftNum > 0) {
		memcpy(&(posUWBBuf.data[posUWBBuf.length]), src, ONE_POS_INFO_SIZE * leftNum);
	}
	return (4 - leftNum);
}

void clearTagPosBuf(void) {
	posUWBBuf.length = 0;
	posUWBBuf.num = 0;
}

/*****************************************************
 * this function re-enables RX ...
 ****************************************************/
void anch_no_timeout_rx_reenable(void) {
	dwt_setrxtimeout(0); //reconfigure the timeout
	dwt_rxenable(DWT_START_RX_IMMEDIATE) ;
}


void prepare_beacon_frame(instance_data_t* inst, uint8_t flag) {
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.destAddr[0] = 0xff;	//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = 0xff;	//广播方式
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_BCN;
	inst->msg_f.messageData[SIDOF] = inst->bcnmag.sessionID;
	inst->msg_f.messageData[FLGOF] = flag;
	inst->msg_f.messageData[CSNOF] = inst->bcnmag.clusterSlotNum;
	inst->msg_f.messageData[CFNOF] = inst->bcnmag.clusterFrameNum;
	inst->msg_f.messageData[CMPOF] = inst->bcnmag.clusterSelfMap & 0xff;
	inst->msg_f.messageData[CMPOF+1] = (inst->bcnmag.clusterSelfMap >> 8) & 0xff;
	inst->msg_f.messageData[NMPOF] = inst->bcnmag.clusterNeigMap & 0xff;
	inst->msg_f.messageData[NMPOF+1] = (inst->bcnmag.clusterNeigMap >> 8) & 0xff;
	
	inst->bcnmag.slotSelfMap = inst->bcnmag.nextSlotMap;	//更新slot位图
	inst->bcnmag.nextSlotMap = 0;
	
	inst->msg_f.messageData[SMPOF] = inst->bcnmag.slotSelfMap & 0xff;
	inst->msg_f.messageData[SMPOF+1] = (inst->bcnmag.slotSelfMap >> 8) & 0xff;
	
	inst->msg_f.messageData[VEROF] = (getVersionOfArea() << 4) | getVersionOfAncs();//版本号
	
	inst->psduLength = 0;
	if(flag & BCN_EXT) {
		inst->msg_f.messageData[VEROF+1] = inst->jcofmsg.msgID;
		inst->msg_f.messageData[VEROF+2] = inst->jcofmsg.chldAddr & 0xff;
		inst->msg_f.messageData[VEROF+3] = (inst->jcofmsg.chldAddr>>8) & 0xff;
		inst->msg_f.messageData[VEROF+4] = --inst->jcofmsg.clusterLock;
		inst->msg_f.messageData[VEROF+5] = inst->jcofmsg.clusterSeat;
		inst->psduLength = 5;
		
		if(inst->jcofmsg.clusterLock == 0) {
			inst->bcnmag.bcnFlag = 0;
		}
	} else if(posUWBBuf.num > 0){
		if((posUWBBuf.num * ONE_POS_INFO_SIZE) == posUWBBuf.length) {
			inst->msg_f.messageData[FLGOF] |= BCN_EXT;
			inst->msg_f.messageData[VEROF+1] = UWBMAC_FRM_TYPE_TAGS_POS;
			inst->msg_f.messageData[VEROF+2] = inst->fatherAddr & 0xff;
			inst->msg_f.messageData[VEROF+3] = (inst->fatherAddr>>8) & 0xff;
			inst->msg_f.messageData[VEROF+4] = posUWBBuf.num;
			memcpy(&(inst->msg_f.messageData[VEROF+5]), posUWBBuf.data, posUWBBuf.length);
			inst->psduLength = posUWBBuf.length + 4;
		}
		clearTagPosBuf();
	}
	inst->psduLength += (BCN_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
}

//返回0成功，1则无可用位置seat，需要重新对网路状态采样或则调整网络连接
uint8 prepare_join_request_frame(instance_data_t* inst) {
	uint8_t i = 0;
	uint16_t mask = 0x01;
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.destAddr[0] = 0xff;	//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = 0xff;
	
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_CL_JOIN;
	
	for(i = 0; i < inst->BCNslots_num; i++) {
		if((inst->bcnmag.clusterNeigMap & mask) == 0x00) {
			inst->bcnmag.clusterFrameNum = i;
			break;
		}
		mask <<= 1;
	}
	if(i == inst->BCNslots_num)	return 1;
	inst->msg_f.messageData[REQOF] = inst->bcnmag.clusterFrameNum;
	
	inst->psduLength = (JOIN_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
	return 0;
}

void BCNLog_Clear(instance_data_t* inst) {
	memset((void *)&inst->bcnlog, 0, sizeof(beacon_msg_log) * 16);
	return;
}

uint8 send_download_req_msg(instance_data_t* inst, uint8 isDelay) {
	char downloadType = getDownloadTaskType();
	if(downloadType <= 0) {
		dwt_setpreambledetecttimeout(0);
		dwt_setrxtimeout(0);
		inst->testAppState = TA_RXE_WAIT;
		inst->twrMode = BCN_LISTENER;	//listen bcn msg for collecting network info.
		//no download task
		return INST_NOT_DONE_YET;
		//inst->instToSleep = TRUE;
	}
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //inst->eui64[0]; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16>>8) & 0xff; 
	inst->msg_f.destAddr[0] = inst->fatherAddr & 0xff;			//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = (inst->fatherAddr >> 8) & 0xff;  
	inst->msg_f.seqNum = inst->frameSN++; //copy sequence number and then increment
	
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_DOWNLOAD_REQ; 	//message ID code
	inst->msg_f.messageData[MIDOF+1] = (u8)downloadType;			//download data type: 1--Anc Position; 2--Dangerous Area
	*((u16 *)&inst->msg_f.messageData[2]) = getDownloadCurLength();	//area data: index
	
	inst->psduLength = (4 + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
	dwt_writetxdata(inst->psduLength, (uint8 *)  &inst->msg_f, 0) ;	// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 0); 						//write frame control
	
	inst->wait4ack = DWT_RESPONSE_EXPECTED; //response is expected - automatically enable the receiver
	dwt_setrxtimeout(0); 
	dwt_setrxaftertxdelay(0);  //立即打开RX
	dwt_setpreambledetecttimeout(0);

	if(isDelay == DWT_START_TX_DELAYED) {
		dwt_setdelayedtrxtime(inst->svcStartStamp32h);
	}
	
	if(dwt_starttx(isDelay | inst->wait4ack) == DWT_ERROR) {
		error_printf("error:download req error!\n");
		return INST_NOT_DONE_YET;
	}
	
	inst->wait4type = UWBMAC_FRM_TYPE_DOWNLOAD_RESP;//等待周围节点的连接请求应答
	inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
	inst->previousState = TA_DOWNLOAD_REQ_WAIT_SEND;
	
	return INST_DONE_WAIT_FOR_NEXT_EVENT;
}

//将接收数据下入内存
uint8 anchor_process_download_data(instance_data_t* inst, event_data_t *dw_event, uint8 *srcAddr, uint8 *messageData) {
	u16 *offset = (u16 *)&messageData[2];
	u16 *filesize = (u16 *)&messageData[4];
	u16 *datalength = (u16 *)&messageData[6];
	u16 sum;
	u8 isEmp;
	sum = downloadWrite(messageData[1], &messageData[8], *datalength, *offset);
	if(updateDownload(messageData[1], sum, *filesize) == 3) {
		//下载完成
	}

	printf("Download:%d--(%d / %d)\n", messageData[1], sum, *filesize);

	isEmp = downloadTaskIsEmpty();
	if(sum >= *filesize && isEmp == 1) {
		ancPosListPrint(4);
		dangerAreaDataPrint_Test();
		
		inst->testAppState = TA_RXE_WAIT; 
	} else {
		send_download_req_msg(inst, 0);
	}
	return 0;
}

uint8 process_beacon_msg(instance_data_t* inst, event_data_t *dw_event, uint8 *srcAddr, uint8 *messageData) {
	uint16_t clustermap = messageData[CMPOF] + (messageData[CMPOF+1] << 8);
	uint16 addr = 0;
	u8 srcClusterFraNum = messageData[CFNOF];
	
	inst->testAppState = TA_RXE_WAIT;
	
	//判断是否带有拓展数据，并解析
	if(messageData[FLGOF] & BCN_EXT) {
		uint8 *dat = messageData + BCN_MSG_LEN;
		addr = dat[1] + (dat[2] << 8);
		//请求加入网络的确认应答
		if((inst->wait4type == dat[0]) && (dat[0] == UWBMAC_FRM_TYPE_CL_JOIN_CFM)) {
			//收集到对应的周围节点的应答后即可加入网络，而不是...
			if(addr == inst->instanceAddress16 && inst->bcnmag.clusterFrameNum < 16) {
				debug_printf("Rec Join Cof: %x\n", addr);
				if(inst->bcnmag.clusterFrameNum == dat[4] && (inst->bcnlog[srcClusterFraNum].flag & 0x01)) {
					inst->bcnlog[srcClusterFraNum].flag &= ~(0x01);
					inst->wait4ackNum -= 1;
					if(inst->wait4ackNum == 0) {
						inst->joinedNet = 1;	//标识成功加入网络
						inst->wait4type = 0;
						inst->bcnmag.clusterSelfMap = 0x01 << inst->bcnmag.clusterFrameNum;
						inst->bcnmag.clusterNeigMap = 0;
						inst->bcnmag.slotSelfMap = 0;
						
						BCNLog_Clear(inst);
					}
				} else if(dat[4] == 0xff) {
					//没有空闲位置分配给该节点，无法加入网络
					inst->bcnmag.clusterFrameNum = 0xff;
					inst->fatherRef = 0xff;
					inst->wait4type = 0;
					
					BCNLog_Clear(inst);
				}
				return 0;
			}
		} else if((dat[0] == UWBMAC_FRM_TYPE_TAGS_POS) && (addr == inst->instanceAddress16)) {
			//处理拓展区域中的标签位置信息
			if(inst->leaderAnchor) {
				//添加到网络缓冲区
				cpy_from_UWBBuf_to_PosNetBuf(dat[4], &dat[5]);
			} else {
				//非网关，继续上传
				debug_printf("%d tag info overflow:%d\n", dat[4], cpy_to_PosBuf(dat[4], &dat[5]));
			}
		}
	}
	
	if(inst->joinedNet > 0) {
		//已经加入网络，收集周围节点信息
		inst->bcnmag.clusterNeigMap |= (0x01 << srcClusterFraNum);
		addr = srcAddr[0] + (srcAddr[1] << 8);
		inst->sampleNum += 1;	//累计采样次数
		if(addr == inst->fatherAddr && (!inst->leaderAnchor)) {
			uint8 version = messageData[VEROF] & 0x0f;
			//依据父节点时间同步本地时间
			int dif = inst->bcnmag.clusterFrameNum - srcClusterFraNum;
			inst->fatherRefTime = dw_event->timeStamp;
			inst->instanceTimerEn = 1;
			inst->beaconTXTimeCnt = dw_event->uTimeStamp + dif * inst->BCNslotDuration_ms * 100;
			inst->svcStartStamp32h = dw_event->timeStamp32h + (inst->BCNslots_num - srcClusterFraNum) * inst->BCNfixTime32h;
			if(dif <= 0) {
				inst->beaconTXTimeCnt += inst->sfPeriod_ms * 100; 
			}
			if((version != 0) && (version !=  getVersionOfAncs())) {
				requestAncList();	//请求下载基站坐标数据
			}
			version = (messageData[VEROF] >> 4) & 0x0f;
			if((version != 0) && (version !=  getVersionOfAncs())) {
				requestAreaData();	//请求下载危险区域数据
			}
		}
		//达到一定采样次数后，重新设定节点的网络状态，以回收未被使用的资源
		if(inst->sampleNum >= 50) {
			inst->bcnmag.clusterSelfMap = 0;
			inst->bcnmag.clusterSelfMap |= (0x01 << inst->bcnmag.clusterFrameNum);
			inst->bcnmag.clusterNeigMap = 0;
			inst->sampleNum = 0;
		}
		
		//更新日志
		if(srcClusterFraNum < 16) {
			inst->bcnlog[srcClusterFraNum].srcAddr = srcAddr[0] + (srcAddr[1] << 8);
			inst->bcnlog[srcClusterFraNum].clusMap = clustermap;
			inst->bcnlog[srcClusterFraNum].neigMap = messageData[NMPOF] + (messageData[NMPOF+1] << 8);
		}
		return 0;
	}
	
	/******************************************************************************/
	//以下部分是寻找参考节点，并统计网络状态采样次数, 为加入网络找准时机
	if(srcClusterFraNum < inst->fatherRef) {
		inst->fatherRef = srcClusterFraNum;
		inst->fatherAddr = srcAddr[0] + (srcAddr[1] << 8);
		inst->sampleNum = 0;
		inst->bcnmag.clusterNeigMap = 0;
		inst->bcnmag.sessionID = messageData[SIDOF];
		inst->wait4ackNum = 0;
	} else if(srcClusterFraNum == inst->fatherRef) {
		inst->sampleNum += 1;
	} 
		
	inst->bcnmag.clusterNeigMap |= clustermap;
	//更新日志
	if(srcClusterFraNum < 16) {
		inst->bcnlog[srcClusterFraNum].srcAddr = srcAddr[0] + (srcAddr[1] << 8);
		inst->bcnlog[srcClusterFraNum].clusMap = clustermap;
		inst->bcnlog[srcClusterFraNum].neigMap = messageData[NMPOF] + (messageData[NMPOF+1] << 8);
		if((inst->bcnlog[srcClusterFraNum].flag & 0x01) == 0) {
			//计算需要收集周围多少个节点的应答个数
			inst->bcnlog[srcClusterFraNum].flag |= 0x01;
			inst->wait4ackNum += 1;
		}
	}
	
	//printf("NeiMap=%x, times=%d\n", inst->bcnmag.clusterNeigMap, inst->sampleNum);
	
	if(inst->sampleNum > 5 && inst->joinedNet == 0) {
		//请求加入网络
		inst->sampleNum = 0;
		if(prepare_join_request_frame(inst)) {
			//failed, clear data
			inst->fatherRef = 0xff;
			inst->testAppState = TA_RXE_WAIT;
			return 0;
		}
		
		inst->svcStartStamp32h = dw_event->timeStamp32h + inst->BCNfixTime32h * (inst->BCNslots_num - messageData[CFNOF]);
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 0); //write frame control
		//在SVC阶段RX on以及time out，units are 1.0256us
		dwt_setrxaftertxdelay(inst->BCNslotDuration_ms * 500);
		dwt_setrxtimeout(0);  //disable rx timeout
		//dwt_setpreambledetecttimeout(PTO_PACS); //configure preamble timeout
		inst->wait4ack = 1;
		dwt_setdelayedtrxtime(inst->svcStartStamp32h);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			error_printf("error: join net request error!\n");
			return 1;
		}
		info_printf("join net requesting\n");
		
		inst->wait4type = UWBMAC_FRM_TYPE_CL_JOIN_CFM;//等待周围节点的连接请求应答
		
		inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
		inst->previousState = TA_TXSVC_WAIT_SEND;
	}
	return 0;
}

uint8 process_join_request_msg(instance_data_t* inst, event_data_t *dw_event, uint8 *srcAddr, uint8 *messageData) {
	uint16 srcAddr16 = srcAddr[0] + (srcAddr[1] << 8);
	uint8 seat = messageData[REQOF];
	uint16 mask = 0x01 << seat;
	
	inst->testAppState = TA_RXE_WAIT;
	
	info_printf("[%x] Req Join!\n", srcAddr16);
	//如果当前额外数据没有被占用，而且源节点请求的位置没有被占用，则为该源节点准备应答数据
	if((inst->joinedNet > 0) && (inst->jcofmsg.clusterLock == 0) && ((inst->bcnmag.clusterSelfMap & mask) == 0)) {
		inst->bcnmag.bcnFlag |= BCN_EXT;
		inst->jcofmsg.chldAddr = srcAddr16;
		inst->jcofmsg.clusterLock = 5;
		inst->jcofmsg.clusterSeat = seat;
		inst->jcofmsg.msgID = UWBMAC_FRM_TYPE_CL_JOIN_CFM;
		 
		//inst->bcnmag.clusterSelfMap |= mask; //不能在该处设置，因为请求源节点未必能成功加入网络
	}
	return 0;
}

void send_beacon_msg(instance_data_t* inst, uint8 flag) {
//	uint32 dlyTime = 0;
	if(inst->leaderAnchor) {
		prepare_beacon_frame(inst, BCN_INIT | flag);
	} else {
		prepare_beacon_frame(inst, flag);
	}
	//数据之前已经准备好了
	dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 0); //write frame control
	//在SVC阶段RX on以及time out，units are 1.0256us
//	dlyTime = inst->BCNfixTimeDly_sy * (inst->bcnmag.clusterSlotNum - inst->bcnmag.clusterFrameNum);
	dwt_setrxaftertxdelay(inst->BCNslotDuration_ms * 500);
	dwt_setrxtimeout(0);  //disable rx timeout
	//dwt_setpreambledetecttimeout(PTO_PACS); //configure preamble timeout
	
	dwt_starttx(DWT_START_TX_IMMEDIATE); //transmit the frame
	
	if(inst->leaderAnchor) {
		inst->instanceTimerEn = 1;
		inst->beaconTXTimeCnt = portGetTickCnt_10us() + 100 * inst->sfPeriod_ms;
	}
	
	inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
	inst->previousState = TA_TXBCN_WAIT_SEND;
	
	//info_printf("Info:sBeacon sT:%u, cT:%d\n", (u32)dwt_readsystimestamphi32(), portGetTickCnt_10us());
}

//返回1：标识本基站不处理该group poll msg，将继续接收信息
//返回0：将发送poll msg
uint8 process_grp_poll_msg(uint8 srcAddr_index, uint8 msgid_index, uint8* frame, event_data_t *event) {
	uint16 *data = (uint16 *)&frame[msgid_index + GP_FLGOF];
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint16 mask = 0x01 << (inst->bcnmag.clusterFrameNum);
	//判断对应cluster是否置位 
	if((data[0] & mask) == mask) {
		uint32 timeStamp32 = 0;
//		uint16 srcAddr = frame[srcAddr_index] + (frame[srcAddr_index + 1] << 8);
		int rxOnTime = 0;
		//查询地址表是否包含本基站地址
		for(inst->idxOfAnc = 0; inst->idxOfAnc < 4; ++inst->idxOfAnc) {
			if(data[2+inst->idxOfAnc] == inst->instanceAddress16)	break;
		}
		if(inst->idxOfAnc >= 4)	return 1;
		
		/********************start构建poll msg帧*************************/
		inst->msg_f.seqNum = inst->frameSN++;
		inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff;
		inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
		memcpy(&inst->msg_f.destAddr[0], &frame[srcAddr_index], ADDR_BYTE_SIZE_S);
		inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_TWR_POLL;
		inst->msg_f.messageData[PL_FLGOF] =	0;	//暂时设置为无错误
		inst->msg_f.messageData[PL_SMPOF + 0] = inst->bcnmag.nextSlotMap & 0xff;
		inst->msg_f.messageData[PL_SMPOF + 1] = (inst->bcnmag.nextSlotMap >> 8) & 0xff;
		
		inst->psduLength = FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC + TWR_POLL_MSG_LEN;
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 1); //write frame control
		/********************end:构建poll msg帧*************************/
		
		/********************start:设置poll发送时间*********************/
		inst->delayedTRXTime32h = event->timeStamp32h;
		rxOnTime = ((3 - inst->idxOfAnc) * inst->fixedPollDelayAnc32h) / 256 - inst->fwto4PollFrame_sy;
		if(rxOnTime < 0) {
			rxOnTime = 0;
		}
		dwt_setrxaftertxdelay((uint32)rxOnTime);//(((3 - inst->idxOfAnc) * inst->fixedPollDelayAnc32h) / 256 - 400);	//转为为1.0256us
		dwt_setrxtimeout(0);//dwt_setrxtimeout((uint16)inst->fwto4RespFrame_sy);
		timeStamp32 = inst->delayedTRXTime32h + inst->fixedOffsetDelay32h + (inst->idxOfAnc + 1) * inst->fixedPollDelayAnc32h;
		dwt_setdelayedtrxtime(timeStamp32);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			uint32 curStamp32 = dwt_readsystimestamphi32();
			error_printf("error:time is pass: %lu--%lu\n", timeStamp32, curStamp32);
//			curStamp32 = dwt_readsystimestamphi32();
//			if((timeStamp32 - curStamp32) > 25000) {	//100us
//				while((timeStamp32 - curStamp32) > 25000) {
//					curStamp32 = dwt_readsystimestamphi32();
//					delay_us(100);
//				}
//			} else if((curStamp32 - timeStamp32) > 100000) {//0.4ms
//				return 1;
//			}
//			dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
		}
		inst->twrmag.pollTxTime[0] = timeStamp32;
		inst->twrmag.pollTxTime[0] = (inst->twrmag.pollTxTime[0] << 8) + inst->txAntennaDelay;
		inst->twrmag.gpollRxTime[0] = event->timeStamp;
		/********************end:设置poll发送时间*********************/
		
		inst->wait4ack = 1;
		inst->wait4type = UWBMAC_FRM_TYPE_TWR_RESP;
		inst->wait4ackOfAddr = frame[srcAddr_index] + (frame[srcAddr_index] << 8);
		
		//info_printf("Poll-T:%lu, S:%lu\n", timeStamp32, dwt_readsystimestamphi32());
		//printf("rec grpPoll, sending poll\n");
		return 0;
	} else {
		inst->wait4ackOfAddr = 0xffff;
	}
	return 1;
}

//返回1：标识本基站不处理该msg，将继续接收信息
//返回0：将发送poll msg
uint8 process_response_msg(uint8 srcAddr_index, uint8 msgid_index, uint8* frame, event_data_t *event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint16 mask = 0x01 << frame[msgid_index + RP_DSLOF];
	uint16 srcAddr = frame[srcAddr_index] + (frame[srcAddr_index] << 8);
	
	if((inst->wait4type == UWBMAC_FRM_TYPE_TWR_RESP) && (inst->wait4ackOfAddr == srcAddr)) {
		uint8 idxOfAnc = 3 - inst->idxOfAnc;
		uint32 timeStamp32 = event->timeStamp32h + (idxOfAnc + 1) * inst->fixedFinalDelayAnc32h;
		uint64 timeStamp = timeStamp32;	
		int rxOnTime = 0;
		timeStamp = (timeStamp << 8) + inst->txAntennaDelay; 
		/********************start:构建final msg帧*************************/
		inst->msg_f.seqNum = inst->frameSN++;
		inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff;
		inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
		memcpy(&inst->msg_f.destAddr[0], &frame[srcAddr_index], ADDR_BYTE_SIZE_S);
		inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_TWR_FINAL;
		inst->msg_f.messageData[FN_FLGOF] = ((mask & inst->bcnmag.nextSlotMap) == 0) ? 1 : 0;
		inst->bcnmag.nextSlotMap |= mask;
		//Tx time of Poll message-- or ------Db(plllTxTime - gpollRxTime)
		inst->twrmag.pollTxTime[1] = inst->twrmag.pollTxTime[0] - inst->twrmag.gpollRxTime[0];
		memcpy(&(inst->msg_f.messageData[FN_TPTOF]), (uint8 *)&inst->twrmag.pollTxTime[1], 5);
		//Rx time of response message or ----Rb(respRxTime - plllTxTime)
		inst->twrmag.respRxTime[1] = event->timeStamp - inst->twrmag.pollTxTime[0];
		memcpy(&(inst->msg_f.messageData[FN_RRTOF]), (uint8 *)&inst->twrmag.respRxTime[1], 5);
		//Tx time of final message or -------Db1(finalTxTime - respRxTime)
		inst->twrmag.finalTxTime[1] = timeStamp - event->timeStamp;
		memcpy(&(inst->msg_f.messageData[FN_TFTOF]), (uint8 *)&inst->twrmag.finalTxTime[1], 5);
		
		inst->psduLength = FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC + TWR_FINAL_MSG_LEN;
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 1); //write frame control
		/********************end:构建final msg帧*************************/
		
		/********************start:设置时间***************************************/
		inst->delayedTRXTime32h = event->timeStamp32h;
		rxOnTime = (((3 - idxOfAnc) * inst->fixedFinalDelayAnc32h) / 256) - inst->fwto4FinalFrame_sy;
		if(rxOnTime < 0) {
			rxOnTime = 0;
		}
		dwt_setrxaftertxdelay((uint32)rxOnTime);//转化为1.0256us
		dwt_setrxtimeout(0);
		dwt_setdelayedtrxtime(timeStamp32);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			uint32 curStamp32 = dwt_readsystimestamphi32();
			error_printf("error:time is pass: %lu--%lu\n", timeStamp32, curStamp32);
		}
		//inst->twrmag.respRxTime[0] = event->timeStamp;
		//inst->twrmag.finalTxTime[0] = (timeStamp32 << 8) + 0x4000;	//64 ns
		/**********************end:设置时间***************************************/
		
		inst->wait4type = 0;	//不等待指定类型信息帧
		inst->wait4ackOfAddr = 0xffff;
		
		//info_printf("Tx:%u, sT:%u\n", (u32)timeStamp32, (u32)dwt_readsystimestamphi32());
		//info_printf("rec resp[%d], sending fin[%x]\n", frame[msgid_index + RP_DSLOF], inst->bcnmag.slotSelfMap);
		return 0;
	}
	return 1;
}

uint8 process_download_req(uint8 srcAddr_index, uint8 msgid_index, uint8* frame) {
	uint16 frameLength = 0;
	uint16 readlength = 0;
	u16 *offset = NULL;
	u16 *ptr = NULL;
	instance_data_t* inst = instance_get_local_structure_ptr(0);

	inst->psduLength = frameLength = 100 + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC;
	memcpy(&inst->msg_f.destAddr[0], &frame[srcAddr_index], ADDR_BYTE_SIZE_S); //remember who to send the reply to (set destination address)
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff;
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.seqNum = inst->frameSN++;
	
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_DOWNLOAD_RESP; //message ID code
	//write download data
	inst->msg_f.messageData[MIDOF+1] = frame[msgid_index + 1];
	offset = (u16 *)&frame[msgid_index + 2];	//data offset which want to read;
	readlength = downloadRead(frame[msgid_index + 1], &inst->msg_f.messageData[8], 100-8, *offset);	//read data
	ptr = (u16 *)&inst->msg_f.messageData[6];
	*(ptr--) = readlength;									//current block size
	*ptr = downloadGetSize(frame[msgid_index + 1]);			//data size
	inst->msg_f.messageData[MIDOF+2] = frame[msgid_index + 2];
	inst->msg_f.messageData[MIDOF+3] = frame[msgid_index + 3];
	info_printf("download (%d / %d)\n", *offset, *ptr);
	
	//write the TX data
	dwt_writetxfctrl(frameLength, 0, 0);	//1
	dwt_writetxdata(frameLength, (uint8 *)  &inst->msg_f, 0) ;	// write the frame data
	dwt_setrxaftertxdelay(0);
	if(dwt_starttx(DWT_START_TX_IMMEDIATE) == DWT_ERROR) {
		error_printf("error: download error\n");
		return 1;
	}
	
	return 0;
}

void anchor_init(instance_data_t* inst) {
	memcpy(inst->eui64, &inst->instanceAddress16, ADDR_BYTE_SIZE_S);
	dwt_seteui(inst->eui64);
	dwt_setpanid(inst->panID);

	//set source address
	dwt_setaddress16(inst->instanceAddress16);
	//根据地址低8位在数组中寻找对应项，后期应更改结构
	inst->shortAdd_idx = (inst->instanceAddress16 & 0xff);
	//将地址为0x0000的基站暂定为网关，也就是协调器
	if(inst->instanceAddress16 == 0x0000) {
		inst->leaderAnchor = TRUE;
	}
	
	dwt_enableframefilter(DWT_FF_NOTYPE_EN);	//disable filter
	// First time anchor listens we don't do a delayed RX
	dwt_setrxaftertxdelay(0);
	dwt_setrxtimeout(0);
	
	instance_config_frameheader_16bit(inst);
	
	if(inst->leaderAnchor) {
		inst->testAppState = TA_TXBCN_WAIT_SEND;
		inst->joinedNet = 1;		
		inst->bcnmag.sessionID = 0x55;//后面改用随机器产生
		inst->fatherAddr = inst->instanceAddress16;
		inst->bcnmag.clusterSlotNum = 15;
		inst->bcnmag.clusterFrameNum = 0;
		inst->bcnmag.clusterSelfMap = 0x0001;
		//协调器等到superFrame的开始点,这里最坏情况也就延时一个超帧时间，不会有太大影响，后面如果有更好的方法可以更换；
		while(portGetTickCnt_10us() % (inst->sfPeriod_ms * 100) != 0);
	} else {
		dwt_setrxaftertxdelay(0);
		dwt_setrxtimeout(0);  //disable rx timeout
		inst->wait4ack = 0;
		inst->sampleNum = 0;
		inst->fatherRef = 0;
		inst->joinedNet = 0;
		inst->bcnmag.clusterFrameNum = 0;
		inst->bcnmag.clusterSelfMap = 0x0000;
		//change to next state - wait to receive a message
		inst->testAppState = TA_RXE_WAIT ;
	}
	inst->bcnmag.clusterNeigMap = 0x0000;
	inst->bcnmag.slotSelfMap = 0x0000;	
	inst->bcnmag.nextSlotMap = 0x0000;
	inst->bcnmag.bcnFlag = 0;
	BCNLog_Clear(inst);
}


/****************************************************************************************************
 * this function handles frame error event, it will either signal timeout or re-enable the receiver
 ***************************************************************************************************/
void anch_handle_error_unknownframe_timeout(event_data_t *dw_event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	dw_event->type = 0;
	dw_event->rxLength = 0;
	dwt_rxenable(DWT_START_RX_IMMEDIATE);
}

/************************************************************
 * @brief this is the receive timeout event callback handler
 ************************************************************/
void rx_to_cb_anch(const dwt_cb_data_t *rxd) {
	event_data_t dw_event;
	//microcontroller time at which we received the frame
    dw_event.uTimeStamp = portGetTickCnt_10us();
   	anch_handle_error_unknownframe_timeout(&dw_event);
}

/***********************************************************
 * @brief this is the receive error event callback handler
 **********************************************************/
void rx_err_cb_anch(const dwt_cb_data_t *rxd) {
	event_data_t dw_event;
	//microcontroller time at which we received the frame
    dw_event.uTimeStamp = portGetTickCnt_10us();
    anch_handle_error_unknownframe_timeout(&dw_event);
}

/*****************************************************************************************************
 * this is the receive event callback handler, the received event is processed and the instance either
 * responds by sending a response frame or re-enables the receiver to await the next frame
 * once the immediate action is taken care of the event is queued up for application to process
 ****************************************************************************************************/
void rx_ok_cb_anch(const dwt_cb_data_t *rxd) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint8 rxTimeStamp[5]  = {0, 0, 0, 0, 0};
    uint8 rxd_event = 0, is_knownframe = 0;
	uint8 msgid_index  = 0, srcAddr_index = 0;
	event_data_t dw_event;
	
	//printf("rece data\n");
	//microcontroller time at which we received the frame
	dw_event.uTimeStamp = portGetTickCnt_10us();
	dw_event.rxLength = rxd->datalength;
	//need to process the frame control bytes to figure out what type of frame we have received
	if((rxd->fctrl[0] == 0x41) && ((rxd->fctrl[1] & 0xcc) == 0x88)) {
		msgid_index = FRAME_CRTL_AND_ADDRESS_S;
		srcAddr_index = FRAME_CTRLP + ADDR_BYTE_SIZE_S;
		rxd_event = DWT_SIG_RX_OKAY;
	} else {
		rxd_event = SIG_RX_UNKNOWN;	//not supported
	}
	
	//read RX timestamp
	dwt_readrxtimestamp(rxTimeStamp);
	//dwt_readsystime(rxTimeStamp);
	//read data frame
	dwt_readrxdata((uint8 *)&dw_event.msgu.frame[0], rxd->datalength, 0);
	instance_seteventtime(&dw_event, rxTimeStamp);
	//type will be added as part of adding to event queue
	dw_event.type = 0;
	dw_event.typePend = DWT_SIG_DW_IDLE;
	
	//Process good/known frame types
	if(rxd_event == DWT_SIG_RX_OKAY) {
		//uint16 srcAddress = (((uint16)dw_event.msgu.frame[srcAddr_index+1]) << 8) + dw_event.msgu.frame[srcAddr_index];
		uint16 dstAddress = (((uint16)dw_event.msgu.frame[srcAddr_index-1]) << 8) + dw_event.msgu.frame[srcAddr_index-2];
		//debug_printf("rec [%x]\n", srcAddress);
		//PAN ID must match - else discard this frame
		if((dw_event.msgu.rxmsg_ss.panID[0] != (inst->panID & 0xff)) || (dw_event.msgu.rxmsg_ss.panID[1] != (inst->panID >> 8))) {
			dwt_rxenable(DWT_START_RX_IMMEDIATE);//anch_handle_error_unknownframe_timeout(&dw_event);
			return;
		}
		if(dstAddress != 0xffff && dstAddress != inst->instanceAddress16) {
			dwt_rxenable(DWT_START_RX_IMMEDIATE);//anch_handle_error_unknownframe_timeout(&dw_event);
			return;
		}
		//可以在此处针对某些类型帧进行处理，若不急着处理，然后将事件放入队列
		//检查是否是TWR信息
		switch(dw_event.msgu.frame[msgid_index]) {
			case UWBMAC_FRM_TYPE_TWR_GRP_POLL:
				if(inst->mode == ANCHOR && process_grp_poll_msg(srcAddr_index, msgid_index, &dw_event.msgu.frame[0], &dw_event) == 0) {
					//exit this interrupt and notify the application that TX is in progress.
					dw_event.typePend = DWT_SIG_TX_PENDING;
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_TWR_RESP:
				if(inst->mode == ANCHOR && process_response_msg(srcAddr_index, msgid_index, &dw_event.msgu.frame[0], &dw_event) == 0) {
					//exit this interrupt and notify the application that TX is in progress
					dw_event.typePend = DWT_SIG_TX_PENDING;
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_DOWNLOAD_REQ:
				if(process_download_req(srcAddr_index, msgid_index, &dw_event.msgu.frame[0]) == 0) {
					dw_event.typePend = DWT_SIG_TX_PENDING;
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_BCN:
			case UWBMAC_FRM_TYPE_SVC:
			case UWBMAC_FRM_TYPE_CL_JOIN:
			case UWBMAC_FRM_TYPE_CL_JOIN_CFM:
			case UWBMAC_FRM_TYPE_POS:
			case UWBMAC_FRM_TYPE_DOWNLOAD_RESP:
			case UWBMAC_FRM_TYPE_ALMA:
				is_knownframe = 1;
				break;
			case UWBMAC_FRM_TYPE_TWR_POLL:
			case UWBMAC_FRM_TYPE_TWR_FINAL:
			default:	//ignore unknown frame
			{
				anch_no_timeout_rx_reenable();
				return;
			}
		}
	}
	if(is_knownframe == 1) {
		instance_putevent(&dw_event, rxd_event);
	} else {
		debug_printf("rec unknown frame\n");
		//need to re-enable the rx (got unknown frame type)
		anch_handle_error_unknownframe_timeout(&dw_event);
	}
}
// -----------------------------------------------------------------------------------
// the main instance state machine for anchor application
// -----------------------------------------------------------------------------------
int anch_app_run(instance_data_t *inst) {
	int instDone = INST_NOT_DONE_YET;
	//get any of the received events from ISR
    int message = instance_peekevent();
	u8 ldebug = 0;
	
    switch (inst->testAppState)
    {
		case TA_INIT:
			if(ldebug)	debug_printf("TA_INIT\n");
			anchor_init(inst);
			break;
		case TA_TXBCN_WAIT_SEND:
			if(ldebug)	debug_printf("TA_TXBCN_WAIT_SEND\n");
			send_beacon_msg(inst, inst->bcnmag.bcnFlag);
			break;
		case TA_DOWNLOAD_REQ_WAIT_SEND:
			instDone = send_download_req_msg(inst, 0);
			break;
		case TA_TX_WAIT_CONF:
		{
			event_data_t* dw_event = instance_getevent(11); //get and clear this event
			//wait for TX done confirmation
			if(ldebug) debug_printf("TA_TX_WAIT_CONF\n");
			if(dw_event->type != DWT_SIG_TX_DONE) {
				instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
                break;
			}
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			inst->testAppState = TA_RXE_WAIT;
			if(inst->previousState == TA_TXBCN_WAIT_SEND) {

			} else if(inst->previousState == TA_TXPOLL_WAIT_SEND) {
				inst->twrmag.pollTxTime[0] = dw_event->timeStamp;
			} /*else if(inst->previousState == TA_TXSVC_WAIT_SEND) {
			
			} else if(inst->previousState == TA_TXFINAL_WAIT_SEND) {
			
			} */
			break;
		}	
		case TA_RXE_WAIT:
			if(ldebug == 1)	debug_printf("TA_RXE_WAIT\n");
			//if this is set the RX will turn on automatically after TX
			if(inst->wait4ack == 0) {
				if(dwt_read16bitoffsetreg(0x19, 1) != 0x0505) {
					//turn on RX
					dwt_rxenable(DWT_START_RX_IMMEDIATE);
				}
			} else {
				//clear the flag, the next time we want to turn the RX on it might not be auto
				inst->wait4ack = 0;
			}
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			inst->testAppState = TA_RX_WAIT_DATA;
			if(message == 0)	break;
		case TA_RX_WAIT_DATA:
			switch(message) {
				//if we have received a DWT_SIG_RX_OKAY event - this means that the message is IEEE data type
				//need to check frame control to know which addressing mode is used
				case DWT_SIG_RX_OKAY:
				{
					event_data_t *dw_event = instance_getevent(15);
					uint8 *srcAddr = &(dw_event->msgu.rxmsg_ss.sourceAddr[0]);
//					uint8 *dstAddr = &(dw_event->msgu.rxmsg_ss.destAddr[0]);
					int msgid = 0;
					uint16 srcAddress = 0;
					uint8 *messageData = NULL;
					if(ldebug == 1)	debug_printf("DWT_SIG_RX_OKAY\n");
//					memcpy(&srcAddr[0], &(dw_event->msgu.rxmsg_ss.sourceAddr[0]), ADDR_BYTE_SIZE_S);
//					memcpy(&dstAddr[0], &(dw_event->msgu.rxmsg_ss.destAddr[0]), ADDR_BYTE_SIZE_S);
					msgid = dw_event->msgu.rxmsg_ss.messageData[MIDOF];
					messageData = &(dw_event->msgu.rxmsg_ss.messageData[0]);
					srcAddress = srcAddr[0] + (srcAddr[1] << 8);
					switch(msgid) {
						case UWBMAC_FRM_TYPE_BCN:
							process_beacon_msg(inst, dw_event, srcAddr, messageData);
							break;
						case UWBMAC_FRM_TYPE_CL_JOIN:
							process_join_request_msg(inst, dw_event, srcAddr, messageData);
							break;
						case UWBMAC_FRM_TYPE_TWR_GRP_POLL:
							debug_printf("rec gpoll:%x\n", srcAddress);
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_TXPOLL_WAIT_SEND;
							}
							if(messageData[GP_QFAOF] & 0x80){
								//选择第一个基站传替数据
								//最短路径法？负载均衡法？选最短路径，需要beacon帧信息的支持，由tag选择最短路径节点
								uint16 firstAdd = messageData[GP_ADDOF] + (messageData[GP_ADDOF + 1] << 8);
								uint16 elecValue = messageData[GP_UPPOF] + (messageData[GP_UPPOF + 1] << 8);
								if(firstAdd == inst->instanceAddress16) {
									if(inst->leaderAnchor == 1) {
										//如果是网关，写入网络缓冲区，长度(1024-x), 最多可放50个标签信息
										//small order transform to big order
										ByteOrder_Inverse_Int(&messageData[GP_COROF], NULL, 3);
										addTagInfo(srcAddress, &messageData[GP_COROF], (messageData[GP_QFAOF] & 0x03), messageData[GP_SQNOF]);
									} else {
										//写入基站缓冲区,(127-x)长度，最多可放4个标签信息
										//UWB传输不需要大小端转换
										addTagPos_ToBuf(srcAddress, &messageData[GP_COROF], (messageData[GP_QFAOF] & 0x03), messageData[GP_SQNOF], elecValue);
									}
								}
							}
							break;
						case UWBMAC_FRM_TYPE_TWR_RESP:
							debug_printf("rec resp:%x\n", srcAddress);
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_TXFINAL_WAIT_SEND;
							}
							break;
						case UWBMAC_FRM_TYPE_DOWNLOAD_REQ:
							debug_printf("[%d]Download type: %d, %d\n", srcAddress, messageData[1],  *((uint16*)&messageData[2]));
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_DOWNLOAD_RESP_WAIT_SEND;
							}
							break;
						case UWBMAC_FRM_TYPE_DOWNLOAD_RESP:
							anchor_process_download_data(inst, dw_event, srcAddr, messageData);
							break;
						default:
							break;
					}
				}
					break;
				default:
					if(message) {
						instance_getevent(20);	//如果有未知事件，则将它清除
					}
					if(instDone == INST_NOT_DONE_YET) {
						instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
					}
					break;
			}
			break;	//end case TA_RX_WAIT_DATA
		default:
			break;
    } // end switch on testAppState

    return instDone;
} // end testapprun_anch()

// -------------------------------------------------------------------------
int anch_run(void) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
    int done = INST_NOT_DONE_YET;
	
	if(inst->instanceTimerEn) {
		if(portGetTickCnt_10us() >= inst->beaconTXTimeCnt) {
			if(downloadTaskIsEmpty() == 0) {
				inst->testAppState = TA_DOWNLOAD_REQ_WAIT_SEND;
			} else {
				inst->testAppState = TA_TXBCN_WAIT_SEND;
			}
			inst->beaconTXTimeCnt += inst->sfPeriod_ms * 100;
			/* Reset RX to properly reinitialise LDE operation. */
			//dwt_rxreset();
			dwt_forcetrxoff(); //disable DW1000
			instance_clearevents(); //clear any events
		}
	}

	while(done == INST_NOT_DONE_YET) {
		done = anch_app_run(inst); // run the communications application
	}
	return 0;
}

