/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SB1250 UART driver			File: dev_sb1250_uart.c
    *  
    *  This is a console device driver for the SB1250's on-chip UARTs
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */




#include "cfe.h"
#include "sbmips.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_uart.h"

#include "bsp_config.h"

#ifdef _MAGICWID_
#undef V_DUART_BAUD_RATE
#define V_DUART_BAUD_RATE(x) ((*((uint64_t *) 0xBFC00018)*1000000)/((x)*20)-1)
#endif

#ifdef SB1250_REFCLK_HZ
#undef V_DUART_BAUD_RATE
#define V_DUART_BAUD_RATE(x) ((SB1250_REFCLK_HZ)/((x)*20)-1)
#endif

static void sb1250_uart_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int sb1250_uart_open(cfe_devctx_t *ctx);
static int sb1250_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int sb1250_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_uart_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t sb1250_uart_dispatch = {
    sb1250_uart_open,
    sb1250_uart_read,
    sb1250_uart_inpstat,
    sb1250_uart_write,
    sb1250_uart_ioctl,
    sb1250_uart_close,	
    NULL,
    NULL
};

const cfe_driver_t sb1250_uart = {
    "SB1250 DUART",
    "uart",
    CFE_DEV_SERIAL,
    &sb1250_uart_dispatch,
    sb1250_uart_probe
};

typedef struct sb1250_uart_s {
    unsigned long uart_mode_reg_1;
    unsigned long uart_mode_reg_2;
    unsigned long uart_clk_sel;
    unsigned long uart_cmd;
    unsigned long uart_imr;
    unsigned long uart_status;
    unsigned long uart_tx_hold;
    unsigned long uart_rx_hold;
    unsigned long uart_oprset;
    int uart_speed;
    int uart_flowcontrol;
    int uart_channel;
} sb1250_uart_t;


#define SBDUARTWRITE(softc,reg,val) \
     (SBREADCSR(softc->uart_mode_reg_1),SBWRITECSR(softc->reg,val))

#define SBDUARTREAD(softc,reg) \
     (SBREADCSR(softc->uart_mode_reg_1),SBREADCSR(softc->reg))

static void sb1250_uart_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    sb1250_uart_t *softc;
    char descr[80];

    /* 
     * probe_a is the DUART base address.
     * probe_b is the channel-number-within-duart (0 or 1)
     * probe_ptr is unused.
     */

    softc = (sb1250_uart_t *) KMALLOC(sizeof(sb1250_uart_t),0);
    if (softc) {
	softc->uart_mode_reg_1 = probe_a + R_DUART_CHANREG(probe_b,R_DUART_MODE_REG_1);
	softc->uart_mode_reg_2 = probe_a + R_DUART_CHANREG(probe_b,R_DUART_MODE_REG_2);
	softc->uart_clk_sel    = probe_a + R_DUART_CHANREG(probe_b,R_DUART_CLK_SEL);
	softc->uart_cmd        = probe_a + R_DUART_CHANREG(probe_b,R_DUART_CMD);
	softc->uart_status     = probe_a + R_DUART_CHANREG(probe_b,R_DUART_STATUS);
	softc->uart_tx_hold    = probe_a + R_DUART_CHANREG(probe_b,R_DUART_TX_HOLD);
	softc->uart_rx_hold    = probe_a + R_DUART_CHANREG(probe_b,R_DUART_RX_HOLD);	
	softc->uart_imr        = probe_a + R_DUART_IMRREG(probe_b);
	softc->uart_oprset     = probe_a + R_DUART_SET_OPR;
	xsprintf(descr,"%s at 0x%X channel %d",drv->drv_description,probe_a,probe_b);
	softc->uart_speed = CFG_SERIAL_BAUD_RATE;
	softc->uart_flowcontrol = SERIAL_FLOW_NONE;
    
	cfe_attach(drv,softc,NULL,descr);
	}

}


