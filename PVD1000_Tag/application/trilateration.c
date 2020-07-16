#include <string.h>
#include "trilateration.h"


/*largest nongegative number still considered zero*/
#define MAXZERO 0.001

vec3d * vdiff(vec3d *const rv, const vec3d *v1, const vec3d *v2) {
	rv->x = v1->x - v2->x;
	rv->y = v1->y - v2->y;
	rv->z = v1->z - v2->z;
	return rv;
}

vec3d * vsum(vec3d *const rv, const vec3d *v1, const vec3d *v2) {
	rv->x = v1->x + v2->x;
	rv->y = v1->y +	v2->y;
	rv->z = v1->z + v2->z;
	return rv;
}

vec3d * vmul(vec3d *const rv, const vec3d *v, const double n) {
	rv->x = v->x * n;
	rv->y = v->y * n;
	rv->z = v->z * n;
	return rv;
}

vec3d * vdiv(vec3d *const rv, const vec3d *v, const double n) {
	rv->x = v->x / n;
	rv->y = v->y / n;
	rv->z = v->z / n;
	return rv;
}

double vdist(const vec3d *v1, const vec3d *v2) {
	double xd = v1->x - v2->x;
	double yd = v1->y - v2->y;
	double zd = v1->z - v2->z;
	return sqrt(xd*xd + yd*yd + zd*zd);
}

double vdistpow2(const vec3d *v1, const vec3d *v2) {
	double xd = v1->x - v2->x;
	double yd = v1->y - v2->y;
	double zd = v1->z - v2->z;
	return (xd*xd + yd*yd + zd*zd);
}

double vnorm(const vec3d *v) {
	return sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
}

double vdot(const vec3d *v1, const vec3d *v2) {
	return (v1->x*v2->x + v1->y*v2->y + v1->z*v2->z);
}

vec3d * vcross(vec3d *const rv, const vec3d *v1, const vec3d *v2) {
	rv->x = v1->y*v2->z - v1->z*v2->y;
	rv->y = v1->z*v2->x - v1->x*v2->z;
	rv->z = v1->x*v2->y - v1->y*v2->x;
	return rv;
}

/*calculate the unit vector of v1-v2, return the length of v1-v2*/
double vdiffunit(vec3d *const rv, const vec3d *v1, const vec3d *v2) {
	double h;
	rv->x = v1->x - v2->x;
	rv->y = v1->y - v2->y;
	rv->z = v1->z - v2->z;
	h = sqrt(rv->x*rv->x + rv->y*rv->y + rv->z*rv->z);
	rv->x /=h;
	rv->y /=h;
	rv->z /=h;
	return h;
}

/*Return the GDOP (Geometric Dilution of Precision) rate between 0-1*/
/*Lower GDOP rate means better precision of intersection*/
double gdoprate(const vec3d *tag, const vec3d *p1, const vec3d *p2, const vec3d *p3) {
	vec3d t1, t2, t3;
	double gdop1, gdop2, gdop3, maxgdop;
	
	vdiffunit(&t1, p1, tag);
	vdiffunit(&t2, p2, tag);
	vdiffunit(&t3, p3, tag);
	
	gdop1 = fabs(vdot(&t1, &t2));
	gdop2 = fabs(vdot(&t2, &t3));
	gdop3 = fabs(vdot(&t3, &t1));
	
	if(gdop1 < gdop2) 
		maxgdop = gdop2;
	else 
		maxgdop = gdop1;
	
	if(maxgdop < gdop3)
		maxgdop = gdop3;
	
	return maxgdop;
}

/*Intersecting a sphere sc with radius of r, with a line p1-p2.
 *return zero if success, negative error otherwise.
 *mu1 & mu2 are constant to find points of intersection.
*/
int sphereline(const vec3d *p1, const vec3d *p2, const vec3d *sc, double r, double *const mu1, double *const mu2) {
	double a, b, c;
	double bb4ac;
	vec3d dp;
	/*vector (p2-p1), gradient*/
	dp.x = p2->x - p1->x;
	dp.y = p2->y - p1->y;
	dp.z = p2->z - p1->z;
	
	a = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
	
	b = 2 * (dp.x * (p1->x - sc->x) + dp.y * (p1->y - sc->y) + dp.z * (p1->z - sc->z));
	
	c = p1->x*p1->x +  p1->y* p1->y +  p1->z* p1->z;
	c += sc->x*sc->x + sc->y*sc->y + sc->z*sc->z;
	c -= 2 * (p1->x*sc->x + p1->y*sc->y + p1->z*sc->z);
	c -= r*r;
	
	bb4ac = b*b - 4*a*c;
	if(a == 0 || bb4ac < 0) {
		*mu1 = 0;
		*mu2 = 0;
		return -1;
	}
	
	*mu1 = (-b + sqrt(bb4ac)) / (2 * a);
	*mu1 = (-b - sqrt(bb4ac)) / (2 * a);
	
	return 0;
}

