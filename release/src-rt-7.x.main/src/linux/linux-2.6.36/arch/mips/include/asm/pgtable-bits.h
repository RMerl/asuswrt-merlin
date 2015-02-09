/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2002 by Ralf Baechle
 * Copyright (C) 1999, 2000, 2001 Silicon Graphics, Inc.
 * Copyright (C) 2002  Maciej W. Rozycki
 */
#ifndef _ASM_PGTABLE_BITS_H
#define _ASM_PGTABLE_BITS_H


/*
 * Note that we shift the lower 32bits of each EntryLo[01] entry
 * 6 bits to the left. That way we can convert the PFN into the
 * physical address by a single 'and' operation and gain 6 additional
 * bits for storing information which isn't present in a normal
 * MIPS page table.
 *
 * Similar to the Alpha port, we need to keep track of the ref
 * and mod bits in software.  We have a software "yeah you can read
 * from this page" bit, and a hardware one which actually lets the
 * process read from the page.  On the same token we have a software
 * writable bit and the real hardware one which actually lets the
 * process write to the page, this keeps a mod bit via the hardware
 * dirty bit.
 *
 * Certain revisions of the R4000 and R5000 have a bug where if a
 * certain sequence occurs in the last 3 instructions of an executable
 * page, and the following page is not mapped, the cpu can do
 * unpredictable things.  The code (when it is written) to deal with
 * this problem will be in the update_mmu_cache() code for the r4k.
 */
#if defined(CONFIG_64BIT_PHYS_ADDR) && defined(CONFIG_CPU_MIPS32)

#define _PAGE_PRESENT               (1<<6)  /* implemented in software */
#define _PAGE_READ                  (1<<7)  /* implemented in software */
#define _PAGE_WRITE                 (1<<8)  /* implemented in software */
#define _PAGE_ACCESSED              (1<<9)  /* implemented in software */
#define _PAGE_MODIFIED              (1<<10) /* implemented in software */
#define _PAGE_FILE                  (1<<10) /* set:pagecache unset:swap */

#define _PAGE_R4KBUG                (1<<0)
#define _PAGE_GLOBAL                (1<<0)
#define _PAGE_VALID                 (1<<1)
#define _PAGE_SILENT_READ           (1<<1)  /* synonym                 */
#define _PAGE_DIRTY                 (1<<2)  /* The MIPS dirty bit      */
#define _PAGE_SILENT_WRITE          (1<<2)
#define _CACHE_SHIFT                3
#define _CACHE_MASK                 (7<<3)

#elif defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)

#define _PAGE_PRESENT               (1<<0)  /* implemented in software */
#define _PAGE_READ                  (1<<1)  /* implemented in software */
#define _PAGE_WRITE                 (1<<2)  /* implemented in software */
#define _PAGE_ACCESSED              (1<<3)  /* implemented in software */
#define _PAGE_MODIFIED              (1<<4)  /* implemented in software */
#define _PAGE_FILE                  (1<<4)  /* set:pagecache unset:swap */

#define _PAGE_GLOBAL                (1<<8)
#define _PAGE_VALID                 (1<<9)
#define _PAGE_SILENT_READ           (1<<9)  /* synonym                 */
#define _PAGE_DIRTY                 (1<<10) /* The MIPS dirty bit      */
#define _PAGE_SILENT_WRITE          (1<<10)
#define _CACHE_UNCACHED             (1<<11)
#define _CACHE_MASK                 (1<<11)

#else /* 'Normal' r4K case */
/*
 * When using the RI/XI bit support, we have 13 bits of flags below
 * the physical address. The RI/XI bits are placed such that a SRL 5
 * can strip off the software bits, then a ROTR 2 can move the RI/XI
 * into bits [63:62]. This also limits physical address to 56 bits,
 * which is more than we need right now.
 */

