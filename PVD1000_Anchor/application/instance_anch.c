#include <stdlib.h>

#include "deca_device_api.h"
#include "deca_spi.h"
#include "deca_regs.h"

#include "instance.h"
#include "route.h"

// ----------------------------------------------------------
//      Data Definitions
// ----------------------------------------------------------
#define ONE_POS_INFO_SIZE		(18)
#define POS_UWB_BUF_MAX_SIZE	(4 * ONE_POS_INFO_SIZE)
typedef struct {
	u16 length;
	u8 num;
	//addr(2 bytes), xyz(12 bytes), alarm(1 byte), elecValue(2 bytes), seqNum(1 byte) = 18 bytes
	u8 data[POS_UWB_BUF_MAX_SIZE];
}PosUWBBufStr;

PosUWBBufStr posUWBBuf = {0, 0};

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// NOTE: the maximum RX timeout is ~ 65ms
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// ----------------------------------------------------------
// Functions
// ----------------------------------------------------------
//���Ż�����1�������岻��ʱ���Ƿ��Ǹ��ǻ������Ѵ��ڵı�ǩ��Ϣ����������������ʹ��̨�Ͽ��ȡ��������
void addTagPos_ToBuf(uint16 addr, uint8* pos, u8 alarm, uint16 elecValue, u8 seqNum) {
	uint16 offset = posUWBBuf.length;
	//printf("Add tag pos, cur offset:%d\n", offset);
	if((offset + ONE_POS_INFO_SIZE) > POS_UWB_BUF_MAX_SIZE)	return;	//����װ���£�Ŀǰ����
	//printf("Buffer is empty\n");
	posUWBBuf.data[offset + 0] = addr & 0xff;
	posUWBBuf.data[offset + 1] = (addr >> 8) & 0xff;
	memcpy(&posUWBBuf.data[offset + 2], pos, 12);
	posUWBBuf.data[offset + 14] = alarm;
	posUWBBuf.data[offset + 15] = elecValue & 0xff;
	posUWBBuf.data[offset + 16] = (elecValue >> 8) & 0xff;
	posUWBBuf.data[offset + 17] = seqNum;
	
	posUWBBuf.length = offset + 18;
	posUWBBuf.num += 1;
}

//����ʣ����ٸ���ǩ����û�п���
//���Ż�����1��������ռ䲻��ʱ���Ƿ��Ǹ��ǻ������Ѵ��ڵı�ǩ��Ϣ����������������ʹ��̨�Ͽ��ȡ��������
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
		posUWBBuf.length += ONE_POS_INFO_SIZE * leftNum;
		posUWBBuf.num += leftNum;
	} else {
		debug_printf("%d tag info overflow:%d\n", num, num - leftNum);
	}
	return (num - leftNum);
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

void anch_fill_msg_addr(instance_data_t* inst, uint16 dstaddr, uint8 msgid) {
	inst->msg_f.sourceAddr[0] = inst->instanceAddress16 & 0xff; //copy the address
	inst->msg_f.sourceAddr[1] = (inst->instanceAddress16 >> 8) & 0xff;
	inst->msg_f.destAddr[0] = dstaddr & 0xff;
	inst->msg_f.destAddr[1] = (dstaddr >> 8) & 0xff;
	inst->msg_f.seqNum = inst->frameSN++;
	inst->msg_f.messageData[MIDOF] = msgid;
}