int SingleDimLocate(vec3d *const solution, vec3d** ancPos, double *dist) {
	vec3d ex, t1;
	double d, mu;
	double cosx = 0;
	
	d = vdiffunit(&ex, ancPos[1], ancPos[0]); //uint vector with respect to p1 (new coordinate system)
	/*cosines law*/
	//mu是标签在俩个基站连线上的投影点到基站[0]的距离
	mu = d / 2.0 + (dist[0]*dist[0] - dist[1]*dist[1]) / (2.0 * d);
	cosx = mu / dist[0];
	if(cosx > 1.0 || cosx < -1.0) {
		//适当缩减mu的大小
		mu = mu / cosx;
		if(cosx < 0.0) {
			mu = -mu;
		}
	}
	//solution = mu * ex
	vmul(solution, &ex, mu);
	vsum(solution, solution, ancPos[0]);
	
	return ((mu * 1000) / 1);
}

/*
* Return TRIL_3SPHERES if it is performed using 3 spheres;
* For TRIL_3SPHERES, there are two solutions: solution[0] and solution[1];
*
* return negative number for other errors;
*
* the last parameter is the largest nonnegative number considered zero;
* it is somewhat analogous to mathine epsilon.
*/
int trilateration(vec3d *const solution, const vec3d *p, double *r, const double maxzero, int use4thAnchor, int ignNegtive) {
	vec3d ex, ey, ez, t1, t2, temp;
	double h, i, j, x, y, z, t;
	double mu, mu1, mu2;
	double cosx = 0;
	int result;
	
	/*Find two points form the three spheres*/
	h = vdiffunit(&ex, (p+1), (p+0)); //uint vector with respect to p1 (new coordinate system)
	//printf("ex:(%.2f, %.2f, %.2f) ", ex.x,ex.y, ex.z);
	
	/*t1 = p3 - p1, t2 = ex.(p3 - p1)ex*/
	vdiff(&t1, (p+2), (p+0));	//vector p13
	i = vdot(&ex, &t1);	//the scalar of t1 on the ex direction
	vmul(&t2, &ex, i);	//colinear vector to p13 with the length of i
	
	/*ey = (t1 - t2), t = |t1 - t2|*/
	vdiff(&ey, &t1, &t2);	//vector t21 perpendicular to t1
	t = vnorm(&ey);			//scalar t21
	if( t > maxzero) {
		/*ey = (t1 - t2) / |t1 - t2|*/
		vdiv(&ey, &ey, t);	//uint vector ey with respect to p1 (new coordinate system)
		
		/*j = ey . (p3 - p1)*/
		j = vdot(&ey, &t1);	//scalar t1 on the ey direction
	} else {
		j = 0.0;
	}
	
	//printf("ey:(%.2f, %.2f, %.2f) ", ey.x, ey.y, ey.z);
	
	/*Note: t < maxzero implies j = 0.0; which means 3 Anchor colinear*/
	if(fabs(j) < maxzero) {
		/*Is point p1 + (r1 along the axis) the intersection?*/
		vsum(&t2, (p+0), vmul(&temp, &ex, r[0]));
		if( fabs(vdist((p+1), &t2) - r[1]) <= maxzero && fabs(vdist((p+2), &t2) - r[2]) <= maxzero ) {
			if(solution) 
				solution[0] = t2;
				solution[1] = t2;
			return TRIL_3SPHERES;
		}
		
		/*Is point p1 - (r1 along the axis) the intersection?*/
		vsum(&t2, (p+0), vmul(&temp, &ex, -r[0]));
		if( fabs(vdist((p+1), &t2) - r[1]) <= maxzero && fabs(vdist((p+2), &t2) - r[2]) <= maxzero ) {
			if(solution) 
				solution[0] = t2;
				solution[1] = t2;
			return TRIL_3SPHERES;
		}
		/*p1, p2, p3 are colinear with more than one solution*/
		//printf("\nerr:ERR_TRIL_COLINEAR\n");
		return ERR_TRIL_COLINEAR;
	}
	
	/*ez = ex x ey*/
	vcross(&ez, &ex, &ey);
	/*cosines law*/
	x = (r[0]*r[0] - r[1]*r[1]) / (2.0*h) + h / 2.0;
	cosx = x / r[0];	//矫正
	if(cosx > 1.0 || cosx < -1.0) {
		if(cosx < 0.0) {
			cosx = -cosx;
		}
		x = x / cosx;
	}
	y = (r[0]*r[0] - r[2]*r[2] + i*i) / (2.0*j) + j / 2.0 - x * i / j;
	cosx = y / r[0];
	if(cosx > 1.0 || cosx < -1.0) {
		if(cosx < 0.0) {
			cosx = -cosx;
		}
		y = y / cosx;
	}
	
	//printf("dx: %.2f, dy: %.2f", x,y);
	//printf("(%.2f, %.2f, %.2f, %.2f", r[0],r[1],r[2],r[3]);
	/**********************************/
//	vdiff(&temp, (p+2), (p+0));
//	h = vnorm(&temp);
//	mu = (r[0]*r[0] + h*h - r[2]*r[2]) / (2*r[0]*h);
//	mu2 = 1 - mu*mu;
	
	z = r[0]*r[0] - x*x - y*y;
	if(z < -maxzero && !ignNegtive) {
		/*the solution is invalid, square root of negative number*/
		//printf("\nerr:ERR_TRIL_SQRTNEGNUMB\n");
		//printf("  err: %.3f\n", z);
		return ERR_TRIL_SQRTNEGNUMB;
	} else if(z > 0.0) {
		z = sqrt(z);
	} else {
		z = 0.0;
	}
	
//	if(mu2 >= 0) {
//		z = sqrt(mu2)*r[0];
//	}
	
	/*t2 = p1 + x ex + y ey*/
	vsum(&t2, (p+0), vmul(&temp, &ex, x));
	vsum(&t2, &t2, vmul(&temp, &ey, y));
	
	/*result = p1 + x ex + y ey + z ez*/
	if(solution) {
		vsum(&solution[0], &t2, vmul(&temp, &ez, z));
		vsum(&solution[1], &t2, vmul(&temp, &ez, -z));
	}
	/*End of finding two points form the 3 spheres*/
	/*result1 and result2 are solutions, otherwise return error*/
	if(use4thAnchor == 0) {
		return TRIL_3SPHERES;
	}
	
	/**************add to calculate the z: 2018.10.8*************/
	vdiff(&ex, (p+3), (p+0));
	h = vnorm(&ex);
	if(h <= maxzero) {
		return TRIL_3SPHERES;
	}
	
	vdiff(&ex, (p+3), (p+1));
	h = vnorm(&ex);
	if(h <= maxzero) {
		return TRIL_3SPHERES;
	}
	
	vdiff(&ex, (p+3), (p+2));
	h = vnorm(&ex);
	if(h <= maxzero) {
		return TRIL_3SPHERES;
	}
	
	/*确保result1到p4的距离最近*/
	vdiff(&t2, (p+3), (solution+0));
	h = vnorm(&t2);
	vdiff(&t2, (p+3), (solution+1));
	i = vnorm(&t2);
	if(h > i) {
		temp = solution[0];
		solution[0] = solution[1];
		solution[1] = temp;
	}
	result = 1;
	i = 0;
	h = r[3] - 0.4;
	while(result && i < 15) {
		result = sphereline(&solution[0], &solution[1], (p+3), h, &mu1, &mu2);
		h += 0.1;
		i++;
	}
	
	if(result) {
		return TRIL_3SPHERES;
	} else {
		vdiff(&ex, &solution[1], &solution[0]);
		h = vnorm(&ex);
		vdiv(&ex, &ex, h);
		
		if(mu1 < 0 && mu2 < 0) {
			if(fabs(mu1) <= fabs(mu2))	mu = mu1; else mu = mu2;
			mu = 0.5 * mu;
			vmul(&t2, &ex, mu*h);
			vsum(&solution[0], &solution[0], &t2);
		} else if((mu1 < 0 && mu2 > 1)||(mu2 < 0 && mu1 > 1)) {
			if(mu1 > mu2)	mu = mu1; else mu = mu2;

			vmul(&t2, &ex, mu*h);
			vsum(&t2, &solution[0], &t2);
			
			vdiff(&t1, &solution[1], &t2);
			vmul(&t1, &t1, 0.5);
			vsum(&solution[0], &t2, &t1);
		} else if(((mu1>0&&mu1<1)&&(mu2<0||mu2>1)) || ((mu2>0&&mu2<1)&&(mu1<0||mu1>1))) {
			if(mu1 > 0 && mu1 <= 1)	mu = mu1; else mu = mu2;
			if(mu < 0.5) mu -= 0.5*mu; else mu -= 0.5*(1 - mu);
		
			vmul(&t2, &ex, mu*h);
			vsum(&solution[0], &solution[0], &t2);		
		} else if(mu1 == mu2) {
			mu = mu1;
			if(mu <= 0.25) mu -= 0.5*mu;
			else if(mu <= 0.5)	mu -= 0.5*(0.5-mu);
			else if(mu <= 0.75)	mu -= 0.5*(mu-0.5);
			else mu -= 0.5*(1-mu);
			
			vmul(&t2, &ex, mu*h);
			vsum(&solution[0], &solution[0], &t2);
		} else {
			mu = 0.5 * (mu1 + mu2);
			
			vmul(&t2, &ex, mu*h);
			vsum(&solution[0], &solution[0], &t2);
		}
	}
	return TRIL_4SPHERES;
}

