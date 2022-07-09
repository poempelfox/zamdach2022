
#ifndef _LPS25HB_H_
#define _LPS25HB_H_

#include "driver/i2c.h" /* Needed for i2c_port_t */

void lps25hb_init(i2c_port_t port);
double lps25hb_readpressure(void);

#endif /* _LPS25HB_H_ */