/* implemented in software */
#define _PAGE_PRESENT_SHIFT	(0)
#define _PAGE_PRESENT		(1 << _PAGE_PRESENT_SHIFT)
/* implemented in software, should be unused if kernel_uses_smartmips_rixi. */
#define _PAGE_READ_SHIFT	(kernel_uses_smartmips_rixi ? _PAGE_PRESENT_SHIFT : _PAGE_PRESENT_SHIFT + 1)
#define _PAGE_READ ({if (kernel_uses_smartmips_rixi) BUG(); 1 << _PAGE_READ_SHIFT; })
/* implemented in software */
#define _PAGE_WRITE_SHIFT	(_PAGE_READ_SHIFT + 1)
#define _PAGE_WRITE		(1 << _PAGE_WRITE_SHIFT)
/* implemented in software */
#define _PAGE_ACCESSED_SHIFT	(_PAGE_WRITE_SHIFT + 1)
#define _PAGE_ACCESSED		(1 << _PAGE_ACCESSED_SHIFT)
/* implemented in software */
#define _PAGE_MODIFIED_SHIFT	(_PAGE_ACCESSED_SHIFT + 1)
#define _PAGE_MODIFIED		(1 << _PAGE_MODIFIED_SHIFT)
/* set:pagecache unset:swap */
#define _PAGE_FILE		(_PAGE_MODIFIED)

#ifdef CONFIG_HUGETLB_PAGE
/* huge tlb page */
#define _PAGE_HUGE_SHIFT	(_PAGE_MODIFIED_SHIFT + 1)
#define _PAGE_HUGE		(1 << _PAGE_HUGE_SHIFT)
#else
#define _PAGE_HUGE_SHIFT	(_PAGE_MODIFIED_SHIFT)
#define _PAGE_HUGE		({BUG(); 1; })  /* Dummy value */
#endif

/* Page cannot be executed */
#define _PAGE_NO_EXEC_SHIFT	(kernel_uses_smartmips_rixi ? _PAGE_HUGE_SHIFT + 1 : _PAGE_HUGE_SHIFT)
#define _PAGE_NO_EXEC		({if (!kernel_uses_smartmips_rixi) BUG(); 1 << _PAGE_NO_EXEC_SHIFT; })

/* Page cannot be read */
#define _PAGE_NO_READ_SHIFT	(kernel_uses_smartmips_rixi ? _PAGE_NO_EXEC_SHIFT + 1 : _PAGE_NO_EXEC_SHIFT)
#define _PAGE_NO_READ		({if (!kernel_uses_smartmips_rixi) BUG(); 1 << _PAGE_NO_READ_SHIFT; })

#define _PAGE_GLOBAL_SHIFT	(_PAGE_NO_READ_SHIFT + 1)
#define _PAGE_GLOBAL		(1 << _PAGE_GLOBAL_SHIFT)

#define _PAGE_VALID_SHIFT	(_PAGE_GLOBAL_SHIFT + 1)
#define _PAGE_VALID		(1 << _PAGE_VALID_SHIFT)
/* synonym                 */
#define _PAGE_SILENT_READ	(_PAGE_VALID)

/* The MIPS dirty bit      */
#define _PAGE_DIRTY_SHIFT	(_PAGE_VALID_SHIFT + 1)
#define _PAGE_DIRTY		(1 << _PAGE_DIRTY_SHIFT)
#define _PAGE_SILENT_WRITE	(_PAGE_DIRTY)

#define _CACHE_SHIFT		(_PAGE_DIRTY_SHIFT + 1)
#define _CACHE_MASK		(7 << _CACHE_SHIFT)

#define _PFN_SHIFT		(PAGE_SHIFT - 12 + _CACHE_SHIFT + 3)

#endif /* defined(CONFIG_64BIT_PHYS_ADDR && defined(CONFIG_CPU_MIPS32) */

#ifndef _PFN_SHIFT
#define _PFN_SHIFT                  PAGE_SHIFT
#endif
#define _PFN_MASK		(~((1 << (_PFN_SHIFT)) - 1))

#ifndef _PAGE_NO_READ
#define _PAGE_NO_READ ({BUG(); 0; })
#define _PAGE_NO_READ_SHIFT ({BUG(); 0; })
#endif
#ifndef _PAGE_NO_EXEC
#define _PAGE_NO_EXEC ({BUG(); 0; })
#endif
#ifndef _PAGE_GLOBAL_SHIFT
#define _PAGE_GLOBAL_SHIFT ilog2(_PAGE_GLOBAL)
#endif


