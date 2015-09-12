/*
 * BCM47XX Sonics SiliconBackplane MIPS core routines
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
 * $Id: hndmips.c 404499 2013-05-28 01:06:37Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <sbmemc.h>
#include <mipsinc.h>
#include <mips33_core.h>
#include <mips74k_core.h>
#include <hndcpu.h>
#include <hndmips.h>
#include <hndpmu.h>
#include <dmemc_core.h>

/* debug/trace */
#ifdef BCMDBG_ERR
#define	HNDMIPS_ERROR(args)	printf args
#else
#define	HNDMIPS_ERROR(args)
#endif	/* BCMDBG_ERR */

#ifdef BCMDBG
#define	HNDMIPS_MSG(args)	printf args
#else
#define	HNDMIPS_MSG(args)
#endif	/* BCMDBG */
#define	HNDMIPS_NONE(args)

/* sbipsflag register format, indexed by irq. */
static const uint32 sbips_int_mask[] = {
	0,	/* placeholder */
	SBIPS_INT1_MASK,
	SBIPS_INT2_MASK,
	SBIPS_INT3_MASK,
	SBIPS_INT4_MASK
};

static const uint32 sbips_int_shift[] = {
	0,	/* placeholder */
	SBIPS_INT1_SHIFT,
	SBIPS_INT2_SHIFT,
	SBIPS_INT3_SHIFT,
	SBIPS_INT4_SHIFT
};

/*
 * Map SB cores sharing the MIPS hardware IRQ0 to virtual dedicated OS IRQs.
 * Per-port BSP code is required to provide necessary translations between
 * the shared MIPS IRQ and the virtual OS IRQs based on SB core flag.
 *
 * See si_irq() for the mapping.
 */
static uint shirq_map_base = 0;

/*
 * Returns the MIPS IRQ assignment of the current core. If unassigned,
 * 0 is returned.
 */
static uint
si_getirq(si_t *sih)
{
	osl_t *osh;
	uint idx;
	void *regs;
	sbconfig_t *sb;
	uint32 flag, sbipsflag;
	uint irq = 0;

	osh = si_osh(sih);
	flag = si_flag(sih);

	idx = si_coreidx(sih);

	if ((regs = si_setcore(sih, MIPS74K_CORE_ID, 0)) != NULL) {
		/* IntMask1,2,3,4 regs are configured to enable propagation of
		 * backplane interrupts 0,1,2,3 to mips hw interrupts 1,2,3,4.
		 */
		for (irq = 1; irq <= 4; irq++) {
			if (R_REG(osh, &((mips74kregs_t *)regs)->intmask[irq]) &
			          (1 << flag))
				break;
		}
	} else if ((regs = si_setcore(sih, MIPS33_CORE_ID, 0)) != NULL) {
		sb = (sbconfig_t *)((ulong) regs + SBCONFIGOFF);

		/* sbipsflag specifies which core is routed to interrupts 1 to 4 */
		sbipsflag = R_REG(osh, &sb->sbipsflag);
		for (irq = 1; irq <= 4; irq++) {
			if (((sbipsflag & sbips_int_mask[irq]) >>
			     sbips_int_shift[irq]) == flag)
				break;
		}
	} else {
		ASSERT("Unknown processor core" == NULL);
		return 1000;	/* An invalid value */
	}

	/* If we didn't find it it must be in the shared int (0) */
	if (irq == 5)
		irq = 0;

	si_setcoreidx(sih, idx);

	return irq;
}

/*
 * Return the MIPS IRQ assignment of the current core. If necessary
 * map cores sharing the MIPS hw IRQ0 to virtual dedicated OS IRQs.
 */
uint
si_irq(si_t *sih)
{
	uint irq = si_getirq(sih);
	if (irq == 0 && shirq_map_base)
		irq = si_flag(sih) + shirq_map_base;
	return irq;
}

/* Clears the specified MIPS IRQ. */
static void
BCMINITFN(si_clearirq)(si_t *sih, uint irq)
{
	osl_t *osh;
	void *regs;
	sbconfig_t *sb;

	osh = si_osh(sih);

	if ((regs = si_setcore(sih, MIPS74K_CORE_ID, 0)) != NULL) {
		W_REG(osh, &((mips74kregs_t *)regs)->intmask[irq], 0);
	} else if ((regs = si_setcore(sih, MIPS33_CORE_ID, 0)) != NULL) {
		sb = (sbconfig_t *)((ulong) regs + SBCONFIGOFF);
		if (irq == 0)
			W_REG(osh, &sb->sbintvec, 0);
		else
			OR_REG(osh, &sb->sbipsflag, sbips_int_mask[irq]);
	} else
		ASSERT("Unknown processor core" == NULL);
}

/*
 * Assigns the specified MIPS IRQ to the specified core. Shared MIPS
 * IRQ 0 may be assigned more than once.
 *
 * The old assignment to the specified core is removed first.
 */
static void
BCMATTACHFN(si_setirq)(si_t *sih, uint irq, uint coreid, uint coreunit)
{
	osl_t *osh;
	void *regs;
	sbconfig_t *sb;
	uint32 flag;
	uint oldirq;

	osh = si_osh(sih);

	regs = si_setcore(sih, coreid, coreunit);
	ASSERT(regs);
	flag = si_flag(sih);
	oldirq = si_getirq(sih);
	if (oldirq)
		si_clearirq(sih, oldirq);

	if ((regs = si_setcore(sih, MIPS74K_CORE_ID, 0)) != NULL) {
		if (!oldirq)
			AND_REG(osh, &((mips74kregs_t *)regs)->intmask[0], ~(1 << flag));

		if (irq == 0)
			OR_REG(osh, &((mips74kregs_t *)regs)->intmask[0], 1 << flag);
		else {
			W_REG(osh, &((mips74kregs_t *)regs)->intmask[irq], 1 << flag);
		}
	} else if ((regs = si_setcore(sih, MIPS33_CORE_ID, 0)) != NULL) {
		sb = (sbconfig_t *)((ulong) regs + SBCONFIGOFF);

		if (!oldirq)
			AND_REG(osh, &sb->sbintvec, ~(1 << flag));

		if (irq == 0)
			OR_REG(osh, &sb->sbintvec, 1 << flag);
		else {
			flag <<= sbips_int_shift[irq];
			ASSERT(!(flag & ~sbips_int_mask[irq]));
			flag |= R_REG(osh, &sb->sbipsflag) & ~sbips_int_mask[irq];
			W_REG(osh, &sb->sbipsflag, flag);
		}
	} else
		ASSERT("Unknown processor core" == NULL);
}

/*
 * Initializes clocks and interrupts. SB and NVRAM access must be
 * initialized prior to calling.
 *
 * 'shirqmap' enables virtual dedicated OS IRQ mapping if non-zero.
 */
void
BCMATTACHFN(si_mips_init)(si_t *sih, uint shirqmap)
{
	osl_t *osh;
	uint32 c0reg;
	ulong hz, ns, tmp;
	chipcregs_t *cc;
	uint irq;

	osh = si_osh(sih);

	/* Disable interrupts */
	c0reg = MFC0(C0_STATUS, 0);
	c0reg &= ~ST0_IE;
	MTC0(C0_STATUS, 0, c0reg);

	/* Figure out current SB clock speed */
	if ((hz = si_clock(sih)) == 0)
		hz = 100000000;
	ns = 1000000000 / hz;

	/* Setup external interface timing */
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	/* Set timing for the flash */
	tmp = CEIL(10, ns) << FW_W3_SHIFT;	/* W3 = 10nS */
	tmp |= CEIL(10, ns) << FW_W1_SHIFT;	/* W1 = 10nS */
	tmp |= CEIL(120, ns);			/* W0 = 120nS */
	if (sih->ccrev < 9)
		W_REG(osh, &cc->flash_waitcount, tmp);

	if ((sih->ccrev < 9) ||
	    ((CHIPID(sih->chip) == BCM5350_CHIP_ID) && CHIPREV(sih->chiprev) == 0)) {
		W_REG(osh, &cc->pcmcia_memwait, tmp);
	}

	/* Save shared IRQ mapping base */
	shirq_map_base = shirqmap;

	/* Chip specific initialization */
	switch (CHIPID(sih->chip)) {
	case BCM5350_CHIP_ID:
		/* Clear interrupt map */
		for (irq = 0; irq <= 4; irq++)
			si_clearirq(sih, irq);
		si_setirq(sih, 0, CC_CORE_ID, 0);
		si_setirq(sih, 0, MIPS33_CORE_ID, 0);
		si_setirq(sih, 1, D11_CORE_ID, 0);
		si_setirq(sih, 2, ENET_CORE_ID, 0);
		si_setirq(sih, 3, PCI_CORE_ID, 0);
		si_setirq(sih, 4, USB_CORE_ID, 0);
		break;
	case BCM4785_CHIP_ID:
		/* Reassign PCI to irq 4 */
		si_setirq(sih, 4, PCI_CORE_ID, 0);
		break;
	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
		/* Clear interrupt map */
		for (irq = 0; irq <= 4; irq++)
			si_clearirq(sih, irq);
		si_setirq(sih, 1, D11_CORE_ID, 0);
		si_setirq(sih, 2, GMAC_CORE_ID, 0);
		si_setirq(sih, 3, USB20H_CORE_ID, 0);
		si_setirq(sih, 4, PCIE_CORE_ID, 0);
		si_setirq(sih, 0, CC_CORE_ID, 0);
		si_setirq(sih, 0, I2S_CORE_ID, 0);
		break;
	case BCM5356_CHIP_ID:
	case BCM47162_CHIP_ID:
	case BCM53572_CHIP_ID:
		/* Clear interrupt map */
		for (irq = 0; irq <= 4; irq++)
			si_clearirq(sih, irq);
		si_setirq(sih, 1, D11_CORE_ID, 0);
		si_setirq(sih, 2, GMAC_CORE_ID, 0);
		si_setirq(sih, 0, CC_CORE_ID, 0);
		break;
	case BCM5357_CHIP_ID:
	case BCM4749_CHIP_ID:
		/* Clear interrupt map */
		for (irq = 0; irq <= 4; irq++)
			si_clearirq(sih, irq);
		si_setirq(sih, 1, D11_CORE_ID, 0);
		si_setirq(sih, 2, GMAC_CORE_ID, 0);
		si_setirq(sih, 3, USB20H_CORE_ID, 0);
		si_setirq(sih, 0, CC_CORE_ID, 0);
		si_setirq(sih, 0, I2S_CORE_ID, 0);
		break;
	case BCM4706_CHIP_ID:
		/* Clear interrupt map */
		for (irq = 0; irq <= 4; irq++)
			si_clearirq(sih, irq);
		si_setirq(sih, 1, PCIE_CORE_ID, 0);
		si_setirq(sih, 2, GMAC_CORE_ID, 0);
		si_setirq(sih, 3, PCIE_CORE_ID, 1);
		si_setirq(sih, 4, USB20H_CORE_ID, 0);
		si_setirq(sih, 0, CC_CORE_ID, 0);

		break;
	}
}

