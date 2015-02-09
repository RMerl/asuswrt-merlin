/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  PCI Configuration			File: pciconf.c
    *  
    *********************************************************************  
    *
    *  Copyright 2001,2002,2003
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
    */

/*
 * Based in part on the algor p5064 version of pciconf.c:
 *  generic PCI bus configuration
 * Copyright (c) 1999 Algorithmics Ltd
 * which in turn appears to be based on PMON code.
 */

#include "cfe_pci.h"
#include "cfe_timer.h"
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "lib_malloc.h"

#include "pcivar.h"
#include "pcireg.h"
#include "pcidevs.h"
#include "ldtreg.h"

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

#define PRINTF printf
#define VPRINTF vprintf

extern void cfe_ledstr(const char *);
#define	SBD_DISPLAY(msg)	cfe_ledstr(msg)

int _pciverbose;

/* pci_devinfo uses sprintf(), and we don't necessarily want to drag
   in all those tables for a minimal build, so set this function
   pointer if it is required. */
void	(*_pci_devinfo_func) (pcireg_t, pcireg_t, int, char *);

int	  _pci_enumerated = 0;

/* The "devices" here are actually PCI "functions" */
#ifndef PCIMAX_DEV
#define PCIMAX_DEV	16	/* arbitrary */
#endif
#ifndef PCIMAX_MEMWIN
#define PCIMAX_MEMWIN	3	/* arbitrary per device */
#endif
#ifndef PCIMAX_IOWIN
#define PCIMAX_IOWIN	1	/* arbitrary per device */
#endif

/* Summary data structures retained after configuration. */

struct pci_tree {
    int nargs;
    struct pci_attach_args *args;
};

static struct pci_tree pcitree[PCI_HOST_PORTS];


/* Additional configuration-time data structures. */

struct pcidev {
    struct pci_attach_args *pa;
    int                 bus;
    unsigned char	min_gnt;
    unsigned char	max_lat;
    short		nmemwin;
    short		niowin;
};

struct pciwin {
    struct pcidev *dev;
    int		reg;
    size_t      size;
    pcireg_t	address;
};

struct pcirange {
    pcireg_t    base;
    pcireg_t    next;
    pcireg_t    limit;
};

static struct pci_attach_args *pciarg;   /* the array of devices (external) */
static struct pcidev *pcidev;            /* parallel attr array (internal) */

static int pci_nbus;
static int pcindev;
static int pcimaxdev;

static struct pciwin *pcimemwin;  /* the array of memory windows */
static int pcinmemwin;
static int pcimaxmemwin;
static struct pcirange pcimemaddr;

static struct pciwin *pciiowin;   /* the array of i/o windows */
static int pciniowin;
static int pcimaxiowin;
static struct pcirange pciioaddr;
    


static int lhb_secondary_bus;
static int lhb_subordinate_bus;


static void
print_bdf (int port, int bus, int device, int function)
{
    PRINTF ("PCI");
    if (PCI_HOST_PORTS > 1 && port >= 0)
	PRINTF ("[%d]", port);
    if (bus >= 0)
	PRINTF (" bus %d", bus);
    if (device >= 0)
	PRINTF (" slot %d", device);
    if (function >= 0)
	PRINTF ("/%d", function);
    PRINTF (": ");
}

static void
pci_bdfprintf (int port, int bus, int device, int function,
	       const char *fmt, ...)
{
    va_list arg;

    print_bdf (port, bus, device, function);
#ifdef __VARARGS_H
    va_start(arg);
#else
    va_start(arg, fmt);
#endif
    VPRINTF (fmt, arg);
    va_end(arg);
}

void
pci_tagprintf (pcitag_t tag, const char *fmt, ...)
{
    va_list arg;
    int port, bus, device, function;

    pci_break_tag (tag, &port, &bus, &device, &function); 
    print_bdf (port, bus, device, function);

#ifdef __VARARGS_H
    va_start(arg);
#else
    va_start(arg, fmt);
#endif
    VPRINTF (fmt, arg);
    va_end(arg);
}


/* Initialize the pci-pci bridges and bus hierarchy. */

/* let rec */
static void pci_businit (int port, int bus, pci_flags_t flags);

