
#ifndef _TSL2591_H_
#define _TSL2591_H_

#include "driver/i2c.h" /* Needed for i2c_port_t */

void tsl2591_init(i2c_port_t port);
double tsl2591_readlux(void);

#endif /* _TSL2591_H_ */