/* Do any setup necessary to run a new image and jump to it. */
void
hnd_cpu_jumpto(void *addr)
{
	void (*jumpto)(void) = addr;

	(jumpto)();
}

uint32
BCMINITFN(si_cpu_clock)(si_t *sih)
{
	osl_t *osh;
	chipcregs_t *cc;
	uint32 n, m;
	uint idx;
	uint32 pll_type, rate = 0;


	osh = si_osh(sih);

	if (sih->cccaps & CC_CAP_PMU)
		return si_pmu_cpu_clock(sih, osh);

	/* get index of the current core */
	idx = si_coreidx(sih);

	/* switch to chipc core */
	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	pll_type = sih->cccaps & CC_CAP_PLL_MASK;
	n = R_REG(osh, &cc->clockcontrol_n);
	if ((pll_type == PLL_TYPE2) ||
	    (pll_type == PLL_TYPE4) ||
	    (pll_type == PLL_TYPE6) ||
	    (pll_type == PLL_TYPE7))
		m = R_REG(osh, &cc->clockcontrol_m3);
	else if (pll_type == PLL_TYPE5) {
		rate = 200000000;
		goto out;
	} else if (pll_type == PLL_TYPE3) {
		if (CHIPID(sih->chip) == BCM5365_CHIP_ID) {
			rate = 200000000;
			goto out;
		}
		/* 5350 uses m2 to control mips */
		else
			m = R_REG(osh, &cc->clockcontrol_m2);
	} else
		m = R_REG(osh, &cc->clockcontrol_sb);

	/* calculate rate */
	rate = si_clock_rate(pll_type, n, m);

	if (pll_type == PLL_TYPE6)
		rate = SB2MIPS_T6(rate);

out:
	/* switch back to previous core */
	si_setcoreidx(sih, idx);

	return rate;
}

uint32
BCMINITFN(si_mem_clock)(si_t *sih)
{
	osl_t *osh;

	osh = si_osh(sih);

	if (sih->cccaps & CC_CAP_PMU)
		return si_pmu_mem_clock(sih, osh);

	return si_clock(sih);
}

#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4)

static void __attribute__ ((__noinline__))
ephy_poll_phyaccess(void)
{
	asm("phypoll: \tlui  $8, 0xb800\n\t"
		"lw   $9, 0x2180($8)\n\t"
		"lui  $8, 0x4000\n\t"
		"and $9, $9, $8\n\t"
		"bnez $9, phypoll\n\t"
		"nop");
}

static void __attribute__ ((__noinline__))
coma_delay(void)
{
	/* for (i = 0; i < 3000000; i++); */
	asm("lui  $8, 0x2d\n\t"
		"ori     $8,$8,0xc6c0\n\t"
		"coma_delay_loop: \taddiu   $8,$8,-1\n\t"
		"bnez    $8,coma_delay_loop\n\t"
		"nop");
}

static void __attribute__ ((__noinline__))
do_router_coma(si_t *sih, void *dmem, int delay)
{
	uint8 phy;

	/* set jtag user reg 0 = 0x80 to set DDR pad power saving mode */
	asm("lui  $8, 0xb800");
	asm("li   $9, 0xff03ff3a");   /* (16 + addr) << 20 | 0xfe03ff3a */
	asm("sw   $9, 0x0034($8)");
	asm("li   $9, 0x80");      /* data */
	asm("sw   $9, 0x0038($8)");
	asm("li   $9, 0x80071f1f");
	asm("sw   $9, 0x0030($8)");
	asm("sync");

	coma_delay();

	/* ephy ports powerdown */

	/* robo_wreg 0x0 0xf 0x1f 0x2 */
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x0090001e");
	asm("sw   $9, 0x2188($8)");
	asm("sync");
	asm("nop");
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x701e0001");
	asm("sw   $9, 0x2180($8)");
	asm("sync");
	asm("nop");

	ephy_poll_phyaccess();

	asm("lui  $8, 0xb800");
	asm("li   $9, 0x0090001e");
	asm("sw   $9, 0x2188($8)");
	asm("sync");
	asm("nop");
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x781e001f");
	asm("sw   $9, 0x2180($8)");
	asm("sync");
	asm("nop");

	ephy_poll_phyaccess();

	asm("lui  $8, 0xb800");
	asm("li   $9, 0x0090001e");
	asm("sw   $9, 0x2188($8)");
	asm("sync");
	asm("nop");
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x711e0f01");
	asm("sw   $9, 0x2180($8)");
	asm("sync");
	asm("nop");

	ephy_poll_phyaccess();

	asm("lui  $8, 0xb800");
	asm("li   $9, 0x0090001e");
	asm("sw   $9, 0x2188($8)");
	asm("sync");
	asm("nop");
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x511e0000");
	asm("sw   $9, 0x2180($8)");
	asm("sync");
	asm("nop");

	ephy_poll_phyaccess();

	/* ports 0-5 writes start */

	asm("li  $10, 0");

	for (phy = 0; phy < 5; phy++) {

		asm("sll  $11, $10, 16");

		asm("li   $9, 0x00900000");
		asm("or   $9, $9, $10");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2188($8)");
		asm("sync");
		asm("nop");
		asm("li   $9, 0x7f00008b");
		asm("or   $9, $9, $11");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2180($8)");
		asm("sync");
		asm("nop");

		ephy_poll_phyaccess();

		asm("li   $9, 0x00900000");
		asm("or   $9, $9, $10");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2188($8)");
		asm("sync");
		asm("nop");
		asm("li   $9, 0x74006000");
		asm("or   $9, $9, $11");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2180($8)");
		asm("sync");
		asm("nop");

		ephy_poll_phyaccess();

		asm("li   $9, 0x00900000");
		asm("or   $9, $9, $10");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2188($8)");
		asm("sync");
		asm("nop");
		asm("li   $9, 0x70000700");
		asm("or   $9, $9, $11");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2180($8)");
		asm("sync");
		asm("nop");

		ephy_poll_phyaccess();

		asm("li   $9, 0x00900000");
		asm("or   $9, $9, $10");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2188($8)");
		asm("sync");
		asm("nop");
		asm("li   $9, 0x71001000");
		asm("or   $9, $9, $11");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2180($8)");
		asm("sync");
		asm("nop");

		ephy_poll_phyaccess();

		asm("li   $9, 0x00900000");
		asm("or   $9, $9, $10");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2188($8)");
		asm("sync");
		asm("nop");
		asm("li   $9, 0x7f00000b");
		asm("or   $9, $9, $11");
		asm("lui  $8, 0xb800");
		asm("sw   $9, 0x2180($8)");
		asm("sync");
		asm("nop");

		ephy_poll_phyaccess();

		asm("addi $10, $10, 1");
	}

	coma_delay();
	/* ports 0-5 writes end */

	if (((CHIPID(sih->chip)) == BCM53572_CHIP_ID)) {
		/* set ephy pll and bias power save through chipc registers */
		asm("lui  $8, 0xb800");
		asm("li   $9, 0x4");
		asm("sw   $9, 0x0650($8)");
		asm("li   $9, 0x8a60e001");
		asm("sw   $9, 0x0654($8)");
		asm("sync");
		asm("nop");

		coma_delay();

		asm("lui  $8, 0xb800");
		asm("li   $9, 0x2");
		asm("sw   $9, 0x0650($8)");
		asm("li   $9, 0xcad0000f");
		asm("sw   $9, 0x0654($8)");
		asm("sync");
		asm("nop");

		coma_delay();

		/* Clear the dmems ddrctrl reg */
		asm("lui  $8, 0xb800");
		asm("li   $9, 0x0");
		asm("sw   $9, 0x41e4($8)");
		asm("sync");
		asm("nop");

		coma_delay();
	}
	else {
		/* A0 vs B0 steps */
		if (sih->chiprev == 0) {

			/* set jtag user reg 3 = 0x60000 to turn off ephy pll and bias power */
			asm("lui  $8, 0xb800");
			asm("li   $9, 0xff33ff3a");   /* (16 + addr) << 20 | 0xfe03ff3a */
			asm("sw   $9, 0x0034($8)");
			asm("li   $9, 0x60000");      /* data */
			asm("sw   $9, 0x0038($8)");
			asm("li   $9, 0x80071f1f");
			asm("sw   $9, 0x0030($8)");
			asm("sync");

			coma_delay();
		} else {

			/* set ephy pll and bias power power save through chipc registers */
			asm("lui  $8, 0xb800");
			asm("li   $9, 0x4");
			asm("sw   $9, 0x0650($8)");
			asm("li   $9, 0x8a60e001");
			asm("sw   $9, 0x0654($8)");
			asm("sync");
			asm("nop");

			coma_delay();

			asm("lui  $8, 0xb800");
			asm("li   $9, 0x2");
			asm("sw   $9, 0x0650($8)");
			asm("li   $9, 0xcad0000f");
			asm("sw   $9, 0x0654($8)");
			asm("sync");
			asm("nop");

			coma_delay();
		}
	}

	/* set jtag user reg 7 = 0xc0 to turn off the pll and bias power of ephy */
	asm("lui  $8, 0xb800");
	asm("li   $9, 0xff73ff3a");
	asm("sw   $9, 0x0034($8)");
	asm("li   $9, 0xc0");      /* data */
	asm("sw   $9, 0x0038($8)");
	asm("li   $9, 0x80071f1f");
	asm("sw   $9, 0x0030($8)");
	asm("sync");

	coma_delay();

	/* set gmac dmp io control = 0 */
	asm("lui  $8, 0xb810");
	asm("li   $9, 0x0");
	asm("sw   $9, 0x2408($8)");
	asm("sync");
	asm("nop");

	coma_delay();

	if (((CHIPID(sih->chip)) == BCM53572_CHIP_ID)) {
		/* set ddr dmp io control = 0 */
		asm("lui  $8, 0xb810");
		asm("li   $9, 0x0");
		asm("sw   $9, 0x4408($8)");
		asm("sync");
		asm("nop");
		/* put dmems in reset */
		asm("li   $9, 0x1");
		asm("sw   $9, 0x4800($8)");
		asm("sync");
		asm("nop");
	} else {
		/* set ddr dmp io control = 0 */
		asm("lui  $8, 0xb810");
		asm("li   $9, 0x0");
		asm("sw   $9, 0x5408($8)");
		asm("sync");
		asm("nop");
		/* put dmemc in reset */
		asm("li   $9, 0x1");
		asm("sw   $9, 0x5800($8)");
		asm("sync");
		asm("nop");
	}

	coma_delay();

	/* set PMU control = 1 */
	asm("lui  $8, 0xb800");
	asm("li   $9, 0x1");
	asm("sw   $9, 0x0600($8)");
	asm("sync");
	asm("nop");

	coma_delay();

	if (((CHIPID(sih->chip)) != BCM53572_CHIP_ID)) {
		/* Set switching freq of internal 12V regulator to 600kHz */
		asm("lui  $8, 0xb800");
		asm("li   $9, 0x1");
		asm("sw   $9, 0x0658($8)");
		asm("sync");
		asm("nop");
		asm("lui  $8, 0xb800");
		asm("li   $9, 0x00018000");
		asm("sw   $9, 0x065c($8)");
		asm("sync");
		asm("nop");

		coma_delay();
	}

	/* set mips dmp io control = 0 */
	asm("lui  $8, 0xb810");
	asm("li   $9, 0x0");
	asm("sw   $9, 0x3408($8)");
	asm("sync");
	asm("nop");

	/* wait for watch dog timer done */
	__asm__(
		".set\tmips3\n\t"
		"sync\n\t"
		"wait\n\t"
		".set\tmips0");

	asm("nop");
	asm("nop");
}