void prepare_beacon_frame(instance_data_t* inst, uint8_t flag) {
	uint16_t offset = 0;
	const RouteElement *element = getNextElement();
	
	anch_fill_msg_addr(inst, 0xffff, UWBMAC_FRM_TYPE_BCN);
	inst->msg_f.messageData[SIDOF] = inst->bcnmag.sessionID;
	inst->msg_f.messageData[FLGOF] = flag;
	
	if(isGateway()) {
		//�ж��Ƿ�������
		inst->msg_f.messageData[FLGOF] |= BCN_GATEWAY;
	}
	
	inst->msg_f.messageData[CSNOF] = inst->bcnmag.clusterSlotNum;
	inst->msg_f.messageData[CFNOF] = inst->bcnmag.clusterFrameNum;
	inst->msg_f.messageData[CMPOF] = inst->bcnmag.clusterSelfMap & 0xff;
	inst->msg_f.messageData[CMPOF+1] = (inst->bcnmag.clusterSelfMap >> 8) & 0xff;
	//clusterNeigMap can be deleted
	inst->msg_f.messageData[NMPOF] = inst->bcnmag.clusterNeigMap & 0xff;
	inst->msg_f.messageData[NMPOF+1] = (inst->bcnmag.clusterNeigMap >> 8) & 0xff;
	
	inst->bcnmag.slotSelfMap = inst->bcnmag.nextSlotMap;	//����slotλͼ
	inst->bcnmag.nextSlotMap = 0;
	
	inst->msg_f.messageData[SMPOF] = inst->bcnmag.slotSelfMap & 0xff;
	inst->msg_f.messageData[SMPOF+1] = (inst->bcnmag.slotSelfMap >> 8) & 0xff;
	
	inst->msg_f.messageData[VEROF] = (getVersionOfArea() << 4) | getVersionOfAncs();//�汾��
	
	//printf("verison: %x\n", inst->msg_f.messageData[VEROF]);
	
	//route table element
	inst->msg_f.messageData[DEPTHOF] = getGatewayDepth();
	inst->msg_f.messageData[DESTOF] = element->dstID & 0xff;
	inst->msg_f.messageData[DESTOF + 1] = (element->dstID >> 8) & 0xff;
	inst->msg_f.messageData[DPGAOF] = element->depth;
	if(element->isGateway) {
		inst->msg_f.messageData[DPGAOF] |= BCN_GATEWAY;
	}

	inst->psduLength = 0;
	offset = DPGAOF;
	
	if(flag & BCN_EXT) {
		inst->msg_f.messageData[offset+1] = inst->jcofmsg.msgID;
		inst->msg_f.messageData[offset+2] = inst->jcofmsg.chldAddr & 0xff;
		inst->msg_f.messageData[offset+3] = (inst->jcofmsg.chldAddr>>8) & 0xff;
		inst->msg_f.messageData[offset+4] = --inst->jcofmsg.clusterLock;
		inst->msg_f.messageData[offset+5] = 0xff;
		if((inst->bcnmag.clusterSelfMap & (0x01 << inst->jcofmsg.clusterSeat)) == 0) {
			inst->msg_f.messageData[offset+5] = inst->jcofmsg.clusterSeat;
		}
		inst->psduLength = 5;
		
		if(inst->jcofmsg.clusterLock == 0) {
			inst->bcnmag.bcnFlag &= (~BCN_EXT);
			inst->bcnmag.clusterSelfMap |= (0x01 << inst->jcofmsg.clusterSeat);
		}
	} else if(posUWBBuf.num > 0){
		//��ǩλ�û����������ݣ���ת��
		if((posUWBBuf.num * ONE_POS_INFO_SIZE) == posUWBBuf.length) {
			uint16_t uploadAddr = getGatewayOut();
			
			inst->msg_f.messageData[FLGOF] |= BCN_EXT;
			inst->msg_f.messageData[offset+1] = UWBMAC_FRM_TYPE_TAGS_POS;
			
			if(uploadAddr == 0xffff) {
				inst->msg_f.messageData[offset+2] = inst->fatherAddr & 0xff;
				inst->msg_f.messageData[offset+3] = (inst->fatherAddr >> 8) & 0xff;
			} else {
				inst->msg_f.messageData[offset+2] = uploadAddr & 0xff;
				inst->msg_f.messageData[offset+3] = (uploadAddr >> 8) & 0xff;
			}
			
			inst->msg_f.messageData[offset+4] = posUWBBuf.num;
			memcpy(&(inst->msg_f.messageData[offset+5]), posUWBBuf.data, posUWBBuf.length);
			inst->psduLength = posUWBBuf.length + 4;
		}
		clearTagPosBuf();
	}
	inst->psduLength += (BCN_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
}

//����0�ɹ���1���޿���λ��seat����Ҫ���¶���·״̬�������������������
uint8 prepare_join_request_frame(instance_data_t* inst) {
	uint8_t i = 0;
	uint16_t mask = 0x01;

	anch_fill_msg_addr(inst, 0xffff, UWBMAC_FRM_TYPE_CL_JOIN);
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
//		dwt_setpreambledetecttimeout(0);
//		dwt_setrxtimeout(0);
		inst->testAppState = TA_RXE_WAIT;
		inst->twrMode = BCN_LISTENER;	//listen bcn msg for collecting network info.
		//no download task
		return INST_NOT_DONE_YET;
		//inst->instToSleep = TRUE;
	}
	//�������֡����ͷ�̶���ʽ��Ϣ
	anch_fill_msg_addr(inst, inst->fatherAddr, UWBMAC_FRM_TYPE_DOWNLOAD_REQ);

	inst->msg_f.messageData[MIDOF+1] = (u8)downloadType;			//download data type: 1--Anc Position; 2--Dangerous Area
	*((u16 *)&inst->msg_f.messageData[2]) = getDownloadCurLength();	//area data: index
	
	inst->psduLength = (4 + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC);
	dwt_writetxdata(inst->psduLength, (uint8 *)  &inst->msg_f, 0) ;	// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 0); 						//write frame control
	
	inst->wait4ack = DWT_RESPONSE_EXPECTED; //response is expected - automatically enable the receiver
	//dwt_setrxtimeout(0); 
	//dwt_setpreambledetecttimeout(0);
	dwt_setrxaftertxdelay(0);  //������RX

	//����������ʱ������ָ��ʱ��㷢��
	if(isDelay == DWT_START_TX_DELAYED) {
		dwt_setdelayedtrxtime(inst->svcStartStamp32h);
	}
	
	if(dwt_starttx(isDelay | inst->wait4ack) == DWT_ERROR) {
		error_printf("error:download req error!\n");
		return INST_NOT_DONE_YET;
	}
	debug_printf("req[%d] download[%d]\n", inst->fatherAddr, downloadType);
	inst->wait4type = UWBMAC_FRM_TYPE_DOWNLOAD_RESP;//�ȴ���Χ�ڵ����������Ӧ��
	inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
	inst->previousState = TA_DOWNLOAD_REQ_WAIT_SEND;
	
	return INST_DONE_WAIT_FOR_NEXT_EVENT;
}

//���������������ڴ�
uint8 anchor_process_download_data(instance_data_t* inst, event_data_t *dw_event, uint16 srcAddr16, uint8 *messageData) {
	u16 *offset = (u16 *)&messageData[2];		//д���ַ��ƫ����
	u16 *filesize = (u16 *)&messageData[4];		//���������ݵ��ܴ�С
	u16 *datalength = (u16 *)&messageData[6];	//��ǰ���ݿ�ĳ���
	u16 sum;
	u8 isEmp;
	//�����������ڴ�
	sum = downloadWrite(messageData[1], &messageData[8], *datalength, *offset);
	if(updateDownload(messageData[1], sum, *filesize) == 3) {
		//������ɣ�ָʾ�Ƴ���
		LED_STATUS2 = 1;
	}
	
	printf("Download:%d--(%d / %d)\n", messageData[1], sum, *filesize);
	//����Ƿ�����������
	isEmp = downloadTaskIsEmpty();	
	if(sum >= *filesize && isEmp == 1) {
		ancPosListPrint(4);
		dangerAreaDataPrint_Test();
		
		inst->testAppState = TA_RXE_WAIT; 
	} else {
		//***********����2��start******************
		//��û��������ɣ������ʵ�ʱ����������
		//����Ѿ���������ռ�õ�bcn slot����ֻ�����¸������м�������
		//int time_out = inst->beaconTXTimeCnt + inst->BCNslotDuration_ms * 100 - (portGetTickCnt_10us() + 100 * inst->sfPeriod_ms);
		//if(time_out < 0) {
		//
		//}
		//***********����2��end******************
		
		//***********����1��start******************
		//send_download_req_msg(inst, 0);
		//***********����1��end******************
		
		//����bcn slotʱ��϶̣���ȡһ�����ݿ��ʣ��ʱ���Ѿ������ˣ��������ǰһ�ַ���
		//����ÿ�����ȷ�����beacon��ʣ��ʱ�䷢��һ���������أ�Ȼ��תȥ�����ŵ�
		
		inst->testAppState = TA_RXE_WAIT; 
	}
	return 0;
}

