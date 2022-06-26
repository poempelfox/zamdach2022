/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "math.h"
#include "sdkconfig.h"

static const char *TAG = "zamdach2022";

#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */
#define LPS25HBADDR 0x5c  /* That is hardwired on our breakout board */

static esp_err_t lps25hb_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(0, LPS25HBADDR, &reg_addr, 1, data, len,
                                           I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t lps25hb_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(0, LPS25HBADDR, write_buf, sizeof(write_buf),
                                        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

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
  int altitude = 279;
  return (press / pow((1 - (altitude * 0.0000225577)), 5.25588));
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

void app_main(void)
{
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    i2c_config_t i2cp0conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 13,  /* GPIO13 */
        .scl_io_num = 16,  /* GPIO16 */
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 250000, /* There is really no need to hurry */
    };
    i2c_param_config(0, &i2cp0conf);
    if (i2c_driver_install(0, i2cp0conf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGI(TAG, "Oh dear: I2C-Init for Port 0 failed.");
    } else {
        ESP_LOGI(TAG, "I2C master port 0 initialized");
    }

    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = 0;  /* ESP32-POE-ISO */
    phy_config.reset_gpio_num = -1; /* ESP32-POE-ISO */

    /* EDIT for ESP32-POE-ISO: Turn Power on through GPIO12 */
    int phypowerpin = 12;
    gpio_pad_select_gpio(phypowerpin);
    gpio_set_direction(phypowerpin, GPIO_MODE_OUTPUT);
    gpio_set_level(phypowerpin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    mac_config.smi_mdc_gpio_num = 23; /* ESP32-POE-ISO */
    mac_config.smi_mdio_gpio_num = 18; /* ESP32-POE-ISO */
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    /* Configure the LPS25HB */
    /* CTRL_REG1 0x20: Power on/Enable, 1 Hz */
    lps25hb_register_write_byte(0x20, (0x80 | 0x40));

    vTaskDelay(pdMS_TO_TICKS(1111));

    uint8_t prh = 0, prl = 0, prxl = 0;
    int lpshadreaderr = 0;
    if (lps25hb_register_read(0x2a, &prh, 1) != ESP_OK) { lpshadreaderr = 1; }
    if (lps25hb_register_read(0x29, &prl, 1) != ESP_OK) { lpshadreaderr = 1; }
    if (lps25hb_register_read(0x28, &prxl, 1) != ESP_OK) { lpshadreaderr = 1; }
    ESP_LOGI(TAG, "LPS25HB read - err %d values %02x%02x%02x", lpshadreaderr, prh, prl, prxl);
    double press = (((uint32_t)prh  << 16)
                  + ((uint32_t)prl  <<  8)
                  + ((uint32_t)prxl <<  0)) / 4096.0;
    ESP_LOGI(TAG, "That converts to a measured pressure of %.3f hPa, or a calculated pressure of %.3f hPa at sea level.",
                  press, reducedairpressurecalc(press));
}

