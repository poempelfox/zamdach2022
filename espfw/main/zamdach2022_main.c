/* ZAMDACH2022 main "app"
*/
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_sleep.h>
#include <math.h>
#include <nvs_flash.h>
#include <time.h>
#include <esp_ota_ops.h>
#include <esp_sntp.h>
#include "secrets.h"
#include "i2c.h"
#include "lps25hb.h"
#include "ltr390.h"
#include "network.h"
#include "rg15.h"
#include "sen50.h"
#include "sht4x.h"
#include "submit.h"
#include "webserver.h"
#include "windsens.h"

static const char *TAG = "zamdach2022";

/* Global / Exported variables, used to provide the webserver.
 * struct ev is defined in webserver.h for practical reasons. */
char chipid[30] = "ZAMDACH-UNSET";
struct ev evs[2];
int activeevs = 0;
/* Has the firmware been marked as "good" yet, or is ist still pending
 * verification? */
int pendingfwverify = 0;
/* How often has the measured humidity been "too high"? */
long too_wet_ctr = 0;
/* And how many percent are "too high"? We use 90% because Sensiron uses
 * that for "way too high" in its documentation, but it might be an idea to
 * lower this to e.g. 80%. */
#define TOOWETTHRESHOLD 90.0
int forcesht4xheater = 0;
/* If we turn on the heater, how many times in a row do we do it?
 * We should do it a few times in short succession, to generate a large
 * temperature delta, that is way better for removing creep than repeating
 * a lower temperature delta more often. */
