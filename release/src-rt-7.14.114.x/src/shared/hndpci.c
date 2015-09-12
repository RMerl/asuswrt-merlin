/*
 * Low-Level PCI and SI support for BCM47xx
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hndpci.c 404499 2013-05-28 01:06:37Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <pcicfg.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <bcmutils.h>
#include <siutils.h>
#include <pci_core.h>
#include <pcie_core.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <hndcpu.h>
#include <hndpci.h>
#include <nicpci.h>
#include <sbchipc.h>

/* For now we need some real Silicon Backplane utils */
#include "siutils_priv.h"

/* debug/trace */
#define	PCI_MSG(args)

/* to free some function memory after boot */
#ifndef linux
#define __init
#endif /* linux */

/* Emulated configuration space */
typedef struct {
	int	n;
	uint	size[PCI_BAR_MAX];
} si_bar_cfg_t;
static pci_config_regs si_config_regs[SI_MAXCORES];
static si_bar_cfg_t si_bar_cfg[SI_MAXCORES];

/* Links to emulated and real PCI configuration spaces */
#define MAXFUNCS	2
typedef struct {
	pci_config_regs *emu;	/* emulated PCI config */
	pci_config_regs *pci;	/* real PCI config */
	si_bar_cfg_t *bar;	/* region sizes */
} si_pci_cfg_t;
static si_pci_cfg_t si_pci_cfg[SI_MAXCORES][MAXFUNCS];

/* Special emulated config space for non-existing device */
static pci_config_regs si_pci_null = { 0xffff, 0xffff };

/* Banned cores */
static uint16 pci_ban[SI_MAXCORES] = { 0 };
static uint pci_banned = 0;

/* CardBus mode */
static bool cardbus = FALSE;

/* The OS's enumerated bus numbers for supported PCI/PCIe core units */
static uint pci_busid[SI_PCI_MAXCORES] = { 0, };

static uint32 pci_membase_cfg[SI_PCI_MAXCORES] = {
	SI_PCI_CFG,
};

static uint32 pci_membase[SI_PCI_MAXCORES] = {
	SI_PCI_DMA,
};

static uint32 pci_membase_1G[SI_PCI_MAXCORES] = {
	SI_PCI_DMA,
};

/* Disable PCI host core */
static bool pci_disabled[SI_PCI_MAXCORES] = {	FALSE };

/* Host bridge slot #, default to 0 */
static uint8 pci_hbslot = 0;

/* Internal macros */
#define PCI_SLOTAD_MAP	16	/* SLOT<n> mapps to AD<n+16> */
#define PCI_HBSBCFG_REV	8	/* MIN corerev to access host bridge PCI cfg space from SB */

/* Functions for accessing external PCI configuration space, Assume one-hot slot wiring */
#define PCI_SLOT_MAX	16	/* Max. PCI Slots */

static void
hndpci_set_busid(uint busid)
{	int i;

	for (i = 0; i < SI_PCI_MAXCORES; i++) {
		if (pci_busid[i] == 0) {
			pci_busid[i] = busid;
			printf("PCI/PCIe coreunit %d is set to bus %d.\n", i, pci_busid[i]);
			return;
		}
		if (busid == pci_busid[i])
			return;
	}
}

static int
hndpci_pci_coreunit(uint bus)
{	uint i;

	ASSERT(bus >= 1);
	for (i = SI_PCI_MAXCORES - 1; i >= 0; i--) {
		if (pci_busid[i] && bus >= pci_busid[i])
			return i;
	}
	return -1;
}

bool
hndpci_is_hostbridge(uint bus, uint dev)
{	uint i;

	ASSERT(bus >= 1);
	if (dev != pci_hbslot)
		return FALSE;

	for (i = 0; i < SI_PCI_MAXCORES; i++)
		if (bus == pci_busid[i])
			return TRUE;

	return FALSE;
}

uint32 hndpci_get_membase(uint bus)
{
	int coreunit;

	coreunit = hndpci_pci_coreunit(bus);
	ASSERT(coreunit >= 0);
	ASSERT(pci_membase[coreunit]);
	return pci_membase[coreunit];
}

