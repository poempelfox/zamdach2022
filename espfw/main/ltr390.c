/* Talking to the LTR390 UV sensor */

#include "esp_log.h"
#include "ltr390.h"
#include "sdkconfig.h"


#define LTR390ADDR 0x53
#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

#define LTR390_REG_MAINCTRL 0x00
#define LTR390_UVSMODE 0x08  /* UV-mode instead of Ambient Light mode */
#define LTR390_LSENABLE 0x02 /* Enable UV/AL-sensor */

#define LTR390_REG_MEASRATE 0x04
#define LTR390_RES20BIT 0x00
#define LTR390_RES19BIT 0x10
#define LTR390_RES18BIT 0x20 /* this is the poweron default */
#define LTR390_RES17BIT 0x30
#define LTR390_RES16BIT 0x40
#define LTR390_RES13BIT 0x50
#define LTR390_RATE0025MS 0x00
#define LTR390_RATE0050MS 0x01
#define LTR390_RATE0100MS 0x02 /* this is the poweron default */
#define LTR390_RATE0200MS 0x03
#define LTR390_RATE0500MS 0x04
#define LTR390_RATE1000MS 0x05
#define LTR390_RATE2000MS 0x06

#define LTR390_REG_GAIN 0x05
#define LTR390_GAIN01 0x00
#define LTR390_GAIN03 0x01 /* this is the poweron default */
#define LTR390_GAIN06 0x02
#define LTR390_GAIN09 0x03
#define LTR390_GAIN18 0x04

#define LTR390_REG_INTCFG 0x19
#define LTR390_INT_USEALS 0x10 /* this is the poweron default */
#define LTR390_INT_USEUVS 0x30
#define LTR390_INT_ENABLE 0x04

#define LTR390_REG_UVSDATAL 0x10  /* LSB */
#define LTR390_REG_UVSDATAM 0x11
#define LTR390_REG_UVSDATAH 0x12  /* MSB */

static i2c_port_t ltr390i2cport;

static esp_err_t ltr390_writereg(uint8_t reg, uint8_t val)
{
    uint8_t regandval[2];
    regandval[0] = reg;
    regandval[1] = val;
    return i2c_master_write_to_device(ltr390i2cport, LTR390ADDR,
                                      regandval, 2,
                                      I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void ltr390_init(i2c_port_t port)
{
    ltr390i2cport = port;

    /* Configure the LTR390 */
    ltr390_writereg(LTR390_REG_MAINCTRL, (LTR390_UVSMODE | LTR390_LSENABLE));
    ltr390_writereg(LTR390_REG_MEASRATE, (LTR390_RES20BIT | LTR390_RATE2000MS));
    ltr390_writereg(LTR390_REG_GAIN, LTR390_GAIN18);
}

static esp_err_t ltr390_readreg(uint8_t reg, uint8_t * val)
{
    return i2c_master_write_read_device(ltr390i2cport, LTR390ADDR,
                                        &reg, 1, val, 1,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

double ltr390_readuv(void)
{
    uint8_t uvsreg[3];
    int isvalid = 1;
    if (ltr390_readreg(LTR390_REG_UVSDATAL, &uvsreg[0]) != ESP_OK) {
      isvalid = 0;
    }
    if (ltr390_readreg(LTR390_REG_UVSDATAM, &uvsreg[1]) != ESP_OK) {
      isvalid = 0;
    }
    if (ltr390_readreg(LTR390_REG_UVSDATAH, &uvsreg[2]) != ESP_OK) {
      isvalid = 0;
    }
    if (isvalid != 1) {
      /* Read error, signal that we received nonsense by returning a negative UV index */
      ESP_LOGI("ltr390.c", "ERROR: I2C-read from LTR390 failed.");
      return -1.0;
    }
    uint32_t uvsr32 = ((uint32_t)(uvsreg[2] & 0x0F) << 16)
                    | ((uint32_t)uvsreg[1] << 8)
                    | uvsreg[0];
    /* The datasheet uses "UV sensitivity" in the UV index formula, and that one is
     * only given for gain=18 and resolution=20 bits, so we cannot really use anything
     * else. */
    double uvsensitivity = 2300.0;
    double wfac = 1.0; /* Correction factor for case with window. */
    double uvind = ((double)uvsr32 / uvsensitivity) * wfac;
    return uvind;
}

