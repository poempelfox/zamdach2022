
#ifndef _WINDSENS_H_
#define _WINDSENS_H_

// The windsensor is connected to GPI34, which equals
// RTC_GPIO4.
#define WSPORT GPIO_NUM_34

void ws_init(void);

uint16_t ws_readaenometer(void);

#endif /* _WINDSENS_H_ */

