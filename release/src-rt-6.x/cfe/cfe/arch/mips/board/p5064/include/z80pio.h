/*
 * z80pio.h: Z80 parallel i/o chip register definitions
 */

#ifndef __ASSEMBLER__
typedef struct {
    unsigned int	a_dat;
    unsigned int	b_dat;
    unsigned int	a_ctl;
    unsigned int	b_ctl;
} zpiodev;
#endif

/* register offsets */
#define ZPIO_A_DAT	0
#define ZPIO_B_DAT	4
#define ZPIO_A_CTL	8
#define ZPIO_B_CTL	12

#define ZPIO_MODE_OUT	0x0f
#define ZPIO_MODE_IN	0x4f
#define ZPIO_MODE_BIDIR	0x8f
#define ZPIO_MODE_CTRL	0xcf

#define ZPIO_ICW_ENABLE	0x80
#define ZPIO_ICW_AND	0x40
#define ZPIO_ICW_HIGH	0x40
#define ZPIO_ICW_MASK	0x40
#define ZPIO_ICW_SET	0x07

#define ZPIO_INT_ENB	0x83
#define ZPIO_INT_DIS	0x03

#define ZPIOB_I2C_SDA	0x80	/* in:  DIMM serial data */
#define ZPIOB_I2C_SCL	0x40	/* out: DIMM serial clock */
#define ZPIOB_I2C_DIR	0x20	/* out: DIMM serial direction (1=out, 0=in)*/
#define ZPIOB_NFAULT	0x01	/* out: centronics NFAULT signal */
#define ZPIOB_OMASK	(ZPIOB_I2C_SCL|ZPIOB_I2C_DIR|ZPIOB_NFAULT)	

#ifndef __ASSEMBLER__
/* safe programmatic interface to pio */
unsigned int _zpiob_get (void);
unsigned int _zpiob_set (unsigned int);
unsigned int _zpiob_bis (unsigned int);
unsigned int _zpiob_bic (unsigned int);

void _zpioa_setctl (unsigned int);
unsigned int _zpioa_get (void);
unsigned int _zpioa_set (unsigned int);
unsigned int _zpioa_bis (unsigned int);
unsigned int _zpioa_bic (unsigned int);
#endif