static uint32
config_cmd(si_t *sih, uint coreunit, uint bus, uint dev, uint func, uint off)
{
	uint coreidx;
	sbpciregs_t *pci;
	uint32 addr = 0, *sbtopci1;
	osl_t *osh;

	/* CardBusMode supports only one device */
	if (cardbus && dev > 1)
		return 0;

	osh = si_osh(sih);

	coreidx = si_coreidx(sih);
	pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, coreunit);

	if (pci) {
		sbtopci1 = &pci->sbtopci1;
	} else {
		sbpcieregs_t *pcie;

		pcie = (sbpcieregs_t *)si_setcore(sih, PCIE_CORE_ID, coreunit);

		/* Issue config commands only when the data link is up (atleast
		 * one external pcie device is present).
		 */
		if (pcie && (dev < 2) &&
		    (pcie_readreg(sih, pcie, PCIE_PCIEREGS,
		    PCIE_DLLP_LSREG) & PCIE_DLLP_LSREG_LINKUP)) {
			sbtopci1 = &pcie->sbtopcie1;
		} else {
			si_setcoreidx(sih, coreidx);
			return 0;
		}
	}


	/* Type 0 transaction */
	if (!hndpci_is_hostbridge(bus, dev)) {
		/* Skip unwired slots */
		if (dev < PCI_SLOT_MAX) {
			/* Slide the PCI window to the appropriate slot */
			if (pci) {
				uint32 win;

				win = (SBTOPCI_CFG0 |
					((1 << (dev + PCI_SLOTAD_MAP)) & SBTOPCI1_MASK));
				W_REG(osh, sbtopci1, win);
				addr = (pci_membase_cfg[coreunit] |
					((1 << (dev + PCI_SLOTAD_MAP)) & ~SBTOPCI1_MASK) |
					(func << PCICFG_FUN_SHIFT) |
					(off & ~3));
			} else {
				W_REG(osh, sbtopci1, SBTOPCI_CFG0);
				addr = (pci_membase_cfg[coreunit] |
					(dev << PCIECFG_SLOT_SHIFT) |
					(func << PCIECFG_FUN_SHIFT) |
					(off & ~3));
			}
		}
	} else {
		/* Type 1 transaction */
		W_REG(osh, sbtopci1, SBTOPCI_CFG1);
		addr = (pci_membase_cfg[coreunit] |
			(pci ? PCI_CONFIG_ADDR(bus, dev, func, (off & ~3)) :
			PCIE_CONFIG_ADDR((bus - 1), dev, func, (off & ~3))));
	}

	si_setcoreidx(sih, coreidx);

	return addr;
}

/**
 * Read host bridge PCI config registers from Silicon Backplane ( >= rev8 ).
 *
 * It returns TRUE to indicate that access to the host bridge's pci config
 * from SI is ok, and values in 'addr' and 'val' are valid.
 *
 * It can only read registers at multiple of 4-bytes. Callers must pick up
 * needed bytes from 'val' based on 'off' value. Value in 'addr' reflects
 * the register address where value in 'val' is read.
 */
static bool
si_pcihb_read_config(si_t *sih, uint coreunit, uint bus, uint dev, uint func,
	uint off, uint32 **addr, uint32 *val)
{
	sbpciregs_t *pci;
	osl_t *osh;
	uint coreidx;
	bool ret = FALSE;

	/* sanity check */
	ASSERT(hndpci_is_hostbridge(bus, dev));

	/* we support only two functions on device 0 */
	if (func > 1)
		return FALSE;

	osh = si_osh(sih);

	/* read pci config when core rev >= 8 */
	coreidx = si_coreidx(sih);
	pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, coreunit);
	if (pci) {
		if (si_corerev(sih) >= PCI_HBSBCFG_REV) {
			*addr = (uint32 *)&pci->pcicfg[func][off >> 2];
			*val = R_REG(osh, *addr);
			ret = TRUE;
		}
	} else {
		sbpcieregs_t *pcie;

		/* read pcie config */
		pcie = (sbpcieregs_t *)si_setcore(sih, PCIE_CORE_ID, coreunit);
		if (pcie != NULL) {
			/* accesses to config registers with offsets >= 256
			 * requires indirect access.
			 */
			if (off >= 256)
				*val = pcie_readreg(sih, pcie, PCIE_CONFIGREGS,
				                    PCIE_CONFIG_INDADDR(func, off));
			else {
				*addr = (uint32 *)&pcie->pciecfg[func][off >> 2];
				*val = R_REG(osh, *addr);
			}
			ret = TRUE;
		}
	}

	si_setcoreidx(sih, coreidx);

	return ret;
}

int
extpci_read_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	uint32 addr = 0, *reg = NULL, val;
	int ret = 0;
	int coreunit = hndpci_pci_coreunit(bus);

	if (coreunit < 0)
		return -1;

	/*
	 * Set value to -1 when:
	 *	flag 'pci_disabled' is true;
	 *	value of 'addr' is zero;
	 *	REG_MAP() fails;
	 *	BUSPROBE() fails;
	 */
	if (pci_disabled[coreunit])
		val = 0xffffffff;
	else if (hndpci_is_hostbridge(bus, dev)) {
		if (!si_pcihb_read_config(sih, coreunit, bus, dev, func, off, &reg, &val))
			return -1;
	} else if (((addr = config_cmd(sih, coreunit, bus, dev, func, off)) == 0) ||
	         ((reg = (uint32 *)REG_MAP(addr, len)) == 0) ||
	         (BUSPROBE(val, reg) != 0)) {
		PCI_MSG(("%s: Failed to read!\n", __FUNCTION__));
		val = 0xffffffff;
	}

	PCI_MSG(("%s: 0x%x <= 0x%p(0x%x), len %d, off 0x%x, buf 0x%p\n",
	       __FUNCTION__, val, reg, addr, len, off, buf));

	val >>= 8 * (off & 3);
	if (len == 4)
		*((uint32 *)buf) = val;
	else if (len == 2)
		*((uint16 *)buf) = (uint16) val;
	else if (len == 1)
		*((uint8 *)buf) = (uint8) val;
	else
		ret = -1;

	if (reg && addr)
		REG_UNMAP(reg);

	return ret;
}

