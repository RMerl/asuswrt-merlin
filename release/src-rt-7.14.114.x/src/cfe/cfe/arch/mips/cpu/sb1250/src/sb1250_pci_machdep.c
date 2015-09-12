/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  BCM1250-specific PCI support		File: sb1250_pci_machdep.c
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
    ********************************************************************* */

/*
 * Based in part on the algor p5064 version of pci_machdep.c,
 * from SiByte sources dated 20000824.
 */

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
 * Sibyte SB-1250 machine-specific functions for PCI autoconfiguration.
 */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "bsp_config.h"
#include "cfe.h"
#include "sbmips.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "lib_physio.h"
#include "cfe_timer.h"
#include "env_subr.h"
extern void cfe_ledstr(const char *);

#include "pcivar.h"
#include "pcireg.h"
#include "ldtreg.h"

#define	SBD_DISPLAY(msg)	cfe_ledstr(msg)

const cons_t pci_optnames[] = {
    {"verbose",PCI_FLG_VERBOSE},
    {"ldt_prefetch",PCI_FLG_LDT_PREFETCH},
    {"ldt_rev_017",PCI_FLG_LDT_REV_017},
    {NULL,0}
};

extern int _pciverbose;


/* PCI regions in system physical (ZBbus) space.  See Figure 37. */

static struct {
    /* ZBbus space allocated for mapping to the standard PCI address spaces */
    uint32_t mem_space;
    uint32_t mem_space_size;
    uint32_t io_space;
    uint32_t io_space_size;
    uint32_t cfg_space;
    uint32_t cfg_space_size;

    /* PCI space available for configuration */
    uint32_t pci_mem_base;
    uint32_t pci_io_base;

    /* Bits for endian policy (0: match bytes, 1: match bits) */
    uint32_t mem_bit_endian;
    uint32_t io_bit_endian;
    uint32_t cfg_bit_endian;

    /* Match bits base for configuration (convenience variable) */
    physaddr_t cfg_base;
} Q;

static void
pci_set_root (void)
{
    Q.mem_space = A_PHYS_LDTPCI_IO_MATCH_BYTES_32;  /* 0x0040000000 */
    Q.mem_space_size = 0x0020000000;
    Q.io_space = A_PHYS_LDTPCI_IO_MATCH_BYTES;      /* 0x00DC000000 */
    Q.io_space_size = 0x0002000000;
    Q.cfg_space = A_PHYS_LDTPCI_CFG_MATCH_BYTES;    /* 0x00DE000000 */
    Q.cfg_space_size = 0x0001000000;

    Q.pci_mem_base = 0x40000000;
    Q.pci_io_base = 0x00000000;

    Q.mem_bit_endian = 0x0020000000;
    Q.io_bit_endian = 0x0020000000;
    Q.cfg_bit_endian = 0x0020000000;

    Q.cfg_base = PHYS_TO_XKSEG_UNCACHED(Q.cfg_space | Q.cfg_bit_endian);
}


/* Templates for bus attributes. */

static const struct pci_bus sb1250_pci_bus = {
	0,		/* minimum grant */
	255,		/* maximum latency */
	1,		/* devsel time = medium */
	1,		/* we support fast back-to-back */
	1,		/* we support prefetch */
	1,		/* we support 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	0,		/* initially no devices on bus */
};

static const struct pci_bus secondary_pci_bus = {
	0,		/* minimum grant */
	255,		/* maximum latency */
	0,		/* devsel time = unknown */
	0,		/* configure fast back-to-back */
	0,		/* we don't prefetch */
	0,		/* configure 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	0,		/* initially no devices on bus */
};

#define MAXBUS	10
static struct pci_bus _pci_bus[MAXBUS];
static int _pci_nbus = 0;

#define	SB1250_PCI_MAKE_TAG(b,d,f)					\
    (((b) << 16) | ((d) << 11) | ((f) << 8))

#if defined(__MIPSEB)
/* This is for big-endian with a match bits policy. */
#define	SB1250_CFG_ADDR(t, o, w)					\
    ((Q.cfg_base + (t) + (o)) ^ (4 - (w)))
#elif defined(__MIPSEL)
/* This is for little-endian, either policy. */
#define	SB1250_CFG_ADDR(t, o, w)					\
    (Q.cfg_base + (t) + (o))
#else
#error "Must specifiy either MIPSEL or MIPSEB"
#endif

pcireg_t  pci_conf_read8(pcitag_t, int);
void	  pci_conf_write8(pcitag_t, int, pcireg_t);
#ifndef pci_conf_read32
#define pci_conf_read32  pci_conf_read
#endif
#ifndef pci_conf_write32
#define pci_conf_write32 pci_conf_write
#endif


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
    /* skip the 16MB reserved for ISA mem space */
    return Q.pci_mem_base + 0x1000000;
}

pcireg_t
pci_maxmemaddr (int port)
{
    return Q.pci_mem_base + Q.mem_space_size;
}

pcireg_t
pci_minioaddr (int port)
{
    /* Skip the 32KB reserved for ISA i/o space. */
    return Q.pci_io_base + 0x8000;
}

pcireg_t
pci_maxioaddr (int port)
{
    return Q.pci_io_base + Q.io_space_size;
}


/* The SB-1250 integrated host bridges. */

#define PCI_VENDOR_SIBYTE               0x166d

#define	SB1250_PCI_BRIDGE	(SB1250_PCI_MAKE_TAG(0,0,0))
#define	SB1250_LDT_BRIDGE	(SB1250_PCI_MAKE_TAG(0,1,0))

static int sb1250_in_device_mode;
static int sb1250_ldt_slave_mode;
static int sb1250_ldt_init;   /* Set to one after LHB sees InitDone */

/* The pass 1 BCM1250 does not implement EOI cycles correctly.  Later
   passes do.  The following variable controls whether PCI interrupt
   mappings are programmed to be level-sensitive (EOI) or
   edge-sensitive (no EOI) in LDT-PCI bridges. */
