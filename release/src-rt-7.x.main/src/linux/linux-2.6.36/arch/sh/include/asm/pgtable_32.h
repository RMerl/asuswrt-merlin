#ifndef __ASM_SH_PGTABLE_32_H
#define __ASM_SH_PGTABLE_32_H

/* Legacy and compat mode bits */
#define	_PAGE_WT	0x001		/* WT-bit on SH-4, 0 on SH-3 */
#define _PAGE_HW_SHARED	0x002		/* SH-bit  : shared among processes */
#define _PAGE_DIRTY	0x004		/* D-bit   : page changed */
#define _PAGE_CACHABLE	0x008		/* C-bit   : cachable */
#define _PAGE_SZ0	0x010		/* SZ0-bit : Size of page */
#define _PAGE_RW	0x020
#define _PAGE_USER	0x040
#define _PAGE_SZ1	0x080		/* SZ1-bit : Size of page (on SH-4) */
#define _PAGE_PRESENT	0x100		/* V-bit   : page is valid */
#define _PAGE_PROTNONE	0x200		/* software: if not present  */
#define _PAGE_ACCESSED	0x400		/* software: page referenced */
#define _PAGE_FILE	_PAGE_WT	/* software: pagecache or swap? */
#define _PAGE_SPECIAL	0x800		/* software: special page */

#define _PAGE_SZ_MASK	(_PAGE_SZ0 | _PAGE_SZ1)
#define _PAGE_PR_MASK	(_PAGE_RW | _PAGE_USER)

/* Extended mode bits */
#define _PAGE_EXT_ESZ0		0x0010	/* ESZ0-bit: Size of page */
#define _PAGE_EXT_ESZ1		0x0020	/* ESZ1-bit: Size of page */
#define _PAGE_EXT_ESZ2		0x0040	/* ESZ2-bit: Size of page */
#define _PAGE_EXT_ESZ3		0x0080	/* ESZ3-bit: Size of page */

#define _PAGE_EXT_USER_EXEC	0x0100	/* EPR0-bit: User space executable */
#define _PAGE_EXT_USER_WRITE	0x0200	/* EPR1-bit: User space writable */
#define _PAGE_EXT_USER_READ	0x0400	/* EPR2-bit: User space readable */

#define _PAGE_EXT_KERN_EXEC	0x0800	/* EPR3-bit: Kernel space executable */
#define _PAGE_EXT_KERN_WRITE	0x1000	/* EPR4-bit: Kernel space writable */
#define _PAGE_EXT_KERN_READ	0x2000	/* EPR5-bit: Kernel space readable */

#define _PAGE_EXT_WIRED		0x4000	/* software: Wire TLB entry */

/* Wrapper for extended mode pgprot twiddling */
#define _PAGE_EXT(x)		((unsigned long long)(x) << 32)

#ifdef CONFIG_X2TLB
#define _PAGE_PCC_MASK	0x00000000	/* No legacy PTEA support */
#else

/* software: moves to PTEA.TC (Timing Control) */
#define _PAGE_PCC_AREA5	0x00000000	/* use BSC registers for area5 */
#define _PAGE_PCC_AREA6	0x80000000	/* use BSC registers for area6 */

/* software: moves to PTEA.SA[2:0] (Space Attributes) */
#define _PAGE_PCC_IODYN 0x00000001	/* IO space, dynamically sized bus */
#define _PAGE_PCC_IO8	0x20000000	/* IO space, 8 bit bus */
#define _PAGE_PCC_IO16	0x20000001	/* IO space, 16 bit bus */
#define _PAGE_PCC_COM8	0x40000000	/* Common Memory space, 8 bit bus */
#define _PAGE_PCC_COM16	0x40000001	/* Common Memory space, 16 bit bus */
#define _PAGE_PCC_ATR8	0x60000000	/* Attribute Memory space, 8 bit bus */
#define _PAGE_PCC_ATR16	0x60000001	/* Attribute Memory space, 6 bit bus */

#define _PAGE_PCC_MASK	0xe0000001

/* copy the ptea attributes */
static inline unsigned long copy_ptea_attributes(unsigned long x)
{
	return	((x >> 28) & 0xe) | (x & 0x1);
}
#endif

