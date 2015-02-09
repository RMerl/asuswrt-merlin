/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_PGTABLE_H
#define _ASM_MICROBLAZE_PGTABLE_H

#include <asm/setup.h>

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)		\
		remap_pfn_range(vma, vaddr, pfn, size, prot)

#ifndef __ASSEMBLY__
extern int mem_init_done;
#endif

#ifndef CONFIG_MMU

#define pgd_present(pgd)	(1) /* pages are always present on non MMU */
#define pgd_none(pgd)		(0)
#define pgd_bad(pgd)		(0)
#define pgd_clear(pgdp)
#define kern_addr_valid(addr)	(1)
#define	pmd_offset(a, b)	((void *) 0)

#define PAGE_NONE		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_SHARED		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_COPY		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_READONLY		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_KERNEL		__pgprot(0) /* these mean nothing to non MMU */

#define pgprot_noncached(x)	(x)

#define __swp_type(x)		(0)
#define __swp_offset(x)		(0)
#define __swp_entry(typ, off)	((swp_entry_t) { ((typ) | ((off) << 7)) })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

#ifndef __ASSEMBLY__
static inline int pte_file(pte_t pte) { return 0; }
#endif /* __ASSEMBLY__ */

#define ZERO_PAGE(vaddr)	({ BUG(); NULL; })

#define swapper_pg_dir ((pgd_t *) NULL)

#define pgtable_cache_init()	do {} while (0)

#define arch_enter_lazy_cpu_mode()	do {} while (0)

#define pgprot_noncached_wc(prot)	prot

#else /* CONFIG_MMU */

#include <asm-generic/4level-fixup.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/sched.h>
#include <linux/threads.h>
#include <asm/processor.h>		/* For TASK_SIZE */
#include <asm/mmu.h>
#include <asm/page.h>

#define FIRST_USER_ADDRESS	0

extern unsigned long va_to_phys(unsigned long address);
extern pte_t *va_to_pte(unsigned long address);

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */

static inline int pte_special(pte_t pte)	{ return 0; }

static inline pte_t pte_mkspecial(pte_t pte)	{ return pte; }

/* Start and end of the vmalloc area. */
/* Make sure to map the vmalloc area above the pinned kernel memory area
   of 32Mb.  */
#define VMALLOC_START	(CONFIG_KERNEL_START + \
				max(32 * 1024 * 1024UL, memory_size))
#define VMALLOC_END	ioremap_bot

#endif /* __ASSEMBLY__ */

/*
 * Macro to mark a page protection value as "uncacheable".
 */

#define _PAGE_CACHE_CTL	(_PAGE_GUARDED | _PAGE_NO_CACHE | \
							_PAGE_WRITETHRU)

#define pgprot_noncached(prot) \
			(__pgprot((pgprot_val(prot) & ~_PAGE_CACHE_CTL) | \
					_PAGE_NO_CACHE | _PAGE_GUARDED))

#define pgprot_noncached_wc(prot) \
			 (__pgprot((pgprot_val(prot) & ~_PAGE_CACHE_CTL) | \
							_PAGE_NO_CACHE))

/*
 * The MicroBlaze MMU is identical to the PPC-40x MMU, and uses a hash
 * table containing PTEs, together with a set of 16 segment registers, to
 * define the virtual to physical address mapping.
 *
 * We use the hash table as an extended TLB, i.e. a cache of currently
 * active mappings.  We maintain a two-level page table tree, much
 * like that used by the i386, for the sake of the Linux memory
 * management code.  Low-level assembler code in hashtable.S
 * (procedure hash_page) is responsible for extracting ptes from the
 * tree and putting them into the hash table when necessary, and
 * updating the accessed and modified bits in the page table tree.
 */