static void
pci_businit_dev_func (pcitag_t tag, pci_flags_t flags)
{
    pcireg_t id, class, bhlc;

    class = pci_conf_read(tag, PCI_CLASS_REG);
    id = pci_conf_read(tag, PCI_ID_REG);
    bhlc = pci_conf_read(tag, PCI_BHLC_REG);

    pcindev++;

    if (PCI_CLASS(class) == PCI_CLASS_BRIDGE && PCI_HDRTYPE_TYPE(bhlc) == 1) {
	enum {NONE, PCI, LDT} sec_type;
	int  offset;
	int port, bus, device, function;
	int  bus2;
	struct pci_bus *ppri, *psec;
	pcireg_t data;

	sec_type = NONE;
	offset = 0;
	switch (PCI_SUBCLASS(class)) {
	    case PCI_SUBCLASS_BRIDGE_PCI:
		/* See if there is an LDT capability for the secondary. */
		offset = pci_find_ldt_cap(tag, LDT_SECONDARY);
		sec_type = offset == 0 ? PCI : LDT;
		break;
  	    case PCI_SUBCLASS_BRIDGE_HOST:
	    case PCI_SUBCLASS_BRIDGE_MISC:
	        /* A Type 1 host bridge (e.g., SB-1250 LDT) or an
		   X-to-LDT bridge with unassigned subclass (LDT?).
		   Probe iff the secondary is LDT (best policy?) */
		offset = pci_find_ldt_cap(tag, LDT_SECONDARY);
		if (offset != 0) sec_type = LDT;
		break;
	}

	if (sec_type == NONE)
	    return;

	if (sec_type == LDT && offset != 0) {
	    pcireg_t cr = pci_conf_read(tag, offset+LDT_COMMAND_CAP_OFF);
	    if ((cr & LDT_COMMAND_DOUBLE_ENDED) != 0)
	        return;
	}

	bus2 = pci_nextbus(port);
	if (bus2 < 0)
	    return;
	pci_nbus = bus2 + 1;

	pci_break_tag(tag, &port, &bus, &device, &function);
	ppri = pci_businfo(port, bus);

	psec = pci_businfo(port, bus2);
	psec->tag = tag;
	psec->primary = bus;
	psec->port = port;
	
	/*
	 * set primary to bus, secondary to bus2, and
	 * subordinate to max possible bus number
	 */
	data = (PCI_BUSMAX << 16) | (bus2 << 8) | bus;
	pci_conf_write(tag, PPB_BUSINFO_REG, data);

	/*
	 * set base interrupt mapping.
	 */
	if (bus == 0) {
	    /* We assume board-specific wiring for bus 0 devices. */
	    psec->inta_shift = pci_int_shift_0(tag);
	} else {
	    /* We assume expansion boards wired per PCI Bridge spec */
	    psec->inta_shift = (ppri->inta_shift + device) % 4;
	}

	/* if the new bus is LDT, do the fabric initialization */
	if (sec_type == LDT)
	    psec->no_probe = ldt_chain_init(tag, port, bus2, flags);
	else
	    psec->no_probe = 0;

#ifdef _CSWARM_
	/* We must avoid attempting to scan the secondary bus of the
           diagnostic sturgeon on a cswarm (MasterAbortMode == 0
           appears not to suppress propagation of aborts).  We know
           its secondary bus number will be 2 on cswarm. */
	if (bus2 == 2) {
	    psec->no_probe = 1;
	}
#endif

	if (psec->no_probe) {
	    psec->min_io_addr = 0xffffffff;
	    psec->max_io_addr = 0;
	    psec->min_mem_addr = 0xffffffff;
	    psec->max_mem_addr = 0;
	} else
	    pci_businit(port, bus2, flags);
	    
	/* reset subordinate bus number */
	data = (data & 0xff00ffff) | ((pci_nbus - 1) << 16);
	pci_conf_write(tag, PPB_BUSINFO_REG, data);

	if (PCI_VENDOR(id) == PCI_VENDOR_SIBYTE &&
	    PCI_PRODUCT(id) == PCI_PRODUCT_SIBYTE_SB1250_LDT &&
	    PCI_REVISION(class) == 1) {
	    lhb_secondary_bus = bus2;
	    lhb_subordinate_bus = pci_nbus - 1;
	}
    }
}

static void
pci_businit_dev (int port, int bus, int device, pci_flags_t flags)
{
    pcitag_t tag;
    pcireg_t bhlc;
    int function, maxfunc;

    tag = pci_make_tag(port, bus, device, 0);
    if (!pci_canscan (tag))
        return;

    if (!pci_probe_tag(tag))
	return;

    bhlc = pci_conf_read(tag, PCI_BHLC_REG);
    maxfunc = PCI_HDRTYPE_MULTIFN(bhlc) ? PCI_FUNCMAX : 0;

    for (function = 0; function <= maxfunc; function++) {
	tag = pci_make_tag(port, bus, device, function);
	if (pci_probe_tag(tag))
	    pci_businit_dev_func(tag, flags);
    }
}


static void
pci_businit (int port, int bus, pci_flags_t flags)
{
    int device;
    struct pci_bus *ppri = pci_businfo(port, bus);

    ppri->min_io_addr = 0xffffffff;
    ppri->max_io_addr = 0;
    ppri->min_mem_addr = 0xffffffff;
    ppri->max_mem_addr = 0;

    /* Pass 1 errata: we must number the buses in ascending order to
       avoid problems with the LDT host bridge capturing all
       configuration cycles. */

    for (device = 0; device <= PCI_DEVMAX; device++)
	pci_businit_dev (port, bus, device, flags);
}


/* Scan each PCI device on the system and record its configuration
   requirements. */

