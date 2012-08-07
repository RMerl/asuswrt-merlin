/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IDE disk driver				File: dev_ide.c
    *  
    *  This is a simple driver for IDE hard disks.  The disks
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

#include "dev_ide.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static void idedrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);

/*  *********************************************************************
    *  Device Dispatch
    ********************************************************************* */

static cfe_devdisp_t idedrv_dispatch = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

const cfe_driver_t idedrv = {
    "IDE disk",
    "ide",
    CFE_DEV_DISK,
    &idedrv_dispatch,
    idedrv_probe
};

const cfe_driver_t atapidrv = {
    "ATAPI device",
    "atapi",
    CFE_DEV_DISK,
    &idedrv_dispatch,
    idedrv_probe
};


#define IDE_REG_ADDR_SHIFT	5


/*  *********************************************************************
    *  Port I/O routines
    *  
    *  These routines are called back from the common code to do
    *  I/O cycles to the IDE disk.  We provide routines for
    *  reading and writing bytes, words, and strings of words.
    ********************************************************************* */

static uint8_t idedrv_inb(idecommon_dispatch_t *disp,uint32_t reg)
{
    reg <<= IDE_REG_ADDR_SHIFT;

    return  (*((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr)));
}

static uint16_t idedrv_inw(idecommon_dispatch_t *disp,uint32_t reg)
{
    reg <<= IDE_REG_ADDR_SHIFT;

    return *((volatile uint16_t *) PHYS_TO_K1((reg+disp->baseaddr)));
}

static void idedrv_ins(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;
    uint16_t *buf16;

    reg <<= IDE_REG_ADDR_SHIFT;

    /* Do 16-bit reads/writes so that the byteswaps will work out right */

    buf16 = (uint16_t *) buf;

    while (len > 0) {
	data = *((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr));
	*buf16++ = data;
	len--;
	len--;
	}

}

static void idedrv_outb(idecommon_dispatch_t *disp,uint32_t reg,uint8_t val)
{
    reg <<= IDE_REG_ADDR_SHIFT;

    *((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr)) = (uint8_t) (val);
}

static void idedrv_outw(idecommon_dispatch_t *disp,uint32_t reg,uint16_t val)
{
    reg <<= IDE_REG_ADDR_SHIFT;

    *((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr)) = val;
}

static void idedrv_outs(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;
    uint16_t *buf16;

    reg <<= IDE_REG_ADDR_SHIFT;
    
    buf16 = (uint16_t *) buf;

    while (len > 0) {
	data = *buf16++;
	*((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr)) = data;
	len--;
	len--;
	}
}



/*  *********************************************************************
    *  idedrv_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Our probe routine.  Attach an IDE device to the firmware.
    *  
    *  Input parameters: 
    *  	   drv - driver structure
    *  	   probe_a - physical address of IDE registers
    *  	   probe_b - unit number
    *  	   probe_ptr - not used
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void idedrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    idecommon_t *softc = NULL;
    idecommon_dispatch_t *disp = NULL;
    char descr[80];
    char unitstr[50];
    int res;
    cfe_driver_t *realdrv;
    int unit;

    /* 
     * probe_a is the IDE base address
     * probe_b is a bitmask of unit numbers to check
     * probe_ptr is unused.
     */

    for (unit = 0; unit < 2; unit++) {
	
	if (IDE_PROBE_GET_TYPE(probe_b,unit) == IDE_DEVTYPE_NOPROBE) {
	    continue;
	    }

	softc = (idecommon_t *) KMALLOC(sizeof(idecommon_t),0);
	disp = (idecommon_dispatch_t *) KMALLOC(sizeof(idecommon_dispatch_t),0);

	if (!softc || !disp) {
	    if (softc) KFREE(softc);
	    if (disp) KFREE(disp);
	    return;		/* out of memory, stop here */
	    }

	softc->idecommon_addr = probe_a;
	softc->idecommon_unit = unit;
	softc->idecommon_deferprobe = 0;

	disp->ref = softc;
	disp->baseaddr = softc->idecommon_addr;
	softc->idecommon_dispatch = disp;

	disp->outb = idedrv_outb;
	disp->outw = idedrv_outw;
	disp->outs = idedrv_outs;

	disp->inb = idedrv_inb;
	disp->inw = idedrv_inw;
	disp->ins = idedrv_ins;

	/*
	 * If we're autoprobing, do it now.  Loop back if we have
	 * trouble finding the device.  
	 * 
	 * If not autoprobing, assume the device is there and set the
	 * common routines to double check later.
	 */

	if (IDE_PROBE_GET_TYPE(probe_b,unit) == IDE_DEVTYPE_AUTO) {
	    res = idecommon_devprobe(softc,1);
	    if (res < 0) {
		KFREE(softc);
		KFREE(disp);
		continue;
		}
	    }
	else {
	    idecommon_init(softc,IDE_PROBE_GET_TYPE(probe_b,unit));
	    softc->idecommon_deferprobe = 1;
	    }


	xsprintf(descr,"%s unit %d at %08X",drv->drv_description,unit,probe_a);
	xsprintf(unitstr,"%d",unit);

	realdrv = (cfe_driver_t *) (softc->idecommon_atapi ? &atapidrv : &idedrv);
	idecommon_attach(&idedrv_dispatch);

	cfe_attach(realdrv,softc,unitstr,descr);
	}
}
