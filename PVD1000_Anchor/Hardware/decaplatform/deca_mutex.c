#include "deca_mutex.h"


/*Function: decamutexon()
 * Description: This function should disable interrupts. This is called at the start of a critical section
 * It returns the irq state before disable, this value is used to re-enable in decamutexoff call
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * returns the state of the DW1000 interrupt
 */
decaIrqStatus_t decamutexon(void) {
	decaIrqStatus_t s = DWM_GetEXT_IRQStatus();

	if(s) {
		DWM_DisableEXT_IRQ(); //disable the external interrupt line
	}
	// return state before disable, value is used to re-enable in decamutexoff call
	return s ;   
}

/*Function: decamutexoff()
 * Description: This function should re-enable interrupts, or at least restore their state as returned(&saved) by decamutexon 
 * This is called at the end of a critical section
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * input parameters:	
 * @param s - the state of the DW1000 interrupt as returned by decamutexon
 */
void decamutexoff(decaIrqStatus_t s) {
	//need to check the port state as we can't use level sensitive interrupt on the STM ARM
	if(s) {
		DWM_EnableEXT_IRQ();
	}
}