int
extpci_write_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	osl_t *osh;
	uint32 addr = 0, *reg = NULL, val;
	int ret = 0;
	bool is_hostbridge;
	int coreunit = hndpci_pci_coreunit(bus);

	if (coreunit < 0)
		return -1;

	osh = si_osh(sih);

	/*
	 * Ignore write attempt when:
	 *	flag 'pci_disabled' is true;
	 *	value of 'addr' is zero;
	 *	REG_MAP() fails;
	 *	BUSPROBE() fails;
	 */
	if (pci_disabled[coreunit])
		return 0;
	if ((is_hostbridge = hndpci_is_hostbridge(bus, dev))) {
		if (!si_pcihb_read_config(sih, coreunit, bus, dev, func, off, &reg, &val))
			return -1;
	} else if (((addr = config_cmd(sih, coreunit, bus, dev, func, off)) == 0) ||
	         ((reg = (uint32 *)REG_MAP(addr, len)) == 0) ||
	         (BUSPROBE(val, reg) != 0)) {
		PCI_MSG(("%s: Failed to write!\n", __FUNCTION__));
		goto done;
	}

	if (len == 4)
		val = *((uint32 *)buf);
	else if (len == 2) {
		val &= ~(0xffff << (8 * (off & 3)));
		val |= *((uint16 *)buf) << (8 * (off & 3));
	} else if (len == 1) {
		val &= ~(0xff << (8 * (off & 3)));
		val |= *((uint8 *)buf) << (8 * (off & 3));
	} else {
		ret = -1;
		goto done;
	}

	PCI_MSG(("%s: 0x%x => 0x%p\n", __FUNCTION__, val, reg));

	if (is_hostbridge && reg == NULL) {
		sbpcieregs_t *pcie;
		uint coreidx;

		coreidx = si_coreidx(sih);

		/* read pcie config */
		pcie = (sbpcieregs_t *)si_setcore(sih, PCIE_CORE_ID, coreunit);
		if (pcie != NULL)
			/* accesses to config registers with offsets >= 256
			 * requires indirect access.
			 */
			pcie_writereg(sih, pcie, PCIE_CONFIGREGS,
			              PCIE_CONFIG_INDADDR(func, off), val);

		si_setcoreidx(sih, coreidx);
	} else {
		W_REG(osh, reg, val);

		if ((CHIPID(sih->chip) == BCM4716_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4748_CHIP_ID))
			(void)R_REG(osh, reg);
	}


done:
	if (reg && addr)
		REG_UNMAP(reg);

	return ret;
}

/**
 * Must access emulated PCI configuration at these locations even when
 * the real PCI config space exists and is accessible.
 *
 * PCI_CFG_VID (0x00)
 * PCI_CFG_DID (0x02)
 * PCI_CFG_PROGIF (0x09)
 * PCI_CFG_SUBCL  (0x0a)
 * PCI_CFG_BASECL (0x0b)
 * PCI_CFG_HDR (0x0e)
 * PCI_CFG_INT (0x3c)
 * PCI_CFG_PIN (0x3d)
 */
#define FORCE_EMUCFG(off, len) \
	((off == PCI_CFG_VID) || (off == PCI_CFG_DID) || \
	 (off == PCI_CFG_PROGIF) || \
	 (off == PCI_CFG_SUBCL) || (off == PCI_CFG_BASECL) || \
	 (off == PCI_CFG_HDR) || \
	 (off == PCI_CFG_INT) || (off == PCI_CFG_PIN))

/** Sync the emulation registers and the real PCI config registers. */
static void
si_pcid_read_config(si_t *sih, uint coreidx, si_pci_cfg_t *cfg, uint off, uint len)
{
	osl_t *osh;
	uint oldidx;

	ASSERT(cfg);
	ASSERT(cfg->emu);
	ASSERT(cfg->pci);

	/* decide if real PCI config register access is necessary */
	if (FORCE_EMUCFG(off, len))
		return;

	osh = si_osh(sih);

	/* access to the real pci config space only when the core is up */
	oldidx = si_coreidx(sih);
	si_setcoreidx(sih, coreidx);
	if (si_iscoreup(sih)) {
		if (len == 4)
			*(uint32 *)((ulong)cfg->emu + off) =
			        htol32(R_REG(osh, (uint32 *)((ulong)cfg->pci + off)));
		else if (len == 2)
			*(uint16 *)((ulong)cfg->emu + off) =
			        htol16(R_REG(osh, (uint16 *)((ulong)cfg->pci + off)));
		else if (len == 1)
			*(uint8 *)((ulong)cfg->emu + off) =
			        R_REG(osh, (uint8 *)((ulong)cfg->pci + off));
	}
	si_setcoreidx(sih, oldidx);
}

static void
si_pcid_write_config(si_t *sih, uint coreidx, si_pci_cfg_t *cfg, uint off, uint len)
{
	osl_t *osh;
	uint oldidx;

	ASSERT(cfg);
	ASSERT(cfg->emu);
	ASSERT(cfg->pci);

	osh = si_osh(sih);

	/* decide if real PCI config register access is necessary */
	if (FORCE_EMUCFG(off, len))
		return;

	/* access to the real pci config space only when the core is up */
	oldidx = si_coreidx(sih);
	si_setcoreidx(sih, coreidx);
	if (si_iscoreup(sih)) {
		if (len == 4)
			W_REG(osh, (uint32 *)((ulong)cfg->pci + off),
			      ltoh32(*(uint32 *)((ulong)cfg->emu + off)));
		else if (len == 2)
			W_REG(osh, (uint16 *)((ulong)cfg->pci + off),
			      ltoh16(*(uint16 *)((ulong)cfg->emu + off)));
		else if (len == 1)
			W_REG(osh, (uint8 *)((ulong)cfg->pci + off),
			      *(uint8 *)((ulong)cfg->emu + off));
	}
	si_setcoreidx(sih, oldidx);
}

