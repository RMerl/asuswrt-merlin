/*
 * p6032/pci_machdep.c: Machine-specific functions for PCI autoconfiguration.
 *
 * Copyright (c) 2000-2001, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

#include "cfe_pci.h"
#include "sbmips.h"
#include "pcivar.h"
#include "pcireg.h"
#include "sbd.h"

#define PCI_MISC_LTIM_SHIFT                     8
#define PCI_MISC_LTIM_MASK                      0xff
#define PCI_MISC_LTIM(mr) \
            (((mr) >> PCI_MISC_LTIM_SHIFT) & PCI_MISC_LTIM_MASK)
#define PCI_MISC_LTIM_SET(mr,v) \
            (mr) = ((mr) & ~(PCI_MISC_LTIM_MASK << PCI_MISC_LTIM_SHIFT)) | \
                ((v) << PCI_MISC_LTIM_SHIFT)


/* default PCI mem regions in PCI space */
#define PCI_MEM_SPACE_PCI_BASE		0x00000000
#define PCI_LOCAL_MEM_PCI_BASE		0x80000000
#define PCI_LOCAL_MEM_ISA_BASE		0x00800000

/* soft versions of above */
static pcireg_t const pci_mem_space = PCI_MEM_SPACE;	/* CPU base for access to mapped PCI memory */
static pcireg_t pci_mem_space_pci_base;			/* PCI mapped memory base */
static pcireg_t pci_local_mem_pci_base;			/* PCI address of local memory */
static pcireg_t pci_local_mem_isa_base;			/* PCI address of ISA accessible local memory */

static pcireg_t const pci_io_space = PCI_IO_SPACE;	/* CPU base for access to mapped PCI IO */
static pcireg_t const pci_io_space_pci_base = 0;	/* PCI mapped IO base */

/* PCI mem space allocation */
pcireg_t minpcimemaddr;
pcireg_t nextpcimemaddr;
pcireg_t minpcimemaddr;
pcireg_t maxpcimemaddr;


/* PCI i/o space allocation */
pcireg_t minpciioaddr;
pcireg_t maxpciioaddr;
pcireg_t nextpciioaddr;

typedef long hsaddr_t;

#define hs_write8(a,b) *((volatile uint8_t *) (a)) = (b)
#define hs_write16(a,b) *((volatile uint16_t *) (a)) = (b)
#define hs_write32(a,b) *((volatile uint32_t *) (a)) = (b)
#define hs_write64(a,b) *((volatile uint32_t *) (a)) = (b)
#define hs_read8(a) *((volatile uint8_t *) (a))
#define hs_read16(a) *((volatile uint16_t *) (a))
#define hs_read32(a) *((volatile uint32_t *) (a))
#define hs_read64(a) *((volatile uint64_t *) (a))


static const struct pci_bus bonito_pci_bus = {
	0,		/* minimum grant */
	255,		/* maximum latency */
	0,		/* devsel time = fast */
	0,		/* we don't support fast back-to-back */
	0,		/* we don't support prefetch */
	0,		/* we don't support 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	1		/* initially one device on bus (i.e. us) */
};

#ifdef _CFE_
const cons_t pci_optnames[] = {
    {"verbose",PCI_FLG_VERBOSE},
    {NULL,0}};
#endif

extern int _pciverbose;

#define MAXBUS	3
const int _pci_maxbus = MAXBUS;		/* maximum # buses we support */
static int _pci_nbus = 0;
static struct pci_bus _pci_bus[MAXBUS];

/* Access functions */

/* The following must either fail or return the next sequential bus
   number to make secondary/subordinate numbering work correctly. */
int
pci_nextbus (int port)
{
    int bus = _pci_nbus;

    if (bus >= MAXBUS)
	return -1;
    _pci_nbus++;
    return bus;
}
  
int
pci_maxbus (int port)
{
    return _pci_nbus - 1;
}

struct pci_bus *
pci_businfo (int port, int bus)
{
    return (bus < _pci_nbus ? &_pci_bus[bus] : NULL);
}

/*
 * PCI address resources.
 * NB: initial limits for address allocation are assumed to be aligned
 * appropriately for PCI bridges (4K boundaries for I/O, 1M for memory).
 */