/*find the solution*/
int threeDimen_locate(vec3d *const solution, int *const nosolution_count, vec3d *p, double *r, int use4thAnchor) {
	int overlook_count;
	double rr1[4], added_cm = 0;
	int sucess, result;
	int excahnge_count = 0, i = 0;
//    uint8_t excahnge_idx[7][2] = {
//            {1, 2},
//            {0, 1},
//            {1, 2},
//            {0, 1},
//            {1, 2},
//    };
	
	sucess = 0;
	
	for(excahnge_count = 0; excahnge_count < 6; ++excahnge_count) {
        overlook_count = 0;
        do{
            //奇数递减，偶数递增
            if(overlook_count & 0x01) {
                added_cm = -0.05 * (overlook_count + 1) / 2;
            } else {
                added_cm = 0.05 * overlook_count / 2;
            }
            for(i = 0; i < 4; ++i) {
                rr1[i] = r[i] + added_cm;
            }
			
			result = trilateration(solution, p, rr1, MAXZERO, use4thAnchor, 0);
			switch(result) {
				case TRIL_3SPHERES:
					sucess = 1;
					*nosolution_count = overlook_count;
					break;
				case TRIL_4SPHERES:
					*nosolution_count = overlook_count;
					sucess = 1;
					break;
				default:
					overlook_count++;
					break;
			}
			
		}while(!sucess && (overlook_count <= CM_ERR_ADDED));
	
		if(sucess) {
            break;
        } else {
            int one = 1, two = 2;
            double temp = 0;
            vec3d vec;
            if(excahnge_count & 0x01) {
                one = 0;
                two = 1;
            }

            temp = r[one];
            r[one] = r[two];
            r[two] = temp;
            memcpy(&vec, p+one, sizeof(vec3d));
            memcpy(p+one, p+two, sizeof(vec3d));
            memcpy(p+two, &vec, sizeof(vec3d));
		}
		
	}
	
	//若以上方法无法解出坐标，则以忽略z轴精度为代价算出较可靠的坐标
	if(result == ERR_TRIL_SQRTNEGNUMB) {
		result = trilateration(solution, p, r, MAXZERO, use4thAnchor, 1);
	}
	
	return result;
}

