/*
 * Misc utility routines for accessing chip-specific features
 * of the SiliconBackplane-based Broadcom chips.
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
 * $Id: sbutils.c 426572 2013-09-30 06:06:51Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#if !defined(BCMDONGLEHOST)
#include <pci_core.h>
#endif /* !defined(BCMDONGLEHOST) */
#include <pcicfg.h>
#include <sbpcmcia.h>

#include "siutils_priv.h"


/* local prototypes */
static uint _sb_coreidx(si_info_t *sii, uint32 sba);
static uint _sb_scan(si_info_t *sii, uint32 sba, void *regs, uint bus, uint32 sbba,
                     uint ncores);
static uint32 _sb_coresba(si_info_t *sii);
static void *_sb_setcoreidx(si_info_t *sii, uint coreidx);

#define	SET_SBREG(sii, r, mask, val)	\
		W_SBREG((sii), (r), ((R_SBREG((sii), (r)) & ~(mask)) | (val)))
#define	REGS2SB(va)	(sbconfig_t*) ((int8*)(va) + SBCONFIGOFF)

/* sonicsrev */
#define	SONICS_2_2	(SBIDL_RV_2_2 >> SBIDL_RV_SHIFT)
#define	SONICS_2_3	(SBIDL_RV_2_3 >> SBIDL_RV_SHIFT)

#define	R_SBREG(sii, sbr)	sb_read_sbreg((sii), (sbr))
#define	W_SBREG(sii, sbr, v)	sb_write_sbreg((sii), (sbr), (v))
#define	AND_SBREG(sii, sbr, v)	W_SBREG((sii), (sbr), (R_SBREG((sii), (sbr)) & (v)))
#define	OR_SBREG(sii, sbr, v)	W_SBREG((sii), (sbr), (R_SBREG((sii), (sbr)) | (v)))

static uint32
sb_read_sbreg(si_info_t *sii, volatile uint32 *sbr)
{
	uint8 tmp;
	uint32 val, intr_val = 0;


	/*
	 * compact flash only has 11 bits address, while we needs 12 bits address.
	 * MEM_SEG will be OR'd with other 11 bits address in hardware,
	 * so we program MEM_SEG with 12th bit when necessary(access sb regsiters).
	 * For normal PCMCIA bus(CFTable_regwinsz > 2k), do nothing special
	 */
	if (PCMCIA(sii)) {
		INTR_OFF(sii, intr_val);
		tmp = 1;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, MEM_SEG, &tmp, 1);
		sbr = (volatile uint32 *)((uintptr)sbr & ~(1 << 11)); /* mask out bit 11 */
	}

	val = R_REG(sii->osh, sbr);

	if (PCMCIA(sii)) {
		tmp = 0;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, MEM_SEG, &tmp, 1);
		INTR_RESTORE(sii, intr_val);
	}

	return (val);
}

static void
sb_write_sbreg(si_info_t *sii, volatile uint32 *sbr, uint32 v)
{
	uint8 tmp;
	volatile uint32 dummy;
	uint32 intr_val = 0;


	/*
	 * compact flash only has 11 bits address, while we needs 12 bits address.
	 * MEM_SEG will be OR'd with other 11 bits address in hardware,
	 * so we program MEM_SEG with 12th bit when necessary(access sb regsiters).
	 * For normal PCMCIA bus(CFTable_regwinsz > 2k), do nothing special
	 */
	if (PCMCIA(sii)) {
		INTR_OFF(sii, intr_val);
		tmp = 1;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, MEM_SEG, &tmp, 1);
		sbr = (volatile uint32 *)((uintptr)sbr & ~(1 << 11)); /* mask out bit 11 */
	}

	if (BUSTYPE(sii->pub.bustype) == PCMCIA_BUS) {
#ifdef IL_BIGENDIAN
		dummy = R_REG(sii->osh, sbr);
		BCM_REFERENCE(dummy);
		W_REG(sii->osh, ((volatile uint16 *)sbr + 1), (uint16)((v >> 16) & 0xffff));
		dummy = R_REG(sii->osh, sbr);
		BCM_REFERENCE(dummy);
		W_REG(sii->osh, (volatile uint16 *)sbr, (uint16)(v & 0xffff));
#else
		dummy = R_REG(sii->osh, sbr);
		BCM_REFERENCE(dummy);
		W_REG(sii->osh, (volatile uint16 *)sbr, (uint16)(v & 0xffff));
		dummy = R_REG(sii->osh, sbr);
		BCM_REFERENCE(dummy);
		W_REG(sii->osh, ((volatile uint16 *)sbr + 1), (uint16)((v >> 16) & 0xffff));
#endif	/* IL_BIGENDIAN */
	} else
		W_REG(sii->osh, sbr, v);

	if (PCMCIA(sii)) {
		tmp = 0;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, MEM_SEG, &tmp, 1);
		INTR_RESTORE(sii, intr_val);
	}
}

uint
sb_coreid(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	return ((R_SBREG(sii, &sb->sbidhigh) & SBIDH_CC_MASK) >> SBIDH_CC_SHIFT);
}

