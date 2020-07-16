#ifndef __SMOOTH_ALGORITHM_H_
#define __SMOOTH_ALGORITHM_H_

#include <stdio.h>
#include <math.h>
#include <stm32f10x.h>


void kalmanFilter(double *dat, u8 cn);

u8 kalFilter_Linearize(double *dat, u8 cn, double T);


#endif