uint8 process_beacon_msg(instance_data_t* inst, event_data_t *dw_event, uint16 srcAddr16, uint8 *messageData) {
	uint16_t clustermap = messageData[CMPOF] + (messageData[CMPOF+1] << 8);
	uint16_t neigmap = messageData[NMPOF] + (messageData[NMPOF+1] << 8);
	uint16 addr = 0;
	u8 srcClusterFraNum = messageData[CFNOF];
	
	inst->testAppState = TA_RXE_WAIT;
	
	//�ж��Ƿ������չ���ݣ�������
	if(messageData[FLGOF] & BCN_EXT) {
		uint8 *dat = messageData + BCN_MSG_LEN;
		addr = dat[1] + (dat[2] << 8);
		
		//������������ȷ��Ӧ��������Ҫ�ϸ���ƣ������������ܼ���
		if((inst->wait4type == dat[0]) && (dat[0] == UWBMAC_FRM_TYPE_CL_JOIN_CFM)) {
			
			//�ռ�����Ӧ����Χ�ڵ��Ӧ��󼴿ɼ������磬������...
			if((addr == inst->instanceAddress16) && (inst->bcnmag.clusterFrameNum < 16)) {
				debug_printf("Rec[%d] Join Cof: %x[%d]\n", srcAddr16, addr, dat[4]);
				
				if((inst->bcnmag.clusterFrameNum == dat[4]) && (inst->bcnlog[srcClusterFraNum].flag & 0x01)) {
					inst->bcnlog[srcClusterFraNum].flag &= (~0x01);
					inst->wait4ackNum -= 1;
					if(inst->wait4ackNum == 0) {
						inst->joinedNet = 1;	//��ʶ�ɹ���������
						LED_STATUS4 = 0;		//��˵���Ѿ���������
						inst->wait4type = 0;
						inst->bcnmag.clusterSelfMap = 0x01 << inst->bcnmag.clusterFrameNum;
						inst->bcnmag.clusterNeigMap = 0;
						inst->bcnmag.slotSelfMap = 0;
						
						BCNLog_Clear(inst);
					}
					return 0;
				} else if(dat[4] == 0xff) {
					//û�п���bcn slot������ýڵ㣬�޷���������
					inst->bcnmag.clusterFrameNum = 0xff;
					inst->fatherRef = 0xff;
					inst->wait4type = 0;
					BCNLog_Clear(inst);
				}
			} else {
				//��ʱSVC��������Ӧ�����ڵ㣬���²ɼ���Ϣ
				inst->bcnmag.clusterFrameNum = 0xff;
				inst->fatherRef = 0xff;
				inst->wait4type = 0;
				BCNLog_Clear(inst);
			}
		} else if((dat[0] == UWBMAC_FRM_TYPE_TAGS_POS) && (addr == inst->instanceAddress16)) {
			//������չ�����еı�ǩλ����Ϣ
			if(isGateway()) {
				//��ӵ����绺����
				cpy_from_UWBBuf_to_PosNetBuf(dat[3], &dat[4]);
			} else {
				//�����أ������ϴ�
				cpy_to_PosBuf(dat[3], &dat[4]);
			}
		}
	}
	
	//update route table������ȡ����·������µ�����·�ɱ�
    addr = messageData[DESTOF] + (messageData[DESTOF+1] << 8);
    if(addr != inst->instanceAddress16) {
        routeUpdate(addr, srcAddr16, (messageData[DPGAOF] & 0x7f) + 1, messageData[DPGAOF] & 0x80);
    }
    routeUpdate(srcAddr16, srcAddr16, 1, messageData[FLGOF] & BCN_GATEWAY);
	
	if(inst->joinedNet > 0) {
		//�Ѿ��������磬�ռ���Χ�ڵ���Ϣ
		inst->bcnmag.clusterNeigMap |= clustermap;
		inst->bcnmag.clusterSelfMap |= (0x01 << srcClusterFraNum);
		
		if(srcAddr16 == inst->fatherAddr && (!inst->leaderAnchor)) {
			uint8 version = messageData[VEROF] & 0x0f;
			//���ݸ��ڵ�ʱ��ͬ������ʱ��
			int dif = inst->bcnmag.clusterFrameNum - srcClusterFraNum;
			
			inst->sampleNum += 1;	//�ۼƲ�������
			
			inst->fatherRefTime = dw_event->timeStamp;
			inst->instanceTimerEn = 1;
			inst->beaconTXTimeCnt = dw_event->uTimeStamp + dif * inst->BCNslotDuration_ms * 100;
			inst->svcStartStamp32h = dw_event->timeStamp32h + (inst->BCNslots_num - srcClusterFraNum) * inst->BCNfixTime32h;
			if(dif <= 0) {
				inst->beaconTXTimeCnt += inst->sfPeriod_ms * 100; 
			}
			if((version != 0) && (version !=  getVersionOfAncs())) {
				requestAncList();	//�������ػ�վ��������
				debug_printf("get new version:%x\n", messageData[VEROF]);
			} else if((version == 0) && (getVersionOfAncs() != 0)) {
				clearAncPosList();
			}
			version = (messageData[VEROF] >> 4) & 0x0f;
			if((version != 0) && (version !=  getVersionOfArea())) {
				requestAreaData();	//��������Σ����������
				debug_printf("get new version:%x\n", messageData[VEROF]);
			} else if((version == 0) && (getVersionOfArea() != 0)) {
				dangerAreaClear();
			}
		} else if(inst->leaderAnchor) {
			//���صĻ���ͳ�Ʒ���beacon�ĳ����������������ط�����
			//inst->sampleNum += 1;	//�ۼƲ�������
		}
		//�ﵽһ�����������������趨�ڵ������״̬���Ի���δ��ʹ�õ���Դ
		//�Ż���ѡ1����ֱ�����״̬�����ݽ��յ���beacon slot���������ָ����Χ��λ--->�����ʵ��Ч������
		if(inst->sampleNum >= inst->resetSampleNum) {
			inst->bcnmag.clusterSelfMap = (0x01 << inst->bcnmag.clusterFrameNum);
			inst->bcnmag.clusterNeigMap = 0;
			inst->sampleNum = 0;
			BCNLog_Clear(inst);
			
			srand(inst->beaconTXTimeCnt);   			//����������ӣ���ʱ�������Ϊ���ӣ���ֹα�����
			inst->resetSampleNum = (rand() & 0x0f) + 5;	//���������(0~15)֮�䣬Ȼ������10����ֹ�������С
		}
//		else {
//			//���ݽ��յ���beacon slot���������ָ����Χ��λ
//			while(inst->bcnmag.lastRecBcnNum != srcClusterFraNum) {
//				//������������һ����Χ��ѭ��
//				inst->bcnmag.lastRecBcnNum = inst->bcnmag.lastRecBcnNum + 1;
//				if(inst->bcnmag.lastRecBcnNum >= inst->BCNslots_num) {
//					inst->bcnmag.lastRecBcnNum = 0;
//				}
//				
//				if(inst->bcnmag.lastRecBcnNum != inst->bcnmag.clusterFrameNum) {
//					inst->bcnmag.clusterSelfMap &= (~(0x01 << inst->bcnmag.lastRecBcnNum));
//					inst->bcnmag.clusterNeigMap &= (~(0x01 << inst->bcnmag.lastRecBcnNum));
//				}
//			}
//		}
//		inst->bcnmag.lastRecBcnNum = srcClusterFraNum;
		
		//������־
		if(srcClusterFraNum < inst->BCNslots_num) {
			inst->bcnlog[srcClusterFraNum].srcAddr = srcAddr16;
			inst->bcnlog[srcClusterFraNum].clusMap = clustermap;
			inst->bcnlog[srcClusterFraNum].neigMap = neigmap;
		}
		return 0;
	}
	LED_STATUS4 = 1;		//��˵����û�н�������--��Ӧ��ƻƵ�
	
	/******************************************************************************/
	//���²�����Ѱ�Ҳο��ڵ㣬��ͳ������״̬��������, Ϊ����������׼ʱ��
	//��ѡ�Ż�����1����ͨ��ʱ϶���룬����ͨ��·�ɾ���ѡ�����ڵ㣬ʹ�������˸��������������ܰ�
	if(srcClusterFraNum < inst->fatherRef) {
		inst->fatherRef = srcClusterFraNum;
		inst->fatherAddr = srcAddr16;
		inst->sampleNum = 0;
		inst->bcnmag.clusterNeigMap = 0;
		inst->bcnmag.sessionID = messageData[SIDOF];
		inst->wait4ackNum = 0;
		//��Ҫ���²���
		BCNLog_Clear(inst);	//must re-sample!!!!!!!!!
		return 0;
	} else if(srcClusterFraNum == inst->fatherRef) {
		inst->sampleNum += 1;
	} 
		
	inst->bcnmag.clusterNeigMap |= clustermap;
	//������־
	if(srcClusterFraNum < 16) {
		inst->bcnlog[srcClusterFraNum].srcAddr = srcAddr16;
		inst->bcnlog[srcClusterFraNum].clusMap = clustermap;
		inst->bcnlog[srcClusterFraNum].neigMap = neigmap;
		if((inst->bcnlog[srcClusterFraNum].flag & 0x01) == 0) {
			//������Ҫ�ռ���Χ���ٸ��ڵ��Ӧ�����
			inst->bcnlog[srcClusterFraNum].flag |= 0x01;
			inst->wait4ackNum += 1;
		}
	}
	//printf("NeiMap=%x, times=%d\n", inst->bcnmag.clusterNeigMap, inst->sampleNum);
	if(inst->sampleNum > 5 && inst->joinedNet == 0) {
		//�����������
		inst->sampleNum = 0;
		if(prepare_join_request_frame(inst)) {
			//failed, clear data
			inst->bcnmag.clusterFrameNum = 0xff;
			inst->fatherRef = 0xff;
			BCNLog_Clear(inst);
			inst->testAppState = TA_RXE_WAIT;
			return 0;
		}
		inst->bcnmag.clusterNeigMap = 0;
		inst->svcStartStamp32h = dw_event->timeStamp32h + inst->BCNfixTime32h * (inst->BCNslots_num - messageData[CFNOF]);
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 0); //write frame control
		//��SVC�׶�RX on�Լ�time out��units are 1.0256us
		dwt_setrxaftertxdelay(inst->BCNslotDuration_ms * 500);
		//dwt_setrxtimeout(0);  				//disable rx timeout
		//dwt_setpreambledetecttimeout(0); 	//configure preamble timeout
		dwt_setdelayedtrxtime(inst->svcStartStamp32h);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			error_printf("error: join net request error!\n");
			inst->bcnmag.clusterFrameNum = 0xff;
			inst->fatherRef = 0xff;
			BCNLog_Clear(inst);
			return 1;
		}
		info_printf("join net requesting[%d]-num[%d]\n", inst->bcnmag.clusterFrameNum, inst->wait4ackNum);
		
		inst->wait4ack = DWT_RESPONSE_EXPECTED;
		inst->wait4type = UWBMAC_FRM_TYPE_CL_JOIN_CFM;//�ȴ���Χ�ڵ����������Ӧ��
		inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
		inst->previousState = TA_TXSVC_WAIT_SEND;
	}
	return 0;
}

