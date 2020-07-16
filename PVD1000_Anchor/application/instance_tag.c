#include "deca_device_api.h"
#include "deca_spi.h"
#include "deca_regs.h"

#include "instance.h"
#include "math.h"

// ----------------------------------------------------------
//      Data Definitions
// ----------------------------------------------------------

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// NOTE: the maximum RX timeout is ~ 65ms
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

/*---------------------function-----------------------------*/
uint8 send_response_msg(instance_data_t* inst);

uint8 findFirstZeroBit(uint16 bitMap, uint8 num) {
	uint8 i = 0;
	uint16 mask = 0x01;
	for(i = 0; i < num; ++i) {
		if((bitMap & mask) == 0x00) {
			return i;
		}
		mask <<= 1;
	}
	return i;
}

/**********************************************************************************************
 * this function either enables the receiver (delayed)
 *********************************************************************************************/
void tag_enable_rx(uint32 dlyTime, int fwto_sy) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	//subtract preamble duration (beacuse when instructing delayed TX the time is the time of SFD)
	//however when doing delayed RX the time is RX on time)
	dwt_setdelayedtrxtime(dlyTime - inst->preambleDuration32h);
	if(dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR)) {
		//if the delayed RX failed - time has passed - do immediate enable
		dwt_setpreambledetecttimeout(0);			//clear preamble timeout as RX is turned on early/late
		dwt_setrxtimeout((uint16)fwto_sy * 2);		//reconfigure the timeout before enable 
		//longer timeour as we cannot do delayed receive... so receiver needs to stay on for longer
		dwt_rxenable(DWT_START_RX_IMMEDIATE);
		dwt_setpreambledetecttimeout(PTO_PACS);		//configure preamble timeout
		dwt_setrxtimeout((uint16)fwto_sy);
		error_printf("error:enable rx fail\n");
	}
}

uint8 setPollRxStamp(instance_data_t *inst, uint8 idx, uint8 error) {
	uint8 type_pend = DWT_SIG_RX_TWR_ERROR;
	
	idx = idx & (MAX_ANCHOR_LIST_SIZE - 1);
	if(error == 0) {
		inst->remainingRespToRx = MAX_ANCHOR_LIST_SIZE - 1 - idx;
	}
	//printf("RxPoll--idx=%d, remainRxN=%d, error=%d\n", idx, inst->remainingRespToRx, error);
	//收到最后一个基站应答或则剩余应答个数为0，则发送TWR RESP帧
	if(inst->remainingRespToRx == 0 || idx == (MAX_ANCHOR_LIST_SIZE - 1)) {
		if(inst->twrmag.pollMask != 0) {
			inst->occupySlot = findFirstZeroBit(inst->slotMoniMap, inst->TWRslots_num);
			type_pend = send_response_msg(inst);
		} else {
			type_pend = DWT_SIG_RX_TWR_ERROR;
		}
	} else {		
		uint32 txStamp32h = (inst->grpPollTxStamp >> 8) + inst->fixedOffsetDelay32h;
		txStamp32h += (MAX_ANCHOR_LIST_SIZE + 1 - inst->remainingRespToRx) * inst->fixedPollDelayAnc32h;
		tag_enable_rx(txStamp32h, inst->fwto4PollFrame_sy);
		type_pend = DWT_SIG_RX_PENDING;
		//if(inst->remainingRespToRx == 1) printf("PTx:%u, sT:%u, cT:%u\n", (u32)txStamp32h, (u32)dwt_readsystimestamphi32(), portGetTickCnt_10us());
	}
	
	return type_pend;
}

uint8 setFinalRxStamp(instance_data_t *inst, uint8 idx, uint8 error) {
	uint8 type_pend = DWT_SIG_DW_IDLE;
	
	idx = idx & (MAX_ANCHOR_LIST_SIZE - 1);	//限制idx在合理的范围
	if(error == 0) {
		//inst->remainingRespToRx = MAX_ANCHOR_LIST_SIZE - 1 - idx;
	}
	//printf("RxFinal--idx=%d, remainRxN=%d, error=%d\n", idx, inst->remainingRespToRx, error);
	if(inst->remainingRespToRx == 0 || idx == (MAX_ANCHOR_LIST_SIZE - 1)) {
		if(inst->twrmag.finalCfm != (inst->twrmag.pollMask & 0x0f)) {
			//说明请求的slot被占用或则其它基站未发送final，重新查找slot
			inst->occupySlot = 0xff;
			//printf("occupy error:");
		}
		
		//printf("cfm:%x--valid:%x\n", inst->twrmag.finalCfm, inst->twrmag.validNum);
		if(inst->occupySlot < inst->TWRslots_num) {
			type_pend = DWT_SIG_RX_TWR_OKAY;
			//info_printf("finished TWR:%ld\n", dwt_readsystimestamphi32());
		} else {
			type_pend = DWT_SIG_RX_TWR_ERROR;
		}
		inst->wait4type = 0;
	} else {
		uint32 txStamp32h = inst->responseTxStamp32h;
		txStamp32h += (MAX_ANCHOR_LIST_SIZE + 1 - inst->remainingRespToRx) * inst->fixedFinalDelayAnc32h;
		tag_enable_rx(txStamp32h, inst->fwto4FinalFrame_sy);
		type_pend = DWT_SIG_RX_PENDING;
	}
	
	return type_pend;
}

void tag_change_state_to_listen(instance_data_t *inst) {
	dwt_forcetrxoff();
	inst->testAppState = TA_RXE_WAIT;
	inst->wait4type = UWBMAC_FRM_TYPE_BCN;
	inst->wait4ack = 0;
	inst->twrMode = BCN_LISTENER;
	dwt_setrxtimeout(0);
	dwt_setpreambledetecttimeout(0);
}

/**********************************************************************************************
 * function to process RX timeout event
 *********************************************************************************************/
