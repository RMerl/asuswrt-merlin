/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PCMCIA IDE disk driver			File: dev_pcmcia_pcic.c
    *  
    *  This is a stub driver for PCMCIA disks that are connected to
    *  an Intel-style ISA PCIC chip.  Don't have one of those on
    *  your BCM12500 eval board?  I hope not; this one is just used
    *  on the Algorithmics P5064 so I can learn about the quirks of
    *  real PCMCIA disks.   See dev_pcmcia.c for the BCM12500 driver.
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
#include "cfe_error.h"
#include "cfe_console.h"

#include "dev_ide_common.h"

#include "dev_ide.h"

#include "pcivar.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */


#define PCMCIA_CARDPRESENT	1
#define _BYTESWAP_

#define OUTB(x,y) outb(x,y)
#define INB(x) inb(x)

#define PCICSET(reg,val) OUTB(0x3E0,(reg)); OUTB(0x3E1,(val))
#define PCICGET(reg,val) OUTB(0x3E0,(reg)); val = INB(0x3E1)
#define WINBASE(x) (0x10+(x)*8)

/*  *********************************************************************
    *  External references
    ********************************************************************* */

extern void _wbflush(void);

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static void pcmcia_ata_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int pcmcia_ata_open(cfe_devctx_t *ctx);

/*  *********************************************************************
    *  Device Dispatch
    ********************************************************************* */

const static cfe_devdisp_t pcmcia_ata_dispatch = {
    pcmcia_ata_open,
    idecommon_read,
    idecommon_inpstat,
    idecommon_write,
    idecommon_ioctl,
    idecommon_close,	
    NULL,
    NULL
};

const cfe_driver_t pcmcia_pcic_drv = {
    "PCMCIA ATA disk",
    "pcmcia",
    CFE_DEV_DISK,
    &pcmcia_ata_dispatch,
    pcmcia_ata_probe
};


/*  *********************************************************************
    *  Port I/O routines
    *  
    *  These routines are called back from the common code to do
    *  I/O cycles to the IDE disk.  We provide routines for
    *  reading and writing bytes, words, and strings of words.
    ********************************************************************* */


static uint8_t pcmcia_inb(idecommon_dispatch_t *disp,uint32_t reg)
{
    return *((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr));
}

static uint16_t pcmcia_inw(idecommon_dispatch_t *disp,uint32_t reg)
{
    return *((volatile uint16_t *) PHYS_TO_K1((reg+disp->baseaddr)));
}

static void pcmcia_ins(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
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

static void pcmcia_outb(idecommon_dispatch_t *disp,uint32_t reg,uint8_t val)
{
    *((volatile uint8_t *) PHYS_TO_K1(reg+disp->baseaddr)) = val;
    _wbflush();
}

static void pcmcia_outw(idecommon_dispatch_t *disp,uint32_t reg,uint16_t val)
{
    *((volatile uint16_t *) PHYS_TO_K1(reg+disp->baseaddr)) = val;
    _wbflush();
}

static void pcmcia_outs(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len)
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


/*  *********************************************************************
    *  pcic_cardpresent()
    *  
    *  Returns TRUE if a PCMCIA card (any card) is present in the
    *  slot.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   true if card is present.
    ********************************************************************* */

static int pcic_cardpresent(void)
{
    uint8_t det;

    PCICGET(0x01,det);

    det &= 0x0C;

    return (det == 0x0C);
}

/*  *********************************************************************
    *  pcic_poweron()
    *  
    *  Turn on the power to the PCMCIA slot.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void pcic_poweron(void)
{
    PCICSET(0x2F,0x70);
    PCICSET(0x02,0x30);
    cfe_sleep(CFE_HZ/4);
    PCICSET(0x02,0xB0);
}


/*  *********************************************************************
    *  pcic_mapwin(win,sysstart,sysend,pcmciaoffset,attr)
    *  
    *  Map a window using the PCIC.
    *  
    *  Input parameters: 
    *  	   win - window number (0-4)
    *  	   sysstart,sysend - ISA address of the beginning and end of
    *  	         the window
    *  	   pcmciaoffset - offset into PCCARD space that the window
    *  	         should be mapped to
    *  	   attr - nonzero to map attribute memory
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void pcic_mapwin(int win,uint32_t sysstart,uint32_t sysend,uint32_t pcmciaoffset,int attr)
{
    uint8_t ena;
    uint32_t offset;

    /*
     * Disable the window we're mucking with
     */

    PCICGET(0x06,ena);
    ena &= ~(1<<win);
    PCICSET(0x06,ena);

    /*
     * Set the start and stop system address registers
     */

    PCICSET(WINBASE(win)+0,(sysstart >> 12) & 0xFF);
    PCICSET(WINBASE(win)+1,((sysstart >> 20) & 0x0F) | 0x80);	/* note: 16 bits */
    PCICSET(WINBASE(win)+2,(sysend >> 12) & 0xFF);
    PCICSET(WINBASE(win)+3,((sysend >> 20) & 0x0F) | 0x00);

    /*
     * Set the PCMCIA offset and attribute register
     */

    offset = (uint32_t) ((int32_t) pcmciaoffset - (int32_t) sysstart);
    PCICSET(WINBASE(win)+4,(offset >> 12) & 0xFF);
    PCICSET(WINBASE(win)+5,(((offset >> 20) & 0x3F) | (attr ? 0x40 : 0x00)));

    /*
     * Reenable the window
     */

    ena |= (1<<win);
    PCICSET(0x06,ena);
}


