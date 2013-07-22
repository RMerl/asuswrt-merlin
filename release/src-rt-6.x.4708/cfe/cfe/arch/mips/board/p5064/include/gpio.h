#define GPIO1_DATA	0x00
#define GPIO1_DIR	0x01
#define GPIO1_OTYPE	0x02
#define GPIO1_PULLUP	0x03
#define GPIO2_DATA	0x04
#define GPIO2_DIR	0x05
#define GPIO2_OTYPE	0x06
#define GPIO2_PULLUP	0x07

#define GPIO2_I2C_SDA	0x80	/* in:  DIMM serial data */
#define GPIO2_I2C_SCL	0x40	/* out: DIMM serial clock */
#define GPIO2_I2C_DIR	0x20	/* out: DIMM serial direction (1=out, 0=in)*/
#define GPIO2_NFAULT	0x01	/* out: centronics NFAULT signal */
#define GPIO2_OMASK	(GPIO2_I2C_SCL|GPIO2_I2C_DIR|GPIO2_NFAULT)