uint
sb_intflag(si_t *sih)
{
	si_info_t *sii;
	void *corereg;
	sbconfig_t *sb;
	uint origidx, intflag, intr_val = 0;

	sii = SI_INFO(sih);

	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);
	corereg = si_setcore(sih, CC_CORE_ID, 0);
	ASSERT(corereg != NULL);
	sb = REGS2SB(corereg);
	intflag = R_SBREG(sii, &sb->sbflagst);
	sb_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);

	return intflag;
}

uint
sb_flag(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	return R_SBREG(sii, &sb->sbtpsflag) & SBTPS_NUM0_MASK;
}

void
sb_setint(si_t *sih, int siflag)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint32 vec;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	if (siflag == -1)
		vec = 0;
	else
		vec = 1 << siflag;
	W_SBREG(sii, &sb->sbintvec, vec);
}

/* return core index of the core with address 'sba' */
static uint
BCMATTACHFN(_sb_coreidx)(si_info_t *sii, uint32 sba)
{
	uint i;

	for (i = 0; i < sii->numcores; i ++)
		if (sba == sii->coresba[i])
			return i;
	return BADIDX;
}

/* return core address of the current core */
static uint32
BCMATTACHFN(_sb_coresba)(si_info_t *sii)
{
	uint32 sbaddr;


	switch (BUSTYPE(sii->pub.bustype)) {
	case SI_BUS: {
		sbconfig_t *sb = REGS2SB(sii->curmap);
		sbaddr = sb_base(R_SBREG(sii, &sb->sbadmatch0));
		break;
	}

	case PCI_BUS:
		sbaddr = OSL_PCI_READ_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32));
		break;

	case PCMCIA_BUS: {
		uint8 tmp = 0;
		OSL_PCMCIA_READ_ATTR(sii->osh, PCMCIA_ADDR0, &tmp, 1);
		sbaddr  = (uint32)tmp << 12;
		OSL_PCMCIA_READ_ATTR(sii->osh, PCMCIA_ADDR1, &tmp, 1);
		sbaddr |= (uint32)tmp << 16;
		OSL_PCMCIA_READ_ATTR(sii->osh, PCMCIA_ADDR2, &tmp, 1);
		sbaddr |= (uint32)tmp << 24;
		break;
	}


#ifdef BCMJTAG
	case JTAG_BUS:
		sbaddr = (uint32)(uintptr)sii->curmap;
		break;
#endif

	default:
		sbaddr = BADCOREADDR;
		break;
	}

	return sbaddr;
}

uint
sb_corevendor(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	return ((R_SBREG(sii, &sb->sbidhigh) & SBIDH_VC_MASK) >> SBIDH_VC_SHIFT);
}

uint
sb_corerev(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint sbidh;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);
	sbidh = R_SBREG(sii, &sb->sbidhigh);

	return (SBCOREREV(sbidh));
}

/* set core-specific control flags */
void
sb_core_cflags_wo(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint32 w;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	ASSERT((val & ~mask) == 0);

	/* mask and set */
	w = (R_SBREG(sii, &sb->sbtmstatelow) & ~(mask << SBTML_SICF_SHIFT)) |
	        (val << SBTML_SICF_SHIFT);
	W_SBREG(sii, &sb->sbtmstatelow, w);
}

/* set/clear core-specific control flags */
uint32
sb_core_cflags(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint32 w;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	ASSERT((val & ~mask) == 0);

	/* mask and set */
	if (mask || val) {
		w = (R_SBREG(sii, &sb->sbtmstatelow) & ~(mask << SBTML_SICF_SHIFT)) |
		        (val << SBTML_SICF_SHIFT);
		W_SBREG(sii, &sb->sbtmstatelow, w);
	}

	/* return the new value
	 * for write operation, the following readback ensures the completion of write opration.
	 */
	return (R_SBREG(sii, &sb->sbtmstatelow) >> SBTML_SICF_SHIFT);
}

/* set/clear core-specific status flags */
uint32
sb_core_sflags(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint32 w;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	ASSERT((val & ~mask) == 0);
	ASSERT((mask & ~SISF_CORE_BITS) == 0);

	/* mask and set */
	if (mask || val) {
		w = (R_SBREG(sii, &sb->sbtmstatehigh) & ~(mask << SBTMH_SISF_SHIFT)) |
		        (val << SBTMH_SISF_SHIFT);
		W_SBREG(sii, &sb->sbtmstatehigh, w);
	}

	/* return the new value */
	return (R_SBREG(sii, &sb->sbtmstatehigh) >> SBTMH_SISF_SHIFT);
}

bool
sb_iscoreup(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	return ((R_SBREG(sii, &sb->sbtmstatelow) &
	         (SBTML_RESET | SBTML_REJ_MASK | (SICF_CLOCK_EN << SBTML_SICF_SHIFT))) ==
	        (SICF_CLOCK_EN << SBTML_SICF_SHIFT));
}

/*
 * Switch to 'coreidx', issue a single arbitrary 32bit register mask&set operation,
 * switch back to the original core, and return the new value.
 *
 * When using the silicon backplane, no fidleing with interrupts or core switches are needed.
 *
 * Also, when using pci/pcie, we can optimize away the core switching for pci registers
 * and (on newer pci cores) chipcommon registers.
 */
