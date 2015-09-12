/*****************************************************************************
* Copyright 2003 - 20011 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <asm/hardware/gic.h>
#include <plat/mpcore.h>
#include <mach/io_map.h>

	.macro	disable_fiq
	.endm

	/*
	 * This is the interrupt handling part of the GIC code -
	 * the base_va exists in a variable, but here is defined
	 * at compile time for effeciency (?)
	 *
	 * Use this solution first to support both CA9 and CA7.
	 * Let's find if there is any other better way later.
	 */

	.macro  get_irqnr_preamble, base, tmp
	mrc	p15, 0, \tmp, c0, c0, 0		@ Read MIDR
	ldr	\base, =0x0000fff0
	and	\tmp, \tmp, \base		@ Get primary part number
	ldr	\base, =0x0000c070		@ 0xc07 for CA7, 0xc09 for CA9
	cmp	\tmp, \base
	ldreq	\base, =MPCORE_GIC_CPUIF_VA_CA7
	ldrne	\base, =MPCORE_GIC_CPUIF_VA
	.endm

	/*
	 * Interrupts 0-15 are IPI
	 * 16-31 are local
	 * 32-1020 are global
	 * 1021-1022 are reserved
	 * 1023 is "spurious" (no interrupt)
	 *
	 * Spurious interrupt must be ignored in all events.
	 * When in SMP mode, then IPI interrupts must be ignored here,
	 * amd picked up later with the test_for_ipi macro.
	 * When in SMP mode and local timers are enabled,
	 * the private timer/watchdog interrupt must be ignored here
	 * so it can be handled later in test_for_ltirq routine.
	 *
	 * A simple read from the controller will tell us the number of the
	 * highest priority enabled interrupt.  We then just need to check
	 * whether it is in the range that must be handled.
	 *
	 * Upon return, Z=1 tells to ignore this interrupt
	 */

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp

	/* bits 12-10 = src CPU, 9-0 = int # */
	ldr     \irqstat, [\base, #GIC_CPU_INTACK]
	bic     \irqnr, \irqstat, #0x1c00
	ldr	\tmp, =0x3ff		@ spurious irqnum

	/* Ignore spurious interrupt */
	cmp	\irqnr, \tmp

#ifdef CONFIG_SMP
#ifdef CONFIG_LOCAL_TIMERS
	/* Leave private timer handling for later */
	cmpne	\irqnr, #MPCORE_IRQ_LOCALTIMER
	moveq	\tmp, \irqnr
#endif

	/* Leave IPI (irq=0..15) handling for later */
	cmp	\irqnr, #16
	movcc	\tmp, \irqnr
	cmp	\tmp, \irqnr
#endif
	.endm

	.macro	get_irqnr_and_base_ca7, irqnr, irqstat, base, tmp
	/* bits 12-10 = src CPU, 9-0 = int # */
	ldr     \irqstat, [\base, #GIC_CPU_INTACK]
	bic     \irqnr, \irqstat, #0x1c00
	ldr	\tmp, =0x3ff		@ spurious irqnum

	/* Ignore spurious interrupt */
	cmp	\irqnr, \tmp
	.endm

#ifdef CONFIG_SMP
	/* We assume that irqstat (the raw value of the IRQ acknowledge
	 * register) is preserved from the macro above.
	 * If there is an IPI, we immediately signal end of interrupt on the
	 * controller, since this requires the original irqstat value which
	 * we won't easily be able to recreate later.
	 */
	.macro	test_for_ipi, irqnr, irqstat, base, tmp
	bic	\irqnr, \irqstat, #0x1c00
	cmp	\irqnr, #16
	strcc	\irqstat, [\base, #GIC_CPU_EOI]
	cmpcs	\irqnr, \irqnr
	.endm

#ifdef CONFIG_LOCAL_TIMERS
	.macro	test_for_ltirq, irqnr, irqstat, base,tmp
	bic	\irqnr, \irqstat, #0x1c00
	mov	\tmp, #0
	cmp	\irqnr, #MPCORE_IRQ_LOCALTIMER
	moveq	\tmp, #1
	streq	\irqstat, [\base, #GIC_CPU_EOI]
	cmp	\tmp, #0
	.endm
#endif
#endif /* SMP */

	.macro  arch_ret_to_user, tmp1, tmp2
	.endm
