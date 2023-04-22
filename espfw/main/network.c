
#include "network.h"
#include <lwip/dhcp6.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <esp_eth.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <time.h>
#include "sdkconfig.h"
#include "secrets.h"

extern char chipid[30];

EventGroupHandle_t network_event_group;

#ifndef CONFIG_ZAMDACH_USEWIFI
/* We need to make this a global variable, because the eth_event_handler
 * needs it to set options for DHCP6. */
esp_netif_t * mainnetif = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    struct netif * lwip_netif;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI("network.c", "Ethernet Link Up");
        ESP_LOGI("network.c", "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        /* This is of course not in the documentation, but a bugreport
         * reveals the next 2 lines are needed to have working IPv6 on
         * Ethernet. It seems to work fine without these on WiFi though! */
        lwip_netif = esp_netif_get_netif_impl(mainnetif);
        netif_set_flags(lwip_netif, NETIF_FLAG_MLD6);
        esp_netif_create_ip6_linklocal(mainnetif);
        /* Unfortunately, neither esp-idf nor the LWIP-library they
         * use beneath support proper DHCPv6. They only support
         * "stateless DHCPv6" and with that they mean they do not
         * support address assignment, but can query the DHCP for
         * settings.
         * The following line would enable to get DNS and NTP servers
         * from DHCPv6, if "Enable DHCPv6 stateless address
         * autoconfiguration" was on in sdkconfig. However, that's
         * pretty pointless, so we don't use it for now */
        //dhcp6_enable_stateless(lwip_netif);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI("network.c", "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI("network.c", "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI("network.c", "Ethernet Stopped");
        break;
    default:
        break;
    }
}
#endif /* !CONFIG_ZAMDACH_USEWIFI */

#ifdef CONFIG_ZAMDACH_USEWIFI
/** Event handler for WiFi events */
static time_t lastwifireconnect = 0;
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t * ev_co = (wifi_event_sta_connected_t *)event_data;
    wifi_event_sta_disconnected_t * ev_dc = (wifi_event_sta_disconnected_t *)event_data;
    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI("network.c", "WiFi Connected: channel %u bssid %02x%02x%02x%02x%02x%02x",
                           ev_co->channel, ev_co->bssid[0], ev_co->bssid[1], ev_co->bssid[2],
                           ev_co->bssid[3], ev_co->bssid[4], ev_co->bssid[5]);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI("network.c", "WiFi Disconnected: reason %u", ev_dc->reason);
            if (ev_dc->reason == WIFI_REASON_ASSOC_LEAVE) break; /* This was an explicit call to disconnect() */
            if ((lastwifireconnect == 0)
             || ((time(NULL) - lastwifireconnect) > 5)) {
              /* Last reconnect attempt more than 5 seconds ago - try it again */
              ESP_LOGI("network.c", "Attempting WiFi reconnect");
              lastwifireconnect = time(NULL);
              esp_wifi_connect();
            }
            break;
        default: break;
    }
}
#endif /* CONFIG_ZAMDACH_USEWIFI */

/** Event handler for IP_EVENT_(ETH|STA)_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t * event4;
    const esp_netif_ip_info_t *ip_info;
    ip_event_got_ip6_t * event6;
    const esp_netif_ip6_info_t * ip6_info;
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
    case IP_EVENT_ETH_GOT_IP:
      ESP_LOGI("network.c", "We got an IPv4 address!");
      event4 = (ip_event_got_ip_t *)event_data;
      ip_info = &event4->ip_info;
      ESP_LOGI("network.c", "IP:     " IPSTR, IP2STR(&ip_info->ip));
      ESP_LOGI("network.c", "NETMASK:" IPSTR, IP2STR(&ip_info->netmask));
      ESP_LOGI("network.c", "GW:     " IPSTR, IP2STR(&ip_info->gw));
      break;
    case IP_EVENT_GOT_IP6:
      ESP_LOGI("network.c", "We got an IPv6 address!");
      event6 = (ip_event_got_ip6_t *)event_data;
      ip6_info = &event6->ip6_info;
      ESP_LOGI("network.c", "IPv6:" IPV6STR, IPV62STR(ip6_info->ip));
      break;
    case IP_EVENT_STA_LOST_IP:
    case IP_EVENT_ETH_LOST_IP:
      ESP_LOGI("network.c", "IP-address lost.");
    };
    xEventGroupSetBits(network_event_group, NETWORK_CONNECTED_BIT);
}

void network_prepare(void)
{
    network_event_group = xEventGroupCreate();
#ifdef CONFIG_ZAMDACH_USEWIFI
    esp_netif_create_default_wifi_sta(); /* This seems to return a completely useless structure */
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wicfg));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));

    wifi_config_t wccfg = {
      .sta = {
        .ssid = CONFIG_ZAMDACH_WIFISSID,
        .password = ZAMDACH_WIFIPASSWORD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK
      }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wccfg));
#ifndef CONFIG_ZAMDACH_DOPOWERSAVE
    /* If we don't try to do powersaving, we just keep
     * connected all the time, so connect now. */
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
#endif /* ! CONFIG_ZAMDACH_DOPOWERSAVE */
#else /* !CONFIG_ZAMDACH_USEWIFI - we use ethernet */
    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t nicfg = ESP_NETIF_DEFAULT_ETH();
    mainnetif = esp_netif_new(&nicfg);
    esp_netif_set_hostname(mainnetif, chipid);

    /* for ESP32-POE-ISO: Turn PHY Power on through GPIO12 */
    int phypowerpin = 12;
    esp_rom_gpio_pad_select_gpio(phypowerpin);
    gpio_set_direction(phypowerpin, GPIO_MODE_OUTPUT);
    gpio_set_level(phypowerpin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_mdc_gpio_num = 23; /* ESP32-POE-ISO */
    emac_config.smi_mdio_gpio_num = 18; /* ESP32-POE-ISO */
    esp_eth_mac_t * ethmac = esp_eth_mac_new_esp32(&emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 0;  /* ESP32-POE-ISO */
    phy_config.reset_gpio_num = -1; /* ESP32-POE-ISO */

    esp_eth_phy_t * ethphy = esp_eth_phy_new_lan87xx(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(ethmac, ethphy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(mainnetif, esp_eth_new_netif_glue(eth_handle)));

    /* Register user defined event handers. Should be the
     * last thing before esp_eth_start! */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
#endif /* !CONFIG_ZAMDACH_USEWIFI */
}

void network_on(void)
{
#ifdef CONFIG_ZAMDACH_DOPOWERSAVE
#ifdef CONFIG_ZAMDACH_USEWIFI
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
#else /* !CONFIG_ZAMDACH_USEWIFI - we use ethernet */
#endif /* !CONFIG_ZAMDACH_USEWIFI */
#endif /* CONFIG_ZAMDACH_DOPOWERSAVE */
}

void network_off(void)
{
#ifdef CONFIG_ZAMDACH_DOPOWERSAVE
#ifdef CONFIG_ZAMDACH_USEWIFI
    xEventGroupClearBits(network_event_group, NETWORK_CONNECTED_BIT);
    ESP_ERROR_CHECK(esp_wifi_stop());
#else /* !CONFIG_ZAMDACH_USEWIFI - we use ethernet */
#endif /* !CONFIG_ZAMDACH_USEWIFI */
#endif /* CONFIG_ZAMDACH_DOPOWERSAVE */
}

