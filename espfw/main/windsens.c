
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include "windsens.h"

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

static adc_cali_handle_t adc_calhan;
static adc_oneshot_unit_handle_t ws_adchan;

static portMUX_TYPE wsspinlock = portMUX_INITIALIZER_UNLOCKED;
static long wscounter = 0;
static int lastwsstate = 1;
static int64_t lastwsts = -1;
static int64_t minwstsdiff = -1;
static int64_t wsactualevts = 0;
static esp_timer_handle_t ws_wstimer;

static int64_t gethighrests(void)
{
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t res = (int64_t)t.tv_sec * 1000000LL + (int64_t)t.tv_usec;
  return res;
}

void ws_windspeedtimercb(void * arg)
{
  int curwsstate = gpio_get_level(WSPORT);
  lastwsstate = curwsstate;
  if (curwsstate == 0) { /* We were pulled low, so the aenometer made 1/3 turn */
    int64_t curwsts = wsactualevts;
    taskENTER_CRITICAL_ISR(&wsspinlock);
    wscounter++;
    /* We probably should not use float here, because ESP-IDF does
     * not always save floating point registers when context-switching.
     * So instead we just save the minimum timestamp difference to
     * calculate the peak windspeed later (outside of interrupt context). */
    if (lastwsts > 0) {
      int64_t tsdiff = curwsts - lastwsts;
      if (tsdiff > 0) {
        if ((minwstsdiff < 0) || (tsdiff < minwstsdiff)) {
          /* This is a new minimum */
          minwstsdiff = tsdiff;
        }
      }
    }
    taskEXIT_CRITICAL_ISR(&wsspinlock);
    lastwsts = curwsts;
  }
}

void ws_windspeedirq(void * arg)
{
  // n.b.: We must not LOG in irq context
  /* We're doing debouncing by scheduling a timer. Only if when the
   * timer runs the pin status still is changed, we process it. */
  int curwsstate = gpio_get_level(WSPORT);
  if (curwsstate == lastwsstate) { /* No change? Then there is nothing to do. */
    /* Cancel any potentially pending timer */
    esp_timer_stop(ws_wstimer);
  } else {
    /* Save the timestamp at which the event ACTUALLY occoured so
     * the timer routine can later get it... */
    wsactualevts = gethighrests();
    /* FIXME: is 1000 us a good value? */
    esp_timer_start_once(ws_wstimer, 1000);
  }
}

void ws_init(void)
{
    /* Initialize the GPIO for the wind speed sensor */
    /* The wind speed sensor is connected to GPI34.
     * That pin is also connected to the button on the ESP32-POE-ISO,
     * and pulled high by a 10k resistor on the board.
     * Thus, we should normally read a "1" on the GPIO, and it
     * will temporarily be pulled to 0 on every 1/3rd of an
     * aenometer rotation. */
    esp_timer_create_args_t tca = {
      .callback = ws_windspeedtimercb,
      .arg = NULL,
      .name = "windspeeddebouncetimer",
      .dispatch_method = ESP_TIMER_TASK,
    };
    ESP_ERROR_CHECK(esp_timer_create(&tca, &ws_wstimer));
    esp_err_t iise = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_EDGE);
    if ((iise != ESP_OK) && (iise != ESP_ERR_INVALID_STATE)) {
      ESP_LOGE("windsens.c", "gpio_install_isr_service returned an error. Interrupts will not work. Wind speed cannot be measured.");
    } else {
      ESP_ERROR_CHECK(gpio_isr_handler_add(WSPORT, ws_windspeedirq, NULL));
    }
    gpio_config_t wss = {
      .pin_bit_mask = (1ULL << WSPORT),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = 0,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&wss));

    /* Initialize the ADC for the wind direction sensor */
    /* 12 bit width. */
    /* Maximum attenuation (11 dB) - gives full-scale voltage
     * of 3.9V for the ADC, instead of 1.1V, although it's not
     * good above 2.45V or below 0.15V
     * (src = ESP-IDF-documentation) and still must not exceed
     * the 3.3V pin maximum voltage  */
    adc_oneshot_unit_init_cfg_t unitcfg = {
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitcfg, &ws_adchan));
    adc_oneshot_chan_cfg_t chconf = {
      .bitwidth = ADC_BITWIDTH_12,
      .atten = ADC_ATTEN_DB_11
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ws_adchan, WDADCPORT, &chconf));

    adc_cali_line_fitting_config_t caliconfig = {
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_11,
      .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&caliconfig, &adc_calhan));
    for (int i = 0; i < 4; i++) {
      ESP_LOGI("windsens.c", "Voltagemapping-Table %d/4: %d %d %d %d.",
                i+1, voltagemappingtable[i*4], voltagemappingtable[i*4+1],
                voltagemappingtable[i*4+2], voltagemappingtable[i*4+3]);
    }
}

uint16_t ws_readaenometer(void)
{
    taskENTER_CRITICAL(&wsspinlock);
    uint16_t res = wscounter;
    wscounter = 0;
    taskEXIT_CRITICAL(&wsspinlock);
    return res;
}

float ws_readpeakws(void)
{
    taskENTER_CRITICAL(&wsspinlock);
    int64_t td = minwstsdiff;
    minwstsdiff = -1;
    taskEXIT_CRITICAL(&wsspinlock);
    float res = 0.0;
    if (td > 0) {
      res = (1000000.0 / (double)td) * 2.4;
    }
    return res;
}

uint8_t ws_readwinddirection(void)
{
    uint8_t res = 99;
    uint16_t mindiff = 0xffff;
    int adcv;
    if (adc_oneshot_read(ws_adchan, WDADCPORT, &adcv) != ESP_OK) {
      ESP_LOGE("windsens.c", "adc_oneshot_read returned error!");
      adcv = -9999.99;
    }
    int v;
    if (adc_cali_raw_to_voltage(adc_calhan, adcv, &v) != ESP_OK) {
      ESP_LOGE("windsens.c", "adc_cali_raw_to_voltage returned error!");
      v = -9999.99;
    }
    ESP_LOGI("windsens.c", "Wind vane raw voltage: %d (raw ADV: %d)", v, adcv);
    /* There is one special case: If voltage is way above the
     * expected maximum for the ADC, then apparently only the
     * pullup resistor is connected and the wind vane is unplugged.
     * Unfortunately, the ESP32s ADC is so horribly bad that we cannot
     * reliably distinguish 2.8 and 3.3V, so if we use this cutoff we
     * occasionally invalidate measurements. This is why this is
     * commented out by default. */
    //if (adcv > 4000) {
    //  return 99;  /* no valid measurement */
    //}
    for (int i = 0; i < 16; i++) {
      if (abs((int)v - (int)voltagemappingtable[i]) < mindiff) {
        mindiff = (uint16_t)abs((int)v - (int)voltagemappingtable[i]);
        res = i;
      }
    }
    return res;
}