/*
 * Functions for accessing translated SI configuration space
 */
static int
si_read_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	pci_config_regs *cfg;

	if (dev >= SI_MAXCORES || func >= MAXFUNCS || (off + len) > sizeof(pci_config_regs))
		return -1;

	cfg = si_pci_cfg[dev][func].emu;

	ASSERT(ISALIGNED(off, len));
	ASSERT(ISALIGNED(buf, len));

	/* use special config space if the device does not exist */
	if (!cfg)
		cfg = &si_pci_null;
	/* sync emulation with real PCI config if necessary */
	else if (si_pci_cfg[dev][func].pci)
		si_pcid_read_config(sih, dev, &si_pci_cfg[dev][func], off, len);

	if (len == 4)
		*((uint32 *)buf) = ltoh32(*((uint32 *)((ulong) cfg + off)));
	else if (len == 2)
		*((uint16 *)buf) = ltoh16(*((uint16 *)((ulong) cfg + off)));
	else if (len == 1)
		*((uint8 *)buf) = *((uint8 *)((ulong) cfg + off));
	else
		return -1;

	return 0;
}

static int
si_write_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	uint coreidx;
	void *regs;
	pci_config_regs *cfg;
	osl_t *osh;
	si_bar_cfg_t *bar;

	if (dev >= SI_MAXCORES || func >= MAXFUNCS || (off + len) > sizeof(pci_config_regs))
		return -1;
	cfg = si_pci_cfg[dev][func].emu;
	if (!cfg)
		return -1;

	ASSERT(ISALIGNED(off, len));
	ASSERT(ISALIGNED(buf, len));

	osh = si_osh(sih);

	if (cfg->header_type == PCI_HEADER_BRIDGE) {
		uint busid = 0;
		if (off == OFFSETOF(ppb_config_regs, prim_bus) && len >= 2)
			busid = (*((uint16 *)buf) & 0xff00) >> 8;
		else if (off == OFFSETOF(ppb_config_regs, sec_bus))
			busid = *((uint8 *)buf);
		if (busid)
			hndpci_set_busid(busid);
	}

	/* Emulate BAR sizing */
	if (off >= OFFSETOF(pci_config_regs, base[0]) &&
	    off <= OFFSETOF(pci_config_regs, base[3]) &&
	    len == 4 && *((uint32 *)buf) == ~0) {
		coreidx = si_coreidx(sih);
		if ((regs = si_setcoreidx(sih, dev))) {
			bar = si_pci_cfg[dev][func].bar;
			/* Highest numbered address match register */
			if (off == OFFSETOF(pci_config_regs, base[0]))
				cfg->base[0] = ~(bar->size[0] - 1);
			else if (off == OFFSETOF(pci_config_regs, base[1]) && bar->n >= 1)
				cfg->base[1] = ~(bar->size[1] - 1);
			else if (off == OFFSETOF(pci_config_regs, base[2]) && bar->n >= 2)
				cfg->base[2] = ~(bar->size[2] - 1);
			else if (off == OFFSETOF(pci_config_regs, base[3]) && bar->n >= 3)
				cfg->base[3] = ~(bar->size[3] - 1);
		}
		si_setcoreidx(sih, coreidx);
	} else if (len == 4)
		*((uint32 *)((ulong) cfg + off)) = htol32(*((uint32 *)buf));
	else if (len == 2)
		*((uint16 *)((ulong) cfg + off)) = htol16(*((uint16 *)buf));
	else if (len == 1)
		*((uint8 *)((ulong) cfg + off)) = *((uint8 *)buf);
	else
		return -1;

	/* sync emulation with real PCI config if necessary */
	if (si_pci_cfg[dev][func].pci)
		si_pcid_write_config(sih, dev, &si_pci_cfg[dev][func], off, len);

	return 0;
}

int
hndpci_read_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	if (bus == 0)
		return si_read_config(sih, bus, dev, func, off, buf, len);
	else
		return extpci_read_config(sih, bus, dev, func, off, buf, len);
}

int
hndpci_write_config(si_t *sih, uint bus, uint dev, uint func, uint off, void *buf, int len)
{
	if (bus == 0)
		return si_write_config(sih, bus, dev, func, off, buf, len);
	else
		return extpci_write_config(sih, bus, dev, func, off, buf, len);
}

void
hndpci_ban(uint16 core)
{
	if (pci_banned < ARRAYSIZE(pci_ban))
		pci_ban[pci_banned++] = core;
}