int eoi_implemented;


/* Implementation-specific registers for the PCI Host Bridge (PHB) */

#define PHB_FEATURE_REG                 0x40

#define PHB_FEATURE_DEFAULTS		0x7db38080

#define PHB_MAP_REG_BASE                0x44
#define PHB_MAP_N_ENTRIES               16

/* The format of MAP table entries */
#define PHB_MAP_ENABLE                  (1 << 0)
#define PHB_MAP_SEND_LDT                (1 << 1)
#define PHB_MAP_L2CA                    (1 << 2)
#define PHB_MAP_ENDIAN                  (1 << 3)
#define PHB_MAP_ADDR_SHIFT              12
#define PHB_MAP_ADDR_MASK               0xfffff000
#define PHB_MAP_ENTRY_SPAN              (1 << 20)

#define PHB_ERRORADDR_REG               0x84

#define PHB_ADD_STAT_CMD_REG            0x88

#define PHB_SUBSYSSET_REG               0x8c

/* pass 2 additions */

#define PHB_READHOST_REG                0x94

#define PHB_READHOST_DISABLE            (0 << 0)   /* write only */
#define PHB_READHOST_ENABLE             (1 << 0)

#define PHB_ADAPTEXT_REG                0x98


/* PCI host bridge configuration
 *
 * Note that the PCI host bridge has two, mostly disjoint, sets of
 * configuration registers.  One is used in Host mode and is
 * accessible from the ZBbus; the other is used in Device mode and is
 * accessible from the PCI bus.  The MAP registers are shared but in
 * Pass 1 are write-only, from the ZBbus side.  In pass 2, they are
 * readable iff read_host is set.
 */
static void
phb_init (void)
{
    int i;
    pcireg_t csr, cr, icr;
    pcireg_t t;            /* used for reads that push writes */

    /* reset the PCI busses */
    /* PCI is only reset at system reset */
    
    /* PCI: disable and clear the BAR0 MAP registers */
    for (i = 0; i < PHB_MAP_N_ENTRIES; i++)
        pci_conf_write32(SB1250_PCI_BRIDGE, PHB_MAP_REG_BASE + 4*i, 0);

    /* Because they write to the ZBbus bank of configuration
       registers, some of the following initializations are noops in
       Device mode, but they do no harm. */

    /* PCI: set feature and timeout registers to their default. */
    pci_conf_write32(SB1250_PCI_BRIDGE, PHB_FEATURE_REG, PHB_FEATURE_DEFAULTS);

    /* PCI: enable bridge to PCI and PCI memory accesses, including
       write-invalidate, plus error handling */
    csr = PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE |
          PCI_COMMAND_INVALIDATE_ENABLE |
          PCI_COMMAND_SERR_ENABLE |  PCI_COMMAND_PARITY_ENABLE;
    pci_conf_write32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG, csr);

    /* PCI: clear errors */
    csr = pci_conf_read32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG);
    csr |= PCI_STATUS_PARITY_ERROR | PCI_STATUS_SYSTEM_ERROR |
           PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT |
           PCI_STATUS_TARGET_TARGET_ABORT | PCI_STATUS_PARITY_DETECT;
    pci_conf_write32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG, csr);

    /* PCI: set up interrupt mapping */
    icr = pci_conf_read(SB1250_PCI_BRIDGE, PCI_BPARAM_INTERRUPT_REG);
    icr &=~ (PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
    icr |= (pci_int_line(pci_int_map_0(SB1250_PCI_BRIDGE))
	    << PCI_INTERRUPT_LINE_SHIFT);
    pci_conf_write32(SB1250_PCI_BRIDGE, PCI_BPARAM_INTERRUPT_REG, icr);

    /* PCI: push the writes */
    t = pci_conf_read32(SB1250_PCI_BRIDGE, PCI_ID_REG);

    cr = pci_conf_read32(SB1250_PCI_BRIDGE, PCI_CLASS_REG);
    if (PCI_REVISION(cr) >= 2) {
	pcireg_t id;

	id = pci_conf_read32(SB1250_PCI_BRIDGE, PCI_ID_REG);
	pci_conf_write32(SB1250_PCI_BRIDGE, PHB_SUBSYSSET_REG, id);
	pci_conf_write32(SB1250_PCI_BRIDGE, PHB_READHOST_REG,
			 PHB_READHOST_DISABLE);
	t = pci_conf_read32(SB1250_PCI_BRIDGE, PHB_READHOST_REG);  /* push */
    }
}


/* Implementation-specific registers for the LDT Host Bridge (LHB) */

#define LHB_LINK_BASE                   0x40

#define LHB_LINKCMD_REG                 (LHB_LINK_BASE+LDT_COMMAND_CAP_OFF)

#define LHB_LINKCTRL_REG                (LHB_LINK_BASE+LDT_LINK_OFF(0))

#define LHB_LINKCTRL_CRCERROR           (1 << LDT_LINKCTRL_CRCERROR_SHIFT)
#define LHB_LINKCTRL_ERRORS             (LHB_LINKCTRL_CRCERROR | \
                                         LDT_LINKCTRL_LINKFAIL)

#define LHB_LINKFREQ_REG                (LHB_LINK_BASE+LDT_FREQ_OFF)
#define LHB_LFREQ_REG                   (LHB_LINKFREQ_REG+1)

#define LHB_LFREQ_200                   LDT_FREQ_200        /* 200 MHz */
#define LHB_LFREQ_300                   LDT_FREQ_300        /* 300 MHz */
#define LHB_LFREQ_400                   LDT_FREQ_400        /* 400 MHz */
#define LHB_LFREQ_500                   LDT_FREQ_500        /* 500 MHz */
#define LHB_LFREQ_600                   LDT_FREQ_600        /* 600 MHz */
#define LHB_LFREQ_800                   LDT_FREQ_800        /* 800 MHz */

