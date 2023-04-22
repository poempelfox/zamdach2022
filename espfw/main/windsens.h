
#ifndef _WINDSENS_H_
#define _WINDSENS_H_

// The wind speed sensor is connected to GPI34, which equals
// RTC_GPIO4.
#define WSPORT GPIO_NUM_34

// The wind direction sensor is connected to GPI35, which
// equals ADC1_CH7
#define WDPORT GPIO_NUM_35
#define WDADCPORT ADC_CHANNEL_7

void ws_init(void);

/* 1 count per second would equal 2.4 km/h wind speed */
uint16_t ws_readaenometer(void);

/* This returns a value between 0 and 15, or 99 on error.
 * 0 is North (0 degrees), and it goes clockwise from there
 * in 22.5 degrees increments. So for example:
 * 0 = 0 deg = N; 1 = 22.5 deg = NNE ; 2 = 45 deg = NE ;
 * 4 = 90 deg = E ; 8 = 180 deg = S ; 12 = 270 deg = W  */
uint8_t ws_readwinddirection(void);

#endif /* _WINDSENS_H_ */