static void __attribute__ ((__noinline__))
BCMINITFN(aftercoma)(void)
{

}

void
si_router_coma(si_t *sih, int reset, int delay)
{
	void *dmem = NULL;
	chipcregs_t *cc;
	uint ic_size, ic_lsize;
	ulong start, end;
	uint32 c0reg;
	uint32 tmp;
	int i;
	osl_t *osh;


	osh = si_osh(sih);
	/* Disable interrupts */

	c0reg = MFC0(C0_STATUS, 0);
	tmp = (c0reg & ~(ALLINTS | ST0_IE));
	MTC0(C0_STATUS, 0, tmp);

	icache_probe(MFC0(C0_CONFIG, 1), &ic_size, &ic_lsize);

	/* Put coma routine into the icache */
	start = (ulong)&ephy_poll_phyaccess;
	end = (ulong)&aftercoma;
	for (i = 0; i < (end - start); i += ic_lsize)
		cache_op(start + i, Fill_I);

	/* Prepare JTAG registers */
	si_setcore(sih, CC_CORE_ID, 0);
	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);

	W_REG(osh, &cc->jtagctrl, 0x01);
	W_REG(osh, &cc->jtagcmd, 0x80030000);
	W_REG(osh, &cc->gpioouten, 0x0);

	/* disable gpios */
	W_REG(osh, &cc->gpioouten, 0x0);
	W_REG(osh, &cc->chipcontrol_addr, 0x2);
	W_REG(osh, &cc->chipcontrol_data, 0x04000600);

	/* Set the watchdog */
	if (((CHIPID(sih->chip)) == BCM5357_CHIP_ID) && (sih->chiprev == 0)) {
		W_REG(osh, &cc->watchdog, reset*ILP_CLOCK);
	} else {
		si_watchdog_ms(sih, reset*1000);
	}

	if (((CHIPID(sih->chip)) == BCM53572_CHIP_ID))
		dmem = (void *)si_setcore(sih, DMEMS_CORE_ID, 0);
	else
		dmem = (void *)si_setcore(sih, DMEMC_CORE_ID, 0);

	do_router_coma(sih, dmem, delay);
}

#define PLL_ENTRIES_4706	1
static bool
BCMINITFN(mips_pmu_setclock_4706)(si_t *sih, uint32 mipsclock,
	uint32 ddrclock, uint32 axiclock)
{
	chipcregs_t *cc = NULL;
	osl_t *osh;
	uint idx, i;
	bool ret = TRUE, boolChanged = FALSE;
	/* 25MHz table for 4706 */
	static uint32 BCMINITDATA(pll25mhz_table)[][3 + PLL_ENTRIES_4706] = {
	/*	cpu, ddr, axi, proc_PLL,    */
		{ 200, 100,  50, 0xc0011080, },
		{ 300, 150,  75, 0xc00110c0, },
		{ 400, 200, 100, 0xc0011100, },
		{ 500, 250, 125, 0xc0011140, },
		{ 600, 300, 150, 0xc0011180, },
		{ 632, 316, 158, 0xc00157e8, },
		{ 650, 325, 162, 0xc00111a0, },
		{ 662, 331, 165, 0xc00111a8, },
		{0}
	};
	static uint32 BCMINITDATA(pll25mhz_table_4706L)[][3 + PLL_ENTRIES_4706] = {
	/*	cpu, ddr, axi, proc_PLL,    */
		{ 200, 100,  50, 0xc0011100, },
		{ 300, 150,  75, 0xc0011180, },
		{ 400, 200, 100, 0xc0011200, },
		{0}
	};
	uint32 (*pll_table)[4] = pll25mhz_table;

	/* Since we already in 4706 specific routine, we no longer check the chip id
	 * The 4706L PLL m value is default to 4, thus the n value needs to be doubled
	 * And since it supports the max frequency is 400MHz, we replace the pll table
	 * here
	 */
	if (sih->chippkg == BCM4706L_PKG_ID) {
		pll_table = pll25mhz_table_4706L;
	}

	osh = si_osh(sih);

	/* get index of the current core */
	idx = si_coreidx(sih);

	/* switch to chipc core */
	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	mipsclock /= 1000000;
	ddrclock /= 1000000;
	axiclock /= 1000000;

	for (idx = 0; pll_table[idx][0] != 0; idx++) {
		if ((mipsclock <= pll_table[idx][0]) &&
		    ((ddrclock == 0) || (ddrclock <= pll_table[idx][1])) &&
		    ((axiclock == 0) || (axiclock <= pll_table[idx][2])))
			break;
	}

	if (pll_table[idx][0] == 0) {
		ret = FALSE;
		goto done;
	}

	for (i = 0; i < PLL_ENTRIES_4706; i++) {
		W_REG(osh, &cc->pllcontrol_addr, PMU6_4706_PROCPLL_OFF + i);
		(void)R_REG(osh, &cc->pllcontrol_addr);
		if (R_REG(osh, &cc->pllcontrol_data) != pll_table[idx][i + 3]) {
			W_REG(osh, &cc->pllcontrol_data, pll_table[idx][i + 3]);
			boolChanged = TRUE;
		}
	}

	if (boolChanged == FALSE)
		goto done;

	/* Wait for the last write */
	(void)R_REG(osh, &cc->pllcontrol_data);

	/* And now do the pll update */
	W_REG(osh, &cc->pmucontrol,
	      R_REG(osh, &cc->pmucontrol) | PCTL_PLL_PLLCTL_UPD);

	__asm__ __volatile__(
		"nop\n"
		"nop\n"
		"nop\n"
		"nop");

done:
	si_setcoreidx(sih, idx);
	return ret;
}

/*
 * Set the MIPS, backplane and DDR clocks as closely as possible in chips
 * with a PMU. So far that means 4716, 47162, 5357, and 5356 all of which share
 * the same PLL controls.
 */