pcireg_t
pci_minmemaddr (int port)
{
    return minpcimemaddr;
}

pcireg_t
pci_maxmemaddr (int port)
{
    return maxpcimemaddr;
}

pcireg_t
pci_minioaddr (int port)
{
    return minpciioaddr;
}

pcireg_t
pci_maxioaddr (int port)
{
    return maxpciioaddr;
}

static char * const _bonito = (char * const) PHYS_TO_K1(BONITO_BASE);

static int pcimaster;
static int pcireserved;

/*
 * Called to initialise the bridge at the beginning of time
 */
int
pci_hwinit (int port, pci_flags_t flags)
{
    int initialise = 1;

    pcimaster = 1;
    pcireserved = 0;


    _pci_bus[0] = bonito_pci_bus;
    _pci_nbus = 1;

    if (initialise && pcimaster) {
	/*
	 * We are initialising and we are the bus master
	 * so we get to define the mappings
	 */
	pci_mem_space_pci_base = PCI_MEM_SPACE_PCI_BASE;

	pci_local_mem_pci_base = PCI_LOCAL_MEM_PCI_BASE;
	pci_local_mem_isa_base = PCI_LOCAL_MEM_ISA_BASE;

	/* point to start and end of region (leaving bottom 16M for ISA) */
	minpcimemaddr  = pci_mem_space_pci_base;
	if (minpcimemaddr < 0x1000000)
	    minpcimemaddr = 0x1000000;
	nextpcimemaddr = pci_mem_space_pci_base + PCI_MEM_SPACE_SIZE;
	maxpcimemaddr = nextpcimemaddr;

	printf("minpcimem=%08X next=%08X\n",minpcimemaddr,nextpcimemaddr);

	/* leave 64KB at beginning of PCI i/o space for ISA bridge */
	minpciioaddr  = pci_io_space_pci_base;
	if (minpciioaddr < 0x10000)
	    minpciioaddr = 0x10000;
	nextpciioaddr = pci_io_space_pci_base + PCI_IO_SPACE_SIZE;
	maxpciioaddr = nextpciioaddr;
    }
    else {
	/* If we are not the master then wait for somebody to configure us */
	if (!pcimaster) {
	    /* wait for up to 3s until we get configured */
	    int i;
	    for (i = 0; i < 300; i++) {
		msdelay (10);
		if (BONITO_PCICMD & PCI_COMMAND_MEM_ENABLE)
		    break;
	    }
	    /* wait a little bit longer for the master scan to complete */
	    msdelay (100);
	}

	/* This assumes the address space is contiguous starting at PCIMAP_LO0 */
	pci_mem_space_pci_base = (BONITO_PCIMAP & BONITO_PCIMAP_PCIMAP_LO0) << (26 - BONITO_PCIMAP_PCIMAP_LO0_SHIFT);

	pci_local_mem_pci_base = BONITO_PCIBASE0 & PCI_MAPREG_MEM_ADDR_MASK;

	/* assume BAR1 is suitable for 24 bit accesses */
	pci_local_mem_isa_base = BONITO_PCIBASE1 & PCI_MAPREG_MEM_ADDR_MASK;
    }


    if (initialise) {
	/* set up Local->PCI mappings */
	/* LOCAL:PCI_MEM_SPACE+00000000 -> PCI:pci_mem_space_pci_base#0x0c000000 */
	/* LOCAL:80000000	        -> PCI:80000000#0x80000000 */
	BONITO_PCIMAP =
	    BONITO_PCIMAP_WIN(0, pci_mem_space_pci_base+0x00000000) |	
	    BONITO_PCIMAP_WIN(1, pci_mem_space_pci_base+0x04000000) |
	    BONITO_PCIMAP_WIN(2, pci_mem_space_pci_base+0x08000000) |
	    BONITO_PCIMAP_PCIMAP_2;

	/* LOCAL:PCI_IO_SPACE -> PCI:00000000-00100000 */
	/* hardwired */
    }

    if (pcimaster) {
	/* set up PCI->Local mappings */

	/* pcimembasecfg has been set up by low-level code */

	/* Initialise BARs for access to our memory */
	/* PCI:pci_local_mem_pci_base -> LOCAL:0 */
	BONITO_PCIBASE0 = pci_local_mem_pci_base;

	/* PCI:pci_local_mem_isa_base -> LOCAL:0 */
	BONITO_PCIBASE1 = pci_local_mem_isa_base;

	/* PCI:PCI_LOCAL_MEM_PCI_BASE+10000000 -> LOCAL bonito registers (512b) */
	BONITO_PCIBASE2 = pci_local_mem_pci_base + 0x10000000;

	if (!initialise)
	    return 0;

#ifdef BONITO_PCICFG_PCIRESET
	/* reset the PCI bus */
	BONITO_PCICFG &= ~BONITO_PCICFG_PCIRESET;
	usdelay (1);

	/* unreset the PCI bus */
	BONITO_PCICFG |= BONITO_PCICFG_PCIRESET;
#endif
    }

    return initialise && pcimaster;
}