#define HEATERITS 3

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
    memset(evs, 0, sizeof(evs));
    time_t lastmeasts = 0;
    time_t lastsht4xheat = 0;
    time_t lastanemomread = 0;
    /* Initialize the windsensor. */
    ws_init();
    /* This is in all OTA-Update examples, so I consider it mandatory.
     * Also, WiFi will not work without nvs_flash_init. */
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
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
    
    network_prepare();

    /* Configure our (2) I2C-ports and then the sensors */
    i2cport_init();
    lps25hb_init(1);
    ltr390_init(1);
    rg15_init();
    sen50_init(0);
    sen50_startmeas(); /* FIXME Perhaps we don't want this on all the time. */
    sht4x_init(1);

    /* We do NTP to provide useful timestamps in our webserver output. */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp2.fau.de");
    esp_sntp_setservername(1, "ntp3.fau.de");
    esp_sntp_init();

    /* In case we were OTA-updating, we set this fact in a variable for the
     * webserver. Someone will need to click "Keep this firmware" in the
     * webinterface to mark the updated firmware image as good, otherwise we
     * will roll back to the previous and known working version on the next
     * reset. */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        pendingfwverify = 1;
      }
    }

    /* now start the webserver */
    webserver_start();

    vTaskDelay(pdMS_TO_TICKS(3000)); /* Mainly to give the RG15 a chance to */
    /* process our initialization sequence, though that doesn't always work. */
    /* Wait for up to 4 more seconds to connect to WiFi and get an IP */
    EventBits_t eb = xEventGroupWaitBits(network_event_group,
                                         NETWORK_CONNECTED_BIT,
                                         pdFALSE, pdFALSE,
                                         (4000 / portTICK_PERIOD_MS));
    if ((eb & NETWORK_CONNECTED_BIT) == NETWORK_CONNECTED_BIT) {
      ESP_LOGI(TAG, "Successfully connected to network.");
    } else {
      ESP_LOGW(TAG, "Warning: Could not connect to network. This is probably not good.");
    }

    while (1) {
      if (lastmeasts > time(NULL)) { /* This should not be possible, but we've
         * seen time(NULL) temporarily report insane timestamps far in the
         * future - not sure why, possibly after a watchdog reset, but in any
         * case, lets try to work around it: */
        ESP_LOGE(TAG, "Lets do the time warp again: Time jumped backwards - trying to cope.");
        lastmeasts = time(NULL); // We should return to normal measurements in 60s
      }
      if (((time(NULL) - lastmeasts) >= 60)
        || (lastmeasts == 0)) { /* time() starts at 0 on startup, so we need this special case. */
        lastmeasts = time(NULL);

        /* Request update from the sensors that don't autoupdate all the time */
        rg15_requestread();
        sht4x_startmeas();
        /* Read UV index and switch to ambient light measurement */
        float uvind = ltr390_readuv();
        ltr390_startalmeas();

        /* slight delay to allow the RG15 to reply */
        vTaskDelay(pdMS_TO_TICKS(1111));

        /* Read all the sensors */
        float press = lps25hb_readpressure();
        float raing = rg15_readraincount();
        uint16_t wsctr = ws_readanemometer();
        /* We'll need this extra timestamp to calculate windspeed from number of pulses */
        time_t curanemomread = time(NULL);
        uint8_t wsdir = ws_readwinddirection();
        struct sht4xdata temphum;
        sht4x_read(&temphum);
        struct sen50data pmdata;
        sen50_read(&pmdata);
        /* Read Ambient Light in Lux (may block for up to 500 ms!) and switch
         * right back to UV mode */
        float lux = ltr390_readal();
        ltr390_startuvmeas();

        int naevs = (activeevs == 0) ? 1 : 0;
        evs[naevs].lastupd = lastmeasts;
        /* The following is before we potentially turn on the heater and update
         * lastsht4xheat on purpose: We will only turn on the heater AFTER the
         * measurements, so it cannot affect that measurement, only the next
         * one. And the whole point of that timestamp is to allow users to
         * see whether a heating might have influenced the measurements. */
        evs[naevs].lastsht4xheat = lastsht4xheat;

        /* We will now start to submit our measurements, so we need
         * a working network connection. Potentially wait for up to
         * 4 more seconds if we haven't got an IP address yet */
        xEventGroupWaitBits(network_event_group, NETWORK_CONNECTED_BIT,
                            pdFALSE, pdFALSE,
                            (4000 / portTICK_PERIOD_MS));
        if (press > 0) {
          ESP_LOGI(TAG, "Measured pressure: %.3f hPa, calculated pressure at sea level (FIXME better formula): %.3f hPa",
                        press, reducedairpressurecalc(press));
          /* submit that measurement */
          evs[naevs].press = press;
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_PRESSURE, press);
          submit_to_opensensemap(CONFIG_ZAMDACH_OSM_BOXID, CONFIG_ZAMDACH_OSMSID_PRESSURE, press);
        } else {
          evs[naevs].press = NAN;
        }

        if (raing > -0.1) {
          ESP_LOGI(TAG, "Rain: %.3f mm", raing);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_RAINGAUGE1, raing);
          //FIXME not yet, values not sane
          //submit_to_opensensemap(CONFIG_ZAMDACH_OSM_BOXID, CONFIG_ZAMDACH_OSMSID_RAINGAUGE1, raing);
          evs[naevs].raing = raing;
        } else {
          evs[naevs].raing = NAN;
        }

        if (lastanemomread != 0) { /* Ignore the first read on startup, but other */
          /* than that, we really have no way of telling if a reading is valid or not */
          int tsdif = curanemomread - lastanemomread;
          /* calculate Wind Speed in km/h from the number of impulses and the timestamp difference */
          float windspeed = 2.4 * wsctr / tsdif;
          float peakws = ws_readpeakws();
          ESP_LOGI(TAG, "Wind speed: %.2f km/h, Peak: %.2f km/h", windspeed, peakws);
          struct osm wsosm[2];
          wsosm[0].sensorid = CONFIG_ZAMDACH_WPDSID_WINDSPEED;
          wsosm[0].value = windspeed;
          wsosm[1].sensorid = CONFIG_ZAMDACH_WPDSID_WINDSPMAX;
          wsosm[1].value = peakws;
          submit_to_wpd_multi(2, wsosm);
          /* We just reuse the struct, overwriting just the sensorid. */
          wsosm[0].sensorid = CONFIG_ZAMDACH_OSMSID_WINDSPEED;
          wsosm[1].sensorid = CONFIG_ZAMDACH_OSMSID_WINDSPMAX;
          submit_to_opensensemap_multi(CONFIG_ZAMDACH_OSM_BOXID, 2, wsosm);
          evs[naevs].windspeed = windspeed;
          evs[naevs].windspmax = peakws;
        } else {
          evs[naevs].windspeed = NAN;
          evs[naevs].windspmax = NAN;
        }
        lastanemomread = curanemomread;

        if (wsdir < 16) { /* Only Range 0-15 is valid */
          char * winddirmap[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
          ESP_LOGI(TAG, "Wind direction: %d (%s)", wsdir, winddirmap[wsdir]);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_WINDDIR, (float)wsdir * 22.5);
          submit_to_opensensemap(CONFIG_ZAMDACH_OSM_BOXID, CONFIG_ZAMDACH_OSMSID_WINDDIR, (float)wsdir * 22.5);
          evs[naevs].winddirdeg = (float)wsdir * 22.5;
          strcpy(evs[naevs].winddirtxt, winddirmap[wsdir]);
        } else {
          evs[naevs].winddirdeg = -1;
          strcpy(evs[naevs].winddirtxt, "N/A");
        }

        if (temphum.valid > 0) {
          ESP_LOGI(TAG, "Temperature: %.2f degC (raw: %x)", temphum.temp, temphum.tempraw);
          ESP_LOGI(TAG, "Humidity: %.2f %% (raw: %x)", temphum.hum, temphum.humraw);
          if (temphum.hum >= TOOWETTHRESHOLD) { /* This will cause creep */
            too_wet_ctr++;
          }
          /* creep mitigation through the integrated heater in the SHT4x.
           * This is inside temphum.valid on purpose: If we cannot communicate
           * with the sensor to read it, we probably cannot tell it to heat
           * either... */
          if (((too_wet_ctr > 60)
            && (temphum.temp >= 4.0) && (temphum.temp <= 60.0)
            && (temphum.hum <= 75.0)
            && ((time(NULL) - lastsht4xheat) > 10))
           || (forcesht4xheater > 0)) {
            /* It has been very wet for a long time, temperature is suitable
             * for heating, and heater has not been on in last 10 minutes.
             * Or someone clicked on 'force heater on' in the Web-Interface. */
            for (int i = 0; i < HEATERITS; i++) {
              if (i != 0) { vTaskDelay(pdMS_TO_TICKS(1500)); }
              sht4x_heatercycle();
            }
            lastsht4xheat = time(NULL);
            too_wet_ctr -= 30;
            forcesht4xheater = 0;
          }
          struct osm thosm[2];
          thosm[0].sensorid = CONFIG_ZAMDACH_WPDSID_TEMPERATURE;
          thosm[0].value = temphum.temp;
          thosm[1].sensorid = CONFIG_ZAMDACH_WPDSID_HUMIDITY;
          thosm[1].value = temphum.hum;
          submit_to_wpd_multi(2, thosm);
          /* We'll just reuse the struct. No need to set 'value' again. */
          thosm[0].sensorid = CONFIG_ZAMDACH_OSMSID_TEMPERATURE;
          thosm[1].sensorid = CONFIG_ZAMDACH_OSMSID_HUMIDITY;
          submit_to_opensensemap_multi(CONFIG_ZAMDACH_OSM_BOXID, 2, thosm);
          evs[naevs].temp = temphum.temp;
          evs[naevs].hum = temphum.hum;
        } else {
          evs[naevs].temp = NAN;
          evs[naevs].hum = NAN;
        }

        if (pmdata.valid > 0) {
          ESP_LOGI(TAG, "PM 1.0: %.1f (raw: %x)", pmdata.pm010, pmdata.pm010raw);
          ESP_LOGI(TAG, "PM 2.5: %.1f (raw: %x)", pmdata.pm025, pmdata.pm025raw);
          ESP_LOGI(TAG, "PM 4.0: %.1f (raw: %x)", pmdata.pm040, pmdata.pm040raw);
          ESP_LOGI(TAG, "PM10.0: %.1f (raw: %x)", pmdata.pm100, pmdata.pm100raw);
          struct osm pmdosm[4];
          pmdosm[0].sensorid = CONFIG_ZAMDACH_WPDSID_PM010;
          pmdosm[0].value = pmdata.pm010;
          pmdosm[1].sensorid = CONFIG_ZAMDACH_WPDSID_PM025;
          pmdosm[1].value = pmdata.pm025;
          pmdosm[2].sensorid = CONFIG_ZAMDACH_WPDSID_PM040;
          pmdosm[2].value = pmdata.pm040;
          pmdosm[3].sensorid = CONFIG_ZAMDACH_WPDSID_PM100;
          pmdosm[3].value = pmdata.pm100;
          submit_to_wpd_multi(4, pmdosm);
          /* We'll just reuse the struct. No need to set 'value' again. */
          pmdosm[0].sensorid = CONFIG_ZAMDACH_OSMSID_PM010;
          pmdosm[1].sensorid = CONFIG_ZAMDACH_OSMSID_PM025;
          pmdosm[2].sensorid = CONFIG_ZAMDACH_OSMSID_PM040;
          pmdosm[3].sensorid = CONFIG_ZAMDACH_OSMSID_PM100;
          submit_to_opensensemap_multi(CONFIG_ZAMDACH_OSM_BOXID, 4, pmdosm);
          evs[naevs].pm010 = pmdata.pm010;
          evs[naevs].pm025 = pmdata.pm025;
          evs[naevs].pm040 = pmdata.pm040;
          evs[naevs].pm100 = pmdata.pm100;
        } else {
          evs[naevs].pm010 = NAN;
          evs[naevs].pm025 = NAN;
          evs[naevs].pm040 = NAN;
          evs[naevs].pm100 = NAN;
        }

        if (uvind >= 0.0) {
          ESP_LOGI(TAG, "UV-Index: %.2f", uvind);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_UV, uvind);
          //FIXME not yet, values not sane
          //submit_to_opensensemap(CONFIG_ZAMDACH_OSM_BOXID, CONFIG_ZAMDACH_OSMSID_UV, uvind);
          evs[naevs].uvind = uvind;
        } else {
          evs[naevs].uvind = NAN;
        }

        if (lux >= 0.0) {
          ESP_LOGI(TAG, "Ambient light/Illuminance: %.2f lux", lux);
          submit_to_wpd(CONFIG_ZAMDACH_WPDSID_ILLUMINANCE, lux);
          submit_to_opensensemap(CONFIG_ZAMDACH_OSM_BOXID, CONFIG_ZAMDACH_OSMSID_ILLUMINANCE, lux);
          evs[naevs].lux = lux;
        } else {
          evs[naevs].lux = NAN;
        }

        /* Now mark the updated values as the current ones for the webserver */
        activeevs = naevs;

        long howmuchtosleep = (lastmeasts + 60) - time(NULL) - 1;
        if ((howmuchtosleep < 0) || (howmuchtosleep > 60)) { howmuchtosleep = 60; }
        ESP_LOGI(TAG, "will now wait for %ld seconds before doing the next update", howmuchtosleep);
        vTaskDelay(pdMS_TO_TICKS(howmuchtosleep * 1000));
      } else { // Delay for a short time
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
}

