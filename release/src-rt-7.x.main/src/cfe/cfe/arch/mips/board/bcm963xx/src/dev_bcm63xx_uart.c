#include "cfe.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"

#include "sbmips.h"


#define XTALFREQ 25000000

#define WRITEREG(csr,val) *((volatile uint32_t *)(csr)) = (val)
#define READREG(csr)     (*((volatile uint32_t *)(csr)))

#define UARTCTLREG_OFFSET  0x00000020
#define BAUDWRDREG_OFFSET  0x00000004
#define UARTMSCREG_OFFSET  0x00000008
#define UARTSTATUS_OFFSET  0x00000010
#define UARTDATAREG_OFFSET 0x00000014
#define UARTINTREG_OFFSET  0x0000000C


#define INTMASKREG   0xfffe000C
#define INT_UART0    0x00000004
#define INT_UART1    0x00000002
#define INT_TIMER    0x00000001

#define DEFAULTBAUD  115200

/* Control register */
#define BRGEN        0x00800000
#define TXEN         0x00400000
#define RXEN         0x00200000

#define BITS8SYM     0x00003000
#define ONESTOP      0x00000700

/* Miscellaneous register */
#define TX4          0x00004000
#define RX4          0x00000400
#define DTREN        0x00000001
#define RTSEN        0x00000002

/* Interrupt Mask / Status register */
#define RXBRK        0x40000000
#define RXPARERR     0x20000000
#define RXFRAMERR    0x10000000
#define RXOVFERR     0x00800000
#define RXFIFONE     0x00000800
#define RESETTXFIFO  0x00000080
#define RESETRXFIFO  0x00000040
#define TXFIFOEMT    0x00000020
#define TXUNDERR     0x00000002
#define TXOVFERR     0x00000004


static void bcm63xx_uart_probe(cfe_driver_t *drv,
                               unsigned long probe_a, unsigned long probe_b,
                               void *probe_ptr);

static int bcm63xx_uart_open(cfe_devctx_t *ctx);
static int bcm63xx_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm63xx_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm63xx_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm63xx_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm63xx_uart_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t bcm63xx_uart_dispatch = {
    bcm63xx_uart_open,
    bcm63xx_uart_read,
    bcm63xx_uart_inpstat,
    bcm63xx_uart_write,
    bcm63xx_uart_ioctl,
    bcm63xx_uart_close,
    NULL,
    NULL
};


const cfe_driver_t bcm63xx_uart = {
    "BCM63xx DUART",
    "uart",
    CFE_DEV_SERIAL,
    &bcm63xx_uart_dispatch,
    bcm63xx_uart_probe
};


typedef struct bcm63xx_uart_s {
	int      baudrate;
	uint32_t uartctlreg;
	uint32_t baudwrdreg;
	uint32_t uartmscreg;
	uint32_t uartintreg;

	uint32_t uart_status;
	uint32_t uart_tx_hold;
	uint32_t uart_rx_hold;
} bcm63xx_uart_t;


static void bcm63xx_set_baudrate( bcm63xx_uart_t * softc )
{
	uint32_t baudwd;

	baudwd = (XTALFREQ / softc->baudrate) / 8;
	if( baudwd & 0x1 ) {
		baudwd = baudwd / 2 - 1;
	}
	WRITEREG( softc->baudwrdreg, baudwd );
}


static void bcm63xx_uart_probe(cfe_driver_t *drv,
                               unsigned long probe_a, unsigned long probe_b,
                               void *probe_ptr)
{
	bcm63xx_uart_t * softc;
	char descr[80];

	/* enable the transmitter interrupt? */

    /*
     * probe_a is the DUART base address.
     * probe_b is the channel-number-within-duart (0 or 1)
     * probe_ptr is unused.
     */
    softc = (bcm63xx_uart_t *) KMALLOC(sizeof(bcm63xx_uart_t),0);
    if (softc) {
		softc->uartctlreg = probe_a + probe_b * UARTCTLREG_OFFSET;
		softc->baudwrdreg = softc->uartctlreg + BAUDWRDREG_OFFSET;
		softc->uartmscreg = softc->uartctlreg + UARTMSCREG_OFFSET;
		softc->uartintreg = softc->uartctlreg + UARTINTREG_OFFSET;

		softc->uart_status  = softc->uartctlreg + UARTSTATUS_OFFSET;
		softc->uart_tx_hold = softc->uartctlreg + UARTDATAREG_OFFSET;
		softc->uart_rx_hold = softc->uartctlreg + UARTDATAREG_OFFSET;

		xsprintf( descr, "%s at 0x%X channel %d", drv->drv_description, probe_a,
		                                                            probe_b );
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

	return bcm63xx_uart_write( myctx, &data );
}
#endif


static int bcm63xx_uart_open(cfe_devctx_t *ctx)
{
	uint32_t tmpreg;
	bcm63xx_uart_t * softc = ctx->dev_softc;

	/* Enable the UART clock */
	tmpreg = READREG( 0xFFFE0004 );
	tmpreg |= 0x00000008;
	WRITEREG( 0xFFFE0004, tmpreg );


	softc->baudrate = DEFAULTBAUD;
	bcm63xx_set_baudrate( softc );

	WRITEREG( softc->uartctlreg, BRGEN | TXEN | RXEN |
	                               BITS8SYM    | ONESTOP     |
	                               RESETTXFIFO | RESETRXFIFO |
	                               0x00000005 );

	WRITEREG( softc->uartmscreg, TX4 | RX4 );
	WRITEREG( softc->uartintreg, 0x0 );

#ifdef RAW_WRITE_HACK
	myctx = ctx;
#endif

	return 0;
}


static int bcm63xx_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	bcm63xx_uart_t * softc = ctx->dev_softc;
	unsigned char * bptr;
	int blen;
	uint32_t status;
	char inval;
	uint32_t ctrlreg;

	bptr = buffer->buf_ptr;
	blen = buffer->buf_length;

    while (blen > 0) {
		status = READREG(softc->uart_status);
        if(status & (RXOVFERR | RXPARERR | RXFRAMERR | RXBRK)) {
            /* RX over flow */
            if(status & RXOVFERR) {
                /* reset RX FIFO to clr interrupt */
                /* pChan->uartChannel->fifoctl |= RSTRXFIFOS; */
				ctrlreg = READREG(softc->uartctlreg);
				ctrlreg &= ~RESETRXFIFO;
				WRITEREG( softc->uartctlreg, ctrlreg );
            }

            /* other errors just read the bad character to clear the bit */
            inval = READREG( softc->uart_rx_hold ) & 0xFF;
        }
        else if(status & RXFIFONE) {
			*bptr++ = READREG( softc->uart_rx_hold ) & 0xFF;
			blen--;
        }
        else
            break;
    }

	buffer->buf_retlen = buffer->buf_length - blen;
	return 0;
}