static bool
BCMINITFN(mips_pmu_setclock)(si_t *sih, uint32 mipsclock, uint32 ddrclock, uint32 axiclock)
{
	osl_t *osh;
	chipcregs_t *cc = NULL;
	uint idx, i;
	uint mainpll_pll0 = PMU4716_MAINPLL_PLL0;
	bool ret = TRUE;
	uint32 (*pll_table)[8];

	/* 20MHz table for 4716, 4717, 4718, 47162, 5357 */
	static uint32 BCMINITDATA(pll20mhz_table)[][8] = {
		/* cpu, ddr, axi, pllctl12,  pllctl13,   pllctl14,   pllctl15,	 pllctl16 */
		{  66,  66,  66, 0x11100070, 0x00121212, 0x03c00000, 0x20000000, 0x200005c0 },
		{  75,  75,  75, 0x11100070, 0x00101010, 0x03c00000, 0x20000000, 0x200005c0 },
		{  80,  80,  80, 0x11100070, 0x000a0a0a, 0x02800000, 0x20000000, 0x200005c0 },
		{  83,  83,  83, 0x11100070, 0x000c0c0c, 0x03200000, 0x20000000, 0x200005c0 },
		{ 100,  66,  66, 0x11100070, 0x0012120c, 0x03c00000, 0x30000000, 0x200005c0 },
		{ 100, 100, 100, 0x11100070, 0x000c0c0c, 0x03c00000, 0x20000000, 0x200005c0 },
		{ 120,  60,  60, 0x11100070, 0x00101008, 0x03000000, 0x40000000, 0x200005c0 },
		{ 120, 120, 120, 0x11100070, 0x00080808, 0x03000000, 0x20000000, 0x200005c0 },
		{ 125,  83,  83, 0x11100070, 0x000c0c08, 0x03200000, 0x30000000, 0x200005c0 },
		{ 133,  66,  66, 0x11100070, 0x0018180c, 0x05000000, 0x40000000, 0x200005c0 },
		{ 133, 133, 133, 0x11100070, 0x000c0c0c, 0x05000000, 0x20000000, 0x200005c0 },
		{ 148, 148,  74, 0x11100070, 0x00120909, 0x04300000, 0x28000000, 0x200005c0 },
		{ 150,  75,  75, 0x11100070, 0x00101008, 0x03c00000, 0x40000000, 0x200005c0 },
		{ 150, 100, 100, 0x11100070, 0x000c0c08, 0x03c00000, 0x30000000, 0x200005c0 },
		{ 150, 150,  75, 0x11100070, 0x00100808, 0x03c00000, 0x28000000, 0x200005c0 },
		{ 150, 150, 150, 0x11100070, 0x00080808, 0x03c00000, 0x20000000, 0x200005c0 },
		{ 155, 155,  77, 0x11100070, 0x00120909, 0x04600000, 0x28000000, 0x200005c0 },
		{ 155, 155, 155, 0x11100070, 0x00090909, 0x04600000, 0x20000000, 0x200005c0 },
		{ 166,  83,  83, 0x11100070, 0x000c0c06, 0x03200000, 0x40000000, 0x200005c0 },
		{ 166, 166,  83, 0x11100070, 0x000c0606, 0x03200000, 0x28000000, 0x200005c0 },
		{ 166, 166, 166, 0x11100070, 0x00060606, 0x03200000, 0x20000000, 0x200005c0 },
		{ 200, 200, 100, 0x11100070, 0x000c0606, 0x03c00000, 0x28000000, 0x200005c0 },
		{ 223, 148,  74, 0x11100070, 0x00120906, 0x04300000, 0x38000000, 0x200005c0 },
		{ 240, 120, 120, 0x11100070, 0x00080804, 0x03000000, 0x40000000, 0x200005c0 },
		{ 240, 240, 120, 0x11100070, 0x00080404, 0x03000000, 0x28000000, 0x200005c0 },
		{ 250, 166,  83, 0x11100070, 0x000c0604, 0x03200000, 0x38000000, 0x200005c0 },
		{ 250, 166, 166, 0x11100070, 0x00060604, 0x03200000, 0x30000000, 0x200005c0 },
		{ 266, 133, 133, 0x11100070, 0x000c0c06, 0x05000000, 0x40000000, 0x200005c0 },
		{ 266, 266, 133, 0x11100070, 0x000c0606, 0x05000000, 0x28000000, 0x200005c0 },
		{ 300, 100, 100, 0x11100070, 0x000c0c04, 0x03c00000, 0x60000000, 0x200005c0 },
		{ 300, 150,  75, 0x11100070, 0x00100804, 0x03c00000, 0x48000000, 0x200005c0 },
		{ 300, 150, 150, 0x11100070, 0x00080804, 0x03c00000, 0x40000000, 0x200005c0 },
		{ 300, 200, 100, 0x11100070, 0x000c0604, 0x03c00000, 0x38000000, 0x200005c0 },
		{ 320, 160,  80, 0x11100070, 0x00100804, 0x04000000, 0x48000000, 0x200005c0 },
		{ 320, 213, 106, 0x11100070, 0x000c0604, 0x04000000, 0x38000000, 0x200005c0 },
		{ 320, 240, 120, 0x11100070, 0x00080403, 0x03000000, 0x38000000, 0x200005c0 },
		{ 320, 256, 128, 0x11100070, 0x000a0504, 0x04000000, 0x38000000, 0x200005c0 },
		{ 330, 165,  82, 0x11100070, 0x00100804, 0x04200000, 0x48000000, 0x200005c0 },
		{ 330, 165, 165, 0x11100070, 0x00080804, 0x04200000, 0x40000000, 0x200005c0 },
		{ 333, 166,  83, 0x11100070, 0x000c0603, 0x03200000, 0x48000000, 0x200005c0 },
		{ 333, 166, 166, 0x11100070, 0x00060603, 0x03200000, 0x40000000, 0x200005c0 },
		{ 340, 226, 113, 0x11100070, 0x000c0604, 0x04400000, 0x38000000, 0x200005c0 },
		{ 350, 175,  87, 0x11100070, 0x00100804, 0x04600000, 0x48000000, 0x200005c0 },
		{ 353, 176,  88, 0x11100070, 0x000c0603, 0x03500000, 0x48000000, 0x200005c0 },
		{ 360, 240, 120, 0x11100070, 0x000c0604, 0x04800000, 0x38000000, 0x200005c0 },
		{ 370, 185,  92, 0x11100070, 0x00100804, 0x04a00000, 0x48000000, 0x200005c0 },
		{ 370, 246, 123, 0x11100070, 0x000c0604, 0x04a00000, 0x38000000, 0x200005c0 },
		{ 373, 186,  93, 0x11100070, 0x000c0603, 0x03800000, 0x48000000, 0x200005c0 },
		{ 400, 133, 133, 0x11100070, 0x000c0c04, 0x05000000, 0x60000000, 0x200005c0 },
		{ 400, 160,  80, 0x11100070, 0x00140a04, 0x05000000, 0x58000000, 0x200005c0 },
		{ 400, 160, 160, 0x11100070, 0x000a0a04, 0x05000000, 0x50000000, 0x200005c0 },
		{ 400, 200, 100, 0x11100070, 0x00100804, 0x05000000, 0x48000000, 0x200005c0 },
		{ 400, 266, 133, 0x11100070, 0x000c0604, 0x05000000, 0x38000000, 0x200005c0 },
		{ 426, 213, 106, 0x11100070, 0x000c0603, 0x04000000, 0x48000000, 0x200005c0 },
		{ 440, 220, 110, 0x11100070, 0x000c0603, 0x04200000, 0x48000000, 0x200005c0 },
		{ 446, 148,  74, 0x11100070, 0x00120903, 0x04300000, 0x68000000, 0x200005c0 },
		{ 453, 226, 113, 0x11100070, 0x000c0603, 0x04400000, 0x48000000, 0x200005c0 },
		{ 466, 233, 116, 0x11100070, 0x000c0603, 0x04600000, 0x48000000, 0x200005c0 },
		{ 480, 137,  68, 0x11100070, 0x000e0702, 0x03000000, 0x78000000, 0x200005c0 },
		{ 480, 137, 137, 0x11100070, 0x00070702, 0x03000000, 0x70000000, 0x200005c0 },
		{ 480, 160,  80, 0x11100070, 0x000c0602, 0x03000000, 0x68000000, 0x200005c0 },
		{ 480, 240, 120, 0x11100070, 0x00080402, 0x03000000, 0x48000000, 0x200005c0 },
		{ 500, 100, 100, 0x11100070, 0x000a0a02, 0x03200000, 0xa0000000, 0x200005c0 },
		{ 500, 166,  83, 0x11100070, 0x000c0602, 0x03200000, 0x68000000, 0x200005c0 },
		{ 500, 166, 166, 0x11100070, 0x00060602, 0x03200000, 0x60000000, 0x200005c0 },
		{ 500, 200, 100, 0x11100070, 0x000a0502, 0x03200000, 0x58000000, 0x200005c0 },
		{ 500, 250, 125, 0x11100070, 0x00080402, 0x03200000, 0x48000000, 0x200005c0 },
		{ 530, 176,  88, 0x11100070, 0x000c0602, 0x03500000, 0x68000000, 0x200005c0 },
		{ 530, 176, 176, 0x11100070, 0x00060602, 0x03500000, 0x60000000, 0x200005c0 },
		{ 530, 212, 106, 0x11100070, 0x000a0502, 0x03500000, 0x58000000, 0x200005c0 },
		{ 530, 265, 132, 0x11100070, 0x00080402, 0x03500000, 0x48000000, 0x200005c0 },
		{ 533, 133, 133, 0x11100070, 0x000c0c03, 0x05000000, 0x80000000, 0x200005c0 },
		{ 533, 266, 133, 0x11100070, 0x000c0603, 0x05000000, 0x48000000, 0x200005c0 },
		{0}
	};

	/* 25MHz table for 5356 */
	static uint32 BCMINITDATA(pll25mhz_table)[][8] = {
		/* cpu, ddr, axi, pllctl12,  pllctl13,   pllctl14,   pllctl15,   pllctl16 */
		{  66,  66,  66, 0x11100070, 0x00121212, 0x03000000, 0x20000000, 0x200005c0 },
		{  75,  75,  75, 0x11100070, 0x00101010, 0x03000000, 0x20000000, 0x200005c0 },
		{  80,  80,  80, 0x11100070, 0x000a0a0a, 0x02000000, 0x20000000, 0x200005c0 },
		{  83,  83,  83, 0x11100070, 0x000c0c0c, 0x02800000, 0x20000000, 0x200005c0 },
		{ 100,  66,  66, 0x11100070, 0x0012120c, 0x03000000, 0x30000000, 0x200005c0 },
		{ 100, 100, 100, 0x11100070, 0x000c0c0c, 0x03000000, 0x20000000, 0x200005c0 },
		{ 125,  83,  83, 0x11100070, 0x000c0c08, 0x02800000, 0x30000000, 0x200005c0 },
		{ 133, 133, 133, 0x11100070, 0x000c0c0c, 0x04000000, 0x20000000, 0x200005c0 },
		{ 150,  75,  75, 0x11100070, 0x00101008, 0x03000000, 0x40000000, 0x200005c0 },
		{ 150, 100, 100, 0x11100070, 0x000c0c08, 0x03000000, 0x30000000, 0x200005c0 },
		{ 150, 150,  75, 0x11100070, 0x00100808, 0x03000000, 0x28000000, 0x200005c0 },
		{ 150, 150, 150, 0x11100070, 0x00080808, 0x03000000, 0x20000000, 0x200005c0 },
		{ 166,  83,  83, 0x11100070, 0x000c0c06, 0x02800000, 0x40000000, 0x200005c0 },
		{ 166, 166,  83, 0x11100070, 0x000c0606, 0x02800000, 0x28000000, 0x200005c0 },
		{ 166, 166, 166, 0x11100070, 0x00060606, 0x02800000, 0x20000000, 0x200005c0 },
		{ 200, 133, 133, 0x11100070, 0x000c0c08, 0x04000000, 0x30000000, 0x200005c0 },
		{ 200, 200, 100, 0x11100070, 0x000c0606, 0x03000000, 0x28000000, 0x200005c0 },
		{ 250, 166,  83, 0x11100070, 0x000c0604, 0x02800000, 0x38000000, 0x200005c0 },
		{ 250, 166, 166, 0x11100070, 0x00060604, 0x02800000, 0x30000000, 0x200005c0 },
		{ 293, 195,  97, 0x11100070, 0x000c0604, 0x02f00000, 0x38000000, 0x200005c0 },
		{ 300, 100, 100, 0x11100070, 0x000c0c04, 0x03000000, 0x60000000, 0x200005c0 },
		{ 300, 120, 120, 0x11100070, 0x000a0a04, 0x03000000, 0x50000000, 0x200005c0 },
		{ 300, 150,  75, 0x11100070, 0x00100804, 0x03000000, 0x48000000, 0x200005c0 },
		{ 300, 150, 150, 0x11100070, 0x00080804, 0x03000000, 0x40000000, 0x200005c0 },
		{ 300, 200, 100, 0x11100070, 0x000c0604, 0x03000000, 0x38000000, 0x200005c0 },
		{ 332, 110, 110, 0x11100070, 0x000c0c04, 0x03540000, 0x6047ae14, 0x202c2820 },
		{ 332, 133, 133, 0x11100070, 0x000a0a04, 0x03540000, 0x5047ae14, 0x202c2820 },
		{ 332, 166,  83, 0x11100070, 0x00100804, 0x03540000, 0x4847ae14, 0x202c2820 },
		{ 333, 111, 111, 0x11100070, 0x00090903, 0x02800000, 0x60000000, 0x200005c0 },
		{ 333, 133, 133, 0x11100070, 0x000f0f06, 0x05000000, 0x50000000, 0x38000700 },
		{ 333, 166,  83, 0x11100070, 0x000c0603, 0x02800000, 0x48000000, 0x200005c0 },
		{ 333, 166, 166, 0x11100070, 0x00060603, 0x02800000, 0x40000000, 0x200005c0 },
		{ 400, 133, 133, 0x11100070, 0x000c0c04, 0x04000000, 0x60000000, 0x200005c0 },
		{ 400, 200, 100, 0x11100070, 0x000c0603, 0x03000000, 0x48000000, 0x200005c0 },
		{ 400, 266, 133, 0x11100070, 0x000c0604, 0x04000000, 0x38000000, 0x200005c0 },
		{ 500, 166,  83, 0x11100070, 0x000c0602, 0x02800000, 0x68000000, 0x200005c0 },
		{ 500, 166, 166, 0x11100070, 0x00060602, 0x02800000, 0x60000000, 0x200005c0 },
		{ 500, 200, 100, 0x11100070, 0x000a0502, 0x02800000, 0x58000000, 0x200005c0 },
		{ 500, 250, 125, 0x11100070, 0x00080402, 0x02800000, 0x48000000, 0x200005c0 },
		{0}
	};

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID)
		return mips_pmu_setclock_4706(sih, mipsclock, ddrclock, axiclock);

	/* By default use the 20MHz pll table */
	pll_table = pll20mhz_table;

	osh = si_osh(sih);

	/* Adjust the mainpll_pll0 address and pll table for 5356 */
	if (CHIPID(sih->chip) == BCM5356_CHIP_ID) {
	  mainpll_pll0 = PMU5356_MAINPLL_PLL0;
	  pll_table = pll25mhz_table;
	}
	/* Adjust the mainpll_pll0 address and pll table for 5357 */
	if ((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4749_CHIP_ID)) {
	  mainpll_pll0 = PMU5357_MAINPLL_PLL0;
	  pll_table = pll20mhz_table;
	}

	/* get index of the current core */
	idx = si_coreidx(sih);

	/* switch to chipc core */
	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	mipsclock /= 1000000;
	ddrclock /= 1000000;
	axiclock /= 1000000;

	HNDMIPS_NONE(("Looking for %d/%d/%d\n", mipsclock, ddrclock, axiclock));

	for (idx = 0; pll_table[idx][0] != 0; idx++) {
		uint16 chippkg;
		/* Bypass pll entries that are not allowed */
		if ((((pll_table[idx][4] & 0xff) < 4) ||
		     (((pll_table[idx][5] >> 20) & 0x1ff) > 0x50)) &&
		    ((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4749_CHIP_ID)) &&
		    (((chippkg = R_REG(osh, &cc->sromotp[23])) & 0x80) == 0x80)) {
			continue;
		}
		if ((mipsclock <= pll_table[idx][0]) &&
		    ((ddrclock == 0) || (ddrclock <= pll_table[idx][1])) &&
		    ((axiclock == 0) || (axiclock <= pll_table[idx][2])))
			break;
	}

	if (pll_table[idx][0] == 0) {
		ret = FALSE;
		goto done;
	}

	HNDMIPS_NONE(("Using entry %d: %d/%d/%d, 0x%08x, 0x%08x, 0x%08x, 0x%08x, %d\n", idx,
	              pll_table[idx][0], pll_table[idx][1], pll_table[idx][2],
	              pll_table[idx][3], pll_table[idx][4], pll_table[idx][5],
	              pll_table[idx][6], pll_table[idx][7]));

	for (i = PMU5_PLL_P1P2_OFF; i <= PMU5_PLL_FMAB_OFF; i++) {
		W_REG(osh, &cc->pllcontrol_addr, mainpll_pll0 + i);
		(void)R_REG(osh, &cc->pllcontrol_addr);
		if (R_REG(osh, &cc->pllcontrol_data) != pll_table[idx][i + 3])
			break;
	}

	/* All matched, no change needed */
	if (i == (PMU5_PLL_FMAB_OFF + 1))
		goto done;

	/* Write new PLL settings */
	for (i = PMU5_PLL_P1P2_OFF; i <= PMU5_PLL_PLLCTL_OFF; i++) {
		uint32 tmp;

		W_REG(osh, &cc->pllcontrol_addr, mainpll_pll0 + i);
		(void)R_REG(osh, &cc->pllcontrol_addr);
		tmp = pll_table[idx][i + 3];
		W_REG(osh, &cc->pllcontrol_data, tmp);
	}
	/* Wait for the last write */
	(void)R_REG(osh, &cc->pllcontrol_data);

	if (CHIPID(sih->chip) == BCM47162_CHIP_ID) {
		/* In 47162, clear min_res_mask */
		W_REG(osh, &cc->min_res_mask,
		      R_REG(osh, &cc->min_res_mask) & ~RES4716_PROC_HT_AVAIL);

		/* Reset, use chipcommon's watchdog, not the PMU */
		W_REG(osh, &cc->watchdog, 1000);

		/* And now do the pll update */
		W_REG(osh, &cc->pmucontrol,
		      R_REG(osh, &cc->pmucontrol) | PCTL_PLL_PLLCTL_UPD);
	} else
		si_watchdog(sih, 100);

	/* wait for timer interrupt */
	while (1)
		__asm__ __volatile__(
			".set\tmips3\n\t"
			"sync\n\t"
			"wait\n\t"
			".set\tmips0");

