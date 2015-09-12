/*
 * Platform-specific assembly head
 * intended to perform whatever fixup is needed
 * adter the boot loader.
 * Also includes the secondary entry point for SMP.
 */


#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/system.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <plat/plat-bcm5301x.h>
#include <armca9_core.h>

#ifndef CONFIG_DEBUG_LL
ENTRY(printch)
	mov	pc,lr
ENDPROC(printch)

ENTRY(printhex8)
	mov	pc,lr
ENDPROC(printhex8)
#endif

/*
 * This is called from head.S at the beginning.
 * If the boot monitor provides corect r1, r2 values
 * then these need to be preserved.
 *
 * On return, the requirements are: 
 * MMU = off, D-cache = off, I-cache = dont care, 
 * r0 = 0, r1 = machine nr, r2 = atags pointer, or 0.
 */
	__HEAD
ENTRY(__mach_head_fixup)
	mov	r12,lr		@ save return address
	@ Disable d-chace and MMU for UART to work for sure
	mrc	p15, 0, r0, c1, c0, 0		@ Read SCTLR
	bic	r0, #CR_C|CR_M
	mcr	p15, 0, r0, c1, c0, 0		@ Write SCTLR
	/* Clean up any residule in caches */
	bl	v7_all_dcache_invalidate
	mov	r0, #'.'
	bl	printch
	mov	r0,#0

	ldr	r1, =0x18000000		@ SI_ENUM_BASE
	ldr	r3, =0			@ CC_CHIPID
	ldr	r3, [r1, r3]
	/* Compare Chip Rev */
	ldr	r5, =0x000f0000		@ CID_REV_MASK
	and	r4, r3, r5
	/* Compare Chip ID */
	ldr	r1, =0x0000ffff		@ CID_ID_MASK
	and	r3, r3, r1
	ldr	r1, =53573		@ BCM53573_CHIP_ID
	cmp	r3, r1
	beq     3f                      @ It's BCM53573
	ldr     r1, =53574              @ BCM53574_CHIP_ID
	cmp     r3, r1
	bne	no_cci_snoop		@ Neither BCM53573 nor 53574
3:
	/* Enable CCI400 S4 snoop request for CA7 on BCM53573 */
	ldr	r0, =0x18305000		@ CCI400 S4 snoop control register base for CA7
	ldr	r3, [r0]
	orr	r3, r3, #1		@ Enable snoop request on S4 for CA7
	str	r3, [r0]
	dsb
	isb
	/* Polling until change_pending bit is clear */
	ldr	r0, =0x1830000c		@ Status register base
1:
	ldr	r3, [r0]
	tst	r3, #1
	bne	1b

	ldr	r8, =coherence_flag	@ get VA of coherence_flag
	ldr	r1, =PAGE_OFFSET
	sub	r8, r8, r1
	ldr	r1, =CONFIG_DRAM_BASE
	add	r8, r8, r1		@ PA = VA - PAGE_OFFSET + CONFIG_DRAM_BASE
	ldr	r8, [r8]		@ get real coherence_flag value
	cmp	r8, #0
	beq	2f			@ non-coherence if r8 = 0
	ldr	r0, =0x18010c40		@ Enable access write for GCI control register 11
	ldr	r1, =0xb
	str	r1, [r0]
	ldr	r0, =0x18010e00		@ Enable all coherence bits for all DMA
	ldr	r1, =0xffffffff
	cmp     r4, #0x20000		@ The revision number of BCM53573 A0=0, A1=1, B0=2.
	bne	4f
	ldr	r7, [r0]
	ldr     r0, =0x18010c40         @ Enable access write for GCI control register 11
	ldr     r1, =0xb
	str     r1, [r0]
	ldr     r0, =0x18010e00
	ldr	r1, =0x13ffff		@ DDR shadow region, bit20=1, for BCM53573 B0.
	orr	r1, r7, r1
	ldr	r5, =ddr_phys_offset_va
	ldr	r6, =PAGE_OFFSET
	sub	r5, r5, r6
	ldr	r6, =CONFIG_DRAM_BASE
	add	r5, r5, r6
	ldr	r6, [r5]
	ldr	r3, =PADDR_ACE_BCM53573
	cmp	r6, r3			@ To know DDR shadow region or normal region.
	bne	4f
	ldr	r1, =0xffe3ffff		@ DDR normal region, bit20=0, bit19=0, bit18=0.
	and	r1, r7, r1
