
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "windsens.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

void ws_init(void)
{
    /* Initialize ULP */
    rtc_gpio_init(GPIO_NUM_34);
    rtc_gpio_set_direction(GPIO_NUM_34, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(GPIO_NUM_34);
    rtc_gpio_pulldown_dis(GPIO_NUM_34);
    ESP_ERROR_CHECK(ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t)));
    ESP_ERROR_CHECK(ulp_run(&ulp_entry - RTC_SLOW_MEM));
}

uint16_t ws_readaenometer(void)
{
    uint16_t res = (ulp_windsenscounter & 0xffff);
    ulp_windsenscounter = 0;
    return res;
}

