/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  JTAG console device			File: dev_jtag.c
    *  
    *  This file supports a serial-port-style console over the
    *  BCM12500's JTAG port.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *           Kip Walker        (kwalker@broadcom.com)
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
#include "sb1250_defs.h"

#define JTAG_CONS_CONTROL  0x00
#define JTAG_CONS_INPUT    0x20
#define JTAG_CONS_OUTPUT   0x40
#define JTAG_CONS_MAGICNUM 0x50FABEEF12349873

#define jtag_input_len(data)    (((data) >> 56) & 0xFF)

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static void jtag_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int jtag_open(cfe_devctx_t *ctx);
static int jtag_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int jtag_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int jtag_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int jtag_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int jtag_close(cfe_devctx_t *ctx);

/*  *********************************************************************
    *  Device dispatch table
    ********************************************************************* */

const static cfe_devdisp_t jtag_dispatch = {
    jtag_open,
    jtag_read,
    jtag_inpstat,
    jtag_write,
    jtag_ioctl,
    jtag_close,	
    NULL,
    NULL
};

/*  *********************************************************************
    *  Device descriptor
    ********************************************************************* */

const cfe_driver_t jtagconsole = {
    "JTAG serial console",
    "jtag",
    CFE_DEV_SERIAL,
    &jtag_dispatch,
    jtag_probe
};

/*  *********************************************************************
    *  Local constants and structures
    ********************************************************************* */


typedef struct jtag_s {
    unsigned long jtag_input;
    unsigned long jtag_output;
    unsigned long jtag_control;
    uint64_t input_buf;
    int waiting_input;
} jtag_t;


/*  *********************************************************************
    *  jtag_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Device "Probe" routine.  This routine creates the soft
    *  context for the device and calls the attach routine.
    *  
    *  Input parameters: 
    *  	   drv - driver structure
    *  	   probe_a,probe_b,probe_ptr - probe args
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


static void jtag_probe(cfe_driver_t *drv,
			  unsigned long probe_a, 
			  unsigned long probe_b, 
			  void *probe_ptr)
{
    jtag_t *softc;
    char descr[80];

    /* 
     * probe_a is the physical base address within JTAG space of the
     * communications area.
     */

    softc = (jtag_t *) KMALLOC(sizeof(jtag_t),0);
    if (softc) {
	softc->jtag_input = probe_a + JTAG_CONS_INPUT;
	softc->jtag_output = probe_a + JTAG_CONS_OUTPUT;
	softc->jtag_control = probe_a + JTAG_CONS_CONTROL;
        softc->waiting_input = 0;
	xsprintf(descr,"%s at 0x%X",drv->drv_description,probe_a);
	cfe_attach(drv,softc,NULL,descr);
    }
}



/*  *********************************************************************
    *  jtag_open(ctx)
    *  
    *  Open the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */
static int jtag_open(cfe_devctx_t *ctx)
{
    jtag_t *softc = ctx->dev_softc; 
    int64_t timer;
    uint64_t magic;

    TIMER_SET(timer, 120*CFE_HZ);
    do {
      POLL();
      magic = SBREADCSR(softc->jtag_control);
    } while ((magic == 0) && !TIMER_EXPIRED(timer));

    if (magic != JTAG_CONS_MAGICNUM) {
      return -1;
    }

    return 0;
}

/*  *********************************************************************
    *  jtag_grab_dword(softc)
    *  
    *  Get the next dword (if any) from the JTAG client.  (Local helper
    *  function - not part of the device dispatch table).
    *  
    *  Input parameters: 
    *  	   softc - jtag structure
    ********************************************************************* */
static void jtag_grab_dword(jtag_t *softc)
{
    uint64_t inbuf;

    inbuf = SBREADCSR(softc->jtag_input);
    softc->waiting_input = jtag_input_len(inbuf);
    softc->input_buf = inbuf << 8;
}

/*  *********************************************************************
    *  jtag_read(ctx,buffer)
    *  
    *  Read data from the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - I/O buffer descriptor
    *  	   
    *  Return value:
    *  	   0 if successful, or <0 if error
    ********************************************************************* */
static int jtag_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    jtag_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    /* If the caller always inpstats first, we don't need to do this!  */
    if (softc->waiting_input == 0) {
        jtag_grab_dword(softc);
    }
    
    while ((blen > 0) && (softc->waiting_input)) {
        int bytes = blen > softc->waiting_input ? softc->waiting_input : blen;
        int i;

        /* Take min(waiting,blen) from input_buf */
        for (i=0; i<bytes; i++) {
            *bptr++ = (softc->input_buf >> 56) & 0xFF;
            softc->input_buf <<= 8;
        }

        softc->waiting_input -= bytes;
        blen -= bytes;

        if (softc->waiting_input == 0) {
            /* Look for more */
            jtag_grab_dword(softc);
        }
    }
    
    buffer->buf_retlen = buffer->buf_length - blen;

    return 0;
}

/*  *********************************************************************
    *  jtag_inpstat(ctx,inpstat)
    *  
    *  Determine if read data is available
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   inpstat - input status structure
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int jtag_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    jtag_t *softc = ctx->dev_softc;
    
    if (softc->waiting_input == 0) {
        jtag_grab_dword(softc);
    }

    inpstat->inp_status = softc->waiting_input ? 1 : 0;

    return 0;
}

/*  *********************************************************************
    *  jtag_write(ctx,buffer)
    *  
    *  Write data to the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - I/O buffer descriptor
    *  	   
    *  Return value:
    *  	   0 if successful, or <0 if error
    ********************************************************************* */
static int jtag_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    jtag_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen, bytes, i;
    uint64_t data;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    
    while (blen > 0) {
        bytes = (blen > 7) ? 7 : blen;
        data = bytes;
        for (i=0; i<bytes; i++) {
            data <<= 8;
            data |= *bptr++;
        }
        if (bytes < 7)
            data <<= 8 * (7-bytes);
        blen -= bytes;
        SBWRITECSR(softc->jtag_output, data);
    }

    buffer->buf_retlen = buffer->buf_length - blen; /* always 0 */
    return 0;
}

/*  *********************************************************************
    *  jtag_ioctl(ctx,buffer)
    *  
    *  Do I/O control operations
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - I/O control args
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int jtag_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    return -1;
}

/*  *********************************************************************
    *  jtag_close(ctx)
    *  
    *  Close the device.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int jtag_close(cfe_devctx_t *ctx)
{
    return 0;
}
