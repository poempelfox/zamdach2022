
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "windsens.h"

/* the binary blob we load into the ULP.
 * (The ULP code gets compiled automatically during the build process and
 * the binary will be embedded into the firmware image we flash onto the
 * ESP as well, but it needs to be loaded into the ULP by the main
 * application at runtime) */
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

/* See the docs/ directory for instructions on how to wire up the
 * wind sensor assembly */

/* The resistance (in Ohm) of our other resistor in the voltage divider */
#define OTHERRESISTOR 22000
/* How to calculate the expected voltage from the resistor in the voltage divider */
#define VDCALC(r) (uint16_t)((r * 3300UL) / (r + OTHERRESISTOR))
/* Table of resistor values from sensor datasheet.
 * This contains the expected voltages, as they would be
 * measured by a PROPER ADC. (Unfortunately, the ADC on the
 * ESP32 is almost unusable crap, so we'll need to correct
 * for that) */
static const uint16_t voltagemappingtable[16] = {
  VDCALC( 33000), /*    0.0 deg  0  N   */
  VDCALC(  6570), /*   22.5 deg  1      */
  VDCALC(  8200), /*   45.0 deg  2      */
  VDCALC(   891), /*   67.5 deg  3      */
  VDCALC(  1000), /*   90.0 deg  4      */
  VDCALC(   688), /*  112.5 deg  5      */
  VDCALC(  2200), /*  135.0 deg  6      */
  VDCALC(  1410), /*  157.5 deg  7      */
  VDCALC(  3900), /*  180.0 deg  8  S   */
  VDCALC(  3140), /*  202.5 deg  9      */
  VDCALC( 16000), /*  225.0 deg 10      */
  VDCALC( 14120), /*  247.5 deg 11      */
  VDCALC(120000), /*  270.0 deg 12      */
  VDCALC( 42120), /*  292.5 deg 13      */
  VDCALC( 64900), /*  315.0 deg 14      */
  VDCALC( 21880)  /*  337.5 deg 15      */
};

static esp_adc_cal_characteristics_t * adc_characteristics;


void ws_init(void)
{
    /* Initialize the GPIO for the wind speed sensor
     * and the ULP that will process the pin changes from that GPIO. */
    rtc_gpio_init(GPIO_NUM_34);
    rtc_gpio_set_direction(GPIO_NUM_34, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(GPIO_NUM_34);
    rtc_gpio_pulldown_dis(GPIO_NUM_34);
    ESP_ERROR_CHECK(ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t)));
    ESP_ERROR_CHECK(ulp_run(&ulp_entry - RTC_SLOW_MEM));

    /* Initialize the ADC for the wind direction sensor */
    adc_power_acquire();
    adc1_config_width(ADC_WIDTH_BIT_12);
    /* Maximum attenuation (11 dB) - gives full-scale voltage
     * of 3.9V for the ADC, instead of 1.1V, although it's not
     * good above 2.45V or below 0.15V
     * (src = ESP-IDF-documentation) and still must not exceed
     * the 3.3V pin maximum voltage  */
    adc1_config_channel_atten(WDADCPORT, ADC_ATTEN_DB_11);
    adc_characteristics = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_characteristics);
    for (int i = 0; i < 4; i++) {
      ESP_LOGI("windsens.c", "Voltagemapping-Table %d/4: %d %d %d %d.",
                i+1, voltagemappingtable[i*4], voltagemappingtable[i*4+1],
                voltagemappingtable[i*4+2], voltagemappingtable[i*4+3]);
    }
}

uint16_t ws_readaenometer(void)
{
    uint16_t res = (ulp_windsenscounter & 0xffff);
    ulp_windsenscounter = 0;
    return res;
}

uint8_t ws_readwinddirection(void)
{
    uint8_t res = 99;
    uint16_t mindiff = 0xffff;
    uint32_t adcv = adc1_get_raw(WDADCPORT);
    uint32_t v = esp_adc_cal_raw_to_voltage(adcv, adc_characteristics);
    ESP_LOGI("windsens.c", "Wind vane raw voltage: %d (raw ADV: %d)", v, adcv);
    /* There is one special case: If voltage is way above the
     * expected maximum for the ADC, then apparently only the
     * pullup resistor is connected and the wind vane is unplugged. */
    if (v > 3200) {
      return 99;  /* no valid measurement */
    }
    for (int i = 0; i < 16; i++) {
      if (abs((int)v - (int)voltagemappingtable[i]) < mindiff) {
        mindiff = (uint16_t)abs((int)v - (int)voltagemappingtable[i]);
        res = i;
      }
    }
    return res;
}