uint
sb_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	uint32 *r = NULL;
	uint w;
	uint intr_val = 0;
	bool fast = FALSE;
	si_info_t *sii;

	sii = SI_INFO(sih);

	ASSERT(GOODIDX(coreidx));
	ASSERT(regoff < SI_CORE_SIZE);
	ASSERT((val & ~mask) == 0);

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sii->pub.bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!sii->regs[coreidx]) {
			sii->regs[coreidx] = REG_MAP(sii->coresba[coreidx],
			                            SI_CORE_SIZE);
			ASSERT(GOODREGS(sii->regs[coreidx]));
		}
		r = (uint32 *)((uchar *)sii->regs[coreidx] + regoff);
	} else if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((sii->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (uint32 *)((char *)sii->curmap + PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (uint32 *)((char *)sii->curmap +
				               PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (uint32 *)((char *)sii->curmap +
				               ((regoff >= SBCONFIGOFF) ?
				                PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
				               regoff);
		}
	}

	if (!fast) {
		INTR_OFF(sii, intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (uint32*) ((uchar*)sb_setcoreidx(&sii->pub, coreidx) + regoff);
	}
	ASSERT(r != NULL);

	/* mask and set */
	if (mask || val) {
		if (regoff >= SBCONFIGOFF) {
			w = (R_SBREG(sii, r) & ~mask) | val;
			W_SBREG(sii, r, w);
		} else {
			w = (R_REG(sii->osh, r) & ~mask) | val;
			W_REG(sii->osh, r, w);
		}
	}

	/* readback */
	if (regoff >= SBCONFIGOFF)
		w = R_SBREG(sii, r);
	else {
		if ((CHIPID(sii->pub.chip) == BCM5354_CHIP_ID) &&
		    (coreidx == SI_CC_IDX) &&
		    (regoff == OFFSETOF(chipcregs_t, watchdog))) {
			w = val;
		} else
			w = R_REG(sii->osh, r);
	}

	if (!fast) {
		/* restore core index */
		if (origidx != coreidx)
			sb_setcoreidx(&sii->pub, origidx);

		INTR_RESTORE(sii, intr_val);
	}

	return (w);
}

/*
 * If there is no need for fiddling with interrupts or core switches (typically silicon
 * back plane registers, pci registers and chipcommon registers), this function
 * returns the register offset on this core to a mapped address. This address can
 * be used for W_REG/R_REG directly.
 *
 * For accessing registers that would need a core switch, this function will return
 * NULL.
 */
uint32 *
sb_corereg_addr(si_t *sih, uint coreidx, uint regoff)
{
	uint32 *r = NULL;
	bool fast = FALSE;
	si_info_t *sii;

	sii = SI_INFO(sih);

	ASSERT(GOODIDX(coreidx));
	ASSERT(regoff < SI_CORE_SIZE);

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sii->pub.bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!sii->regs[coreidx]) {
			sii->regs[coreidx] = REG_MAP(sii->coresba[coreidx],
			                            SI_CORE_SIZE);
			ASSERT(GOODREGS(sii->regs[coreidx]));
		}
		r = (uint32 *)((uchar *)sii->regs[coreidx] + regoff);
	} else if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((sii->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (uint32 *)((char *)sii->curmap + PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (uint32 *)((char *)sii->curmap +
				               PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (uint32 *)((char *)sii->curmap +
				               ((regoff >= SBCONFIGOFF) ?
				                PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
				               regoff);
		}
	}

	if (!fast)
		return 0;

	return (r);
}

/* Scan the enumeration space to find all cores starting from the given
 * bus 'sbba'. Append coreid and other info to the lists in 'si'. 'sba'
 * is the default core address at chip POR time and 'regs' is the virtual
 * address that the default core is mapped at. 'ncores' is the number of
 * cores expected on bus 'sbba'. It returns the total number of cores
 * starting from bus 'sbba', inclusive.
 */
#define SB_MAXBUSES	2
static uint
BCMATTACHFN(_sb_scan)(si_info_t *sii, uint32 sba, void *regs, uint bus, uint32 sbba, uint numcores)
{
	uint next;
	uint ncc = 0;
	uint i;

	if (bus >= SB_MAXBUSES) {
		SI_ERROR(("_sb_scan: bus 0x%08x at level %d is too deep to scan\n", sbba, bus));
		return 0;
	}
	SI_MSG(("_sb_scan: scan bus 0x%08x assume %u cores\n", sbba, numcores));

	/* Scan all cores on the bus starting from core 0.
	 * Core addresses must be contiguous on each bus.
	 */
	for (i = 0, next = sii->numcores; i < numcores && next < SB_BUS_MAXCORES; i++, next++) {
		sii->coresba[next] = sbba + (i * SI_CORE_SIZE);

		/* keep and reuse the initial register mapping */
		if ((BUSTYPE(sii->pub.bustype) == SI_BUS) && (sii->coresba[next] == sba)) {
			SI_VMSG(("_sb_scan: reuse mapped regs %p for core %u\n", regs, next));
			sii->regs[next] = regs;
		}

		/* change core to 'next' and read its coreid */
		sii->curmap = _sb_setcoreidx(sii, next);
		sii->curidx = next;

		sii->coreid[next] = sb_coreid(&sii->pub);

		/* core specific processing... */
		/* chipc provides # cores */
		if (sii->coreid[next] == CC_CORE_ID) {
			chipcregs_t *cc = (chipcregs_t *)sii->curmap;
			uint32 ccrev = sb_corerev(&sii->pub);

			/* determine numcores - this is the total # cores in the chip */
			if (((ccrev == 4) || (ccrev >= 6))) {
				ASSERT(cc);
				numcores = (R_REG(sii->osh, &cc->chipid) & CID_CC_MASK) >>
				        CID_CC_SHIFT;
			} else {
				/* Older chips */
				uint chip = CHIPID(sii->pub.chip);

				if (chip == BCM4306_CHIP_ID)	/* < 4306c0 */
					numcores = 6;
				else if (chip == BCM4704_CHIP_ID)
					numcores = 9;
				else if (chip == BCM5365_CHIP_ID)
					numcores = 7;
				else {
					SI_ERROR(("sb_chip2numcores: unsupported chip 0x%x\n",
					          chip));
					ASSERT(0);
					numcores = 1;
				}
			}
			SI_VMSG(("_sb_scan: there are %u cores in the chip %s\n", numcores,
				sii->pub.issim ? "QT" : ""));
		}
		/* scan bridged SB(s) and add results to the end of the list */
		else if (sii->coreid[next] == OCP_CORE_ID) {
			sbconfig_t *sb = REGS2SB(sii->curmap);
			uint32 nsbba = R_SBREG(sii, &sb->sbadmatch1);
			uint nsbcc;

			sii->numcores = next + 1;

			if ((nsbba & 0xfff00000) != SI_ENUM_BASE)
				continue;
			nsbba &= 0xfffff000;
			if (_sb_coreidx(sii, nsbba) != BADIDX)
				continue;

			nsbcc = (R_SBREG(sii, &sb->sbtmstatehigh) & 0x000f0000) >> 16;
			nsbcc = _sb_scan(sii, sba, regs, bus + 1, nsbba, nsbcc);
			if (sbba == SI_ENUM_BASE)
				numcores -= nsbcc;
			ncc += nsbcc;
		}
	}

	SI_MSG(("_sb_scan: found %u cores on bus 0x%08x\n", i, sbba));

	sii->numcores = i + ncc;
	return sii->numcores;
}

/* scan the sb enumerated space to identify all cores */
void
BCMATTACHFN(sb_scan)(si_t *sih, void *regs, uint devid)
{
	si_info_t *sii;
	uint32 origsba;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	sii->pub.socirev = (R_SBREG(sii, &sb->sbidlow) & SBIDL_RV_MASK) >> SBIDL_RV_SHIFT;

	/* Save the current core info and validate it later till we know
	 * for sure what is good and what is bad.
	 */
	origsba = _sb_coresba(sii);

	/* scan all SB(s) starting from SI_ENUM_BASE */
	sii->numcores = _sb_scan(sii, origsba, regs, 0, SI_ENUM_BASE, 1);
}

/*
 * This function changes logical "focus" to the indicated core;
 * must be called with interrupts off.
 * Moreover, callers should keep interrupts off during switching out of and back to d11 core
 */
void *
sb_setcoreidx(si_t *sih, uint coreidx)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (coreidx >= sii->numcores)
		return (NULL);

	/*
	 * If the user has provided an interrupt mask enabled function,
	 * then assert interrupts are disabled before switching the core.
	 */
	ASSERT((sii->intrsenabled_fn == NULL) || !(*(sii)->intrsenabled_fn)((sii)->intr_arg));

	sii->curmap = _sb_setcoreidx(sii, coreidx);
	sii->curidx = coreidx;

	return (sii->curmap);
}

/* This function changes the logical "focus" to the indicated core.
 * Return the current core's virtual address.
 */
static void *
_sb_setcoreidx(si_info_t *sii, uint coreidx)
{
	uint32 sbaddr = sii->coresba[coreidx];
	void *regs;

	switch (BUSTYPE(sii->pub.bustype)) {
	case SI_BUS:
		/* map new one */
		if (!sii->regs[coreidx]) {
			sii->regs[coreidx] = REG_MAP(sbaddr, SI_CORE_SIZE);
			ASSERT(GOODREGS(sii->regs[coreidx]));
		}
		regs = sii->regs[coreidx];
		break;

	case PCI_BUS:
		/* point bar0 window */
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, sbaddr);
		regs = sii->curmap;
		break;

	case PCMCIA_BUS: {
		uint8 tmp = (sbaddr >> 12) & 0x0f;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, PCMCIA_ADDR0, &tmp, 1);
		tmp = (sbaddr >> 16) & 0xff;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, PCMCIA_ADDR1, &tmp, 1);
		tmp = (sbaddr >> 24) & 0xff;
		OSL_PCMCIA_WRITE_ATTR(sii->osh, PCMCIA_ADDR2, &tmp, 1);
		regs = sii->curmap;
		break;
	}

#ifdef BCMJTAG
	case JTAG_BUS:
		/* map new one */
		if (!sii->regs[coreidx]) {
			sii->regs[coreidx] = (void *)(uintptr)sbaddr;
			ASSERT(GOODREGS(sii->regs[coreidx]));
		}
		regs = sii->regs[coreidx];
		break;
#endif	/* BCMJTAG */

	default:
		ASSERT(0);
		regs = NULL;
		break;
	}

	return regs;
}