/*
 * The MicroBlaze processor has a TLB architecture identical to PPC-40x. The
 * instruction and data sides share a unified, 64-entry, semi-associative
 * TLB which is maintained totally under software control. In addition, the
 * instruction side has a hardware-managed, 2,4, or 8-entry, fully-associative
 * TLB which serves as a first level to the shared TLB. These two TLBs are
 * known as the UTLB and ITLB, respectively (see "mmu.h" for definitions).
 */

/*
 * The normal case is that PTEs are 32-bits and we have a 1-page
 * 1024-entry pgdir pointing to 1-page 1024-entry PTE pages.  -- paulus
 *
 */

/* PMD_SHIFT determines the size of the area mapped by the PTE pages */
#define PMD_SHIFT	(PAGE_SHIFT + PTE_SHIFT)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a top-level page table entry can map */
#define PGDIR_SHIFT	PMD_SHIFT
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/*
 * entries per page directory level: our page-table tree is two-level, so
 * we don't really have any PMD directory.
 */
#define PTRS_PER_PTE	(1 << PTE_SHIFT)
#define PTRS_PER_PMD	1
#define PTRS_PER_PGD	(1 << (32 - PGDIR_SHIFT))

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#define USER_PGD_PTRS (PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD-USER_PGD_PTRS)

#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte "PTE_FMT".\n", \
		__FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pmd %08lx.\n", \
		__FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", \
		__FILE__, __LINE__, pgd_val(e))

/*
 * Bits in a linux-style PTE.  These match the bits in the
 * (hardware-defined) PTE as closely as possible.
 */

/* There are several potential gotchas here.  The hardware TLBLO
 * field looks like this:
 *
 * 0  1  2  3  4  ... 18 19 20 21 22 23 24 25 26 27 28 29 30 31
 * RPN.....................  0  0 EX WR ZSEL.......  W  I  M  G
 *
 * Where possible we make the Linux PTE bits match up with this
 *
 * - bits 20 and 21 must be cleared, because we use 4k pages (4xx can
 * support down to 1k pages), this is done in the TLBMiss exception
 * handler.
 * - We use only zones 0 (for kernel pages) and 1 (for user pages)
 * of the 16 available.  Bit 24-26 of the TLB are cleared in the TLB
 * miss handler.  Bit 27 is PAGE_USER, thus selecting the correct
 * zone.
 * - PRESENT *must* be in the bottom two bits because swap cache
 * entries use the top 30 bits.  Because 4xx doesn't support SMP
 * anyway, M is irrelevant so we borrow it for PAGE_PRESENT.  Bit 30
 * is cleared in the TLB miss handler before the TLB entry is loaded.
 * - All other bits of the PTE are loaded into TLBLO without
 *  * modification, leaving us only the bits 20, 21, 24, 25, 26, 30 for
 * software PTE bits.  We actually use use bits 21, 24, 25, and
 * 30 respectively for the software bits: ACCESSED, DIRTY, RW, and
 * PRESENT.
 */

/* Definitions for MicroBlaze. */
#define	_PAGE_GUARDED	0x001	/* G: page is guarded from prefetch */
#define _PAGE_FILE	0x001	/* when !present: nonlinear file mapping */
#define _PAGE_PRESENT	0x002	/* software: PTE contains a translation */
#define	_PAGE_NO_CACHE	0x004	/* I: caching is inhibited */
#define	_PAGE_WRITETHRU	0x008	/* W: caching is write-through */
#define	_PAGE_USER	0x010	/* matches one of the zone permission bits */
#define	_PAGE_RW	0x040	/* software: Writes permitted */
#define	_PAGE_DIRTY	0x080	/* software: dirty page */
#define _PAGE_HWWRITE	0x100	/* hardware: Dirty & RW, set in exception */
#define _PAGE_HWEXEC	0x200	/* hardware: EX permission */
#define _PAGE_ACCESSED	0x400	/* software: R: page referenced */
#define _PMD_PRESENT	PAGE_MASK

/*
 * Some bits are unused...
 */
