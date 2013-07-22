/*
 * PIO bit definitions
 */

#define PIO_I2C_SDA	0x80	/* in:  DIMM serial data */
#define PIO_I2C_SCL	0x40	/* out: DIMM serial clock */
#define PIO_I2C_DIR	0x20	/* out: DIMM serial direction (1=out, 0=in)*/
#define PIO_NFAULT	0x01	/* out: centronics NFAULT signal */
#define PIO_OMASK	(PIO_I2C_SCL|PIO_I2C_DIR|PIO_NFAULT)	


#ifndef __ASSEMBLER__
void		_pioa_dir (unsigned int outputs);
unsigned int	_pioa_get (void);
unsigned int	_pioa_set (unsigned int new);
unsigned int	_pioa_bic (unsigned int bits);
unsigned int	_pioa_bis (unsigned int bits);
void		_piob_dir (unsigned int outputs);
unsigned int	_piob_get (void);
unsigned int	_piob_set (unsigned int new);
unsigned int	_piob_bic (unsigned int bits);
unsigned int	_piob_bis (unsigned int bits);
#endif