/*
 * Called to reinitialise the bridge after we've scanned each PCI device
 * and know what is possible.
 */
void
pci_hwreinit (int port, pci_flags_t flags)
{
    if (pcimaster) {
	/* set variable latency timer */
	PCI_MISC_LTIM_SET(BONITO_PCILTIMER, _pci_bus[0].def_ltim);

	if (_pci_bus[0].prefetch) {
	    /* we can safely prefetch from all pci mem devices */
	}
    }
}


void
pci_flush (void)
{
    /* flush read-ahead fifos (!) */
}




pcitag_t
pci_make_tag(int port, int bus, int device, int function)
{
    pcitag_t tag;
    tag = (bus << 16) | (device << 11) | (function << 8);
    return tag;
}

void __inline__
pci_break_tag(pcitag_t tag,
	      int *portp, int *busp, int *devicep, int *functionp)
{
    if (portp) *portp = 0;
    if (busp) *busp = (tag >> 16) & 255;
    if (devicep) *devicep = (tag >> 11) & 31;
    if (functionp) *functionp = (tag >> 8) & 7;
}

int
pci_canscan (pcitag_t tag)
{
    int bus, dev;
    pci_break_tag (tag, NULL, &bus, &dev, NULL);
    return !(bus == 0 && (pcireserved & (1 << dev)));
}

/*
 * flush Bonito register writes
 */
static void __inline__
bflush (void)
{
    (void)BONITO_PCIMAP_CFG;	/* register address is arbitrary */
}

pcireg_t
pci_conf_read(pcitag_t tag, int reg)
{
    uint32_t addr, type;
    pcireg_t data, stat;
    int bus, device, function;

    if ((reg & 3) || reg < 0 || reg >= 0x100) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_read: bad reg 0x%x\n", reg);
	return ~0;
    }

    pci_break_tag (tag, NULL, &bus, &device, &function); 
    if (bus == 0) {
	/* Type 0 configuration on onboard PCI bus */
	if (device > 20 || function > 7)
	    return ~0;		/* device out of range */
	addr = (1 << (device+11)) | (function << 8) | reg;
	type = 0x00000;
    }
    else {
	/* Type 1 configuration on offboard PCI bus */
	if (bus > 255 || device > 31 || function > 7)
	    return ~0;	/* device out of range */
	addr = (bus << 16) | (device << 11) | (function << 8) | reg;
	type = 0x10000;
    }

    /* clear aborts */
    BONITO_PCICMD |= PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

    BONITO_PCIMAP_CFG = (addr >> 16) | type;
    bflush ();

    data = *(volatile pcireg_t *)PHYS_TO_K1(BONITO_PCICFG_BASE | (addr & 0xfffc));

    stat = BONITO_PCICMD;

    if (stat & PCI_STATUS_MASTER_ABORT) {
	return ~0;
    }

    if (stat & PCI_STATUS_MASTER_TARGET_ABORT) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_read: target abort\n");
	return ~0;
    }

    return data;
}