#ifndef _PAGE_HASHPTE
#define _PAGE_HASHPTE	0
#endif
#ifndef _PTE_NONE_MASK
#define _PTE_NONE_MASK	0
#endif
#ifndef _PAGE_SHARED
#define _PAGE_SHARED	0
#endif
#ifndef _PAGE_HWWRITE
#define _PAGE_HWWRITE	0
#endif
#ifndef _PAGE_HWEXEC
#define _PAGE_HWEXEC	0
#endif
#ifndef _PAGE_EXEC
#define _PAGE_EXEC	0
#endif

#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

/*
 * Note: the _PAGE_COHERENT bit automatically gets set in the hardware
 * PTE if CONFIG_SMP is defined (hash_page does this); there is no need
 * to have it in the Linux PTE, and in fact the bit could be reused for
 * another purpose.  -- paulus.
 */
#define _PAGE_BASE	(_PAGE_PRESENT | _PAGE_ACCESSED)
#define _PAGE_WRENABLE	(_PAGE_RW | _PAGE_DIRTY | _PAGE_HWWRITE)

#define _PAGE_KERNEL \
	(_PAGE_BASE | _PAGE_WRENABLE | _PAGE_SHARED | _PAGE_HWEXEC)

#define _PAGE_IO	(_PAGE_KERNEL | _PAGE_NO_CACHE | _PAGE_GUARDED)

#define PAGE_NONE	__pgprot(_PAGE_BASE)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_SHARED	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW)
#define PAGE_SHARED_X \
		__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)

#define PAGE_KERNEL	__pgprot(_PAGE_KERNEL)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_BASE | _PAGE_SHARED)
#define PAGE_KERNEL_CI	__pgprot(_PAGE_IO)

/*
 * We consider execute permission the same as read.
 * Also, write permissions imply read permissions.
 */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY_X
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY_X
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY_X
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY_X

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY_X
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED_X
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY_X
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED_X

#ifndef __ASSEMBLY__
/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern unsigned long empty_zero_page[1024];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#endif /* __ASSEMBLY__ */

#define pte_none(pte)		((pte_val(pte) & ~_PTE_NONE_MASK) == 0)
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)
#define pte_clear(mm, addr, ptep) \
	do { set_pte_at((mm), (addr), (ptep), __pte(0)); } while (0)

#define pmd_none(pmd)		(!pmd_val(pmd))
#define	pmd_bad(pmd)		((pmd_val(pmd) & _PMD_PRESENT) == 0)
#define	pmd_present(pmd)	((pmd_val(pmd) & _PMD_PRESENT) != 0)
#define	pmd_clear(pmdp)		do { pmd_val(*(pmdp)) = 0; } while (0)

#define pte_page(x)		(mem_map + (unsigned long) \
				((pte_val(x) - memory_start) >> PAGE_SHIFT))
#define PFN_SHIFT_OFFSET	(PAGE_SHIFT)

#define pte_pfn(x)		(pte_val(x) >> PFN_SHIFT_OFFSET)

#define pfn_pte(pfn, prot) \
	__pte(((pte_basic_t)(pfn) << PFN_SHIFT_OFFSET) | pgprot_val(prot))

