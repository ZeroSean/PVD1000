#ifndef __NETPROTO_H_
#define __NETPROTO_H_

#include "dma_uart2.h"
#include "dma_uart3.h"
#include "anc_version.h"
#include "delay.h"
#include "anchors.h"
#include "instance.h"

#define RESEND_DURATION		(3 * 1000) //60s
#define SENDPOS_DURATION	(5) //5ms

#define NC_POWERLINK		0x00	//power link ctrl code
#define NC_SYNCLOCK			0x01	//sync clock ctrl code
#define NC_TAGINFO			0xDA	//tag information(position, wanring state) ctrl code
#define NC_DANGERAREA		0xC2	//danger area data ctrl code
#define NC_ANCINFO			0xC3	//anchor information(position) ctrl code

#define START_CODE			0x68	//protocol start code
#define END_CODE			0x16	//protocol end code

#define MAX_BUFFER_LENGTH	1024
#define GATEWAYID_LENGTH	6		//gateway id buffer length
#define DATASIZE_LENGTH		2
#define CHECKCODE_PRESIZE	3
#define CHECKCODE_STARTIN	7
#define MIN_STATELENGTH		(1 + GATEWAYID_LENGTH + 1 + DATASIZE_LENGTH)
#define MIN_PROTOLENGTH		(MIN_STATELENGTH + 2)	//data area is null
#define MAX_DATALENGTH		(MAX_BUFFER_LENGTH - MIN_STATELENGTH - 2) //remain for data

#define CLOCK_SIZE			6
#define TIMESTAMP_OFFSET 	(0)	//时间戳在data中的偏移
#define TIMESTAMP_LENGTH	(6)	//长度
#define TAG_NUM_OFFSET		(6)	//偏移
#define TAG_NUM_LENGTH		(1)

#define ERR_TMOUT			1
#define ERR_UNDCODE			2
#define ERR_BFOVER			3
#define ERR_NOSPACE			4
#define ERR_RECLENGTH		5
#define ERR_FCODE			6
#define ERR_CHECKCODE		7
#define ERR_PROCESS			8
#define ERR_LENGTH			9
#define ERR_TOOSHORT		10

/*net communication control fd*/
#define FD_CLOSE	0
#define UART2_FD	2
#define UART3_FD	3

typedef struct {
	u8 fcode;
	u8 gatewayID[GATEWAYID_LENGTH];
	u8 ctrl;
	u8 length[DATASIZE_LENGTH];
	u8 data[MAX_DATALENGTH + 2];	//2 is remain for checkcode and endcode
	u8 checkcode;					//ctrl + length + data (byte calculate)
	u8 endcode;
	uint16_t putptr;				//point to buffer which can be pu data
	uint16_t maxptr;				//max length: point to buffer which can be pu data
}NET_MSG_S;

void netpro_init(void);

void ByteOrder_Inverse_Int(u8 *src, u8 *dest, u16 count);	//ByteOrder_Inverse

void netpro_run(void);

u8 netpro_open(u8 fd);
u8 netpro_close(u8 fd);

void addTagInfo(u16 addr, u8 *pos, u8 alarm, u8 seqNum);
void cpy_from_UWBBuf_to_PosNetBuf(uint8 num, uint8* dat);

#endif