void tag_process_rx_event(instance_data_t *inst, uint8 eventTypePend) {
	if(eventTypePend == DWT_SIG_RX_PENDING) {
		//已设置接收，跳过
	} else if(eventTypePend == DWT_SIG_TX_PENDING) {
		inst->testAppState = TA_TX_WAIT_CONF;
		inst->previousState = TA_TXRESPONSE_WAIT_SEND;
	} else if(eventTypePend == DWT_SIG_RX_TWR_OKAY) {
		dwt_forcetrxoff();	//this will clear all events
		inst->instToSleep = TRUE;
		inst->testAppState = TA_TXE_WAIT;
		inst->nextState = TA_TXGRPPOLL_WAIT_SEND;
		info_printf("finished TWR, slot:%d\n", inst->occupySlot);
	} else if(eventTypePend == DWT_SIG_RX_TWR_ERROR || eventTypePend == DWT_SIG_TX_ERROR){
		tag_change_state_to_listen(inst);
	} else if((inst->twrMode == TWR_INITIATOR) && (inst->previousState == TA_TXRESPONSE_WAIT_SEND)) {
		inst->instToSleep = TRUE;
		inst->testAppState = TA_TXE_WAIT;
		inst->nextState = TA_TXGRPPOLL_WAIT_SEND;
	} else {
		tag_change_state_to_listen(inst);
	}
}

/*********************************************************************************************
 * this function handles frame error event, it will either signal TO or re-enable the receiver
 *********************************************************************************************/
void tag_handle_error_unknownframe(event_data_t *dw_event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	
	if(inst->twrMode == TWR_INITIATOR) {
		dw_event->typePend = DWT_SIG_RX_PENDING;
		if(inst->wait4type == UWBMAC_FRM_TYPE_TWR_POLL) {
			inst->remainingRespToRx -= 1;
			dw_event->typePend = setPollRxStamp(inst, 0, 1);
		} else if(inst->wait4type == UWBMAC_FRM_TYPE_TWR_FINAL) {
			inst->remainingRespToRx -= 1;
			dw_event->typePend = setFinalRxStamp(inst, 0, 1);
		} else {
			dwt_setrxtimeout(0);
			dwt_setpreambledetecttimeout(0);
			dwt_rxenable(DWT_START_RX_IMMEDIATE);
			dw_event->typePend = DWT_SIG_RX_PENDING;
		}
	} else {
		dw_event->typePend = DWT_SIG_DW_IDLE;
	}
	
	dw_event->type = 0;
	dw_event->rxLength = 0;
	instance_putevent(dw_event, DWT_SIG_RX_TIMEOUT);
}

uint8 tag_process_beacon_msg(instance_data_t* inst, event_data_t *dw_event, uint8 *srcAddr, uint8 *messageData) {
	uint8 clfnum = messageData[CFNOF];
	
	inst->testAppState = TA_RXE_WAIT;
	
	inst->bcnlog[clfnum].srcAddr = srcAddr[0] + (srcAddr[1] << 8);
	inst->bcnlog[clfnum].slotMap = messageData[SMPOF] + (messageData[SMPOF + 1] << 8);
	inst->bcnlog[clfnum].clusMap = messageData[CMPOF] + (messageData[CMPOF + 1] << 8);
	inst->bcnlog[clfnum].receNum += 1;
	//收到俩个以上周期的beacon帧信息，查找合适的twr slot位置
	if(inst->bcnlog[clfnum].receNum > 2) {
		uint8 i = 0;
		inst->slotMoniMap = 0;
		for(i = 0; i < inst->TWRslots_num; ++i) {
			inst->slotMoniMap |= inst->bcnlog[i].slotMap;
		}
		i = findFirstZeroBit(inst->slotMoniMap, inst->TWRslots_num);
		inst->occupySlot = i;
		if(inst->occupySlot < inst->TWRslots_num) {
			uint8 selectNum = 0, version = 0;
			int timelength = 0;
			//此处说明有可用twr slot位置	
			timelength = ((inst->BCNslots_num - clfnum) * inst->BCNslotDuration_ms * 100);
			timelength += (inst->SVCslots_num * inst->SVCslotDuration_ms * 100);
			inst->refSlotStartTime_us = dw_event->uTimeStamp + timelength;
			inst->refSlotStartStamp = (dw_event->timeStamp + instance_convert_usec_to_devtimeu(timelength * 10)) & MASK_TXDTS;
			
			timelength = inst->occupySlot * inst->TWRslotDuration_ms * 100;
			inst->grpPollTxTime_us = inst->refSlotStartTime_us + timelength;
			inst->grpPollTxStamp = (inst->refSlotStartStamp + instance_convert_usec_to_devtimeu(timelength * 10)) & MASK_TXDTS;
			
			inst->fatherAddr = inst->bcnlog[clfnum].srcAddr;
			inst->fatherRef = clfnum;
			//inst->refSlotStartStamp = dw_event->timeStamp;
			
			version = (messageData[VEROF] >> 4) & 0x0f;
			if((version != 0) && (version !=  getVersionOfAncs())) {
				requestAncList();	//请求下载基站坐标数据
			}
			version = (messageData[VEROF] >> 4) & 0x0f;
			if((version != 0) && (version !=  getVersionOfArea())) {
				requestAreaData();	//请求下载危险区域数据
			}
			
			if(downloadTaskIsEmpty() == 0) {
				inst->testAppState = TA_DOWNLOAD_REQ_WAIT_SEND;
				info_printf("found new data version, now req download\n");
			} else {
				inst->nextState = TA_TXGRPPOLL_WAIT_SEND;
				inst->testAppState = TA_TXE_WAIT;
				info_printf("found empty slot:%d, now statr[%lld] twr exchange\n", inst->occupySlot, inst->grpPollTxStamp / 256);
			}
			
			//选择至多4个基站进行测距协议通信
			inst->twrmag.flag = 0;
			for(i = 0; i < inst->TWRslots_num; ++i) {
				if(inst->bcnlog[i].receNum > 0) {
					inst->twrmag.flag |= (0x01 << i);
					inst->twrmag.ancAddr[selectNum] = inst->bcnlog[i].srcAddr;
					selectNum += 1;
					inst->twrmag.idist[i] = 0;
					inst->twrmag.avgDist[i] = 0;
				}
				if(selectNum >= 4)	break;
			}
			for(i = selectNum; i < 4; ++i) {
				inst->twrmag.ancAddr[i] = 0xffff;
				inst->twrmag.idist[i] = 0;
				inst->twrmag.avgDist[i] = 0;
			}
			//inst->twrmag.ancAddr[3] = inst->twrmag.ancAddr[0];
			//inst->twrmag.ancAddr[0] = 0xffff;
			inst->twrmag.pollMask = selectNum;
		}
		for(i = 0; i < inst->TWRslots_num; ++i) {
			inst->bcnlog[i].receNum = 0;
			inst->bcnlog[i].slotMap = 0;
		}
	}
	return 0;
}