uint8 process_join_request_msg(instance_data_t* inst, event_data_t *dw_event, uint16 srcAddr16, uint8 *messageData) {
	uint8 seat = messageData[REQOF];
	uint16 mask = 0x01 << seat;
	
	inst->testAppState = TA_RXE_WAIT;
	
	//�����ǰ��������û�б�ռ�ã�����Դ�ڵ������λ��û�б�ռ�ã���Ϊ��Դ�ڵ�׼��Ӧ������
	if((inst->joinedNet > 0) && (inst->jcofmsg.clusterLock == 0) && ((inst->bcnmag.clusterSelfMap & mask) == 0)) {
		inst->bcnmag.bcnFlag |= BCN_EXT;
		inst->jcofmsg.chldAddr = srcAddr16;
		inst->jcofmsg.clusterLock = 8;
		inst->jcofmsg.clusterSeat = seat;
		inst->jcofmsg.msgID = UWBMAC_FRM_TYPE_CL_JOIN_CFM;
		
		info_printf("[%x] Req Join[%d]!\n", srcAddr16, seat);
		//inst->bcnmag.clusterSelfMap |= mask; //�����ڸô����ã���Ϊ����Դ�ڵ�δ���ܳɹ���������
	} else if(inst->joinedNet > 0){
		info_printf("[%x] Req Join[%d], But busy!\n", srcAddr16, seat);
	}
	return 0;
}