/* Mask which drops unused bits from the PTEL value */
#if defined(CONFIG_CPU_SH3)
#define _PAGE_CLEAR_FLAGS	(_PAGE_PROTNONE | _PAGE_ACCESSED| \
				 _PAGE_FILE	| _PAGE_SZ1	| \
				 _PAGE_HW_SHARED)
#elif defined(CONFIG_X2TLB)
/* Get rid of the legacy PR/SZ bits when using extended mode */
#define _PAGE_CLEAR_FLAGS	(_PAGE_PROTNONE | _PAGE_ACCESSED | \
				 _PAGE_FILE | _PAGE_PR_MASK | _PAGE_SZ_MASK)
#else
#define _PAGE_CLEAR_FLAGS	(_PAGE_PROTNONE | _PAGE_ACCESSED | _PAGE_FILE)
#endif

#define _PAGE_FLAGS_HARDWARE_MASK	(phys_addr_mask() & ~(_PAGE_CLEAR_FLAGS))

/* Hardware flags, page size encoding */
#if !defined(CONFIG_MMU)
# define _PAGE_FLAGS_HARD	0ULL
#elif defined(CONFIG_X2TLB)
# if defined(CONFIG_PAGE_SIZE_4KB)
#  define _PAGE_FLAGS_HARD	_PAGE_EXT(_PAGE_EXT_ESZ0)
# elif defined(CONFIG_PAGE_SIZE_8KB)
#  define _PAGE_FLAGS_HARD	_PAGE_EXT(_PAGE_EXT_ESZ1)
# elif defined(CONFIG_PAGE_SIZE_64KB)
#  define _PAGE_FLAGS_HARD	_PAGE_EXT(_PAGE_EXT_ESZ2)
# endif
#else
# if defined(CONFIG_PAGE_SIZE_4KB)
#  define _PAGE_FLAGS_HARD	_PAGE_SZ0
# elif defined(CONFIG_PAGE_SIZE_64KB)
#  define _PAGE_FLAGS_HARD	_PAGE_SZ1
# endif
#endif

#if defined(CONFIG_X2TLB)
# if defined(CONFIG_HUGETLB_PAGE_SIZE_64K)
#  define _PAGE_SZHUGE	(_PAGE_EXT_ESZ2)
# elif defined(CONFIG_HUGETLB_PAGE_SIZE_256K)
#  define _PAGE_SZHUGE	(_PAGE_EXT_ESZ0 | _PAGE_EXT_ESZ2)
# elif defined(CONFIG_HUGETLB_PAGE_SIZE_1MB)
#  define _PAGE_SZHUGE	(_PAGE_EXT_ESZ0 | _PAGE_EXT_ESZ1 | _PAGE_EXT_ESZ2)
# elif defined(CONFIG_HUGETLB_PAGE_SIZE_4MB)
#  define _PAGE_SZHUGE	(_PAGE_EXT_ESZ3)
# elif defined(CONFIG_HUGETLB_PAGE_SIZE_64MB)
#  define _PAGE_SZHUGE	(_PAGE_EXT_ESZ2 | _PAGE_EXT_ESZ3)
# endif
# define _PAGE_WIRED	(_PAGE_EXT(_PAGE_EXT_WIRED))
#else
# if defined(CONFIG_HUGETLB_PAGE_SIZE_64K)
#  define _PAGE_SZHUGE	(_PAGE_SZ1)
# elif defined(CONFIG_HUGETLB_PAGE_SIZE_1MB)
#  define _PAGE_SZHUGE	(_PAGE_SZ0 | _PAGE_SZ1)
# endif
# define _PAGE_WIRED	(0)
#endif

/*
 * Stub out _PAGE_SZHUGE if we don't have a good definition for it,
 * to make pte_mkhuge() happy.
 */
#ifndef _PAGE_SZHUGE
# define _PAGE_SZHUGE	(_PAGE_FLAGS_HARD)
#endif

/*
 * Mask of bits that are to be preserved accross pgprot changes.
 */