done:
	/* switch back to previous core */
	si_setcoreidx(sih, idx);

	return ret;
}

static void
BCMINITFN(handler)(void)
{
	__asm__(
		".set\tmips32\n\t"
		"ssnop\n\t"
		"ssnop\n\t"
	/* Disable interrupts */
	/*	MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) & ~(ALLINTS | STO_IE)); */
		"mfc0 $15, $12\n\t"
	/* Just a Hack to not to use reg 'at' which was causing problems on 4704 A2 */
		"li $14, -31746\n\t"
		"and $15, $15, $14\n\t"
		"mtc0 $15, $12\n\t"
		"eret\n\t"
		"nop\n\t"
		"nop\n\t"
		".set\tmips0");
}

/* The following MUST come right after handler() */
static void
BCMINITFN(afterhandler)(void)
{
}

/*
 * Set the MIPS, backplane and PCI clocks as closely as possible.
 *
 * MIPS clocks synchronization function has been moved from PLL in chipcommon
 * core rev. 15 to a DLL inside the MIPS core in 4785.
 */
bool
BCMINITFN(si_mips_setclock)(si_t *sih, uint32 mipsclock, uint32 siclock, uint32 pciclock)
{
	osl_t *osh;
	chipcregs_t *cc = NULL;
	mips33regs_t *mipsr = NULL;
	volatile uint32 *clockcontrol_n, *clockcontrol_sb, *clockcontrol_pci, *clockcontrol_m2;
	uint32 orig_n, orig_sb, orig_pci, orig_m2, orig_mips, orig_ratio_parm, orig_ratio_cfg;
	uint32 pll_type, sync_mode;
	uint ic_size, ic_lsize;
	uint idx, i;

	/* PLL configuration: type 3 */
	typedef struct {
		uint32 mipsclock;
		uint16 n;
		uint32 m2; /* that is the clockcontrol_m2 */
	} type3_table_t;
	static type3_table_t type3_table[] = {
		/* for 5350, mips clock is always double sb clock */
		{ 150000000, 0x311, 0x4020005 },
		{ 200000000, 0x311, 0x4020003 },
		};

	/* PLL configuration: type 2, 4, 7 */
	typedef struct {
		uint32 mipsclock;
		uint32 sbclock;
		uint32 pciclock;
		uint16 n;
		uint32 sb;
		uint32 pci33;
		uint32 m2;
		uint32 m3;
		uint32 ratio_cfg;
		uint32 ratio_parm;
		uint32 dll_r1;
		uint32 dll_r2;
	} n4m_table_t;
	static n4m_table_t BCMINITDATA(type2_table)[] = {
		{ 120000000, 60000000, 32000000, 0x0303, 0x01000200, 0x01000600, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 150000000, 75000000, 33333333, 0x0303, 0x01000100, 0x01000600, 0x01000100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 180000000, 80000000, 30000000, 0x0403, 0x01010000, 0x01020300, 0x01020600,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 180000000, 90000000, 30000000, 0x0403, 0x01000100, 0x01020300, 0x01000100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 200000000, 100000000, 33333333, 0x0303, 0x02010000, 0x02040001, 0x02010000,
		0x06000001, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 211200000, 105600000, 30171428, 0x0902, 0x01000200, 0x01030400, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 220800000, 110400000, 31542857, 0x1500, 0x01000200, 0x01030400, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 230400000, 115200000, 32000000, 0x0604, 0x01000200, 0x01020600, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 234000000, 104000000, 31200000, 0x0b01, 0x01010000, 0x01010700, 0x01020600,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 240000000, 120000000, 33333333, 0x0803, 0x01000200, 0x01020600, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 252000000, 126000000, 33333333, 0x0504, 0x01000100, 0x01020500, 0x01000100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 264000000, 132000000, 33000000, 0x0903, 0x01000200, 0x01020700, 0x01000200,
		0x05000200, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 270000000, 120000000, 30000000, 0x0703, 0x01010000, 0x01030400, 0x01020600,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 276000000, 122666666, 31542857, 0x1500, 0x01010000, 0x01030400, 0x01020600,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 280000000, 140000000, 31111111, 0x0503, 0x01000000, 0x01010600, 0x01000000,
		0x05000000, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 288000000, 128000000, 32914285, 0x0604, 0x01010000, 0x01030400, 0x01020600,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 288000000, 144000000, 32000000, 0x0404, 0x01000000, 0x01010600, 0x01000000,
		0x05000000, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 300000000, 133333333, 33333333, 0x0803, 0x01010000, 0x01020600, 0x01010100,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 300000000, 133333333, 37500000, 0x0803, 0x01010000, 0x01020500, 0x01010100,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 300000000, 133333333, 42857142, 0x0803, 0x01010000, 0x01020400, 0x01010100,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 300000000, 133333333, 50000000, 0x0803, 0x01010000, 0x01020300, 0x01010100,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 300000000, 133333333, 60000000, 0x0803, 0x01010000, 0x01020200, 0x01010100,
		0x05000100, 8, 0x012a00a9, 9 /* ratio  4/9 */, 0x012a00a9 },
		{ 300000000, 150000000, 33333333, 0x0803, 0x01000100, 0x01020600, 0x01010100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 300000000, 150000000, 37500000, 0x0803, 0x01000100, 0x01020500, 0x01010100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 300000000, 150000000, 42857142, 0x0803, 0x01000100, 0x01020400, 0x01010100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 300000000, 150000000, 50000000, 0x0803, 0x01000100, 0x01020300, 0x01010100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 300000000, 150000000, 60000000, 0x0803, 0x01000100, 0x01020200, 0x01010100,
		0x05000100, 11, 0x0aaa0555, 8 /* ratio  4/8 */, 0x00aa0055 },
		{ 330000000, 132000000, 33000000, 0x0903, 0x01000200, 0x00020200, 0x01010100,
		0x05000100, 0, 0, 10 /* ratio 4/10 */, 0x02520129 },
		{ 330000000, 146666666, 33000000, 0x0903, 0x01010000, 0x00020200, 0x01010100,
		0x05000100, 0, 0, 9 /* ratio 4/9 */, 0x012a00a9 },
		{ 330000000, 165000000, 33000000, 0x0903, 0x01000100, 0x00020200, 0x01010100,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 330000000, 165000000, 41250000, 0x0903, 0x01000100, 0x00020100, 0x01010100,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 330000000, 165000000, 55000000, 0x0903, 0x01000100, 0x00020000, 0x01010100,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 360000000, 120000000, 32000000, 0x0a03, 0x01000300, 0x00010201, 0x01010200,
		0x05000100, 0, 0, 12 /* ratio 4/12 */, 0x04920492 },
		{ 360000000, 144000000, 32000000, 0x0a03, 0x01000200, 0x00010201, 0x01010200,
		0x05000100, 0, 0, 10 /* ratio 4/10 */, 0x02520129 },
		{ 360000000, 160000000, 32000000, 0x0a03, 0x01010000, 0x00010201, 0x01010200,
		0x05000100, 0, 0, 9 /* ratio 4/9 */, 0x012a00a9 },
		{ 360000000, 180000000, 32000000, 0x0a03, 0x01000100, 0x00010201, 0x01010200,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 360000000, 180000000, 40000000, 0x0a03, 0x01000100, 0x00010101, 0x01010200,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 360000000, 180000000, 53333333, 0x0a03, 0x01000100, 0x00010001, 0x01010200,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
		{ 390000000, 130000000, 32500000, 0x0b03, 0x01010100, 0x00020101, 0x01020100,
		0x05000100, 0, 0, 12 /* ratio 4/12 */, 0x04920492 },
		{ 390000000, 156000000, 32500000, 0x0b03, 0x01000200, 0x00020101, 0x01020100,
		0x05000100, 0, 0, 10 /* ratio 4/10 */, 0x02520129 },
		{ 390000000, 173000000, 32500000, 0x0b03, 0x01010000, 0x00020101, 0x01020100,
		0x05000100, 0, 0, 9 /* ratio 4/9 */, 0x012a00a9 },
		{ 390000000, 195000000, 32500000, 0x0b03, 0x01000100, 0x00020101, 0x01020100,
		0x05000100, 0, 0, 8 /* ratio 4/8 */, 0x00aa0055 },
	};
	static n4m_table_t BCMINITDATA(type4_table)[] = {
		{ 120000000, 60000000, 0, 0x0009, 0x11020009, 0x01030203, 0x11020009, 0x04000009,
		11, 0x0aaa0555 },
		{ 150000000, 75000000, 0, 0x0009, 0x11050002, 0x01030203, 0x11050002, 0x04000005,
		11, 0x0aaa0555 },
		{ 192000000, 96000000, 0, 0x0702, 0x04000011, 0x11030011, 0x04000011, 0x04000003,
		11, 0x0aaa0555 },
		{ 198000000, 99000000, 0, 0x0603, 0x11020005, 0x11030011, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 200000000, 100000000, 0, 0x0009, 0x04020011, 0x11030011, 0x04020011, 0x04020003,
		11, 0x0aaa0555 },
		{ 204000000, 102000000, 0, 0x0c02, 0x11020005, 0x01030303, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 208000000, 104000000, 0, 0x0802, 0x11030002, 0x11090005, 0x11030002, 0x04000003,
		11, 0x0aaa0555 },
		{ 210000000, 105000000, 0, 0x0209, 0x11020005, 0x01030303, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 216000000, 108000000, 0, 0x0111, 0x11020005, 0x01030303, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 224000000, 112000000, 0, 0x0205, 0x11030002, 0x02002103, 0x11030002, 0x04000003,
		11, 0x0aaa0555 },
		{ 228000000, 101333333, 0, 0x0e02, 0x11030003, 0x11210005, 0x01030305, 0x04000005,
		8, 0x012a00a9 },
		{ 228000000, 114000000, 0, 0x0e02, 0x11020005, 0x11210005, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 240000000, 102857143, 0, 0x0109, 0x04000021, 0x01050203, 0x11030021, 0x04000003,
		13, 0x254a14a9 },
		{ 240000000, 120000000, 0, 0x0109, 0x11030002, 0x01050203, 0x11030002, 0x04000003,
		11, 0x0aaa0555 },
		{ 252000000, 100800000, 0, 0x0203, 0x04000009, 0x11050005, 0x02000209, 0x04000002,
		9, 0x02520129 },
		{ 252000000, 126000000, 0, 0x0203, 0x04000005, 0x11050005, 0x04000005, 0x04000002,
		11, 0x0aaa0555 },
		{ 264000000, 132000000, 0, 0x0602, 0x04000005, 0x11050005, 0x04000005, 0x04000002,
		11, 0x0aaa0555 },
		{ 272000000, 116571428, 0, 0x0c02, 0x04000021, 0x02000909, 0x02000221, 0x04000003,
		13, 0x254a14a9 },
		{ 280000000, 120000000, 0, 0x0209, 0x04000021, 0x01030303, 0x02000221, 0x04000003,
		13, 0x254a14a9 },
		{ 288000000, 123428571, 0, 0x0111, 0x04000021, 0x01030303, 0x02000221, 0x04000003,
		13, 0x254a14a9 },
		{ 300000000, 120000000, 0, 0x0009, 0x04000009, 0x01030203, 0x02000902, 0x04000002,
		9, 0x02520129 },
		{ 300000000, 150000000, 0, 0x0009, 0x04000005, 0x01030203, 0x04000005, 0x04000002,
		11, 0x0aaa0555 }
	};
	static n4m_table_t BCMINITDATA(type7_table)[] = {
		{ 183333333, 91666666, 0, 0x0605, 0x04000011, 0x11030011, 0x04000011, 0x04000003,
		11, 0x0aaa0555 },
		{ 187500000, 93750000, 0, 0x0a03, 0x04000011, 0x11030011, 0x04000011, 0x04000003,
		11, 0x0aaa0555 },
		{ 196875000, 98437500, 0, 0x1003, 0x11020005, 0x11050011, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 200000000, 100000000, 0, 0x0311, 0x04000011, 0x11030011, 0x04000009, 0x04000003,
		11, 0x0aaa0555 },
		{ 200000000, 100000000, 0, 0x0311, 0x04020011, 0x11030011, 0x04020011, 0x04020003,
		11, 0x0aaa0555 },
		{ 206250000, 103125000, 0, 0x1103, 0x11020005, 0x11050011, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 212500000, 106250000, 0, 0x0c05, 0x11020005, 0x01030303, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 215625000, 107812500, 0, 0x1203, 0x11090009, 0x11050005, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 216666666, 108333333, 0, 0x0805, 0x11020003, 0x11030011, 0x11020003, 0x04000003,
		11, 0x0aaa0555 },
		{ 225000000, 112500000, 0, 0x0d03, 0x11020003, 0x11030011, 0x11020003, 0x04000003,
		11, 0x0aaa0555 },
		{ 233333333, 116666666, 0, 0x0905, 0x11020003, 0x11030011, 0x11020003, 0x04000003,
		11, 0x0aaa0555 },
		{ 237500000, 118750000, 0, 0x0e05, 0x11020005, 0x11210005, 0x11020005, 0x04000005,
		11, 0x0aaa0555 },
		{ 240000000, 120000000, 0, 0x0b11, 0x11020009, 0x11210009, 0x11020009, 0x04000009,
		11, 0x0aaa0555 },
		{ 250000000, 125000000, 0, 0x0f03, 0x11020003, 0x11210003, 0x11020003, 0x04000003,
		11, 0x0aaa0555 }
	};

	ulong start, end, dst;
	bool ret = FALSE;

	volatile uint32 *dll_ctrl = (volatile uint32 *)0xff400008;
	volatile uint32 *dll_r1 = (volatile uint32 *)0xff400010;
	volatile uint32 *dll_r2 = (volatile uint32 *)0xff400018;

	/* 5354 chipcommon pll setting can't be changed.
	 * The PMU on power up comes up with the default clk frequency
	 * of 240MHz
	 */
	if ((CHIPID(sih->chip) == BCM5354_CHIP_ID) || (CHIPID(sih->chip) == BCM53572_CHIP_ID))
		return TRUE;

	if (sih->cccaps & CC_CAP_PMU)
		return mips_pmu_setclock(sih, mipsclock, siclock, pciclock);

	osh = si_osh(sih);

	/* get index of the current core */
	idx = si_coreidx(sih);
	clockcontrol_m2 = NULL;

	/* switch to chipc core */
	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	pll_type = sih->cccaps & CC_CAP_PLL_MASK;
	if (pll_type == PLL_TYPE6) {
		clockcontrol_n = NULL;
		clockcontrol_sb = NULL;
		clockcontrol_pci = NULL;
	} else {
		clockcontrol_n = &cc->clockcontrol_n;
		clockcontrol_sb = &cc->clockcontrol_sb;
		clockcontrol_pci = &cc->clockcontrol_pci;
		clockcontrol_m2 = &cc->clockcontrol_m2;
	}

	if (pll_type == PLL_TYPE6) {
		/* Silence compilers */
		orig_n = orig_sb = orig_pci = 0;
	} else {
		/* Store the current clock register values */
		orig_n = R_REG(osh, clockcontrol_n);
		orig_sb = R_REG(osh, clockcontrol_sb);
		orig_pci = R_REG(osh, clockcontrol_pci);
	}

	if (pll_type == PLL_TYPE3) {
		/* 5350 */
		if (CHIPID(sih->chip) != BCM5365_CHIP_ID) {
			/*
			 * Search for the closest MIPS clock less than or equal to
			 * a preferred value.
			 */
			for (i = 0; i < ARRAYSIZE(type3_table); i++) {
				if (type3_table[i].mipsclock > mipsclock)
					break;
			}
			if (i == 0) {
				ret = FALSE;
				goto done;
			} else {
				ret = TRUE;
				i--;
			}
			ASSERT(type3_table[i].mipsclock <= mipsclock);

			/* No PLL change */
			orig_m2 = R_REG(osh, &cc->clockcontrol_m2);
			if ((orig_n == type3_table[i].n) &&
			    (orig_m2 == type3_table[i].m2)) {
				goto done;
			}

			/* Set the PLL controls */
			W_REG(osh, clockcontrol_n, type3_table[i].n);
			W_REG(osh, clockcontrol_m2, type3_table[i].m2);

			/* Reset */
			si_watchdog(sih, 1);
			while (1);
		}
	} else if ((pll_type == PLL_TYPE2) ||
	           (pll_type == PLL_TYPE4) ||
	           (pll_type == PLL_TYPE6) ||
	           (pll_type == PLL_TYPE7)) {
		n4m_table_t *table = NULL, *te;
		uint tabsz = 0;

		ASSERT(cc);

		orig_mips = R_REG(osh, &cc->clockcontrol_m3);

		switch (pll_type) {
		case PLL_TYPE6: {
			uint32 new_mips = 0;

			ret = TRUE;
			if (mipsclock <= SB2MIPS_T6(CC_T6_M1))
				new_mips = CC_T6_MMASK;

			if (orig_mips == new_mips)
				goto done;

			W_REG(osh, &cc->clockcontrol_m3, new_mips);
			goto end_fill;
		}
		case PLL_TYPE2:
			table = type2_table;
			tabsz = ARRAYSIZE(type2_table);
			break;
		case PLL_TYPE4:
			table = type4_table;
			tabsz = ARRAYSIZE(type4_table);
			break;
		case PLL_TYPE7:
			table = type7_table;
			tabsz = ARRAYSIZE(type7_table);
			break;
		default:
			ASSERT("No table for plltype" == NULL);
			break;
		}

		/* Store the current clock register values */
		orig_m2 = R_REG(osh, &cc->clockcontrol_m2);
		orig_ratio_parm = 0;
		orig_ratio_cfg = 0;

		/* Look up current ratio */
		for (i = 0; i < tabsz; i++) {
			if ((orig_n == table[i].n) &&
			    (orig_sb == table[i].sb) &&
			    (orig_pci == table[i].pci33) &&
			    (orig_m2 == table[i].m2) &&
			    (orig_mips == table[i].m3)) {
				orig_ratio_parm = table[i].ratio_parm;
				orig_ratio_cfg = table[i].ratio_cfg;
				break;
			}
		}

		/* Search for the closest MIPS clock greater or equal to a preferred value */
		for (i = 0; i < tabsz; i++) {
			ASSERT(table[i].mipsclock ==
			       si_clock_rate(pll_type, table[i].n, table[i].m3));
			if ((mipsclock <= table[i].mipsclock) &&
			    ((siclock == 0) || (siclock <= table[i].sbclock)) &&
			    ((pciclock == 0) || (pciclock <= table[i].pciclock)))
				break;
		}
		if (i == tabsz) {
			ret = FALSE;
			goto done;
		} else {
			te = &table[i];
			ret = TRUE;
		}

		/* No PLL change */
		if ((orig_n == te->n) &&
		    (orig_sb == te->sb) &&
		    (orig_pci == te->pci33) &&
		    (orig_m2 == te->m2) &&
		    (orig_mips == te->m3))
			goto done;

		/* Set the PLL controls */
		W_REG(osh, clockcontrol_n, te->n);
		W_REG(osh, clockcontrol_sb, te->sb);
		W_REG(osh, clockcontrol_pci, te->pci33);
		W_REG(osh, &cc->clockcontrol_m2, te->m2);
		W_REG(osh, &cc->clockcontrol_m3, te->m3);

		/* Set the chipcontrol bit to change mipsref to the backplane divider if needed */
		if ((pll_type == PLL_TYPE7) && (te->sb != te->m2) &&
		    (si_clock_rate(pll_type, te->n, te->m2) == 120000000))
			W_REG(osh, &cc->chipcontrol,
			      R_REG(osh, &cc->chipcontrol) | 0x100);

		/* No ratio change */
		if (CHIPID(sih->chip) != BCM4785_CHIP_ID) {
			if (orig_ratio_parm == te->ratio_parm)
				goto end_fill;
		}

		/* Preload the code into the cache */
		icache_probe(MFC0(C0_CONFIG, 1), &ic_size, &ic_lsize);
		if (CHIPID(sih->chip) == BCM4785_CHIP_ID) {
			start = ((ulong) &&start_fill_4785) & ~(ic_lsize - 1);
			end = ((ulong) &&end_fill_4785 + (ic_lsize - 1)) & ~(ic_lsize - 1);
		}
		else {
			start = ((ulong) &&start_fill) & ~(ic_lsize - 1);
			end = ((ulong) &&end_fill + (ic_lsize - 1)) & ~(ic_lsize - 1);
		}
		while (start < end) {
			cache_op(start, Fill_I);
			start += ic_lsize;
		}

		/* 4785 clock freq change procedures */
		if (CHIPID(sih->chip) == BCM4785_CHIP_ID) {
	start_fill_4785:
			/* Switch to async */
			MTC0(C0_BROADCOM, 4, (1 << 22));

			/* Set clock ratio in MIPS */
			*dll_r1 = (*dll_r1 & 0xfffffff0) | (te->dll_r1 - 1);
			*dll_r2 = te->dll_r2;

			/* Enable new settings in MIPS */
			*dll_r1 = *dll_r1 | 0xc0000000;

			/* Set active cfg */
			MTC0(C0_BROADCOM, 2, MFC0(C0_BROADCOM, 2) | (1 << 3) | 1);

			/* Fake soft reset (clock cfg registers not reset) */
			MTC0(C0_BROADCOM, 5, MFC0(C0_BROADCOM, 5) | (1 << 2));

			/* Clear active cfg */
			MTC0(C0_BROADCOM, 2, MFC0(C0_BROADCOM, 2) & ~(1 << 3));

			/* set watchdog timer */
			W_REG(osh, &cc->watchdog, 20);
			(void) R_REG(osh, &cc->chipid);

			/* wait for timer interrupt */
			__asm__ __volatile__(
				".set\tmips3\n\t"
				"sync\n\t"
				"wait\n\t"
				".set\tmips0");
	end_fill_4785:
			while (1);
		}
		/* Generic clock freq change procedures */
		else {
			/* Copy the handler */
			start = (ulong) &handler;
			end = (ulong) &afterhandler;
			dst = KSEG1ADDR(0x180);
			for (i = 0; i < (end - start); i += 4)
				*((ulong *)(dst + i)) = *((ulong *)(start + i));

			/* Preload the handler into the cache one line at a time */
			for (i = 0; i < (end - start); i += ic_lsize)
				cache_op(dst + i, Fill_I);

			/* Clear BEV bit */
			MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) & ~ST0_BEV);

			/* Enable interrupts */
			MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) | (ALLINTS | ST0_IE));

			/* Enable MIPS timer interrupt */
			mipsr = si_setcore(sih, MIPS33_CORE_ID, 0);
			ASSERT(mipsr);
			W_REG(osh, &mipsr->intmask, 1);

	start_fill:
			/* step 1, set clock ratios */
			MTC0(C0_BROADCOM, 3, te->ratio_parm);
			MTC0(C0_BROADCOM, 1, te->ratio_cfg);

			/* step 2: program timer intr */
			W_REG(osh, &mipsr->timer, 100);
			(void) R_REG(osh, &mipsr->timer);

			/* step 3, switch to async */
			sync_mode = MFC0(C0_BROADCOM, 4);
			MTC0(C0_BROADCOM, 4, 1 << 22);

			/* step 4, set cfg active */
			MTC0(C0_BROADCOM, 2, (1 << 3) | 1);

			/* steps 5 & 6 */
			__asm__ __volatile__(
				".set\tmips3\n\t"
				"wait\n\t"
				".set\tmips0");

			/* step 7, clear cfg active */
			MTC0(C0_BROADCOM, 2, 0);

			/* Additional Step: set back to orig sync mode */
			MTC0(C0_BROADCOM, 4, sync_mode);

			/* step 8, fake soft reset */
			MTC0(C0_BROADCOM, 5, MFC0(C0_BROADCOM, 5) | (1 << 2));

	end_fill:
			/* set watchdog timer */
			W_REG(osh, &cc->watchdog, 20);
			(void) R_REG(osh, &cc->chipid);

			/* wait for timer interrupt */
			__asm__ __volatile__(
				".set\tmips3\n\t"
				"sync\n\t"
				"wait\n\t"
				".set\tmips0");
			while (1);
		}
	}

