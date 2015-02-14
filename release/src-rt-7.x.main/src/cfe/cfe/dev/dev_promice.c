/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PromICE console device			File: dev_promice.c
    *  
    *  This device driver supports Grammar Engine's PromICE AI2
    *  serial communications options.  With this console, you can
    *  communicate with the firmware using only uncached reads in the
    *  boot ROM space.  See Grammar Engine's PromICE manuals
    *  for more information at http://www.gei.com
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


/*
 * Example PromICE initialization file:
 *
 * -----------------------
 * output=com1
 * pponly=lpt1
 * rom=27040
 * word=8
 * file=cfe.srec
 * ailoc 7FC00,9600
 * -----------------------
 *
 * The offset specified in the 'ailoc' line must be the location where you
 * will configure the AI2 serial port.  In this example, the ROM is assumed
 * to be 512KB, and the AI2 serial port is at 511KB, or offset 0x7FC00.
 * This area is filled with 0xCC to detect AI2's initialization sequence
 * properly (see the PromICE manual).  You should connect your 
 * PromICE's serial port up to the PC and run a terminal emulator on it.
 * The parallel port will be used for downloading data to the PromICE.
 * 
 * If you have connected the write line to the PromICE, then you can
 * define the _AIDIRT_ symbol to increase performance.
 */



#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "addrspace.h"

#define _AIDIRT_

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static void promice_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int promice_open(cfe_devctx_t *ctx);
static int promice_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int promice_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int promice_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int promice_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int promice_close(cfe_devctx_t *ctx);

/*  *********************************************************************
    *  Device dispatch table
    ********************************************************************* */

const static cfe_devdisp_t promice_dispatch = {
    promice_open,
    promice_read,
    promice_inpstat,
    promice_write,
    promice_ioctl,
    promice_close,	
    NULL,
    NULL
};

/*  *********************************************************************
    *  Device descriptor
    ********************************************************************* */

const cfe_driver_t promice_uart = {
    "PromICE AI2 Serial Port",
    "promice",
    CFE_DEV_SERIAL,
    &promice_dispatch,
    promice_probe
};

/*  *********************************************************************
    *  Local constants and structures
    ********************************************************************* */

/*
 * If your PromICE is connected to a 32-bit host (emulating four
 * flash ROMs) and the SB1250 is set to boot from that host, define
 * the PROMICE_32BITS symbol to make sure the AI2 interface is
 * configured correctly.
 */

/*#define PROMICE_32BITS*/

#ifdef PROMICE_32BITS
#define WORDSIZE	4
#define WORDTYPE	uint32_t
#else
#define WORDSIZE	1
#define WORDTYPE	uint8_t
#endif


#define ZERO_OFFSET	(0)
#define ONE_OFFSET	(1)
#define DATA_OFFSET	(2)
#define STATUS_OFFSET	(3)

#define TDA 0x01 	/* Target data available */
#define HDA 0x02 	/* Host data available */
#define OVR 0x04 	/* Host data overflow */

typedef struct promice_s {
    unsigned long ai2_addr;
    unsigned int ai2_wordsize;
    volatile WORDTYPE *zero;
    volatile WORDTYPE *one;
    volatile WORDTYPE *data;
    volatile WORDTYPE *status;
} promice_t;


/*  *********************************************************************
    *  promice_probe(drv,probe_a,probe_b,probe_ptr)
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


static void promice_probe(cfe_driver_t *drv,
			  unsigned long probe_a, 
			  unsigned long probe_b, 
			  void *probe_ptr)
{
    promice_t *softc;
    char descr[80];

    /* 
     * probe_a is the address in the ROM of the AI2 interface
     * on the PromICE.  
     * probe_b is the word size (1,2,4)
     */

    softc = (promice_t *) KMALLOC(sizeof(promice_t),0);
    if (softc) {
	softc->ai2_addr = probe_a;
	if (probe_b) softc->ai2_wordsize = probe_b;
	else softc->ai2_wordsize = WORDSIZE;
	xsprintf(descr,"%s at 0x%X",drv->drv_description,probe_a);
	cfe_attach(drv,softc,NULL,descr);
	}
}



/*  *********************************************************************
    *  promice_open(ctx)
    *  
    *  Open the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */
static int promice_open(cfe_devctx_t *ctx)
{
    promice_t *softc = ctx->dev_softc; 
    uint8_t dummy;

    softc->zero   = (volatile WORDTYPE *) 
	UNCADDR(softc->ai2_addr + (ZERO_OFFSET*softc->ai2_wordsize));
    softc->one    = (volatile WORDTYPE *) 
	UNCADDR(softc->ai2_addr + (ONE_OFFSET*softc->ai2_wordsize));
    softc->data   = (volatile WORDTYPE *) 
	UNCADDR(softc->ai2_addr + (DATA_OFFSET*softc->ai2_wordsize));
    softc->status = (volatile WORDTYPE *) 
	UNCADDR(softc->ai2_addr + (STATUS_OFFSET*softc->ai2_wordsize));

    /*
     * Wait for bit 3 to clear so we know the interface is ready.
     */

    while (*(softc->status) == 0xCC) ; 	/* NULL LOOP */

    /* 
     * a dummy read is required to clear out the interface.
     */

    dummy = *(softc->data);

    return 0;
}

/*  *********************************************************************
    *  promice_read(ctx,buffer)
    *  
    *  Read data from the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - I/O buffer descriptor
    *  	   
    *  Return value:
    *  	   number of bytes transferred, or <0 if error
    ********************************************************************* */
static int promice_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    promice_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while ((blen > 0) && (*(softc->status) & HDA)) {
	*bptr++ = *(softc->data);
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

/*  *********************************************************************
    *  promice_inpstat(ctx,inpstat)
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

static int promice_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    promice_t *softc = ctx->dev_softc;

    inpstat->inp_status = (*(softc->status) & HDA) ? 1 : 0;

    return 0;
}

/*  *********************************************************************
    *  promice_write(ctx,buffer)
    *  
    *  Write data to the device
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - I/O buffer descriptor
    *  	   
    *  Return value:
    *  	   number of bytes transferred, or <0 if error
    ********************************************************************* */
static int promice_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    promice_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;
    uint8_t data;
#ifndef _AIDIRT_
    uint8_t dummy;
    int count;
#endif

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;


    /*
     * The AI2 interface requires you to transmit characters
     * one bit at a time.  First a '1' start bit,
     * then 8 data bits (lsb first) then another '1' stop bit.
     *
     * Just reference the right memory location to transmit a bit.
     */

    while ((blen > 0) && !(*(softc->status) & TDA)) {

#ifdef _AIDIRT_
	data = *bptr++;
	*(softc->zero) = data;
#else
	dummy = *(softc->one);		/* send start bit */

	data = *bptr++;

	for (count = 0; count < 8; count++) {
	    if (data & 1) dummy = *(softc->one);
	    else dummy = *(softc->zero);
	    data >>= 1; 		/* shift in next bit */
	    }

	dummy = *(softc->one);		/* send stop bit */
#endif

	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

/*  *********************************************************************
    *  promice_ioctl(ctx,buffer)
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

static int promice_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    return -1;
}

/*  *********************************************************************
    *  promice_close(ctx)
    *  
    *  Close the device.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int promice_close(cfe_devctx_t *ctx)
{
    return 0;
}
