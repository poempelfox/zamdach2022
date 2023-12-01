/* Talking to the LPS25HB pressure sensor */

#include "esp_log.h"
#include "lps25hb.h"
#include "sdkconfig.h"


#define LPS25HBADDR 0x5c  /* That is hardwired on our breakout board */
#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

static i2c_port_t lps25hbi2cport;

static esp_err_t lps25hb_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(lps25hbi2cport,
                                        LPS25HBADDR, &reg_addr, 1, data, len,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t lps25hb_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(lps25hbi2cport,
                                     LPS25HBADDR, write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

void lps25hb_init(i2c_port_t port)
{
    lps25hbi2cport = port;

    /* Configure the LPS25HB */
    /* CTRL_REG1 0x20: Power on/Enable, 1 Hz */
    lps25hb_register_write_byte(0x20, (0x80 | 0x40));
}

double lps25hb_readpressure(void)
{
    uint8_t prr[3];
    static int lpsnonsensereadcounter = 0;
    if (lps25hb_register_read(0x80 | 0x28, &prr[0], 3) != ESP_OK) {
      /* There was an I2C read error - return a negative pressure to signal that. */
      return -999999.9;
    }
    if ((prr[2] == 0x2f) && (prr[1] == 0x80) && (prr[0] == 0x00)) {
      /* This is the poweron-default-value of the register, meaning the
       * thing might have reset. */
      lpsnonsensereadcounter++;
      if (lpsnonsensereadcounter > 10) { /* OK, we read nonsense 10 times in a row. */
        /* Lets try to resend the config command - we just call our own init func for that. */
        lps25hb_init(lps25hbi2cport);
        lpsnonsensereadcounter = 0;
      }
      /* In any case, 760hPa is a nonsense-value - return negative pressure to signal that. */
      return -999999.9;
    }
    ESP_LOGI("lps25hb.c", "LPS25HB read - values %02x%02x%02x", prr[0], prr[1], prr[2]);
    double press = (((uint32_t)prr[2]  << 16)
                  + ((uint32_t)prr[1]  <<  8)
                  + ((uint32_t)prr[0]  <<  0)) / 4096.0;
    return press;
}