#define LHB_SRI_CMD_REG                 0x50

#define LHB_SRI_CMD_SIPREADY            (1 << 16)
#define LHB_SRI_CMD_SYNCPTRCTL          (1 << 17)
#define LHB_SRI_CMD_REDUCESYNCZERO      (1 << 18)
#define LHB_SRI_CMD_DISSTARVATIONCNT    (1 << 19)
#define LHB_SRI_CMD_RXMARGIN_SHIFT      20
#define LHB_SRI_CMD_RXMARGIN_MASK       (0x1F << LHB_SRI_CMD_RXMARGIN_SHIFT)
#define LHB_SRI_CMD_PLLCOMPAT           (1 << 25)
#define LHB_SRI_CMD_TXOFFSET_SHIFT      28
#define LHB_SRI_CMD_TXOFFSET_MASK       (0x7 << LHB_SRI_CMD_TXOFFSET_SHIFT)
#define LHB_SRI_CMD_LINKFREQDIRECT      (1 << 31)

#define LHB_SRI_TXNUM_REG               0x54
#define LHB_SRI_RXNUM_REG               0x58

#define LHB_ERR_CTRL_REG                0x68

#define LHB_ERR_CTRL                    0x00ffffff
#define LHB_ERR_STATUS                  0xff000000

#define LHB_SRI_CTRL_REG                0x6c

#define LHB_ASTAT_REG                   0x70

#define LHB_ASTAT_TGTDONE_SHIFT         0
#define LHB_ASTAT_TGTDONE_MASK          (0xFF << LHB_ASTAT_TGTDONE_SHIFT)

#define LHB_TXBUFCNT_REG                0xc8

#if (LDT_DEBUG > 1)
static void
show_ldt_status (void)
{
    pcireg_t cmd;

    cmd = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_CMD_REG);
    xprintf(" SriCmd %04x\n", (cmd >> 16) & 0xffff);
    xprintf(" TXNum %08x TxDen %02x  RxNum %08x RxDen %02x ErrCtl %08x\n",
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_TXNUM_REG),
	    cmd & 0xff,
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_RXNUM_REG),
	    (cmd >> 8) & 0xff,
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG));
    xprintf(" LDTCmd %08x LDTCfg %08x LDTFreq %08x\n",
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCMD_REG),
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG),
	    pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKFREQ_REG));
}
#else
#define show_ldt_status() ((void)0)
#endif /* LDT_DEBUG */

/*
 * Assert warm reset for the given number of ticks.
 */
static void
lhb_link_reset (int delay)
{
    pcireg_t prev, cmd;
    pcireg_t brctrl;

    prev = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCMD_REG);
    cmd = prev | LDT_COMMAND_WARM_RESET;
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_LINKCMD_REG, cmd);

    brctrl = pci_conf_read32(SB1250_LDT_BRIDGE, PPB_BRCTL_INTERRUPT_REG);
    brctrl |= PPB_BRCTL_SECONDARY_RESET;
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_BRCTL_INTERRUPT_REG, brctrl);

    cfe_sleep(delay);

    brctrl &=~ PPB_BRCTL_SECONDARY_RESET;
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_BRCTL_INTERRUPT_REG, brctrl);
    brctrl = pci_conf_read32(SB1250_LDT_BRIDGE, PPB_BRCTL_INTERRUPT_REG);

    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_LINKCMD_REG, prev);
}

/*
 * Poll for InitDone on LHB's outgoing link.
 */
static int
lhb_link_ready (int maxpoll)
{
    volatile pcireg_t ctrl;
    int count;
    int linkerr;

    count = 0;
    linkerr = 0;
    ctrl = 0;

    while ((ctrl & (LDT_LINKCTRL_INITDONE | LDT_LINKCTRL_LINKFAIL)) == 0
	   && count < maxpoll) {
        ctrl = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG);
	count++;
	if ((ctrl & LHB_LINKCTRL_ERRORS) != 0 && !linkerr) {
	    if (_pciverbose > PCI_FLG_NORMAL)
		pci_tagprintf(SB1250_LDT_BRIDGE,
			      "LDT Err, count %d Err = 0x%04x\n",
			      count, ctrl & 0xffff);
	    linkerr = 1;
	}
	if (count == maxpoll) {
	    if (_pciverbose > PCI_FLG_NORMAL)
		pci_tagprintf(SB1250_LDT_BRIDGE, "Link timeout\n");
	    linkerr = 1;
	}
    }

    if (_pciverbose > PCI_FLG_NORMAL)
	pci_tagprintf(SB1250_LDT_BRIDGE, "lhb_link_ready: count %d\n", count);
    return linkerr;
}

static void
lhb_null_config (void)
{
    /* Even if the LDT fabric is not to be initialized by us, we must
       write the bus number, base and limit registers in the host
       bridge for proper operation (see 8.11.1) */
    pcireg_t iodata;
    pcireg_t memdata;

    /* The primary bus is 0.  Set secondary, subordinate to 0 also. */
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_BUSINFO_REG, 0);

    iodata = pci_conf_read32(SB1250_LDT_BRIDGE, PPB_IO_STATUS_REG);
    iodata &=~ (PPB_IO_BASE_MASK | PPB_IO_LIMIT_MASK);
    iodata |= (1 << 4) | (0 << (8+4));
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_IO_STATUS_REG, iodata);
    iodata = 0x0000f200;   /* recommended value */
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_IO_UPPER_REG, iodata);

    memdata = pci_conf_read32(SB1250_LDT_BRIDGE, PPB_MEM_REG);
    memdata &=~ (PPB_MEM_BASE_MASK | PPB_MEM_LIMIT_MASK);
    memdata |= (1 << 4) | (0 << (16+4));  /* empty window */
    pci_conf_write32(SB1250_LDT_BRIDGE, PPB_MEM_REG, memdata);
}