done:
	/* Enable 4785 DLL */
	if (CHIPID(sih->chip) == BCM4785_CHIP_ID) {
		uint32 tmp;

		/* set mask to 1e, enable DLL (bit 0) */
		*dll_ctrl |= 0x0041e021;

		/* enable aggressive hardware mode */
		*dll_ctrl |= 0x00000080;

		/* wait for lock flag to clear */
		while ((*dll_ctrl & 0x2) == 0);

		/* clear sticky flags (clear on write 1) */
		tmp = *dll_ctrl;
		*dll_ctrl = tmp;

		/* set mask to 5b'10001 */
		*dll_ctrl = (*dll_ctrl & 0xfffc1fff) | 0x00022000;

		/* enable sync mode */
		MTC0(C0_BROADCOM, 4, MFC0(C0_BROADCOM, 4) & 0xfe3fffff);
		(void)MFC0(C0_BROADCOM, 4);
	}

	/* switch back to previous core */
	si_setcoreidx(sih, idx);

	return ret;
}

void
BCMINITFN(enable_pfc)(uint32 mode)
{
	uint32 prid;
	ulong start, end;
	uint ic_size, ic_lsize;

	prid = MFC0(C0_PRID, 0);
	if (!BCM330X(prid))
		return;

	/* enable prefetch cache if available */
	if (MFC0(C0_BROADCOM, 0) & BRCM_PFC_AVAIL) {
		/* If auto then choose the correct mode for this
		 * platform, currently we only ever select one mode
		 */
		if (mode == PFC_AUTO)
			mode = PFC_INST;

		icache_probe(MFC0(C0_CONFIG, 1), &ic_size, &ic_lsize);

		start = ((ulong) &&setpfc_start) & ~(ic_lsize - 1);
		end = ((ulong) &&setpfc_end + (ic_lsize - 1)) & ~(ic_lsize - 1);

		/* Preload setpfc code into the cache one line at a time */
		while (start < end) {
			cache_op(start, Fill_I);
			start += ic_lsize;
		}

		/* Now set the pfc */
	setpfc_start:
		/* write range */
		*(volatile uint32 *)PFC_CR1 = 0xffff0000;

		/* enable */
		*(volatile uint32 *)PFC_CR0 = mode;
	setpfc_end:
		/* Compiler foder */
		ic_size = 0;
	}
}

