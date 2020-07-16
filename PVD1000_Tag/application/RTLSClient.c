#include "RTLSClient.h"

u8 myRTLS_State = 0xf0;
u8 myRTLSPrint_Counter = 0;
tag_rtls_c myTagRTLS_c;
anc_struct_c	myAnc_c;
vec3d myTagPos_c;

#define RTLS_ANCPOSITION_ISSET	0x01
#define RTLS_ANCPOS_ISSET_PRINT	0x10
#define RTLS_ANC_ISENOUGH		0x02
#define RTLS_ANC_ISENOUGH_PRINT	0x20
#define RTLS_PRINT_ERR			0x80

u8 updateAncPosition(void *ptr) {
	char *data = (char *)ptr;
	int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
	if((data[0] == 's' || data[0] == 'S') && (data[1] == 'a' || data[1] == 'A') && data[2] == ':') {
		sscanf((data+3), "(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)", &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4);
		myAnc_c.anc[0].x = x1 / 1000.0;
		myAnc_c.anc[0].y = y1 / 1000.0;
		myAnc_c.anc[0].z = z1 / 1000.0;
		myAnc_c.anc[1].x = x2 / 1000.0;
		myAnc_c.anc[1].y = y2 / 1000.0;
		myAnc_c.anc[1].z = z2 / 1000.0;
		myAnc_c.anc[2].x = x3 / 1000.0;
		myAnc_c.anc[2].y = y3 / 1000.0;
		myAnc_c.anc[2].z = z3 / 1000.0;
		myAnc_c.anc[3].x = x4 / 1000.0;
		myAnc_c.anc[3].y = y4 / 1000.0;
		myAnc_c.anc[3].z = z4 / 1000.0;
		myRTLS_State |= RTLS_ANCPOSITION_ISSET;
		myRTLSPrint_Counter = 0;
		printf("rec:(%d,%d,%d),(%d,%d,%d),(%d,%d,%d),(%d,%d,%d)\n", x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4);
		return 0;
	}
	return 1;
}

u8 setAncPosition(int32_t *p, u8 index) {
	if(index >= MAX_ANCS_NUM) {
		return 1;
	}
	myAnc_c.anc[index].x = p[0] / 1000.0;
	myAnc_c.anc[index].y = p[1] / 1000.0;
	myAnc_c.anc[index].z = p[2] / 1000.0;
	myRTLS_State |= RTLS_ANCPOSITION_ISSET;
	myRTLSPrint_Counter = 0;
	return 0;
}

//输入长度为2的vec3d数组
void posAvgSmooth(vec3d * const pos) {
	myTagRTLS_c.av_x = myTagRTLS_c.av_x + (pos[0].x - myTagRTLS_c.x_arr[myTagRTLS_c.count]) / HIS_LENGTH;
	myTagRTLS_c.av_y = myTagRTLS_c.av_y + (pos[0].y - myTagRTLS_c.y_arr[myTagRTLS_c.count]) / HIS_LENGTH;
	myTagRTLS_c.av_z = myTagRTLS_c.av_z + (pos[0].z - myTagRTLS_c.z_arr[myTagRTLS_c.count]) / HIS_LENGTH;
	myTagRTLS_c.x_arr[myTagRTLS_c.count] = pos[0].x;
	myTagRTLS_c.y_arr[myTagRTLS_c.count] = pos[0].y;
	myTagRTLS_c.z_arr[myTagRTLS_c.count] = pos[0].z;
	if((++myTagRTLS_c.count) >= HIS_LENGTH) {
		myTagRTLS_c.count = 0;
	}
	pos[1].x = myTagRTLS_c.av_x;
	pos[1].y = myTagRTLS_c.av_y;
	pos[1].z = myTagRTLS_c.av_z;
}

