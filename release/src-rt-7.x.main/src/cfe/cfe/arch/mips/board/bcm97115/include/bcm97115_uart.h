#ifdef DEBUG_ENV_SIM
#define XTALFREQ    8333333
#else
#define XTALFREQ   27000000
#endif

#define RXSTAT_OFFSET  0x00
#define RXDATA_OFFSET  0x01
#define CTRL_OFFSET    0x03
#define BAUDHI_OFFSET  0x04
#define BAUDLO_OFFSET  0x05
#define TXSTAT_OFFSET  0x06
#define TXDATA_OFFSET  0x07

/* Control register */
#define TXEN           0x04
#define RXEN           0x02
#define BITM8          0x10

/* Tx status register */
#define TXDRE          0x01

/* Rx status register */
#define RXRDA          0x04
#define RXOVFERR       0x08
#define RXPARERR       0x20
#define RXFRAMERR      0x10