/*
 * LDT host bridge initialization.
 */
static void
lhb_init (int rev017, unsigned linkfreq, unsigned buffctl)
{
    int i;
    pcireg_t cr;
    pcireg_t sri_cmd;
    volatile pcireg_t t;      /* used for reads that push writes */
    uint8_t ldt_freq;
    int linkerr;
    int retry;

    sb1250_ldt_init = 0;

    /* Clear any pending error bits */
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG);
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG, t | LHB_ERR_STATUS);

    cr = pci_conf_read32(SB1250_LDT_BRIDGE, PCI_CLASS_REG);
    eoi_implemented = (PCI_REVISION(cr) >= 2);

    /* First set up System Reset Initialization registers (Table
       8-12).  This code is designed to be run following a full reset.
       After a chip warm reset (SipReady already set), most of it is
       skipped.  Depending on previous history, that can leave things
       in an inconsistent state, but there's no recovery from that
       short of a chip cold reset. */

    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_CMD_REG);
    if (t & LHB_SRI_CMD_SIPREADY) {
        pci_tagprintf(SB1250_LDT_BRIDGE, "Warning: SipReady already set\n");
	/* Try just doing a warm reset. */
	lhb_link_reset(CFE_HZ/10);
	goto finish;
    }

    if (rev017) {
	/* LDT 0.17 compatibility mode:
	     SriCmd = (!LinkFreqDirect, LdtPLLCompat, [DisStarveCnt,]
	               TxInitialOffset=5,
	               RxMargin=2,
	               !SipReady)
	*/
	sri_cmd = LHB_SRI_CMD_PLLCOMPAT;
    } else {
	/* LDT 1.0x partial compatibility mode:
	     SriCmd = (!LinkFreqDirect, !LdtPLLCompat, [DisStarveCnt,]
	               TxInitialOffset=5,
	               RxMargin=2,
	               !SipReady)
	*/
	sri_cmd = 0;
    }

    /* Empirically, RxMargin is not critical with a 200 MHz LDT clock
       but must be small (less than 15, and 0 seems reliable) with a
       400 MHz LDT clock.  Current default is 2. */
    sri_cmd |= ((2 << LHB_SRI_CMD_RXMARGIN_SHIFT) |
		(5 << LHB_SRI_CMD_TXOFFSET_SHIFT) |
		0x1010);        /* Rx/TxDen defaults */

    /* Setting DisStarveCnt is recommended for Pass 1 parts. */
    if (PCI_REVISION(cr) == 1)
	sri_cmd |= LHB_SRI_CMD_DISSTARVATIONCNT;

    /* Map the link frequency to a supported value.  Note: we assume
       that the CPU and IOB0 clocks are set high enough for the
       selected frequency.  In LDT 0.17 compatibility mode, this is
       the final frequency.  In LDT 1.0x partial compatibility mode,
       this becomes the target frequency for link sizing.  */
    if (linkfreq < 200)
	linkfreq = 200;
    else if (linkfreq > 800)
	linkfreq = 800;
    /* Passes 1 and 2 do not support LINKFREQDIRECT. Force a standard value. */
    linkfreq = ((linkfreq + (50-1))/100) * 100;
    if (linkfreq == 700) linkfreq = 600;   /* No code point */
    if (rev017)
	xprintf("HyperTransport: %d MHz\n", linkfreq);

    if (linkfreq % 100 == 0 && linkfreq != 700) {
	/* Encode supported standard values per the LDT spec. */      
	switch (linkfreq) {
	default:
	case 200:  ldt_freq = LHB_LFREQ_200;  break;
	case 300:  ldt_freq = LHB_LFREQ_300;  break;
	case 400:  ldt_freq = LHB_LFREQ_400;  break;
	case 500:  ldt_freq = LHB_LFREQ_500;  break;
	case 600:  ldt_freq = LHB_LFREQ_600;  break;
	case 800:  ldt_freq = LHB_LFREQ_800;  break;
	}
    } else {
	/* Compute PLL ratio for 100 MHz reference in 3b1 format (Table 23) */
	sri_cmd |= LHB_SRI_CMD_LINKFREQDIRECT;
	ldt_freq = linkfreq / 50;
    }
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_SRI_CMD_REG, sri_cmd);

    /* Set the SRI dividers */
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_SRI_TXNUM_REG, 0x0000ffff);
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_SRI_RXNUM_REG, 0x0000ffff);
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_RXNUM_REG); /* push */

    /* Directed test: SPIN(10) here */
    for (i = 0; i < 10; i++)
      t = pci_conf_read32(SB1250_LDT_BRIDGE, PCI_ID_REG);


    if (rev017) {
	if (ldt_freq == LHB_LFREQ_200) {
	    pci_conf_write8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG, LHB_LFREQ_400);
	    t = pci_conf_read8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG);
	    cfe_sleep (CFE_HZ/2);
	}
	pci_conf_write8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG, ldt_freq);
    } else {
	pci_conf_write8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG, LHB_LFREQ_400);
	t = pci_conf_read8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG);
	cfe_sleep (CFE_HZ/2);
	pci_conf_write8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG, LHB_LFREQ_200);
    }

    /* Set the Error Control register (some fatal interrupts). */
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG, 0x00001209);

    /* Set the SRI Xmit Control register. */
    pci_conf_write32(SB1250_LDT_BRIDGE,
		     LHB_SRI_CTRL_REG, 0x00040000 | buffctl);
    if (_pciverbose > PCI_FLG_NORMAL)
	pci_tagprintf(SB1250_LDT_BRIDGE, "BuffCtl = 0x%08x\n",
		      pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_CTRL_REG));
   
    /* Set the Tx buffer size (16 buffers each). */
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_TXBUFCNT_REG, 0x00ffffff);

    /* Push the writes */
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_TXBUFCNT_REG); /* push */

    /* Indicate SIP Ready */
    sri_cmd |= LHB_SRI_CMD_SIPREADY;
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_SRI_CMD_REG, sri_cmd);
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_SRI_CMD_REG);  /* push */

    retry = (sb1250_ldt_slave_mode ? 4 : 1);
    for (;;) {
	/* wait for LinkFail or InitDone */
	linkerr = lhb_link_ready(1<<20);   /* empirical delay */

	t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG);
	if ((t & LHB_ERR_STATUS) != 0) {
	    linkerr = 1;
	    if (_pciverbose > PCI_FLG_NORMAL)
		pci_tagprintf(SB1250_LDT_BRIDGE,
			      "ErrStat = 0x%02x\n", t >> 24);
	    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG,
			     t | LHB_ERR_STATUS);
	}

	t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG);
	if ((t & LHB_LINKCTRL_ERRORS) != 0) {
	    linkerr = 1;
	    if (_pciverbose > PCI_FLG_NORMAL)
		pci_tagprintf(SB1250_LDT_BRIDGE,
			      "LinkCtrl CRCErr = 0x%01x\n", (t >> 8) & 0xf);
	}

	if (!linkerr || retry == 0)
	    break;

	/* Clear errors in preparation for another try.  Delay long
           enough (below) for CRC errors to reappear; otherwise, the
           link can look good, but subsequent non-posted transactions
           will hang. */
	t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG);
	t |= LDT_LINKCTRL_CRCERROR_MASK;    /* Clear CrcErr bits */
	pci_conf_write32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG, t);

	/* Try again.  Do a reset iff an LDT master, since a poorly
           timed reset by a slave will break any link initialization
           in progress. */
	retry--;

	if (sb1250_ldt_slave_mode)
	    cfe_sleep(CFE_HZ/10);
	else
	    lhb_link_reset(CFE_HZ/10);
    }

    /* Rev 0.17 does not support dyanmic frequency updates. */
    if (!rev017) {
	/* Leave the target frequency in the LinkFreq register, which is
	   just a shadow until a link reset happens.  */
	pci_conf_write8(SB1250_LDT_BRIDGE, LHB_LFREQ_REG, ldt_freq);
    }
    
 finish:
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG);

    if ((t & LDT_LINKCTRL_INITDONE) == 0) {
	xprintf("HyperTransport not initialized: InitDone not set\n");
	if (_pciverbose > PCI_FLG_NORMAL)
	    pci_tagprintf(SB1250_LDT_BRIDGE,
			  "  Link Cmd = 0x%08x, Link Ctrl = 0x%08x\n",
			  pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCMD_REG),
			  pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG));
    } else if ((t & LHB_LINKCTRL_ERRORS) != 0) {
	xprintf("HyperTransport not initialized: "
		"LinkFail or CRCErr set, LinkCtrl = 0x%08x\n", t);
    } else if (!sb1250_ldt_slave_mode)
	sb1250_ldt_init = 1;

    /* Clear any pending error bits */
    t = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG);
    pci_conf_write32(SB1250_LDT_BRIDGE, LHB_ERR_CTRL_REG, t & 0xff000000);

    if (sb1250_ldt_slave_mode) {
	/* This is LDT slave mode.  The documentation is not very clear
	   on how much low level initialization should be done before
	   sleeping.  We just set Master Enable so that we can subsequently
           access LDT space. */
	pcireg_t cmd;

	/* If there are intermediate devices on the LDT, we would like
           our addressing to match the master's, but we don't know it
           and can't force it here.  Instead, we close all the windows
           into configurable space, which is at least safe. */
	lhb_null_config();

	cmd = pci_conf_read32(SB1250_LDT_BRIDGE, PCI_COMMAND_STATUS_REG);
	cmd &= (PCI_COMMAND_MASK << PCI_COMMAND_SHIFT);  /* preserve status */
	cmd |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write32(SB1250_LDT_BRIDGE, PCI_COMMAND_STATUS_REG, cmd);
    } else if (!sb1250_ldt_init) {
	pcireg_t lr;
  
	lhb_null_config();

	/* Also, terminate the link */
	lr = pci_conf_read32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG);
	lr |= LDT_LINKCTRL_EOC;
	pci_conf_write32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG, lr);
	lr |= LDT_LINKCTRL_TXOFF;
	pci_conf_write32(SB1250_LDT_BRIDGE, LHB_LINKCTRL_REG, lr);
    }

    show_ldt_status();
}


