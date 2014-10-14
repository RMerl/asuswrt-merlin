#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#include <asm/virtconvert.h>
#include <asm/mem-layout.h>
#include <asm/sections.h>
#include <asm/setup.h>

#ifndef __ASSEMBLY__

#define get_user_page(vaddr)			__get_free_page(GFP_KERNEL)
#define free_user_page(page, addr)		free_page(addr)

#define clear_page(pgaddr)			memset((pgaddr), 0, PAGE_SIZE)
#define copy_page(to,from)			memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(pgaddr, vaddr, page)	memset((pgaddr), 0, PAGE_SIZE)
#define copy_user_page(vto, vfrom, vaddr, topg)	memcpy((vto), (vfrom), PAGE_SIZE)

/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long	pte;	} pte_t;
typedef struct { unsigned long	ste[64];} pmd_t;
typedef struct { pmd_t		pue[1]; } pud_t;
typedef struct { pud_t		pge[1];	} pgd_t;
typedef struct { unsigned long	pgprot;	} pgprot_t;
typedef struct page *pgtable_t;

#define pte_val(x)	((x).pte)
#define pmd_val(x)	((x).ste[0])
#define pud_val(x)	((x).pue[0])
#define pgd_val(x)	((x).pge[0])
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __pmd(x)	((pmd_t) { (x) } )
#define __pud(x)	((pud_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )
#define PTE_MASK	PAGE_MASK

#define devmem_is_allowed(pfn)	1

#define __pa(vaddr)		virt_to_phys((void *) (unsigned long) (vaddr))
#define __va(paddr)		phys_to_virt((unsigned long) (paddr))

#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;
extern unsigned long max_pfn;

#ifdef CONFIG_MMU
#define pfn_valid(pfn)		((pfn) < max_mapnr)
#else
#define ARCH_PFN_OFFSET		(PAGE_OFFSET >> PAGE_SHIFT)
#define pfn_valid(pfn)		((pfn) >= min_low_pfn && (pfn) < max_low_pfn)

#endif

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)


#define VM_DATA_DEFAULT_FLAGS \
	(VM_READ | VM_WRITE | \
	((current->personality & READ_IMPLIES_EXEC) ? VM_EXEC : 0 ) | \
		 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __ASSEMBLY__ */

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* _ASM_PAGE_H */
