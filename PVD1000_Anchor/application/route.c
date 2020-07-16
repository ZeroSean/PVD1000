#include "route.h"

#define ROUTE_MAX_SIZE	(50)

static int next = -1;
static int size = 0;
static RouteElement table[ROUTE_MAX_SIZE];
static RouteElement uploadGateway = {0xffff, 0xffff, 0xff, 0};

int routeSize(void) {
	return size;
}

int routeEmpty(void) {
	return (size == 0) ? 0 : 1;
}

static RouteElement * find(uint16_t dest) {
    int left = 0, right = size - 1;

    while(left <= right) {
        int mid = left + (right - left) / 2;

        if(table[mid].dstID == dest) {
            return &table[mid];
        } else if(table[mid].dstID > dest) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return NULL;
}

//待优化方案：增加定时清除指定路由项功能
void routeUpdate(uint16_t dest, uint16_t out, uint8_t depth, uint8_t isGateway) {
	RouteElement *element = find(dest);

    if(element != NULL) {
		//已存在则直接更新
        if(depth <= element->depth) {
            element->depth = depth;
            element->outID = out;
            element->isGateway = isGateway;
        }
    } else if(size < ROUTE_MAX_SIZE){
        int i = size - 1;

        for(; i >= 0; --i) {
            if(table[i].dstID > dest) {
                table[i + 1] = table[i];
            } else {
                break;
            }
        }
        table[i + 1].dstID = dest;
        table[i + 1].depth = depth;
        table[i + 1].outID = out;
        table[i + 1].isGateway = isGateway;

        element = &table[i + 1];

        size += 1;
    } else {
		printf("route table no has empty space, dest route: %d\n", dest);
    }

    //维护最短路径网关
    if(isGateway && uploadGateway.depth >= depth) {
        memcpy(&uploadGateway, element, sizeof(RouteElement));
    } else if((isGateway == 0) && (uploadGateway.dstID == dest)) {
        int min = -1, i = 0;

        for(; i < size; ++i) {
            if(table[i].isGateway) {
                if(min == -1 || table[min].depth > table[i].depth) {
                    min = i;
                }
            }
        }

        if(min == -1) {
            uploadGateway.isGateway = 0;
            uploadGateway.depth = 0xff;
			uploadGateway.dstID = 0xffff;
            uploadGateway.outID = 0xffff;
        } else {
            memcpy(&uploadGateway, &table[min], sizeof(RouteElement));
        }
    }
}

const RouteElement *routeCfind(uint16_t dest) {
	return find(dest);
}

const RouteElement * routeAt(uint16_t index) {
    if(index < size) {
        return &table[index];
    }
    return NULL;
}

int getOutPort(uint16_t dest) {
    RouteElement * element = find(dest);
    if(element) {
        return element->outID;
    } else {
        return -1;
    }
}

//迭代器
const RouteElement * getNextElement(void) {
    if(size > 0) {
        next = (next + 1) % size;
        return &table[next];
    } else {
        return &uploadGateway;
    }
}

const RouteElement * findShortestGateway(void) {
	return &uploadGateway;
}

uint8_t  getGatewayDepth(void) {
	return uploadGateway.depth;
}

uint16_t getGatewayAddr(void) {
	return uploadGateway.dstID;
}

uint16_t getGatewayOut(void) {
	return uploadGateway.outID;
}