/*
 * Called to initialise IOB0 and the host bridges at the beginning of time.
 */
int
pci_hwinit (int port, pci_flags_t flags)
{
    int i;
    int rev017;
    unsigned linkfreq, buffctl;
    uint64_t syscfg;
    const char *str;

    /* define the address spaces and capabilities */

    if (port != 0)
	return -1;
    pci_set_root();

    /* initialise global data */

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    sb1250_in_device_mode = ((syscfg & M_SYS_PCI_HOST) == 0);
    if (cfe_startflags & CFE_LDT_SLAVE)
        sb1250_ldt_slave_mode = 1;
    else
        sb1250_ldt_slave_mode = 0;

    eoi_implemented = 0;   /* conservative default */

    /* Check for any relevant environment variables. */
    rev017 = ((flags & PCI_FLG_LDT_REV_017) != 0);

    /* Choose the LDT link frequency.  [C]SWARM boards are now set for
       400 MHz by default */
    str = env_getenv("LDT_LINKFREQ");
    linkfreq = (str ? atoi(str) : 400);

    /* Choose the buffer allocation (favor posted writes by default) */
    str = env_getenv("LDT_BUFFERS");
    buffctl = (str ? atoi(str) & 0xFFFF : 0x2525);

    _pci_bus[_pci_nbus] = sb1250_pci_bus;
    _pci_bus[_pci_nbus].port = port;
    _pci_nbus++;
    for (i = _pci_nbus; i < MAXBUS; i++)
	_pci_bus[i] = secondary_pci_bus;

    /* stop the SB-1250 from servicing any further PCI or LDT requests */
    pci_conf_write32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG, 0);
    pci_conf_write32(SB1250_LDT_BRIDGE, PCI_COMMAND_STATUS_REG, 0);

    /* initialize the PCI host bridge */
    phb_init();

    /* initialize the LDT host bridge */
    lhb_init(rev017, linkfreq, buffctl);

    cfe_sleep(CFE_HZ);   /* add some delay */

    return 0;
}