/* Return the address of sbadmatch0/1/2/3 register */
static volatile uint32 *
sb_admatch(si_info_t *sii, uint asidx)
{
	sbconfig_t *sb;
	volatile uint32 *addrm;

	sb = REGS2SB(sii->curmap);

	switch (asidx) {
	case 0:
		addrm =  &sb->sbadmatch0;
		break;

	case 1:
		addrm =  &sb->sbadmatch1;
		break;

	case 2:
		addrm =  &sb->sbadmatch2;
		break;

	case 3:
		addrm =  &sb->sbadmatch3;
		break;

	default:
		SI_ERROR(("%s: Address space index (%d) out of range\n", __FUNCTION__, asidx));
		return 0;
	}

	return (addrm);
}

/* Return the number of address spaces in current core */
int
sb_numaddrspaces(si_t *sih)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	/* + 1 because of enumeration space */
	return ((R_SBREG(sii, &sb->sbidlow) & SBIDL_AR_MASK) >> SBIDL_AR_SHIFT) + 1;
}

/* Return the address of the nth address space in the current core */
uint32
sb_addrspace(si_t *sih, uint asidx)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	return (sb_base(R_SBREG(sii, sb_admatch(sii, asidx))));
}

/* Return the size of the nth address space in the current core */
uint32
sb_addrspacesize(si_t *sih, uint asidx)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	return (sb_size(R_SBREG(sii, sb_admatch(sii, asidx))));
}

