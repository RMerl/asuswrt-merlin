/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  ATAPI device driver			File: dev_atapi.c
    *  
    *  This is a simple driver for ATAPI devices.  The disks
    *  are expected to be connected to the generic bus (this
    *  driver doesn't support PCI).  
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
#include "cfe_timer.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"

#include "dev_ide_common.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define GETWORD_LE(buf,wordidx) (((unsigned int) (buf)[(wordidx)*2]) + \
             (((unsigned int) (buf)[(wordidx)*2+1]) << 8))


/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

extern void _wbflush(void);
static void atapidrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


const static cfe_devdisp_t atapidrv_dispatch = {
    idecommon_open,
    idecommon_read,
    idecommon_inpstat,
    idecommon_write,
    idecommon_ioctl,
    idecommon_close,	
    NULL,
    NULL
};

const cfe_driver_t atapidrv = {
    "ATAPI device",
    "atapi",
    CFE_DEV_DISK,
    &atapidrv_dispatch,
    atapidrv_probe
};



/*  *********************************************************************
    *  Port I/O routines
    *  
    *  These routines are called back from the common code to do
    *  I/O cycles to the IDE disk.  We provide routines for
    *  reading and writing bytes, words, and strings of words.
    ********************************************************************* */

static uint8_t atapidrv_inb(idecommon_dispatch_t *disp,uint32_t reg)
{
    return *((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr));
}

static uint16_t atapidrv_inw(idecommon_dispatch_t *disp,uint32_t reg)
{
    return *((volatile uint16_t *) PHYS_TO_K1((reg+disp->baseaddr)));
}

static void atapidrv_ins(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;

    while (len > 0) {
	data = *((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr));

#ifdef _BYTESWAP_
	*buf++ = (data >> 8) & 0xFF;
	*buf++ = (data & 0xFF);
#else
	*buf++ = (data & 0xFF);
	*buf++ = (data >> 8) & 0xFF;
#endif
	len--;
	len--;
	}

}

static void atapidrv_outb(idecommon_dispatch_t *disp,uint32_t reg,uint8_t val)
{
    *((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr)) = val;
    _wbflush();
}

static void atapidrv_outw(idecommon_dispatch_t *disp,uint32_t reg,uint16_t val)
{
    *((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr)) = val;
    _wbflush();
}

static void atapidrv_outs(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;

    while (len > 0) {
#ifdef _BYTESWAP_
	data = (uint16_t) buf[1] + ((uint16_t) buf[0] << 8);
#else
	data = (uint16_t) buf[0] + ((uint16_t) buf[1] << 8);
#endif

	*((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr)) = data;
	_wbflush();

	buf++;
	buf++;
	len--;
	len--;
	}
}



static void atapidrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    idecommon_t *softc;
    idecommon_dispatch_t *disp;
    char descr[80];
    char unitstr[50];
    int res;

    /* 
     * probe_a is the IDE base address
     * probe_b is the unit number and other flags
     * probe_ptr is unused.
     */

    softc = (idecommon_t *) KMALLOC(sizeof(idecommon_t),0);
    disp = (idecommon_dispatch_t *) KMALLOC(sizeof(idecommon_dispatch_t),0);

    if (softc && disp) {
	softc->idecommon_addr = probe_a;
	softc->idecommon_unit = probe_b;

	disp->ref = softc;
	disp->baseaddr = softc->idecommon_addr;
	softc->idecommon_dispatch = disp;

	disp->outb = atapidrv_outb;
	disp->outw = atapidrv_outw;
	disp->outs = atapidrv_outs;

	disp->inb = atapidrv_inb;
	disp->inw = atapidrv_inw;
	disp->ins = atapidrv_ins;

	res = idecommon_devprobe(softc);
	if (res < 0) {
	    KFREE(softc);
	    KFREE(disp);
	    return;
	    }

	xsprintf(descr,"%s unit %d at %08X",drv->drv_description,probe_b,probe_a);
	xsprintf(unitstr,"%d",probe_b);
	cfe_attach(drv,softc,unitstr,descr);
	}
}
