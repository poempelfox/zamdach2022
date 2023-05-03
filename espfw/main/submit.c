/* ZAMDACH2022
 * Functions for submitting measurements to various APIs/Websites. */

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "submit.h"
#include "sdkconfig.h"
#include "secrets.h"

int submit_to_wpd_multi(int arraysize, struct osm * aoosm)
{
    int res = 0;
    if ((strcmp(ZAMDACH_WPDTOKEN, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM123456789") == 0)
     || (strcmp(ZAMDACH_WPDTOKEN, "") == 0)) {
      ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because no valid token has been set.");
      return 1;
    }
    for (int i = 0; i < arraysize; i++) {
      if (strcmp(aoosm[i].sensorid, "") == 0) {
        ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because sensorid has not been set in arrayelement %d of %d.", i, arraysize);
        return 1;
      }
    }
    char post_data[500];
    /* Build the contents of the HTTP POST we will
     * send to wetter.poempelfox.de */
    strcpy(post_data, "{\"software_version\":\"zamdach2022-0.1\",\"sensordatavalues\":[\n");
    for (int i = 0; i < arraysize; i++) {
      if (i != 0) { strcat(post_data, ",\n"); }
      sprintf(&post_data[strlen(post_data)],
              "{\"value_type\":\"%s\",\"value\":\"%.3f\"}",
              aoosm[i].sensorid, aoosm[i].value);
    }
    strcat(post_data, "\n]}\n");
    ESP_LOGI("submit.c", "wpd-payload: %d bytes: '%s'", strlen(post_data), post_data);
    esp_http_client_config_t httpcc = {
      .url = "http://wetter.poempelfox.de/api/pushmeasurement/",
      .crt_bundle_attach = esp_crt_bundle_attach,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 5000,
      .user_agent = "ZAMDACH2022/0.1 (ESP32)"
    };
    esp_http_client_handle_t httpcl = esp_http_client_init(&httpcc);
    esp_http_client_set_header(httpcl, "Content-Type", "application/json");
    esp_http_client_set_header(httpcl, "X-Sensor", ZAMDACH_WPDTOKEN);
    esp_http_client_set_post_field(httpcl, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(httpcl);
    if (err == ESP_OK) {
        ESP_LOGI("submit.c", "HTTP POST Status = %d, content_length = %lld",
                      esp_http_client_get_status_code(httpcl),
                      esp_http_client_get_content_length(httpcl));
    } else {
        ESP_LOGE("submit.c", "HTTP POST request failed: %s", esp_err_to_name(err));
        res = 1;
    }
    esp_http_client_cleanup(httpcl);
    return res;
}

int submit_to_wpd(char * sensorid, float value)
{
  struct osm aoosm[1];
  if (strcmp(sensorid, "") == 0) {
    ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because sensorid is not set.");
    return 1;
  }
  aoosm[0].sensorid = sensorid;
  aoosm[0].value = value;
  return submit_to_wpd_multi(1, aoosm);
}

int submit_to_opensensemap_multi(char * boxid, int arraysize, struct osm * arrayofosm)
{
    int res = 1;
    /* No check for authentication token because not having one can be perfectly
     * valid for opensensemap */
    if (strcmp(boxid, "") == 0) {
      ESP_LOGI("submit.c", "Not sending data to opensensemap because boxid is not set.");
      return 1;
    }
    /* Send HTTP POST to api.opensensemap.org */
    char post_data[500];
    strcpy(post_data, "[");
    for (int i = 0; i < arraysize; i++) {
      if (strcmp(arrayofosm[i].sensorid, "") != 0) {
        sprintf(&post_data[strlen(post_data)],
                "{\"sensor\":\"%s\",\"value\":\"%.3f\"},",
                arrayofosm[i].sensorid, arrayofosm[i].value);
      }
    }
    post_data[strlen(post_data)-1] = ']'; // We overwrite the ',' after the last entry
    ESP_LOGI("submit.c", "opensensemap-payload: %d bytes: '%s'", strlen(post_data), post_data);
    char apiurl[200];
    sprintf(apiurl, "https://api.opensensemap.org/boxes/%s/data", boxid);
    esp_http_client_config_t httpcc = {
      .url = apiurl,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 5000,
      .user_agent = "ZAMDACH2022/0.1 (ESP32)"
    };
    esp_http_client_handle_t httpcl = esp_http_client_init(&httpcc);
    esp_http_client_set_header(httpcl, "Content-Type", "application/json");
    if (strcmp(ZAMDACH_OSMTOKEN, "") != 0) {
      esp_http_client_set_header(httpcl, "Authorization", ZAMDACH_OSMTOKEN);
    }
    esp_http_client_set_post_field(httpcl, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(httpcl);
    if (err == ESP_OK) {
        ESP_LOGI("submit.c", "HTTP POST Status = %d, content_length = %lld",
                      esp_http_client_get_status_code(httpcl),
                      esp_http_client_get_content_length(httpcl));
    } else {
        ESP_LOGE("submit.c", "HTTP POST request failed: %s", esp_err_to_name(err));
        res = 1;
    }
    esp_http_client_cleanup(httpcl);
    return res;
}

int submit_to_opensensemap(char * boxid, char * sensorid, float value)
{
  struct osm aoosm[1];
  if (strcmp(sensorid, "") == 0) {
    ESP_LOGI("submit.c", "Not sending data to opensensemap because sensorid is not set.");
    return 1;
  }
  aoosm[0].sensorid = sensorid;
  aoosm[0].value = value;
  return submit_to_opensensemap_multi(boxid, 1, aoosm);
}