#if defined(BCMDBG_ERR) || defined(BCMASSERT_SUPPORT)
/* traverse all cores to find and clear source of serror */
static void
sb_serr_clear(si_info_t *sii)
{
	sbconfig_t *sb;
	uint origidx;
	uint i, intr_val = 0;
	void *corereg = NULL;

	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(&sii->pub);

	for (i = 0; i < sii->numcores; i++) {
		corereg = sb_setcoreidx(&sii->pub, i);
		if (NULL != corereg) {
			sb = REGS2SB(corereg);
			if ((R_SBREG(sii, &sb->sbtmstatehigh)) & SBTMH_SERR) {
				AND_SBREG(sii, &sb->sbtmstatehigh, ~SBTMH_SERR);
				SI_ERROR(("sb_serr_clear: SError at core 0x%x\n",
				          sb_coreid(&sii->pub)));
			}
		}
	}

	sb_setcoreidx(&sii->pub, origidx);
	INTR_RESTORE(sii, intr_val);
}

/*
 * Check if any inband, outband or timeout errors has happened and clear them.
 * Must be called with chip clk on !
 */
bool
sb_taclear(si_t *sih, bool details)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint origidx;
	uint intr_val = 0;
	bool rc = FALSE;
	uint32 inband = 0, serror = 0, timeout = 0;
	void *corereg = NULL;
	volatile uint32 imstate, tmstate;
#ifdef BCMDBG
	bool printed = FALSE;
#endif

	sii = SI_INFO(sih);

	if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		volatile uint32 stcmd;

		/* inband error is Target abort for PCI */
		stcmd = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_CMD, sizeof(uint32));
		inband = stcmd & PCI_STAT_TA;
		if (inband) {
#ifdef BCMDBG
			if (details) {
				SI_ERROR(("\ninband:\n"));
				si_viewall((void*)sii, FALSE);
				printed = TRUE;
			}
#endif
			OSL_PCI_WRITE_CONFIG(sii->osh, PCI_CFG_CMD, sizeof(uint32), stcmd);
		}

		/* serror */
		stcmd = OSL_PCI_READ_CONFIG(sii->osh, PCI_INT_STATUS, sizeof(uint32));
		serror = stcmd & PCI_SBIM_STATUS_SERR;
		if (serror) {
#ifdef BCMDBG
			if (details) {
				SI_ERROR(("\nserror:\n"));
				if (!printed)
					si_viewall((void*)sii, FALSE);
				printed = TRUE;
			}
#endif
			sb_serr_clear(sii);
			OSL_PCI_WRITE_CONFIG(sii->osh, PCI_INT_STATUS, sizeof(uint32), stcmd);
		}

		/* timeout */
		imstate = sb_corereg(sih, sii->pub.buscoreidx,
		                     SBCONFIGOFF + OFFSETOF(sbconfig_t, sbimstate), 0, 0);
		if ((imstate != 0xffffffff) && (imstate & (SBIM_IBE | SBIM_TO))) {
			sb_corereg(sih, sii->pub.buscoreidx,
			           SBCONFIGOFF + OFFSETOF(sbconfig_t, sbimstate), ~0,
			           (imstate & ~(SBIM_IBE | SBIM_TO)));
			/* inband = imstate & SBIM_IBE; same as TA above */
			timeout = imstate & SBIM_TO;
			if (timeout) {
#ifdef BCMDBG
				if (details) {
					SI_ERROR(("\ntimeout:\n"));
					if (!printed)
						si_viewall((void*)sii, FALSE);
					printed = TRUE;
				}
#endif
			}
		}

		if (inband) {
			/* dump errlog for sonics >= 2.3 */
			if (sii->pub.socirev == SONICS_2_2)
				;
			else {
				uint32 imerrlog, imerrloga;
				imerrlog = sb_corereg(sih, sii->pub.buscoreidx, SBIMERRLOG, 0, 0);
				if (imerrlog & SBTMEL_EC) {
					imerrloga = sb_corereg(sih, sii->pub.buscoreidx,
					                       SBIMERRLOGA, 0, 0);
					BCM_REFERENCE(imerrloga);
					/* clear errlog */
					sb_corereg(sih, sii->pub.buscoreidx, SBIMERRLOG, ~0, 0);
					SI_ERROR(("sb_taclear: ImErrLog 0x%x, ImErrLogA 0x%x\n",
						imerrlog, imerrloga));
				}
			}
		}


	} else if (BUSTYPE(sii->pub.bustype) == PCMCIA_BUS) {

		INTR_OFF(sii, intr_val);
		origidx = si_coreidx(sih);

		corereg = si_setcore(sih, PCMCIA_CORE_ID, 0);
		if (NULL != corereg) {
			sb = REGS2SB(corereg);

			imstate = R_SBREG(sii, &sb->sbimstate);
			/* handle surprise removal */
			if ((imstate != 0xffffffff) && (imstate & (SBIM_IBE | SBIM_TO))) {
				AND_SBREG(sii, &sb->sbimstate, ~(SBIM_IBE | SBIM_TO));
				inband = imstate & SBIM_IBE;
				timeout = imstate & SBIM_TO;
			}
			tmstate = R_SBREG(sii, &sb->sbtmstatehigh);
			if ((tmstate != 0xffffffff) && (tmstate & SBTMH_INT_STATUS)) {
				if (!inband) {
					serror = 1;
					sb_serr_clear(sii);
				}
				OR_SBREG(sii, &sb->sbtmstatelow, SBTML_INT_ACK);
				AND_SBREG(sii, &sb->sbtmstatelow, ~SBTML_INT_ACK);
			}
		}
		sb_setcoreidx(sih, origidx);
		INTR_RESTORE(sii, intr_val);

	}


	if (inband | timeout | serror) {
		rc = TRUE;
		SI_ERROR(("sb_taclear: inband 0x%x, serror 0x%x, timeout 0x%x!\n",
		          inband, serror, timeout));
	}

	return (rc);
}
#endif 

