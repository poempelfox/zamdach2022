/* Talking to the TSL2591 light sensor */

#include "esp_log.h"
#include "tsl2591.h"
#include "sdkconfig.h"


#define TSL2591ADDR 0x29
#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

/* Register addresses */
#define TLS2591_REG_ENABLE 0x00  /* the "Enable" register turns things on/off */

/* TSL2591 command "register" bits (this register has no address) */
#define TLS2591_CMD_CMD 0x80   /* set this to select the command register */
#define TLS2591_CMD_TRANSACT_NORMAL  0x20
#define TLS2591_CMD_TRANSACT_SPECIAL 0x60
/* bits 4:0 select a register in _TRANSACT_NORMAL */

/* TSL2591 enable register bits */
#define TLS2591_ENA_PON  0x01 /* Power on */
#define TLS2591_ENA_AEN  0x02 /* ALS enable */
#define TLS2591_ENA_AIEN 0x10 /* ALS interrupt enable */

static i2c_port_t tsl2591i2cport;

void tsl2591_init(i2c_port_t port)
{
    tsl2591i2cport = port;

    /* Configure the TSL2591 */
    /* FIXME */
}

double tsl2591_readlux(void)
{
    /* FIXME */
    return 0.0;
}

