#ifndef __ROUTE_H_
#define __ROUTE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f10x.h"

typedef struct RouteElementS{
	uint16_t dstID;			
	uint16_t outID;
	uint8_t  depth;
	uint8_t  isGateway;
}RouteElement;

int routeSize(void);
int routeEmpty(void);
void routeUpdate(uint16_t dest, uint16_t out, uint8_t depth, uint8_t isGateway);
const RouteElement *routeCfind(uint16_t dest);
const RouteElement * routeAt(uint16_t index);
int getOutPort(uint16_t dest);

//µü´úÆ÷
const RouteElement * getNextElement(void);

const RouteElement * findShortestGateway(void);
uint8_t  getGatewayDepth(void);
uint16_t getGatewayAddr(void);
uint16_t getGatewayOut(void);


#endif

