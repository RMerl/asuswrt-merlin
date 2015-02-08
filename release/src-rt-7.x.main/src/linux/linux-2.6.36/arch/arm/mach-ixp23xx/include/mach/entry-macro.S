/*
 * arch/arm/mach-ixp23xx/include/mach/entry-macro.S
 */

		.macro	disable_fiq
		.endm

		.macro  get_irqnr_preamble, base, tmp
		.endm

		.macro  arch_ret_to_user, tmp1, tmp2
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\irqnr, =(IXP23XX_INTC_VIRT + IXP23XX_INTR_IRQ_ENC_ST_OFFSET)
		ldr	\irqnr, [\irqnr]	@ get interrupt number
		cmp	\irqnr, #0x0		@ spurious interrupt ?
		movne	\irqnr, \irqnr, lsr #2	@ skip unwanted low order bits
		subne	\irqnr, \irqnr, #1	@ convert to 0 based

		.endm
