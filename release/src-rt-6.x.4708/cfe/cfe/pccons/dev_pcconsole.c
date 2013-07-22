/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PC Console driver			File: dev_pcconsole.c
    *  
    *  A console driver for a PC-style keyboard and mouse
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




#include "sbmips.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"

#include "lib_physio.h"

#include "kbd_subr.h"
#include "vga_subr.h"

#include "bsp_config.h"
#include "pcireg.h"
#include "pcivar.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define KBD_RXFULL		1		/* bit set if kb has data */
#define KBD_TXFULL		2		/* bit set if we can send cmd */
#define VGA_TEXTBUF_COLOR	0xB8000		/* VGA frame buffer */

#if defined(_P5064_) || defined(_P6064_)
  #define PCI_MEM_SPACE	0x10000000	/* 128MB: s/w configurable */
  #define __ISAaddr(addr) ((physaddr_t)(PCI_MEM_SPACE+(addr)))
#else
  #define __ISAaddr(addr) ((physaddr_t)0x40000000+(addr))
#endif

#define cpu_isamap(x,y) __ISAaddr(x)

/*  *********************************************************************
    *  Forward references
    ********************************************************************* */

static void pcconsole_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int pcconsole_open(cfe_devctx_t *ctx);
static int pcconsole_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int pcconsole_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int pcconsole_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int pcconsole_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int pcconsole_close(cfe_devctx_t *ctx);
static void pcconsole_poll(cfe_devctx_t *ctx,int64_t ticks);

const static cfe_devdisp_t pcconsole_dispatch = {
    pcconsole_open,
    pcconsole_read,
    pcconsole_inpstat,
    pcconsole_write,
    pcconsole_ioctl,
    pcconsole_close,	
    pcconsole_poll,
    NULL
};

const cfe_driver_t pcconsole = {
    "PC Console",
    "pcconsole",
    CFE_DEV_SERIAL,
    &pcconsole_dispatch,
    pcconsole_probe
};


/*  *********************************************************************
    *  Structures
    ********************************************************************* */


typedef struct pcconsole_s {
    vga_term_t vga;
    keystate_t ks;
    uint32_t kbd_status;
    uint32_t kbd_data;
} pcconsole_t;


/*  *********************************************************************
    *  pcconsole_poll(ctx,ticks)
    *  
    *  Poll routine - check for new keyboard events
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   ticks - current time
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


static void pcconsole_poll(cfe_devctx_t *ctx,int64_t ticks)
{
    pcconsole_t *softc = ctx->dev_softc;
    uint8_t status;
    uint8_t b;

    status = inb(softc->kbd_status);

    if (status & KBD_RXFULL) {
	b = inb(softc->kbd_data);
	kbd_doscan(&(softc->ks),b);
	}
}

/*  *********************************************************************
    *  pcconsole_waitcmdready(softc)
    *  
    *  Wait for the keyboard to be ready to accept a command
    *  
    *  Input parameters: 
    *  	   softc - console structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void pcconsole_waitcmdready(pcconsole_t *softc)
{
    uint8_t status;
    uint8_t data;

    for (;;) {
	status = inb(softc->kbd_status);	/* read status */
	if (status & KBD_RXFULL) {
	    data = inb(softc->kbd_data);	/* get data */
	    kbd_doscan(&(softc->ks),data);	/* process scan codes */
	    }
	if (!(status & KBD_TXFULL)) break;	/* stop when kbd ready */
	}
}


/*  *********************************************************************
    *  pcconsole_setleds(ks,leds)
    *  
    *  Callback from the keyboard routines for setting the LEDS
    *  
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   leds - new LED state
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int pcconsole_setleds(keystate_t *ks,int leds)
{
    pcconsole_t *softc = kbd_getref(ks);

    pcconsole_waitcmdready(softc);
    outb(softc->kbd_data,KBDCMD_SETLEDS);
    pcconsole_waitcmdready(softc);
    outb(softc->kbd_data,(leds & 7));

    return 0;
}


/*  *********************************************************************
    *  pcconsole_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Probe routine.  This routine sets up the pcconsole device
    *  
    *  Input parameters: 
    *  	   drv - driver structure
    *  	   probe_a
    *  	   probe_b
    *  	   probe_ptr
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void pcconsole_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    pcconsole_t *softc;
    char descr[80];
    volatile uint8_t *isamem;

    /* 
     * probe_a is
     * probe_b is     
     * probe_ptr is 
     */

    softc = (pcconsole_t *) KMALLOC(sizeof(pcconsole_t),0);
    if (softc) {
	softc->kbd_status = 0x64;
	softc->kbd_data =   0x60;
	kbd_init(&(softc->ks),pcconsole_setleds,softc);

	isamem = (volatile uint8_t *) ((uintptr_t)cpu_isamap(0,1024*1024));
	vga_init(&(softc->vga),__ISAaddr(VGA_TEXTBUF_COLOR),outb);

	xsprintf(descr,"%s",drv->drv_description,probe_a,probe_b);
	cfe_attach(drv,softc,NULL,descr);
	}

}


static int pcconsole_open(cfe_devctx_t *ctx)
{
    pcconsole_t *softc = ctx->dev_softc;

    outb(softc->kbd_data,KBDCMD_RESET);		/* reset keyboard */
    kbd_init(&(softc->ks),pcconsole_setleds,softc);
    vga_clear(&(softc->vga));
    vga_setcursor(&(softc->vga),0,0);
    
    return 0;
}

static int pcconsole_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    pcconsole_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    pcconsole_poll(ctx,0);

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while ((blen > 0) && (kbd_inpstat(&(softc->ks)))) {
	*bptr++ = (kbd_read(&(softc->ks)) & 0xFF);
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

static int pcconsole_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    pcconsole_t *softc = ctx->dev_softc;

    pcconsole_poll(ctx,0);

    inpstat->inp_status = kbd_inpstat(&(softc->ks)) ? 1 : 0;

    return 0;
}

static int pcconsole_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    pcconsole_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    vga_writestr(&(softc->vga),bptr,7,blen);

    buffer->buf_retlen = buffer->buf_length;
    return 0;
}

static int pcconsole_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
/*    pcconsole_t *softc = ctx->dev_softc;*/

    return -1;
}

static int pcconsole_close(cfe_devctx_t *ctx)
{
/*    pcconsole_t *softc = ctx->dev_softc;*/

    return 0;
}