#ifndef __ASSEMBLY__
/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }
#define pgd_clear(xp)				do { } while (0)
#define pgd_page(pgd) \
	((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)  { return pte_val(pte) & _PAGE_USER; }
static inline int pte_write(pte_t pte) { return pte_val(pte) & _PAGE_RW; }
static inline int pte_exec(pte_t pte)  { return pte_val(pte) & _PAGE_EXEC; }
static inline int pte_dirty(pte_t pte) { return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte) { return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_file(pte_t pte)  { return pte_val(pte) & _PAGE_FILE; }

static inline void pte_uncache(pte_t pte) { pte_val(pte) |= _PAGE_NO_CACHE; }
static inline void pte_cache(pte_t pte)   { pte_val(pte) &= ~_PAGE_NO_CACHE; }

static inline pte_t pte_rdprotect(pte_t pte) \
		{ pte_val(pte) &= ~_PAGE_USER; return pte; }
static inline pte_t pte_wrprotect(pte_t pte) \
	{ pte_val(pte) &= ~(_PAGE_RW | _PAGE_HWWRITE); return pte; }
static inline pte_t pte_exprotect(pte_t pte) \
	{ pte_val(pte) &= ~_PAGE_EXEC; return pte; }
static inline pte_t pte_mkclean(pte_t pte) \
	{ pte_val(pte) &= ~(_PAGE_DIRTY | _PAGE_HWWRITE); return pte; }
static inline pte_t pte_mkold(pte_t pte) \
	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }

static inline pte_t pte_mkread(pte_t pte) \
	{ pte_val(pte) |= _PAGE_USER; return pte; }
static inline pte_t pte_mkexec(pte_t pte) \
	{ pte_val(pte) |= _PAGE_USER | _PAGE_EXEC; return pte; }
static inline pte_t pte_mkwrite(pte_t pte) \
	{ pte_val(pte) |= _PAGE_RW; return pte; }
static inline pte_t pte_mkdirty(pte_t pte) \
	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte) \
	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

static inline pte_t mk_pte_phys(phys_addr_t physpage, pgprot_t pgprot)
{
	pte_t pte;
	pte_val(pte) = physpage | pgprot_val(pgprot);
	return pte;
}

#define mk_pte(page, pgprot) \
({									   \
	pte_t pte;							   \
	pte_val(pte) = (((page - mem_map) << PAGE_SHIFT) + memory_start) |  \
			pgprot_val(pgprot);				   \
	pte;								   \
})

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot);
	return pte;
}

/*
 * Atomic PTE updates.
 *
 * pte_update clears and sets bit atomically, and returns
 * the old pte value.
 * The ((unsigned long)(p+1) - 4) hack is to get to the least-significant
 * 32 bits of the PTE regardless of whether PTEs are 32 or 64 bits.
 */
static inline unsigned long pte_update(pte_t *p, unsigned long clr,
				unsigned long set)
{
	unsigned long old, tmp, msr;

	__asm__ __volatile__("\
	msrclr	%2, 0x2\n\
	nop\n\
	lw	%0, %4, r0\n\
	andn	%1, %0, %5\n\
	or	%1, %1, %6\n\
	sw	%1, %4, r0\n\
	mts     rmsr, %2\n\
	nop"
	: "=&r" (old), "=&r" (tmp), "=&r" (msr), "=m" (*p)
	: "r" ((unsigned long)(p + 1) - 4), "r" (clr), "r" (set), "m" (*p)
	: "cc");

	return old;
}

/*
 * set_pte stores a linux PTE into the linux page table.
 */
static inline void set_pte(struct mm_struct *mm, unsigned long addr,
		pte_t *ptep, pte_t pte)
{
	*ptep = pte;
}

static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
		pte_t *ptep, pte_t pte)
{
	*ptep = pte;
}

static inline int ptep_test_and_clear_young(struct mm_struct *mm,
		unsigned long addr, pte_t *ptep)
{
	return (pte_update(ptep, _PAGE_ACCESSED, 0) & _PAGE_ACCESSED) != 0;
}

static inline int ptep_test_and_clear_dirty(struct mm_struct *mm,
		unsigned long addr, pte_t *ptep)
{
	return (pte_update(ptep, \
		(_PAGE_DIRTY | _PAGE_HWWRITE), 0) & _PAGE_DIRTY) != 0;
}

static inline pte_t ptep_get_and_clear(struct mm_struct *mm,
		unsigned long addr, pte_t *ptep)
{
	return __pte(pte_update(ptep, ~_PAGE_HASHPTE, 0));
}

/*static inline void ptep_set_wrprotect(struct mm_struct *mm,
		unsigned long addr, pte_t *ptep)
{
	pte_update(ptep, (_PAGE_RW | _PAGE_HWWRITE), 0);
}*/