static void
pci_query_dev_func (pcitag_t tag)
{
    pcireg_t id, class;
    pcireg_t old, mask;
    pcireg_t stat;
    pcireg_t bparam;
    pcireg_t icr;
    pcireg_t bhlc;
    pcireg_t t;      /* used for pushing writes to cfg registers */
    unsigned int x;
    int reg, mapreg_end, mapreg_rom;
    struct pci_bus *pb;
    struct pci_attach_args *pa;
    struct pcidev *pd;
    struct pciwin *pm, *pi;
    int port, bus, device, function, incr;
    uint16_t cmd;
    uint8_t  pin, pci_int;

    class = pci_conf_read(tag, PCI_CLASS_REG);
    id = pci_conf_read(tag, PCI_ID_REG);
    pci_break_tag(tag, &port, &bus, &device, &function);

    if (_pciverbose && _pci_devinfo_func) {
	char devinfo[256];
	(*_pci_devinfo_func)(id, class, 1, devinfo);
	pci_tagprintf(tag, "%s\n", devinfo);
    }

    if (pcindev >= pcimaxdev) {
	panic ("pci: unexpected device number\n");
	return;
    }

    pa = &pciarg[pcindev];
    pa->pa_tag = tag;
    pa->pa_id = id;
    pa->pa_class = class;

    pd = &pcidev[pcindev++];
    pd->pa = pa;
    pd->bus = bus;
    pd->nmemwin = 0;
    pd->niowin = 0;
    
    pb = pci_businfo(port, bus);
    pb->ndev++;

    stat = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);

    /* do all devices support fast back-to-back */
    if ((stat & PCI_STATUS_BACKTOBACK_SUPPORT) == 0)
	pb->fast_b2b = 0;		/* no, sorry */

    /* do all devices run at 66 MHz */
    if ((stat & PCI_STATUS_66MHZ_SUPPORT) == 0)
	pb->freq66 = 0; 		/* no, sorry */

    /* find slowest devsel */
    x = PCI_STATUS_DEVSEL(stat);
    if (x > pb->devsel)
	pb->devsel = x;

    bparam = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG);

    pd->min_gnt = PCI_BPARAM_GRANT (bparam);
    pd->max_lat = PCI_BPARAM_LATENCY (bparam);

    if (pd->min_gnt != 0 || pd->max_lat != 0) {
	/* find largest minimum grant time of all devices */
	if (pd->min_gnt != 0 && pd->min_gnt > pb->min_gnt)
	    pb->min_gnt = pd->min_gnt;
	
	/* find smallest maximum latency time of all devices */
	if (pd->max_lat != 0 && pd->max_lat < pb->max_lat)
	    pb->max_lat = pd->max_lat;
	
	if (pd->max_lat != 0)
	    /* subtract our minimum on-bus time per sec from bus bandwidth */
	    pb->bandwidth -= pd->min_gnt * 4000000 /
		(pd->min_gnt + pd->max_lat);
    }

    /* Hook any special setup code and test for skipping resource
       allocation, e.g., for our own host bridges.  */
    if (pci_device_preset(tag) != 0)
	return;

    /* Does the function need an interrupt mapping? */
    icr = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG);
    pin = PCI_INTERRUPT_PIN(icr);
    icr &=~ (PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
    if (pin == PCI_INTERRUPT_PIN_NONE)
        pci_int = 0;
    else if (bus == 0)
	pci_int = pci_int_map_0(tag);
    else
	pci_int = (pb->inta_shift + device + (pin - 1)) % 4 + 1;
    icr |= pci_int_line(pci_int) << PCI_INTERRUPT_LINE_SHIFT;
    pci_conf_write(tag, PCI_BPARAM_INTERRUPT_REG, icr);
        
    /* Find and size the BARs */
    bhlc = pci_conf_read(tag, PCI_BHLC_REG);
    switch (PCI_HDRTYPE_TYPE(bhlc)) {
    case 0:   /* Type 0 */
	mapreg_end = PCI_MAPREG_END;
	mapreg_rom = PCI_MAPREG_ROM;
	break;
    case 1:   /* Type 1 (bridge) */
	mapreg_end = PCI_MAPREG_PPB_END;
	mapreg_rom = PCI_MAPREG_PPB_ROM;
	break;
    case 2:   /* Type 2 (cardbus) */
	mapreg_end = PCI_MAPREG_PCB_END;
	mapreg_rom = PCI_MAPREG_NONE;
	break;
    default:  /* unknown */
	mapreg_end = PCI_MAPREG_START;  /* assume none */
	mapreg_rom = PCI_MAPREG_NONE; 
	break;
    }

    cmd = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
    cmd &= (PCI_COMMAND_MASK << PCI_COMMAND_SHIFT);   /* don't clear status */
    pci_conf_write(tag, PCI_COMMAND_STATUS_REG,
	cmd & ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE));
    t = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);   /* push the write */

    for (reg = PCI_MAPREG_START; reg < mapreg_end; reg += incr) {
	old = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, 0xffffffff);
	mask = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, old);

	/* Assume 4 byte reg, unless we find out otherwise below.  */
	incr = 4;

	/* 0 if not implemented, all-1s if (for some reason) 2nd half
	   of 64-bit BAR or if device broken and reg not implemented
	   (should return 0).  */
	if (mask == 0 || mask == 0xffffffff)
	    continue;

	if (_pciverbose >= 3)
	    pci_tagprintf (tag, "reg 0x%x = 0x%x\n", reg, mask);

	if (PCI_MAPREG_TYPE(mask) == PCI_MAPREG_TYPE_IO) {

	    mask |= 0xffff0000; /* must be ones */
		
	    if (pciniowin >= pcimaxiowin) {
		PRINTF ("pci: too many i/o windows\n");
		continue;
	    }
	    pi = &pciiowin[pciniowin++];
	    
	    pi->dev = pd;
	    pi->reg = reg;
	    pi->size = -(PCI_MAPREG_IO_ADDR(mask));
	    pd->niowin++;
	} else {
	    switch (PCI_MAPREG_MEM_TYPE(mask)) {
	    case PCI_MAPREG_MEM_TYPE_32BIT:
	    case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	    case PCI_MAPREG_MEM_TYPE_64BIT:
		incr = 8;
		{
		    pcireg_t oldhi, maskhi;

		    if (reg + 4 >= PCI_MAPREG_END) {
			pci_tagprintf (tag,
				       "misplaced 64-bit region ignored\n");
			continue;
		    }

		    oldhi = pci_conf_read(tag, reg + 4);
		    pci_conf_write(tag, reg + 4, 0xffffffff);
		    maskhi = pci_conf_read(tag, reg + 4);
		    pci_conf_write(tag, reg + 4, oldhi);

		    if (maskhi != 0xffffffff && maskhi != 0x00000000) {
			/* First, fix malformed 0*1* */
			if ((maskhi & (maskhi+1)) == 0x00000000) {
			    pci_tagprintf (tag,
					   "Warning: "
					   "ill-formed 64-bit BAR (%08x)\n",
					   maskhi);
			    maskhi = 0xffffffff;
			}
		    }
		    /* Check for 1*0*. */
		    if ((-maskhi & ~maskhi) != 0x00000000) {
			pci_tagprintf (tag,
				       "true 64-bit region (%08x) ignored\n",
				       maskhi);
			continue;
		    }
		}
		break;
	    default:
		pci_tagprintf (tag, "reserved mapping type 0x%x\n",
			       PCI_MAPREG_MEM_TYPE(mask));
		continue;
	    }
		
	    if  (!PCI_MAPREG_MEM_PREFETCHABLE(mask))
		pb->prefetch = 0;

	    if (pcinmemwin >= pcimaxmemwin) {
		PRINTF ("pci: too many memory windows\n");
		continue;
	    }
	    pm = &pcimemwin[pcinmemwin++];
	    
	    pm->dev = pd;
	    pm->reg = reg;
	    pm->size = -(PCI_MAPREG_MEM_ADDR(mask));
	    pd->nmemwin++;
	}
    }

    /* Finally check for Expansion ROM */
    if (mapreg_rom != PCI_MAPREG_NONE) {
	reg = mapreg_rom;
	old = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, 0xfffffffe);
	mask = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, old);

	/* 0 if not implemented, 0xfffffffe or 0xffffffff if device
	   broken and/or register not implemented.  */
	if (mask != 0 && mask != 0xfffffffe && mask != 0xffffffff) {
	    if (_pciverbose >= 3)
		pci_tagprintf (tag, "reg 0x%x = 0x%x\n", reg, mask);

	    if (pcinmemwin >= pcimaxmemwin) {
		PRINTF ("pci: too many memory windows\n");
		goto done;
	    }

	    pm = &pcimemwin[pcinmemwin++];
	    pm->dev = pd;
	    pm->reg = reg;
	    pm->size = -(PCI_MAPREG_ROM_ADDR(mask));
	    pd->nmemwin++;
	}
    }