void send_beacon_msg(instance_data_t* inst, uint8 flag) {
//	uint32 dlyTime = 0;
	if(inst->leaderAnchor) {
		prepare_beacon_frame(inst, BCN_INIT | flag);
		inst->sampleNum += 1;	//�ۼƲ�������
	} else {
		prepare_beacon_frame(inst, flag);
	}
	//����֮ǰ�Ѿ�׼������
	dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
	dwt_writetxfctrl(inst->psduLength, 0, 0); //write frame control
	//��SVC�׶�RX on�Լ�time out��units are 1.0256us
	//dlyTime = inst->BCNfixTimeDly_sy * (inst->bcnmag.clusterSlotNum - inst->bcnmag.clusterFrameNum);
	dwt_setrxaftertxdelay(inst->BCNslotDuration_ms * 500);
	
	dwt_setrxtimeout(0);  					//disable rx timeout
	dwt_setpreambledetecttimeout(0); 		//configure preamble timeout
	
	dwt_starttx(DWT_START_TX_IMMEDIATE); 	//transmit the frame
	
	if(inst->leaderAnchor) {
		inst->instanceTimerEn = 1;
		inst->beaconTXTimeCnt = portGetTickCnt_10us() + 100 * inst->sfPeriod_ms;
	}
	
	inst->testAppState = TA_TX_WAIT_CONF; // wait confirmation
	inst->previousState = TA_TXBCN_WAIT_SEND;
	
	//info_printf("Info:sBeacon sT:%u, cT:%d\n", (u32)dwt_readsystimestamphi32(), portGetTickCnt_10us());
}

//����1����ʶ����վ�������group poll msg��������������Ϣ
//����0��������poll msg
uint8 process_grp_poll_msg(uint16 srcAddr, uint8* recData, event_data_t *event) {
	uint16 *data = (uint16 *)&recData[GP_FLGOF];
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint16 mask = 0x01 << (inst->bcnmag.clusterFrameNum);
	//�ж϶�Ӧcluster�Ƿ���λ 
	if((data[0] & mask) == mask) {
		uint32 timeStamp32 = 0;
		int rxOnTime = 0;
		//��ѯ��ַ���Ƿ��������վ��ַ
		for(inst->idxOfAnc = 0; inst->idxOfAnc < 4; ++inst->idxOfAnc) {
			if(data[2+inst->idxOfAnc] == inst->instanceAddress16)	break;
		}
		if(inst->idxOfAnc >= 4)	return 1;
		
		/********************start����poll msg֡*************************/
		anch_fill_msg_addr(inst, srcAddr, UWBMAC_FRM_TYPE_TWR_POLL);
		inst->msg_f.messageData[PL_FLGOF] =	0;	//��ʱ����Ϊ�޴���
		inst->msg_f.messageData[PL_SMPOF + 0] = inst->bcnmag.nextSlotMap & 0xff;
		inst->msg_f.messageData[PL_SMPOF + 1] = (inst->bcnmag.nextSlotMap >> 8) & 0xff;
		
		inst->psduLength = FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC + TWR_POLL_MSG_LEN;
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 1); //write frame control
		/********************end:����poll msg֡*************************/
		
		/********************start:����poll����ʱ��*********************/
		inst->delayedTRXTime32h = event->timeStamp32h;
		rxOnTime = ((3 - inst->idxOfAnc) * inst->fixedPollDelayAnc32h) / 256 - inst->fwto4PollFrame_sy;
		if(rxOnTime < 0) {
			rxOnTime = 0;
		}
		dwt_setrxaftertxdelay((uint32)rxOnTime);//(((3 - inst->idxOfAnc) * inst->fixedPollDelayAnc32h) / 256 - 400);	//תΪΪ1.0256us
		//dwt_setrxtimeout(0);//dwt_setrxtimeout((uint16)inst->fwto4RespFrame_sy);
		timeStamp32 = (inst->delayedTRXTime32h + inst->fixedOffsetDelay32h + (inst->idxOfAnc + 1) * inst->fixedPollDelayAnc32h) & MASK_TXT_32BIT;
		dwt_setdelayedtrxtime(timeStamp32);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			uint32 curStamp32 = dwt_readsystimestamphi32();
			error_printf("error:time is pass: %lu--%lu\n", timeStamp32, curStamp32);
			return 1;
		}
		inst->twrmag.pollTxTime[0] = timeStamp32;
		inst->twrmag.pollTxTime[0] = ((inst->twrmag.pollTxTime[0] << 8) + inst->txAntennaDelay) & MASK_40BIT;
		inst->twrmag.gpollRxTime[0] = event->timeStamp;
		/********************end:����poll����ʱ��*********************/
		
		inst->wait4ack = DWT_RESPONSE_EXPECTED;
		inst->wait4type = UWBMAC_FRM_TYPE_TWR_RESP;
		inst->wait4ackOfAddr = srcAddr;
		
		//info_printf("Poll-T:%lu, S:%lu\n", timeStamp32, dwt_readsystimestamphi32());
		//printf("rec grpPoll, sending poll\n");
		return 0;
	} else {
		inst->wait4ackOfAddr = 0xffff;
	}
	return 1;
}