#define _PAGE_CHG_MASK \
	(PTE_MASK | _PAGE_ACCESSED | _PAGE_CACHABLE | \
	 _PAGE_DIRTY | _PAGE_SPECIAL)

#ifndef __ASSEMBLY__

#if defined(CONFIG_X2TLB) /* SH-X2 TLB */
#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD)

#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
				 _PAGE_CACHABLE | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ  | \
					   _PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_USER_READ  | \
					   _PAGE_EXT_USER_WRITE))

#define PAGE_EXECREAD	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
				 _PAGE_CACHABLE | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_EXEC | \
					   _PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_USER_EXEC | \
					   _PAGE_EXT_USER_READ))

#define PAGE_COPY	PAGE_EXECREAD

#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
				 _PAGE_CACHABLE | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_USER_READ))

#define PAGE_WRITEONLY	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
				 _PAGE_CACHABLE | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_USER_WRITE))

#define PAGE_RWX	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
				 _PAGE_CACHABLE | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_KERN_READ  | \
					   _PAGE_EXT_KERN_EXEC  | \
					   _PAGE_EXT_USER_WRITE | \
					   _PAGE_EXT_USER_READ  | \
					   _PAGE_EXT_USER_EXEC))

#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_CACHABLE | \
				 _PAGE_DIRTY | _PAGE_ACCESSED | \
				 _PAGE_HW_SHARED | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_KERN_EXEC))

#define PAGE_KERNEL_NOCACHE \
			__pgprot(_PAGE_PRESENT | _PAGE_DIRTY | \
				 _PAGE_ACCESSED | _PAGE_HW_SHARED | \
				 _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_KERN_EXEC))

#define PAGE_KERNEL_RO	__pgprot(_PAGE_PRESENT | _PAGE_CACHABLE | \
				 _PAGE_DIRTY | _PAGE_ACCESSED | \
				 _PAGE_HW_SHARED | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_KERN_EXEC))

#define PAGE_KERNEL_PCC(slot, type) \
			__pgprot(_PAGE_PRESENT | _PAGE_DIRTY | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD | \
				 _PAGE_EXT(_PAGE_EXT_KERN_READ | \
					   _PAGE_EXT_KERN_WRITE | \
					   _PAGE_EXT_KERN_EXEC) \
				 (slot ? _PAGE_PCC_AREA5 : _PAGE_PCC_AREA6) | \
				 (type))

#elif defined(CONFIG_MMU) /* SH-X TLB */
#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD)

#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | \
				 _PAGE_CACHABLE | _PAGE_ACCESSED | \
				 _PAGE_FLAGS_HARD)

#define PAGE_COPY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD)

#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD)

#define PAGE_EXECREAD	PAGE_READONLY
#define PAGE_RWX	PAGE_SHARED
#define PAGE_WRITEONLY	PAGE_SHARED

#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_CACHABLE | \
				 _PAGE_DIRTY | _PAGE_ACCESSED | \
				 _PAGE_HW_SHARED | _PAGE_FLAGS_HARD)

#define PAGE_KERNEL_NOCACHE \
			__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | \
				 _PAGE_ACCESSED | _PAGE_HW_SHARED | \
				 _PAGE_FLAGS_HARD)

#define PAGE_KERNEL_RO	__pgprot(_PAGE_PRESENT | _PAGE_CACHABLE | \
				 _PAGE_DIRTY | _PAGE_ACCESSED | \
				 _PAGE_HW_SHARED | _PAGE_FLAGS_HARD)

#define PAGE_KERNEL_PCC(slot, type) \
			__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | \
				 _PAGE_ACCESSED | _PAGE_FLAGS_HARD | \
				 (slot ? _PAGE_PCC_AREA5 : _PAGE_PCC_AREA6) | \
				 (type))
#else /* no mmu */
#define PAGE_NONE		__pgprot(0)
#define PAGE_SHARED		__pgprot(0)
#define PAGE_COPY		__pgprot(0)
#define PAGE_EXECREAD		__pgprot(0)
#define PAGE_RWX		__pgprot(0)
#define PAGE_READONLY		__pgprot(0)
#define PAGE_WRITEONLY		__pgprot(0)
#define PAGE_KERNEL		__pgprot(0)
#define PAGE_KERNEL_NOCACHE	__pgprot(0)
#define PAGE_KERNEL_RO		__pgprot(0)

