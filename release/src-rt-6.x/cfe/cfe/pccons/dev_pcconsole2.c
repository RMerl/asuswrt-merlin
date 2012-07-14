/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PC Console driver			File: dev_pcconsole2.c
    *  
    *  A console driver for a PC-style keyboard and mouse
    *
    *  This version is for USB keyboards.  Someday we'll consolidate
    *  everything.
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
#include "lib_string.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"

#include "lib_physio.h"

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

#define __ISAaddr(x)(0x40000000+(x))

/*  *********************************************************************
    *  Forward references
    ********************************************************************* */

int pcconsole_enqueue(uint8_t ch);

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

const cfe_driver_t pcconsole2 = {
    "PC Console (USB)",
    "pcconsole",
    CFE_DEV_SERIAL,
    &pcconsole_dispatch,
    pcconsole_probe
};


/*  *********************************************************************
    *  Structures
    ********************************************************************* */

#define KBD_QUEUE_LEN	32

typedef struct pcconsole_s {
    vga_term_t vga;
    int kbd_in;
    int kbd_out;
    uint8_t kbd_data[KBD_QUEUE_LEN];
} pcconsole_t;

static pcconsole_t *pcconsole_current = NULL;

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
    /* No polling needed, USB will do the work for us */
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

    /* 
     * probe_a is
     * probe_b is     
     * probe_ptr is 
     */

    softc = (pcconsole_t *) KMALLOC(sizeof(pcconsole_t),0);
    if (softc) {

	memset(softc,0,sizeof(pcconsole_t));

	vga_init(&(softc->vga),__ISAaddr(VGA_TEXTBUF_COLOR),outb);

	xsprintf(descr,"%s",drv->drv_description,probe_a,probe_b);
	cfe_attach(drv,softc,NULL,descr);
	}

}


static int pcconsole_open(cfe_devctx_t *ctx)
{
    pcconsole_t *softc = ctx->dev_softc;

    pcconsole_current = softc;

    softc->kbd_in = 0;
    softc->kbd_out = 0;

    vga_clear(&(softc->vga));
    vga_setcursor(&(softc->vga),0,0);
    
    return 0;
}

static int pcconsole_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    pcconsole_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while ((blen > 0) && (softc->kbd_in != softc->kbd_out)) {
	*bptr++ = softc->kbd_data[softc->kbd_out];
	softc->kbd_out++;
	if (softc->kbd_out >= KBD_QUEUE_LEN) {
	    softc->kbd_out = 0;
	    }
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

static int pcconsole_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    pcconsole_t *softc = ctx->dev_softc;

    POLL();

    inpstat->inp_status = (softc->kbd_in != softc->kbd_out);

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
    pcconsole_current = NULL;

    return 0;
}

/*
 * Called by USB system to queue characters.
 */
int pcconsole_enqueue(uint8_t ch)
{
    int newidx;

    if (!pcconsole_current) return -1;

    newidx = pcconsole_current->kbd_in+1;
    if (newidx >= KBD_QUEUE_LEN) newidx = 0;

    if (newidx == pcconsole_current->kbd_out) return -1;

    pcconsole_current->kbd_data[pcconsole_current->kbd_in] = ch;
    pcconsole_current->kbd_in = newidx;
    
    return 0;
}