static inline void ptep_mkdirty(struct mm_struct *mm,
		unsigned long addr, pte_t *ptep)
{
	pte_update(ptep, 0, _PAGE_DIRTY);
}

/*#define pte_same(A,B)	(((pte_val(A) ^ pte_val(B)) & ~_PAGE_HASHPTE) == 0)*/

/* Convert pmd entry to page */
/* our pmd entry is an effective address of pte table*/
/* returns effective address of the pmd entry*/
#define pmd_page_kernel(pmd)	((unsigned long) (pmd_val(pmd) & PAGE_MASK))

/* returns struct *page of the pmd entry*/
#define pmd_page(pmd)	(pfn_to_page(__pa(pmd_val(pmd)) >> PAGE_SHIFT))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* to find an entry in a page-table-directory */
#define pgd_index(address)	 ((address) >> PGDIR_SHIFT)
#define pgd_offset(mm, address)	 ((mm)->pgd + pgd_index(address))

/* Find an entry in the second-level page table.. */
static inline pmd_t *pmd_offset(pgd_t *dir, unsigned long address)
{
	return (pmd_t *) dir;
}

/* Find an entry in the third-level page table.. */
#define pte_index(address)		\
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir, addr)	\
	((pte_t *) pmd_page_kernel(*(dir)) + pte_index(addr))
#define pte_offset_map(dir, addr)		\
	((pte_t *) kmap_atomic(pmd_page(*(dir)), KM_PTE0) + pte_index(addr))
#define pte_offset_map_nested(dir, addr)	\
	((pte_t *) kmap_atomic(pmd_page(*(dir)), KM_PTE1) + pte_index(addr))

#define pte_unmap(pte)		kunmap_atomic(pte, KM_PTE0)
#define pte_unmap_nested(pte)	kunmap_atomic(pte, KM_PTE1)

/* Encode and decode a nonlinear file mapping entry */
#define PTE_FILE_MAX_BITS	29
#define pte_to_pgoff(pte)	(pte_val(pte) >> 3)
#define pgoff_to_pte(off)	((pte_t) { ((off) << 3) | _PAGE_FILE })

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

/*
 * Encode and decode a swap entry.
 * Note that the bits we use in a PTE for representing a swap entry
 * must not include the _PAGE_PRESENT bit, or the _PAGE_HASHPTE bit
 * (if used).  -- paulus
 */
#define __swp_type(entry)		((entry).val & 0x3f)
#define __swp_offset(entry)	((entry).val >> 6)
#define __swp_entry(type, offset) \
		((swp_entry_t) { (type) | ((offset) << 6) })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) >> 2 })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val << 2 })

extern unsigned long iopa(unsigned long addr);

/* Values for nocacheflag and cmode */
/* These are not used by the APUS kernel_map, but prevents
 * compilation errors.
 */
#define	IOMAP_FULL_CACHING	0
#define	IOMAP_NOCACHE_SER	1
#define	IOMAP_NOCACHE_NONSER	2
#define	IOMAP_NO_COPYBACK	3

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range remap_page_range

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

void do_page_fault(struct pt_regs *regs, unsigned long address,
		   unsigned long error_code);

void mapin_ram(void);
int map_page(unsigned long va, phys_addr_t pa, int flags);

extern int mem_init_done;

asmlinkage void __init mmu_init(void);

void __init *early_get_page(void);

#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */

#endif /* CONFIG_MMU */

#ifndef __ASSEMBLY__
#include <asm-generic/pgtable.h>

extern unsigned long ioremap_bot, ioremap_base;

void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle);
void consistent_free(size_t size, void *vaddr);
void consistent_sync(void *vaddr, size_t size, int direction);
void consistent_sync_page(struct page *page, unsigned long offset,
	size_t size, int direction);

void setup_memory(void);
#endif /* __ASSEMBLY__ */

#endif /* _ASM_MICROBLAZE_PGTABLE_H */
