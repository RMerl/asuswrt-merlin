/* Very loosely based on: */
/*	$NetBSD: pci_machdep.c,v 1.17 1995/07/27 21:39:59 cgd Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Algorithmics P5064 machine-specific functions for PCI autoconfiguration.
 */

#ifdef _CFE_
#include "cfe_pci.h"
#else
#include <ametypes.h>
#include "mips.h"
#endif

#include "sbd.h"        /* from Algorithmics */
#include "v96xpbc.h"    /* from Algorithmics */

#include "pcivar.h"
#include "pcireg.h"


typedef long hsaddr_t;

#define hs_write8(a,b) *((volatile uint8_t *) (a)) = (b)
#define hs_write16(a,b) *((volatile uint16_t *) (a)) = (b)
#define hs_write32(a,b) *((volatile uint32_t *) (a)) = (b)
#define hs_write64(a,b) *((volatile uint32_t *) (a)) = (b)
#define hs_read8(a) *((volatile uint8_t *) (a))
#define hs_read16(a) *((volatile uint16_t *) (a))
#define hs_read32(a) *((volatile uint32_t *) (a))
#define hs_read64(a) *((volatile uint64_t *) (a))


#if defined(__MIPSEL)
#define V96X_SWAP_MEM	V96X_SWAP_NONE
#define V96X_SWAP_IO	V96X_SWAP_NONE
#elif defined(__MIPSEB)
#define V96X_SWAP_MEM	V96X_SWAP_8BIT
#define V96X_SWAP_IO	V96X_SWAP_AUTO
#else
#error "Must specifiy either MIPSEL or MIPSEB"
#endif

/* PCI i/o regions in PCI space */
#define PCI_IO_SPACE_PCI_BASE		0x00000000

/* PCI mem regions in PCI space */
#define PCI_MEM_SPACE_PCI_BASE	        0x00000000
#define PCI_LOCAL_MEM_PCI_BASE	        0x80000000
#define PCI_LOCAL_MEM_ISA_BASE	        0x00800000

/* soft versions of above */
static pcireg_t pci_mem_space_pci_base = PCI_MEM_SPACE_PCI_BASE;
static pcireg_t pci_local_mem_pci_base = PCI_LOCAL_MEM_PCI_BASE;
static pcireg_t pci_local_mem_isa_base = PCI_LOCAL_MEM_ISA_BASE;

/* PCI mem space allocation (note - skip the 16MB ISA mem space) */
const pcireg_t minpcimemaddr = PCI_MEM_SPACE_PCI_BASE + 0x1000000;
const pcireg_t maxpcimemaddr = PCI_MEM_SPACE_PCI_BASE + PCI_MEM_SPACE_SIZE;

/* PCI i/o space allocation (note - bottom 512KB reserved for ISA i/o space) */
    /* leave 512K at beginning of PCI i/o space for ISA bridge (it
       actually uses only 64K, but this is needed for a ISA DMA 
       h/w fix which needs a higher address bit to spot ISA cycles). */
//const pcireg_t minpciioaddr =  PCI_IO_SPACE_PCI_BASE + 0x80000;
//const pcireg_t maxpciioaddr = PCI_IO_SPACE_PCI_BASE + PCI_IO_SPACE_SIZE;
const pcireg_t minpciioaddr = PCI_IO_SPACE_PCI_BASE + 0x1000;
const pcireg_t maxpciioaddr = PCI_IO_SPACE_PCI_BASE + 0xF000;

static const struct pci_bus v96x_pci_bus = {
	0,		/* minimum grant */
	255,		/* maximum latency */
	0,		/* devsel time = fast */
	1,		/* we support fast back-to-back */
	1,		/* we support prefetch */
	0,		/* we don't support 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	1		/* initially one device on bus (i.e. us) */
};

