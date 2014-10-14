#ifndef _PARISC_PAGE_H
#define _PARISC_PAGE_H

#include <linux/const.h>

#if defined(CONFIG_PARISC_PAGE_SIZE_4KB)
# define PAGE_SHIFT	12
#elif defined(CONFIG_PARISC_PAGE_SIZE_16KB)
# define PAGE_SHIFT	14
#elif defined(CONFIG_PARISC_PAGE_SIZE_64KB)
# define PAGE_SHIFT	16
#else
# error "unknown default kernel page size"
#endif
#define PAGE_SIZE	(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))


#ifndef __ASSEMBLY__

#include <asm/types.h>
#include <asm/cache.h>

#define clear_page(page)	memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)      copy_user_page_asm((void *)(to), (void *)(from))

struct page;

void copy_user_page_asm(void *to, void *from);
void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
			   struct page *pg);
void clear_user_page(void *page, unsigned long vaddr, struct page *pg);

/*
 * These are used to make use of C type-checking..
 */
#define STRICT_MM_TYPECHECKS
#ifdef STRICT_MM_TYPECHECKS
typedef struct { unsigned long pte; } pte_t; /* either 32 or 64bit */

/* NOTE: even on 64 bits, these entries are __u32 because we allocate
 * the pmd and pgd in ZONE_DMA (i.e. under 4GB) */
typedef struct { __u32 pmd; } pmd_t;
typedef struct { __u32 pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)	((x).pte)
/* These do not work lvalues, so make sure we don't use them as such. */
#define pmd_val(x)	((x).pmd + 0)
#define pgd_val(x)	((x).pgd + 0)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __pmd(x)	((pmd_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#define __pmd_val_set(x,n) (x).pmd = (n)
#define __pgd_val_set(x,n) (x).pgd = (n)

#else
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t;
typedef         __u32 pmd_t;
typedef         __u32 pgd_t;
typedef unsigned long pgprot_t;

#define pte_val(x)      (x)
#define pmd_val(x)      (x)
#define pgd_val(x)      (x)
#define pgprot_val(x)   (x)

#define __pte(x)        (x)
#define __pmd(x)	(x)
#define __pgd(x)        (x)
#define __pgprot(x)     (x)

#define __pmd_val_set(x,n) (x) = (n)
#define __pgd_val_set(x,n) (x) = (n)

#endif /* STRICT_MM_TYPECHECKS */

typedef struct page *pgtable_t;

typedef struct __physmem_range {
	unsigned long start_pfn;
	unsigned long pages;       /* PAGE_SIZE pages */
} physmem_range_t;

extern physmem_range_t pmem_ranges[];
extern int npmem_ranges;

#endif /* !__ASSEMBLY__ */

/* WARNING: The definitions below must match exactly to sizeof(pte_t)
 * etc
 */
#ifdef CONFIG_64BIT
#define BITS_PER_PTE_ENTRY	3
#define BITS_PER_PMD_ENTRY	2
#define BITS_PER_PGD_ENTRY	2
#else
#define BITS_PER_PTE_ENTRY	2
#define BITS_PER_PMD_ENTRY	2
#define BITS_PER_PGD_ENTRY	BITS_PER_PMD_ENTRY
#endif
#define PGD_ENTRY_SIZE	(1UL << BITS_PER_PGD_ENTRY)
#define PMD_ENTRY_SIZE	(1UL << BITS_PER_PMD_ENTRY)
#define PTE_ENTRY_SIZE	(1UL << BITS_PER_PTE_ENTRY)

#define LINUX_GATEWAY_SPACE     0

/* This governs the relationship between virtual and physical addresses.
 * If you alter it, make sure to take care of our various fixed mapping
 * segments in fixmap.h */
#ifdef CONFIG_64BIT
#define __PAGE_OFFSET	(0x40000000)	/* 1GB */
#else
#define __PAGE_OFFSET	(0x10000000)	/* 256MB */
#endif

#define PAGE_OFFSET		((unsigned long)__PAGE_OFFSET)

/* The size of the gateway page (we leave lots of room for expansion) */
#define GATEWAY_PAGE_SIZE	0x4000

/* The start of the actual kernel binary---used in vmlinux.lds.S
 * Leave some space after __PAGE_OFFSET for detecting kernel null
 * ptr derefs */
#define KERNEL_BINARY_TEXT_START	(__PAGE_OFFSET + 0x100000)

/* These macros don't work for 64-bit C code -- don't allow in C at all */
#ifdef __ASSEMBLY__
#   define PA(x)	((x)-__PAGE_OFFSET)
#   define VA(x)	((x)+__PAGE_OFFSET)
#endif
#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))

#ifndef CONFIG_DISCONTIGMEM
#define pfn_valid(pfn)		((pfn) < max_mapnr)
#endif /* CONFIG_DISCONTIGMEM */

#ifdef CONFIG_HUGETLB_PAGE
#define HPAGE_SHIFT		22	/* 4MB (is this fixed?) */
#define HPAGE_SIZE      	((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK		(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#endif

#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#define virt_to_page(kaddr)     pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* _PARISC_PAGE_H */
