/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2002, 2004, 2007 by Ralf Baechle
 * Copyright (C) 2007  Maciej W. Rozycki
 */
#ifndef _ASM_WAR_H
#define _ASM_WAR_H

#include <war.h>

#ifdef CONFIG_CPU_R4000_WORKAROUNDS
#define R4000_WAR 1
#else
#define R4000_WAR 0
#endif

#ifdef CONFIG_CPU_R4400_WORKAROUNDS
#define R4400_WAR 1
#else
#define R4400_WAR 0
#endif

#ifdef CONFIG_CPU_DADDI_WORKAROUNDS
#define DADDI_WAR 1
#else
#define DADDI_WAR 0
#endif

/*
 * Another R4600 erratum.  Due to the lack of errata information the exact
 * technical details aren't known.  I've experimentally found that disabling
 * interrupts during indexed I-cache flushes seems to be sufficient to deal
 * with the issue.
 */
#ifndef R4600_V1_INDEX_ICACHEOP_WAR
#error Check setting of R4600_V1_INDEX_ICACHEOP_WAR for your platform
#endif

/*
 * Pleasures of the R4600 V1.x.  Cite from the IDT R4600 V1.7 errata:
 *
 *  18. The CACHE instructions Hit_Writeback_Invalidate_D, Hit_Writeback_D,
 *      Hit_Invalidate_D and Create_Dirty_Excl_D should only be
 *      executed if there is no other dcache activity. If the dcache is
 *      accessed for another instruction immeidately preceding when these
 *      cache instructions are executing, it is possible that the dcache
 *      tag match outputs used by these cache instructions will be
 *      incorrect. These cache instructions should be preceded by at least
 *      four instructions that are not any kind of load or store
 *      instruction.
 *
 *      This is not allowed:    lw
 *                              nop
 *                              nop
 *                              nop
 *                              cache       Hit_Writeback_Invalidate_D
 *
 *      This is allowed:        lw
 *                              nop
 *                              nop
 *                              nop
 *                              nop
 *                              cache       Hit_Writeback_Invalidate_D
 */
#ifndef R4600_V1_HIT_CACHEOP_WAR
#error Check setting of R4600_V1_HIT_CACHEOP_WAR for your platform
#endif


/*
 * Writeback and invalidate the primary cache dcache before DMA.
 *
 * R4600 v2.0 bug: "The CACHE instructions Hit_Writeback_Inv_D,
 * Hit_Writeback_D, Hit_Invalidate_D and Create_Dirty_Exclusive_D will only
 * operate correctly if the internal data cache refill buffer is empty.  These
 * CACHE instructions should be separated from any potential data cache miss
 * by a load instruction to an uncached address to empty the response buffer."
 * (Revision 2.0 device errata from IDT available on http://www.idt.com/
 * in .pdf format.)
 */
#ifndef R4600_V2_HIT_CACHEOP_WAR
#error Check setting of R4600_V2_HIT_CACHEOP_WAR for your platform
#endif

#ifndef R5432_CP0_INTERRUPT_WAR
#error Check setting of R5432_CP0_INTERRUPT_WAR for your platform
#endif

#ifndef BCM1250_M3_WAR
#error Check setting of BCM1250_M3_WAR for your platform
#endif

#ifndef SIBYTE_1956_WAR
#error Check setting of SIBYTE_1956_WAR for your platform
#endif

#ifndef MIPS4K_ICACHE_REFILL_WAR
#error Check setting of MIPS4K_ICACHE_REFILL_WAR for your platform
#endif

#ifndef MIPS_CACHE_SYNC_WAR
#error Check setting of MIPS_CACHE_SYNC_WAR for your platform
#endif

#ifndef TX49XX_ICACHE_INDEX_INV_WAR
#error Check setting of TX49XX_ICACHE_INDEX_INV_WAR for your platform
#endif

/*
 * On the RM9000 there is a problem which makes the CreateDirtyExclusive
 * eache operation unusable on SMP systems.
 */
#ifndef RM9000_CDEX_SMP_WAR
#error Check setting of RM9000_CDEX_SMP_WAR for your platform
#endif

/*
 * The RM7000 processors and the E9000 cores have a bug (though PMC-Sierra
 * opposes it being called that) where invalid instructions in the same
 * I-cache line worth of instructions being fetched may case spurious
 * exceptions.
 */
#ifndef ICACHE_REFILLS_WORKAROUND_WAR
#error Check setting of ICACHE_REFILLS_WORKAROUND_WAR for your platform
#endif

/*
 * On the R10000 upto version 2.6 (not sure about 2.7) there is a bug that
 * may cause ll / sc and lld / scd sequences to execute non-atomically.
 */
#ifndef R10000_LLSC_WAR
#error Check setting of R10000_LLSC_WAR for your platform
#endif

/*
 * 34K core erratum: "Problems Executing the TLBR Instruction"
 */
#ifndef MIPS34K_MISSED_ITLB_WAR
#error Check setting of MIPS34K_MISSED_ITLB_WAR for your platform
#endif

#endif /* _ASM_WAR_H */
