/* ZAMDACH2022 rg15.c
 * routines for the RG15 rain sensor */

#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "rg15.h"

static QueueHandle_t rainsens_comm_handle;

void rg15_init(void)
{
    uart_config_t rainsens_serial_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
    };
    // Configure UART parameters - we're using UART1.
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 200, 200, 5, &rainsens_comm_handle, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &rainsens_serial_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 4, 36, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    /* Tell the rainsensor we want polling mode, a.k.a. "shut up until you're spoken to".
     * Also, use high res mode and metrical output, disable 
     * tipping-bucket-output, and reset counters. */
    uart_write_bytes(UART_NUM_1, "P\nH\nM\nY\nO\n", 10);
}

void rg15_requestread(void)
{
    /* Make sure there are no more other commands queued first. */
    uart_wait_tx_done(UART_NUM_1, 200);
    /* Now throw away everything in the receive buffer, it's
     * old anyways. */
    uart_flush_input(UART_NUM_1);
    /* And request a new reading.
     * Since we're only interested in changes, "A" should fit us well,
     * but we could also get summed up data with "R". */
    uart_write_bytes(UART_NUM_1, "A\n", 2);
}

float rg15_readraincount(void)
{
    char rcvdata[128];
    int length = 0;
    float res = -99999.9;
    if (uart_get_buffered_data_len(UART_NUM_1, (size_t*)&length) != ESP_OK) {
      ESP_LOGI("rg15.c", "Error talking to the serial port.");
      return -99999.9;
    }
    length = uart_read_bytes(UART_NUM_1, rcvdata, ((length > 100) ? 100 : length), 1);
    if (length > 0) {
      char * stsp; char * spp;
      rcvdata[length] = 0;
      ESP_LOGI("rg15.c", "Serial received %d bytes: %s", length, rcvdata);
      /* Split into lines */
      spp = strtok_r(&rcvdata[0], "\n", &stsp);
      do {
        ESP_LOGI("rg15.c", "Parsing: %s", spp);
        /* Attempt to parse the line. There are a few things we can safely ignore. */
        /* Lines starting with ";" are comments. */
        if (strncmp(spp, ";", 1) == 0) continue;
        /* "h" is acknowledgement of the "H" command that sets high resolution. */
        if ((strncmp(spp, "h", 1) == 0) && (strlen(spp) <= 3)) continue;
        /* "m" is acknowledgement of the "M" command that sets metric units. */
        if ((strncmp(spp, "m", 1) == 0) && (strlen(spp) <= 3)) continue;
        /* "y" is acknowledgement of the "Y" command that disables tipping bucket output. */
        if ((strncmp(spp, "m", 1) == 0) && (strlen(spp) <= 3)) continue;
        if (strncmp(spp, "Acc ", 4) == 0) {
          /* Acc  0.00 mm\r\n */
          char rcv1[128];
          if (sscanf(spp+4, "%f %s", &res, rcv1) < 2) {
            ESP_LOGI("rg15.c", "...failed to parse serial input (1).");
            return -99999.9;
          }
          if (strncmp(rcv1, "mm", 2) != 0) {
            ESP_LOGI("rg15.c", "...failed to parse serial input (2).");
            return -99999.9;
          }
          ESP_LOGI("rg15.c", "Successfully parsed raingauge value: %.3f", res);
        }
      } while ((spp = strtok_r(NULL, "\n", &stsp)) != NULL);
    } else {
      if (length < 0) {
        ESP_LOGI("rg15.c", "Received error from serial port.");
      } else { /* length == 0 */
        ESP_LOGI("rg15.c", "No data available on serial port.");
      }
    }
    return res;
}

