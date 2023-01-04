/* ZAMDACH2022
 * Functions for submitting measurements to various APIs/Websites. */

#include "esp_log.h"
#include "esp_http_client.h"
#include "submit.h"
#include "sdkconfig.h"
#include "secrets.h"

int submit_to_wpd(char * sensorid, char * valuetype, float value)
{
    int res = 0;
    /* Send HTTP POST to wetter.poempelfox.de */
    char post_data[400];
    sprintf(post_data, "{\n\"software_version\":\"0.1\",\n\"sensordatavalues\":[{\"value_type\":\"%s\",\"value\":\"%.3f\"}]}", valuetype, value);
    esp_http_client_config_t httpcc = {
      .url = "http://wetter.poempelfox.de/api/pushmeasurement",
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