done:
    cmd |= PCI_COMMAND_INVALIDATE_ENABLE;  /* any reason not to? */
    pci_conf_write(tag, PCI_COMMAND_STATUS_REG, cmd);
}

static void
pci_query_dev (int port, int bus, int device)
{
    pcitag_t tag;
    pcireg_t bhlc;
    int probed, function, maxfunc;

    tag = pci_make_tag(port, bus, device, 0);
    if (!pci_canscan (tag))
	return;

    if (_pciverbose >= 2)
	pci_bdfprintf (port, bus, device, -1, "probe...");

    probed = pci_probe_tag(tag);

    if (_pciverbose >= 2)
	PRINTF ("completed\n");

    if (!probed)
	return;

    bhlc = pci_conf_read(tag, PCI_BHLC_REG);
    maxfunc = PCI_HDRTYPE_MULTIFN(bhlc) ? PCI_FUNCMAX : 0;

    for (function = 0; function <= maxfunc; function++) {
	tag = pci_make_tag(port, bus, device, function);
	if (pci_probe_tag(tag))
	    pci_query_dev_func(tag);
    }

    if (_pciverbose >= 2)
	pci_bdfprintf (port, bus, device, -1, "done\n");
}


static void
pci_query (int port, int bus)
{
    struct pci_bus *pb = pci_businfo(port, bus);
    int device;
    pcireg_t sec_status;
    unsigned int def_ltim, max_ltim;

    if (bus != 0) {
	sec_status = pci_conf_read(pb->tag, PPB_IO_STATUS_REG);
	pb->fast_b2b = (sec_status & PCI_STATUS_BACKTOBACK_SUPPORT) ? 1 : 0;
	pb->freq66 = (sec_status & PCI_STATUS_66MHZ_SUPPORT) ? 1 : 0;
    }

    if (pb->no_probe)
	pb->ndev = 0;
    else {
	for (device = 0; device <= PCI_DEVMAX; device++)
	    pci_query_dev (port, bus, device);
    }

    if (pb->ndev != 0) {
	/* convert largest minimum grant time to cycle count */
	max_ltim = pb->min_gnt * (pb->freq66 ? 66 : 33) / 4;

	/* now see how much bandwidth is left to distribute */
	if (pb->bandwidth <= 0) {
	    pci_bdfprintf (port, bus, -1, -1,
			   "warning: total bandwidth exceeded\n");
	    def_ltim = 1;
	} else {
	    /* calculate a fair share for each device */
	    def_ltim = pb->bandwidth / pb->ndev;
	    if (def_ltim > pb->max_lat)
		/* that would exceed critical time for some device */
		def_ltim = pb->max_lat;
	    /* convert to cycle count */
	    def_ltim = def_ltim * (pb->freq66 ? 66 : 33) / 4;
	}
	/* most devices don't implement bottom three bits, so round up */
	def_ltim = (def_ltim + 7) & ~7;
	max_ltim = (max_ltim + 7) & ~7;

	pb->def_ltim = MIN (def_ltim, 255);
	pb->max_ltim = MIN (MAX (max_ltim, def_ltim), 255);
    }
}


static int 
wincompare (const void *a, const void *b)
{
    const struct pciwin *wa = a, *wb = b;
    if (wa->dev->bus != wb->dev->bus)
	/* sort into ascending order of bus number */
	return (int)(wa->dev->bus - wb->dev->bus);
    else
	/* sort into descending order of size */
	return (int)(wb->size - wa->size);
}


static pcireg_t
pci_allocate_io(pcitag_t tag, size_t size)
{
    pcireg_t address;

    /* allocate upwards after rounding to size boundary */
    address = (pciioaddr.next + (size - 1)) & ~(size - 1);
    if (size != 0) {
	if (address < pciioaddr.next || address + size > pciioaddr.limit)
	    return -1;
	pciioaddr.next = address + size;
    }
    return address;
}

