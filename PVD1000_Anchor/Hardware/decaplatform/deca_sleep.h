#ifndef __DECA_SLEEP_H_
#define __DECA_SLEEP_H_

#include "deca_device_api.h"
#include "delay.h"

/*keil don't support inline function, so use define*/
#define deca_sleep(time_ms) delay_ms(time_ms)

/*! -----------------------------------------------------------------------------------
 * Function: sleep_ms()
 *
 * Wait for a given amount of time.
 * This implementation is designed for a single threaded application and is blocking.
 *
 * param  time_ms  time to wait in milliseconds
 */
void sleep_ms(unsigned int time_ms);


#endif