/* returns the ncdl value to be programmed into sdram_ncdl for calibration */
uint32
BCMINITFN(si_memc_get_ncdl)(si_t *sih)
{
	osl_t *osh;
	sbmemcregs_t *memc;
	uint32 ret = 0;
	uint32 config, rd, wr, misc, dqsg, cd, sm, sd;
	uint idx, rev;

	osh = si_osh(sih);

	idx = si_coreidx(sih);

	memc = (sbmemcregs_t *)si_setcore(sih, MEMC_CORE_ID, 0);
	if (memc == NULL)
		goto out;

	rev = si_corerev(sih);

	config = R_REG(osh, &memc->config);
	wr = R_REG(osh, &memc->wrncdlcor);
	rd = R_REG(osh, &memc->rdncdlcor);
	misc = R_REG(osh, &memc->miscdlyctl);
	dqsg = R_REG(osh, &memc->dqsgatencdl);

	rd &= MEMC_RDNCDLCOR_RD_MASK;
	wr &= MEMC_WRNCDLCOR_WR_MASK;
	dqsg &= MEMC_DQSGATENCDL_G_MASK;

	if (config & MEMC_CONFIG_DDR) {
		ret = (wr << 16) | (rd << 8) | dqsg;
	} else {
		if (rev > 0)
			cd = rd;
		else
			cd = (rd == MEMC_CD_THRESHOLD) ? rd : (wr + MEMC_CD_THRESHOLD);
		sm = (misc & MEMC_MISC_SM_MASK) >> MEMC_MISC_SM_SHIFT;
		sd = (misc & MEMC_MISC_SD_MASK) >> MEMC_MISC_SD_SHIFT;
		ret = (sm << 16) | (sd << 8) | cd;
	}

out:
	/* switch back to previous core */
	si_setcoreidx(sih, idx);

	return ret;
}

