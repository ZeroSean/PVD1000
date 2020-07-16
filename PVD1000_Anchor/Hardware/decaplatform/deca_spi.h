#ifndef __DECA_SPI_H_
#define __DECA_SPI_H_
/*hardware*/
#include "dwmconfig.h"
/*deca driver*/
#include "deca_types.h"
#include "deca_device_api.h"

#define DECA_MAX_SPI_HEADER_LENGTH		(3)	//max number of bytes in header (for formating & sizing)

#define BUFFLEN		(4096+128)

int openspi(void);
int closespi(void);

#endif