static void sb1250_uart_setflow(sb1250_uart_t *softc)
{
    uint64_t mode1val;
    uint64_t mode2val;

    mode1val = SBDUARTREAD(softc,uart_mode_reg_1);
    mode2val = SBDUARTREAD(softc,uart_mode_reg_2);

    switch (softc->uart_flowcontrol) {
	case SERIAL_FLOW_NONE:
	case SERIAL_FLOW_SOFTWARE:
	    mode1val &= ~M_DUART_RX_RTS_ENA;
	    mode2val &= ~M_DUART_TX_CTS_ENA;
	    break;
	case SERIAL_FLOW_HARDWARE:
	    mode1val |= M_DUART_RX_RTS_ENA;
	    mode2val |= M_DUART_TX_CTS_ENA;
	    break;
	}

    SBDUARTWRITE(softc,uart_mode_reg_1,mode1val);
    SBDUARTWRITE(softc,uart_mode_reg_2,mode2val);
}


static int sb1250_uart_open(cfe_devctx_t *ctx)
{
    sb1250_uart_t *softc = ctx->dev_softc;

    SBDUARTWRITE(softc,uart_mode_reg_1,V_DUART_BITS_PER_CHAR_8 | V_DUART_PARITY_MODE_NONE);
    SBDUARTWRITE(softc,uart_mode_reg_2,M_DUART_STOP_BIT_LEN_1);
    SBDUARTWRITE(softc,uart_clk_sel,   V_DUART_BAUD_RATE(CFG_SERIAL_BAUD_RATE));
    SBDUARTWRITE(softc,uart_imr,       0);		/* DISABLE all interrupts */
    SBDUARTWRITE(softc,uart_cmd,       M_DUART_RX_EN | M_DUART_TX_EN);
    if (softc->uart_channel == 0) {
	SBDUARTWRITE(softc,uart_oprset,    M_DUART_SET_OPR0 | M_DUART_SET_OPR2);	/* CTS and DTR */
	}
    else {
	SBDUARTWRITE(softc,uart_oprset,    M_DUART_SET_OPR1 | M_DUART_SET_OPR3);	/* CTS and DTR */
	}
    sb1250_uart_setflow(softc);

    return 0;
}

static int sb1250_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    sb1250_uart_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while (blen > 0) {
	if (!(SBDUARTREAD(softc,uart_status) & M_DUART_RX_RDY)) break;
	*bptr++ = (SBDUARTREAD(softc,uart_rx_hold) & 0xFF);
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

static int sb1250_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    sb1250_uart_t *softc = ctx->dev_softc;

    inpstat->inp_status = (SBDUARTREAD(softc,uart_status) & M_DUART_RX_RDY) ? 1 : 0;

    return 0;
}

static int sb1250_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    sb1250_uart_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while (blen > 0) {
	if (!(SBDUARTREAD(softc,uart_status) & M_DUART_TX_RDY)) break;
	SBDUARTWRITE(softc,uart_tx_hold,*bptr++);
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

static int sb1250_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    sb1250_uart_t *softc = ctx->dev_softc;
    unsigned int *info = (unsigned int *) buffer->buf_ptr;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_SERIAL_GETSPEED:
	    *info = softc->uart_speed;
	    break;
	case IOCTL_SERIAL_SETSPEED:
	    softc->uart_speed = *info;
	    SBDUARTWRITE(softc,uart_clk_sel,
		       V_DUART_BAUD_RATE(softc->uart_speed));
	    break;
	case IOCTL_SERIAL_GETFLOW:
	    *info = softc->uart_flowcontrol;
	    break;
	case IOCTL_SERIAL_SETFLOW:
	    softc->uart_flowcontrol = *info;
	    sb1250_uart_setflow(softc);
	    break;
	default:
	    return -1;
	}

    return 0;
}

static int sb1250_uart_close(cfe_devctx_t *ctx)
{
    sb1250_uart_t *softc = ctx->dev_softc;

    SBDUARTWRITE(softc,uart_cmd,       0);
    return 0;
}
