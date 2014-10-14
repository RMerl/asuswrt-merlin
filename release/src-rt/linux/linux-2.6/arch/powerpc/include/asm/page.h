#ifndef _ASM_POWERPC_PAGE_H
#define _ASM_POWERPC_PAGE_H

/*
 * Copyright (C) 2001,2005 IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __ASSEMBLY__
#include <linux/types.h>
#else
#include <asm/types.h>
#endif
#include <asm/asm-compat.h>
#include <asm/kdump.h>

/*
 * On regular PPC32 page size is 4K (but we support 4K/16K/64K/256K pages
 * on PPC44x). For PPC64 we support either 4K or 64K software
 * page size. When using 64K pages however, whether we are really supporting
 * 64K pages in HW or not is irrelevant to those definitions.
 */
#if defined(CONFIG_PPC_256K_PAGES)
#define PAGE_SHIFT		18
#elif defined(CONFIG_PPC_64K_PAGES)
#define PAGE_SHIFT		16
#elif defined(CONFIG_PPC_16K_PAGES)
#define PAGE_SHIFT		14
#else
#define PAGE_SHIFT		12
#endif

#define PAGE_SIZE		(ASM_CONST(1) << PAGE_SHIFT)

/* We do define AT_SYSINFO_EHDR but don't use the gate mechanism */
#define __HAVE_ARCH_GATE_AREA		1

/*
 * Subtle: (1 << PAGE_SHIFT) is an int, not an unsigned long. So if we
 * assign PAGE_MASK to a larger type it gets extended the way we want
 * (i.e. with 1s in the high bits)
 */
#define PAGE_MASK      (~((1 << PAGE_SHIFT) - 1))

/*
 * KERNELBASE is the virtual address of the start of the kernel, it's often
 * the same as PAGE_OFFSET, but _might not be_.
 *
 * The kdump dump kernel is one example where KERNELBASE != PAGE_OFFSET.
 *
 * PAGE_OFFSET is the virtual address of the start of lowmem.
 *
 * PHYSICAL_START is the physical address of the start of the kernel.
 *
 * MEMORY_START is the physical address of the start of lowmem.
 *
 * KERNELBASE, PAGE_OFFSET, and PHYSICAL_START are all configurable on
 * ppc32 and based on how they are set we determine MEMORY_START.
 *
 * For the linear mapping the following equation should be true:
 * KERNELBASE - PAGE_OFFSET = PHYSICAL_START - MEMORY_START
 *
 * Also, KERNELBASE >= PAGE_OFFSET and PHYSICAL_START >= MEMORY_START
 *
 * There are two was to determine a physical address from a virtual one:
 * va = pa + PAGE_OFFSET - MEMORY_START
 * va = pa + KERNELBASE - PHYSICAL_START
 *
 * If you want to know something's offset from the start of the kernel you
 * should subtract KERNELBASE.
 *
 * If you want to test if something's a kernel address, use is_kernel_addr().
 */

#define KERNELBASE      ASM_CONST(CONFIG_KERNEL_START)
#define PAGE_OFFSET	ASM_CONST(CONFIG_PAGE_OFFSET)
#define LOAD_OFFSET	ASM_CONST((CONFIG_KERNEL_START-CONFIG_PHYSICAL_START))

#if defined(CONFIG_RELOCATABLE)
#ifndef __ASSEMBLY__

extern phys_addr_t memstart_addr;
extern phys_addr_t kernstart_addr;
#endif
#define PHYSICAL_START	kernstart_addr
#else
#define PHYSICAL_START	ASM_CONST(CONFIG_PHYSICAL_START)
#endif

#ifdef CONFIG_PPC64
#define MEMORY_START	0UL
#elif defined(CONFIG_RELOCATABLE)
#define MEMORY_START	memstart_addr
#else
#define MEMORY_START	(PHYSICAL_START + PAGE_OFFSET - KERNELBASE)
#endif

#ifdef CONFIG_FLATMEM
#define ARCH_PFN_OFFSET		((unsigned long)(MEMORY_START >> PAGE_SHIFT))
#define pfn_valid(pfn)		((pfn) >= ARCH_PFN_OFFSET && (pfn) < max_mapnr)
#endif

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

/*
 * On Book-E parts we need __va to parse the device tree and we can't
 * determine MEMORY_START until then.  However we can determine PHYSICAL_START
 * from information at hand (program counter, TLB lookup).
 *
 * On non-Book-E PPC64 PAGE_OFFSET and MEMORY_START are constants so use
 * the other definitions for __va & __pa.
 */
