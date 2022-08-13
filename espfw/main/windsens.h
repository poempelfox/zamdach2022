
#ifndef _WINDSENS_H_
#define _WINDSENS_H_

// The wind speed sensor is connected to GPI34, which equals
// RTC_GPIO4.
#define WSPORT GPIO_NUM_34

// The wind direction sensor is connected to GPI35, which
// equals ADC1_CH7
#define WDPORT GPIO_NUM_35
#define WDADCPORT ADC1_CHANNEL_7

void ws_init(void);

uint16_t ws_readaenometer(void);
uint8_t ws_readwinddirection(void);

#endif /* _WINDSENS_H_ */

