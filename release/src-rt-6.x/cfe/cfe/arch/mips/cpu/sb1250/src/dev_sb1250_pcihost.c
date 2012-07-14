/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SB1250 PCI host driver			File: dev_sb1250_pcihost.c
    *  
    *  This is a simple driver to support device-mode downloading via
    *  PCI from the host system.
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
#include "lib_physio.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"

#include "bsp_config.h"

static void sb1250_hostpci_probe(cfe_driver_t *drv,
				 unsigned long probe_a, unsigned long probe_b, 
				 void *probe_ptr);


static int sb1250_host_open(cfe_devctx_t *ctx);
static int sb1250_host_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer);
static int sb1250_host_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat);
static int sb1250_host_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer);
static int sb1250_host_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer);
static int sb1250_host_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t sb1250_host_dispatch = {
    sb1250_host_open,
    sb1250_host_read,
    sb1250_host_inpstat,
    sb1250_host_write,
    sb1250_host_ioctl,
    sb1250_host_close,	
    NULL,
    NULL
};

const cfe_driver_t sb1250_pcihost = {
    "SB1250 PCI HOST",
    "host",
    CFE_DEV_OTHER,
    &sb1250_host_dispatch,
    sb1250_hostpci_probe
};

typedef struct sb1250_host_s {
    uint64_t pci_base;
    uint32_t offset;
    uint32_t limit;
} sb1250_host_t;

static void
sb1250_hostpci_probe(cfe_driver_t *drv,
		     unsigned long probe_a, unsigned long probe_b, 
		     void *probe_ptr)
{
    sb1250_host_t *softc;
    char descr[80];

    /* probe_a, probe_b, and probe_ptr are unused. */

    softc = (sb1250_host_t *) KMALLOC(sizeof(sb1250_host_t), 0);
    if (softc) {
        softc->pci_base = 0;
	softc->offset = 0;
	softc->limit = 0;

	xsprintf(descr,"%s", drv->drv_description);
	cfe_attach(drv,softc,NULL,descr);
	}
}


#define K1_PTR64(pa) ((uint64_t *)(PHYS_TO_K1(pa)))
#define PCI_TO_CPU(addr) \
    ((physaddr_t)((uint64_t)(addr) | 0xf800000000))

static int
sb1250_host_open(cfe_devctx_t *ctx)
{
    sb1250_host_t *softc = ctx->dev_softc;
    uint64_t *mbox_p = K1_PTR64 (A_IMR_REGISTER(0, R_IMR_MAILBOX_CPU));
    uint32_t *cmd_p = ((uint32_t *)mbox_p) + 1;
    uint32_t *arg_p = ((uint32_t *)mbox_p) + 0;
    physaddr_t desc_addr, data_addr;    /* in PCI full-access space */


    /* The caller has probably done this already. */
    while ((*cmd_p & 0x3) == 0) {
	POLL();    /* timeout might be good */
	}

    desc_addr = PCI_TO_CPU(*arg_p);
    data_addr = phys_read32(desc_addr);
    softc->pci_base = PCI_TO_CPU(data_addr);
    softc->limit = phys_read32(desc_addr + 4);

    return 0;
}

static int
sb1250_host_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    sb1250_host_t *softc = ctx->dev_softc;
    uint32_t boffset;
    uint32_t blen;

    /* For random-access devices, there is an implicit seek with every
       read. We maintain a current offset in case a sequential mode is
       added. */
    boffset = (uint32_t)buffer->buf_offset;

    if (boffset >= softc->limit) {
	blen = 0;
	softc->offset = softc->limit;
	}
    else {
	uint64_t src;
	uint8_t b, *bptr;
	int i;

	src = softc->pci_base + boffset;
	bptr = buffer->buf_ptr;
	blen = buffer->buf_length;
	if (blen > softc->limit - boffset)
	  blen = softc->limit - boffset;

	/* copy the bytes (better to use the data mover?) */
	for (i = 0; i < blen; i++) {
	    b = phys_read8(src);
	    *bptr++ = b;
	    src++;
	    }

	softc->offset = boffset;
	}

    buffer->buf_retlen = blen;      

    return 0;
}

static int
sb1250_host_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    sb1250_host_t *softc = ctx->dev_softc;

    return softc->pci_base != 0 && softc->offset < softc->limit;
}

static int
sb1250_host_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    /* This is a readonly device */
    return -1;
}

static int
sb1250_host_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    /* None needed (for now?) */
    return -1;
}

static int
sb1250_host_close(cfe_devctx_t *ctx)
{
    /* Put any needed cleanup here. */
    return 0;
}