#define PAGE_KERNEL_PCC(slot, type) \
				__pgprot(0)
#endif

#endif /* __ASSEMBLY__ */

#ifndef __ASSEMBLY__

/*
 * Certain architectures need to do special things when PTEs
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#ifdef CONFIG_X2TLB
static inline void set_pte(pte_t *ptep, pte_t pte)
{
	ptep->pte_high = pte.pte_high;
	smp_wmb();
	ptep->pte_low = pte.pte_low;
}
#else
#define set_pte(pteptr, pteval) (*(pteptr) = pteval)
#endif

#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)

/*
 * (pmds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

#define pfn_pte(pfn, prot) \
	__pte(((unsigned long long)(pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define pfn_pmd(pfn, prot) \
	__pmd(((unsigned long long)(pfn) << PAGE_SHIFT) | pgprot_val(prot))

#define pte_none(x)		(!pte_val(x))
#define pte_present(x)		((x).pte_low & (_PAGE_PRESENT | _PAGE_PROTNONE))

#define pte_clear(mm,addr,xp) do { set_pte_at(mm, addr, xp, __pte(0)); } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x))
#define pmd_clear(xp)	do { set_pmd(xp, __pmd(0)); } while (0)
#define	pmd_bad(x)	(pmd_val(x) & ~PAGE_MASK)

#define pages_to_mb(x)	((x) >> (20-PAGE_SHIFT))
#define pte_page(x)	pfn_to_page(pte_pfn(x))

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
#define pte_not_present(pte)	(!((pte).pte_low & _PAGE_PRESENT))
#define pte_dirty(pte)		((pte).pte_low & _PAGE_DIRTY)
#define pte_young(pte)		((pte).pte_low & _PAGE_ACCESSED)
#define pte_file(pte)		((pte).pte_low & _PAGE_FILE)
#define pte_special(pte)	((pte).pte_low & _PAGE_SPECIAL)

#ifdef CONFIG_X2TLB
#define pte_write(pte) \
	((pte).pte_high & (_PAGE_EXT_USER_WRITE | _PAGE_EXT_KERN_WRITE))
#else
#define pte_write(pte)		((pte).pte_low & _PAGE_RW)
#endif

#define PTE_BIT_FUNC(h,fn,op) \
static inline pte_t pte_##fn(pte_t pte) { pte.pte_##h op; return pte; }

#ifdef CONFIG_X2TLB
/*
 * We cheat a bit in the SH-X2 TLB case. As the permission bits are
 * individually toggled (and user permissions are entirely decoupled from
 * kernel permissions), we attempt to couple them a bit more sanely here.
 */
PTE_BIT_FUNC(high, wrprotect, &= ~(_PAGE_EXT_USER_WRITE | _PAGE_EXT_KERN_WRITE));
PTE_BIT_FUNC(high, mkwrite, |= _PAGE_EXT_USER_WRITE | _PAGE_EXT_KERN_WRITE);
PTE_BIT_FUNC(high, mkhuge, |= _PAGE_SZHUGE);
#else
PTE_BIT_FUNC(low, wrprotect, &= ~_PAGE_RW);
PTE_BIT_FUNC(low, mkwrite, |= _PAGE_RW);
PTE_BIT_FUNC(low, mkhuge, |= _PAGE_SZHUGE);
#endif

PTE_BIT_FUNC(low, mkclean, &= ~_PAGE_DIRTY);
PTE_BIT_FUNC(low, mkdirty, |= _PAGE_DIRTY);
PTE_BIT_FUNC(low, mkold, &= ~_PAGE_ACCESSED);
PTE_BIT_FUNC(low, mkyoung, |= _PAGE_ACCESSED);
PTE_BIT_FUNC(low, mkspecial, |= _PAGE_SPECIAL);

#define __HAVE_ARCH_PTE_SPECIAL

/*
 * Macro and implementation to make a page protection as uncachable.
 */
#define pgprot_writecombine(prot) \
	__pgprot(pgprot_val(prot) & ~_PAGE_CACHABLE)