static const struct pci_bus secondary_pci_bus = {
	0,		/* minimum grant */
	255,		/* maximum latency */
	0,		/* devsel time = fast */
	0,		/* we don't fast back-to-back */
	0,		/* we don't prefetch */
	0,		/* we don't support 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	0		/* initially no devices on bus */
};

#ifdef _CFE_
const cons_t pci_optnames[] = {
    {"verbose",PCI_FLG_VERBOSE},
    {NULL,0}};
#endif

extern int _pciverbose;


#define MAXBUS	3
const int _pci_maxbus = MAXBUS;
struct pci_bus _pci_bus[MAXBUS];

static unsigned char	v96x_vrev;

#define sbddelay(n) ((void)(0))

/*
 * Called to initialise the host bridge at the beginning of time
 */
int
pci_hwinit (pci_flags_t flags)
{
    int initialise = 1;
    unsigned char * const _v96xp = (unsigned char *) MIPS_PHYS_TO_K1(V96XPBC_BASE);
    unsigned int pci_rd0, pci_rd1;
    int i;

    /* initialise global data */
    v96x_vrev = V96X_PCI_CC_REV & V96X_PCI_CC_REV_VREV;
    if (v96x_vrev < V96X_VREV_B2) {
	printf ("V96 revisions < B.2 not supported\n");
	return -1;
    }

    _pci_bus[0] = v96x_pci_bus;
    _pci_nbus = 1;
    for (i = _pci_nbus; i < MAXBUS; i++)
	_pci_bus[i] = secondary_pci_bus;

    pci_local_mem_pci_base = PCI_LOCAL_MEM_PCI_BASE;
    pci_local_mem_isa_base = PCI_LOCAL_MEM_ISA_BASE;
    pci_mem_space_pci_base = PCI_MEM_SPACE_PCI_BASE;

    if (!initialise)
	return 0;

    /* stop the V3 chip from servicing any further PCI requests */
    V96X_PCI_CMD = 0;
    mips_wbflush ();

    /* reset the PCI bus */
    V96X_SYSTEM &= ~V96X_SYSTEM_RST_OUT;
    
    /* enable bridge to PCI and PCI memory accesses, plus error handling */
    V96X_PCI_CMD = V96X_PCI_CMD_MASTER_EN
	| V96X_PCI_CMD_MEM_EN
	| V96X_PCI_CMD_SERR_EN
	| V96X_PCI_CMD_PAR_EN;

    /* clear errors and say we do fast back-to-back transfers */
    V96X_PCI_STAT = V96X_PCI_STAT_PAR_ERR
	| V96X_PCI_STAT_SYS_ERR	
	| V96X_PCI_STAT_M_ABORT	
	| V96X_PCI_STAT_T_ABORT	
	| V96X_PCI_STAT_PAR_REP	
	| V96X_PCI_STAT_FAST_BACK;

    /* Local to PCI aptr 0 - LOCAL:PCI_CONF_SPACE -> PCI:config (1MB) */
    V96X_LB_BASE0 = PCI_CONF_SPACE | V96X_SWAP_IO | V96X_ADR_SIZE_1MB 
	| V96X_LB_BASEx_ENABLE;

    V96X_LB_BASE1 = PCI_MEM_SPACE | V96X_SWAP_MEM | V96X_ADR_SIZE_128MB 
	| V96X_LB_BASEx_ENABLE;
    V96X_LB_MAP1 = (pci_mem_space_pci_base >> 16) | V96X_LB_TYPE_MEM;

    V96X_LB_BASE2 = (PCI_IO_SPACE >> 16) | (V96X_SWAP_IO >> 2)
	| V96X_LB_BASEx_ENABLE;
    V96X_LB_MAP2 = (PCI_IO_SPACE_PCI_BASE >> 16);

    /* PCI to local aptr 1 - PCI:80000000-90000000 -> LOCAL:00000000 */
    /* 256MB window for PCI bus masters to get at our local memory */
    V96X_PCI_BASE1 = pci_local_mem_pci_base | V96X_PCI_BASEx_MEM
	| V96X_PCI_BASEx_PREFETCH;
    V96X_PCI_MAP1 =  0x00000000 | V96X_ADR_SIZE_256MB
	| V96X_SWAP_MEM /*| V96X_PCI_MAPx_RD_POST_INH*/
	| V96X_PCI_MAPx_REG_EN | V96X_PCI_MAPx_ENABLE;
    pci_rd1 = (v96x_vrev >= V96X_VREV_C0)
	? V96X_FIFO_PRI_FLUSHBURST : V96X_FIFO_PRI_FLUSHALL;

    /* PCI to local aptr 0 - PCI:00800000-01000000 -> LOCAL:00000000 */
    /* 8MB window for ISA bus masters to get at our local memory */
    V96X_PCI_BASE0 = pci_local_mem_isa_base | V96X_PCI_BASEx_MEM
      | V96X_PCI_BASEx_PREFETCH;
    V96X_PCI_MAP0 =  0x00000000 | V96X_ADR_SIZE_8MB
      | V96X_SWAP_MEM /*| V96X_PCI_MAPx_RD_POST_INH*/
      | V96X_PCI_MAPx_REG_EN | V96X_PCI_MAPx_ENABLE;
    pci_rd0 = (v96x_vrev >= V96X_VREV_C0)
      ? V96X_FIFO_PRI_FLUSHBURST : V96X_FIFO_PRI_FLUSHALL;

    /* PCI to internal registers - disabled (but avoid address overlap) */
    V96X_PCI_IO_BASE = 0xffffff00 | V96X_PCI_IO_BASE_IO;

    /* Disable PCI_IO_BASE and set optional AD(1:0) to 01b for type1 config */
    V96X_PCI_CFG = V96X_PCI_CFG_IO_DIS | (0x1 << V96X_PCI_CFG_AD_LOW_SHIFT);

    V96X_FIFO_CFG = 
	(V96X_FIFO_CFG_BRST_256 << V96X_FIFO_CFG_PBRST_MAX_SHIFT)
	| (V96X_FIFO_CFG_WR_ENDBRST << V96X_FIFO_CFG_WR_LB_SHIFT)
	| (V96X_FIFO_CFG_RD_NOTFULL << V96X_FIFO_CFG_RD_LB1_SHIFT)
	| (V96X_FIFO_CFG_RD_NOTFULL << V96X_FIFO_CFG_RD_LB0_SHIFT)
	| (V96X_FIFO_CFG_BRST_16 << V96X_FIFO_CFG_LBRST_MAX_SHIFT)
	| (V96X_FIFO_CFG_WR_ENDBRST << V96X_FIFO_CFG_WR_PCI_SHIFT)
	| (V96X_FIFO_CFG_RD_NOTFULL << V96X_FIFO_CFG_RD_PCI1_SHIFT)
	| (V96X_FIFO_CFG_RD_NOTFULL << V96X_FIFO_CFG_RD_PCI0_SHIFT);
    
    /* Set fifo priorities: note that on Rev C.0 and above we set the
       read prefetch fifos to flush at the end of a burst, and not to
       retain data like a cache (which causes coherency problems).  For
       Rev B.2 and below we can't do this, so we set them to be
       flushed by any write cycle (inefficient but safer), and we also
       require explicit software flushing of the fifos to maintain
       full coherency (i.e. call pci_flush() from the cache flush
       routines or after modifying uncached descriptors). */

    /* initial setting, may be updated in pci_hwreinit(), below  */
    V96X_FIFO_PRIORITY = 
	V96X_FIFO_PRIORITY_LOCAL_WR /* local->pci write priority (safe) */
	| V96X_FIFO_PRIORITY_PCI_WR /* pci->local write priority (safe) */ 
	| (V96X_FIFO_PRI_NOFLUSH << V96X_FIFO_PRIORITY_LB_RD0_SHIFT)
	| (V96X_FIFO_PRI_NOFLUSH << V96X_FIFO_PRIORITY_LB_RD1_SHIFT)
	| (pci_rd0 << V96X_FIFO_PRIORITY_PCI_RD0_SHIFT)
	| (pci_rd1 << V96X_FIFO_PRIORITY_PCI_RD1_SHIFT);
    

    /* clear latched PCI interrupts */
    V96X_LB_ISTAT = 0;		

    /* enable V3 general interrupts */
    V96X_LB_IMASK = V96X_LB_INTR_PCI_RD|V96X_LB_INTR_PCI_WR;

    /* finally unreset the PCI bus */
    V96X_SYSTEM |= V96X_SYSTEM_RST_OUT;

    /* ... and the onboard PCI devices */
    {
	volatile p5064bcr1 * const bcr1 = (p5064bcr1 *) MIPS_PHYS_TO_K1(BCR1_BASE);
	bcr1->eth = BCR1_ENABLE;
	bcr1->scsi = BCR1_ENABLE;
	bcr1->isa = BCR1_ENABLE;
	bcr1->pcmcia = BCR1_ENABLE;
	sbddelay (1);
    }

    return 1;
}


