idf_component_register(SRCS "zamdach2022_main.c" "i2c.c" "lps25hb.c" "ltr390.c" "network.c" "rg15.c" "sen50.c" "sht4x.c" "submit.c" "webserver.c" "windsens.c"
                       INCLUDE_DIRS "." ""
                       REQUIRES soc nvs_flash ulp driver esp_http_client esp_adc esp_http_server app_update esp_https_ota esp_eth esp_phy esp_wifi esp_netif esp_timer lwip mbedtls)

