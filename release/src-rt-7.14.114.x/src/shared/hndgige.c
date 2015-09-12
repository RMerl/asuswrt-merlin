/*
 * HND SiliconBackplane Gigabit Ethernet core software interface
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
 * $Id: hndgige.c 310902 2012-01-26 19:45:33Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbgige.h>
#include <hndpci.h>
#include <hndgige.h>

/*
 * Setup the gige core.
 * Resetting the core will lose all settings.
 */
void
hndgige_init(si_t *sih, uint32 unit, bool *rgmii)
{
	volatile pci_config_regs *pci;
	sbgige_pcishim_t *ocp;
	sbconfig_t *sb;
	osl_t *osh;
	uint32 statelow;
	uint32 statehigh;
	uint32 base;
	uint32 idx;
	void *regs;

	/* Sanity checks */
	ASSERT(sih);
	ASSERT(rgmii);

	idx = si_coreidx(sih);

	/* point to the gige core registers */
	regs = si_setcore(sih, GIGETH_CORE_ID, unit);
	ASSERT(regs);

	osh = si_osh(sih);

	pci = &((sbgige_t *)regs)->pcicfg;
	ocp = &((sbgige_t *)regs)->pcishim;
	sb = &((sbgige_t *)regs)->sbconfig;

	/* Enable the core clock and memory access */
	if (!si_iscoreup(sih))
		si_core_reset(sih, 0, 0);

	/*
	 * Setup the 64K memory-mapped region base address through BAR0.
	 * Leave the other BAR values alone.
	 */
	base = si_addrspace(sih, 1);
	W_REG(osh, &pci->base[0], base);
	W_REG(osh, &pci->base[1], 0);

	/*
	 * Enable the PCI memory access anyway. Any PCI config commands
	 * issued before the core is enabled will go to the emulation
	 * only and will not go to the real PCI config registers.
	 */
	OR_REG(osh, &pci->command, 2);

	/*
	 * Enable the posted write flush scheme as follows:
	 *
	 * - Enable flush on any core register read
	 * - Enable timeout on the flush
	 * - Disable the interrupt mask when flushing
	 *
	 * This differs from the default setting only in that interrupts are
	 * not masked.  Since posted writes are not flushed on interrupt, the
	 * driver must explicitly request a flush in its interrupt handling
	 * by reading a core register.
	 */
	W_REG(osh, &ocp->FlushStatusControl, 0x68);

	/*
	 * Determine whether the GbE is in GMII or RGMII mode.  This is
	 * indicated in bit 16 of the SBTMStateHigh register, which is
	 * part of the core-specific flags field.
	 *
	 * For GMII, bypass the Rx/Tx DLLs, i.e. add no delay to RXC/GTXC
	 * within the core.  For RGMII, do not bypass the DLLs, resulting
	 * in added delay for RXC/GTXC.  The SBTMStateLow register contains
	 * the controls for doing this in the core-specific flags field:
	 *
	 *   bit 24 - Enable DLL controls
	 *   bit 20 - Bypass Rx DLL
	 *   bit 19 - Bypass Tx DLL
	 */
	statelow = R_REG(osh, &sb->sbtmstatelow);	/* DLL controls */
	statehigh = R_REG(osh, &sb->sbtmstatehigh);	/* GMII/RGMII mode */
	if ((statehigh & (1 << 16)) != 0)	/* RGMII */
	{
		statelow &= ~(1 << 20);		/* no Rx bypass (delay) */
		statelow &= ~(1 << 19);		/* no Tx bypass (delay) */
		*rgmii = TRUE;
	}
	else					/* GMII */
	{
		statelow |= (1 << 20);		/* Rx bypass (no delay) */
		statelow |= (1 << 19);		/* Tx bypass (no delay) */
		*rgmii = FALSE;
	}
	statelow |= (1 << 24);			/* enable DLL controls */
	W_REG(osh, &sb->sbtmstatelow, statelow);

	si_setcoreidx(sih, idx);
}