/* do buffered registers update */
void
sb_commit(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;

	sii = SI_INFO(sih);

	origidx = sii->curidx;
	ASSERT(GOODIDX(origidx));

	INTR_OFF(sii, intr_val);

	/* switch over to chipcommon core if there is one, else use pci */
	if (sii->pub.ccrev != NOREV) {
		chipcregs_t *ccregs = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
		ASSERT(ccregs != NULL);

		/* do the buffer registers update */
		W_REG(sii->osh, &ccregs->broadcastaddress, SB_COMMIT);
		W_REG(sii->osh, &ccregs->broadcastdata, 0x0);
#if !defined(BCMDONGLEHOST)
	} else if (PCI(sii)) {
		sbpciregs_t *pciregs = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, 0);

		/* do the buffer registers update */
		W_REG(sii->osh, &pciregs->bcastaddr, SB_COMMIT);
		W_REG(sii->osh, &pciregs->bcastdata, 0x0);
#endif /* !defined(BCMDONGLEHOST) */
	} else
		ASSERT(0);

	/* restore core index */
	sb_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);
}

void
sb_core_disable(si_t *sih, uint32 bits)
{
	si_info_t *sii;
	volatile uint32 dummy;
	sbconfig_t *sb;

	sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curmap));
	sb = REGS2SB(sii->curmap);

	/* if core is already in reset, just return */
	if (R_SBREG(sii, &sb->sbtmstatelow) & SBTML_RESET)
		return;

	/* if clocks are not enabled, put into reset and return */
	if ((R_SBREG(sii, &sb->sbtmstatelow) & (SICF_CLOCK_EN << SBTML_SICF_SHIFT)) == 0)
		goto disable;

	/* set target reject and spin until busy is clear (preserve core-specific bits) */
	OR_SBREG(sii, &sb->sbtmstatelow, SBTML_REJ);
	dummy = R_SBREG(sii, &sb->sbtmstatelow);
	BCM_REFERENCE(dummy);
	OSL_DELAY(1);
	SPINWAIT((R_SBREG(sii, &sb->sbtmstatehigh) & SBTMH_BUSY), 100000);
	if (R_SBREG(sii, &sb->sbtmstatehigh) & SBTMH_BUSY)
		SI_ERROR(("%s: target state still busy\n", __FUNCTION__));

	if (R_SBREG(sii, &sb->sbidlow) & SBIDL_INIT) {
		OR_SBREG(sii, &sb->sbimstate, SBIM_RJ);
		dummy = R_SBREG(sii, &sb->sbimstate);
		BCM_REFERENCE(dummy);
		OSL_DELAY(1);
		SPINWAIT((R_SBREG(sii, &sb->sbimstate) & SBIM_BY), 100000);
	}

	/* set reset and reject while enabling the clocks */
	W_SBREG(sii, &sb->sbtmstatelow,
	        (((bits | SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT) |
	         SBTML_REJ | SBTML_RESET));
	dummy = R_SBREG(sii, &sb->sbtmstatelow);
	BCM_REFERENCE(dummy);
	OSL_DELAY(10);

	/* don't forget to clear the initiator reject bit */
	if (R_SBREG(sii, &sb->sbidlow) & SBIDL_INIT)
		AND_SBREG(sii, &sb->sbimstate, ~SBIM_RJ);

disable:
	/* leave reset and reject asserted */
	W_SBREG(sii, &sb->sbtmstatelow, ((bits << SBTML_SICF_SHIFT) | SBTML_REJ | SBTML_RESET));
	OSL_DELAY(1);
}