#ifdef CONFIG_BOOKE
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) - PHYSICAL_START + KERNELBASE))
#define __pa(x) ((unsigned long)(x) + PHYSICAL_START - KERNELBASE)
#else
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) + PAGE_OFFSET - MEMORY_START))
#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET + MEMORY_START)
#endif

/*
 * Unfortunately the PLT is in the BSS in the PPC32 ELF ABI,
 * and needs to be executable.  This means the whole heap ends
 * up being executable.
 */
#define VM_DATA_DEFAULT_FLAGS32	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define VM_DATA_DEFAULT_FLAGS64	(VM_READ | VM_WRITE | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#ifdef __powerpc64__
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)

/*
 * Don't compare things with KERNELBASE or PAGE_OFFSET to test for
 * "kernelness", use is_kernel_addr() - it should do what you want.
 */
#ifdef CONFIG_PPC_BOOK3E_64
#define is_kernel_addr(x)	((x) >= 0x8000000000000000ul)
#else
#define is_kernel_addr(x)	((x) >= PAGE_OFFSET)
#endif

#ifndef __ASSEMBLY__

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/* These are used to make use of C type-checking. */

/* PTE level */
typedef struct { pte_basic_t pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) })

/* 64k pages additionally define a bigger "real PTE" type that gathers
 * the "second half" part of the PTE for pseudo 64k pages
 */
#if defined(CONFIG_PPC_64K_PAGES) && defined(CONFIG_PPC_STD_MMU_64)
typedef struct { pte_t pte; unsigned long hidx; } real_pte_t;
#else
typedef struct { pte_t pte; } real_pte_t;
#endif

/* PMD level */
#ifdef CONFIG_PPC64
typedef struct { unsigned long pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) })

/* PUD level exusts only on 4k pages */
#ifndef CONFIG_PPC_64K_PAGES
typedef struct { unsigned long pud; } pud_t;
#define pud_val(x)	((x).pud)
#define __pud(x)	((pud_t) { (x) })
#endif /* !CONFIG_PPC_64K_PAGES */
#endif /* CONFIG_PPC64 */

/* PGD level */
typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) })

/* Page protection bits */
typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })

#else

/*
 * .. while these make it easier on the compiler
 */

typedef pte_basic_t pte_t;
#define pte_val(x)	(x)
#define __pte(x)	(x)

#if defined(CONFIG_PPC_64K_PAGES) && defined(CONFIG_PPC_STD_MMU_64)
typedef struct { pte_t pte; unsigned long hidx; } real_pte_t;
#else
typedef pte_t real_pte_t;
#endif


#ifdef CONFIG_PPC64
typedef unsigned long pmd_t;
#define pmd_val(x)	(x)
#define __pmd(x)	(x)

#ifndef CONFIG_PPC_64K_PAGES
typedef unsigned long pud_t;
#define pud_val(x)	(x)
#define __pud(x)	(x)
#endif /* !CONFIG_PPC_64K_PAGES */
#endif /* CONFIG_PPC64 */

typedef unsigned long pgd_t;
#define pgd_val(x)	(x)
#define pgprot_val(x)	(x)

typedef unsigned long pgprot_t;
#define __pgd(x)	(x)
#define __pgprot(x)	(x)

#endif

typedef struct { signed long pd; } hugepd_t;
#define HUGEPD_SHIFT_MASK     0x3f

#ifdef CONFIG_HUGETLB_PAGE
static inline int hugepd_ok(hugepd_t hpd)
{
	return (hpd.pd > 0);
}

#define is_hugepd(pdep)               (hugepd_ok(*((hugepd_t *)(pdep))))
#else /* CONFIG_HUGETLB_PAGE */
#define is_hugepd(pdep)			0
#endif /* CONFIG_HUGETLB_PAGE */

struct page;
extern void clear_user_page(void *page, unsigned long vaddr, struct page *pg);
extern void copy_user_page(void *to, void *from, unsigned long vaddr,
		struct page *p);
extern int page_is_ram(unsigned long pfn);

#ifdef CONFIG_PPC_SMLPAR
void arch_free_page(struct page *page, int order);
#define HAVE_ARCH_FREE_PAGE
#endif

struct vm_area_struct;

typedef struct page *pgtable_t;

#include <asm-generic/memory_model.h>
#endif /* __ASSEMBLY__ */

#endif /* _ASM_POWERPC_PAGE_H */
