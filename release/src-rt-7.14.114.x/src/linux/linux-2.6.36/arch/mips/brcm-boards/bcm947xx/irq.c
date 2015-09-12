/*
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
 * Broadcom HND BCM47xx chips interrupt dispatcher.
 * Derived from ../generic/irq.c
 *
 *	MIPS IRQ	Source
 *      --------        ------------------
 *             0	Software
 *             1        Software
 *             2        Hardware (shared)
 *             3        Hardware
 *             4        Hardware
 *             5        Hardware
 *             6        Hardware
 *             7        Hardware (r4k timer)
 *
 *      MIPS IRQ        Linux IRQ
 *      --------        -----------
 *         0 - 1        0 - 1
 *             2        8 and above
 *         3 - 7        3 - 7
 *
 * MIPS has 8 IRQs as indicated and assigned above. SB cores
 * that use dedicated MIPS IRQ3 to IRQ6 are 1-to-1 mapped to
 * linux IRQ3 to IRQ6. SB cores sharing MIPS IRQ2 are mapped
 * to linux IRQ8 and above as virtual IRQs using the following
 * mapping:
 *
 *   <linux-IRQ#> = <SB-core-flag> + <base-IRQ> + 2
 *
 * where <base-IRQ> is specified in setup.c when calling
 * sb_mips_init(), 2 is to offset the two software IRQs.
 *
 * $Id: irq.c,v 1.11 2010-01-07 06:40:35 $
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif

#ifdef CONFIG_SMP
/*
 * This code is designed to work on Uniprocessor only.
 *
 * To support SMP we must know:
 *   - interrupt architecture
 *   - interrupt distribution machenism
 */
#error "This implementation does not support SMP"
#endif

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>

#include <asm/mipsregs.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <hndsoc.h>
#include <mips74k_core.h>
#include "bcm947xx.h"

/* cp0 SR register IM field */
#define SR_IM(irq)	(1 << ((irq) + STATUSB_IP0))

/* cp0 CR register IP field */
#define CR_IP(irq)	(1 << ((irq) + CAUSEB_IP0))

/* other local constants */
#define NUM_IRQS	32

/* local variables and functions */
static sbconfig_t *ccsbr = NULL;	/* Chipc core SB config regs */
static sbconfig_t *mipssbr = NULL;	/* MIPS core SB config regs */
static int mipsirq = -1;		/* MIPS core virtual IRQ number */
static uint32 shints = 0;		/* Set of shared interrupts */
static int irq2en = 0;			/* MIPS IRQ2 enable count */
static uint *mips_corereg = NULL;

/* global variables and functions */
extern si_t *bcm947xx_sih;		/* defined in setup.c */

extern asmlinkage void brcmIRQ(void);

/* Control functions for MIPS IRQ0 to IRQ7 */
static INLINE void
enable_brcm_irq(unsigned int irq)
{
	set_c0_status(SR_IM(irq));
	irq_enable_hazard();
}

static INLINE void
disable_brcm_irq(unsigned int irq)
{
	clear_c0_status(SR_IM(irq));
	irq_disable_hazard();
}

static void
ack_brcm_irq(unsigned int irq)
{
	/* Done in plat_irq_dispatch()! */
}

static void
end_brcm_irq(unsigned int irq)
{
	/* Done in plat_irq_dispatch()! */
}

/* Control functions for linux IRQ8 and above */
static INLINE void
enable_brcm_irq2(unsigned int irq)
{
	ASSERT(irq2en >= 0);
	if (irq2en++)
		return;
	enable_brcm_irq(2);
}

static INLINE void
disable_brcm_irq2(unsigned int irq)
{
	ASSERT(irq2en > 0);
	if (--irq2en)
		return;
	disable_brcm_irq(2);
}

static void
ack_brcm_irq2(unsigned int irq)
{
	/* Already done in plat_irq_dispatch()! */
}

static void
end_brcm_irq2(unsigned int irq)
{
	/* Already done in plat_irq_dispatch()! */
}

/*
 * Route interrupts to ISR(s).
 *
 * This function is entered with the IE disabled. It can be
 * re-entered as soon as the IE is re-enabled in function
 * handle_IRQ_envet().
 */
