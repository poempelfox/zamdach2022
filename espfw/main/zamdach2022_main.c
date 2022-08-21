/* ZAMDACH2022 main "app"
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "math.h"
#include "nvs_flash.h"
#include "time.h"
#include "sdkconfig.h"
#include "secrets.h"
#include "i2c.h"
#include "lps25hb.h"
#include "ltr390.h"
#include "network.h"
#include "rg15.h"
#include "sht4x.h"
#include "submit.h"
#include "windsens.h"

static const char *TAG = "zamdach2022";

char chipid[30] = "ZAMDACH-UNSET";

#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

double reducedairpressurecalc(double press)
{
  /* This is a FIXME.
   * I was unable to find out which formula DWD uses.
   * And there are a gazillion variants, because if you wanted to do this entirely
   * correctly, you would need to know temperatures at every height level.
   * Simpler formulas assume temperature does not change with height, but
   * that still leaves you with the problem of WHICH temperature to chose.
   * A constant one? An average one for every month of the year? etc... */
  /* formula taken from a random arduino library. */
  int altitude = 279; /* That is what Wikipedia says for Erlangen */
  return (press / pow((1 - (altitude * 0.0000225577)), 5.25588));
}


void app_main(void)
{
    time_t lastmeasts = 0;
    time_t lastaenomread = 0;
    /* Initialize the windsensor. This includes initializing the ULP
     * Co-processor, we let it count the number of signals received
     * from the windsensor / aenometer. */
    ws_init();
    /* WiFi does not work without this because... who knows, who cares. */
    nvs_flash_init();
    /* Get our ChipID */
    {
      uint8_t mainmac[6];
      if (esp_efuse_mac_get_custom(mainmac) != ESP_OK) {
        /* No custom MAC in EFUSE3, use default MAC */
        esp_read_mac(mainmac, ESP_MAC_WIFI_STA);
      }
      sprintf(chipid, "ZAMDACH-%02x%02x%02x%02x%02x%02x",
                      mainmac[0], mainmac[1], mainmac[2],
                      mainmac[3], mainmac[4], mainmac[5]);
      ESP_LOGI(TAG, "My Chip-ID: %s", chipid);
    }

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    /* Configure our (2) I2C-ports */
    i2cport_init();

    network_prepare();

    lps25hb_init(1);
    ltr390_init(1);
    rg15_init();
    sht4x_init(1);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Mainly to give the RG15 time to
    // process our initialization sequence.

    while (1) {
      if (((time(NULL) - lastmeasts) >= 60)
        || (lastmeasts == 0)) { /* time() starts at 0 on startup, so we need this special case. */
        lastmeasts = time(NULL);

        /* Request update from the sensors that don't autoupdate all the time */
        rg15_requestread();
        sht4x_startmeas();

        network_on(); /* Turn network on, i.e. connect to WiFi */

        /* slight delay to allow the RG15 to reply */
        vTaskDelay(pdMS_TO_TICKS(1111));
        /* Read all the sensors */
        float press = lps25hb_readpressure();
        float uvind = ltr390_readuv();
        float raing = rg15_readraincount();
        uint16_t wsctr = ws_readaenometer();
        /* We'll need this extra timestamp to calculate windspeed from number of pulses */
        time_t curaenomread = time(NULL);
        uint8_t wsdir = ws_readwinddirection();
        struct sht4xdata temphum;
        sht4x_read(&temphum);

        /* potentially wait for up to 4 more seconds if we haven't got an
         * IP address yet */
        xEventGroupWaitBits(network_event_group, NETWORK_CONNECTED_BIT,
                            pdFALSE, pdFALSE,
                            (4000 / portTICK_PERIOD_MS));
        if (press > 0) {
          ESP_LOGI(TAG, "That converts to a measured pressure of %.3f hPa, or a calculated pressure of %.3f hPa at sea level.",
                        press, reducedairpressurecalc(press));
          /* submit that measurement */
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_PRESSURE, "pressure", press);
        }

        if (raing > -0.1) {
          ESP_LOGI(TAG, "Rain: %.3f mm", raing);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_RAINGAUGE1, "precipitation", raing);
        }

        if (lastaenomread != 0) { /* Ignore the first read on startup, but other */
          /* than that, we really have no way of telling if a reading is valid or not */
          int tsdif = curaenomread - lastaenomread;
          /* calculate Wind Speed in km/h from the number of impulses and the timestamp difference */
          float windspeed = 2.4 * wsctr / tsdif;
          ESP_LOGI(TAG, "Wind speed: %.1f km/h", windspeed);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_WINDSPEED, "windspeed", windspeed);
        }
        lastaenomread = curaenomread;

        if (wsdir < 16) { /* Only Range 0-15 is valid */
          char * winddirmap[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
          ESP_LOGI(TAG, "Wind direction: %d (%s)", wsdir, winddirmap[wsdir]);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_WINDDIR, "winddirection", (float)wsdir * 22.5);
        }

        if (temphum.valid > 0) {
          ESP_LOGI(TAG, "Temperature: %.2f degC (raw: %x)", temphum.temp, temphum.tempraw);
          ESP_LOGI(TAG, "Humidity: %.2f %% (raw: %x)", temphum.hum, temphum.humraw);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_TEMPERATURE, "temperature", temphum.temp);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_HUMIDITY, "humidity", temphum.hum);
        }

        if (uvind >= 0.0) {
          ESP_LOGI(TAG, "UV-Index: %.2f", uvind);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_UV, "uv", uvind);
        }

        network_off();
        long howmuchtosleep = (lastmeasts + 60) - time(NULL) - 1;
#ifdef CONFIG_ZAMDACH_USEWIFI
        ESP_LOGI(TAG, "will now enter sleep mode for %ld seconds", howmuchtosleep);
        if (howmuchtosleep > 0) {
          /* This is given in microseconds */
          esp_sleep_enable_timer_wakeup(howmuchtosleep * (int64_t)1000000);
          esp_light_sleep_start();
        }
#else /* !CONFIG_ZAMDACH_USEWIFI - we use Ethernet */
        ESP_LOGI(TAG, "will now wait for %ld seconds (no sleep with POE)", howmuchtosleep);
        vTaskDelay(pdMS_TO_TICKS(howmuchtosleep * 1000));
#endif /* !CONFIG_ZAMDACH_USEWIFI */
      } else { // Delay for a short time
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

}