static pcireg_t
pci_align_io_addr(pcireg_t addr)
{
  /* align to appropriate bridge boundaries (4K for Rev 1.1 Bridge Arch).
     Over/underflow will show up in subsequent allocations. */
  return (addr + ((1 << 12)-1)) & ~((1 << 12)-1);
}

static void
pci_assign_iowins(int port, int bus,
		  struct pciwin *pi_first, struct pciwin *pi_limit)
{
    struct pciwin *pi;
    struct pci_bus *pb = pci_businfo(port, bus);
    pcireg_t t;        /* for pushing writes */

    pciioaddr.next = pci_align_io_addr(pciioaddr.next);

    if (bus == lhb_secondary_bus) {
        pb->min_io_addr = pciioaddr.next;
	pciioaddr.next += (1 << 12);
	pb->max_io_addr = pciioaddr.next - 1;
    }

    for (pi = pi_first; pi < pi_limit; pi++) {
	struct pcidev *pd = pi->dev;
	pcitag_t tag = pd->pa->pa_tag;
	pcireg_t base;

	if (pd->niowin < 0)
	    continue;
	pi->address = pci_allocate_io (tag, pi->size);
	if (pi->address == -1) {
	    pci_tagprintf (tag, 
			   "(%d) not enough PCI i/o space (%ld requested)\n", 
			   pi->reg, (long)pi->size);
	    pd->nmemwin = pd->niowin = -1;
	    continue;
	}

	if (pi->address < pb->min_io_addr)
	    pb->min_io_addr = pi->address;
	if (pi->address + pi->size - 1 > pb->max_io_addr)
	    pb->max_io_addr = pi->address + pi->size - 1;

	if (_pciverbose >= 2)
	    pci_tagprintf (tag,
			    "I/O BAR at 0x%x gets %ld bytes @ 0x%x\n", 
			    pi->reg, (long)pi->size, pi->address);
	base = pci_conf_read(tag, pi->reg);
	base = (base & ~PCI_MAPREG_IO_ADDR_MASK) | pi->address;
	pci_conf_write(tag, pi->reg, base);
	t = pci_conf_read(tag, pi->reg);
    }

    if (pb->min_io_addr < pb->max_io_addr) {
	/* if any io on bus, expand to valid bridge limit */
        pb->max_io_addr |= ((1 << 12)-1);
	pciioaddr.next = pb->max_io_addr + 1;
    }

    if (bus == lhb_subordinate_bus) {
        pciioaddr.next = pci_align_io_addr(pciioaddr.next) + (1 << 12);
    }
}

static void
pci_setup_iowins (int port)
{
    struct pciwin *pi, *pi_first, *pi_limit;
    int bus;

    qsort(pciiowin, pciniowin, sizeof(struct pciwin), wincompare);
    pi_first = pciiowin;
    pi_limit = &pciiowin[pciniowin];

    for (bus = 0; bus < pci_nbus; bus++) {
        pi = pi_first;
	while (pi != pi_limit && pi->dev->bus == bus)
	    pi++;
	pci_assign_iowins(port, bus, pi_first, pi);
	pi_first = pi;
    }
}


static pcireg_t
pci_allocate_mem(pcitag_t tag, size_t size)
{
    pcireg_t address;

    /* allocate upwards after rounding to size boundary */
    address = (pcimemaddr.next + (size - 1)) & ~(size - 1);
    if (size != 0) {
	if (address < pcimemaddr.next || address + size > pcimemaddr.limit)
	    return -1;
	pcimemaddr.next = address + size;
    }
    return address;
}

static pcireg_t
pci_align_mem_addr(pcireg_t addr)
{
  /* align to appropriate bridge boundaries (1M for Rev 1.1 Bridge Arch).
     Over/underflow will show up in subsequent allocations. */
  return (addr + ((1 << 20)-1)) & ~((1 << 20)-1);
}

static void
pci_assign_memwins(int port, int bus,
		   struct pciwin *pm_first, struct pciwin *pm_limit)
{
    struct pciwin *pm;
    struct pci_bus *pb = pci_businfo(port, bus);
    pcireg_t t;      /* for pushing writes */

    pcimemaddr.next = pci_align_mem_addr(pcimemaddr.next);

    if (bus == lhb_secondary_bus) {
        pb->min_mem_addr = pcimemaddr.next;
	pcimemaddr.next += (1 << 20);
	pb->max_mem_addr = pcimemaddr.next - 1;
    }

    for (pm = pm_first; pm < pm_limit; ++pm) {
	struct pcidev *pd = pm->dev;
	pcitag_t tag = pd->pa->pa_tag;

	if (pd->nmemwin < 0)
	    continue;
	pm->address = pci_allocate_mem (tag, pm->size);
	if (pm->address == -1) {
	    pci_tagprintf (tag, 
			   "(%d) not enough PCI mem space (%ld requested)\n", 
			   pm->reg, (long)pm->size);
	    pd->nmemwin = pd->niowin = -1;
	    continue;
	}
	if (_pciverbose >= 2)
	    pci_tagprintf (tag,
			   "%s BAR at 0x%x gets %ld bytes @ 0x%x\n",
			   pm->reg != PCI_MAPREG_ROM ? "MEM" : "ROM",
			   pm->reg, (long)pm->size, pm->address);

	if (pm->address < pb->min_mem_addr)
	    pb->min_mem_addr = pm->address;
	if (pm->address + pm->size - 1 > pb->max_mem_addr)
	    pb->max_mem_addr = pm->address + pm->size - 1;

	if (pm->reg != PCI_MAPREG_ROM) {
	    /* normal memory - expansion rom done below */
	    pcireg_t base = pci_conf_read(tag, pm->reg);
	    base = pm->address | (base & ~PCI_MAPREG_MEM_ADDR_MASK);
	    pci_conf_write(tag, pm->reg, base);
	    t = pci_conf_read(tag, pm->reg);
	    if (PCI_MAPREG_MEM_TYPE(t) == PCI_MAPREG_MEM_TYPE_64BIT) {
	        pci_conf_write(tag, pm->reg + 4, 0);
		t = pci_conf_read(tag, pm->reg + 4);
	    }
	}
    }