void BCMFASTPATH
plat_irq_dispatch(struct pt_regs *regs)
{
	u32 pending, ipvec;
	unsigned long flags = 0;
	int irq;

	/* Disable MIPS IRQs with pending interrupts */
	pending = read_c0_cause() & CAUSEF_IP;
	pending &= read_c0_status();
	clear_c0_status(pending);
	irq_disable_hazard();

	/* Handle MIPS timer interrupt. Re-enable MIPS IRQ7
	 * immediately after servicing the interrupt so that
	 * we can take this kind of interrupt again later
	 * while servicing other interrupts.
	 */
	if (pending & CAUSEF_IP7) {
		do_IRQ(7);
		pending &= ~CAUSEF_IP7;
		set_c0_status(STATUSF_IP7);
		irq_enable_hazard();
	}

	/* Build bitvec for pending interrupts. Start with
	 * MIPS IRQ2 and add linux IRQs to higher bits to
	 * make the interrupt processing uniform.
	 */
	ipvec = pending >> CAUSEB_IP2;
	if (pending & CAUSEF_IP2) {
		if (ccsbr)
			flags = R_REG(NULL, &ccsbr->sbflagst);

		/* Read intstatus */
		if (mips_corereg)
			flags = R_REG(NULL, &((mips74kregs_t *)mips_corereg)->intstatus);

		flags &= shints;
		ipvec |= flags << SBMIPS_VIRTIRQ_BASE;
	}

#ifdef CONFIG_HND_BMIPS3300_PROF
	/* Handle MIPS core interrupt. Re-enable the MIPS IRQ that
	 * MIPS core is assigned to immediately after servicing the
	 * interrupt so that we can take this kind of interrupt again
	 * later while servicing other interrupts.
	 *
	 * mipsirq < 0 indicates MIPS core IRQ # is unknown.
	 */
	if (mipsirq >= 0 && (ipvec & (1 << mipsirq))) {
		/* MIPS core raised the interrupt on the shared MIPS IRQ2.
		 * Make sure MIPS core is the only interrupt source before
		 * re-enabling the IRQ.
		 */
		if (mipsirq >= SBMIPS_VIRTIRQ_BASE) {
			if (flags == (1 << (mipsirq-SBMIPS_VIRTIRQ_BASE))) {
				irq = mipsirq + 2;
				do_IRQ(irq);
				ipvec &= ~(1 << mipsirq);
				pending &= ~CAUSEF_IP2;
				set_c0_status(STATUSF_IP2);
				irq_enable_hazard();
			}
		}
		/* MIPS core raised the interrupt on a dedicated MIPS IRQ.
		 * Re-enable the IRQ immediately.
		 */
		else {
			irq = mipsirq + 2;
			do_IRQ(irq);
			ipvec &= ~(1 << mipsirq);
			pending &= ~CR_IP(irq);
			set_c0_status(SR_IM(irq));
			irq_enable_hazard();
		}
	}
#endif	/* CONFIG_HND_BMIPS3300_PROF */

        /* Shared interrupt bits are shifted to respective bit positions in
	 * ipvec above. IP2 (bit 0) is of no significance, hence shifting the
	 * bit map by 1 to the right.
	 */
	ipvec >>= 1;

	/* Handle all other interrupts. Re-enable disabled MIPS IRQs
	 * after processing all pending interrupts.
	 */
	for (irq = 3; ipvec != 0; irq++) {
		if (ipvec & 1)
			do_IRQ(irq);
		ipvec >>= 1;
	}
	set_c0_status(pending);
	irq_enable_hazard();

	/* Process any pending softirqs (tasklets, softirqs ...) */
	local_irq_save(flags);
	if (local_softirq_pending() && !in_interrupt())
		__do_softirq();
	local_irq_restore(flags);
}

/* MIPS IRQ0 to IRQ7 interrupt controller */
static struct irq_chip brcm_irq_type = {
	.name = "MIPS",
	.ack = ack_brcm_irq,
	.mask = disable_brcm_irq,
	.mask_ack = disable_brcm_irq,
	.unmask = enable_brcm_irq,
	.end = end_brcm_irq
};

/* linux IRQ8 and above interrupt controller */
static struct irq_chip brcm_irq2_type = {
	.name = "IRQ2",
	.ack = ack_brcm_irq2,
	.mask = disable_brcm_irq2,
	.mask_ack = disable_brcm_irq2,
	.unmask = enable_brcm_irq2,
	.end = end_brcm_irq2
};

/*
 * We utilize chipcommon configuration register SBFlagSt to implement a
 * smart shared IRQ handling machenism through which only ISRs registered
 * for the SB cores that raised the interrupt are invoked. This machenism
 * relies on the SBFlagSt register's reliable recording of the SB cores
 * that raised the interrupt.
 */
void __init
arch_init_irq(void)
{
	int i;
	uint32 coreidx, mips_core_id;
	void *regs;

	if (BCM330X(current_cpu_data.processor_id))
		mips_core_id = MIPS33_CORE_ID;
	else if (MIPS74K(current_cpu_data.processor_id))
		mips_core_id = MIPS74K_CORE_ID;
	else {
		printk(KERN_ERR "MIPS CPU type %x unknown", current_cpu_data.processor_id);
		return;
	}

	/* Cache chipc and mips33 config registers */
	ASSERT(bcm947xx_sih);
	coreidx = si_coreidx(bcm947xx_sih);
	regs = si_setcore(bcm947xx_sih, mips_core_id, 0);
	mipsirq = si_irq(bcm947xx_sih);
	if (bcm947xx_sih->socitype == SOCI_SB) {
		if (regs)
			mipssbr = (sbconfig_t *)((ulong)regs + SBCONFIGOFF);

		if ((regs = si_setcore(bcm947xx_sih, CC_CORE_ID, 0)))
			ccsbr = (sbconfig_t *)((ulong)regs + SBCONFIGOFF);
	}
	si_setcoreidx(bcm947xx_sih, coreidx);

	if (BCM330X(current_cpu_data.processor_id)) {
		/* Cache mips33 sbintvec register */
		if (mipssbr)
			shints = R_REG(NULL, &mipssbr->sbintvec);
	} else {
		uint32 *intmask;

		/* Use intmask5 register to route the timer interrupt */
		intmask = (uint32 *) &((mips74kregs_t *)regs)->intmask[5];
		W_REG(NULL, intmask, 1 << 31);

		intmask = (uint32 *) &((mips74kregs_t *)regs)->intmask[0];
		shints = R_REG(NULL, intmask);

		/* Save the pointer to mips core registers */
		mips_corereg = regs;
	}

	/* Install interrupt controllers */
	for (i = 0; i < NR_IRQS; i++) {
		set_irq_chip(i, (i < SBMIPS_NUMIRQS ? &brcm_irq_type : &brcm_irq2_type));
	}
}
