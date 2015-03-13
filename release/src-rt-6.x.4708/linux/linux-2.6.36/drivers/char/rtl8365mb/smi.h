
#ifndef __SMI_H__
#define __SMI_H__

#include <rtk_types.h>
#include "rtk_error.h"

#define MDC_MDIO_CTRL0_REG          31
#define MDC_MDIO_START_REG          29
#define MDC_MDIO_CTRL1_REG          21
#define MDC_MDIO_ADDRESS_REG        23
#define MDC_MDIO_DATA_WRITE_REG     24
#define MDC_MDIO_DATA_READ_REG      25
#define MDC_MDIO_PREAMBLE_LEN       32

#define MDC_MDIO_START_OP          0xFFFF
#define MDC_MDIO_ADDR_OP           0x000E
#define MDC_MDIO_READ_OP           0x0001
#define MDC_MDIO_WRITE_OP          0x0003

#define SPI_READ_OP                 0x3
#define SPI_WRITE_OP                0x2
#define SPI_READ_OP_LEN             0x8
#define SPI_WRITE_OP_LEN            0x8
#define SPI_REG_LEN                 16
#define SPI_DATA_LEN                16

#define GPIO_DIR_IN                 1
#define GPIO_DIR_OUT                0

#define ack_timer                   5

#define DELAY                        10000
#define CLK_DURATION(clk)            { int i; for(i=0; i<clk; i++); }

#define GPIO_DIRECTION_SET(gpioID, direction)   set_gpio_dir(gpioID, direction)
#define GPIO_DATA_SET(gpioID, data)             set_gpio_data(gpioID, data)
#define GPIO_DATA_GET(gpioID, pData)            get_gpio_data(gpioID, pData)

void just_smi_start(void);
rtk_int32 smi_read(rtk_uint32 mAddrs, rtk_uint32 *rData);
rtk_int32 smi_write(rtk_uint32 mAddrs, rtk_uint32 rData);

#endif /* __SMI_H__ */


