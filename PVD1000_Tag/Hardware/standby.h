#ifndef __STAND_BY_H_
#define __STAND_BY_H_

#include "stm32f10x.h"
#include "stmflash.h"

#include "deca_device_api.h"
#include "delay.h"


u16 check_standby(void);
void Sys_Standby(void);
void Sys_Enter_Standby(void);



#endif