/*
 * Called to update the host bridge after we've scanned each PCI device
 * and know what is possible.
 */
void
pci_hwreinit (int port, pci_flags_t flags)
{
    pcireg_t cmd;

    /* note: this is not officially supported by sb1250, perhaps no effect! */
    if (_pci_bus[0].fast_b2b) {
	/* fast back-to-back is supported by all devices */
	cmd = pci_conf_read32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG);
	cmd &= (PCI_COMMAND_MASK << PCI_COMMAND_SHIFT);  /* preserve status */
	cmd |= PCI_COMMAND_BACKTOBACK_ENABLE;
	pci_conf_write32(SB1250_PCI_BRIDGE, PCI_COMMAND_STATUS_REG, cmd);
    }

    /* Latency timer, cache line size set by pci_setup_devices (pciconf.c) */

    /* enable PCI read/write error interrupts */
}


/* The following functions provide for device-specific setup required
   during configuration.  There is nothing SiByte-specific about them,
   and it would be better to do the packaging and registration in a
   more modular way. */

#define	PCI_VENDOR_API			0x14d9
#define PCI_PRODUCT_API_STURGEON	0x0010
extern void sturgeon_setup(pcitag_t tag, pci_flags_t flags);

#define	PCI_VENDOR_AMD			0x1022
#define PCI_PRODUCT_PLX_HT7520		0x7450
#define PCI_PRODUCT_PLX_HT7520_APIC	0x7451
extern void ht7520apic_preset(pcitag_t tag);
extern void ht7520apic_setup(pcitag_t tag);

#define PCI_PRODUCT_AMD_8151            0x7454

/* Dispatch functions for device pre- and post-configuration hooks. */

/* Called for each function prior to assigning PCI resources.  */
int
pci_device_preset (pcitag_t tag)
{
    pcireg_t id;
    int skip;

    skip = 0;
    id = pci_conf_read(tag, PCI_ID_REG);
    switch (PCI_VENDOR(id)) {
	case PCI_VENDOR_SIBYTE:
	    /* Check for a host bridge seen internally, in which case
	       we don't want to allocate any address space for its
	       BARs. */
	    if (tag == SB1250_PCI_BRIDGE)
		skip = 1;
	    break;
	case PCI_VENDOR_AMD:
	    switch (PCI_PRODUCT(id)) {
		case PCI_PRODUCT_PLX_HT7520_APIC:
		    ht7520apic_preset (tag);
		    break;
		case PCI_PRODUCT_AMD_8151:
		    skip = 1;
		    break;
		default:
		    break;
	    }
	    break;
	default:
	    break;
    }
    return skip;
}


void
pci_device_setup (pcitag_t tag)
{
    pcireg_t id = pci_conf_read(tag, PCI_ID_REG);

    switch (PCI_VENDOR(id)) {
	case PCI_VENDOR_AMD:
	    if (PCI_PRODUCT(id) == PCI_PRODUCT_PLX_HT7520_APIC)
		ht7520apic_setup (tag);
	    break;
	default:
	    break;
    }
}

/* Called for each bridge (Type 1) function after configuring the
   secondary bus, to allow device-specific initialization. */
void
pci_bridge_setup (pcitag_t tag, pci_flags_t flags)
{
    pcireg_t id = pci_conf_read(tag, PCI_ID_REG);

    switch (PCI_VENDOR(id)) {
	case PCI_VENDOR_API:
	    if (PCI_PRODUCT(id) == PCI_PRODUCT_API_STURGEON)
		sturgeon_setup (tag, flags);
	    break;
        case PCI_VENDOR_AMD:
	    /* The PLX ht7520 requires configuration of the
	       interrupt mapping, but it packages the IOAPIC as a
	       separate function, registers of which will not yet have
	       been initialized if the standard traversal order is
	       followed.  See previous.  */
	    break;
	default:
	    break;
    }
}


/* Machine dependent access primitives and utility functions */

void
pci_flush (void)
{
    /* note: this is a noop for the SB-1250. */
}


pcitag_t
pci_make_tag (int port, int bus, int device, int function)
{
    return SB1250_PCI_MAKE_TAG(bus, device, function);
}

void
pci_break_tag (pcitag_t tag,
	       int *portp, int *busp, int *devicep, int *functionp)
{
    if (portp) *portp = (tag >> 24) & PCI_PORTMAX;
    if (busp) *busp = (tag >> 16) & PCI_BUSMAX;
    if (devicep) *devicep = (tag >> 11) & PCI_DEVMAX;
    if (functionp) *functionp = (tag >> 8) & PCI_FUNCMAX;
}


int
pci_canscan (pcitag_t tag)
{
    int port, bus, device, function;

    pci_break_tag (tag, &port, &bus, &device, &function); 

    if (port > PCI_PORTMAX
	|| bus > PCI_BUSMAX || device > PCI_DEVMAX || function > PCI_FUNCMAX)
	return 0;

    if (bus == 0) {
	if (sb1250_in_device_mode) {
	    /* Scan the LDT chain, but only the LDT host bridge on PCI. */
	    if (device != 1)
	        return 0;
	}
	if (sb1250_ldt_slave_mode || !sb1250_ldt_init) {
	    /* Scan the PCI devices but not the LDT chain. */
            if (device == 1)
	        return 0;
	}
	if (device > 20) {
	    /* Chip bug: asserts IDSEL for device 20 for all devices > 20. */
	    return 0;
	}
    }

    return 1;
}