void
pci_conf_write(pcitag_t tag, int reg, pcireg_t data)
{
    uint32_t addr, type;
    pcireg_t stat;
    int bus, device, function;

    if ((reg & 3) || reg < 0 || reg >= 0x100) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_write: bad reg %x\n", reg);
	return;
    }

    pci_break_tag (tag, NULL, &bus, &device, &function);

    if (bus == 0) {
	/* Type 0 configuration on onboard PCI bus */
	if (device > 20 || function > 7)
	    return;		/* device out of range */
	addr = (1 << (device+11)) | (function << 8) | reg;
	type = 0x00000;
    }
    else {
	/* Type 1 configuration on offboard PCI bus */
	if (bus > 255 || device > 31 || function > 7)
	    return;	/* device out of range */
	addr = (bus << 16) | (device << 11) | (function << 8) | reg;
	type = 0x10000;
    }

    /* clear aborts */
    BONITO_PCICMD |= PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

    BONITO_PCIMAP_CFG = (addr >> 16) | type;
    bflush ();

    *(volatile pcireg_t *)PHYS_TO_K1(BONITO_PCICFG_BASE | (addr & 0xfffc)) = data;
    stat = BONITO_PCICMD;

    if (stat & PCI_STATUS_MASTER_ABORT) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_write: master abort\n");
    }

    if (stat & PCI_STATUS_MASTER_TARGET_ABORT) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_write: target abort\n");
    }
}

int
pci_conf_write_acked(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_write(tag,reg,data);
    return 1;
}

pcireg_t
pci_conf_read16(pcitag_t tag, int reg)
{
    return pci_conf_read (tag, reg);
}

void
pci_conf_write16(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_write (tag, reg, data);
}




int
pci_map_io(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
    pcireg_t address;
    phys_addr_t pa;
    
    if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	if (_pciverbose >= 1)
	    pci_tagprintf(tag, "_pci_map_io: bad request\n");
	return -1;
    }
    
    address = pci_conf_read(tag, reg);
    
    if ((address & PCI_MAPREG_TYPE_IO) == 0) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "_pci_map_io: attempt to i/o map a memory region\n");
	return -1;
    }

    pa = (address & PCI_MAPREG_IO_ADDR_MASK) - pci_io_space_pci_base;
    pa += pci_io_space;
    *pap = pa;
    
    if (_pciverbose >= 3)
	pci_tagprintf(tag, "_pci_map_io: mapping i/o at physical %016llx\n", 
		       (uint64_t)*pap);

    return 0;
}

int
pci_map_mem(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
    pcireg_t address;
    phys_addr_t pa;

    if (reg == PCI_MAPREG_ROM) {
	/* expansion ROM */
	address = pci_conf_read(tag, reg);
	if (!(address & PCI_MAPREG_ROM)) {
	    pci_tagprintf (tag, "_pci_map_mem: attempt to map missing rom\n");
	    return -1;
	}
	pa = address & PCI_MAPREG_ROM_ADDR_MASK;
    }
    else {
	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	    if (_pciverbose >= 1)
		pci_tagprintf(tag, "_pci_map_mem: bad request\n");
	    return -1;
	}
	
	address = pci_conf_read(tag, reg);
	
	if ((address & PCI_MAPREG_TYPE_IO) != 0) {
	    if (_pciverbose >= 1)
		pci_tagprintf(tag, "pci_map_mem: attempt to memory map an I/O region\n");
	    return -1;
	}
	
	switch (address & PCI_MAPREG_MEM_TYPE_MASK) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
	    break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
	    if (_pciverbose >= 1)
		pci_tagprintf (tag, "_pci_map_mem: attempt to map 64-bit region\n");
	    return -1;
	default:
	    if (_pciverbose >= 1)
		pci_tagprintf (tag, "_pci_map_mem: reserved mapping type\n");
	    return -1;
	}
	pa = address & PCI_MAPREG_MEM_ADDR_MASK;
    }

    
    pa -= pci_mem_space_pci_base;
    pa += pci_mem_space;
    *pap = pa;

    if (_pciverbose >= 3)
	pci_tagprintf (tag, "_pci_map_mem: mapping memory at physical 0x%016llX\n", 
			(uint64_t)*pap);
    return 0;
}

/*
 * allocate PCI address spaces
 * only applicable if we are the PCI master
 */




/* 
 * Handle mapping of fixed ISA addresses 
 */


#define ISAPORT_ADDR(port) (PHYS_TO_K1(pci_io_space_pci_base + (port)))