uint8 sent_grppoll_msg(instance_data_t* inst) {
	uint16 *data = NULL;
	uint32 *pos = NULL;
	uint32 txStamp32 = 0;
	uint8  i = 0;
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.destAddr[0] = 0xff;	//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = 0xff;	//广播方式
	inst->msg_f.seqNum = inst->frameSN++;
	
	inst->twrmag.quaFac = 0;	//后面可以寻找算法计算定位质量，此处暂时将其置0
	inst->twrmag.seqNum = inst->rangeNum++;
	//因为twrmag内部成员变量地址对齐关系，不建议直接memcpy复制
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_TWR_GRP_POLL;
	data = (uint16 *)&(inst->msg_f.messageData[GP_FLGOF]);
	data[0] = inst->twrmag.flag;
	data[1] = inst->twrmag.upPeriod;	//场强值
	for(i = 0; i < 4; ++i) {
		data[2 + i] = inst->twrmag.ancAddr[i];
	}
	
	inst->msg_f.messageData[GP_QFAOF] = inst->twrmag.quaFac; //
	if(inst->twrmag.posValid) {
		inst->msg_f.messageData[GP_QFAOF] |= 0x80;	//坐标有效
		inst->twrmag.posValid = 0;
	}
	inst->twrmag.seqNum = (inst->twrmag.seqNum + 1) & 0xff;
	inst->msg_f.messageData[GP_SQNOF] = inst->twrmag.seqNum; //序列码
	
	pos = (uint32 *)&(inst->msg_f.messageData[GP_COROF]);
	for(i = 0; i < 3; ++i) {
		pos[i] = inst->mypos.pos[i];
	}
	
	inst->psduLength = (TWR_GRP_POLL_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);	
	dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 1); 					//write frame control
	
	/********************start:设置group poll时间**********************/
	//time out，units are 1.0256us
	txStamp32 = inst->grpPollTxStamp >> 8;
	dwt_setrxaftertxdelay((uint32)(inst->tagPollRxDelay_sy));	//应该设置超时时间
	dwt_setrxtimeout((uint16)inst->fwto4PollFrame_sy);
	dwt_setpreambledetecttimeout(PTO_PACS); 					//configure preamble timeout
	dwt_setdelayedtrxtime(txStamp32);
	if((dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) || (dwt_readsystimestamphi32() == 0)) {
		error_printf("error: starttx\n");
		tag_change_state_to_listen(inst);
		return INST_NOT_DONE_YET;
	}
	inst->twrmag.gpollTxTime = txStamp32;
	inst->twrmag.gpollTxTime = (inst->twrmag.gpollTxTime << 8) + inst->txAntennaDelay;
	//printf("T:%u, s:%u\n", (u32)txStamp32, (u32)dwt_readsystimestamphi32());
	/********************end:设置group poll时间**********************/
	
	inst->remainingRespToRx = MAX_ANCHOR_LIST_SIZE; //expecting 4 anchor responses of poll
	inst->rxResponseMask = 0;	//reset/clear the mask of received poll responses when tx group poll
	inst->wait4type = UWBMAC_FRM_TYPE_TWR_POLL;//等待周围节点的连接请求应答
	inst->twrMode = TWR_INITIATOR;
	inst->wait4ack = 1;
	inst->idxOfAnc = 0;
	inst->twrmag.pollMask = 0;
	inst->slotMoniMap = 0;
	
	info_printf("GPoll-slot:%d AAds:[%d,%d,%d,%d]\n", inst->occupySlot, inst->twrmag.ancAddr[0], inst->twrmag.ancAddr[1], inst->twrmag.ancAddr[2], inst->twrmag.ancAddr[3]);
	//printf("s:%u, c:%d\n",(u32)dwt_readsystimestamphi32(), portGetTickCnt_10us());
	inst->previousState = TA_TXGRPPOLL_WAIT_SEND;
	inst->testAppState = TA_TX_WAIT_CONF;
	return INST_DONE_WAIT_FOR_NEXT_EVENT; //will use RX FWTO to time out (set above);
}

uint8 send_response_msg(instance_data_t* inst) {
	uint32 txStamp32h = 0;
	
	/*****************start:构建response帧***************************/
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.destAddr[0] = 0xff;	//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = 0xff;	//广播方式
	inst->msg_f.seqNum = inst->frameSN++;
	
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_TWR_RESP;
	inst->msg_f.messageData[RP_FLGOF] = 0;
	inst->msg_f.messageData[RP_DSLOF] = inst->occupySlot;	//请求的twr slot
	
	inst->psduLength = (TWR_RESP_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);	
	dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 1); //write frame control
	/*****************end:构建response帧*****************************/
	
	/****************start:设置时间****************************/
	txStamp32h = (inst->grpPollTxStamp >> 8) + inst->fixedOffsetDelay32h + MAX_ANCHOR_LIST_SIZE * inst->fixedPollDelayAnc32h + inst->fixedGuardDelay32h;
	
	dwt_setrxaftertxdelay((uint32)inst->tagFinalRxDelay_sy);
	dwt_setrxtimeout((uint16)inst->fwto4FinalFrame_sy);
	//dwt_setpreambledetecttimeout(PTO_PACS); //configure preamble timeout
	dwt_setdelayedtrxtime(txStamp32h);
	if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) { //transmit the frame
		error_printf("error:send resp fail-T:%u S:%u\n", (u32)txStamp32h, (u32)dwt_readsystimestamphi32());
		return DWT_SIG_TX_ERROR;
	}
	
	//printf("T:%u S:%u\n", (u32)txStamp32h, (u32)dwt_readsystimestamphi32());
	inst->responseTxStamp32h = txStamp32h;
	inst->twrmag.respTxTime = txStamp32h;
	inst->twrmag.respTxTime = (inst->twrmag.gpollTxTime << 8) + inst->txAntennaDelay;
	/****************end:设置时间******************************/
	
	inst->remainingRespToRx = MAX_ANCHOR_LIST_SIZE; //expecting 4 anchor responses of poll
	inst->rxResponseMask = 0;	//reset/clear the mask of received poll responses when tx group poll
	inst->wait4type = UWBMAC_FRM_TYPE_TWR_FINAL;//等待周围节点的连接请求应答
	inst->wait4ack = 1;
	inst->idxOfAnc = 0;
	inst->twrmag.finalMask = 0;
	inst->twrmag.finalCfm = 0;
	
	//printf("sending resp--requestSlot:%d\n", inst->occupySlot);
	return DWT_SIG_TX_PENDING;
}