static int bcm63xx_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
	bcm63xx_uart_t * softc = ctx->dev_softc;

	inpstat->inp_status = READREG(softc->uart_status) & RXFIFONE;

	return 0;
}


static int bcm63xx_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	bcm63xx_uart_t * softc = ctx->dev_softc;
	unsigned char * bptr;
	int blen;
	uint32_t status;
	uint32_t control;

	bptr = buffer->buf_ptr;
	blen = buffer->buf_length;

	status = 0;
	while( (blen > 0) && !status ) {
		/* Wait for the buffer to empty before we write the next character */
		/*         character at a time.  Why doesn't it work though?       */
		do {
			status = READREG( softc->uart_status ) & TXFIFOEMT;
		} while( !status );
		WRITEREG( softc->uart_tx_hold, *bptr );
		bptr++;
		blen--;

		status  = READREG( softc->uart_status )  & (TXOVFERR|TXUNDERR);
	}

	if( status ) {
		/* Reset TX FIFO */
		control  = READREG( softc->uartctlreg );
		control |= RESETTXFIFO;
		WRITEREG( softc->uartctlreg, control );
		blen++;
	}

	buffer->buf_retlen = buffer->buf_length - blen;
	return 0;
}


static int bcm63xx_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	uint32_t tmpreg;
	bcm63xx_uart_t * softc = ctx->dev_softc;
	unsigned int * info = (unsigned int *) buffer->buf_ptr;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_SERIAL_GETSPEED:
	    *info = softc->baudrate;
	    break;
	case IOCTL_SERIAL_SETSPEED:
	    softc->baudrate = *info;
		bcm63xx_set_baudrate( softc );
	    break;
	case IOCTL_SERIAL_GETFLOW:
		tmpreg  = READREG( softc->uartmscreg );
		if( tmpreg & (DTREN | RTSEN) ) {
			*info = SERIAL_FLOW_HARDWARE;
		} else {
			*info = SERIAL_FLOW_SOFTWARE;
		}
	    break;
	case IOCTL_SERIAL_SETFLOW:
		tmpreg  = READREG( softc->uartmscreg );
		switch( *info ) {
			case SERIAL_FLOW_NONE:
			case SERIAL_FLOW_SOFTWARE:
				tmpreg &= ~(DTREN | RTSEN);
				break;
			case SERIAL_FLOW_HARDWARE:
				tmpreg |= DTREN | RTSEN;
				break;
		}
		WRITEREG( softc->uartmscreg, tmpreg );
	    break;
	default:
	    return -1;
	}

	return 0;
}


static int bcm63xx_uart_close(cfe_devctx_t *ctx)
{
	uint32_t tmpreg;
	bcm63xx_uart_t * softc = ctx->dev_softc;


	/* Turn off the UART clock. */
	tmpreg = READREG( 0xFFFE0004 );
	tmpreg &= ~( 0x00000008 );
	WRITEREG( 0xFFFE0004, tmpreg );

	/* Restore the Interrupt Status Register to its default state. */
	tmpreg  = READREG( INTMASKREG );
	tmpreg |= ( INT_UART1 | INT_UART0 );
	WRITEREG( INTMASKREG, tmpreg );

/*****************************/

	/* Restore the baudword register */
	WRITEREG( softc->baudwrdreg, 0x0 );

	/* Restore the UART control register */
	WRITEREG( softc->uartctlreg, 0x00003704 );

	/* Restore the UART Miscellaneous Control Register */
	WRITEREG( softc->uartmscreg, 0x00008800 );

	/* Restore the UART Interrupt Mask / Status Register */
	WRITEREG( softc->uartintreg, 0x0 );

	return 0;
}