/*  *********************************************************************
    *  pcmcia_ata_poll(x)
    *  
    *  Called periodically so we can check for card activity
    *  (report insertions, removals, etc.)
    *  
    *  Input parameters: 
    *  	   x - void param, it's our idecommon structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void pcmcia_ata_poll(void *x)
{
    idecommon_t *softc = (idecommon_t *) x;
    int present;
    int oldpresent;

    present = pcic_cardpresent();
    oldpresent = (softc->idecommon_flags & PCMCIA_CARDPRESENT) ? 1 : 0;

    if (present == oldpresent) return;

    if (present) {
	console_log("PCMCIA: Card inserted");
	}
    else {
	console_log("PCMCIA: Card removed");
	}

    if (present) {
	softc->idecommon_flags |= PCMCIA_CARDPRESENT;
	}
    else {
	softc->idecommon_flags &= ~PCMCIA_CARDPRESENT;
	}
}

/*  *********************************************************************
    *  pcmcia_ata_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Our probe routine.  Attach a PCMCIA ATA device to the firmware.
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

static void pcmcia_ata_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    idecommon_t *softc;
    idecommon_dispatch_t *disp;
    char descr[80];
    char unitstr[50];

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
	/* always defer probing till later */
	softc->idecommon_deferprobe = 1;

	disp->outb = pcmcia_outb;
	disp->outw = pcmcia_outw;
	disp->outs = pcmcia_outs;

	disp->inb = pcmcia_inb;
	disp->inw = pcmcia_inw;
	disp->ins = pcmcia_ins;

	xsprintf(descr,"%s unit %d at %08X",drv->drv_description,probe_b,probe_a);
	xsprintf(unitstr,"%d",probe_b);
	cfe_attach(drv,softc,unitstr,descr);
	softc->idecommon_flags = pcic_cardpresent() ? PCMCIA_CARDPRESENT : 0;
	cfe_bg_add(pcmcia_ata_poll,softc);
	}
}


/*  *********************************************************************
    *  pcmcia_ata_open(ctx)
    *  
    *  Open routine for the PCMCIA IDE device.  It just hooks in
    *  front of the standard IDE disk open processing so we can turn
    *  on the PCIC slot and test for the card's presence.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int pcmcia_ata_open(cfe_devctx_t *ctx)
{
    int res;
    idecommon_t *softc = ctx->dev_softc;

    if (!pcic_cardpresent()) return CFE_ERR_NOTREADY;

    pcic_poweron();

    pcic_mapwin(0,0xD0000,0xD0FFF,0,0);	/* common memory window */
    pcic_mapwin(1,0xD1000,0xD1FFF,0,1);	/* attribute memory window */

    res = idecommon_devprobe(softc,0);
    if (res < 0) {
	return res;
	}

    return idecommon_open(ctx);
}
