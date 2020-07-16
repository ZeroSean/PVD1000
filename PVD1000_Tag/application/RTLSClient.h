#ifndef __RTLSCLIENT_H_
#define __RTLSCLIENT_H_

#include "stm32f10x.h"
#include "trilateration.h"
#include "host_usart.h"
#include "smoothAlgorithm.h"

#define SET_ANC_POSITION_SIZE	(70-36)

#define HIS_LENGTH		5
#define MAX_ANCS_NUM	4
typedef struct{
	double x_arr[HIS_LENGTH];
	double y_arr[HIS_LENGTH];
	double z_arr[HIS_LENGTH];
	double av_x, av_y, av_z;	//average
	double fx, fy, fz;			//filter
	double errx_arr[HIS_LENGTH];
	double erry_arr[HIS_LENGTH];
	double errz_arr[HIS_LENGTH];
	double averr_x, averr_y, averr_z;
	double variancex, variancey, variancez;
	double std_x, std_y, std_z;
	u8 arr_idx; //index
	
	u8 count; //rangeCount index
	u8 rangeSeq;
	u8 rangeCount[64];
	int rangeValue[64][MAX_ANCS_NUM];
	//u8 rangeCountM[64];
}tag_rtls_c;

typedef struct{
	u8 id[MAX_ANCS_NUM];
	vec3d anc[MAX_ANCS_NUM];
}anc_struct_c;

u8 updateAncPosition(void *ptr);
u8 setAncPosition(int32_t *p, u8 index);
vec3d * calculateTagLocation(u8 mask, int dist1, int dist2, int dist3, int dist4, int seq, u8 *flag);
u8 getTagLocation(int *distance, u8 mask, u16 *ancAddr, int32_t (*ancPos)[3], u8 ancMask, int32_t *tagPos);



#endif