/** return cap_offset if requested capability exists in the PCI config space */
uint8
hndpci_find_pci_capability(si_t *sih, uint bus, uint dev, uint func,
                           uint8 req_cap_id, uchar *buf, uint32 *buflen)
{
	uint8 cap_id;
	uint8 cap_ptr = 0;
	uint32 bufsize;
	uint8 byte_val;

	/* check for Header type 0 */
	hndpci_read_config(sih, bus, dev, func, PCI_CFG_HDR, &byte_val, sizeof(uint8));
	if ((byte_val & 0x7f) != PCI_HEADER_NORMAL)
		return (cap_ptr);

	/* check if the capability pointer field exists */
	hndpci_read_config(sih, bus, dev, func, PCI_CFG_STAT, &byte_val, sizeof(uint8));
	if (!(byte_val & PCI_CAPPTR_PRESENT))
		return (cap_ptr);

	/* check if the capability pointer is 0x00 */
	hndpci_read_config(sih, bus, dev, func, PCI_CFG_CAPPTR, &cap_ptr, sizeof(uint8));
	if (cap_ptr == 0x00)
		return (cap_ptr);

	/* loop thr'u the capability list and see if the requested capabilty exists */
	hndpci_read_config(sih, bus, dev, func, cap_ptr, &cap_id, sizeof(uint8));
	while (cap_id != req_cap_id) {
		hndpci_read_config(sih, bus, dev, func, cap_ptr + 1, &cap_ptr, sizeof(uint8));
		if (cap_ptr == 0x00)
			return (cap_ptr);
		hndpci_read_config(sih, bus, dev, func, cap_ptr, &cap_id, sizeof(uint8));
	}

	/* found the caller requested capability */
	if ((buf != NULL) && (buflen != NULL)) {
		uint8 cap_data;

		bufsize = *buflen;
		if (!bufsize)
			return (cap_ptr);

		*buflen = 0;

		/* copy the cpability data excluding cap ID and next ptr */
		cap_data = cap_ptr + 2;
		if ((bufsize + cap_data)  > SZPCR)
			bufsize = SZPCR - cap_data;
		*buflen = bufsize;
		while (bufsize--) {
			hndpci_read_config(sih, bus, dev, func, cap_data, buf, sizeof(uint8));
			cap_data++;
			buf++;
		}
	}

	return (cap_ptr);
}


/**
 * Initialize PCI core.
 * Return 0 after a successful initialization.
 * Otherwise return -1 to indicate there is no PCI core and
 * return 1 to indicate PCI core is disabled.
 */
