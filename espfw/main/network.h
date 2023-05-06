
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

/* Bits for the network-status event-group */
#define NETWORK_CONNECTED_BIT BIT0  /* Set when we get an IP */

extern EventGroupHandle_t network_event_group;

/* Prepares network connection / starts to bring the
 * interface up. */
void network_prepare(void);

#endif /* _NETWORK_H_ */