void
hnd_cpu_reset(si_t *sih)
{
	if (CHIPID(sih->chip) == BCM4785_CHIP_ID)
		MTC0(C0_BROADCOM, 4, (1 << 22));
	si_watchdog(sih, 1);
	if (CHIPID(sih->chip) == BCM4785_CHIP_ID) {
		__asm__ __volatile__(
			".set\tmips3\n\t"
			"sync\n\t"
			"wait\n\t"
			".set\tmips0");
	}
	while (1);
}

#if defined(BCMPERFSTATS)
/*
 * CP0 Register 25 supports 4 semi-independent 32bit performance counters.
 * $25 select 0, 1, 2, and 3 are the counters.  The counters *decrement* (who thought this one up?)
 * $25 select 4 and 5 each contain 2-16bit control fields, one for each of the 4 counters
 * $25 select 6 is the global perf control register.
 */
/* enable and start instruction counting */

void
hndmips_perf_cyclecount_enable(void)
{
	MTC0(C0_PERFORMANCE, 6, 0x80000200);	/* global enable perf counters */
	MTC0(C0_PERFORMANCE, 4,
	     0x8048 | MFC0(C0_PERFORMANCE, 4));	/* enable cycles counting for counter 0 */
	MTC0(C0_PERFORMANCE, 0, 0);		/* zero counter zero */
}

void
hndmips_perf_instrcount_enable(void)
{
	MTC0(C0_PERFORMANCE, 6, 0x80000200);	/* global enable perf counters */
	MTC0(C0_PERFORMANCE, 4,
	     0x8044 | MFC0(C0_PERFORMANCE, 4));	/* enable instructions counting for counter 0 */
	MTC0(C0_PERFORMANCE, 0, 0);		/* zero counter zero */
}

/* enable and start I$ hit and I$ miss counting */
void
hndmips_perf_icachecount_enable(void)
{
	MTC0(C0_PERFORMANCE, 6, 0x80000218);	/* enable I$ counting */
	MTC0(C0_PERFORMANCE, 4, 0x80148018);	/* count I$ hits in cntr 0 and misses in cntr 1 */
	MTC0(C0_PERFORMANCE, 0, 0);		/* zero counter 0 - # I$ hits */
	MTC0(C0_PERFORMANCE, 1, 0);		/* zero counter 1 - # I$ misses */
}

/* enable and start D$ hit and I$ miss counting */
void
hndmips_perf_dcachecount_enable(void)
{
	MTC0(C0_PERFORMANCE, 6, 0x80000211);	/* enable D$ counting */
	MTC0(C0_PERFORMANCE, 4, 0x80248028);	/* count D$ hits in cntr 0 and misses in cntr 1 */
	MTC0(C0_PERFORMANCE, 0, 0);		/* zero counter 0 - # D$ hits */
	MTC0(C0_PERFORMANCE, 1, 0);		/* zero counter 1 - # D$ misses */
}

void
hndmips_perf_icache_miss_enable()
{
	MTC0(C0_PERFORMANCE, 4,
	     0x80140000 | MFC0(C0_PERFORMANCE, 4)); /* enable cache misses counting for counter 1 */
	MTC0(C0_PERFORMANCE, 1, 0); /* zero counter one */
}


void
hndmips_perf_icache_hit_enable()
{
	MTC0(C0_PERFORMANCE, 5, 0x8018 | MFC0(C0_PERFORMANCE, 5));
	/* enable cache hits counting for counter 2 */
	MTC0(C0_PERFORMANCE, 2, 0);		/* zero counter 2 */
}

uint32
hndmips_perf_read_instrcount()
{
	return -(long)(MFC0(C0_PERFORMANCE, 0));
}

uint32
hndmips_perf_read_cache_miss()
{
	return -(long)(MFC0(C0_PERFORMANCE, 1));
}

uint32
hndmips_perf_read_cache_hit()
{
	return -(long)(MFC0(C0_PERFORMANCE, 2));
}

#endif 
