/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM1250 (BCM1250 as PCI device) driver	   File: dev_bcm1250.c
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

#ifndef _SB_MAKE64
#define _SB_MAKE64(x) ((uint64_t)(x))
#endif
#ifndef _SB_MAKEMASK1
#define _SB_MAKEMASK1(n) (_SB_MAKE64(1) << _SB_MAKE64(n))
#endif

#include "lib_types.h"
#include "lib_physio.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_error.h"
#include "cfe_device.h"

#include "pcivar.h"
#include "pcireg.h"

#include "bsp_config.h"

/* Note that PTR_TO_PHYS only works with 32-bit addresses */
#define PTR_TO_PHYS(x) (K0_TO_PHYS((uint32_t)(uintptr_t)(x)))


static void bcm1250_probe(cfe_driver_t *drv,
			  unsigned long probe_a, unsigned long probe_b, 
			  void *probe_ptr);

static int bcm1250_open(cfe_devctx_t *ctx);
static int bcm1250_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm1250_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm1250_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm1250_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm1250_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t bcm1250_dispatch = {
    bcm1250_open,
    bcm1250_read,
    bcm1250_inpstat,
    bcm1250_write,
    bcm1250_ioctl,
    bcm1250_close,	
    NULL,
    NULL
};

const cfe_driver_t bcm1250drv = {
    "BCM1250",
    "widget",
    CFE_DEV_OTHER,
    &bcm1250_dispatch,
    bcm1250_probe
};


typedef struct bcm1250_s {
    physaddr_t mailbox;
    physaddr_t mem_base;
    uint8_t    irq;              /* interrupt mapping */
    pcitag_t   tag;              /* tag for  configuration register */

    int        downloaded;       /* code has already been downloaded. */
} bcm1250_t;


/*
 * BCM1250_PROBE
 *   probe_a, probe_b and probe_ptr all unused
 */

static void
bcm1250_probe(cfe_driver_t *drv,
	      unsigned long probe_a, unsigned long probe_b, 
	      void *probe_ptr)
{
    int  index;

    index = 0;
    for (;;) {
	pcitag_t tag;

	if (pci_find_device(0x166d, 0x0001, index, &tag) != 0)
	   break;

	if (tag != 0x00000000) {   /* don't configure ourselves */
	    bcm1250_t *softc;
	    char descr[80];
	    phys_addr_t pa;

	    softc = (bcm1250_t *) KMALLOC(sizeof(bcm1250_t), 0);
	    if (softc == NULL) {
	        xprintf("BCM1250: No memory to complete probe\n");
		break;
	    }

	    softc->tag = tag;

	    pci_map_mem(tag, PCI_MAPREG(0), PCI_MATCH_BYTES, &pa);
	    xsprintf(descr, "%s at 0x%X", drv->drv_description, (uint32_t)pa);
	    softc->mem_base = pa;

	    /* Map the CPU0 mailbox registers of the device 1250.
               Note that our BAR2 space maps to its "alias" mailbox
               registers.  Set bit 3 for mbox_set; clear bit 3 for
               reading.  Address bits 15-4 are don't cares. */
	    pci_map_mem(tag, PCI_MAPREG(2), PCI_MATCH_BYTES, &pa);
	    softc->mailbox = pa;

	    softc->downloaded = 0;

	    cfe_attach(drv, softc, NULL, descr);
	}
	index++;
    }
}


#include "elf.h"

static int
elf_header (const uint8_t *hdr)
{
    return (hdr[EI_MAG0] == ELFMAG0 &&
	    hdr[EI_MAG1] == ELFMAG1 &&
	    hdr[EI_MAG2] == ELFMAG2 &&
	    hdr[EI_MAG3] == ELFMAG3);
}


#include "cfe_timer.h"

typedef struct {
    uint32_t addr;          /* source address, in device's PCI space */
    uint32_t len;           /* length of this chunk */
} chunk_desc;


#define MBOX_SET_BIT  0x8

extern void download_start(void), download_end(void);

static int
bcm1250_open(cfe_devctx_t *ctx)
{
    bcm1250_t *softc = ctx->dev_softc;
    physaddr_t cmd_p = softc->mailbox + 4;
    
    if (softc->downloaded) {
	xprintf("bcm1250_open: Warning: Device previously downloaded\n");
	softc->downloaded = 0;
	}

    if (phys_read32(cmd_p) != 0) {
	xprintf("bcm1250_open: Device not in initial state\n");
	return -1;
	}

    return 0;
}

static int
bcm1250_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    return -1;
}

static int
bcm1250_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    return -1;
}

static int
bcm1250_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    bcm1250_t *softc = ctx->dev_softc;
    physaddr_t arg_p = softc->mailbox + 0;
    physaddr_t cmd_p = softc->mailbox + 4;
    chunk_desc code;
    uint32_t cmd;
    int64_t timer;
    int res;
    
    /* Note: This code assumes that PTR_TO_PHYS gives a PCI memory space
       address that is accessible via our BAR4 or BAR5 */

    code.addr = PTR_TO_PHYS((uint8_t *)buffer->buf_ptr);
    code.len  = buffer->buf_length;

    cmd = 0x1;      /* load */
    if (!elf_header((uint8_t *)buffer->buf_ptr)) {
	/* No recognizable elf seal, so assume compressed. */
	cmd |= 0x2;
	}

    phys_write32(arg_p | MBOX_SET_BIT, PTR_TO_PHYS(&code));
    phys_write32(cmd_p | MBOX_SET_BIT, cmd);     /* load */

    /* Wait for handshake */

    res = CFE_ERR_TIMEOUT;
    TIMER_SET(timer, 5*CFE_HZ);
    while (!TIMER_EXPIRED(timer)) {
        if ((phys_read32(cmd_p) & 0x3) == 0) {
	    softc->downloaded = 1;
	    buffer->buf_retlen = 0;
	    /* Note that the result code need not be translated only
               because we are assuming a CFE in the device that is
               compatible with us. */
	    res = (int)phys_read32(arg_p);
	    break;
	    }
	POLL();
	}

    return res;
}

static int
bcm1250_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    return -1;
}

static int
bcm1250_close(cfe_devctx_t *ctx)
{
    return 0;
}