/*
 * Called to reinitialise the bridge after we've scanned each PCI device
 * and know what is possible.
 */
void
pci_hwreinit (pci_flags_t flags)
{
    char * const _v96xp = (char *) MIPS_PHYS_TO_K1(V96XPBC_BASE);

    if (_pci_bus[0].fast_b2b)
	/* fast back-to-back is supported by all devices */
	V96X_PCI_CMD |= V96X_PCI_CMD_FBB_EN;

    /* Rev B.1+: can now use variable latency timer */
    V96X_PCI_HDR_CFG = _pci_bus[0].def_ltim << V96X_PCI_HDR_CFG_LT_SHIFT;

    if (_pci_bus[0].prefetch && v96x_vrev >= V96X_VREV_C0) {
	/* Rev C.0+: we can safely prefetch from all pci mem devices */
	V96X_LB_BASE1 |= V96X_LB_BASEx_PREFETCH;
	V96X_FIFO_PRIORITY = (V96X_FIFO_PRIORITY & ~V96X_FIFO_PRIORITY_LB_RD1)
	    | (V96X_FIFO_PRI_FLUSHBURST << V96X_FIFO_PRIORITY_LB_RD1_SHIFT);
    }

    /* clear latched PCI interrupts */
    V96X_LB_ISTAT = 0;		

    /* enable PCI read/write error interrupts */
    V96X_LB_IMASK = V96X_LB_INTR_PCI_RD | V96X_LB_INTR_PCI_WR;

}