#define pgprot_noncached	 pgprot_writecombine

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 *
 * extern pte_t mk_pte(struct page *page, pgprot_t pgprot)
 */
#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte.pte_low &= _PAGE_CHG_MASK;
	pte.pte_low |= pgprot_val(newprot);

#ifdef CONFIG_X2TLB
	pte.pte_high |= pgprot_val(newprot) >> 32;
#endif

	return pte;
}

#define pmd_page_vaddr(pmd)	((unsigned long)pmd_val(pmd))
#define pmd_page(pmd)		(virt_to_page(pmd_val(pmd)))

/* to find an entry in a page-table-directory. */
#define pgd_index(address)	(((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define pgd_offset(mm, address)	((mm)->pgd + pgd_index(address))
#define __pgd_offset(address)	pgd_index(address)

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address)	pgd_offset(&init_mm, address)

#define __pud_offset(address)	(((address) >> PUD_SHIFT) & (PTRS_PER_PUD-1))
#define __pmd_offset(address)	(((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

/* Find an entry in the third-level page table.. */
#define pte_index(address)	((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define __pte_offset(address)	pte_index(address)

#define pte_offset_kernel(dir, address) \
	((pte_t *) pmd_page_vaddr(*(dir)) + pte_index(address))
#define pte_offset_map(dir, address)		pte_offset_kernel(dir, address)
#define pte_offset_map_nested(dir, address)	pte_offset_kernel(dir, address)

#define pte_unmap(pte)		do { } while (0)
#define pte_unmap_nested(pte)	do { } while (0)

#ifdef CONFIG_X2TLB
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %p(%08lx%08lx).\n", __FILE__, __LINE__, \
	       &(e), (e).pte_high, (e).pte_low)
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %016llx.\n", __FILE__, __LINE__, pgd_val(e))
#else
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))
#endif

/*
 * Encode and de-code a swap entry
 *
 * Constraints:
 *	_PAGE_FILE at bit 0
 *	_PAGE_PRESENT at bit 8
 *	_PAGE_PROTNONE at bit 9
 *
 * For the normal case, we encode the swap type into bits 0:7 and the
 * swap offset into bits 10:30. For the 64-bit PTE case, we keep the
 * preserved bits in the low 32-bits and use the upper 32 as the swap
 * offset (along with a 5-bit type), following the same approach as x86
 * PAE. This keeps the logic quite simple, and allows for a full 32
 * PTE_FILE_MAX_BITS, as opposed to the 29-bits we're constrained with
 * in the pte_low case.
 *
 * As is evident by the Alpha code, if we ever get a 64-bit unsigned
 * long (swp_entry_t) to match up with the 64-bit PTEs, this all becomes
 * much cleaner..
 *
 * NOTE: We should set ZEROs at the position of _PAGE_PRESENT
 *       and _PAGE_PROTNONE bits
 */
#ifdef CONFIG_X2TLB
#define __swp_type(x)			((x).val & 0x1f)
#define __swp_offset(x)			((x).val >> 5)
#define __swp_entry(type, offset)	((swp_entry_t){ (type) | (offset) << 5})
#define __pte_to_swp_entry(pte)		((swp_entry_t){ (pte).pte_high })
#define __swp_entry_to_pte(x)		((pte_t){ 0, (x).val })

/*
 * Encode and decode a nonlinear file mapping entry
 */
#define pte_to_pgoff(pte)		((pte).pte_high)
#define pgoff_to_pte(off)		((pte_t) { _PAGE_FILE, (off) })

#define PTE_FILE_MAX_BITS		32
#else
#define __swp_type(x)			((x).val & 0xff)
#define __swp_offset(x)			((x).val >> 10)
#define __swp_entry(type, offset)	((swp_entry_t){(type) | (offset) <<10})

#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) >> 1 })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val << 1 })

/*
 * Encode and decode a nonlinear file mapping entry
 */
#define PTE_FILE_MAX_BITS	29
#define pte_to_pgoff(pte)	(pte_val(pte) >> 1)
#define pgoff_to_pte(off)	((pte_t) { ((off) << 1) | _PAGE_FILE })
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ASM_SH_PGTABLE_32_H */
