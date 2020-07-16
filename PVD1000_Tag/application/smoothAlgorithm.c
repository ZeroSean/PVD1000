#include "smoothAlgorithm.h"

/*----------------------------------------------------------------------*/
/*       
        Q: provess noise: Q bigger, dynamic response so qucik, steady more bad
        R: meature noise: R bigger, dynamic response so slow, steady better
*************************************************************************/
/*************?????:??*********************************/
#define  MeasureNoise_R		0.03
#define  ProcessNiose_Q		0.01

void kalmanFilter(double *dat, u8 cn) {
	static double x_last[3] = {0,0,0};
	static double p_last[3] = {0,0,0};
	double x_mid;
	double x_now;
	double p_mid;
	double p_now;
	double kg;
	u8 i = 0;
	for(i = 0; i < cn; i++) {
		x_mid = x_last[i];	//x_last=x(k-1|k-1),x_mid=x(k|k-1)
		p_mid = p_last[i] + ProcessNiose_Q; //p_mid=p(k|k-1),p_last=p(k-1|k-1),Q
		kg = p_mid / (p_mid + MeasureNoise_R); //kg?kalman filter
		x_now = x_mid + kg * (dat[i] - x_mid);  //
		
		p_now = (1 - kg) * p_mid;	//covariance
		
		p_last[i] = p_now;
		x_last[i] = x_now;
		
		dat[i] = x_now;
	}
}

void matrix_mul(double *dest, double *m1, double *m2, u8 r, u8 c, u8 k) {
	u8 i = 0, j = 0;
	for(i = 0; i < r; i++) {
		for(j = 0; j < c; j++) {

		}
	}
}

/*KFL-TOA algorithm, more detail refer to paper:"A KFL-TOA UWB Indoor positioning Method for Complex Environment" */
/*return 0: succ, 1: cen't be inversed, 2: invalid argument*/

//state transform noise,
#define W0	0.5			//相当于加速度
#define W1 	0.5

//measure noise
#define V0	0.01
#define V1	0.01