/* reset and re-enable a core
 * inputs:
 * bits - core specific bits that are set during and after reset sequence
 * resetbits - core specific bits that are set only during reset sequence
 */
void
sb_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
	si_info_t *sii;
	sbconfig_t *sb;
	volatile uint32 dummy;

	sii = SI_INFO(sih);
	ASSERT(GOODREGS(sii->curmap));
	sb = REGS2SB(sii->curmap);

	/*
	 * Must do the disable sequence first to work for arbitrary current core state.
	 */
	sb_core_disable(sih, (bits | resetbits));

	/*
	 * Now do the initialization sequence.
	 */

	/* set reset while enabling the clock and forcing them on throughout the core */
	W_SBREG(sii, &sb->sbtmstatelow,
	        (((bits | resetbits | SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT) |
	         SBTML_RESET));
	dummy = R_SBREG(sii, &sb->sbtmstatelow);
	BCM_REFERENCE(dummy);
	OSL_DELAY(1);

	if (R_SBREG(sii, &sb->sbtmstatehigh) & SBTMH_SERR) {
		W_SBREG(sii, &sb->sbtmstatehigh, 0);
	}
	if ((dummy = R_SBREG(sii, &sb->sbimstate)) & (SBIM_IBE | SBIM_TO)) {
		AND_SBREG(sii, &sb->sbimstate, ~(SBIM_IBE | SBIM_TO));
	}

	/* clear reset and allow it to propagate throughout the core */
	W_SBREG(sii, &sb->sbtmstatelow,
	        ((bits | resetbits | SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT));
	dummy = R_SBREG(sii, &sb->sbtmstatelow);
	BCM_REFERENCE(dummy);
	OSL_DELAY(1);

	/* leave clock enabled */
	W_SBREG(sii, &sb->sbtmstatelow, ((bits | SICF_CLOCK_EN) << SBTML_SICF_SHIFT));
	dummy = R_SBREG(sii, &sb->sbtmstatelow);
	BCM_REFERENCE(dummy);
	OSL_DELAY(1);
}

/*
 * Set the initiator timeout for the "master core".
 * The master core is defined to be the core in control
 * of the chip and so it issues accesses to non-memory
 * locations (Because of dma *any* core can access memeory).
 *
 * The routine uses the bus to decide who is the master:
 *	SI_BUS => mips
 *	JTAG_BUS => chipc
 *	PCI_BUS => pci or pcie
 *	PCMCIA_BUS => pcmcia
 *	SDIO_BUS => pcmcia
 *
 * This routine exists so callers can disable initiator
 * timeouts so accesses to very slow devices like otp
 * won't cause an abort. The routine allows arbitrary
 * settings of the service and request timeouts, though.
 *
 * Returns the timeout state before changing it or -1
 * on error.
 */

#define	TO_MASK	(SBIMCL_RTO_MASK | SBIMCL_STO_MASK)

uint32
sb_set_initiator_to(si_t *sih, uint32 to, uint idx)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	uint32 tmp, ret = 0xffffffff;
	sbconfig_t *sb;

	sii = SI_INFO(sih);

	if ((to & ~TO_MASK) != 0)
		return ret;

	/* Figure out the master core */
	if (idx == BADIDX) {
		switch (BUSTYPE(sii->pub.bustype)) {
		case PCI_BUS:
			idx = sii->pub.buscoreidx;
			break;
		case JTAG_BUS:
			idx = SI_CC_IDX;
			break;
		case PCMCIA_BUS:
			idx = si_findcoreidx(sih, PCMCIA_CORE_ID, 0);
			break;
		case SI_BUS:
			idx = si_findcoreidx(sih, MIPS33_CORE_ID, 0);
			break;
		default:
			ASSERT(0);
		}
		if (idx == BADIDX)
			return ret;
	}

	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	sb = REGS2SB(sb_setcoreidx(sih, idx));

	tmp = R_SBREG(sii, &sb->sbimconfiglow);
	ret = tmp & TO_MASK;
	W_SBREG(sii, &sb->sbimconfiglow, (tmp & ~TO_MASK) | to);

	sb_commit(sih);
	sb_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);
	return ret;
}

uint32
sb_base(uint32 admatch)
{
	uint32 base;
	uint type;

	type = admatch & SBAM_TYPE_MASK;
	ASSERT(type < 3);

	base = 0;

	if (type == 0) {
		base = admatch & SBAM_BASE0_MASK;
	} else if (type == 1) {
		ASSERT(!(admatch & SBAM_ADNEG));	/* neg not supported */
		base = admatch & SBAM_BASE1_MASK;
	} else if (type == 2) {
		ASSERT(!(admatch & SBAM_ADNEG));	/* neg not supported */
		base = admatch & SBAM_BASE2_MASK;
	}

	return (base);
}