int __init
BCMATTACHFN(hndpci_init_pci)(si_t *sih, uint coreunit)
{
	uint chip, chiprev, chippkg, host;
	uint32 boardflags;
	sbpciregs_t *pci;
	sbpcieregs_t *pcie = NULL;
	uint32 val;
	int ret = 0;
	const char *hbslot;
	osl_t *osh;
	int bus;

	chip = sih->chip;
	chiprev = sih->chiprev;
	chippkg = sih->chippkg;

	osh = si_osh(sih);

	pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, coreunit);
	if (pci == NULL) {
		pcie = (sbpcieregs_t *)si_setcore(sih, PCIE_CORE_ID, coreunit);
		if (pcie == NULL) {
			printf("PCI: no core\n");
			pci_disabled[coreunit] = TRUE;
			return -1;
		}
	}

	if ((CHIPID(chip) == BCM4706_CHIP_ID) && (coreunit == 1)) {
		/* Check if PCIE 1 is disabled */
		if (sih->chipst & CST4706_PCIE1_DISABLE) {
			pci_disabled[coreunit] = TRUE;
			host = 0;
			PCI_MSG(("PCIE port %d is disabled\n", port));
		}
	}

	boardflags = (uint32)getintvar(NULL, "boardflags");

	/*
	 * The NOPCI boardflag indicates we should not touch the PCI core,
	 * it may not be bonded out or the pins may be floating.
	 * The 200-pin BCM4712 package does not bond out PCI, and routers
	 * based on it did not use the boardflag.
	 */
	if ((boardflags & BFL_NOPCI) ||
	    ((chip == BCM4712_CHIP_ID) &&
	     ((chippkg == BCM4712SMALL_PKG_ID) || (chippkg == BCM4712MID_PKG_ID)))) {
		pci_disabled[coreunit] = TRUE;
		host = 0;
	} else {
		/* Enable the core */
		si_core_reset(sih, 0, 0);

		/* Figure out if it is in host mode:
		 * In host mode, it returns 0, in client mode, this register access will trap
		 * Trap handler must be implemented to support this like hndrte_mips.c
		 */
		host = !BUSPROBE(val, (pci ? &pci->control : &pcie->control));
	}

	if (!host) {
		ret = 1;

		/* Disable PCI interrupts in client mode */
		si_setint(sih, -1);

		/* Disable the PCI bridge in client mode */
		hndpci_ban(pci? PCI_CORE_ID : PCIE_CORE_ID);

		/* Make sure the core is disabled */
		si_core_disable(sih, 0);

		/* On 4716 (and other AXI chips?) make sure the slave wrapper
		 * is also put in reset.
		 */
		if ((chip == BCM4716_CHIP_ID) || (chip == BCM4748_CHIP_ID) ||
			(chip == BCM4706_CHIP_ID)) {
			uint32 *resetctrl;

			resetctrl = (uint32 *)OSL_UNCACHED(SI_WRAP_BASE + (9 * SI_CORE_SIZE) +
			                                   AI_RESETCTRL);
			W_REG(osh, resetctrl, AIRC_RESET);
		}

		printf("PCI: Disabled\n");
	} else {
		printf("PCI: Initializing host\n");

		/* Disable PCI SBReqeustTimeout for BCM4785 rev. < 2 */
		if (chip == BCM4785_CHIP_ID && chiprev < 2) {
			sbconfig_t *sb;
			sb = (sbconfig_t *)((ulong) pci + SBCONFIGOFF);
			AND_REG(osh, &sb->sbimconfiglow, ~0x00000070);
			sb_commit(sih);
		}

		if (pci) {
			/* Reset the external PCI bus and enable the clock */
			W_REG(osh, &pci->control, 0x5);	/* enable tristate drivers */
			W_REG(osh, &pci->control, 0xd);	/* enable the PCI clock */
			OSL_DELAY(150);			/* delay > 100 us */
			W_REG(osh, &pci->control, 0xf);	/* deassert PCI reset */
			/* Use internal arbiter and park REQ/GRNT at external master 0
			 * We will set it later after the bus has been probed
			 */
			W_REG(osh, &pci->arbcontrol, PCI_INT_ARB);
			OSL_DELAY(1);			/* delay 1 us */
		} else {
			printf("PCI: Reset RC\n");
			OSL_DELAY(3000);
			W_REG(osh, &pcie->control, PCIE_RST_OE);
			OSL_DELAY(50000);		/* delay 50 ms */
			W_REG(osh, &pcie->control, PCIE_RST | PCIE_RST_OE);
		}

		/* Enable CardBusMode */
		cardbus = getintvar(NULL, "cardbus") == 1;
		if (cardbus) {
			printf("PCI: Enabling CardBus\n");
			/* GPIO 1 resets the CardBus device on bcm94710ap */
			si_gpioout(sih, 1, 1, GPIO_DRV_PRIORITY);
			si_gpioouten(sih, 1, 1, GPIO_DRV_PRIORITY);
			W_REG(osh, &pci->sprom[0], R_REG(osh, &pci->sprom[0]) | 0x400);
		}

		/* Host bridge slot # nvram overwrite */
		if ((hbslot = nvram_get("pcihbslot"))) {
			pci_hbslot = bcm_strtoul(hbslot, NULL, 0);
			ASSERT(pci_hbslot < PCI_MAX_DEVICES);
		}

		bus = pci_busid[coreunit] = coreunit + 1;
		if (pci) {
			/* 64 MB I/O access window */
			W_REG(osh, &pci->sbtopci0, SBTOPCI_IO);
			/* 64 MB configuration access window */
			W_REG(osh, &pci->sbtopci1, SBTOPCI_CFG0);
			/* 1 GB memory access window */
			W_REG(osh, &pci->sbtopci2, SBTOPCI_MEM | SI_PCI_DMA);
		} else {
			uint8 cap_ptr, root_ctrl, root_cap, dev;
			uint16 val16;

			/* 64 MB I/O access window. On 4716, use
			 * sbtopcie0 to access the device registers. We
			 * can't use address match 2 (1 GB window) region
			 * as mips can't generate 64-bit address on the
			 * backplane.
			 */
			if ((chip == BCM4716_CHIP_ID) || (chip == BCM4748_CHIP_ID))
				W_REG(osh, &pcie->sbtopcie0, SBTOPCIE_MEM |
					(pci_membase[coreunit] = SI_PCI_MEM));
			else if (chip == BCM4706_CHIP_ID) {
				if (coreunit == 0) {
					pci_membase[coreunit] = SI_PCI_MEM;
					pci_membase_1G[coreunit] = SI_PCIE_DMA_H32;
				} else if (coreunit == 1) {
					pci_membase_cfg[coreunit] = SI_PCI1_CFG;
					pci_membase[coreunit] = SI_PCI1_MEM;
					pci_membase_1G[coreunit] = SI_PCIE1_DMA_H32;
				}
				W_REG(osh, &pcie->sbtopcie0,
					SBTOPCIE_MEM | SBTOPCIE_PF | SBTOPCIE_WR_BURST |
					pci_membase[coreunit]);
			}
			else
				W_REG(osh, &pcie->sbtopcie0, SBTOPCIE_IO);

			/* 64 MB configuration access window */
			W_REG(osh, &pcie->sbtopcie1, SBTOPCIE_CFG0);

			/* 1 GB memory access window */
			W_REG(osh, &pcie->sbtopcie2, SBTOPCIE_MEM |
				pci_membase_1G[coreunit]);

			/* As per PCI Express Base Spec 1.1 we need to wait for
			 * at least 100 ms from the end of a reset (cold/warm/hot)
			 * before issuing configuration requests to PCI Express
			 * devices.
			 */
			OSL_DELAY(100000);

			/* If the root port is capable of returning Config Request
			 * Retry Status (CRS) Completion Status to software then
			 * enable the feature.
			 */
			cap_ptr = hndpci_find_pci_capability(sih, bus, pci_hbslot, 0,
			                                     PCI_CAP_PCIECAP_ID, NULL, NULL);
			ASSERT(cap_ptr);

			root_cap = cap_ptr + OFFSETOF(pciconfig_cap_pcie, root_cap);
			hndpci_read_config(sih, bus, pci_hbslot, 0, root_cap,
			                   &val16, sizeof(uint16));
			if (val16 & PCIE_RC_CRS_VISIBILITY) {
				/* Enable CRS software visibility */
				root_ctrl = cap_ptr + OFFSETOF(pciconfig_cap_pcie, root_ctrl);
				val16 = PCIE_RC_CRS_EN;
				hndpci_write_config(sih, bus, pci_hbslot, 0, root_ctrl,
				                    &val16, sizeof(uint16));

				/* Initiate a configuration request to read the vendor id
				 * field of the device function's config space header after
				 * 100 ms wait time from the end of Reset. If the device is
				 * not done with its internal initialization, it must at
				 * least return a completion TLP, with a completion status
				 * of "Configuration Request Retry Status (CRS)". The root
				 * complex must complete the request to the host by returning
				 * a read-data value of 0001h for the Vendor ID field and
				 * all 1s for any additional bytes included in the request.
				 * Poll using the config reads for max wait time of 1 sec or
				 * until we receive the successful completion status. Repeat
				 * the procedure for all the devices.
				 */
				for (dev = pci_hbslot + 1; dev < PCI_MAX_DEVICES; dev++) {
					SPINWAIT((hndpci_read_config(sih, bus, dev, 0,
					         PCI_CFG_VID, &val16, sizeof(val16)),
					         (val16 == 0x1)), 1000000);
					if (val16 == 0x1)
						printf("PCI: Broken device in slot %d\n", dev);
				}
			}
		}

		if ((chip == BCM4706_CHIP_ID) || (chip == BCM4716_CHIP_ID)) {
			uint16 val16;
			hndpci_read_config(sih, bus, pci_hbslot, 0, PCI_CFG_DEVCTRL,
			                   &val16, sizeof(val16));
			val16 |= (2 << 5);	/* Max payload size of 512 */
			val16 |= (2 << 12);	/* MRRS 512 */
			hndpci_write_config(sih, bus, pci_hbslot, 0, PCI_CFG_DEVCTRL,
			                    &val16, sizeof(val16));
		}

		/* Enable PCI bridge BAR0 memory & master access */
		val = PCI_CMD_MASTER | PCI_CMD_MEMORY;
		hndpci_write_config(sih, bus, pci_hbslot, 0, PCI_CFG_CMD, &val, sizeof(val));

		/* Enable PCI interrupts */
		if (pci)
			W_REG(osh, &pci->intmask, PCI_INTA);
		else
			W_REG(osh, &pcie->intmask, PCI_INTA);
	}

	/* Reset busid to 0. Bus number will be assigned by OS later */
	pci_busid[coreunit] = 0;
	return ret;
}

