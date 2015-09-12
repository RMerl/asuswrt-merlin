#include "cfe.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"

#include "bsp_config.h"
#include "bcm97115_uart.h"

#define WRITEREG(csr,val) *(csr) = (val)
#define READREG(csr)     (*(csr))


static void bcm97115_uart_probe(cfe_driver_t *drv,
                               unsigned long probe_a, unsigned long probe_b,
                               void *probe_ptr);

static int bcm97115_uart_open(cfe_devctx_t *ctx);
static int bcm97115_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm97115_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm97115_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm97115_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm97115_uart_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t bcm97115_uart_dispatch = {
    bcm97115_uart_open,
    bcm97115_uart_read,
    bcm97115_uart_inpstat,
    bcm97115_uart_write,
    bcm97115_uart_ioctl,
    bcm97115_uart_close,
    NULL,
    NULL
};


const cfe_driver_t bcm97115_uart = {
    "BCM97115 DUART",
    "uart",
    CFE_DEV_SERIAL,
    &bcm97115_uart_dispatch,
    bcm97115_uart_probe
};


typedef struct bcm97115_uart_s {
	int baudrate;

	volatile uint8_t * rxstat;
	volatile uint8_t * rxdata;
	volatile uint8_t * ctrl;
	volatile uint8_t * baudhi;
	volatile uint8_t * baudlo;
	volatile uint8_t * txstat;
	volatile uint8_t * txdata;
} bcm97115_uart_t;


static void bcm97115_set_baudrate( bcm97115_uart_t * softc )
{
	uint32_t baudwd;


	baudwd  = (XTALFREQ / softc->baudrate) / 16;

	baudwd &= 0xFF;
	WRITEREG( softc->baudlo, baudwd );

	baudwd >>= 8;
	baudwd &= 0xFF;
	WRITEREG( softc->baudhi, baudwd );

}


static void bcm97115_uart_probe(cfe_driver_t *drv,
                               unsigned long probe_a, unsigned long probe_b,
                               void *probe_ptr)
{
	bcm97115_uart_t * softc;
	char descr[80];

	/* enable the transmitter interrupt? */

	/*
	 * probe_a is the DUART base address.
	 * probe_b is the channel-number-within-duart (0 or 1)
	 * probe_ptr is unused.
	 */
	softc = (bcm97115_uart_t *) KMALLOC(sizeof(bcm97115_uart_t),0);
	if (softc) {
		softc->rxstat = (uint8_t *)(probe_a);
		softc->rxdata = (uint8_t *)(probe_a + RXDATA_OFFSET);
		softc->ctrl   = (uint8_t *)(probe_a + CTRL_OFFSET);
		softc->baudhi = (uint8_t *)(probe_a + BAUDHI_OFFSET);
		softc->baudlo = (uint8_t *)(probe_a + BAUDLO_OFFSET);
		softc->txstat = (uint8_t *)(probe_a + TXSTAT_OFFSET);
		softc->txdata = (uint8_t *)(probe_a + TXDATA_OFFSET);

		xsprintf( descr, "%s at 0x%X channel %d", drv->drv_description,
		                                          probe_a, probe_b );
		cfe_attach( drv, softc, NULL, descr );
	}
}


#ifdef RAW_WRITE_HACK
cfe_devctx_t * myctx;

int my_write( uint32_t length, char * msg );
int my_write( uint32_t length, char * msg ) {
	iocb_buffer_t data;

	data.buf_ptr    = msg;
	data.buf_length = length;

	return bcm97115_uart_write( myctx, &data );
}
#endif


static int bcm97115_uart_open(cfe_devctx_t *ctx)
{
	bcm97115_uart_t * softc = ctx->dev_softc;

	softc->baudrate = CFG_SERIAL_BAUD_RATE;
	bcm97115_set_baudrate( softc );

	WRITEREG( softc->rxstat, 0x0 );
	WRITEREG( softc->txstat, 0x0 );
	WRITEREG( softc->ctrl, BITM8 | TXEN | RXEN );

#ifdef RAW_WRITE_HACK
	myctx = ctx;
#endif

	return 0;
}


static int bcm97115_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	bcm97115_uart_t * softc = ctx->dev_softc;
	unsigned char * bptr;
	int blen;
	uint32_t status;
	char inval;

	bptr = buffer->buf_ptr;
	blen = buffer->buf_length;

	while (blen > 0) {
		status = READREG(softc->rxstat);
		if(status & (RXOVFERR | RXPARERR | RXFRAMERR)) {
			/* Just read the bad character to clear the bit. */
			inval = READREG( softc->rxdata ) & 0xFF;
		} else if(status & RXRDA) {
			*bptr++ = READREG( softc->rxdata ) & 0xFF;
			blen--;
		}
		else
			break;
	}

	buffer->buf_retlen = buffer->buf_length - blen;
	return 0;
}


static int bcm97115_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
	bcm97115_uart_t * softc = ctx->dev_softc;

	inpstat->inp_status = READREG(softc->rxstat) ? 1 : 0;

	return 0;
}


static int bcm97115_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	bcm97115_uart_t * softc = ctx->dev_softc;
	unsigned char * bptr;
	int      blen;
	uint32_t status;

	bptr = buffer->buf_ptr;
	blen = buffer->buf_length;

	while( blen > 0 ) {
		do {
			status = READREG(softc->txstat) & TXDRE;
		} while( !status );
		WRITEREG( softc->txdata, *bptr );

		bptr++;
		blen--;
	}

	buffer->buf_retlen = buffer->buf_length - blen;

	return 0;
}


static int bcm97115_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	bcm97115_uart_t * softc = ctx->dev_softc;
	unsigned int * info = (unsigned int *) buffer->buf_ptr;

	switch ((int)buffer->buf_ioctlcmd) {
		case IOCTL_SERIAL_GETSPEED:
		     *info = softc->baudrate;
		     break;
		case IOCTL_SERIAL_SETSPEED:
		     softc->baudrate = *info;
		     bcm97115_set_baudrate( softc );
		     break;
		case IOCTL_SERIAL_GETFLOW:
		     *info = SERIAL_FLOW_HARDWARE;
		     break;
		case IOCTL_SERIAL_SETFLOW:
		     /* Fall through */
		default:
		     return -1;
	}

	return 0;
}


static int bcm97115_uart_close(cfe_devctx_t *ctx)
{
	uint32_t tmpreg;
	bcm97115_uart_t * softc = ctx->dev_softc;

	tmpreg  = READREG( softc->ctrl );
	tmpreg &= ~(TXEN | RXEN);
	WRITEREG( softc->ctrl, tmpreg );

	return 0;
}