uint32
sb_size(uint32 admatch)
{
	uint32 size;
	uint type;

	type = admatch & SBAM_TYPE_MASK;
	ASSERT(type < 3);

	size = 0;

	if (type == 0) {
		size = 1 << (((admatch & SBAM_ADINT0_MASK) >> SBAM_ADINT0_SHIFT) + 1);
	} else if (type == 1) {
		ASSERT(!(admatch & SBAM_ADNEG));	/* neg not supported */
		size = 1 << (((admatch & SBAM_ADINT1_MASK) >> SBAM_ADINT1_SHIFT) + 1);
	} else if (type == 2) {
		ASSERT(!(admatch & SBAM_ADNEG));	/* neg not supported */
		size = 1 << (((admatch & SBAM_ADINT2_MASK) >> SBAM_ADINT2_SHIFT) + 1);
	}

	return (size);
}

#if defined(BCMDBG)
/* print interesting sbconfig registers */
void
sb_dumpregs(si_t *sih, struct bcmstrbuf *b)
{
	si_info_t *sii;
	sbconfig_t *sb;
	uint origidx, i, intr_val = 0;

	sii = SI_INFO(sih);
	origidx = sii->curidx;

	INTR_OFF(sii, intr_val);

	for (i = 0; i < sii->numcores; i++) {
		sb = REGS2SB(sb_setcoreidx(sih, i));

		bcm_bprintf(b, "core 0x%x: \n", sii->coreid[i]);

		if (sii->pub.socirev > SONICS_2_2)
			bcm_bprintf(b, "sbimerrlog 0x%x sbimerrloga 0x%x\n",
			          sb_corereg(sih, si_coreidx(&sii->pub), SBIMERRLOG, 0, 0),
			          sb_corereg(sih, si_coreidx(&sii->pub), SBIMERRLOGA, 0, 0));

		bcm_bprintf(b, "sbtmstatelow 0x%x sbtmstatehigh 0x%x sbidhigh 0x%x "
		            "sbimstate 0x%x\n sbimconfiglow 0x%x sbimconfighigh 0x%x\n",
		            R_SBREG(sii, &sb->sbtmstatelow), R_SBREG(sii, &sb->sbtmstatehigh),
		            R_SBREG(sii, &sb->sbidhigh), R_SBREG(sii, &sb->sbimstate),
		            R_SBREG(sii, &sb->sbimconfiglow), R_SBREG(sii, &sb->sbimconfighigh));
	}

	sb_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);
}
#endif	

#if defined(BCMDBG)
void
sb_view(si_t *sih, bool verbose)
{
	si_info_t *sii;
	sbconfig_t *sb;

	sii = SI_INFO(sih);
	sb = REGS2SB(sii->curmap);

	SI_ERROR(("\nCore ID: 0x%x\n", sb_coreid(&sii->pub)));

	if (sii->pub.socirev > SONICS_2_2)
		SI_ERROR(("sbimerrlog 0x%x sbimerrloga 0x%x\n",
		         sb_corereg(sih, si_coreidx(&sii->pub), SBIMERRLOG, 0, 0),
		         sb_corereg(sih, si_coreidx(&sii->pub), SBIMERRLOGA, 0, 0)));

	/* Print important or helpful registers */
	SI_ERROR(("sbtmerrloga 0x%x sbtmerrlog 0x%x\n",
	          R_SBREG(sii, &sb->sbtmerrloga), R_SBREG(sii, &sb->sbtmerrlog)));
	SI_ERROR(("sbimstate 0x%x sbtmstatelow 0x%x sbtmstatehigh 0x%x\n",
	          R_SBREG(sii, &sb->sbimstate),
	          R_SBREG(sii, &sb->sbtmstatelow), R_SBREG(sii, &sb->sbtmstatehigh)));
	SI_ERROR(("sbimconfiglow 0x%x sbtmconfiglow 0x%x\nsbtmconfighigh 0x%x sbidhigh 0x%x\n",
	          R_SBREG(sii, &sb->sbimconfiglow), R_SBREG(sii, &sb->sbtmconfiglow),
	          R_SBREG(sii, &sb->sbtmconfighigh), R_SBREG(sii, &sb->sbidhigh)));

	/* Print more detailed registers that are otherwise not relevant */
	if (verbose) {
		SI_ERROR(("sbipsflag 0x%x sbtpsflag 0x%x\n",
		          R_SBREG(sii, &sb->sbipsflag), R_SBREG(sii, &sb->sbtpsflag)));
		SI_ERROR(("sbadmatch3 0x%x sbadmatch2 0x%x\nsbadmatch1 0x%x sbadmatch0 0x%x\n",
		          R_SBREG(sii, &sb->sbadmatch3), R_SBREG(sii, &sb->sbadmatch2),
		          R_SBREG(sii, &sb->sbadmatch1), R_SBREG(sii, &sb->sbadmatch0)));
		SI_ERROR(("sbintvec 0x%x sbbwa0 0x%x sbimconfighigh 0x%x\n",
		          R_SBREG(sii, &sb->sbintvec), R_SBREG(sii, &sb->sbbwa0),
		          R_SBREG(sii, &sb->sbimconfighigh)));
		SI_ERROR(("sbbconfig 0x%x sbbstate 0x%x\n",
		          R_SBREG(sii, &sb->sbbconfig), R_SBREG(sii, &sb->sbbstate)));
		SI_ERROR(("sbactcnfg 0x%x sbflagst 0x%x sbidlow 0x%x \n\n",
		          R_SBREG(sii, &sb->sbactcnfg), R_SBREG(sii, &sb->sbflagst),
		          R_SBREG(sii, &sb->sbidlow)));
	}
}
#endif	/* BCMDBG */