//返回
//成功，返回基站个数，失败，返回0
u8 getTagLocation(int *distance, u8 mask, u16 *ancAddr, int32_t (*ancPos)[3], u8 ancMask, int32_t *tagPos) {
	u8 i = 0, idx = 0;
	int error_count = 0, result = -10;
	vec3d pos[2];
	vec3d ancpos[4];
	vec3d *ancposPtr[4] = {0,0,0,0};
	double tagToanc_dis[4] = {0,0,0,0};
	u8 ancIdx[4] = {0, 1, 2, 3};
	
	for(i = 0; i < 4; ++i) {
		if(((0x1 << i) & mask) && ((0x1 << i) & ancMask)) {
			//寻找到接收到信号的基站坐标和对应的距离值
			ancpos[idx].x = ancPos[i][0] / 1000.0;
			ancpos[idx].y = ancPos[i][1] / 1000.0;
			ancpos[idx].z = ancPos[i][2] / 1000.0;
			tagToanc_dis[idx] = distance[i] / 1000.0;
			ancposPtr[idx] = &ancpos[idx];
			ancIdx[idx] = i;
			++idx;
		}
	}
	if(idx == 1) {
		tagPos[0] = ancAddr[ancIdx[0]];
		tagPos[1] = 0;
		tagPos[2] = distance[ancIdx[0]];
		debug_printf("only one:(%x--%d)\n", tagPos[0], tagPos[2]);
		return idx;
	} else if(idx == 2) {
		tagPos[0] = ancAddr[ancIdx[0]];
		tagPos[1] = ancAddr[ancIdx[1]];
		tagPos[2] = SingleDimLocate(&pos[0], &ancposPtr[0], tagToanc_dis);
		tagPos[3] = (pos[0].x * 1000) / 1;
		tagPos[4] = (pos[0].y * 1000) / 1;
		tagPos[5] = (pos[0].z * 1000) / 1;
		debug_printf("only two:(%x-->%x--%d)\n", tagPos[0], tagPos[1], tagPos[2]);
		return idx;
	} else if(idx == 3) {
		result = threeDimen_locate(&pos[0], &error_count, ancpos, tagToanc_dis, 0);
	} else if(idx == 4) {
		//result = threeDimen_locate(&pos, &error_count, ancpos, tagToanc_dis, 1);
		result = lsm_locate(&pos[0], ancpos, tagToanc_dis);
	}
	if(result >= 0) { 
		posAvgSmooth(pos);
		tagPos[0] = (pos[1].x * 1000) / 1;
		tagPos[1] = (pos[1].y * 1000) / 1;
		tagPos[2] = (pos[1].z * 1000) / 1;
		tagPos[3] = tagPos[0];
		tagPos[4] = tagPos[1];
		tagPos[5] = tagPos[2];
		debug_printf("l[%d](%.2f, %.2f, %.2f)\n", idx, pos[1].x, pos[1].y, pos[1].z);
		return idx;
	} else {
		debug_printf("l[%d] error[%d]\n", idx, result);
	}
	return 0;
}