//����1����ʶ����վ�������msg��������������Ϣ
//����0��������poll msg
uint8 process_response_msg(uint16 srcAddr, uint8* recData, event_data_t *event) {
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	uint16 mask = 0x01 << recData[RP_DSLOF];
	
	if((inst->wait4type == UWBMAC_FRM_TYPE_TWR_RESP) && (inst->wait4ackOfAddr == srcAddr)) {
		uint8 idxOfAnc = 3 - inst->idxOfAnc;	//��תfinal֡˳��ʹTWR�ƽ��Գ�
		uint32 timeStamp32 = (event->timeStamp32h + (idxOfAnc + 1) * inst->fixedFinalDelayAnc32h) & MASK_TXT_32BIT;
		int rxOnTime = 0;
		inst->twrmag.finalTxTime[0] = timeStamp32;
		inst->twrmag.finalTxTime[0] = ((inst->twrmag.finalTxTime[0] << 8) + inst->txAntennaDelay) & MASK_40BIT; 
		
		/********************start:����final msg֡*************************/
		anch_fill_msg_addr(inst, srcAddr, UWBMAC_FRM_TYPE_TWR_FINAL);
		inst->msg_f.messageData[FN_FLGOF] = ((mask & inst->bcnmag.nextSlotMap) == 0) ? 1 : 0;
		inst->bcnmag.nextSlotMap |= mask;
		//Tx time of Poll message-- or ------Db(plllTxTime - gpollRxTime)
		inst->twrmag.pollTxTime[1] = inst->twrmag.pollTxTime[0] - inst->twrmag.gpollRxTime[0];
		memcpy(&(inst->msg_f.messageData[FN_TPTOF]), (uint8 *)&inst->twrmag.pollTxTime[1], 5);
		//Rx time of response message or ----Rb(respRxTime - plllTxTime)
		inst->twrmag.respRxTime[1] = event->timeStamp - inst->twrmag.pollTxTime[0];
		memcpy(&(inst->msg_f.messageData[FN_RRTOF]), (uint8 *)&inst->twrmag.respRxTime[1], 5);
		//Tx time of final message or -------Db1(finalTxTime - respRxTime)
		inst->twrmag.finalTxTime[1] = inst->twrmag.finalTxTime[0] - event->timeStamp;
		memcpy(&(inst->msg_f.messageData[FN_TFTOF]), (uint8 *)&inst->twrmag.finalTxTime[1], 5);
		
		inst->psduLength = FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC + TWR_FINAL_MSG_LEN;
		dwt_writetxdata(inst->psduLength, (uint8 *)&inst->msg_f, 0);	// write the frame data
		dwt_writetxfctrl(inst->psduLength, 0, 1); 						//write frame control
		
		//���
		//inst->twrmag.gpollRxTime[0] = 0;
		/********************end:����final msg֡*************************/
		
		/********************start:����ʱ��***************************************/
		inst->wait4ackOfAddr = 0xffff;
		inst->wait4type = 0;	//���ȴ�ָ��������Ϣ֡
		
		inst->delayedTRXTime32h = event->timeStamp32h;
		rxOnTime = (((3 - idxOfAnc) * inst->fixedFinalDelayAnc32h) / 256) - inst->fwto4FinalFrame_sy;
		if(rxOnTime < 0) {
			rxOnTime = 0;
		}
		dwt_setrxaftertxdelay((uint32)rxOnTime);//ת��Ϊ1.0256us
		//dwt_setrxtimeout(0);
		dwt_setdelayedtrxtime(timeStamp32);
		if(dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
			uint32 curStamp32 = dwt_readsystimestamphi32();
			error_printf("error:time is pass: %lu--%lu\n", timeStamp32, curStamp32);
			return 1;
		}
		/**********************end:����ʱ��***************************************/
		inst->wait4ack = DWT_RESPONSE_EXPECTED;
		
		//info_printf("Tx:%u, sT:%u\n", (u32)timeStamp32, (u32)dwt_readsystimestamphi32());
		//info_printf("rec resp[%d], sending fin[%x]\n", frame[msgid_index + RP_DSLOF], inst->bcnmag.slotSelfMap);
		return 0;
	}
	return 1;
}

uint8 process_download_req(uint16 scrAddr, uint8* recData) {
	uint16 frameLength = 0;
	uint16 readlength = 0;
	u16 *offset = NULL;
	u16 *ptr = NULL;
	instance_data_t* inst = instance_get_local_structure_ptr(0);
	
	inst->psduLength = frameLength = 100 + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC;
	anch_fill_msg_addr(inst, scrAddr, UWBMAC_FRM_TYPE_DOWNLOAD_RESP);

	//write download data
	inst->msg_f.messageData[MIDOF+1] = recData[1];
	offset = (u16 *)&recData[2];	//data offset which want to read;
	readlength = downloadRead(recData[1], &inst->msg_f.messageData[8], 100-8, *offset);	//read data
	ptr = (u16 *)&inst->msg_f.messageData[6];
	*(ptr--) = readlength;									//current block size
	*ptr = downloadGetSize(recData[1]);			//data size
	inst->msg_f.messageData[MIDOF+2] = recData[2];
	inst->msg_f.messageData[MIDOF+3] = recData[3];
	info_printf("[%d]download %d(%d / %d), len:%d\n", scrAddr, recData[1], *offset, *ptr, readlength);
	
	//write the TX data
	dwt_writetxfctrl(frameLength, 0, 0);
	dwt_writetxdata(frameLength, (uint8 *)  &inst->msg_f, 0) ;	// write the frame data
	dwt_setrxaftertxdelay(0);
	if(dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) == DWT_ERROR) {
		error_printf("error: download error\n");
		return 1;
	}
	inst->wait4ack = DWT_RESPONSE_EXPECTED;
	return 0;
}