/* Called for each bridge after configuring the secondary bus, to allow
   device-specific initialization. */
void
pci_bridge_setup (pcitag_t tag, pci_flags_t flags)
{
    /* nothing to do */
}

void
pci_flush (void)
{
    /* flush read-ahead fifos (not necessary on Rev C.0 and above) */
    if (v96x_vrev < V96X_VREV_C0) {
	char * const _v96xp = (char *) MIPS_PHYS_TO_K1(V96XPBC_BASE);
	V96X_SYSTEM |= 
	    V96X_SYSTEM_LB_RD_PCI1 | V96X_SYSTEM_LB_RD_PCI0 |
	    V96X_SYSTEM_PCI_RD_LB1 | V96X_SYSTEM_PCI_RD_LB0;
    }
}


/* Map the CPU virtual address of an area of local memory to a PCI
   address that can be used by a PCI bus master to access it. */
pci_addr_t
pci_dmamap (v_addr_t va, unsigned int len)
{
    return pci_local_mem_pci_base + MIPS_K1_TO_PHYS(va);
}

/* Map the PCI address of an area of local memory to a CPU physical
   address. */
phys_addr_t
pci_cpumap (pci_addr_t pcia, unsigned int len)
{
    return pcia - pci_local_mem_pci_base;
}


/* Map an ISA address to the corresponding CPU physical address */
phys_addr_t
cpu_isamap(pci_addr_t isaaddr,unsigned int len)
{
    return PCI_MEM_SPACE + isaaddr;
}

