/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 *  linux/arch/arm/kernel/head.S
 *
 *  Copyright (C) 1994-2002 Russell King
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Kernel startup code for all 32-bit CPUs
 */
#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/domain.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/memory.h>
#include <asm/thread_info.h>
#include <asm/system.h>

#if (CONFIG_DRAM_BASE & 0x001fffff)
#error "CONFIG_DRAM_BASE must be at an even 2MiB boundary!"
#endif

#define KERNEL_RAM_VADDR	(PAGE_OFFSET + TEXT_OFFSET)
#define KERNEL_RAM_PADDR	(CONFIG_DRAM_BASE + TEXT_OFFSET)


/*
 * swapper_pg_dir is the virtual address of the initial page table.
 * We place the page tables 16K below KERNEL_RAM_VADDR.  Therefore, we must
 * make sure that KERNEL_RAM_VADDR is correctly set.  Currently, we expect
 * the least significant 16 bits to be 0x8000, but we could probably
 * relax this restriction to KERNEL_RAM_VADDR >= PAGE_OFFSET + 0x4000.
 */
#if (KERNEL_RAM_VADDR & 0xffff) != 0x8000
#error KERNEL_RAM_VADDR must start at 0xXXXX8000
#endif

	.globl	swapper_pg_dir
	.equ	swapper_pg_dir, KERNEL_RAM_VADDR - 0x4000
	.macro	get_ddr_phys_offset_pa, rd, rs
	ldr	\rd, =ddr_phys_offset_va
	ldr	\rs, =PAGE_OFFSET
	sub	\rd, \rd, \rs
	ldr	\rs, =CONFIG_DRAM_BASE
	add	\rd, \rd, \rs
	.endm

	.macro	get_ddr_pgtbl, rd, rs
	mov	\rd, \rs
	ldr	\rd, [\rd, #0x0]
	ldr	\rs, =TEXT_OFFSET
	add	\rd, \rd, \rs
	sub	\rd, \rd, #0x4000
	.endm

	.macro	get_ddr_phys_offset, rd, rs
	mov	\rd, \rs
	ldr	\rd, [\rd, #0x0]
	.endm

	.macro	get_coherence_flag_pa, rd, rs
	ldr	\rd, =coherence_flag
	ldr	\rs, =PAGE_OFFSET
	sub	\rd, \rd, \rs
	ldr	\rs, =CONFIG_DRAM_BASE
	add	\rd, \rd, \rs
	.endm

	.macro	pgtbl, rd
	ldr	\rd, =(KERNEL_RAM_PADDR - 0x4000)
	.endm

#ifdef CONFIG_XIP_KERNEL
#define KERNEL_START	XIP_VIRT_ADDR(CONFIG_XIP_PHYS_ADDR)
#define KERNEL_END	_edata_loc
#else
#define KERNEL_START	KERNEL_RAM_VADDR
#define KERNEL_END	_end
#endif

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code.  The requirements
 * are: MMU = off, D-cache = off, I-cache = dont care, r0 = 0,
 * r1 = machine nr, r2 = atags pointer.
 *
 * This code is mostly position independent, so if you link the kernel at
 * 0xc0008000, you call this at __pa(0xc0008000).
 *
 * See linux/arch/arm/tools/mach-types for the complete list of machine
 * numbers for r1.
 *
 * We're trying to keep crap to a minimum; DO NOT add any machine specific
 * crap here - that's what the boot loader (or in extreme, well justified
 * circumstances, zImage) is for.
 */
	__HEAD
ENTRY(stext)
	setmode	PSR_F_BIT | PSR_I_BIT | SVC_MODE, r9 @ ensure svc mode
						@ and irqs disabled

	ldr	r3, =0x18000000			@ SI_ENUM_BASE
	ldr	r4, =0				@ CC_CHIPID
	ldr	r4, [r3, r4]
	/* Compare Chip ID */
	ldr	r3, =0x0000ffff			@ CID_ID_MASK
	and	r5, r4, r3
	ldr	r3, =53010			@ BCM4707_CHIP_ID
	cmp	r5, r3				@ r5 to store chip id
	beq	1f
	ldr	r3, =53030			@ BCM47094_CHIP_ID
	cmp	r5, r3
	bne	check_ace			@ Not NS chips
	b	bx_acp
1:
	/* Compare Chip Rev */
	ldr	r3, =0x000f0000			@ CID_REV_MASK
	and	r4, r4, r3
	cmp	r4, #0				@ The revision number of NS-Ax is 0.
	bne	bx_acp
ax_acp:
	/* A0 and A1 don't support ACP */
	ldr	r3, =0x1800702c			@ CCB_SRAB_CMDSTAT
	ldr	r4, =0x02400001			@ SPI page 0x02, SPI offset 0x40
	str	r4, [r3]
	ldr	r3, =0x1800703c			@ CCB_SRAB_RDL
	ldr	r4, [r3]
	ldr	r3, =0xff			@ 8-bits CHIP_REVID mask
	and	r4, r4, r3
	cmp	r4, #1				@ REVID: A0 = 0, A1 = 1
	ble	no_acp

	/* A2 and later are able to support ACP */
	get_coherence_flag_pa r6, r7		@ r6 to store coherence_flag
	mov	r7, #COHERENCE_ACP_WAR		@ set coherence_flag = 1
	str	r7, [r6]			@ for NS-Ax with ACP WAR

	ldr	r3, =tgt_phys
	ldr	r4, =0xc0000000			@ Clear the upper two bits
	bic	r3, r3, r4
	ldr	r4, =PADDR_ACP_AX		@ NS-Ax ACP starting address
	orr	r3, r3, r4
	mov	pc, r3				@ PC=0x80008000 for NS-Ax ACP
	b	tgt_phys
bx_acp:
	get_coherence_flag_pa r6, r7		@ r6 to store coherence_flag
	mov	r7, #COHERENCE_ACP		@ set coherence_flag = 2
	str	r7, [r6]			@ arch is coherent

	ldr	r3, =tgt_phys
	ldr	r4, =0xc0000000			@ Clear the upper two bits
	bic	r3, r3, r4
	ldr	r4, =PADDR_ACP_BX		@ NS-Bx ACP starting address
	orr	r3, r3, r4
	mov	pc, r3				@ PC=0x40008000 for NS-Bx ACP
	b	tgt_phys
check_ace:
	ldr	r3, =53573			@ BCM53573_CHIP_ID
	cmp	r5, r3				@ r5 to store chip id
	beq     1f                              @ It's BCM53573
	ldr     r3, =53574                      @ BCM53574_CHIP_ID
	cmp     r5, r3                          @ r5 to store chip id
	bne	no_acp				@ Neither BCM53573 nor 53574
1:
	ldr     r3, =ddr_phys_offset2_va
	ldr     r0, =PAGE_OFFSET
	sub     r3, r3, r0
	ldr     r0, =CONFIG_DRAM_BASE
	add     r3, r3, r0
	ldr     r0, =0x08000000                 @ phys_offset2 for BCM53573
	str     r0, [r3]

	get_coherence_flag_pa r6, r7		@ r6 to store coherence_flag
	mov	r7, #COHERENCE_ACP_WAR		@ set coherence_flag = 1
	str	r7, [r6]			@ arch is coherent for BCM53573

	/* Compare Chip Rev */
	ldr	r3, =0x000f0000			@ CID_REV_MASK
	and	r4, r4, r3

	ldr	r3, =tgt_phys
	ldr	r7, =0xc0000000			@ Clear the upper two bits
	bic	r3, r3, r7
	ldr	r7, =PADDR_ACE_BCM53573		@ BCM53573 ACE starting address
	cmp	r4, #0x20000			@ The revision number of BCM53573 A0 = 0, A1 = 1, B0 = 2.
	bne	2f
	ldr	r7, =PADDR_ACE1_BCM53573        @ Shadow region to PADDR_ACE_BCM53573
2:
	orr	r3, r3, r7
	mov	pc, r3				@ PC=0x80008000 for BCM53573 ACE
	b	tgt_phys

no_acp:
	get_coherence_flag_pa r6, r7		@ r6 to store coherence_flag
	mov	r7, #COHERENCE_NONE		@ set coherence_flag = 0
	str	r7, [r6]			@ arch is non-coherent

	mov	r3, pc				@ PC=0x8000
tgt_phys:
	ldr	r0, =0xf0000000
	and	r3, r3, r0
	get_ddr_phys_offset_pa r6, r7
	str	r3, [r6]			@ r3=0x80000000 for NS-Ax ACP
						@ r3=0x40000000 for NS-Bx ACP
						@ r3=0x0 for Non ACP
	mov	r0, #0				@ restore r0 to 0x0

#ifdef	CONFIG_ARCH_HAS_HEAD_FIXUP
	bl	__mach_head_fixup
#endif
	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
	beq	__error_p			@ yes, error 'p'
	bl	__lookup_machine_type		@ r5=machinfo
	movs	r8, r5				@ invalid machine (r5=0)?
	beq	__error_a			@ yes, error 'a'
	bl	__vet_atags
	bl	__create_page_tables

	/*
	 * The following calls CPU specific code in a position independent
	 * manner.  See arch/arm/mm/proc-*.S for details.  r10 = base of
	 * xxx_proc_info structure selected by __lookup_machine_type
	 * above.  On return, the CPU will be ready for the MMU to be
	 * turned on, and r0 will hold the CPU control register value.
	 */
	ldr	r13, __switch_data		@ address to jump to after
						@ mmu has been enabled
	adr	lr, BSYM(__enable_mmu)		@ return (PIC) address
 ARM(	add	pc, r10, #PROCINFO_INITFUNC	)
 THUMB(	add	r12, r10, #PROCINFO_INITFUNC	)
 THUMB(	mov	pc, r12				)
ENDPROC(stext)

#if defined(CONFIG_SMP)
ENTRY(secondary_startup)
	/*
	 * Common entry point for secondary CPUs.
	 *
	 * Ensure that we're in SVC mode, and IRQs are disabled.  Lookup
	 * the processor type - there is no need to check the machine type
	 * as it has already been validated by the primary processor.
	 */
	setmode	PSR_F_BIT | PSR_I_BIT | SVC_MODE, r9
	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type
	movs	r10, r5				@ invalid processor?
	moveq	r0, #'p'			@ yes, error 'p'
	beq	__error

	/*
	 * Use the page tables supplied from  __cpu_up.
	 */
	adr	r4, __secondary_data
	ldmia	r4, {r5, r7, r12}		@ address to jump to after
	sub	r4, r4, r5			@ mmu has been enabled
	ldr	r4, [r7, r4]			@ get secondary_data.pgdir
	adr	lr, BSYM(__enable_mmu)		@ return address
	mov	r13, r12			@ __secondary_switched address
 ARM(	add	pc, r10, #PROCINFO_INITFUNC	) @ initialise processor
						  @ (return control reg)
 THUMB(	add	r12, r10, #PROCINFO_INITFUNC	)
 THUMB(	mov	pc, r12				)
ENDPROC(secondary_startup)

	/*
	 * r6  = &secondary_data
	 */
ENTRY(__secondary_switched)
	ldr	sp, [r7, #4]			@ get secondary_data.stack
	mov	fp, #0
	b	secondary_start_kernel
ENDPROC(__secondary_switched)

	.type	__secondary_data, %object
__secondary_data:
	.long	.
	.long	secondary_data
	.long	__secondary_switched
#endif /* defined(CONFIG_SMP) */



/*
 * Setup common bits before finally enabling the MMU.  Essentially
 * this is just loading the page table pointer and domain access
 * registers.
 */
__enable_mmu:
#ifdef CONFIG_ALIGNMENT_TRAP
	orr	r0, r0, #CR_A
#else
	bic	r0, r0, #CR_A
#endif
#ifdef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CR_C
#endif
#ifdef CONFIG_CPU_BPREDICT_DISABLE
	bic	r0, r0, #CR_Z
#endif
#ifdef CONFIG_CPU_ICACHE_DISABLE
	bic	r0, r0, #CR_I
#endif
	mov	r5, #(domain_val(DOMAIN_USER, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_TABLE, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_IO, DOMAIN_CLIENT))
	mcr	p15, 0, r5, c3, c0, 0		@ load domain access register
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
	b	__turn_mmu_on
ENDPROC(__enable_mmu)

/*
 * Enable the MMU.  This completely changes the structure of the visible
 * memory space.  You will not be able to trace execution through this.
 * If you have an enquiry about this, *please* check the linux-arm-kernel
 * mailing list archives BEFORE sending another post to the list.
 *
 *  r0  = cp#15 control register
 *  r13 = *virtual* address to jump to upon completion
 *
 * other registers depend on the function called upon completion
 */
	.align	5
__turn_mmu_on:
	mov	r0, r0
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
	mrc	p15, 0, r3, c0, c0, 0		@ read id reg
	mov	r3, r3
	mov	r3, r13
	mov	pc, r3
ENDPROC(__turn_mmu_on)


/*
 * Setup the initial page tables.  We only setup the barest
 * amount which are required to get the kernel running, which
 * generally means mapping in the kernel code.
 *
 * r8  = machinfo
 * r9  = cpuid
 * r10 = procinfo
 *
 * Returns:
 *  r0, r3, r6, r7 corrupted
 *  r4 = physical page table address
 */
__create_page_tables:
	get_ddr_phys_offset_pa r0, r4
	get_ddr_pgtbl r4, r0			@ page table address

	/*
	 * Clear the 16K level 1 swapper page table
	 */
	mov	r0, r4
	mov	r3, #0
	add	r6, r0, #0x4000
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	teq	r0, r6
	bne	1b

	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] @ mm_mmuflags

	/*
	 * Create identity mapping for first MB of kernel to
	 * cater for the MMU enable.  This identity mapping
	 * will be removed by paging_init().  We use our current program
	 * counter to determine corresponding section base address.
	 */
	mov	r6, pc
	mov	r6, r6, lsr #20			@ start of kernel section
	orr	r3, r7, r6, lsl #20		@ flags + kernel base
	str	r3, [r4, r6, lsl #2]		@ identity mapping

	/*
	 * Now setup the pagetables for our kernel direct
	 * mapped region.
	 */
	add	r0, r4,  #(KERNEL_START & 0xff000000) >> 18
	str	r3, [r0, #(KERNEL_START & 0x00f00000) >> 18]!
	ldr	r6, =(KERNEL_END - 1)
	add	r0, r0, #4
	add	r6, r4, r6, lsr #18
1:	cmp	r0, r6
	add	r3, r3, #1 << 20
	strls	r3, [r0], #4
	bls	1b

#ifdef CONFIG_XIP_KERNEL
	/*
	 * Map some ram to cover our .data and .bss areas.
	 */
	orr	r3, r7, #(KERNEL_RAM_PADDR & 0xff000000)
	.if	(KERNEL_RAM_PADDR & 0x00f00000)
	orr	r3, r3, #(KERNEL_RAM_PADDR & 0x00f00000)
	.endif
	add	r0, r4,  #(KERNEL_RAM_VADDR & 0xff000000) >> 18
	str	r3, [r0, #(KERNEL_RAM_VADDR & 0x00f00000) >> 18]!
	ldr	r6, =(_end - 1)
	add	r0, r0, #4
	add	r6, r4, r6, lsr #18
1:	cmp	r0, r6
	add	r3, r3, #1 << 20
	strls	r3, [r0], #4
	bls	1b
#endif

	/*
	 * Then map first 1MB of ram in case it contains our boot params.
	 */
	add	r0, r4, #PAGE_OFFSET >> 18
	get_ddr_phys_offset_pa r11, r3
	get_ddr_phys_offset r3, r11		@ get DDR base address
	and	r3, r3, #0xff000000
	orr	r6, r7, r3

	get_ddr_phys_offset_pa r11, r3
	get_ddr_phys_offset r3, r11		@ get DDR base address
	and	r3, r3, #0x00f00000
	cmp	r3, #0
	orrgt	r6, r6, r3
	str	r6, [r0]

#ifdef CONFIG_DEBUG_LL
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
	/*
	 * Map in IO space for serial debugging.
	 * This allows debug messages to be output
	 * via a serial console before paging_init.
	 */
	ldr	r3, [r8, #MACHINFO_PGOFFIO]
	add	r0, r4, r3
	rsb	r3, r3, #0x4000			@ PTRS_PER_PGD*sizeof(long)
	cmp	r3, #0x0800			@ limit to 512MB
	movhi	r3, #0x0800
	add	r6, r0, r3
	ldr	r3, [r8, #MACHINFO_PHYSIO]
	orr	r3, r3, r7
1:	str	r3, [r0], #4
	add	r3, r3, #1 << 20
	teq	r0, r6
	bne	1b
#if defined(CONFIG_ARCH_NETWINDER) || defined(CONFIG_ARCH_CATS)
	/*
	 * If we're using the NetWinder or CATS, we also need to map
	 * in the 16550-type serial port for the debug messages
	 */
	add	r0, r4, #0xff000000 >> 18
	orr	r3, r7, #0x7c000000
	str	r3, [r0]
#endif
#ifdef CONFIG_ARCH_RPC
	/*
	 * Map in screen at 0x02000000 & SCREEN2_BASE
	 * Similar reasons here - for debug.  This is
	 * only for Acorn RiscPC architectures.
	 */
	add	r0, r4, #0x02000000 >> 18
	orr	r3, r7, #0x02000000
	str	r3, [r0]
	add	r0, r4, #0xd8000000 >> 18
	str	r3, [r0]
#endif
#endif
	mov	pc, lr
ENDPROC(__create_page_tables)
	.ltorg

#include "head-common.S"