u8 kalFilter_Linearize(double *dat, u8 cn, double T) {
	static u8 init = 0;
	static double x_last[4] = {0.0, 0.0, 0.0, 0.0};
	static double p_last[4][4] = {	{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0}	};
	static double Rw[4][4] = {	{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0},
									{0.0, 0.0, 0.0, 0.0}	};
	static double Rv[2][2] = {	{0.0, 0.0},
								{0.0, 0.0}	};
	double x_mid[4] = {0.0, 0.0, 0.0, 0.0};
	//double x_now[4] = {0.0, 0.0, 0.0, 0.0};
	double p_mid[4][4];
	//double p_now[4][4];
	double kg[4][2];
	double a, b, c, d, A, Tpow2, Tpow3, Tpow4;
	u8 i = 0, j = 0;
	
	if(cn < 2) {
		return 2;	//invalid argument
	}
	
	//step1：init
	if(init == 0) {
		x_last[0] = dat[0];
		x_last[2] = dat[1];
		
		Rv[0][0] = V0 * V0;
		Rv[0][1] = V0 * V1;
		Rv[1][0] = Rv[0][1];
		Rv[1][1] = V1 * V1;
		
		init = 1;
	}
	Tpow2 = T * T;
	Tpow3 = Tpow2 * T;
	Tpow4 = Tpow3 * T;
	
	//calculate noise
	Rw[0][0] = Tpow4 * W0 * W0 / 4;
	Rw[0][1] = Tpow3 * W0 * W0 / 2;
	Rw[0][2] = Tpow4 * W0 * W1 / 4;
	Rw[0][3] = Tpow3 * W0 * W1 / 2;
	
	Rw[1][0] = Rw[0][1]; 			//Tpow3 * W0 * W0 / 2;
	Rw[1][1] = Tpow2 * W0 * W0;
	Rw[1][2] = Rw[0][3]; 			//Tpow3 * W0 * W1 / 2;
	Rw[1][3] = Tpow2 * W0 * W1;
	
	Rw[2][0] = Rw[0][2]; 			//Tpow4 * W0 * W1 / 4;
	Rw[2][1] = Rw[0][3]; 			//Tpow3 * W0 * W1 / 2;
	Rw[2][2] = Tpow4 * W1 * W1 / 4;
	Rw[2][3] = Tpow3 * W1 * W1 / 2;
	
	Rw[3][0] = Rw[0][3]; 			//Tpow3 * W0 * W1 / 2;
	Rw[3][1] = Rw[1][3]; 			//Tpow2 * W0 * W1;
	Rw[3][2] = Rw[2][3];			//Tpow3 * W1 * W1 / 2;
	Rw[3][3] = Tpow2 * W1 * W1;
	
	x_mid[0] = x_last[0] + x_last[1] * T;
	x_mid[1] = x_last[1];
	x_mid[2] = x_last[2] + x_last[3] * T;
	x_mid[3] = x_last[3];
	
	//step3: calculate predict covariance
	p_mid[0][0] = p_last[0][0] + T * p_last[1][0] + T * p_last[0][1] + T * T * p_last[1][1];
	p_mid[0][1] = p_last[0][1] + T * p_last[1][1];
	p_mid[0][2] = p_last[0][2] + T * p_last[1][2] + T * p_last[0][3] + T * T * p_last[1][3];
	p_mid[0][3] = p_last[0][3] + T * p_last[1][3];
	
	p_mid[1][0] = p_last[1][0] + T * p_last[1][1];
	p_mid[1][1] = p_last[1][1];
	p_mid[1][2] = p_last[1][2] + T * p_last[1][3];
	p_mid[1][3] = p_last[1][3];
	
	p_mid[2][0] = p_last[2][0] + T * p_last[3][0] + T * p_last[2][1] + T * T * p_last[3][1];
	p_mid[2][1] = p_last[2][1] + T * p_last[3][1];
	p_mid[2][2] = p_last[2][2] + T * p_last[3][2] + T * p_last[2][3] + T * T * p_last[3][3];
	p_mid[2][3] = p_last[2][3] + T * p_last[3][3];
	
	p_mid[3][0] = p_last[3][0] + T * p_last[3][1];
	p_mid[3][1] = p_last[3][1];
	p_mid[3][2] = p_last[3][2] + T * p_last[3][3];
	p_mid[3][3] = p_last[3][3];
	
	for(i = 0; i < 4; i++) {
		for(j = 0; j < 4; j++) {
			p_mid[i][j] += Rw[i][j];
		}
	}
	
	//step4: calculate gain
	a = p_mid[0][0] + Rv[0][0]; //matrix: {{a, b},{c, d}}
	b = p_mid[0][2] + Rv[0][1];
	c = p_mid[2][0] + Rv[1][0];
	d = p_mid[2][2] + Rv[1][1];
	A = a * d - b * c;
	if(A == 0) {
		return 1;
		//matrix can't be inversed
	}
	for(i = 0; i < 4; i++) {
		kg[i][0] = (d * p_mid[i][0] - c * p_mid[i][2]) / A;
		kg[i][1] = (a * p_mid[i][2] - b * p_mid[i][0]) / A;
	}
	/* kg[0][0] = (d * p_mid[0][0] - c * p_mid[0][2]) / A;
	kg[0][1] = (a * p_mid[0][2] - b * p_mid[0][0]) / A;
	kg[1][0] = (d * p_mid[1][0] - c * p_mid[1][2]) / A;
	kg[1][1] = (a * p_mid[1][2] - b * p_mid[1][0]) / A;
	kg[2][0] = (d * p_mid[2][0] - c * p_mid[2][2]) / A;
	kg[2][1] = (a * p_mid[2][2] - b * p_mid[2][0]) / A;
	kg[3][0] = (d * p_mid[3][0] - c * p_mid[3][2]) / A;
	kg[3][1] = (a * p_mid[3][2] - b * p_mid[3][0]) / A; */
	
	//step5: update state
	for(i = 0; i < 4; i++) {
		x_last[i] = x_mid[i] + kg[i][0] * (dat[0] - x_mid[0]) + kg[i][1] * (dat[1] - x_mid[2]);
	}
	
	//step6: estimate covariance
	for(i = 0; i < 4; i++) {
		for(j = 0; j < 4; j++) {
			p_last[i][j] = p_mid[i][j] - (kg[i][0] * p_mid[0][j] + kg[i][1] * p_mid[2][j]);
		}	
		/* p_last[i][0] = p_mid[i][0] - (kg[i][0] * p_mid[0][0] + kg[i][1] * p_mid[2][0]);
		p_last[i][1] = p_mid[i][1] - (kg[i][0] * p_mid[0][1] + kg[i][1] * p_mid[2][1]);
		p_last[i][2] = p_mid[i][2] - (kg[i][0] * p_mid[0][2] + kg[i][1] * p_mid[2][2]);
		p_last[i][3] = p_mid[i][3] - (kg[i][0] * p_mid[0][3] + kg[i][1] * p_mid[2][3]); */
	}
	dat[0] = x_last[0];	//x direction
	dat[1] = x_last[2];	//y direction
	
	return 0;
}