uint8_t
inb (unsigned int port)
{
    return hs_read8(ISAPORT_ADDR(port));
}

uint16_t
inw (unsigned int port)
{
    return hs_read16(ISAPORT_ADDR(port));
}

uint32_t
inl (unsigned int port)
{
    return hs_read32(ISAPORT_ADDR(port));
}

void
outb (unsigned int port, uint8_t val)
{
    hs_write8(ISAPORT_ADDR(port),val);
    mips_wbflush ();
}

void
outw (unsigned int port, uint16_t val)
{
    hs_write16(ISAPORT_ADDR(port),val);
    mips_wbflush ();
}

void
outl (unsigned int port, uint32_t val)
{
    hs_write32(ISAPORT_ADDR(port),val);
    mips_wbflush ();
}


int
pci_probe_tag(pcitag_t tag)
{
    pcireg_t data;

    if (!pci_canscan(tag))
	return 0;

    data = pci_conf_read(tag,PCI_ID_REG);

    mips_wbflush ();

    if ((data == 0) || (data == 0xffffffff)) return 0;

    return 1;
}

int
pci_map_window(phys_addr_t va,
	       unsigned int offset, unsigned int len,
	       int l2ca, int endian)
{
    return -1;
}

int
pci_unmap_window(unsigned int offset, unsigned int len)
{
    return -1;
}

int
pci_device_preset (pcitag_t tag)
{
    /* Nothing to do for now. */
    return 0;
}
void
pci_device_setup (pcitag_t tag)
{
    /* Nothing to do for now. */
}


/* Called for each bridge after configuring the secondary bus, to allow
   device-specific initialization. */
void
pci_bridge_setup (pcitag_t tag, pci_flags_t flags)
{
    /* nothing to do */
}

/* The base shift of a slot or device on the motherboard.  Only device
   ids 3, 4 and 5 are implemented as PCI connectors. */
uint8_t
pci_int_shift_0(pcitag_t tag)
{
    int bus, device;

    pci_break_tag (tag, NULL, &bus, &device, NULL);

    if (bus != 0)
	return 0;

    switch (device) {
    case 0: case 1: case 2:            /* dedicated on-board devices */
	return 0;
    case 3: case 4: case 5:            /* PCI slots */
        return ((device - 3) % 4);
    default:
        return 0;
    }
}

#define PCI_INTERRUPT_ENET  5
#define PCI_INTERRUPT_SCSI  6
#define PCI_INTERRUPT_USB   8

/* Return the mapping of a P5064 device/function interrupt to an
   interrupt line.  Values 1-4 indicate the PCIIRQ0-3 inputs to the
   interrupt mapper, respectively, dedicated values as above, or 0 if
   there is no mapping.  This is board specific. */
uint8_t
pci_int_map_0(pcitag_t tag)
{
    pcireg_t data;
    int pin, bus, device;

    data = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG);
    pin = PCI_INTERRUPT_PIN(data);
    if (pin == 0) {
	/* No IRQ used. */
	return 0;
    }
    if (pin > 4) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_map_int: bad interrupt pin %d\n", pin);
	return 0;
    }

    pci_break_tag (tag, NULL, &bus, &device, NULL);

    if (bus != 0)
	return 0;

    switch (device) {
    case 0:
        return PCI_INTERRUPT_ENET;
    case 1:
        return PCI_INTERRUPT_SCSI;
    case 2:
        return PCI_INTERRUPT_USB;
    case 3: case 4: case 5:
        return (((pin - 1) + pci_int_shift_0(tag)) % 4) + 1;
    default:
        return 0;
    }
}

/* Map PCI interrupts A, B, C, D into a value for the IntLine
   register.  For SB1250, return the source number used by the
   interrupt mapper, or 0xff if none. */
uint8_t
pci_int_line(uint8_t pci_int)
{
  switch (pci_int) {
  case PCI_INTERRUPT_ENET:
      return 0x02;
  case PCI_INTERRUPT_SCSI:
      return 0x04;
  case PCI_INTERRUPT_USB:
      return 0x08;
  case 1: case 2: case 3: case 4:   /* PCI_INT_A .. PCI_INT_D */
      return 0x10 << (pci_int - 1);
  default:
      return 0;
  }
}
