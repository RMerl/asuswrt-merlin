/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PCI IDE disk driver			File: dev_ide_pci.c
    *  
    *  This is a simple driver for IDE hard disks that are connected
    *  to PCI IDE controllers, such as a Promise UltraXX.
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

#include "pcivar.h"
#include "pcireg.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define _BYTESWAP_ 	/* don't byteswap these disks */

#define OUTB(x,y) outb(x,y)
#define OUTW(x,y) outw(x,y)
#define INB(x) inb(x)
#define INW(x) inw(x)

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

extern void _wbflush(void);

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

const cfe_driver_t pciidedrv = {
    "PCI IDE disk",
    "ide",
    CFE_DEV_DISK,
    &idedrv_dispatch,
    idedrv_probe
};

const cfe_driver_t pciatapidrv = {
    "PCI ATAPI device",
    "atapi",
    CFE_DEV_DISK,
    &idedrv_dispatch,
    idedrv_probe
};

/*  *********************************************************************
    *  Supported PCI devices
    ********************************************************************* */

#define DEVID(vid,pid) (((pid)<<16)|(vid))

static uint32_t pciidedrv_devlist[] = {
    DEVID(0x105a,0x4d33),		/* Promise Ultra33 */
    DEVID(0x1095,0x0649),		/* CMD PCI0649 */
    DEVID(0x1095,0x0648),               /* CMD PCI0648 */  
    0xFFFFFFFF
};


/*  *********************************************************************
    *  Port I/O routines
    *  
    *  These routines are called back from the common code to do
    *  I/O cycles to the IDE disk.  We provide routines for
    *  reading and writing bytes, words, and strings of words.
    ********************************************************************* */

static uint8_t idedrv_inb(idecommon_dispatch_t *disp,uint32_t reg)
{
    return INB(reg+disp->baseaddr);
}

static uint16_t idedrv_inw(idecommon_dispatch_t *disp,uint32_t reg)
{
    return INW(reg+disp->baseaddr);
}

static void idedrv_ins(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;

    while (len > 0) {
	data = INW(reg+disp->baseaddr);

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

static void idedrv_outb(idecommon_dispatch_t *disp,uint32_t reg,uint8_t val)
{
    OUTB(reg+disp->baseaddr,val);
}

static void idedrv_outw(idecommon_dispatch_t *disp,uint32_t reg,uint16_t val)
{
    OUTW(reg+disp->baseaddr,val);
}

static void idedrv_outs(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
{
    uint16_t data;

    while (len > 0) {
#ifdef _BYTESWAP_
	data = (uint16_t) buf[1] + ((uint16_t) buf[0] << 8);
#else
	data = (uint16_t) buf[0] + ((uint16_t) buf[1] << 8);
#endif

	OUTW(reg+disp->baseaddr,data);

	buf++;
	buf++;
	len--;
	len--;
	}
}


/*  *********************************************************************
    *  pciidedrv_find(devid,list)
    *  
    *  Find a particular product ID on the list.  Return >= 0 if
    *  the ID is valid.
    *  
    *  Input parameters: 
    *  	   devid - product and device ID we have
    *  	   list - list of product and device IDs we're looking for
    *  	   
    *  Return value:
    *  	   index into table, or -1 if not found
    ********************************************************************* */
static int pciidedrv_find(uint32_t devid,uint32_t *list)
{
    int idx = 0;

    while (list[idx] != 0xFFFFFFFF) {
	if (list[idx] == devid) return idx;
	idx++;
	}

    return -1;
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
    idecommon_t *softc;
    idecommon_dispatch_t *disp;
    char descr[80];
    char unitstr[50];
    pcitag_t tag;
    int index;
    uint32_t devid;
    uint32_t reg;
    int res;
    int unit;
    cfe_driver_t *realdrv;
    int attached = 0;

    /* 
     * probe_a is unused
     * probe_b is unused
     * probe_ptr is unused.
     */

    index = 0;

    for (;;) {
	if (pci_find_class(PCI_CLASS_MASS_STORAGE,index,&tag) != 0) break;
	index++;
	devid = pci_conf_read(tag,PCI_ID_REG);

	if (pciidedrv_find(devid,pciidedrv_devlist) < 0) {
	    continue;
	    }


	reg = pci_conf_read(tag,PCI_MAPREG(0));

	if (PCI_MAPREG_TYPE(reg) != PCI_MAPREG_TYPE_IO) {
	    xprintf("Skipping this IDE device, we don't do memory mapped IDE yet\n");
	    continue;
	    }

	reg &= ~PCI_MAPREG_TYPE_MASK;

	for (unit = 0; unit < 2; unit++) {

	    /* 
	     * If we've deliberately disabled probing of this
	     * device, then skip it.
	     */

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


	    softc->idecommon_addr = reg;
	    disp->ref = softc;
	    disp->baseaddr = softc->idecommon_addr;
	    softc->idecommon_deferprobe = 0;
	    softc->idecommon_dispatch = disp;
	    softc->idecommon_unit = unit;

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

	    xsprintf(descr,"%s unit %d at I/O %04X",drv->drv_description,
		     softc->idecommon_unit,softc->idecommon_addr);
	    xsprintf(unitstr,"%d",unit);

	    realdrv = (cfe_driver_t *) (softc->idecommon_atapi ? &pciatapidrv : &pciidedrv);

	    idecommon_attach(&idedrv_dispatch);
	    cfe_attach(realdrv,softc,unitstr,descr);
	    attached++;
	    }

	}

    xprintf("PCIIDE: %d controllers found\n",attached);
}