//返回typePend
uint8 process_poll_msg(uint16 sourceAddress, uint16 dataSlotMap, event_data_t *event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint8 i = 0;
	
	--inst->remainingRespToRx;
	if(inst->remainingRespToRx >= MAX_ANCHOR_LIST_SIZE)	return DWT_SIG_RX_ERROR;
	for(i = MAX_ANCHOR_LIST_SIZE - 1 - inst->remainingRespToRx; i < MAX_ANCHOR_LIST_SIZE; ++i) {
		if(inst->twrmag.ancAddr[i] == sourceAddress) break;
	}
	if(i == MAX_ANCHOR_LIST_SIZE) return DWT_SIG_RX_ERROR;
	//printf("ad[%d]:%x-%x\n", i, inst->twrmag.ancAddr[i], sourceAddress);//打印这句信息，twr时间将会错乱
	//inst->remainingRespToRx = MAX_ANCHOR_LIST_SIZE - 1 - i;
	inst->twrmag.pollMask |= (0x01 << i);			//标识对应位下标基站poll帧已经接收到
	inst->twrmag.pollMask += 0x10;					//接收到的poll帧数量
	
	inst->twrmag.pollRxTime[i] = event->timeStamp;	//记录poll接收时间
	
	inst->slotMoniMap |= dataSlotMap;
	return setPollRxStamp(inst, i, 0);
}

uint8 process_final_msg(uint16 sourceAddress, uint8 msgid_index, uint8* frame, event_data_t *event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint8 i = 0;
	
	--inst->remainingRespToRx;
	if(inst->remainingRespToRx >= MAX_ANCHOR_LIST_SIZE)	return DWT_SIG_RX_ERROR;
	for(i = 0; i < MAX_ANCHOR_LIST_SIZE; ++i) {//MAX_ANCHOR_LIST_SIZE - 1 - inst->remainingRespToRx
		if(inst->twrmag.ancAddr[i] == sourceAddress) break;
	}
	if(i == MAX_ANCHOR_LIST_SIZE) return DWT_SIG_RX_ERROR;
	
	inst->twrmag.finalMask |= (0x01 << i);//标识对应位下标基站final帧已经接收到
	inst->twrmag.finalMask += 0x10;		//接收到的final帧数量
	if(frame[msgid_index + FN_FLGOF] == 1) {
		inst->twrmag.finalCfm |= (0x01 << i); 	//标识对应基站对请求slot的分配确认
	}
	inst->twrmag.finalRxTime[i] = event->timeStamp;	//记录final接收时间
	//Tx time of Poll message
	memcpy((uint8 *)&inst->twrmag.pollTxTime[i], &frame[msgid_index + FN_TPTOF], 5);
	//Rx time of response message
	memcpy((uint8 *)&inst->twrmag.respRxTime[i], &frame[msgid_index + FN_RRTOF], 5);
	//Tx time of final message
	memcpy((uint8 *)&inst->twrmag.finalTxTime[i], &frame[msgid_index + FN_TFTOF], 5);
	
	return setFinalRxStamp(inst, i, 0);
}
	  