//最小二乘法定位,要求输入到4个基站的距离，基站数目越多越好，这里针对的是4个基站的特化版
int lsm_locate(vec3d *const solution, const vec3d *p, double *r) {
	double A[3][3];	//3x3系数矩阵
	double Ainverse[3][3];
	double Avalue = 0;
	double D[3];	//3x1矩阵
	int result = 0, i = 0;
	double d4 = vdot(&p[3], &p[3]) - r[3] * r[3];
	
	for(i = 0; i < 3; ++i) {
		A[i][0] = 2 * (p[i].x - p[3].x);
		A[i][1] = 2 * (p[i].y - p[3].y);
		A[i][2] = 2 * (p[i].z - p[3].z);
		D[i] = vdot(&p[i], &p[i]) - r[i] * r[i] - d4;
	}
	Ainverse[0][0] = A[1][1] * A[2][2] - A[1][2] * A[2][1];
	Ainverse[0][1] = A[0][2] * A[2][1] - A[0][1] * A[2][2];
	Ainverse[0][2] = A[0][1] * A[1][2] - A[0][2] * A[1][1];
	Ainverse[1][0] = A[1][2] * A[2][0] - A[1][0] * A[2][2];
	Ainverse[1][1] = A[0][0] * A[2][2] - A[0][2] * A[2][0];
	Ainverse[1][2] = A[0][2] * A[1][0] - A[0][0] * A[1][2];
	Ainverse[2][0] = A[1][0] * A[2][1] - A[1][1] * A[2][0];
	Ainverse[2][1] = A[0][1] * A[2][0] - A[0][0] * A[2][1];
	Ainverse[2][2] = A[0][0] * A[1][1] - A[0][1] * A[1][0];
	
	Avalue = Ainverse[0][0] * A[0][0] + Ainverse[0][1] * A[1][0] + Ainverse[0][2] * A[2][0];
	if((Avalue > 0.001) || (Avalue < -0.001)) {
		solution->x = vdot((vec3d*)&Ainverse[0], (vec3d*)D) / Avalue;
		solution->y = vdot((vec3d*)&Ainverse[1], (vec3d*)D) / Avalue;
		solution->z = vdot((vec3d*)&Ainverse[2], (vec3d*)D) / Avalue;
		result = 0;
	} else {
		result = ERR_TRIL_CANT_INVERSE;
	}
	return result;
}