void
hndpci_arb_park(si_t *sih, uint parkid)
{
	sbpciregs_t *pci;
	uint pcirev;
	uint32  arb;

	pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, 0);
	if ((pci == NULL) || pci_disabled[0]) {
		/* Should not happen */
		PCI_MSG(("%s: no PCI core\n", __FUNCTION__));
		return;
	}

	pcirev = si_corerev(sih);

	/* Nothing to do, not supported for these revs */
	if (pcirev < 8)
		return;

	/* Get parkid from NVRAM */
	if (parkid == PCI_PARK_NVRAM) {
		parkid = getintvar(NULL, "parkid");
		if (getvar(NULL, "parkid") == NULL)
			/* Not present in NVRAM use defaults */
			parkid = (pcirev >= 11) ? PCI11_PARKID_LAST : PCI_PARKID_LAST;
	}

	/* Check the parkid is valid, if not set it to default */
	if (parkid > ((pcirev >= 11) ? PCI11_PARKID_LAST : PCI_PARKID_LAST)) {
		printf("%s: Invalid parkid %d\n", __FUNCTION__, parkid);
		parkid = (pcirev >= 11) ? PCI11_PARKID_LAST : PCI_PARKID_LAST;
	}

	/* Now set the parkid */
	arb = R_REG(si_osh(sih), &pci->arbcontrol);
	arb &= ~PCI_PARKID_MASK;
	arb |= parkid << PCI_PARKID_SHIFT;
	W_REG(si_osh(sih), &pci->arbcontrol, arb);
	OSL_DELAY(1);
}

int
hndpci_deinit_pci(si_t *sih, uint coreunit)
{
	int coreidx;
	sbpciregs_t *pci;
	sbpcieregs_t *pcie = NULL;

	if (pci_disabled[coreunit])
		return 0;

	coreidx = si_coreidx(sih);
	pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, coreunit);
	if (pci == NULL) {
		pcie = (sbpcieregs_t *)si_setcore(sih, PCIE_CORE_ID, coreunit);
		if (pcie == NULL) {
			printf("PCI: no core\n");
			return -1;
		}
	}

	if (pci)
			W_REG(si_osh(sih), &pci->control, PCI_RST_OE);
	else
			W_REG(si_osh(sih), &pcie->control, PCIE_RST_OE);

	si_core_disable(sih, 0);
	si_setcoreidx(sih, coreidx);
	return 0;
}

/** Deinitialize PCI cores */
void
hndpci_deinit(si_t *sih)
{
	int coreunit;

	for (coreunit = 0; coreunit < SI_PCI_MAXCORES; coreunit++)
		hndpci_deinit_pci(sih, coreunit);
}