    /* align final bus window */
    if (pb->min_mem_addr < pb->max_mem_addr) {
        pb->max_mem_addr |= ((1 << 20) - 1);
	pcimemaddr.next = pb->max_mem_addr + 1;
    }

    if (bus == lhb_subordinate_bus) {
        pcimemaddr.next = pci_align_mem_addr(pcimemaddr.next) + (1 << 20);
    }
}

static void
pci_setup_memwins (int port)
{
    struct pciwin *pm, *pm_first, *pm_limit;
    int bus;

    qsort(pcimemwin, pcinmemwin, sizeof(struct pciwin), wincompare);
    pm_first = pcimemwin;
    pm_limit = &pcimemwin[pcinmemwin];

    for (bus = 0; bus < pci_nbus; bus++) {
        pm = pm_first;
	while (pm != pm_limit && pm->dev->bus == bus)
	    pm++;
	pci_assign_memwins(port, bus, pm_first, pm);
	pm_first = pm;
    }

    /* Program expansion rom address base after normal memory base, 
       to keep DEC ethernet chip happy */
    for (pm = pcimemwin; pm < pm_limit; pm++) {
	if (pm->reg == PCI_MAPREG_ROM && pm->address != -1) {
	    struct pcidev *pd = pm->dev;   /* expansion rom */
	    pcitag_t tag = pd->pa->pa_tag;
	    pcireg_t base;
	    pcireg_t t;     /* for pushing writes */

	    /* Do not enable ROM at this time -- PCI spec 2.2 s6.2.5.2 last
	       paragraph, says that if the expansion ROM is enabled, accesses
	       to other registers via the BARs may not be done by portable
	       software!!! */
	    base = pci_conf_read(tag, pm->reg);
	    base = pm->address | (base & ~PCI_MAPREG_ROM_ADDR_MASK);
	    base &= ~PCI_MAPREG_ROM_ENABLE;
	    pci_conf_write(tag, pm->reg, base);
	    t = pci_conf_read(tag, pm->reg);
	}
    }
}