/* Map the CPU virtual address of an area of local memory to an ISA
   address that can be used by a ISA bus master to access it. */
pci_addr_t
isa_dmamap (v_addr_t va, unsigned int len)
{
    unsigned long pa = MIPS_K1_TO_PHYS (va);

    /* restrict ISA DMA access to bottom 8/16MB of local memory */
    if (pa + len > 0x1000000 - pci_local_mem_isa_base)
	return (phys_addr_t)0xffffffff;
    return pci_local_mem_isa_base + pa;
}

/* Map the ISA address of an area of local memory to a CPU physical
   address. */
phys_addr_t
isa_cpumap (pci_addr_t pcia, unsigned int len)
{
    return pcia - pci_local_mem_isa_base;
}


pcitag_t
pci_make_tag(int bus, int device, int function)
{
    pcitag_t tag;
    tag = (bus << 16) | (device << 11) | (function << 8);
    return tag;
}

void
pci_break_tag(pcitag_t tag, int *busp, int *devicep, int *functionp)
{
    if (busp) *busp = (tag >> 16) & PCI_BUSMAX;
    if (devicep) *devicep = (tag >> 11) & PCI_DEVMAX;
    if (functionp) *functionp = (tag >> 8) & PCI_FUNCMAX;
}

int
pci_canscan (pcitag_t tag)
{
    return 1;
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

void
pci_device_preset (pcitag_t tag)
{
    /* Nothing to do for now. */
}
void
pci_device_setup (pcitag_t tag)
{
    /* Nothing to do for now. */
}


/* Read/write access to PCI configuration registers.  For most
   applications, pci_conf_read<N> and pci_conf_write<N> are deprecated
   unless N = 32. */

static pcireg_t
pci_conf_readn(pcitag_t tag, int reg, int width)
{
    char * const _v96xp = (char *) MIPS_PHYS_TO_K1(V96XPBC_BASE);
    uint32_t addr, ad_low;
    hsaddr_t addrp;

    pcireg_t data;
    int bus, device, function;

    if (reg & (width-1) || reg < 0 || reg >= PCI_REGMAX) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_readn: bad reg 0x%x\r\n", reg);
	return ~0;
    }

    pci_break_tag (tag, &bus, &device, &function); 
    if (bus == 0) {
	/* Type 0 configuration on onboard PCI bus */
	if (device > 5 || function > 7)
	    return ~0;		/* device out of range */
	addr = (1 << (device+24)) | (function << 8) | reg;
	ad_low = 0;
    } else if (v96x_vrev >= V96X_VREV_C0) {
	/* Type 1 configuration on offboard PCI bus */
	if (bus > PCI_BUSMAX || device > PCI_DEVMAX || function > PCI_FUNCMAX)
	    return ~0;	/* device out of range */
	addr = (bus << 16) | (device << 11) | (function << 8) | reg;
	ad_low = V96X_LB_MAPx_AD_LOW_EN;
    } else {
	return ~0;		/* bus out of range */
    }

    /* high 12 bits of address go in map register; set conf space */
    V96X_LB_MAP0 = ((addr >> 16) & V96X_LB_MAPx_MAP_ADR)
	| ad_low | V96X_LB_TYPE_CONF;

    /* clear aborts */
    V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT | V96X_PCI_STAT_T_ABORT; 

    mips_wbflush ();

    /* low 20 bits of address are in the actual address */
    addrp = MIPS_PHYS_TO_K1(PCI_CONF_SPACE + (addr & 0xfffff));
    switch (width) {
    case 1:
	data = (pcireg_t)hs_read8(addrp);
	break;
    case 2:
	data = (pcireg_t)hs_read16(addrp);
	break;
    default:
    case 4:
	data = (pcireg_t)hs_read32(addrp);
	break;
    }

    if (V96X_PCI_STAT & V96X_PCI_STAT_M_ABORT) {
	V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT;
	return ~0;
    }

    if (V96X_PCI_STAT & V96X_PCI_STAT_T_ABORT) {
	V96X_PCI_STAT |= V96X_PCI_STAT_T_ABORT;
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_read: target abort\r\n");
	return ~0;
    }

    return data;
}