void anchor_init(instance_data_t* inst) {
	memcpy(inst->eui64, &inst->instanceAddress16, ADDR_BYTE_SIZE_S);
	dwt_seteui(inst->eui64);
	dwt_setpanid(inst->panID);

	//set source address
	dwt_setaddress16(inst->instanceAddress16);
	//���ݵ�ַ��8λ��������Ѱ�Ҷ�Ӧ�����Ӧ���Ľṹ
	inst->shortAdd_idx = (inst->instanceAddress16 & 0xff);
	//����ַΪ0x0000�Ļ�վ�ݶ�ΪЭ����
	if(inst->instanceAddress16 == 0x0000) {
		inst->leaderAnchor = TRUE;
	} else {
		inst->leaderAnchor = FALSE;
	}
	
	dwt_enableframefilter(DWT_FF_NOTYPE_EN);	//disable filter
	// First time anchor listens we don't do a delayed RX
	dwt_setrxaftertxdelay(0);
	//dwt_setpreambledetecttimeout(0);
	dwt_setrxtimeout(0);
	
	instance_config_frameheader_16bit(inst);
	
	//��ʼ���ø�λ�ɼ�����Ϊ5���������һ�������У������1~63֮��
	inst->resetSampleNum = 5;  
	
	if(inst->leaderAnchor) {
		inst->testAppState = TA_TXBCN_WAIT_SEND;
		inst->joinedNet = 1;		
		inst->bcnmag.sessionID = 0x55;//����������������
		inst->fatherAddr = inst->instanceAddress16;
		inst->bcnmag.clusterSlotNum = 15;	//twr slot�Ļ���������ĿǰΪ15
		inst->bcnmag.clusterFrameNum = 0;
		inst->bcnmag.clusterSelfMap = 0x0001;
		//Э�����ȵ�superFrame�Ŀ�ʼ��,��������Ҳ����ʱһ����֡ʱ�䣬������̫��Ӱ�죬��������и��õķ������Ը�����
		while(portGetTickCnt_10us() % (inst->sfPeriod_ms * 100) != 0);
	} else {
		dwt_setrxaftertxdelay(0);
		dwt_setrxtimeout(0);  //disable rx timeout
		inst->wait4ack = 0;
		inst->sampleNum = 0;
		inst->fatherRef = 0xff;		//ռʱδ֪���ڵ㣬��Ϊ0xff����ʶ�޸��ڵ�
		inst->joinedNet = 0;
		inst->bcnmag.clusterFrameNum = 0;
		inst->bcnmag.clusterSelfMap = 0x0000;
		//change to next state - wait to receive a message
		inst->testAppState = TA_RXE_WAIT ;
	}
	inst->bcnmag.clusterNeigMap = 0x0000;	//��ʼ��������slotδ��ռ�ã���ʼΪ0
	inst->bcnmag.slotSelfMap = 0x0000;	
	inst->bcnmag.nextSlotMap = 0x0000;
	inst->bcnmag.bcnFlag = 0;
	
	inst->jcofmsg.clusterLock = 0;
	
	BCNLog_Clear(inst);
	clearTagPosBuf();
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
		uint16 srcAddress = (((uint16)dw_event.msgu.frame[srcAddr_index+1]) << 8) + dw_event.msgu.frame[srcAddr_index];
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
		//�����ڴ˴����ĳЩ����֡���д����������Ŵ���Ȼ���¼��������
		//����Ƿ���TWR��Ϣ
		switch(dw_event.msgu.frame[msgid_index]) {
			case UWBMAC_FRM_TYPE_TWR_GRP_POLL:
				if(inst->mode == ANCHOR && process_grp_poll_msg(srcAddress, &dw_event.msgu.frame[msgid_index], &dw_event) == 0) {
					//exit this interrupt and notify the application that TX is in progress.
					dw_event.typePend = DWT_SIG_TX_PENDING;
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_TWR_RESP:
				if(inst->mode == ANCHOR && process_response_msg(srcAddress, &dw_event.msgu.frame[msgid_index], &dw_event) == 0) {
					//exit this interrupt and notify the application that TX is in progress
					dw_event.typePend = DWT_SIG_TX_PENDING;
					is_knownframe = 1;
				}
				break;
			case UWBMAC_FRM_TYPE_DOWNLOAD_REQ:
				if(process_download_req(srcAddress, &dw_event.msgu.frame[msgid_index]) == 0) {
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
		//debug_printf("rec unknown frame\n");
		
		//need to re-enable the rx (got unknown frame type)
		anch_handle_error_unknownframe_timeout(&dw_event);
	}
}
// -----------------------------------------------------------------------------------
// the main instance state machine for anchor application
// -----------------------------------------------------------------------------------
int anch_app_run(instance_data_t *inst) {
	int instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
	//get any of the received events from ISR
    int message = instance_peekevent();
	u8 ldebug = 0;
	
    switch (inst->testAppState)
    {
		case TA_INIT:
			if(ldebug)	printf("TA_INIT\n");
			anchor_init(inst);
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			break;
		
		case TA_TXBCN_WAIT_SEND:
			if(ldebug)	printf("TA_TXBCN_WAIT_SEND\n");
			
			routeUpdate(inst->instanceAddress16, inst->instanceAddress16, 0, isGateway());
		
			send_beacon_msg(inst, inst->bcnmag.bcnFlag);
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			break;
		
		case TA_DOWNLOAD_REQ_WAIT_SEND:
			LED_STATUS2 = 0;//ָ����������
			instDone = send_download_req_msg(inst, 0);
			break;
		
		case TA_TX_WAIT_CONF:
		{
			event_data_t* dw_event = instance_getevent(11); //get and clear this event
			//wait for TX done confirmation
			if(ldebug) printf("TA_TX_WAIT_CONF\n");
			if(dw_event->type != DWT_SIG_TX_DONE) {
				instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
                break;
			}
			instDone = INST_DONE_WAIT_FOR_NEXT_EVENT;
			inst->testAppState = TA_RXE_WAIT;
			if(inst->previousState == TA_TXBCN_WAIT_SEND) {
				//������ɺ󣬼���Ƿ������������еû�������һ����������
				if(downloadTaskIsEmpty() == 0) {
					inst->testAppState = TA_DOWNLOAD_REQ_WAIT_SEND;
				} 
				
				//debug_printf("txBea ok\n");
			} else if(inst->previousState == TA_TXPOLL_WAIT_SEND) {
				inst->twrmag.pollTxTime[0] = dw_event->timeStamp;
				debug_printf("txPoll ok\n");
			} else if(inst->previousState == TA_TXFINAL_WAIT_SEND) {
				//debug_printf("txFinal ok:(%llx-%llx)%lld\n", inst->twrmag.finalTxTime[0], dw_event->timeStamp, (inst->twrmag.finalTxTime[0] - dw_event->timeStamp));
				debug_printf("txFinal ok\n");
			} else if(inst->previousState == TA_DOWNLOAD_REQ_WAIT_SEND) {
				debug_printf("txDownReq ok\n");
			} else if(inst->previousState == TA_DOWNLOAD_RESP_WAIT_SEND) {
				debug_printf("txDownResp ok\n");
			}
			break;
		}	
		
		case TA_RXE_WAIT:
			if(ldebug == 1)	printf("TA_RXE_WAIT\n");
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
//					uint8 *srcAddr = &(dw_event->msgu.rxmsg_ss.sourceAddr[0]);
//					uint8 *dstAddr = &(dw_event->msgu.rxmsg_ss.destAddr[0]);
					int msgid = 0;
					uint16 srcAddress16 = dw_event->msgu.rxmsg_ss.sourceAddr[0] + (dw_event->msgu.rxmsg_ss.sourceAddr[1] << 8);
					uint8 *messageData = NULL;
					if(ldebug == 1)	printf("DWT_SIG_RX_OKAY\n");
//					memcpy(&srcAddr[0], &(dw_event->msgu.rxmsg_ss.sourceAddr[0]), ADDR_BYTE_SIZE_S);
//					memcpy(&dstAddr[0], &(dw_event->msgu.rxmsg_ss.destAddr[0]), ADDR_BYTE_SIZE_S);
					msgid = dw_event->msgu.rxmsg_ss.messageData[MIDOF];
					messageData = &(dw_event->msgu.rxmsg_ss.messageData[0]);
//					srcAddress16 = srcAddr[0] + (srcAddr[1] << 8);
					switch(msgid) {
						case UWBMAC_FRM_TYPE_BCN:
							process_beacon_msg(inst, dw_event, srcAddress16, messageData);
							break;
						case UWBMAC_FRM_TYPE_CL_JOIN:
							process_join_request_msg(inst, dw_event, srcAddress16, messageData);
							break;
						case UWBMAC_FRM_TYPE_TWR_GRP_POLL:
							//debug_printf("rec gpoll:%x\n", srcAddress16);
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_TXPOLL_WAIT_SEND;
							}
							if(messageData[GP_QFAOF]){// & 0x80��0������
								//ѡ���һ����վ��������
								//���·���������ؾ��ⷨ��
								//Ŀǰѡ·�����·������Ҫbeacon֡��Ϣ��֧�֣���tagѡ�����·���ڵ�
								uint16 firstAdd = messageData[GP_ADDOF] + (messageData[GP_ADDOF + 1] << 8);
								uint16 elecValue = messageData[GP_UPPOF] + (messageData[GP_UPPOF + 1] << 8);
								if(firstAdd == inst->instanceAddress16) {
									if(isGateway()) {
										printf("I am gateway\n");
										//��������أ�д�����绺����������(1024-x), ���ɷ�50����ǩ��Ϣ
										//small order transform to big order
										ByteOrder_Inverse_Int(&messageData[GP_COROF], NULL, 3);
										addTagInfo(srcAddress16, &messageData[GP_COROF], messageData[GP_QFAOF], elecValue, messageData[GP_SQNOF]);
									} else {
										//д���վ������,(127-x)���ȣ����ɷ�4����ǩ��Ϣ
										//UWB���䲻��Ҫ��С��ת��
										printf("I am not gateway\n");
										addTagPos_ToBuf(srcAddress16, &messageData[GP_COROF], messageData[GP_QFAOF], elecValue, messageData[GP_SQNOF]);
									}
								}
							}
							break;
						case UWBMAC_FRM_TYPE_TWR_RESP:
							//debug_printf("rec resp:%x\n", srcAddress16);
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_TXFINAL_WAIT_SEND;
							}
							break;
						case UWBMAC_FRM_TYPE_DOWNLOAD_REQ:
							//debug_printf("[%d]Download type: %d, %d\n", srcAddress16, messageData[1],  *((uint16*)&messageData[2]));
							if(dw_event->typePend == DWT_SIG_TX_PENDING) {
								inst->testAppState = TA_TX_WAIT_CONF;
								inst->previousState = TA_DOWNLOAD_RESP_WAIT_SEND;
							}
							break;
						case UWBMAC_FRM_TYPE_DOWNLOAD_RESP:
							LED_STATUS2 = 1;//ָ���յ����ػ�Ӧ
							anchor_process_download_data(inst, dw_event, srcAddress16, messageData);
							break;
						default:
							break;
					}
				}
					break;
				default:
					if(message) {
						instance_getevent(20);	//�����δ֪�¼����������
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
		if(SysTick_TimeIsOn_10us(inst->beaconTXTimeCnt)) {
			DWM_DisableEXT_IRQ(); //disable ScenSor IRQ before starting
			
			//������������bcn slotʱ϶����ʱ����������ͨ��
//			if(downloadTaskIsEmpty() == 0) {
//				inst->testAppState = TA_DOWNLOAD_REQ_WAIT_SEND;
//			} else {
//				inst->testAppState = TA_TXBCN_WAIT_SEND;
//			}
			inst->testAppState = TA_TXBCN_WAIT_SEND;
			
			inst->beaconTXTimeCnt += inst->sfPeriod_ms * 100;
			/* Reset RX to properly reinitialise LDE operation. */
			//dwt_rxreset();
			dwt_forcetrxoff(); //disable DW1000
			instance_clearevents(); //clear any events
			
			DWM_EnableEXT_IRQ(); //enable ScenSor IRQ before starting
		}
	}

	while(done == INST_NOT_DONE_YET) {
		done = anch_app_run(inst); // run the communications application
	}
	return 0;
}

