#ifndef __TRILATERATION_H_
#define __TRILATERATION_H_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TRIL_3SPHERES 3
#define TRIL_4SPHERES 4

#define CM_ERR_ADDED (20)

#define ERR_TRIL_CONCENTRIC		(-1)
#define ERR_TRIL_COLINEAR 		(-2)
#define ERR_TRIL_SQRTNEGNUMB	(-3)
#define ERR_TRIL_NOINTERSECTION	(-4)
#define ERR_TRIL_NEEDMORESPHERE	(-5)
#define ERR_TRIL_CANT_INVERSE	(-6)


typedef struct{
	double x;
	double y;
	double z;
}vec3d;

vec3d * vdiff(vec3d *const rv, const vec3d *v1, const vec3d *v2);
vec3d * vsum(vec3d *const rv, const vec3d *v1, const vec3d *v2);
vec3d * vmul(vec3d *const rv, const vec3d *v, const double n);
vec3d * vdiv(vec3d *const rv, const vec3d *v, const double n);
double vdist(const vec3d *v1, const vec3d *v2);
double vnorm(const vec3d *v);
double vdot(const vec3d *v1, const vec3d *v2);
vec3d * vcross(vec3d *const rv, const vec3d *v1, const vec3d *v2);

int threeDimen_locate(vec3d *const solution, int *const nosolution_count, vec3d *p, double *r, int use4thAnchor);
int GetLocation(vec3d *const best_solution, int use4thAnchor, vec3d *anchorArray, int *distanceArray);
int SingleDimLocate(vec3d *const solution, vec3d** ancPos, double *dist);
int lsm_locate(vec3d *const solution, const vec3d *p, double *r);

#endif