uint8 tag_send_download_req_msg(instance_data_t* inst, uint8 isDelay) {
	u8 downloadType = getDownloadTaskType();
	if(downloadType <= 0) {
		//no download task
		tag_change_state_to_listen(inst);
		return INST_NOT_DONE_YET;
	}
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //inst->eui64[0]; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16>>8) & 0xff; 
	inst->msg_f.destAddr[0] = inst->fatherAddr & 0xff;			//set the destination address (broadcast == 0xffff)
	inst->msg_f.destAddr[1] = (inst->fatherAddr >> 8) & 0xff;  
	inst->msg_f.seqNum = inst->frameSN++; //copy sequence number and then increment
	
	inst->msg_f.messageData[MIDOF] = UWBMAC_FRM_TYPE_DOWNLOAD_REQ; 	//message ID code
	inst->msg_f.messageData[MIDOF+1] = downloadType;			//download data type: 1--Anc Position; 2--Dangerous Area
	*((u16 *)&inst->msg_f.messageData[2]) = getDownloadCurLength();	//area data: index
	
	inst->psduLength = (4 + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
	dwt_writetxdata(inst->psduLength, (uint8 *)  &inst->msg_f, 0);	// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 0); 						//write frame control
	
	dwt_setrxtimeout(50000); 
	dwt_setrxaftertxdelay(0);  //立即打开RX
	dwt_setpreambledetecttimeout(0);
	inst->wait4ack = DWT_RESPONSE_EXPECTED; //response is expected - automatically enable the receiver

	if(isDelay == DWT_START_TX_DELAYED) {
		dwt_setdelayedtrxtime((inst->grpPollTxStamp >> 8));
	}
	
	if(dwt_starttx(isDelay | inst->wait4ack) == DWT_ERROR) {
		error_printf("error:download req error!\n");
		tag_change_state_to_listen(inst);
		return INST_NOT_DONE_YET;
	}
	
	inst->wait4type = UWBMAC_FRM_TYPE_DOWNLOAD_RESP;//等待周围节点的连接请求应答
	inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
	inst->previousState = TA_DOWNLOAD_REQ_WAIT_SEND;
	
	return INST_DONE_WAIT_FOR_NEXT_EVENT;
}

uint8 tag_process_download_data(instance_data_t* inst, event_data_t *dw_event, uint8 *srcAddr, uint8 *messageData) {
	u16 *offset = (u16 *)&messageData[2];
	u16 *filesize = (u16 *)&messageData[4];
	u16 *datalength = (u16 *)&messageData[6];
	u16 sum;
	u8 isEmp;
	sum = downloadWrite(messageData[1], &messageData[8], *datalength, *offset);
	if(updateDownload(messageData[1], sum, *filesize) == 3) {
		//下载完成
		//BUZZER_On(200);
	}

	printf("Download：%d----(%d / %d)\n", messageData[1], sum, *filesize);

	isEmp = downloadTaskIsEmpty();
	if(sum >= *filesize && isEmp == 1) {
		ancPosListPrint(4);
		dangerAreaDataPrint_Test();
		//BUZZER_On(200);
		tag_change_state_to_listen(inst);
		return INST_NOT_DONE_YET;
	} else {
		return tag_send_download_req_msg(inst, 0);
	}
}

void tag_init(instance_data_t* inst) {
	//allow data, ack frames;
	dwt_enableframefilter(DWT_FF_DATA_EN | DWT_FF_ACK_EN);
	//设置唯一EUI64
	memcpy(inst->eui64, &inst->instanceAddress16, ADDR_BYTE_SIZE_S);
	dwt_seteui(inst->eui64);
	dwt_setpanid(inst->panID);
	dwt_setaddress16(inst->instanceAddress16);

	inst->twrmag.seqNum = 0;
	inst->twrmag.upPeriod = inst->sfPeriod_ms;
	inst->slotMoniMap = 0;
	inst->rangeNum = 0;
	inst->wait4ack = 0;

	dwt_setrxaftertxdelay(0);
	
#if (DEEP_SLEEP == 1)
	//configure the on wake parameters (upload the IC config settings)
	dwt_configuresleep((DWT_PRESRV_SLEEP|DWT_CONFIG|DWT_TANDV), (DWT_WAKE_WK|DWT_WAKE_CS|DWT_SLP_EN));
#endif
	
	instance_config_frameheader_16bit(inst);
	
	tag_change_state_to_listen(inst);
}

/*********************************************************************************************
 * this is the receive timeout event callback handler
 *********************************************************************************************/
void rx_to_cb_tag(const dwt_cb_data_t *rxd) {
	event_data_t dw_event;
	
	//printf("s:%u, c:%u\n", dwt_readsystimestamphi32(), portGetTickCnt_10us());
	//microcontroller time at which we received the frame
    dw_event.uTimeStamp = portGetTickCnt_10us();
    tag_handle_error_unknownframe(&dw_event);
}

/*********************************************************************************************
 * this is the receive error event callback handler
 *********************************************************************************************/
void rx_err_cb_tag(const dwt_cb_data_t *rxd) {
	event_data_t dw_event;

	//microcontroller time at which we received the frame
    dw_event.uTimeStamp = portGetTickCnt_10us();
    tag_handle_error_unknownframe(&dw_event);
}

/**********************************************************************************************
 * this is the receive event callback handler, the received event is processed and the instance either
 * responds by sending a response frame or re-enables the receiver to await the next frame
 * once the immediate action is taken care of the event is queued up for application to process
 *********************************************************************************************/
void rx_ok_cb_tag(const dwt_cb_data_t *rxd) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint8 rxTimeStamp[5]  = {0, 0, 0, 0, 0};

    uint8 rxd_event = 0, is_knownframe = 0;
	uint8 msgid_index  = 0, srcAddr_index = 0;
	event_data_t dw_event;

	//printf("rxdata...\n");
	
	//microcontroller time at which we received the frame
    dw_event.uTimeStamp = portGetTickCnt_10us();
	dw_event.rxLength = rxd->datalength;
	//need to process the frame control bytes to figure out what type of frame we have received
	if(rxd->fctrl[0] == 0x41) {
		if((rxd->fctrl[1] & 0xcc) == 0x88) {	//short address
			//function code is in first byte after source address
			msgid_index = FRAME_CRTL_AND_ADDRESS_S;
			srcAddr_index = FRAME_CTRLP + ADDR_BYTE_SIZE_S;
			rxd_event = DWT_SIG_RX_OKAY;
		} else if((rxd->fctrl[1] & 0xcc) == 0x8c) {
			//function code is in first byte after source address
			msgid_index = FRAME_CRTL_AND_ADDRESS_LS;
			srcAddr_index = FRAME_CTRLP + ADDR_BYTE_SIZE_L;
			rxd_event = DWT_SIG_RX_OKAY;
		} else {
			rxd_event = SIG_RX_UNKNOWN;	//not supported
		}
	} else {
		rxd_event = SIG_RX_UNKNOWN;		//not supported
	}
	
	dwt_readrxtimestamp(rxTimeStamp);	//read RX timestamp
	dwt_readrxdata((uint8 *)&dw_event.msgu.frame[0], rxd->datalength, 0);	//read data frame
	instance_seteventtime(&dw_event, rxTimeStamp);
	
	dw_event.type = 0; //type will be added as part of adding to event queue
	dw_event.typePend = DWT_SIG_DW_IDLE;
	
	//Process good/known frame types
	if(rxd_event == DWT_SIG_RX_OKAY) {
		uint16 sourceAddress = (((uint16)dw_event.msgu.frame[srcAddr_index+1]) << 8) + dw_event.msgu.frame[srcAddr_index];
		uint16 destAddress = (((uint16)dw_event.msgu.frame[srcAddr_index-1]) << 8) + dw_event.msgu.frame[srcAddr_index-2];
		//PAN ID must match - else discard this frame
//		if((dw_event.msgu.rxmsg_ss.panID[0] != (inst->panID & 0xff)) || (dw_event.msgu.rxmsg_ss.panID[1] != (inst->panID >> 8))) {
//			tag_handle_error_unknownframe(dw_event);
//			return;
//		}
		//过滤目的地址
		if((destAddress != 0xffff) && (destAddress != inst->instanceAddress16)) {
			tag_handle_error_unknownframe(&dw_event);
			return;
		}
		switch(dw_event.msgu.frame[msgid_index]) {
			case UWBMAC_FRM_TYPE_TWR_POLL:
				if(inst->twrMode == TWR_INITIATOR) {
					uint16 dataSlotMap = (((uint16)dw_event.msgu.frame[msgid_index + PL_SMPOF + 1]) << 8) + dw_event.msgu.frame[msgid_index + PL_SMPOF];
					//需要记录哪些基站应答了，哪些是超时了，用于final应答帧确认slot是否分配成功
					dw_event.typePend = process_poll_msg(sourceAddress, dataSlotMap, &dw_event);
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_TWR_FINAL:
				if(inst->twrMode == TWR_INITIATOR) {
					dw_event.typePend = process_final_msg(sourceAddress, msgid_index, &dw_event.msgu.frame[0], &dw_event);
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_BCN:
			case UWBMAC_FRM_TYPE_SVC:
			case UWBMAC_FRM_TYPE_DOWNLOAD_RESP:
				is_knownframe = 1;
				break;
			case UWBMAC_FRM_TYPE_CL_JOIN:
			case UWBMAC_FRM_TYPE_TWR_GRP_POLL:
			case UWBMAC_FRM_TYPE_TWR_RESP:
			default:
				is_knownframe = 0;
				break;
		}
		
	}
	if(is_knownframe == 1) {
		instance_putevent(&dw_event, rxd_event);
	} else {
		//need to re-enable the rx (got unknown frame type)
		tag_handle_error_unknownframe(&dw_event);
	}
}

void tag_calc_distance(instance_data_t *inst) {
	u8 i = 0, mask = 0x01, negNum = 0;
	int32 tof = 0, tof1 = 0, avgTof = 0;// tof2 = 0, tof3 = 0;
	int64 Rb, Da, Ra, Db, Ra1, Db1;
	double RaRbxDaDb = 0;
	double RbyDb = 0;
	double RayDa = 0;
	int quaFac = 0;
	
	for(i = 0; i < 4; ++i) {
		if((inst->twrmag.finalMask & mask) != 0) {
			//printf("Poll Tx:%lld, Rx:%lld\n", inst->twrmag.pollTxTime[i], inst->twrmag.pollRxTime[i]);
			//printf("Resp Rx:%lld, Tx:%lld\n", inst->twrmag.respRxTime[i], inst->twrmag.respTxTime);
			//printf("Fina Tx:%lld, Rx:%lld\n", inst->twrmag.finalTxTime[i], inst->twrmag.finalRxTime[i]);
			// poll response round trip delay time is calculated as
			// (RespRxTime - PollTxTime) - (RespTxTime - PollRxTime)
//			Ra = (int64)((inst->twrmag.respRxTime[i] - inst->twrmag.pollTxTime[i]) & MASK_40BIT);
//			Db = (int64)((inst->twrmag.respTxTime - inst->twrmag.pollRxTime[i]) & MASK_40BIT);
//					
//			// response final round trip delay time is calculated as
//			// (FinalRxTime - RespTxTime) - (FinalTxTime - RespRxTime)
//			Rb = (int64)((inst->twrmag.finalRxTime[i] - inst->twrmag.respTxTime) & MASK_40BIT);
//			Da = (int64)((inst->twrmag.finalTxTime[i] - inst->twrmag.respRxTime[i]) & MASK_40BIT);

//			RaRbxDaDb = (((double)Ra))*(((double)Rb)) - (((double)Da))*(((double)Db));
//			printf("Ra:%lld, Rb:%lld, ", Ra, Rb);
//			printf("Da:%lld, Db:%lld, ", Da, Db);
//			RbyDb = ((double)Rb + (double)Db);
//			RayDa = ((double)Ra + (double)Da);
//			tof = (int32) ( RaRbxDaDb/(RbyDb + RayDa) );
//			printf("R-R:%.1f, R+R:%.1f", RaRbxDaDb, (RbyDb + RayDa));
//			printf("addR:%d tof:%d-%ld\n", inst->twrmag.ancAddr[i], i, tof);
			
			Ra = (int64)((inst->twrmag.pollRxTime[i] - inst->twrmag.gpollTxTime) & MASK_40BIT);
			Da = (int64)((inst->twrmag.respTxTime - inst->twrmag.pollRxTime[i]) & MASK_40BIT);
			Db = inst->twrmag.pollTxTime[i];
			Rb = inst->twrmag.respRxTime[i];
			Ra1 = (int64)((inst->twrmag.finalRxTime[i] - inst->twrmag.respTxTime) & MASK_40BIT);
			Db1 = inst->twrmag.finalTxTime[i];
			
			RaRbxDaDb = (((double)Ra))*(((double)Rb)) - (((double)Da))*(((double)Db));
			RbyDb = ((double)Rb + (double)Db);
			RayDa = ((double)Ra + (double)Da);
			tof = (int32) ( RaRbxDaDb/(RbyDb + RayDa) );
			//if(tof > 8200)	tof -= 8200;
			//printf("Ra:%lld, Da:%lld, ", Ra, Da);
			//printf("Rb:%lld, Db:%lld, ", Rb, Db);
			//printf("RR:%.1f, RRDD:%.1f", RaRbxDaDb, (RbyDb + RayDa));
			//printf("addR:%d idx:%d, tof:%ld, ", inst->twrmag.ancAddr[i], i, tof);
		
//			
//			RaRbxDaDb = (((double)Ra))*(((double)Ra1)) - (((double)Db))*(((double)Db1));
//			RbyDb = ((double)Ra1 + (double)Db);
//			RayDa = ((double)Ra + (double)Db1);
//			tof2 = (int32) ( RaRbxDaDb/(RbyDb + RayDa) );
			
//			RbyDb = ((double)Ra + (double)Rb + (double)Ra1);
//			RayDa = ((double)Da + (double)Db + (double)Db1);
//			RaRbxDaDb = (RbyDb - RayDa) * (RbyDb + 2.0 * RayDa) / 9.0 + ((double)(Da) * (double)(Db) + (double)(Da) * (double)(Db1) + (double)(Db) * (double)(Db1));
//			RaRbxDaDb = RaRbxDaDb * 2.0;
//			RbyDb = ((double)Ra * (double)Rb) / (double)Db1; 
//			RayDa = ((double)Da * (double)Db) / (double)Ra1;
//			RaRbxDaDb = (double)Db1 * (double)Ra1 / RaRbxDaDb;
//			tof3 = (int32) ((RaRbxDaDb * (RbyDb - RayDa)) / 1);
			
//			printf("addR:%d idx:%d, tof:(%ld, %ld, %ld, %ld), ", inst->twrmag.ancAddr[i], i, tof, tof1, tof2, tof3);
			
//			if(instance_calculate_rangefromTOF(i, tof) == 1) {
//				inst->newRange = TOF_REPORT_T2A;
//				inst->rxResponseMaskReport |= (0x01 << i);
//			} else {
//				instance_cleardisttable(i);
//			}
//			printf("Dis:%.2f\n", instance_get_idist_mm(i)/1000.0);
			
			
			RaRbxDaDb = (((double)Ra1))*(((double)Rb)) - (((double)Da))*(((double)Db1));
			RbyDb = ((double)Rb + (double)Db1);
			RayDa = ((double)Ra1 + (double)Da);
			tof1 = (int32) ( RaRbxDaDb/(RbyDb + RayDa) );
			//printf("Ra:%lld, Da:%lld, ", Ra1, Da);
			//printf("Rb:%lld, Db:%lld, ", Rb, Db1);
			//printf("RR:%.1f, RRDD:%.1f", RaRbxDaDb, (RbyDb + RayDa));
			//printf("addR:%d idx:%d, tof:%ld, ", inst->twrmag.ancAddr[i], i, tof1);
//			printf("[%ld, %ld] ", tof, tof1);
//			negNum = 0;
//			avgTof = (tof + tof1) / 2;
//			if(tof < 0) {
//				tof = -tof;
//				negNum += 1;
//				avgTof = tof1;
//			}
//			if(tof1 < 0) {
//				tof1 = -tof1;
//				negNum += 1;
//				avgTof = tof;
//			}
//			quaFac = (100 * abs((tof - tof1))) / avgTof;
//			if(((negNum == 0)&&(quaFac < 67)) || ((negNum == 1)&&(quaFac > 70))) {
//				if(instance_calculate_rangefromTOF(i, avgTof) == 1) {
//					inst->newRange = TOF_REPORT_T2A;
//					inst->rxResponseMaskReport |= (0x01 << i);
//					inst->twrmag.idist[i] = instance_get_idist_mm(i)/1000.0;
//				} else {
//					instance_cleardisttable(i);
//				}
//			}
//			printf("addR:%d idx:%d, tof:%ld, qua:%d ", inst->twrmag.ancAddr[i], i, avgTof, quaFac);
			
			if(instance_calculate_rangefromTOF(i, tof) == 1) {
				inst->newRange = TOF_REPORT_T2A;
				inst->rxResponseMaskReport |= (0x01 << i);
				inst->twrmag.idist[i] = instance_get_idist_mm(i)/1000.0;
				inst->twrmag.avgDist[i] = 0.2 * inst->twrmag.avgDist[i] + 0.8 * inst->twrmag.idist[i];//滤波
			} else {
				instance_cleardisttable(i);
			}
			printf("addR:%d idx:%d, [%ld, %ld] ", inst->twrmag.ancAddr[i], i, tof, tof1);
			printf("Dis:%.2f, avg:%.2f\n", inst->twrmag.idist[i], inst->twrmag.avgDist[i]);
			
		}
		mask <<= 1;
	}
}


// ------------------------------------------------------------------------------
// the main instance state machine for tag application
// ------------------------------------------------------------------------------
int tag_app_run(instance_data_t *inst) {
	int instDone = INST_NOT_DONE_YET;
    int message = instance_peekevent(); //get any of the received events from ISR
	uint8 ldebug = 0;
	static uint8 lastState = 0;
	
    switch(inst->testAppState) {
		case TA_INIT:
			if(ldebug == 1) printf("TA_INIT\n");
			tag_init(inst);
			break;
		case TA_SLEEP_DONE:
        {
        	event_data_t* dw_event = instance_getevent(10); //clear the event from the queue
			int32 time = 0;
			if(ldebug == 1 && lastState != inst->testAppState) {
				printf("TA_SLEEP_DONE\n");
				lastState = inst->testAppState;
			}
			//wait timeout for wake up the DW1000 IC 
			if (dw_event->type != DWT_SIG_RX_TIMEOUT) {
                instDone = INST_DONE_WAIT_FOR_NEXT_EVENT; //wait here for sleep timeout
                break;
            }
            instDone = INST_NOT_DONE_YET;
            inst->instToSleep = FALSE ;
            inst->testAppState = inst->nextState;
            inst->nextState = 0; //clear
			inst->instanceWakeTime_us = portGetTickCnt_10us(); // Record the time count when IC wake-up
#if (DEEP_SLEEP == 1)
            {
				led_on(LED4);
            	DWM_wakeup_fast();	//wake up device from low power mode, ~2.2ms
				if(dwt_readsystimestamphi32() == 0) {
					//DWM_wakeup_fast();	//确保dw1000被唤醒了--~9ms
					DWM_set_slowrate();
					if(dwt_spicswakeup(inst->msg_f.messageData, 100)) {
						error_printf("error:wake up\n");
					}
					DWM_set_fastrate();
				}
            	led_off(LED4);
				
                dwt_setleds(1);
                //TX antenna delay needs reprogramming as it is not preserved (only RX)
                dwt_settxantennadelay(inst->txAntennaDelay) ;
#if(DISCOVERY == 0)
                //set EUI as it will not be preserved unless the EUI is programmed and loaded from NVM
				dwt_seteui(inst->eui64);
#endif
            }
#else
            delay_ms(3); //to approximate match the time spent in the #if above
#endif
            instance_set_antennadelays(); //this will update the antenna delay if it has changed
            instance_set_txpower(); //configure TX power if it has changed
			
#if (DEEP_SLEEP == 1)
			//delay_ms(5);
			time = inst->refSlotStartTime_us + inst->occupySlot * inst->TWRslotDuration_ms * 100 - portGetTickCnt_10us();
//			if(time > 1000) {
//				delay_ms((time - 1000) / 100);
//			}
			inst->grpPollTxStamp = dwt_readsystimestamphi32();
			inst->grpPollTxStamp = inst->grpPollTxStamp * 256;
			inst->grpPollTxStamp += instance_convert_usec_to_devtimeu((inst->refSlotStartTime_us + inst->occupySlot * inst->TWRslotDuration_ms * 100 - portGetTickCnt_10us()) * 10);
#endif
			//printf("now dw1000 wake up:%ld, ref:%u, ocu:%d, S:%ld\n", time, (u32)inst->refSlotStartTime_us, inst->occupySlot, dwt_readsystimestamphi32());
			//debug_printf("wake up:%ld, s:%ld\n", time * 10, dwt_readsystimestamphi32());
       }			
			break;
		case TA_TXE_WAIT:
			if(ldebug == 1)	printf("TA_TXE_WAIT\n") ;
			//go to sleep before sending the next group poll/ starting new ranging exchange 
			if((inst->nextState == TA_TXGRPPOLL_WAIT_SEND) && (inst->instToSleep)) {
            	inst->rangeNum++; 							//increment the range number before going to sleep
                instDone = INST_DONE_WAIT_FOR_NEXT_EVENT_TO;//wait to timeout
                inst->testAppState = TA_SLEEP_DONE;			//change to sleep state
                {
#if (DEEP_SLEEP == 1)
                	//put device into low power mode
					dwt_entersleep();
#endif
					if(inst->twrmag.finalMask != 0) {
						tag_calc_distance(inst);
						//计算坐标
						inst->twrmag.posValid = 1;
					}
                }
            } else {
				//in otheer situation, change to the nest state
                inst->testAppState = inst->nextState;
                inst->nextState = 0; //clear
            }
            break ; // end case TA_TXE_WAIT
		case TA_TXGRPPOLL_WAIT_SEND:
			if(ldebug == 1)	printf("TA_TXGRPPOLL_WAIT_SEND\n") ;
			instDone = sent_grppoll_msg(inst);
			break;
		case TA_DOWNLOAD_REQ_WAIT_SEND:
			instDone = tag_send_download_req_msg(inst, DWT_START_TX_DELAYED);
			break;
		case TA_TX_WAIT_CONF:
		{
			event_data_t *dw_event = instance_getevent(11);	//get and clear this event
			if(ldebug == 1 && lastState != inst->testAppState) {
				printf("TX_WAIT_CON\n");
				lastState = inst->testAppState;
			}
			if(dw_event->type != DWT_SIG_TX_DONE) {
				instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
				break;
			}
			instDone = INST_NOT_DONE_YET;
			if(inst->previousState == TA_TXRESPONSE_WAIT_SEND) {
				//记录response时间
				inst->responseTxStamp32h = dw_event->timeStamp32h;
				inst->twrmag.respTxTime = dw_event->timeStamp;
				//printf("set response time\n");		
			} else if(inst->previousState == TA_TXGRPPOLL_WAIT_SEND) {
				inst->grpPollTxStamp = dw_event->timeStamp;
				inst->twrmag.gpollTxTime = dw_event->timeStamp;
			}
			inst->testAppState = TA_RXE_WAIT;
			//printf("ssc:%u\n", (u32)dw_event->timeStamp32h);			
		}
			break;
		case TA_RXE_WAIT:
			if(ldebug == 1)	printf("RXE_WAIT\n");
			//if this is set the RX will turn on automatically after TX
			if(inst->wait4ack == 0) {
				if(dwt_read16bitoffsetreg(0x19, 1) != 0x0505) {
					dwt_rxenable(DWT_START_RX_IMMEDIATE);	//turn on RX
				}
			} else {
				//clear the flag, the next time we want to turn the RX on it might not be auto
				inst->wait4ack = 0;
			}
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			inst->testAppState = TA_RX_WAIT_DATA;
			if(message == 0)	break;
		case TA_RX_WAIT_DATA:
			if(ldebug == 1 && lastState != inst->testAppState) {
				lastState = inst->testAppState;
				printf("R_W_DATA\n") ;
			}
			switch(message) {
				//if we have received a DWT_SIG_RX_OKAY event
				//this means that the message is IEEE data type
				//need to check frame control to know which addressing mode is used
				case DWT_SIG_RX_OKAY:
				{
					event_data_t* dw_event = instance_getevent(15); //get and clear the event
					uint8 srcAddr[8] = {0,0,0,0,0,0,0,0};
					uint8 dstAddr[8] = {0,0,0,0,0,0,0,0};
					uint8 msgID = 0;
					uint8 *messageData = NULL;
					if(ldebug == 1)	printf("RX_OKAY:%lu\n", dw_event->timeStamp32h);
					memcpy(&srcAddr[0], &(dw_event->msgu.rxmsg_ss.sourceAddr[0]), ADDR_BYTE_SIZE_S);
					memcpy(&dstAddr[0], &(dw_event->msgu.rxmsg_ss.destAddr[0]), ADDR_BYTE_SIZE_S);
					msgID = dw_event->msgu.rxmsg_ss.messageData[FCODE];
					messageData = &(dw_event->msgu.rxmsg_ss.messageData[0]);
					//process message
					switch(msgID) {
						case UWBMAC_FRM_TYPE_BCN:
							//printf("rbea\n");
							tag_process_beacon_msg(inst, dw_event, srcAddr, messageData);
							break;
						case UWBMAC_FRM_TYPE_TWR_POLL:
							tag_process_rx_event(inst, dw_event->typePend);
							//printf("rpol\n");
							break;
						case UWBMAC_FRM_TYPE_TWR_FINAL:
							tag_process_rx_event(inst, dw_event->typePend);
							//printf("rfin:%d, type:%d\n", srcAddr[0], dw_event->typePend);
							break;
						case UWBMAC_FRM_TYPE_DOWNLOAD_RESP:
							tag_process_download_data(inst, dw_event, srcAddr, messageData);
							break;
						default:
							break;
					}
				}
					break;
				case DWT_SIG_RX_TIMEOUT:
				{
					event_data_t* dw_event = instance_getevent(17);//get and clear the event
					tag_process_rx_event(inst, dw_event->typePend);
					//debug_printf("RTimeout:%d\n", dw_event->typePend);
					message = 0;
				}
					break;
				default:
					if(message) {
						instance_getevent(20);
					}
					if(instDone == INST_NOT_DONE_YET) {
						instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
					}
					break;
			}
			break;
		default:
			break;
    } // end switch on testAppState
	
    return instDone;
} // end testapprun_tag()

// ------------------------------------------------------------------
int tag_run(void) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
    int done = INST_NOT_DONE_YET;

	//check if timer has expired
	if(inst->instanceTimerEn == 1) {
		if((portGetTickCnt_10us() - inst->instanceSleepTime_us) >= inst->nextWakeUpTime_us) {
			event_data_t dw_event;
			inst->instanceTimerEn = 0;
			dw_event.rxLength = 0;
			dw_event.type = DWT_SIG_RX_TIMEOUT;
			instance_putevent(&dw_event, DWT_SIG_RX_TIMEOUT);
		}
	}
	
    while(done == INST_NOT_DONE_YET) {
		done = tag_app_run(inst); // run the communications application
		if(instance_peekevent() != 0) {
			done = INST_NOT_DONE_YET;
		}
	}
	
	if(done == INST_DONE_WAIT_FOR_NEXT_EVENT_TO) {
		//tag has finished TWR and needs to configure sleep time
		inst->refSlotStartTime_us += inst->sfPeriod_ms * 100;
		//提前12ms（1200）切换到唤醒状态，因为需要花费2.2~11.2ms的时间唤醒设备
		inst->instanceSleepTime_us = portGetTickCnt_10us(); // Record the time count when IC sleep
		inst->nextWakeUpTime_us = inst->refSlotStartTime_us - inst->instanceSleepTime_us + inst->occupySlot * inst->TWRslotDuration_ms * 100 - 3000;
		
		inst->refSlotStartStamp = (inst->refSlotStartStamp + instance_convert_usec_to_devtimeu(inst->sfPeriod_ms * 1000)) & MASK_TXDTS;
		inst->grpPollTxStamp = inst->refSlotStartStamp + instance_convert_usec_to_devtimeu(inst->occupySlot * inst->TWRslotDuration_ms * 1000);
		
		inst->instanceTimerEn = 1;	//start timer
		instance_clearevents();
		//printf("sleepT:%d, cutT:%d\n", (u32)inst->instanceSleepTime_us, portGetTickCnt_10us());
	}
	
	return 0;
}