vec3d * calculateTagLocation(u8 mask, int dist1, int dist2, int dist3, int dist4, int seq, u8 *flag) {
	u8 seq_i, i, j;
	int result;
	static u8 counter = 0;//控制打印速度
	vec3d pos2;
//	int dis[4] = {0,0,0,0};
	
	vec3d *ancpos[4] = {NULL, NULL, NULL, NULL};
	double tagToanc_dis[4] = {0,0,0,0};
	
	counter = (counter + 1) & 60;
	
	if(counter == 0) debug_printf("D:(%.2f, %.2f, %.2f, %.2f)\n", dist1 / 1000.0, dist2 / 1000.0, dist3 / 1000.0, dist4 / 1000.0);
	
	seq_i = seq & 0x3f;		//64 - 1
	myTagRTLS_c.rangeSeq = seq_i;
	myTagRTLS_c.rangeCount[seq_i] = 0;
	
	myTagRTLS_c.rangeValue[seq_i][0] = dist1;
	myTagRTLS_c.rangeValue[seq_i][1] = dist2;
	myTagRTLS_c.rangeValue[seq_i][2] = dist3;
	myTagRTLS_c.rangeValue[seq_i][3] = dist4;
	
//	dis[0] = dist1;
//	dis[1] = dist2;
//	dis[2] = dist3;
//	dis[3] = dist4;
	
	//check the mask and process the tag - anchor ranges
	
	for(i = 0, j = 0; i < MAX_ANCS_NUM; i++) {
		if((0x1 << i) & mask) {
			myTagRTLS_c.rangeCount[seq_i]++;
			
			//寻找到接收到信号的基站坐标和对应的距离值
			ancpos[j] = &myAnc_c.anc[i];
			tagToanc_dis[j++] = myTagRTLS_c.rangeValue[seq_i][i] / 1000.0;
		} else {
			myTagRTLS_c.rangeValue[seq_i][i & 0x3] = 0;
		}
	}
	myRTLSPrint_Counter = (myRTLSPrint_Counter + 1) % 100;
	if((myRTLS_State & RTLS_ANCPOSITION_ISSET) == 0) {
		if(myRTLSPrint_Counter == 1) {
			printf("Anc position no be set, can't location!\n");		
		}
		return NULL;
	}
	if(myTagRTLS_c.rangeCount[seq_i] == 2) {
		//one dimension location 
//		if(myRTLSPrint_Counter == 1) {
//			printf("Less than 3 anchor, can't location!\n");
//		}
		result = SingleDimLocate(&myTagPos_c, &ancpos[0], tagToanc_dis);
		myTagRTLS_c.av_x = myTagRTLS_c.av_x + (myTagPos_c.x - myTagRTLS_c.x_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.av_y = myTagRTLS_c.av_y + (myTagPos_c.y - myTagRTLS_c.y_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.av_z = myTagRTLS_c.av_z + (myTagPos_c.z - myTagRTLS_c.z_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.x_arr[myTagRTLS_c.count] = myTagPos_c.x;
		myTagRTLS_c.y_arr[myTagRTLS_c.count] = myTagPos_c.y;
		myTagRTLS_c.z_arr[myTagRTLS_c.count] = myTagPos_c.z;
		if((++myTagRTLS_c.count) >= HIS_LENGTH) {
			myTagRTLS_c.count = 0;
		}
		myTagPos_c.x = myTagRTLS_c.av_x;
		myTagPos_c.y = myTagRTLS_c.av_y;
		myTagPos_c.z = myTagRTLS_c.av_z;
		
		*flag = 1;
		
		if(counter == 0) {
			debug_printf("SD(%.3f, %.3f, %.3f)\n", myTagPos_c.x, myTagPos_c.y, myTagPos_c.z);
		}
		return &myTagPos_c;
	}
	
	result = GetLocation(&myTagPos_c, 0, &myAnc_c.anc[0], &myTagRTLS_c.rangeValue[seq_i][0]);
	if(result >= 0) {
		//debug_printf("L:R(%.3f, %.3f, %.3f) ", myTagPos_c.x, myTagPos_c.y, myTagPos_c.z);
		myTagRTLS_c.av_x = myTagRTLS_c.av_x + (myTagPos_c.x - myTagRTLS_c.x_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.av_y = myTagRTLS_c.av_y + (myTagPos_c.y - myTagRTLS_c.y_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.av_z = myTagRTLS_c.av_z + (myTagPos_c.z - myTagRTLS_c.z_arr[myTagRTLS_c.count]) / HIS_LENGTH;
		myTagRTLS_c.x_arr[myTagRTLS_c.count] = myTagPos_c.x;
		myTagRTLS_c.y_arr[myTagRTLS_c.count] = myTagPos_c.y;
		myTagRTLS_c.z_arr[myTagRTLS_c.count] = myTagPos_c.z;
		if((++myTagRTLS_c.count) >= HIS_LENGTH) {
			myTagRTLS_c.count = 0;
		}
		
		if(counter == 0) debug_printf("A(%.3f, %.3f, %.3f) ", myTagRTLS_c.av_x, myTagRTLS_c.av_y, myTagRTLS_c.av_z);
		myTagPos_c.x = myTagRTLS_c.av_x;
		myTagPos_c.y = myTagRTLS_c.av_y;
		myTagPos_c.z = myTagRTLS_c.av_z;
		*flag = 2;
		
//		kalmanFilter((double *)&myTagPos_c, 3);
//		debug_printf("K(%.3f, %.3f, %.3f) ", myTagPos_c.x, myTagPos_c.y, myTagPos_c.z);
		
		//(myTagRTLS_c.rangeCount[seq_i] >= 4 ? 1 : 0)
		result = GetLocation(&pos2, (myTagRTLS_c.rangeCount[seq_i] >= 4 ? 1 : 0), &myAnc_c.anc[0], &myTagRTLS_c.rangeValue[seq_i][0]);
		if(result >= 0) {
			myTagPos_c.z = pos2.z;
			if(counter == 0) debug_printf("T(%.3f, %.3f, %.3f) ", pos2.x, pos2.y, pos2.z);
		}	

//		for(i = 0; i < 4; i++) {
//			if(seq_i == 0) {
//				seq_i = 0x3f;
//			} else {
//				seq_i--;
//			}
//			dis[0] += myTagRTLS_c.rangeValue[seq_i][0];
//			dis[1] += myTagRTLS_c.rangeValue[seq_i][1];
//			dis[2] += myTagRTLS_c.rangeValue[seq_i][2];
//			dis[3] += myTagRTLS_c.rangeValue[seq_i][3];
//		}
//		dis[0] /= 5;
//		dis[1] /= 5;
//		dis[2] /= 5;
//		dis[3] /= 5;
//		result = GetLocation(&pos2, 0, &myAnc_c.anc[0], dis);
//		debug_printf("S(%.3f, %.3f, %.3f) ", pos2.x, pos2.y, pos2.z);
//		result = GetLocation(&pos2, 1, &myAnc_c.anc[0], dis);
//		debug_printf("ST(%.3f, %.3f, %.3f)", pos2.x, pos2.y, pos2.z);
		
		if(counter == 0) debug_printf("\n");
		return &myTagPos_c;
	} else {
		return NULL;
	}
}

