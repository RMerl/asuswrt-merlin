/*
 * swift.S: MicroSparc-II mmu/cache operations.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 */

#include <asm/psr.h>
#include <asm/asi.h>
#include <asm/page.h>
#include <asm/pgtsrmmu.h>
#include <asm/asm-offsets.h>

	.text
	.align	4

	.globl	swift_flush_cache_all, swift_flush_cache_mm
	.globl	swift_flush_cache_range, swift_flush_cache_page
	.globl	swift_flush_page_for_dma
	.globl	swift_flush_page_to_ram

swift_flush_cache_all:
swift_flush_cache_mm:
swift_flush_cache_range:
swift_flush_cache_page:
swift_flush_page_for_dma:
swift_flush_page_to_ram:
	sethi	%hi(0x2000), %o0
1:	subcc	%o0, 0x10, %o0
	add	%o0, %o0, %o1
	sta	%g0, [%o0] ASI_M_DATAC_TAG
	bne	1b
	 sta	%g0, [%o1] ASI_M_TXTC_TAG
	retl
	 nop

	.globl	swift_flush_sig_insns
swift_flush_sig_insns:
	flush	%o1
	retl
	 flush	%o1 + 4

	.globl	swift_flush_tlb_mm
	.globl	swift_flush_tlb_range
	.globl	swift_flush_tlb_all
swift_flush_tlb_range:
	ld	[%o0 + 0x00], %o0
swift_flush_tlb_mm:
	ld	[%o0 + AOFF_mm_context], %g2
	cmp	%g2, -1
	be	swift_flush_tlb_all_out
swift_flush_tlb_all:
	mov	0x400, %o1
	sta	%g0, [%o1] ASI_M_FLUSH_PROBE
swift_flush_tlb_all_out:
	retl
	 nop

	.globl	swift_flush_tlb_page
swift_flush_tlb_page:
	ld	[%o0 + 0x00], %o0
	mov	SRMMU_CTX_REG, %g1
	ld	[%o0 + AOFF_mm_context], %o3
	andn	%o1, (PAGE_SIZE - 1), %o1
	cmp	%o3, -1
	be	swift_flush_tlb_page_out
	 nop
	mov	0x400, %o1
	sta	%g0, [%o1] ASI_M_FLUSH_PROBE	
swift_flush_tlb_page_out:
	retl
	 nop