int GetLocation(vec3d *const best_solution, int use4thAnchor, vec3d *anchorArray, int *distanceArray) {
	int result, error_counter;
	vec3d solution[2];
	vec3d anchors[4];
	int i;
	double r[4];
	double dist1, dist2;
	
	r[0] = (double) distanceArray[0] / 1000.0;
	r[1] = (double) distanceArray[1] / 1000.0;
	r[2] = (double) distanceArray[2] / 1000.0;
	r[3] = (double) distanceArray[3] / 1000.0;
	
	/*get the location solution using 3 spheres*/
	result = threeDimen_locate(solution, &error_counter, anchorArray, r, use4thAnchor);
	
	if(result >= 0) {
		if(use4thAnchor == 1) {		
			/*if have 4 ranging, then use 4th anchor to pick solution closest to it*/
//			dist1 = vdist((solution+0), (anchorArray+3));
//			dist2 = vdist((solution+1), (anchorArray+3));
//			
//			if(fabs(dist1-r[3]) < fabs(dist2-r[3]))
//				*best_solution = solution[0];
//			else
//				*best_solution = solution[1];
			*best_solution = solution[0];
			
			//save the x, y and then calculate the z using the fouth anchor;
			anchors[0].x =anchorArray[1].z;
			anchors[0].y =anchorArray[1].x;
			anchors[0].z =anchorArray[1].y;
			
//			anchors[0].x = 0.5;
//			anchors[0].y = 8.050;
//			anchors[0].z = 0;
			
			anchors[1].x =anchorArray[3].z;
			anchors[1].y =anchorArray[3].x;
			anchors[1].z =anchorArray[3].y;
			
			anchors[2].x =anchorArray[2].z;
			anchors[2].y =anchorArray[2].x;
			anchors[2].z =anchorArray[2].y;
			
			anchors[3].x =anchorArray[0].z;
			anchors[3].y =anchorArray[0].x;
			anchors[3].z =anchorArray[0].y;
			
			dist1 = r[0];
			r[0] = r[1];
			r[1] = r[3];
			r[3] = dist1;

			result = threeDimen_locate(solution, &error_counter, anchors, r, 0);
			if(result >= 0) {
				//best_solution->z = solution[0].x;
				best_solution->z = solution[0].x;
			}
		} else {
			/*assume tag is above the anchor(1,2,3)*/
//			if(solution[0].z < solution[1].z)
//				*best_solution = solution[0];
//			else
//				*best_solution = solution[1];
			*best_solution = solution[0];
		}
	}
	
	return result;
}