4:
	str	r1, [r0]
	b	2f

no_cci_snoop:
	/* For NS only */
	ldr	r3, =L2CC_BASE_PA	@ L2 cache controller, control reg
	str	r0, [r3, #0x100]	@ Disable L2 cache
	ldr	r0, =0xffff
	str	r0, [r3, #0x77c]	@ Invalidate by all Ways
	nop
	mov	r0, #'.'
	bl	printch
	/* fixup MaskROM LUT holding pen */
	ldr	r1, =SOC_ROM_BASE_PA
	ldr	r0, =0xffff002c
	str	r0, [r1, #SOC_ROM_LUT_OFF]
	nop

	/* disable CCU clock gating */
	ldr	r0,=IHOST_PROC_CLK_CORE0_CLKGATE
	ldr	r1,=0x00010303
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_CORE1_CLKGATE
	ldr	r2,=0x00000303
	str	r2,[r0]
	ldr	r0,=IHOST_PROC_CLK_ARM_SWITCH_CLKGATE
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_ARM_PERIPH_CLKGATE
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_APB0_CLKGATE
	str	r1,[r0]
2:
#ifndef	CONFIG_XIP_KERNEL
	/* Copy ourselves to RAM if loaded in wrong address */
	mov	r0, #0xff000000
	orr	r0, #0x00ff0000
	mov	r8, pc
	and	r8, r8, r0
	ldr	r9, =ddr_phys_offset_va
	ldr	r10, =PAGE_OFFSET
	sub	r9, r9, r10
	ldr	r10, =CONFIG_DRAM_BASE
	add	r9, r9, r10
	ldr	r9, [r9, #0x0]
	and	r9, r9, r0
	cmp	r8, r9
	beq	5f
	ldr	r12, =TEXT_OFFSET
	add	r9, r9, r12
	mov	r12, r9			@ start again in RAM
	/* The __bss_start is located at < 128MB address */
	ldr	r10, =__bss_start
	ldr	r3, =PAGE_OFFSET
	sub	r10, r10, r3		@ get offset
	ldr	r3, =ddr_phys_offset_va
	ldr	r2, =PAGE_OFFSET
	sub	r3, r3, r2
	ldr	r2, =CONFIG_DRAM_BASE
	add	r3, r3, r2
	ldr	r3, [r3, #0x0]		@ get real PHYS_OFFSET value
	add	r10, r10, r3

	mov	r0, #'X'
	bl	printch
	mov	r0, r8
        bl      printhex8
	mov	r0, #':'
	bl	printch
	mov	r0, r9
        bl      printhex8
	mov	r0, #'-'
	bl	printch
	mov	r0, r10
        bl      printhex8
	nop

2:	
	ldmia	r8!, { r0 - r7 }
	stmia	r9!, { r0 - r7 }
	mov	r0, r9 
	mov	r1, #0xff
	orr	r1, r1, #0xff00
	tst	r0, r1
	bne	2b
	mov	r0, #'.'
	bl	printch
	cmp	r9, r10
	blo	2b

	mov	r0, #'!'
	bl	printch
	mov	r0, r9
        bl      printhex8
	nop
5:
	mov	r0, #'.'
	bl	printch
	nop
#endif	/* !CONFIG_XIP_KERNEL */

chip_id_rev:
	ldr	r3,=0x18000000
	ldr	r2,[r3]

	ldr	r1, =0xffff
	and	r0, r2, r1
	adr	r7,__mach_head_fixup
	ldr	r8,=__mach_head_fixup
	sub	r7,r7,r8
	ldr	r8,=_chipid
	add	r7,r8,r7
	str	r0,[r7]				/* _chipid */

	ldr	r1, =0xf
	mov	r0, r2, lsr#16
	and	r0, r0, r1
	adr	r7,__mach_head_fixup
	ldr	r8,=__mach_head_fixup
	sub	r7,r7,r8
	ldr	r8,=_chiprev
	add	r7,r8,r7
	str	r0,[r7]				/* _chiprev */

szmem:
	ldr	r3,=0x80000000
	ldr	r2,=0xaa55beef
	ldr	r1,=0x00000000
	ldr	r6,[r3,#0x3c]			/* Save the value in r6 */
	str	r1,[r3,#0x3c]
	ldr	r0,=(1 << 20)
	ldr	r5,=(1 << 30)
1:
	ldr	r4,=0x80000000
	add	r4,r4,r0
	ldr	r7,[r4,#0x3c]			/* Save the value in r7 */
	str	r2,[r4,#0x3c]
	ldr	r1,[r4,#0x3c]			/* Read back to ensure completion */
	ldr	r1,[r3,#0x3c]
	str	r7,[r4,#0x3c]			/* Restore the value from r7 */
	cmp	r1,r2
	beq	2f

	lsl	r0,r0,#1
	cmp	r0,r5
	bne	1b

	/* Didn't find an alias, must be 128MB */
2:
	str	r6,[r3,#0x3c]			/* Restore the value from r6 */
	adr	r7,__mach_head_fixup
	ldr	r8,=__mach_head_fixup
	sub	r7,r7,r8
	ldr	r8,=_memsize
	add	r7,r8,r7
	str	r0,[r7]

	@ Override machine type and atags pointers
	ldr	r1, =(machine_arch_type)
	mov	r0, #0
	mov	r2, r0
	mov	pc, r12		@ return
	nop
ENDPROC(__mach_head_fixup)
	.ltorg

        .global _memsize
        .type   _memsize, %object
        .size   _memsize, 4
        .align  2
_memsize:
	.word   0

        .global _chipid
        .type   _chipid, %object
        .size   _chipid, 4
        .align  2
_chipid:
	.word   0

        .global _chiprev
        .type   _chiprev, %object
        .size   _chiprev, 4
        .align  2
_chiprev:
	.word   0

#ifdef	CONFIG_SMP
/*
 * Platform specific entry point for secondary CPUs.  This
 * provides a "holding pen" into which all secondary cores are held
 * until we're ready for them to initialise.
 */
	__CPUINIT
ENTRY(platform_secondary_startup)
	/*
	 * Get hardware CPU id of ours
	 */
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #15
	/*
	 * Wait on <pen_release> variable by physical address
	 * to contain our hardware CPU id
	 */
	/* The pen_release is located at < 128MB address */
	ldr	r6, =pen_release
	ldr	r3, =PAGE_OFFSET
	sub	r6, r6, r3		@ get offset
	ldr	r3, =ddr_phys_offset_va
	ldr	r2, =PAGE_OFFSET
	sub	r3, r3, r2
	ldr	r2, =CONFIG_DRAM_BASE
	add	r3, r3, r2
	ldr	r3, [r3, #0x0]		@ get real PHYS_OFFSET value
	add	r6, r6, r3

pen:	ldr	r7, [r6]
	cmp	r7, r0
	bne	pen
	nop
	/*
	 * In case L1 cache has unpredictable contents at power-up
	 * clean its contents without flushing.
	 */
	bl	v7_l1_cache_invalidate
	nop
	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup

ENDPROC(platform_secondary_startup)
	.ltorg
#endif	/* CONFIG_SMP */