static void
pci_setup_ppb(int port, pci_flags_t flags)
{
    int i;

    for (i = pci_nbus - 1; i > 0; i--) {
	struct pci_bus *psec = pci_businfo(port, i);
	struct pci_bus *ppri = pci_businfo(port, psec->primary);
	if (ppri->min_io_addr > psec->min_io_addr)
	    ppri->min_io_addr = psec->min_io_addr;
	if (ppri->max_io_addr < psec->max_io_addr)
	    ppri->max_io_addr = psec->max_io_addr;
	if (ppri->min_mem_addr > psec->min_mem_addr)
	    ppri->min_mem_addr = psec->min_mem_addr;
	if (ppri->max_mem_addr < psec->max_mem_addr)
	    ppri->max_mem_addr = psec->max_mem_addr;
    }

    if (_pciverbose >= 2) {
	struct pci_bus *pb = pci_businfo(port, 0);
	if (pb->min_io_addr < pb->max_io_addr)
	    pci_bdfprintf (port, 0, -1, -1, "io   0x%08x-0x%08x\n", 
			   pb->min_io_addr, pb->max_io_addr);
	if (pb->min_mem_addr < pb->max_mem_addr)
	    pci_bdfprintf (port, 0, -1, -1, "mem  0x%08x-0x%08x\n",
			   pb->min_mem_addr, pb->max_mem_addr);
    }

    for (i = 1; i < pci_nbus; i++) {
	struct pci_bus *pb = pci_businfo(port, i);
	pcireg_t cmd;
	pcireg_t iodata, memdata;
	pcireg_t brctl;
	pcireg_t t;    /* for pushing writes */

	cmd = pci_conf_read(pb->tag, PCI_COMMAND_STATUS_REG);
	if (_pciverbose >= 2)
	    pci_bdfprintf (port, i, -1, -1,
			   "subordinate to bus %d\n", pb->primary);

	cmd |= PCI_COMMAND_MASTER_ENABLE;
	if (pb->min_io_addr < pb->max_io_addr) {
	    uint32_t io_limit;

	    /* Pass 1 work-round: limits are next free, not last used. */
	    io_limit = pb->max_io_addr;
	    if (i == lhb_secondary_bus)
	       io_limit++;

	    cmd |= PCI_COMMAND_IO_ENABLE;
	    if (_pciverbose >= 2)
		pci_bdfprintf (port, i, -1, -1, "io   0x%08x-0x%08x\n",
			       pb->min_io_addr, io_limit);
	    iodata = pci_conf_read(pb->tag, PPB_IO_STATUS_REG);
	    if ((iodata & PPB_IO_ADDR_CAP_MASK) == PPB_IO_ADDR_CAP_32) {
		pcireg_t upperdata;

		upperdata = ((pb->min_io_addr) >> 16) & PPB_IO_UPPER_BASE_MASK;
		upperdata |= (io_limit & PPB_IO_UPPER_LIMIT_MASK);
		pci_conf_write(pb->tag, PPB_IO_UPPER_REG, upperdata);
	    }
	    iodata = (iodata & ~PPB_IO_BASE_MASK)
		| ((pb->min_io_addr >> 8) & 0xf0);
	    iodata = (iodata & ~PPB_IO_LIMIT_MASK)
	        | ((io_limit & PPB_IO_LIMIT_MASK) & 0xf000);
	} else {
	    /* Force an empty window */
	    iodata = pci_conf_read(pb->tag, PPB_IO_STATUS_REG);
	    iodata &=~ (PPB_IO_BASE_MASK | PPB_IO_LIMIT_MASK);
	    iodata |= (1 << 4) | (0 << (8+4));
	}
	pci_conf_write(pb->tag, PPB_IO_STATUS_REG, iodata);
	/* Push the write (see SB-1250 Errata, Section 8.10) */
	t = pci_conf_read(pb->tag, PPB_IO_STATUS_REG);

	if (pb->min_mem_addr < pb->max_mem_addr) {
	    uint32_t mem_limit;

	    mem_limit = pb->max_mem_addr;
	    if (i == lhb_secondary_bus)
	        mem_limit++;

	    cmd |= PCI_COMMAND_MEM_ENABLE;
	    if (_pciverbose >= 2)
		pci_bdfprintf (port, i, -1, -1, "mem  0x%08x-0x%08x\n",
			       pb->min_mem_addr, mem_limit);
	    memdata = pci_conf_read(pb->tag, PPB_MEM_REG);
	    memdata = (memdata & ~PPB_MEM_BASE_MASK)
		| ((pb->min_mem_addr >> 16) & 0xfff0);
	    memdata = (memdata & ~PPB_MEM_LIMIT_MASK)
	        | ((mem_limit & PPB_MEM_LIMIT_MASK) & 0xfff00000);
	} else {
	    /* Force an empty window */
	    memdata = pci_conf_read(pb->tag, PPB_MEM_REG);
	    memdata &=~ (PPB_MEM_BASE_MASK | PPB_MEM_LIMIT_MASK);
	    memdata |= (1 << 4) | (0 << (16+4));
	}
	pci_conf_write(pb->tag, PPB_MEM_REG, memdata);
	/* Push the write (see SB-1250 Errata, Section 8.10) */
	t = pci_conf_read(pb->tag, PPB_MEM_REG);

	/* Force an empty prefetchable memory window */
	memdata = pci_conf_read(pb->tag, PPB_PREFMEM_REG);
	memdata &=~ (PPB_MEM_BASE_MASK | PPB_MEM_LIMIT_MASK);
	memdata |= (1 << 4) | (0 << (16+4));
	pci_conf_write(pb->tag, PPB_PREFMEM_REG, memdata);
	/* Push the write (see SB-1250 Errata, Section 8.10) */
	t = pci_conf_read(pb->tag, PPB_PREFMEM_REG);

	/* Do any final bridge dependent initialization */
	pci_bridge_setup(pb->tag, flags);

	brctl = pci_conf_read(pb->tag, PPB_BRCTL_INTERRUPT_REG);
#ifdef _SB1250_PASS2_
	/* LDT MasterAborts _will_ cause bus errors in pass 2 when
           enabled.  Pending negotiations with clients, leave
           MasterAbortMode off to disable their propagation. */
#else
	brctl |= (PPB_BRCTL_SERR_ENABLE | PPB_BRCTL_MASTER_ABORT_MODE);
#endif
	if (pb->fast_b2b)
	    brctl |= PPB_BRCTL_BACKTOBACK_ENABLE;
	pci_conf_write(pb->tag, PPB_BRCTL_INTERRUPT_REG, brctl);
	t = pci_conf_read(pb->tag, PPB_BRCTL_INTERRUPT_REG);  /* push */

	pci_conf_write(pb->tag, PCI_COMMAND_STATUS_REG, cmd);
    }
}


int
pci_cacheline_log2 (void)
{
    /* default to 8 words == 2^3 */
    return 3;
}


int
pci_maxburst_log2 (void)
{
    return 32;			/* no limit */
}

static void
pci_setup_devices (int port, pci_flags_t flags)
{
    struct pcidev *pd;

    /* Enable each PCI interface */
    for (pd = pcidev; pd < &pcidev[pcindev]; pd++) {
	struct pci_bus *pb = pci_businfo(port, pd->bus);
	pcitag_t tag = pd->pa->pa_tag;
	pcireg_t cmd, misc;
	unsigned int ltim;
	
	/* Consider setting interrupt line here */

	cmd = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MASTER_ENABLE 
	       | PCI_COMMAND_SERR_ENABLE 
	       | PCI_COMMAND_PARITY_ENABLE;
	/* Always enable i/o & memory space, in case this card is
	   just snarfing space from the fixed ISA block and doesn't
	   declare separate PCI space. */
	cmd |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
	if (pb->fast_b2b)
	    cmd |= PCI_COMMAND_BACKTOBACK_ENABLE;

	/* Write status too, to clear any pending error bits. */
	pci_conf_write(tag, PCI_COMMAND_STATUS_REG, cmd);

	ltim = pd->min_gnt * (pb->freq66 ? 66 : 33) / 4;
	ltim = MIN (MAX (pb->def_ltim, ltim), pb->max_ltim);
    
	misc = pci_conf_read (tag, PCI_BHLC_REG);
	PCI_LATTIMER_SET (misc, ltim);
	PCI_CACHELINE_SET (misc, 1 << pci_cacheline_log2());
	pci_conf_write (tag, PCI_BHLC_REG, misc);

	pci_device_setup (tag);    /* hook for post setup */
    }
}