#ifndef __ASSEMBLY__
/*
 * pte_to_entrylo converts a page table entry (PTE) into a Mips
 * entrylo0/1 value.
 */
static inline uint64_t pte_to_entrylo(unsigned long pte_val)
{
	if (kernel_uses_smartmips_rixi) {
		int sa;
#ifdef CONFIG_32BIT
		sa = 31 - _PAGE_NO_READ_SHIFT;
#else
		sa = 63 - _PAGE_NO_READ_SHIFT;
#endif
		/*
		 * C has no way to express that this is a DSRL
		 * _PAGE_NO_EXEC_SHIFT followed by a ROTR 2.  Luckily
		 * in the fast path this is done in assembly
		 */
		return (pte_val >> _PAGE_GLOBAL_SHIFT) |
			((pte_val & (_PAGE_NO_EXEC | _PAGE_NO_READ)) << sa);
	}

	return pte_val >> _PAGE_GLOBAL_SHIFT;
}
#endif

/*
 * Cache attributes
 */
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)

#define _CACHE_CACHABLE_NONCOHERENT 0

#elif defined(CONFIG_CPU_SB1)

/* No penalty for being coherent on the SB1, so just
   use it for "noncoherent" spaces, too.  Shouldn't hurt. */

#define _CACHE_UNCACHED             (2<<_CACHE_SHIFT)
#define _CACHE_CACHABLE_COW         (5<<_CACHE_SHIFT)
#define _CACHE_CACHABLE_NONCOHERENT (5<<_CACHE_SHIFT)
#define _CACHE_UNCACHED_ACCELERATED (7<<_CACHE_SHIFT)

#elif defined(CONFIG_CPU_RM9000)

#define _CACHE_WT		    (0<<_CACHE_SHIFT)
#define _CACHE_WTWA		    (1<<_CACHE_SHIFT)
#define _CACHE_UC_B		    (2<<_CACHE_SHIFT)
#define _CACHE_WB		    (3<<_CACHE_SHIFT)
#define _CACHE_CWBEA		    (4<<_CACHE_SHIFT)
#define _CACHE_CWB		    (5<<_CACHE_SHIFT)
#define _CACHE_UCNB		    (6<<_CACHE_SHIFT)
#define _CACHE_FPC		    (7<<_CACHE_SHIFT)

#define _CACHE_UNCACHED		    _CACHE_UC_B
#define _CACHE_CACHABLE_NONCOHERENT _CACHE_WB

#else

#define _CACHE_CACHABLE_NO_WA	    (0<<_CACHE_SHIFT)  /* R4600 only      */
#define _CACHE_CACHABLE_WA	    (1<<_CACHE_SHIFT)  /* R4600 only      */
#define _CACHE_UNCACHED             (2<<_CACHE_SHIFT)  /* R4[0246]00      */
#define _CACHE_CACHABLE_NONCOHERENT (3<<_CACHE_SHIFT)  /* R4[0246]00      */
#define _CACHE_CACHABLE_CE          (4<<_CACHE_SHIFT)  /* R4[04]00MC only */
#define _CACHE_CACHABLE_COW         (5<<_CACHE_SHIFT)  /* R4[04]00MC only */
#define _CACHE_CACHABLE_COHERENT    (5<<_CACHE_SHIFT)  /* MIPS32R2 CMP    */
#define _CACHE_CACHABLE_CUW         (6<<_CACHE_SHIFT)  /* R4[04]00MC only */
#define _CACHE_UNCACHED_ACCELERATED (7<<_CACHE_SHIFT)  /* R10000 only     */

#endif

#define __READABLE	(_PAGE_SILENT_READ | _PAGE_ACCESSED | (kernel_uses_smartmips_rixi ? 0 : _PAGE_READ))
#define __WRITEABLE	(_PAGE_WRITE | _PAGE_SILENT_WRITE | _PAGE_MODIFIED)

#define _PAGE_CHG_MASK  (_PFN_MASK | _PAGE_ACCESSED | _PAGE_MODIFIED | _CACHE_MASK)

#endif /* _ASM_PGTABLE_BITS_H */
