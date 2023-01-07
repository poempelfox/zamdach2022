/* ZAMDACH2022
 * Functions for submitting measurements to various APIs/Websites. */

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "submit.h"
#include "sdkconfig.h"
#include "secrets.h"

int submit_to_wpd(char * sensorid, char * valuetype, float value)
{
    int res = 0;
    if ((strcmp(ZAMDACH_WPDTOKEN, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM123456789") == 0)
     || (strcmp(ZAMDACH_WPDTOKEN, "") == 0)) {
      ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because no valid token has been set.");
      return 1;
    }
    if (strcmp(sensorid, "") == 0) {
      ESP_LOGI("submit.c", "Not sending '%s' data to wetter.poempelfox.de because sensorid has not been set.", valuetype);
      return 1;
    }
    /* Send HTTP POST to wetter.poempelfox.de */
    char post_data[400];
    sprintf(post_data, "{\n\"software_version\":\"0.1\",\n\"sensordatavalues\":[{\"value_type\":\"%s\",\"value\":\"%.3f\"}]}", valuetype, value);
    esp_http_client_config_t httpcc = {
      .url = "https://wetter.poempelfox.de/api/pushmeasurement",
      .crt_bundle_attach = esp_crt_bundle_attach,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 5000,
      .user_agent = "ZAMDACH2022/0.1 (ESP32)"
    };
    esp_http_client_handle_t httpcl = esp_http_client_init(&httpcc);
    esp_http_client_set_header(httpcl, "Content-Type", "application/json");
    esp_http_client_set_header(httpcl, "X-Pin", sensorid);
    esp_http_client_set_header(httpcl, "X-Sensor", ZAMDACH_WPDTOKEN);
    esp_http_client_set_post_field(httpcl, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(httpcl);
    if (err == ESP_OK) {
        ESP_LOGI("submit.c", "HTTP POST Status = %d, content_length = %d",
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
    int res = 1;
    /* No check for authentication token because not having one can be perfectly
     * valid for opensensemap */
    if ((strcmp(boxid, "") == 0)
     || (strcmp(sensorid, "") == 0)) {
      ESP_LOGI("submit.c", "Not sending data to opensensemap because either boxid or sensorid is not set.");
      return 1;
    }
    /* Send HTTP POST to api.opensensemap.org */
    char post_data[100];
    sprintf(post_data, "{\"value\":\"%.3f\"}", value);
    char apiurl[200];
    sprintf(apiurl, "https://api.opensensemap.org/boxes/%s/%s", boxid, sensorid);
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
        ESP_LOGI("submit.c", "HTTP POST Status = %d, content_length = %d",
                      esp_http_client_get_status_code(httpcl),
                      esp_http_client_get_content_length(httpcl));
    } else {
        ESP_LOGE("submit.c", "HTTP POST request failed: %s", esp_err_to_name(err));
        res = 1;
    }
    esp_http_client_cleanup(httpcl);
    return res;
}