int
pci_probe_tag(pcitag_t tag)
{
    physaddr_t addrp;
    pcireg_t data;

    if (!pci_canscan(tag))
	return 0;

    addrp = (physaddr_t) SB1250_CFG_ADDR(tag, PCI_ID_REG, 4);

    /* An earlier version of this code cleared the MasterAbort and
       TargetAbort bits in the PCI host bridge, did the read, and
       looked for those bits to be set.  For the SB-1250, that's
       inappropriate because
	 - it's the wrong host bridge for devices behind LDT.
	 - PCI host bridge registers aren't readable in Device mode.
	 - it loses status if testing the PCI host bridge itself.
       We rely on getting 0xffff when reading the vendor ID.  Note
       that this still has side effects on the host bridge registers.
    */

    data = phys_read32(addrp);  /* device + vendor ID */
    mips_wbflush();

    /* if it returned all vendor id bits set, it's not a device */
    return (PCI_VENDOR(data) != 0xffff);
}


/* Read/write access to PCI configuration registers.  For most
   applications, pci_conf_read<N> and pci_conf_write<N> are deprecated
   unless N = 32. */

static pcireg_t
pci_conf_readn(pcitag_t tag, int reg, int width)
{
    physaddr_t addrp;
    pcireg_t data;
#if (PCI_DEBUG != 0)
    int port, bus, device, function;

    if (reg & (width-1) || reg < 0 || reg >= PCI_REGMAX) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_conf_readn: bad reg 0x%x\n", reg);
	return 0;
    }

    pci_break_tag(tag, &port, &bus, &device, &function); 
    if (bus > PCI_BUSMAX || device > PCI_DEVMAX || function > PCI_FUNCMAX) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_conf_readn: bad tag 0x%x\n", tag);
	return 0;
    }
#endif /* PCI_DEBUG */

    mips_wbflush();

    addrp = (physaddr_t) SB1250_CFG_ADDR(tag, reg, width);
    switch (width) {
    case 1:
	data = (pcireg_t) phys_read8(addrp);
	break;
    case 2:
	data = (pcireg_t) phys_read16(addrp);
	break;
    default:
    case 4:
	data = (pcireg_t) phys_read32(addrp);
	break;
    }

    mips_wbflush();

    return data;
}

pcireg_t
pci_conf_read8(pcitag_t tag, int reg)
{
    return pci_conf_readn(tag, reg, 1);
}

pcireg_t
pci_conf_read16(pcitag_t tag, int reg)
{
    return pci_conf_readn(tag, reg, 2);
}

pcireg_t
pci_conf_read(pcitag_t tag, int reg)
{
    return pci_conf_readn(tag, reg, 4);
}

static void
pci_conf_writen(pcitag_t tag, int reg, pcireg_t data, int width)
{
    physaddr_t addrp;
#if (PCI_DEBUG != 0)
    int port, bus, device, function;

    if (reg & (width-1) || reg < 0 || reg > PCI_REGMAX) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_conf_writen: bad reg 0x%x\n", reg);
	return;
    }

    pci_break_tag(tag, &port, &bus, &device, &function);
    if (bus > PCI_BUSMAX || device > PCI_DEVMAX || function > PCI_FUNCMAX) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_conf_writen: bad tag 0x%x\n", tag);
	return;
    }
#endif /* PCI_DEBUG */

    mips_wbflush();

    addrp = (physaddr_t) SB1250_CFG_ADDR(tag, reg, width);
    switch (width) {
    case 1:
	phys_write8(addrp, data);
	break;
    case 2:
	phys_write16(addrp, data);
	break;
    default:
    case 4:
	phys_write32(addrp, data);
	break;
    }

    mips_wbflush();
}

void
pci_conf_write8(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen(tag, reg, data, 1);
}

void
pci_conf_write16(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen(tag, reg, data, 2);
}

void
pci_conf_write(pcitag_t tag, int reg, pcireg_t data)
{
    pci_conf_writen(tag, reg, data, 4);
}

/* Acked writes are intended primarily for updating the unitID field
   during HT fabric initialization.  The write changes the address of
   the target, so further accesses should be avoided until the write
   completes or times out.   */
int
pci_conf_write_acked(pcitag_t tag, int reg, pcireg_t data)
{
    int done;

    if (sb1250_ldt_init) {
	int port, bus;
	pcireg_t bus_info, cr;
	int  i;

        pci_break_tag(tag, &port, &bus, NULL, NULL);
	bus_info = pci_conf_read(SB1250_LDT_BRIDGE, PPB_BUSINFO_REG);

	if (bus >= PPB_BUSINFO_SECONDARY(bus_info)
	    && bus <= PPB_BUSINFO_SUBORD(bus_info)) {

	    /* Write through the LDT host bridge.  An HT configuration
	       write is non-posted, but the ZBbus write completes as
	       if it were posted.  The following code assumes there
	       are no overlapping non-posted HT writes.  */

	    cr = pci_conf_read(SB1250_LDT_BRIDGE, PCI_CLASS_REG); 
	    if (PCI_REVISION(cr) >= 2) {
	        /* Current parts can count tgt_done responses. */
	        unsigned int count, prev_count;

		prev_count = pci_conf_read(SB1250_LDT_BRIDGE, LHB_ASTAT_REG);
		prev_count &= LHB_ASTAT_TGTDONE_MASK;

	        pci_conf_write(tag, reg, data);

		for (i = 0; i < 1000; i++) {
		    count = pci_conf_read(SB1250_LDT_BRIDGE, LHB_ASTAT_REG);
		    count &= LHB_ASTAT_TGTDONE_MASK;
		    if (count != prev_count)
			break;
		}
		done = (count != prev_count);
	    } else {
	        /* For pass 1, a couple of innocuous writes seems the
		   best we can do (a read with the new tag could hang) */
	        pci_conf_write(tag, reg, data);
		for (i = 0; i < 10; i++)
		    pci_conf_write(tag, PCI_ID_REG, 0);
		done = 1;
	    }
	} else {
	    /* Write through the PCI host bridge.  Just read it back.  */

	    pci_conf_write(tag, reg, data);
	    (void) pci_conf_read(tag, reg);   /* Push the write */
	    done = 1;
	}
    } else {
	/* No LDT.  Write and read back. */

	pci_conf_write(tag, reg, data);
	(void) pci_conf_read(tag, reg);
	done = 1;
    }
    return done;
}