static void
pci_configure_tree (int port, pci_flags_t flags)
{
    int bus;

    pciarg = NULL;

    lhb_secondary_bus = lhb_subordinate_bus = -1;

    /* initialise the host bridge(s) */
    SBD_DISPLAY ("PCIH");
    if (pci_hwinit(port, flags) < 0)
	return;

    /* initialise any PCI-PCI bridges, discover and number buses */
    SBD_DISPLAY ("PCIB");
    pci_nbus = pci_maxbus(port) + 1;
    pcindev = 0;
    pci_businit(port, 0, flags);

    /* scan configuration space of all devices to collect attributes */
    SBD_DISPLAY ("PCIS");
    pcimaxdev = pcindev;
    pciarg = (struct pci_attach_args *) KMALLOC (pcimaxdev * sizeof(struct pci_attach_args), 0);
    if (pciarg == NULL) {
        PRINTF ("pci: no memory for device table\n");
	pcimaxdev = 0;
    } else {
        pcidev = (struct pcidev *) KMALLOC (pcimaxdev * sizeof(struct pcidev), 0);
	if (pcidev == NULL) {
	    KFREE (pciarg); pciarg = NULL;
	    PRINTF ("pci: no memory for device attribute table\n");
	    pcimaxdev = 0;
	}
    }
    pcindev = 0;

    pcimaxmemwin = PCIMAX_DEV * PCIMAX_MEMWIN;
    pcimemwin = (struct pciwin *) KMALLOC (pcimaxmemwin * sizeof(struct pciwin), 0);
    if (pcimemwin == NULL) {
        PRINTF ("pci: no memory for window table\n");
	pcimaxmemwin = 0;
    }
    pcimaxiowin = PCIMAX_DEV * PCIMAX_IOWIN;
    pciiowin = (struct pciwin *) KMALLOC (pcimaxiowin * sizeof(struct pciwin), 0);
    if (pciiowin == NULL) {
        PRINTF ("pci: no memory for window table\n");
	pcimaxiowin = 0;
    }

    pcinmemwin = pciniowin = 0;
    for (bus = 0; bus < pci_nbus; bus++) {
        pci_query (port, bus);
    }

    if (pcindev != pcimaxdev) {
	panic ("Inconsistent device count\n");
	return;
    }
    
    /* alter PCI bridge parameters based on query data */
    pci_hwreinit (port, flags);

    /* setup the individual device windows */
    pcimemaddr.base = pci_minmemaddr(port);
    pcimemaddr.limit = pci_maxmemaddr(port);
    pciioaddr.base = pci_minioaddr(port);
    pciioaddr.limit = pci_maxioaddr(port);

    pcimemaddr.next = pcimemaddr.base;
    pciioaddr.next = pciioaddr.base;
    pci_setup_iowins (port);
    pci_setup_memwins (port);

    /* set up and enable each device */
    if (pci_nbus > 1)
	pci_setup_ppb (port, flags);
    pci_setup_devices (port, flags);

    KFREE (pciiowin); pciiowin = NULL;
    KFREE (pcimemwin); pcimemwin = NULL;
    KFREE (pcidev); pcidev = NULL;

    pcitree[port].nargs = pcindev;
    pcitree[port].args = pciarg;
}

void
pci_configure (pci_flags_t flags)
{
    int port;

    if (!_pci_enumerated) {
	_pciverbose = (PCI_DEBUG > 1) ? 3 : (flags & PCI_FLG_VERBOSE);
	_pci_devinfo_func = (_pciverbose != 0) ? pci_devinfo : NULL;

	for (port = 0; port < PCI_HOST_PORTS; port++)
	    pci_configure_tree(port, flags);

	_pci_enumerated = 1;
    }
}


int
pci_foreachdev(int (*fn)(pcitag_t tag))
{
    int port, i;

    for (port = 0; port < PCI_HOST_PORTS; port++)
	for (i = 0; i < pcitree[port].nargs; i++) {
	    int rv = (*fn)(pcitree[port].args[i].pa_tag);
	    if (rv != 0)
		return rv;
	}

    return 0;
}


static int
dump_configuration(pcitag_t tag)
{
    pci_tagprintf(tag, "dump of ");
    pci_conf_print(tag);
    return 0;
}
  
void
pci_show_configuration(void)
{
    pci_foreachdev(dump_configuration);
}

int
pci_find_class(uint32_t class, int enumidx, pcitag_t *tag)
{
    int port, i;
    struct pci_attach_args *thisdev;

    for (port = 0; port < PCI_HOST_PORTS; port++) {
	thisdev = pcitree[port].args;
	for (i = 0; i < pcitree[port].nargs && enumidx >= 0; i++) {
	    if (PCI_CLASS(thisdev->pa_class) == class) {
		if (enumidx == 0) {
		    *tag = thisdev->pa_tag;
		    return 0;
		} else {
		    enumidx--;
		}
	    }
	    thisdev++;
	}
    }

    return -1;
}

int
pci_find_device(uint32_t vid, uint32_t did, int enumidx, pcitag_t *tag)
{
    int port, i;
    struct pci_attach_args *thisdev;

    for (port = 0; port < PCI_HOST_PORTS; port++) {
	thisdev = pcitree[port].args;
	for (i = 0; i < pcitree[port].nargs && enumidx >= 0; i++) {
	    if ((PCI_VENDOR(thisdev->pa_id) == vid) &&
		(PCI_PRODUCT(thisdev->pa_id) == did)) {
		if (enumidx == 0) {
		    *tag = thisdev->pa_tag;
		    return 0;
		} else {
		    enumidx--;
		}
	    }
	    thisdev++;
	}
    }

    return -1;
}
