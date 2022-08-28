idf_component_register(SRCS "zamdach2022_main.c" "i2c.c" "lps25hb.c" "ltr390.c" "network.c" "rg15.c" "sen50.c" "sht4x.c" "submit.c" "windsens.c"
                       INCLUDE_DIRS "." ""
                       REQUIRES soc nvs_flash ulp driver esp_http_client)

# ULP is the ultra-low-power coprocessor that the ESP32 has and that can run
# constantly during sleep of the main processor with only 1.5 mA, but that is
# extremely limited in its capabilities and can only be programmed in assembler.
set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_s_sources "ulp/windsensctr.S")
set(ulp_exp_dep_srcs "windsens.c")

ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")