int
pci_map_io(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
    pcireg_t address;
    phys_addr_t pa;
    
    if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_map_io: bad request\n");
	return -1;
    }
    
    address = pci_conf_read(tag, reg);
    
    if ((address & PCI_MAPREG_TYPE_IO) == 0) {
	if (_pciverbose != 0)
	    pci_tagprintf(tag, "pci_map_io: attempt to i/o map a memory region\n");
	return -1;
    }

    pa = ((address & PCI_MAPREG_IO_ADDR_MASK) - Q.pci_io_base) + Q.io_space;
    if (endian == PCI_MATCH_BITS)
        pa |= Q.io_bit_endian;
    *pap = pa;
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
	if ((address & PCI_MAPREG_ROM_ENABLE) == 0) {
	    pci_tagprintf(tag, "pci_map_mem: attempt to map missing rom\n");
	    return -1;
	}
	pa = address & PCI_MAPREG_ROM_ADDR_MASK;
    } else {
	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
	    if (_pciverbose != 0)
		pci_tagprintf(tag, "pci_map_mem: bad request\n");
	    return -1;
	}
	
	address = pci_conf_read(tag, reg);
	
	if ((address & PCI_MAPREG_TYPE_IO) != 0) {
	    if (_pciverbose != 0)
		pci_tagprintf(tag, "pci_map_mem: attempt to memory map an I/O region\n");
	    return -1;
	}
	
	pa = address & PCI_MAPREG_MEM_ADDR_MASK;

	switch (address & PCI_MAPREG_MEM_TYPE_MASK) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
	    break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
	    if (reg + 4 < PCI_MAPREG_END)
	        pa |= ((phys_addr_t)pci_conf_read(tag, reg+4) << 32);
	    else {
	        if (_pciverbose != 0)
		    pci_tagprintf(tag, "pci_map_mem: bad 64-bit reguest\n");
		return -1;
	    }
	    break;
	default:
	    if (_pciverbose != 0)
		pci_tagprintf(tag, "pci_map_mem: reserved mapping type\n");
	    return -1;
	}
    }

    pa = (pa - Q.pci_mem_base) + Q.mem_space;
    if (endian == PCI_MATCH_BITS)
        pa |= Q.mem_bit_endian;
    *pap = pa;
    return 0;
}


#define ISAPORT_BASE(x)     (Q.io_space + (x))

uint8_t
inb (unsigned int port)
{
    return phys_read8(ISAPORT_BASE(port));
}

uint16_t
inw (unsigned int port)
{
    return phys_read16(ISAPORT_BASE(port));
}

uint32_t
inl (unsigned int port)
{
    return phys_read32(ISAPORT_BASE(port));
}

void
outb (unsigned int port, uint8_t val)
{
    phys_write8(ISAPORT_BASE(port), val);
    mips_wbflush();
}

void
outw (unsigned int port, uint16_t val)
{
    phys_write16(ISAPORT_BASE(port), val);
    mips_wbflush();
}

void
outl (unsigned int port, uint32_t val)
{
    phys_write32(ISAPORT_BASE(port), val);
    mips_wbflush();
}


/* Management of MAP table */

int
pci_map_window(phys_addr_t pa,
	       unsigned int offset, unsigned int len,
	       int l2ca, int endian)
{
    unsigned int first, last;
    unsigned int i;
    uint32_t     addr;
    uint32_t     entry;

    if (len == 0)
        return 0;


    first = offset / PHB_MAP_ENTRY_SPAN;
    last = (offset + (len-1)) / PHB_MAP_ENTRY_SPAN;

    if (last >= PHB_MAP_N_ENTRIES)
        return -1;

    addr = (pa / PHB_MAP_ENTRY_SPAN) << PHB_MAP_ADDR_SHIFT;
    for (i = first; i <= last; i++) {
	entry = (addr & PHB_MAP_ADDR_MASK) | PHB_MAP_ENABLE;
	if (l2ca)
	    entry |= PHB_MAP_L2CA;
	if (endian)
	    entry |= PHB_MAP_ENDIAN;
	pci_conf_write32(SB1250_PCI_BRIDGE, PHB_MAP_REG_BASE + 4*i, entry);
	addr += (1 << PHB_MAP_ADDR_SHIFT);
    }

    return 0;
}

int
pci_unmap_window(unsigned int offset, unsigned int len)
{
    unsigned int first, last;
    unsigned int i;

    if (len == 0)
        return 0;


    first = offset / PHB_MAP_ENTRY_SPAN;
    if (first >= PHB_MAP_N_ENTRIES)
        return 0;

    last = (offset + (len-1)) / PHB_MAP_ENTRY_SPAN;
    if (last >= PHB_MAP_N_ENTRIES)
        last = PHB_MAP_N_ENTRIES - 1;

    for (i = first; i <= last; i++)
	pci_conf_write32(SB1250_PCI_BRIDGE, PHB_MAP_REG_BASE + 4*i, 0);

    return 0;
}


/* Map PCI interrupts A, B, C, D into a value for the IntLine
   register.  For SB1250, return the source number used by the
   interrupt mapper, or 0xff if none. */
uint8_t
pci_int_line(uint8_t pci_int)
{
    return (pci_int == 0) ? 0xff : (56 + (pci_int-1));
}
