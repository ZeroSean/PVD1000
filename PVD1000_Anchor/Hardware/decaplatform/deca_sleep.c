#include "deca_sleep.h"

/*Wrapper function to be used by decadriver. Declared in deca_device_api.h*/
/*keil don't support inline function*/
//__inline deca_sleep(time_ms)
void sleep_ms(unsigned int time_ms) {
	delay_ms(time_ms);
}

/*
__INLINE void deca_sleep(unsigned int time_ms) {
	delay_ms(time_ms);
}
*/