/** Get the PCI region address and size information */
static void __init
BCMATTACHFN(hndpci_init_regions)(si_t *sih, uint func, pci_config_regs *cfg, si_bar_cfg_t *bar)
{
	bool issb = sih->socitype == SOCI_SB;
	uint i, n;

	if ((si_coreid(sih) == USB20H_CORE_ID) ||
		(si_coreid(sih) == NS_USB20_CORE_ID)) {
		uint32 base, base1;

		base = htol32(si_addrspace(sih, 0));
		if (issb) {
			base1 = base + 0x800;	/* OHCI/EHCI */
		} else {
			/* In AI chips EHCI is addrspace 0, OHCI is 1 */
			base1 = base;
			if (((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM4749_CHIP_ID)) &&
			    CHIPREV(sih->chiprev) == 0)
				base = 0x18009000;
			else
				base = htol32(si_addrspace(sih, 1));
		}

		i = bar->n = 1;
		cfg->base[0] = func == 0 ? base : base1;
		bar->size[0] = issb ? 0x800 : 0x1000;
	} else {
		bar->n = n = si_numaddrspaces(sih);
		for (i = 0; i < n; i++) {
			int size = si_addrspacesize(sih, i);

			if (size) {
				cfg->base[i] = htol32(si_addrspace(sih, i));
				bar->size[i] = size;
			}
		}
	}
	for (; i < PCI_BAR_MAX; i++) {
		cfg->base[i] = 0;
		bar->size[i] = 0;
	}
}

/**
 * Construct PCI config spaces for SB cores to be accessed as if they were PCI devices.
 */
void __init
BCMATTACHFN(hndpci_init_cores)(si_t *sih)
{
	uint chiprev, coreidx, i;
	pci_config_regs *cfg, *pci;
	si_bar_cfg_t *bar;
	void *regs;
	osl_t *osh;
	uint16 vendor, device;
	uint16 coreid;
	uint8 class, subclass, progif;
	uint dev;
	uint8 header;
	uint func;

	chiprev = sih->chiprev;
	coreidx = si_coreidx(sih);

	osh = si_osh(sih);

	/* Scan the SI bus */
	bzero(si_config_regs, sizeof(si_config_regs));
	bzero(si_bar_cfg, sizeof(si_bar_cfg));
	bzero(si_pci_cfg, sizeof(si_pci_cfg));
	memset(&si_pci_null, -1, sizeof(si_pci_null));
	cfg = si_config_regs;
	bar = si_bar_cfg;
	for (dev = 0; dev < SI_MAXCORES; dev ++) {
		/* Check if the core exists */
		if (!(regs = si_setcoreidx(sih, dev)))
			continue;

		/* Check if this core is banned */
		coreid = si_coreid(sih);
		for (i = 0; i < pci_banned; i++)
			if (coreid == pci_ban[i])
				break;
		if (i < pci_banned)
			continue;

		if (coreid == USB20H_CORE_ID) {
			if (((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM4749_CHIP_ID)) &&
				(sih->chippkg == BCM5357_PKG_ID)) {
				printf("PCI: skip disabled USB20H\n");
				continue;
			}
		}

		if ((CHIPID(sih->chip) == BCM4706_CHIP_ID)) {
			if (coreid == GMAC_CORE_ID) {
				/* Only GMAC core 0 is used by 4706 */
				if (si_coreunit(sih) > 0) {
					continue;
				}
			}
		}

		for (func = 0; func < MAXFUNCS; ++func) {
			/* Make sure we won't go beyond the limit */
			if (cfg >= &si_config_regs[SI_MAXCORES]) {
				printf("PCI: too many emulated devices\n");
				goto done;
			}

			/* Convert core id to pci id */
			if (si_corepciid(sih, func, &vendor, &device, &class, &subclass,
			                 &progif, &header))
				continue;

			/*
			 * Differentiate real PCI config from emulated.
			 * non zero 'pci' indicate there is a real PCI config space
			 * for this device.
			 */
			switch (device) {
			case BCM47XX_GIGETH_ID:
				pci = (pci_config_regs *)((uint32)regs + 0x800);
				break;
			case BCM47XX_SATAXOR_ID:
				pci = (pci_config_regs *)((uint32)regs + 0x400);
				break;
			case BCM47XX_ATA100_ID:
				pci = (pci_config_regs *)((uint32)regs + 0x800);
				break;
			default:
				pci = NULL;
				break;
			}
			/* Supported translations */
			cfg->vendor = htol16(vendor);
			cfg->device = htol16(device);
			cfg->rev_id = chiprev;
			cfg->prog_if = progif;
			cfg->sub_class = subclass;
			cfg->base_class = class;
			cfg->header_type = header;
			hndpci_init_regions(sih, func, cfg, bar);
			/* Save core interrupt flag */
			cfg->int_pin = si_flag(sih);
			/* Save core interrupt assignment */
			cfg->int_line = si_irq(sih);
			/* Indicate there is no SROM */
			*((uint32 *)&cfg->sprom_control) = 0xffffffff;

			/* Point to the PCI config spaces */
			si_pci_cfg[dev][func].emu = cfg;
			si_pci_cfg[dev][func].pci = pci;
			si_pci_cfg[dev][func].bar = bar;
			cfg ++;
			bar ++;
		}
	}

done:
	si_setcoreidx(sih, coreidx);
}

/**
 * Initialize PCI core and construct PCI config spaces for SI cores.
 * Must propagate hndpci_init_pci() return value to the caller to let
 * them know the PCI core initialization status.
 */
int __init
BCMATTACHFN(hndpci_init)(si_t *sih)
{
	int coreunit, status = 0;

	for (coreunit = 0; coreunit < SI_PCI_MAXCORES; coreunit++)
		status |= hndpci_init_pci(sih, coreunit);
	hndpci_init_cores(sih);
	return status;
}
