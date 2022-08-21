
#ifndef _LTR390_H_
#define _LTR390_H_

#include "driver/i2c.h" /* Needed for i2c_port_t */

void ltr390_init(i2c_port_t port);
double ltr390_readuv(void);

#endif /* _LTR390_H_ */