pcireg_t
pci_conf_read8(pcitag_t tag, int reg)
{
    return pci_conf_readn (tag, reg, 1);
}

pcireg_t
pci_conf_read16(pcitag_t tag, int reg)
{
    return pci_conf_readn (tag, reg, 2);
}

#ifndef pci_conf_read32
pcireg_t
pci_conf_read32(pcitag_t tag, int reg)
{
    return pci_conf_readn (tag, reg, 4);
}
#endif

pcireg_t
pci_conf_read(pcitag_t tag, int reg)
{
    return pci_conf_readn (tag, reg, 4);
}

static void
pci_conf_writen(pcitag_t tag, int reg, pcireg_t data, int width)
{
    char * const _v96xp = (char *) MIPS_PHYS_TO_K1(V96XPBC_BASE);
    uint32_t addr, ad_low;
    hsaddr_t addrp;
    int bus, device, function;

    if (reg & (width-1) || reg < 0 || reg > PCI_REGMAX) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_read: bad reg %x\r\n", reg);
	return;
    }

    pci_break_tag (tag, &bus, &device, &function);

    if (bus == 0) {
	/* Type 0 configuration on onboard PCI bus */
	if (device > 5 || function > 7)
	    return;		/* device out of range */
	addr = (1 << (device+24)) | (function << 8) | reg;
	ad_low = 0;
    }
    else if (v96x_vrev >= V96X_VREV_C0) {
	/* Type 1 configuration on offboard PCI bus */
	if (bus > PCI_BUSMAX || device > PCI_DEVMAX || function > PCI_FUNCMAX)
	    return;	/* device out of range */
	addr = (bus << 16) | (device << 11) | (function << 8) | reg;
	ad_low = V96X_LB_MAPx_AD_LOW_EN;
    }
    else
	return;			/* bus out of range */

    /* high 12 bits of address go in map register; set conf space */
    V96X_LB_MAP0 = ((addr >> 16) & V96X_LB_MAPx_MAP_ADR)
	| ad_low | V96X_LB_TYPE_CONF;

    /* clear aborts */
    V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT | V96X_PCI_STAT_T_ABORT; 

    mips_wbflush ();

    /* low 20 bits of address are in the actual address */
    addrp = MIPS_PHYS_TO_K1(PCI_CONF_SPACE + (addr & 0xfffff));
    switch (width) {
    case 1:
	hs_write8(addrp,data);
	break;
    case 2:
	hs_write16(addrp,data);
	break;
    default:
    case 4:
	hs_write32(addrp,data);
	break;
    }

    mips_wbflush ();

    /* wait for write FIFO to empty */
    while (V96X_FIFO_STAT & V96X_FIFO_STAT_L2P_WR)
	continue;

    if (V96X_PCI_STAT & V96X_PCI_STAT_M_ABORT) {
	V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT;
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_write: master abort\r\n");
    }

    if (V96X_PCI_STAT & V96X_PCI_STAT_T_ABORT) {
	V96X_PCI_STAT |= V96X_PCI_STAT_T_ABORT;
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_conf_write: target abort\r\n");
    }
}

void
pci_conf_write8(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen (tag, reg, data, 1);
}

void
pci_conf_write16(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen (tag, reg, data, 2);
}

#ifndef pci_conf_write32
void
pci_conf_write32(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen (tag, reg, data, 4);
}
#endif

void
pci_conf_write(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen (tag, reg, data, 4);
}





int
pci_map_io(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
    pcireg_t address;
    phys_addr_t pa;
    
    if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	if (_pciverbose >= 1)
	    pci_tagprintf(tag, "pci_map_io: bad request\r\n");
	return -1;
    }
    
    address = pci_conf_read(tag, reg);
    
    if ((address & PCI_MAPREG_TYPE_IO) == 0) {
	if (_pciverbose >= 1)
	    pci_tagprintf (tag, "pci_map_io: attempt to i/o map a memory region\r\n");
	return -1;
    }

    pa = (address & PCI_MAPREG_IO_ADDR_MASK) - PCI_IO_SPACE_PCI_BASE;
    pa += PCI_IO_SPACE;
    *pap = pa;
    
    if (_pciverbose >= 3)
	pci_tagprintf(tag, "pci_map_io: mapping i/o at physical %016llx\n", 
		       *pap);

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
	if (!(address & PCI_MAPREG_ROM_ENABLE)) {
	    pci_tagprintf (tag, "pci_map_mem: attempt to map missing rom\r\n");
	    return -1;
	}
	pa = address & PCI_MAPREG_ROM_ADDR_MASK;
    } else {
	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	    if (_pciverbose >= 1)
		pci_tagprintf(tag, "pci_map_mem: bad request\r\n");
	    return -1;
	}
	
	address = pci_conf_read(tag, reg);
	
	if ((address & PCI_MAPREG_TYPE_IO) != 0) {
	    if (_pciverbose >= 1)
		pci_tagprintf(tag, "pci_map_mem: attempt to memory map an I/O region\r\n");
	    return -1;
	}
	
	switch (address & PCI_MAPREG_MEM_TYPE_MASK) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
	    break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
	    if (_pciverbose >= 1)
		pci_tagprintf (tag, "pci_map_mem: attempt to map 64-bit region\r\n");
	    return -1;
	default:
	    if (_pciverbose >= 1)
		pci_tagprintf (tag, "pci_map_mem: reserved mapping type\r\n");
	    return -1;
	}
	pa = address & PCI_MAPREG_MEM_ADDR_MASK;
    }

    pa -= pci_mem_space_pci_base;
    pa += PCI_MEM_SPACE;
    *pap = pa;

    if (_pciverbose >= 3)
	pci_tagprintf (tag, "pci_map_mem: mapping memory at physical 0x%016llx\r\n", 
			*pap);
    return 0;
}


/* Algorthmics P5064 PCI mappings.

   IDSEL (Table 5.8):
        device    IDSEL   DevID      INT{A,B,C,D}
      PCI slot 1   29       5       PCIIRQ{2,3,0,1}
      PCI slot 2   28       4       PCIIRQ{3,0,1,2}
      PCI slot 3   27       3       PCIIRQ{0,1,2,3}
      ISA bridge   26       2
      SCSI ctrl    25       1
      Enet ctrl    24       0
   The IDSEL bit to DevID mapping is nonstandard; see pci_conf_readn
   and pci_conf_writen above.

   Interrupt Request/Enable (PCIINT at 0x8)
        7     6     5     4     3     2     1     0       
      pci3  pci2  pci1  pci0   usb  scsi   eth  e_mdint
*/
    
/* The base shift of a slot or device on the motherboard.  Only device
   ids 3, 4 and 5 are implemented as PCI connectors. */
uint8_t
pci_int_shift_0(pcitag_t tag)
{
    int bus, device;

    pci_break_tag (tag, &bus, &device, NULL);

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

    pci_break_tag (tag, &bus, &device, NULL);

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


#define ISAPORT_ADDR(port) (MIPS_PHYS_TO_K1 (ISAPORT_BASE(port)))

uint64_t 
iodev_map (unsigned int port)
{
    return ISAPORT_ADDR(port);
}

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
