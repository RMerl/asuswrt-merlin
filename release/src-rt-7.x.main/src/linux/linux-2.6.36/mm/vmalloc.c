/*
 *  linux/mm/vmalloc.c
 *
 *  Copyright (C) 1993  Linus Torvalds
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  SMP-safe vmalloc/vfree/ioremap, Tigran Aivazian <tigran@veritas.com>, May 2000
 *  Major rework to support vmap/vunmap, Christoph Hellwig, SGI, August 2002
 *  Numa awareness, Christoph Lameter, SGI, June 2005
 */

#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugobjects.h>
#include <linux/kallsyms.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/radix-tree.h>
#include <linux/rcupdate.h>
#include <linux/pfn.h>
#include <linux/kmemleak.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/shmparam.h>

bool vmap_lazy_unmap __read_mostly = true;

/*** Page table manipulation functions ***/

static void vunmap_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end)
{
	pte_t *pte;

	pte = pte_offset_kernel(pmd, addr);
	do {
		pte_t ptent = ptep_get_and_clear(&init_mm, addr, pte);
		WARN_ON(!pte_none(ptent) && !pte_present(ptent));
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static void vunmap_pmd_range(pud_t *pud, unsigned long addr, unsigned long end)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none_or_clear_bad(pmd))
			continue;
		vunmap_pte_range(pmd, addr, next);
	} while (pmd++, addr = next, addr != end);
}

static void vunmap_pud_range(pgd_t *pgd, unsigned long addr, unsigned long end)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		vunmap_pmd_range(pud, addr, next);
	} while (pud++, addr = next, addr != end);
}

static void vunmap_page_range(unsigned long addr, unsigned long end)
{
	pgd_t *pgd;
	unsigned long next;

	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		vunmap_pud_range(pgd, addr, next);
	} while (pgd++, addr = next, addr != end);
}

static int vmap_pte_range(pmd_t *pmd, unsigned long addr,
		unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pte_t *pte;

	/*
	 * nr is a running index into the array which helps higher level
	 * callers keep track of where we're up to.
	 */

	pte = pte_alloc_kernel(pmd, addr);
	if (!pte)
		return -ENOMEM;
	do {
		struct page *page = pages[*nr];

		if (WARN_ON(!pte_none(*pte)))
			return -EBUSY;
		if (WARN_ON(!page))
			return -ENOMEM;
		set_pte_at(&init_mm, addr, pte, mk_pte(page, prot));
		(*nr)++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
	return 0;
}

static int vmap_pmd_range(pud_t *pud, unsigned long addr,
		unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_alloc(&init_mm, pud, addr);
	if (!pmd)
		return -ENOMEM;
	do {
		next = pmd_addr_end(addr, end);
		if (vmap_pte_range(pmd, addr, next, prot, pages, nr))
			return -ENOMEM;
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static int vmap_pud_range(pgd_t *pgd, unsigned long addr,
		unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_alloc(&init_mm, pgd, addr);
	if (!pud)
		return -ENOMEM;
	do {
		next = pud_addr_end(addr, end);
		if (vmap_pmd_range(pud, addr, next, prot, pages, nr))
			return -ENOMEM;
	} while (pud++, addr = next, addr != end);
	return 0;
}

/*
 * Set up page tables in kva (addr, end). The ptes shall have prot "prot", and
 * will have pfns corresponding to the "pages" array.
 *
 * Ie. pte at addr+N*PAGE_SIZE shall point to pfn corresponding to pages[N]
 */
static int vmap_page_range_noflush(unsigned long start, unsigned long end,
				   pgprot_t prot, struct page **pages)
{
	pgd_t *pgd;
	unsigned long next;
	unsigned long addr = start;
	int err = 0;
	int nr = 0;

	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do {
		next = pgd_addr_end(addr, end);
		err = vmap_pud_range(pgd, addr, next, prot, pages, &nr);
		if (err)
			return err;
	} while (pgd++, addr = next, addr != end);

	return nr;
}

static int vmap_page_range(unsigned long start, unsigned long end,
			   pgprot_t prot, struct page **pages)
{
	int ret;

	ret = vmap_page_range_noflush(start, end, prot, pages);
	flush_cache_vmap(start, end);
	return ret;
}

int is_vmalloc_or_module_addr(const void *x)
{
	/*
	 * ARM, x86-64 and sparc64 put modules in a special place,
	 * and fall back on vmalloc() if that fails. Others
	 * just put it in the vmalloc space.
	 */
#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	unsigned long addr = (unsigned long)x;
	if (addr >= MODULES_VADDR && addr < MODULES_END)
		return 1;
#endif
	return is_vmalloc_addr(x);
}

/*
 * Walk a vmap address to the struct page it maps.
 */
struct page *vmalloc_to_page(const void *vmalloc_addr)
{
	unsigned long addr = (unsigned long) vmalloc_addr;
	struct page *page = NULL;
	pgd_t *pgd = pgd_offset_k(addr);

	VIRTUAL_BUG_ON(!is_vmalloc_or_module_addr(vmalloc_addr));

	if (!pgd_none(*pgd)) {
		pud_t *pud = pud_offset(pgd, addr);
		if (!pud_none(*pud)) {
			pmd_t *pmd = pmd_offset(pud, addr);
			if (!pmd_none(*pmd)) {
				pte_t *ptep, pte;

				ptep = pte_offset_map(pmd, addr);
				pte = *ptep;
				if (pte_present(pte))
					page = pte_page(pte);
				pte_unmap(ptep);
			}
		}
	}
	return page;
}
EXPORT_SYMBOL(vmalloc_to_page);

/*
 * Map a vmalloc()-space virtual address to the physical page frame number.
 */
unsigned long vmalloc_to_pfn(const void *vmalloc_addr)
{
	return page_to_pfn(vmalloc_to_page(vmalloc_addr));
}
EXPORT_SYMBOL(vmalloc_to_pfn);


/*** Global kva allocator ***/

#define VM_LAZY_FREE	0x01
#define VM_LAZY_FREEING	0x02
#define VM_VM_AREA	0x04

struct vmap_area {
	unsigned long va_start;
	unsigned long va_end;
	unsigned long flags;
	struct rb_node rb_node;		/* address sorted rbtree */
	struct list_head list;		/* address sorted list */
	struct list_head purge_list;	/* "lazy purge" list */
	void *private;
	struct rcu_head rcu_head;
};

static DEFINE_SPINLOCK(vmap_area_lock);
static struct rb_root vmap_area_root = RB_ROOT;
static LIST_HEAD(vmap_area_list);
static unsigned long vmap_area_pcpu_hole;

static struct vmap_area *__find_vmap_area(unsigned long addr)
{
	struct rb_node *n = vmap_area_root.rb_node;

	while (n) {
		struct vmap_area *va;

		va = rb_entry(n, struct vmap_area, rb_node);
		if (addr < va->va_start)
			n = n->rb_left;
		else if (addr > va->va_start)
			n = n->rb_right;
		else
			return va;
	}

	return NULL;
}

static void __insert_vmap_area(struct vmap_area *va)
{
	struct rb_node **p = &vmap_area_root.rb_node;
	struct rb_node *parent = NULL;
	struct rb_node *tmp;

	while (*p) {
		struct vmap_area *tmp;

		parent = *p;
		tmp = rb_entry(parent, struct vmap_area, rb_node);
		if (va->va_start < tmp->va_end)
			p = &(*p)->rb_left;
		else if (va->va_end > tmp->va_start)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&va->rb_node, parent, p);
	rb_insert_color(&va->rb_node, &vmap_area_root);

	/* address-sort this list so it is usable like the vmlist */
	tmp = rb_prev(&va->rb_node);
	if (tmp) {
		struct vmap_area *prev;
		prev = rb_entry(tmp, struct vmap_area, rb_node);
		list_add_rcu(&va->list, &prev->list);
	} else
		list_add_rcu(&va->list, &vmap_area_list);
}

static void purge_vmap_area_lazy(void);

/*
 * Allocate a region of KVA of the specified size and alignment, within the
 * vstart and vend.
 */
static struct vmap_area *alloc_vmap_area(unsigned long size,
				unsigned long align,
				unsigned long vstart, unsigned long vend,
				int node, gfp_t gfp_mask)
{
	struct vmap_area *va;
	struct rb_node *n;
	unsigned long addr;
	int purged = 0;

	BUG_ON(!size);
	BUG_ON(size & ~PAGE_MASK);

	va = kmalloc_node(sizeof(struct vmap_area),
			gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!va))
		return ERR_PTR(-ENOMEM);

retry:
	addr = ALIGN(vstart, align);

	spin_lock(&vmap_area_lock);
	if (addr + size - 1 < addr)
		goto overflow;

	n = vmap_area_root.rb_node;
	if (n) {
		struct vmap_area *first = NULL;

		do {
			struct vmap_area *tmp;
			tmp = rb_entry(n, struct vmap_area, rb_node);
			if (tmp->va_end >= addr) {
				if (!first && tmp->va_start < addr + size)
					first = tmp;
				n = n->rb_left;
			} else {
				first = tmp;
				n = n->rb_right;
			}
		} while (n);

		if (!first)
			goto found;

		if (first->va_end < addr) {
			n = rb_next(&first->rb_node);
			if (n)
				first = rb_entry(n, struct vmap_area, rb_node);
			else
				goto found;
		}

		while (addr + size > first->va_start && addr + size <= vend) {
			addr = ALIGN(first->va_end + PAGE_SIZE, align);
			if (addr + size - 1 < addr)
				goto overflow;

			n = rb_next(&first->rb_node);
			if (n)
				first = rb_entry(n, struct vmap_area, rb_node);
			else
				goto found;
		}
	}
found:
	if (addr + size > vend) {
overflow:
		spin_unlock(&vmap_area_lock);
		if (!purged) {
			purge_vmap_area_lazy();
			purged = 1;
			goto retry;
		}
		if (printk_ratelimit())
			printk(KERN_WARNING
				"vmap allocation for size %lu failed: "
				"use vmalloc=<size> to increase size.\n", size);
		kfree(va);
		return ERR_PTR(-EBUSY);
	}

	BUG_ON(addr & (align-1));

	va->va_start = addr;
	va->va_end = addr + size;
	va->flags = 0;
	__insert_vmap_area(va);
	spin_unlock(&vmap_area_lock);

	return va;
}

static void rcu_free_va(struct rcu_head *head)
{
	struct vmap_area *va = container_of(head, struct vmap_area, rcu_head);

	kfree(va);
}

static void __free_vmap_area(struct vmap_area *va)
{
	BUG_ON(RB_EMPTY_NODE(&va->rb_node));
	rb_erase(&va->rb_node, &vmap_area_root);
	RB_CLEAR_NODE(&va->rb_node);
	list_del_rcu(&va->list);

	/*
	 * Track the highest possible candidate for pcpu area
	 * allocation.  Areas outside of vmalloc area can be returned
	 * here too, consider only end addresses which fall inside
	 * vmalloc area proper.
	 */
	if (va->va_end > VMALLOC_START && va->va_end <= VMALLOC_END)
		vmap_area_pcpu_hole = max(vmap_area_pcpu_hole, va->va_end);

	call_rcu(&va->rcu_head, rcu_free_va);
}

/*
 * Free a region of KVA allocated by alloc_vmap_area
 */
static void free_vmap_area(struct vmap_area *va)
{
	spin_lock(&vmap_area_lock);
	__free_vmap_area(va);
	spin_unlock(&vmap_area_lock);
}

/*
 * Clear the pagetable entries of a given vmap_area
 */
static void unmap_vmap_area(struct vmap_area *va)
{
	vunmap_page_range(va->va_start, va->va_end);
}

static void vmap_debug_free_range(unsigned long start, unsigned long end)
{
	/*
	 * Unmap page tables and force a TLB flush immediately if
	 * CONFIG_DEBUG_PAGEALLOC is set. This catches use after free
	 * bugs similarly to those in linear kernel virtual address
	 * space after a page has been freed.
	 *
	 * All the lazy freeing logic is still retained, in order to
	 * minimise intrusiveness of this debugging feature.
	 *
	 * This is going to be *slow* (linear kernel virtual address
	 * debugging doesn't do a broadcast TLB flush so it is a lot
	 * faster).
	 */
#ifdef CONFIG_DEBUG_PAGEALLOC
	vunmap_page_range(start, end);
	flush_tlb_kernel_range(start, end);
#endif
}

/*
 * lazy_max_pages is the maximum amount of virtual address space we gather up
 * before attempting to purge with a TLB flush.
 *
 * There is a tradeoff here: a larger number will cover more kernel page tables
 * and take slightly longer to purge, but it will linearly reduce the number of
 * global TLB flushes that must be performed. It would seem natural to scale
 * this number up linearly with the number of CPUs (because vmapping activity
 * could also scale linearly with the number of CPUs), however it is likely
 * that in practice, workloads might be constrained in other ways that mean
 * vmap activity will not scale linearly with CPUs. Also, I want to be
 * conservative and not introduce a big latency on huge systems, so go with
 * a less aggressive log scale. It will still be an improvement over the old
 * code, and it will be simple to change the scale factor if we find that it
 * becomes a problem on bigger systems.
 */
static unsigned long lazy_max_pages(void)
{
	unsigned int log;

	if (!vmap_lazy_unmap)
		return 0;

	log = fls(num_online_cpus());

	return log * (32UL * 1024 * 1024 / PAGE_SIZE);
}

static atomic_t vmap_lazy_nr = ATOMIC_INIT(0);

/* for per-CPU blocks */
static void purge_fragmented_blocks_allcpus(void);

/*
 * called before a call to iounmap() if the caller wants vm_area_struct's
 * immediately freed.
 */
void set_iounmap_nonlazy(void)
{
	atomic_set(&vmap_lazy_nr, lazy_max_pages()+1);
}

/*
 * Purges all lazily-freed vmap areas.
 *
 * If sync is 0 then don't purge if there is already a purge in progress.
 * If force_flush is 1, then flush kernel TLBs between *start and *end even
 * if we found no lazy vmap areas to unmap (callers can use this to optimise
 * their own TLB flushing).
 * Returns with *start = min(*start, lowest purged address)
 *              *end = max(*end, highest purged address)
 */
static void __purge_vmap_area_lazy(unsigned long *start, unsigned long *end,
					int sync, int force_flush)
{
	static DEFINE_SPINLOCK(purge_lock);
	LIST_HEAD(valist);
	struct vmap_area *va;
	struct vmap_area *n_va;
	int nr = 0;

	/*
	 * If sync is 0 but force_flush is 1, we'll go sync anyway but callers
	 * should not expect such behaviour. This just simplifies locking for
	 * the case that isn't actually used at the moment anyway.
	 */
	if (!sync && !force_flush) {
		if (!spin_trylock(&purge_lock))
			return;
	} else
		spin_lock(&purge_lock);

	if (sync)
		purge_fragmented_blocks_allcpus();

	rcu_read_lock();
	list_for_each_entry_rcu(va, &vmap_area_list, list) {
		if (va->flags & VM_LAZY_FREE) {
			if (va->va_start < *start)
				*start = va->va_start;
			if (va->va_end > *end)
				*end = va->va_end;
			nr += (va->va_end - va->va_start) >> PAGE_SHIFT;
			unmap_vmap_area(va);
			list_add_tail(&va->purge_list, &valist);
			va->flags |= VM_LAZY_FREEING;
			va->flags &= ~VM_LAZY_FREE;
		}
	}
	rcu_read_unlock();

	if (nr)
		atomic_sub(nr, &vmap_lazy_nr);

	if (nr || force_flush)
		flush_tlb_kernel_range(*start, *end);

	if (nr) {
		spin_lock(&vmap_area_lock);
		list_for_each_entry_safe(va, n_va, &valist, purge_list)
			__free_vmap_area(va);
		spin_unlock(&vmap_area_lock);
	}
	spin_unlock(&purge_lock);
}

/*
 * Kick off a purge of the outstanding lazy areas. Don't bother if somebody
 * is already purging.
 */
static void try_purge_vmap_area_lazy(void)
{
	unsigned long start = ULONG_MAX, end = 0;

	__purge_vmap_area_lazy(&start, &end, 0, 0);
}

/*
 * Kick off a purge of the outstanding lazy areas.
 */
static void purge_vmap_area_lazy(void)
{
	unsigned long start = ULONG_MAX, end = 0;

	__purge_vmap_area_lazy(&start, &end, 1, 0);
}

/*
 * Free and unmap a vmap area, caller ensuring flush_cache_vunmap had been
 * called for the correct range previously.
 */
static void free_unmap_vmap_area_noflush(struct vmap_area *va)
{
	va->flags |= VM_LAZY_FREE;
	atomic_add((va->va_end - va->va_start) >> PAGE_SHIFT, &vmap_lazy_nr);
	if (unlikely(atomic_read(&vmap_lazy_nr) > lazy_max_pages()))
		try_purge_vmap_area_lazy();
}

/*
 * Free and unmap a vmap area
 */
static void free_unmap_vmap_area(struct vmap_area *va)
{
	flush_cache_vunmap(va->va_start, va->va_end);
	free_unmap_vmap_area_noflush(va);
}

static struct vmap_area *find_vmap_area(unsigned long addr)
{
	struct vmap_area *va;

	spin_lock(&vmap_area_lock);
	va = __find_vmap_area(addr);
	spin_unlock(&vmap_area_lock);

	return va;
}

static void free_unmap_vmap_area_addr(unsigned long addr)
{
	struct vmap_area *va;

	va = find_vmap_area(addr);
	BUG_ON(!va);
	free_unmap_vmap_area(va);
}


/*** Per cpu kva allocator ***/

/*
 * vmap space is limited especially on 32 bit architectures. Ensure there is
 * room for at least 16 percpu vmap blocks per CPU.
 */
/*
 * If we had a constant VMALLOC_START and VMALLOC_END, we'd like to be able
 * to #define VMALLOC_SPACE		(VMALLOC_END-VMALLOC_START). Guess
 * instead (we just need a rough idea)
 */
#if BITS_PER_LONG == 32
#define VMALLOC_SPACE		(128UL*1024*1024)
#else
#define VMALLOC_SPACE		(128UL*1024*1024*1024)
#endif

#define VMALLOC_PAGES		(VMALLOC_SPACE / PAGE_SIZE)
#define VMAP_MAX_ALLOC		BITS_PER_LONG	/* 256K with 4K pages */
#define VMAP_BBMAP_BITS_MAX	1024	/* 4MB with 4K pages */
#define VMAP_BBMAP_BITS_MIN	(VMAP_MAX_ALLOC*2)
#define VMAP_MIN(x, y)		((x) < (y) ? (x) : (y)) /* can't use min() */
#define VMAP_MAX(x, y)		((x) > (y) ? (x) : (y)) /* can't use max() */
#define VMAP_BBMAP_BITS		VMAP_MIN(VMAP_BBMAP_BITS_MAX,		\
					VMAP_MAX(VMAP_BBMAP_BITS_MIN,	\
						VMALLOC_PAGES / NR_CPUS / 16))

#define VMAP_BLOCK_SIZE		(VMAP_BBMAP_BITS * PAGE_SIZE)

static bool vmap_initialized __read_mostly = false;

struct vmap_block_queue {
	spinlock_t lock;
	struct list_head free;
};

struct vmap_block {
	spinlock_t lock;
	struct vmap_area *va;
	struct vmap_block_queue *vbq;
	unsigned long free, dirty;
	DECLARE_BITMAP(alloc_map, VMAP_BBMAP_BITS);
	DECLARE_BITMAP(dirty_map, VMAP_BBMAP_BITS);
	struct list_head free_list;
	struct rcu_head rcu_head;
	struct list_head purge;
};

/* Queue of free and dirty vmap blocks, for allocation and flushing purposes */
static DEFINE_PER_CPU(struct vmap_block_queue, vmap_block_queue);

/*
 * Radix tree of vmap blocks, indexed by address, to quickly find a vmap block
 * in the free path. Could get rid of this if we change the API to return a
 * "cookie" from alloc, to be passed to free. But no big deal yet.
 */
static DEFINE_SPINLOCK(vmap_block_tree_lock);
static RADIX_TREE(vmap_block_tree, GFP_ATOMIC);

/*
 * We should probably have a fallback mechanism to allocate virtual memory
 * out of partially filled vmap blocks. However vmap block sizing should be
 * fairly reasonable according to the vmalloc size, so it shouldn't be a
 * big problem.
 */

static unsigned long addr_to_vb_idx(unsigned long addr)
{
	addr -= VMALLOC_START & ~(VMAP_BLOCK_SIZE-1);
	addr /= VMAP_BLOCK_SIZE;
	return addr;
}

static struct vmap_block *new_vmap_block(gfp_t gfp_mask)
{
	struct vmap_block_queue *vbq;
	struct vmap_block *vb;
	struct vmap_area *va;
	unsigned long vb_idx;
	int node, err;

	node = numa_node_id();

	vb = kmalloc_node(sizeof(struct vmap_block),
			gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!vb))
		return ERR_PTR(-ENOMEM);

	va = alloc_vmap_area(VMAP_BLOCK_SIZE, VMAP_BLOCK_SIZE,
					VMALLOC_START, VMALLOC_END,
					node, gfp_mask);
	if (unlikely(IS_ERR(va))) {
		kfree(vb);
		return ERR_CAST(va);
	}

	err = radix_tree_preload(gfp_mask);
	if (unlikely(err)) {
		kfree(vb);
		free_vmap_area(va);
		return ERR_PTR(err);
	}

	spin_lock_init(&vb->lock);
	vb->va = va;
	vb->free = VMAP_BBMAP_BITS;
	vb->dirty = 0;
	bitmap_zero(vb->alloc_map, VMAP_BBMAP_BITS);
	bitmap_zero(vb->dirty_map, VMAP_BBMAP_BITS);
	INIT_LIST_HEAD(&vb->free_list);

	vb_idx = addr_to_vb_idx(va->va_start);
	spin_lock(&vmap_block_tree_lock);
	err = radix_tree_insert(&vmap_block_tree, vb_idx, vb);
	spin_unlock(&vmap_block_tree_lock);
	BUG_ON(err);
	radix_tree_preload_end();

	vbq = &get_cpu_var(vmap_block_queue);
	vb->vbq = vbq;
	spin_lock(&vbq->lock);
	list_add_rcu(&vb->free_list, &vbq->free);
	spin_unlock(&vbq->lock);
	put_cpu_var(vmap_block_queue);

	return vb;
}

static void rcu_free_vb(struct rcu_head *head)
{
	struct vmap_block *vb = container_of(head, struct vmap_block, rcu_head);

	kfree(vb);
}

static void free_vmap_block(struct vmap_block *vb)
{
	struct vmap_block *tmp;
	unsigned long vb_idx;

	vb_idx = addr_to_vb_idx(vb->va->va_start);
	spin_lock(&vmap_block_tree_lock);
	tmp = radix_tree_delete(&vmap_block_tree, vb_idx);
	spin_unlock(&vmap_block_tree_lock);
	BUG_ON(tmp != vb);

	free_unmap_vmap_area_noflush(vb->va);
	call_rcu(&vb->rcu_head, rcu_free_vb);
}

static void purge_fragmented_blocks(int cpu)
{
	LIST_HEAD(purge);
	struct vmap_block *vb;
	struct vmap_block *n_vb;
	struct vmap_block_queue *vbq = &per_cpu(vmap_block_queue, cpu);

	rcu_read_lock();
	list_for_each_entry_rcu(vb, &vbq->free, free_list) {

		if (!(vb->free + vb->dirty == VMAP_BBMAP_BITS && vb->dirty != VMAP_BBMAP_BITS))
			continue;

		spin_lock(&vb->lock);
		if (vb->free + vb->dirty == VMAP_BBMAP_BITS && vb->dirty != VMAP_BBMAP_BITS) {
			vb->free = 0; /* prevent further allocs after releasing lock */
			vb->dirty = VMAP_BBMAP_BITS; /* prevent purging it again */
			bitmap_fill(vb->alloc_map, VMAP_BBMAP_BITS);
			bitmap_fill(vb->dirty_map, VMAP_BBMAP_BITS);
			spin_lock(&vbq->lock);
			list_del_rcu(&vb->free_list);
			spin_unlock(&vbq->lock);
			spin_unlock(&vb->lock);
			list_add_tail(&vb->purge, &purge);
		} else
			spin_unlock(&vb->lock);
	}
	rcu_read_unlock();

	list_for_each_entry_safe(vb, n_vb, &purge, purge) {
		list_del(&vb->purge);
		free_vmap_block(vb);
	}
}

static void purge_fragmented_blocks_thiscpu(void)
{
	purge_fragmented_blocks(smp_processor_id());
}

static void purge_fragmented_blocks_allcpus(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		purge_fragmented_blocks(cpu);
}

static void *vb_alloc(unsigned long size, gfp_t gfp_mask)
{
	struct vmap_block_queue *vbq;
	struct vmap_block *vb;
	unsigned long addr = 0;
	unsigned int order;
	int purge = 0;

	BUG_ON(size & ~PAGE_MASK);
	BUG_ON(size > PAGE_SIZE*VMAP_MAX_ALLOC);
	order = get_order(size);

again:
	rcu_read_lock();
	vbq = &get_cpu_var(vmap_block_queue);
	list_for_each_entry_rcu(vb, &vbq->free, free_list) {
		int i;

		spin_lock(&vb->lock);
		if (vb->free < 1UL << order)
			goto next;

		i = bitmap_find_free_region(vb->alloc_map,
						VMAP_BBMAP_BITS, order);

		if (i < 0) {
			if (vb->free + vb->dirty == VMAP_BBMAP_BITS) {
				/* fragmented and no outstanding allocations */
				BUG_ON(vb->dirty != VMAP_BBMAP_BITS);
				purge = 1;
			}
			goto next;
		}
		addr = vb->va->va_start + (i << PAGE_SHIFT);
		BUG_ON(addr_to_vb_idx(addr) !=
				addr_to_vb_idx(vb->va->va_start));
		vb->free -= 1UL << order;
		if (vb->free == 0) {
			spin_lock(&vbq->lock);
			list_del_rcu(&vb->free_list);
			spin_unlock(&vbq->lock);
		}
		spin_unlock(&vb->lock);
		break;
next:
		spin_unlock(&vb->lock);
	}

	if (purge)
		purge_fragmented_blocks_thiscpu();

	put_cpu_var(vmap_block_queue);
	rcu_read_unlock();

	if (!addr) {
		vb = new_vmap_block(gfp_mask);
		if (IS_ERR(vb))
			return vb;
		goto again;
	}

	return (void *)addr;
}

static void vb_free(const void *addr, unsigned long size)
{
	unsigned long offset;
	unsigned long vb_idx;
	unsigned int order;
	struct vmap_block *vb;

	BUG_ON(size & ~PAGE_MASK);
	BUG_ON(size > PAGE_SIZE*VMAP_MAX_ALLOC);

	flush_cache_vunmap((unsigned long)addr, (unsigned long)addr + size);

	order = get_order(size);

	offset = (unsigned long)addr & (VMAP_BLOCK_SIZE - 1);

	vb_idx = addr_to_vb_idx((unsigned long)addr);
	rcu_read_lock();
	vb = radix_tree_lookup(&vmap_block_tree, vb_idx);
	rcu_read_unlock();
	BUG_ON(!vb);

	spin_lock(&vb->lock);
	BUG_ON(bitmap_allocate_region(vb->dirty_map, offset >> PAGE_SHIFT, order));

	vb->dirty += 1UL << order;
	if (vb->dirty == VMAP_BBMAP_BITS) {
		BUG_ON(vb->free);
		spin_unlock(&vb->lock);
		free_vmap_block(vb);
	} else
		spin_unlock(&vb->lock);
}

/**
 * vm_unmap_aliases - unmap outstanding lazy aliases in the vmap layer
 *
 * The vmap/vmalloc layer lazily flushes kernel virtual mappings primarily
 * to amortize TLB flushing overheads. What this means is that any page you
 * have now, may, in a former life, have been mapped into kernel virtual
 * address by the vmap layer and so there might be some CPUs with TLB entries
 * still referencing that page (additional to the regular 1:1 kernel mapping).
 *
 * vm_unmap_aliases flushes all such lazy mappings. After it returns, we can
 * be sure that none of the pages we have control over will have any aliases
 * from the vmap layer.
 */
void vm_unmap_aliases(void)
{
	unsigned long start = ULONG_MAX, end = 0;
	int cpu;
	int flush = 0;

	if (unlikely(!vmap_initialized))
		return;

	for_each_possible_cpu(cpu) {
		struct vmap_block_queue *vbq = &per_cpu(vmap_block_queue, cpu);
		struct vmap_block *vb;

		rcu_read_lock();
		list_for_each_entry_rcu(vb, &vbq->free, free_list) {
			int i;

			spin_lock(&vb->lock);
			i = find_first_bit(vb->dirty_map, VMAP_BBMAP_BITS);
			while (i < VMAP_BBMAP_BITS) {
				unsigned long s, e;
				int j;
				j = find_next_zero_bit(vb->dirty_map,
					VMAP_BBMAP_BITS, i);

				s = vb->va->va_start + (i << PAGE_SHIFT);
				e = vb->va->va_start + (j << PAGE_SHIFT);
				vunmap_page_range(s, e);
				flush = 1;

				if (s < start)
					start = s;
				if (e > end)
					end = e;

				i = j;
				i = find_next_bit(vb->dirty_map,
							VMAP_BBMAP_BITS, i);
			}
			spin_unlock(&vb->lock);
		}
		rcu_read_unlock();
	}

	__purge_vmap_area_lazy(&start, &end, 1, flush);
}
EXPORT_SYMBOL_GPL(vm_unmap_aliases);

/**
 * vm_unmap_ram - unmap linear kernel address space set up by vm_map_ram
 * @mem: the pointer returned by vm_map_ram
 * @count: the count passed to that vm_map_ram call (cannot unmap partial)
 */
void vm_unmap_ram(const void *mem, unsigned int count)
{
	unsigned long size = count << PAGE_SHIFT;
	unsigned long addr = (unsigned long)mem;

	BUG_ON(!addr);
	BUG_ON(addr < VMALLOC_START);
	BUG_ON(addr > VMALLOC_END);
	BUG_ON(addr & (PAGE_SIZE-1));

	debug_check_no_locks_freed(mem, size);
	vmap_debug_free_range(addr, addr+size);

	if (likely(count <= VMAP_MAX_ALLOC))
		vb_free(mem, size);
	else
		free_unmap_vmap_area_addr(addr);
}
EXPORT_SYMBOL(vm_unmap_ram);

/**
 * vm_map_ram - map pages linearly into kernel virtual address (vmalloc space)
 * @pages: an array of pointers to the pages to be mapped
 * @count: number of pages
 * @node: prefer to allocate data structures on this node
 * @prot: memory protection to use. PAGE_KERNEL for regular RAM
 *
 * Returns: a pointer to the address that has been mapped, or %NULL on failure
 */
void *vm_map_ram(struct page **pages, unsigned int count, int node, pgprot_t prot)
{
	unsigned long size = count << PAGE_SHIFT;
	unsigned long addr;
	void *mem;

	if (likely(count <= VMAP_MAX_ALLOC)) {
		mem = vb_alloc(size, GFP_KERNEL);
		if (IS_ERR(mem))
			return NULL;
		addr = (unsigned long)mem;
	} else {
		struct vmap_area *va;
		va = alloc_vmap_area(size, PAGE_SIZE,
				VMALLOC_START, VMALLOC_END, node, GFP_KERNEL);
		if (IS_ERR(va))
			return NULL;

		addr = va->va_start;
		mem = (void *)addr;
	}
	if (vmap_page_range(addr, addr + size, prot, pages) < 0) {
		vm_unmap_ram(mem, count);
		return NULL;
	}
	return mem;
}
EXPORT_SYMBOL(vm_map_ram);

/**
 * vm_area_register_early - register vmap area early during boot
 * @vm: vm_struct to register
 * @align: requested alignment
 *
 * This function is used to register kernel vm area before
 * vmalloc_init() is called.  @vm->size and @vm->flags should contain
 * proper values on entry and other fields should be zero.  On return,
 * vm->addr contains the allocated address.
 *
 * DO NOT USE THIS FUNCTION UNLESS YOU KNOW WHAT YOU'RE DOING.
 */
void __init vm_area_register_early(struct vm_struct *vm, size_t align)
{
	static size_t vm_init_off __initdata;
	unsigned long addr;

	addr = ALIGN(VMALLOC_START + vm_init_off, align);
	vm_init_off = PFN_ALIGN(addr + vm->size) - VMALLOC_START;

	vm->addr = (void *)addr;

	vm->next = vmlist;
	vmlist = vm;
}

void __init vmalloc_init(void)
{
	struct vmap_area *va;
	struct vm_struct *tmp;
	int i;

	for_each_possible_cpu(i) {
		struct vmap_block_queue *vbq;

		vbq = &per_cpu(vmap_block_queue, i);
		spin_lock_init(&vbq->lock);
		INIT_LIST_HEAD(&vbq->free);
	}

	/* Import existing vmlist entries. */
	for (tmp = vmlist; tmp; tmp = tmp->next) {
		va = kzalloc(sizeof(struct vmap_area), GFP_NOWAIT);
		va->flags = tmp->flags | VM_VM_AREA;
		va->va_start = (unsigned long)tmp->addr;
		va->va_end = va->va_start + tmp->size;
		__insert_vmap_area(va);
	}

	vmap_area_pcpu_hole = VMALLOC_END;

	vmap_initialized = true;
}

/**
 * map_kernel_range_noflush - map kernel VM area with the specified pages
 * @addr: start of the VM area to map
 * @size: size of the VM area to map
 * @prot: page protection flags to use
 * @pages: pages to map
 *
 * Map PFN_UP(@size) pages at @addr.  The VM area @addr and @size
 * specify should have been allocated using get_vm_area() and its
 * friends.
 *
 * NOTE:
 * This function does NOT do any cache flushing.  The caller is
 * responsible for calling flush_cache_vmap() on to-be-mapped areas
 * before calling this function.
 *
 * RETURNS:
 * The number of pages mapped on success, -errno on failure.
 */
int map_kernel_range_noflush(unsigned long addr, unsigned long size,
			     pgprot_t prot, struct page **pages)
{
	return vmap_page_range_noflush(addr, addr + size, prot, pages);
}

/**
 * unmap_kernel_range_noflush - unmap kernel VM area
 * @addr: start of the VM area to unmap
 * @size: size of the VM area to unmap
 *
 * Unmap PFN_UP(@size) pages at @addr.  The VM area @addr and @size
 * specify should have been allocated using get_vm_area() and its
 * friends.
 *
 * NOTE:
 * This function does NOT do any cache flushing.  The caller is
 * responsible for calling flush_cache_vunmap() on to-be-mapped areas
 * before calling this function and flush_tlb_kernel_range() after.
 */
void unmap_kernel_range_noflush(unsigned long addr, unsigned long size)
{
	vunmap_page_range(addr, addr + size);
}

/**
 * unmap_kernel_range - unmap kernel VM area and flush cache and TLB
 * @addr: start of the VM area to unmap
 * @size: size of the VM area to unmap
 *
 * Similar to unmap_kernel_range_noflush() but flushes vcache before
 * the unmapping and tlb after.
 */
void unmap_kernel_range(unsigned long addr, unsigned long size)
{
	unsigned long end = addr + size;

	flush_cache_vunmap(addr, end);
	vunmap_page_range(addr, end);
	flush_tlb_kernel_range(addr, end);
}

int map_vm_area(struct vm_struct *area, pgprot_t prot, struct page ***pages)
{
	unsigned long addr = (unsigned long)area->addr;
	unsigned long end = addr + area->size - PAGE_SIZE;
	int err;

	err = vmap_page_range(addr, end, prot, *pages);
	if (err > 0) {
		*pages += err;
		err = 0;
	}

	return err;
}
EXPORT_SYMBOL_GPL(map_vm_area);

/*** Old vmalloc interfaces ***/
DEFINE_RWLOCK(vmlist_lock);
struct vm_struct *vmlist;

static void insert_vmalloc_vm(struct vm_struct *vm, struct vmap_area *va,
			      unsigned long flags, void *caller)
{
	struct vm_struct *tmp, **p;

	vm->flags = flags;
	vm->addr = (void *)va->va_start;
	vm->size = va->va_end - va->va_start;
	vm->caller = caller;
	va->private = vm;
	va->flags |= VM_VM_AREA;

	write_lock(&vmlist_lock);
	for (p = &vmlist; (tmp = *p) != NULL; p = &tmp->next) {
		if (tmp->addr >= vm->addr)
			break;
	}
	vm->next = *p;
	*p = vm;
	write_unlock(&vmlist_lock);
}

static struct vm_struct *__get_vm_area_node(unsigned long size,
		unsigned long align, unsigned long flags, unsigned long start,
		unsigned long end, int node, gfp_t gfp_mask, void *caller)
{
	static struct vmap_area *va;
	struct vm_struct *area;

	BUG_ON(in_interrupt());
	if (flags & VM_IOREMAP) {
		int bit = fls(size);

		if (bit > IOREMAP_MAX_ORDER)
			bit = IOREMAP_MAX_ORDER;
		else if (bit < PAGE_SHIFT)
			bit = PAGE_SHIFT;

		align = 1ul << bit;
	}

	size = PAGE_ALIGN(size);
	if (unlikely(!size))
		return NULL;

	area = kzalloc_node(sizeof(*area), gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!area))
		return NULL;

	/*
	 * We always allocate a guard page.
	 */
	size += PAGE_SIZE;

	va = alloc_vmap_area(size, align, start, end, node, gfp_mask);
	if (IS_ERR(va)) {
		kfree(area);
		return NULL;
	}

	insert_vmalloc_vm(area, va, flags, caller);
	return area;
}

struct vm_struct *__get_vm_area(unsigned long size, unsigned long flags,
				unsigned long start, unsigned long end)
{
	return __get_vm_area_node(size, 1, flags, start, end, -1, GFP_KERNEL,
						__builtin_return_address(0));
}
EXPORT_SYMBOL_GPL(__get_vm_area);

struct vm_struct *__get_vm_area_caller(unsigned long size, unsigned long flags,
				       unsigned long start, unsigned long end,
				       void *caller)
{
	return __get_vm_area_node(size, 1, flags, start, end, -1, GFP_KERNEL,
				  caller);
}

/**
 *	get_vm_area  -  reserve a contiguous kernel virtual area
 *	@size:		size of the area
 *	@flags:		%VM_IOREMAP for I/O mappings or VM_ALLOC
 *
 *	Search an area of @size in the kernel virtual mapping area,
 *	and reserved it for out purposes.  Returns the area descriptor
 *	on success or %NULL on failure.
 */
struct vm_struct *get_vm_area(unsigned long size, unsigned long flags)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,
				-1, GFP_KERNEL, __builtin_return_address(0));
}

struct vm_struct *get_vm_area_caller(unsigned long size, unsigned long flags,
				void *caller)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,
						-1, GFP_KERNEL, caller);
}

struct vm_struct *get_vm_area_node(unsigned long size, unsigned long flags,
				   int node, gfp_t gfp_mask)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,
				  node, gfp_mask, __builtin_return_address(0));
}

static struct vm_struct *find_vm_area(const void *addr)
{
	struct vmap_area *va;

	va = find_vmap_area((unsigned long)addr);
	if (va && va->flags & VM_VM_AREA)
		return va->private;

	return NULL;
}

/**
 *	remove_vm_area  -  find and remove a continuous kernel virtual area
 *	@addr:		base address
 *
 *	Search for the kernel VM area starting at @addr, and remove it.
 *	This function returns the found VM area, but using it is NOT safe
 *	on SMP machines, except for its size or flags.
 */
struct vm_struct *remove_vm_area(const void *addr)
{
	struct vmap_area *va;

	va = find_vmap_area((unsigned long)addr);
	if (va && va->flags & VM_VM_AREA) {
		struct vm_struct *vm = va->private;
		struct vm_struct *tmp, **p;
		/*
		 * remove from list and disallow access to this vm_struct
		 * before unmap. (address range confliction is maintained by
		 * vmap.)
		 */
		write_lock(&vmlist_lock);
		for (p = &vmlist; (tmp = *p) != vm; p = &tmp->next)
			;
		*p = tmp->next;
		write_unlock(&vmlist_lock);

		vmap_debug_free_range(va->va_start, va->va_end);
		free_unmap_vmap_area(va);
		vm->size -= PAGE_SIZE;

		return vm;
	}
	return NULL;
}

static void __vunmap(const void *addr, int deallocate_pages)
{
	struct vm_struct *area;

	if (!addr)
		return;

	if ((PAGE_SIZE-1) & (unsigned long)addr) {
		WARN(1, KERN_ERR "Trying to vfree() bad address (%p)\n", addr);
		return;
	}

	area = remove_vm_area(addr);
	if (unlikely(!area)) {
		WARN(1, KERN_ERR "Trying to vfree() nonexistent vm area (%p)\n",
				addr);
		return;
	}

	debug_check_no_locks_freed(addr, area->size);
	debug_check_no_obj_freed(addr, area->size);

	if (deallocate_pages) {
		int i;

		for (i = 0; i < area->nr_pages; i++) {
			struct page *page = area->pages[i];

			BUG_ON(!page);
			__free_page(page);
		}

		if (area->flags & VM_VPAGES)
			vfree(area->pages);
		else
			kfree(area->pages);
	}

	kfree(area);
	return;
}

/**
 *	vfree  -  release memory allocated by vmalloc()
 *	@addr:		memory base address
 *
 *	Free the virtually continuous memory area starting at @addr, as
 *	obtained from vmalloc(), vmalloc_32() or __vmalloc(). If @addr is
 *	NULL, no operation is performed.
 *
 *	Must not be called in interrupt context.
 */
void vfree(const void *addr)
{
	BUG_ON(in_interrupt());

	kmemleak_free(addr);

	__vunmap(addr, 1);
}
EXPORT_SYMBOL(vfree);

/**
 *	vunmap  -  release virtual mapping obtained by vmap()
 *	@addr:		memory base address
 *
 *	Free the virtually contiguous memory area starting at @addr,
 *	which was created from the page array passed to vmap().
 *
 *	Must not be called in interrupt context.
 */
void vunmap(const void *addr)
{
	BUG_ON(in_interrupt());
	might_sleep();
	__vunmap(addr, 0);
}
EXPORT_SYMBOL(vunmap);

/**
 *	vmap  -  map an array of pages into virtually contiguous space
 *	@pages:		array of page pointers
 *	@count:		number of pages to map
 *	@flags:		vm_area->flags
 *	@prot:		page protection for the mapping
 *
 *	Maps @count pages from @pages into contiguous kernel virtual
 *	space.
 */
void *vmap(struct page **pages, unsigned int count,
		unsigned long flags, pgprot_t prot)
{
	struct vm_struct *area;

	might_sleep();

	if (count > totalram_pages)
		return NULL;

	area = get_vm_area_caller((count << PAGE_SHIFT), flags,
					__builtin_return_address(0));
	if (!area)
		return NULL;

	if (map_vm_area(area, prot, &pages)) {
		vunmap(area->addr);
		return NULL;
	}

	return area->addr;
}
EXPORT_SYMBOL(vmap);

static void *__vmalloc_node(unsigned long size, unsigned long align,
			    gfp_t gfp_mask, pgprot_t prot,
			    int node, void *caller);
static void *__vmalloc_area_node(struct vm_struct *area, gfp_t gfp_mask,
				 pgprot_t prot, int node, void *caller)
{
	struct page **pages;
	unsigned int nr_pages, array_size, i;
	gfp_t nested_gfp = (gfp_mask & GFP_RECLAIM_MASK) | __GFP_ZERO;

	nr_pages = (area->size - PAGE_SIZE) >> PAGE_SHIFT;
	array_size = (nr_pages * sizeof(struct page *));

	area->nr_pages = nr_pages;
	/* Please note that the recursion is strictly bounded. */
	if (array_size > PAGE_SIZE) {
		pages = __vmalloc_node(array_size, 1, nested_gfp|__GFP_HIGHMEM,
				PAGE_KERNEL, node, caller);
		area->flags |= VM_VPAGES;
	} else {
		pages = kmalloc_node(array_size, nested_gfp, node);
	}
	area->pages = pages;
	area->caller = caller;
	if (!area->pages) {
		remove_vm_area(area->addr);
		kfree(area);
		return NULL;
	}

	for (i = 0; i < area->nr_pages; i++) {
		struct page *page;

		if (node < 0)
			page = alloc_page(gfp_mask);
		else
			page = alloc_pages_node(node, gfp_mask, 0);

		if (unlikely(!page)) {
			/* Successfully allocated i pages, free them in __vunmap() */
			area->nr_pages = i;
			goto fail;
		}
		area->pages[i] = page;
	}

	if (map_vm_area(area, prot, &pages))
		goto fail;
	return area->addr;

fail:
	vfree(area->addr);
	return NULL;
}

void *__vmalloc_area(struct vm_struct *area, gfp_t gfp_mask, pgprot_t prot)
{
	void *addr = __vmalloc_area_node(area, gfp_mask, prot, -1,
					 __builtin_return_address(0));

	/*
	 * A ref_count = 3 is needed because the vm_struct and vmap_area
	 * structures allocated in the __get_vm_area_node() function contain
	 * references to the virtual address of the vmalloc'ed block.
	 */
	kmemleak_alloc(addr, area->size - PAGE_SIZE, 3, gfp_mask);

	return addr;
}

/**
 *	__vmalloc_node  -  allocate virtually contiguous memory
 *	@size:		allocation size
 *	@align:		desired alignment
 *	@gfp_mask:	flags for the page level allocator
 *	@prot:		protection mask for the allocated pages
 *	@node:		node to use for allocation or -1
 *	@caller:	caller's return address
 *
 *	Allocate enough pages to cover @size from the page level
 *	allocator with @gfp_mask flags.  Map them into contiguous
 *	kernel virtual space, using a pagetable protection of @prot.
 */
static void *__vmalloc_node(unsigned long size, unsigned long align,
			    gfp_t gfp_mask, pgprot_t prot,
			    int node, void *caller)
{
	struct vm_struct *area;
	void *addr;
	unsigned long real_size = size;

	size = PAGE_ALIGN(size);
	if (!size || (size >> PAGE_SHIFT) > totalram_pages)
		return NULL;

	area = __get_vm_area_node(size, align, VM_ALLOC, VMALLOC_START,
				  VMALLOC_END, node, gfp_mask, caller);

	if (!area)
		return NULL;

	addr = __vmalloc_area_node(area, gfp_mask, prot, node, caller);

	/*
	 * A ref_count = 3 is needed because the vm_struct and vmap_area
	 * structures allocated in the __get_vm_area_node() function contain
	 * references to the virtual address of the vmalloc'ed block.
	 */
	kmemleak_alloc(addr, real_size, 3, gfp_mask);

	return addr;
}

void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	return __vmalloc_node(size, 1, gfp_mask, prot, -1,
				__builtin_return_address(0));
}
EXPORT_SYMBOL(__vmalloc);

/**
 *	vmalloc  -  allocate virtually contiguous memory
 *	@size:		allocation size
 *	Allocate enough pages to cover @size from the page level
 *	allocator and map them into contiguous kernel virtual space.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */
void *vmalloc(unsigned long size)
{
	return __vmalloc_node(size, 1, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL,
					-1, __builtin_return_address(0));
}
EXPORT_SYMBOL(vmalloc);

/**
 * vmalloc_user - allocate zeroed virtually contiguous memory for userspace
 * @size: allocation size
 *
 * The resulting memory area is zeroed so it can be mapped to userspace
 * without leaking data.
 */
void *vmalloc_user(unsigned long size)
{
	struct vm_struct *area;
	void *ret;

	ret = __vmalloc_node(size, SHMLBA,
			     GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			     PAGE_KERNEL, -1, __builtin_return_address(0));
	if (ret) {
		area = find_vm_area(ret);
		area->flags |= VM_USERMAP;
	}
	return ret;
}
EXPORT_SYMBOL(vmalloc_user);

/**
 *	vmalloc_node  -  allocate memory on a specific node
 *	@size:		allocation size
 *	@node:		numa node
 *
 *	Allocate enough pages to cover @size from the page level
 *	allocator and map them into contiguous kernel virtual space.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */
void *vmalloc_node(unsigned long size, int node)
{
	return __vmalloc_node(size, 1, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL,
					node, __builtin_return_address(0));
}
EXPORT_SYMBOL(vmalloc_node);

#ifndef PAGE_KERNEL_EXEC
# define PAGE_KERNEL_EXEC PAGE_KERNEL
#endif

/**
 *	vmalloc_exec  -  allocate virtually contiguous, executable memory
 *	@size:		allocation size
 *
 *	Kernel-internal function to allocate enough pages to cover @size
 *	the page level allocator and map them into contiguous and
 *	executable kernel virtual space.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */

void *vmalloc_exec(unsigned long size)
{
	return __vmalloc_node(size, 1, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL_EXEC,
			      -1, __builtin_return_address(0));
}

#if defined(CONFIG_64BIT) && defined(CONFIG_ZONE_DMA32)
#define GFP_VMALLOC32 GFP_DMA32 | GFP_KERNEL
#elif defined(CONFIG_64BIT) && defined(CONFIG_ZONE_DMA)
#define GFP_VMALLOC32 GFP_DMA | GFP_KERNEL
#else
#define GFP_VMALLOC32 GFP_KERNEL
#endif

/**
 *	vmalloc_32  -  allocate virtually contiguous memory (32bit addressable)
 *	@size:		allocation size
 *
 *	Allocate enough 32bit PA addressable pages to cover @size from the
 *	page level allocator and map them into contiguous kernel virtual space.
 */
void *vmalloc_32(unsigned long size)
{
	return __vmalloc_node(size, 1, GFP_VMALLOC32, PAGE_KERNEL,
			      -1, __builtin_return_address(0));
}
EXPORT_SYMBOL(vmalloc_32);

/**
 * vmalloc_32_user - allocate zeroed virtually contiguous 32bit memory
 *	@size:		allocation size
 *
 * The resulting memory area is 32bit addressable and zeroed so it can be
 * mapped to userspace without leaking data.
 */
void *vmalloc_32_user(unsigned long size)
{
	struct vm_struct *area;
	void *ret;

	ret = __vmalloc_node(size, 1, GFP_VMALLOC32 | __GFP_ZERO, PAGE_KERNEL,
			     -1, __builtin_return_address(0));
	if (ret) {
		area = find_vm_area(ret);
		area->flags |= VM_USERMAP;
	}
	return ret;
}
EXPORT_SYMBOL(vmalloc_32_user);

/*
 * small helper routine , copy contents to buf from addr.
 * If the page is not present, fill zero.
 */

static int aligned_vread(char *buf, char *addr, unsigned long count)
{
	struct page *p;
	int copied = 0;

	while (count) {
		unsigned long offset, length;

		offset = (unsigned long)addr & ~PAGE_MASK;
		length = PAGE_SIZE - offset;
		if (length > count)
			length = count;
		p = vmalloc_to_page(addr);
		/*
		 * To do safe access to this _mapped_ area, we need
		 * lock. But adding lock here means that we need to add
		 * overhead of vmalloc()/vfree() calles for this _debug_
		 * interface, rarely used. Instead of that, we'll use
		 * kmap() and get small overhead in this access function.
		 */
		if (p) {
			/*
			 * we can expect USER0 is not used (see vread/vwrite's
			 * function description)
			 */
			void *map = kmap_atomic(p, KM_USER0);
			memcpy(buf, map + offset, length);
			kunmap_atomic(map, KM_USER0);
		} else
			memset(buf, 0, length);

		addr += length;
		buf += length;
		copied += length;
		count -= length;
	}
	return copied;
}

static int aligned_vwrite(char *buf, char *addr, unsigned long count)
{
	struct page *p;
	int copied = 0;

	while (count) {
		unsigned long offset, length;

		offset = (unsigned long)addr & ~PAGE_MASK;
		length = PAGE_SIZE - offset;
		if (length > count)
			length = count;
		p = vmalloc_to_page(addr);
		/*
		 * To do safe access to this _mapped_ area, we need
		 * lock. But adding lock here means that we need to add
		 * overhead of vmalloc()/vfree() calles for this _debug_
		 * interface, rarely used. Instead of that, we'll use
		 * kmap() and get small overhead in this access function.
		 */
		if (p) {
			/*
			 * we can expect USER0 is not used (see vread/vwrite's
			 * function description)
			 */
			void *map = kmap_atomic(p, KM_USER0);
			memcpy(map + offset, buf, length);
			kunmap_atomic(map, KM_USER0);
		}
		addr += length;
		buf += length;
		copied += length;
		count -= length;
	}
	return copied;
}

/**
 *	vread() -  read vmalloc area in a safe way.
 *	@buf:		buffer for reading data
 *	@addr:		vm address.
 *	@count:		number of bytes to be read.
 *
 *	Returns # of bytes which addr and buf should be increased.
 *	(same number to @count). Returns 0 if [addr...addr+count) doesn't
 *	includes any intersect with alive vmalloc area.
 *
 *	This function checks that addr is a valid vmalloc'ed area, and
 *	copy data from that area to a given buffer. If the given memory range
 *	of [addr...addr+count) includes some valid address, data is copied to
 *	proper area of @buf. If there are memory holes, they'll be zero-filled.
 *	IOREMAP area is treated as memory hole and no copy is done.
 *
 *	If [addr...addr+count) doesn't includes any intersects with alive
 *	vm_struct area, returns 0.
 *	@buf should be kernel's buffer. Because	this function uses KM_USER0,
 *	the caller should guarantee KM_USER0 is not used.
 *
 *	Note: In usual ops, vread() is never necessary because the caller
 *	should know vmalloc() area is valid and can use memcpy().
 *	This is for routines which have to access vmalloc area without
 *	any informaion, as /dev/kmem.
 *
 */

long vread(char *buf, char *addr, unsigned long count)
{
	struct vm_struct *tmp;
	char *vaddr, *buf_start = buf;
	unsigned long buflen = count;
	unsigned long n;

	/* Don't allow overflow */
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;

	read_lock(&vmlist_lock);
	for (tmp = vmlist; count && tmp; tmp = tmp->next) {
		vaddr = (char *) tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			*buf = '\0';
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE - addr;
		if (n > count)
			n = count;
		if (!(tmp->flags & VM_IOREMAP))
			aligned_vread(buf, addr, n);
		else /* IOREMAP area is treated as memory hole */
			memset(buf, 0, n);
		buf += n;
		addr += n;
		count -= n;
	}
finished:
	read_unlock(&vmlist_lock);

	if (buf == buf_start)
		return 0;
	/* zero-fill memory holes */
	if (buf != buf_start + buflen)
		memset(buf, 0, buflen - (buf - buf_start));

	return buflen;
}

/**
 *	vwrite() -  write vmalloc area in a safe way.
 *	@buf:		buffer for source data
 *	@addr:		vm address.
 *	@count:		number of bytes to be read.
 *
 *	Returns # of bytes which addr and buf should be incresed.
 *	(same number to @count).
 *	If [addr...addr+count) doesn't includes any intersect with valid
 *	vmalloc area, returns 0.
 *
 *	This function checks that addr is a valid vmalloc'ed area, and
 *	copy data from a buffer to the given addr. If specified range of
 *	[addr...addr+count) includes some valid address, data is copied from
 *	proper area of @buf. If there are memory holes, no copy to hole.
 *	IOREMAP area is treated as memory hole and no copy is done.
 *
 *	If [addr...addr+count) doesn't includes any intersects with alive
 *	vm_struct area, returns 0.
 *	@buf should be kernel's buffer. Because	this function uses KM_USER0,
 *	the caller should guarantee KM_USER0 is not used.
 *
 *	Note: In usual ops, vwrite() is never necessary because the caller
 *	should know vmalloc() area is valid and can use memcpy().
 *	This is for routines which have to access vmalloc area without
 *	any informaion, as /dev/kmem.
 *
 *	The caller should guarantee KM_USER1 is not used.
 */

long vwrite(char *buf, char *addr, unsigned long count)
{
	struct vm_struct *tmp;
	char *vaddr;
	unsigned long n, buflen;
	int copied = 0;

	/* Don't allow overflow */
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;
	buflen = count;

	read_lock(&vmlist_lock);
	for (tmp = vmlist; count && tmp; tmp = tmp->next) {
		vaddr = (char *) tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE - addr;
		if (n > count)
			n = count;
		if (!(tmp->flags & VM_IOREMAP)) {
			aligned_vwrite(buf, addr, n);
			copied++;
		}
		buf += n;
		addr += n;
		count -= n;
	}
finished:
	read_unlock(&vmlist_lock);
	if (!copied)
		return 0;
	return buflen;
}

/**
 *	remap_vmalloc_range  -  map vmalloc pages to userspace
 *	@vma:		vma to cover (map full range of vma)
 *	@addr:		vmalloc memory
 *	@pgoff:		number of pages into addr before first page to map
 *
 *	Returns:	0 for success, -Exxx on failure
 *
 *	This function checks that addr is a valid vmalloc'ed area, and
 *	that it is big enough to cover the vma. Will return failure if
 *	that criteria isn't met.
 *
 *	Similar to remap_pfn_range() (see mm/memory.c)
 */
int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
						unsigned long pgoff)
{
	struct vm_struct *area;
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;

	if ((PAGE_SIZE-1) & (unsigned long)addr)
		return -EINVAL;

	area = find_vm_area(addr);
	if (!area)
		return -EINVAL;

	if (!(area->flags & VM_USERMAP))
		return -EINVAL;

	if (usize + (pgoff << PAGE_SHIFT) > area->size - PAGE_SIZE)
		return -EINVAL;

	addr += pgoff << PAGE_SHIFT;
	do {
		struct page *page = vmalloc_to_page(addr);
		int ret;

		ret = vm_insert_page(vma, uaddr, page);
		if (ret)
			return ret;

		uaddr += PAGE_SIZE;
		addr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	/* Prevent "things" like memory migration? VM_flags need a cleanup... */
	vma->vm_flags |= VM_RESERVED;

	return 0;
}
EXPORT_SYMBOL(remap_vmalloc_range);

/*
 * Implement a stub for vmalloc_sync_all() if the architecture chose not to
 * have one.
 */
void  __attribute__((weak)) vmalloc_sync_all(void)
{
}


static int f(pte_t *pte, pgtable_t table, unsigned long addr, void *data)
{
	/* apply_to_page_range() does all the hard work. */
	return 0;
}

/**
 *	alloc_vm_area - allocate a range of kernel address space
 *	@size:		size of the area
 *
 *	Returns:	NULL on failure, vm_struct on success
 *
 *	This function reserves a range of kernel address space, and
 *	allocates pagetables to map that range.  No actual mappings
 *	are created.  If the kernel address space is not shared
 *	between processes, it syncs the pagetable across all
 *	processes.
 */
struct vm_struct *alloc_vm_area(size_t size)
{
	struct vm_struct *area;

	area = get_vm_area_caller(size, VM_IOREMAP,
				__builtin_return_address(0));
	if (area == NULL)
		return NULL;

	/*
	 * This ensures that page tables are constructed for this region
	 * of kernel virtual address space and mapped into init_mm.
	 */
	if (apply_to_page_range(&init_mm, (unsigned long)area->addr,
				area->size, f, NULL)) {
		free_vm_area(area);
		return NULL;
	}

	/* Make sure the pagetables are constructed in process kernel
	   mappings */
	vmalloc_sync_all();

	return area;
}
EXPORT_SYMBOL_GPL(alloc_vm_area);

void free_vm_area(struct vm_struct *area)
{
	struct vm_struct *ret;
	ret = remove_vm_area(area->addr);
	BUG_ON(ret != area);
	kfree(area);
}
EXPORT_SYMBOL_GPL(free_vm_area);

static struct vmap_area *node_to_va(struct rb_node *n)
{
	return n ? rb_entry(n, struct vmap_area, rb_node) : NULL;
}

/**
 * pvm_find_next_prev - find the next and prev vmap_area surrounding @end
 * @end: target address
 * @pnext: out arg for the next vmap_area
 * @pprev: out arg for the previous vmap_area
 *
 * Returns: %true if either or both of next and prev are found,
 *	    %false if no vmap_area exists
 *
 * Find vmap_areas end addresses of which enclose @end.  ie. if not
 * NULL, *pnext->va_end > @end and *pprev->va_end <= @end.
 */
static bool pvm_find_next_prev(unsigned long end,
			       struct vmap_area **pnext,
			       struct vmap_area **pprev)
{
	struct rb_node *n = vmap_area_root.rb_node;
	struct vmap_area *va = NULL;

	while (n) {
		va = rb_entry(n, struct vmap_area, rb_node);
		if (end < va->va_end)
			n = n->rb_left;
		else if (end > va->va_end)
			n = n->rb_right;
		else
			break;
	}

	if (!va)
		return false;

	if (va->va_end > end) {
		*pnext = va;
		*pprev = node_to_va(rb_prev(&(*pnext)->rb_node));
	} else {
		*pprev = va;
		*pnext = node_to_va(rb_next(&(*pprev)->rb_node));
	}
	return true;
}

/**
 * pvm_determine_end - find the highest aligned address between two vmap_areas
 * @pnext: in/out arg for the next vmap_area
 * @pprev: in/out arg for the previous vmap_area
 * @align: alignment
 *
 * Returns: determined end address
 *
 * Find the highest aligned address between *@pnext and *@pprev below
 * VMALLOC_END.  *@pnext and *@pprev are adjusted so that the aligned
 * down address is between the end addresses of the two vmap_areas.
 *
 * Please note that the address returned by this function may fall
 * inside *@pnext vmap_area.  The caller is responsible for checking
 * that.
 */
static unsigned long pvm_determine_end(struct vmap_area **pnext,
				       struct vmap_area **pprev,
				       unsigned long align)
{
	const unsigned long vmalloc_end = VMALLOC_END & ~(align - 1);
	unsigned long addr;

	if (*pnext)
		addr = min((*pnext)->va_start & ~(align - 1), vmalloc_end);
	else
		addr = vmalloc_end;

	while (*pprev && (*pprev)->va_end > addr) {
		*pnext = *pprev;
		*pprev = node_to_va(rb_prev(&(*pnext)->rb_node));
	}

	return addr;
}

/**
 * pcpu_get_vm_areas - allocate vmalloc areas for percpu allocator
 * @offsets: array containing offset of each area
 * @sizes: array containing size of each area
 * @nr_vms: the number of areas to allocate
 * @align: alignment, all entries in @offsets and @sizes must be aligned to this
 * @gfp_mask: allocation mask
 *
 * Returns: kmalloc'd vm_struct pointer array pointing to allocated
 *	    vm_structs on success, %NULL on failure
 *
 * Percpu allocator wants to use congruent vm areas so that it can
 * maintain the offsets among percpu areas.  This function allocates
 * congruent vmalloc areas for it.  These areas tend to be scattered
 * pretty far, distance between two areas easily going up to
 * gigabytes.  To avoid interacting with regular vmallocs, these areas
 * are allocated from top.
 *
 * Despite its complicated look, this allocator is rather simple.  It
 * does everything top-down and scans areas from the end looking for
 * matching slot.  While scanning, if any of the areas overlaps with
 * existing vmap_area, the base address is pulled down to fit the
 * area.  Scanning is repeated till all the areas fit and then all
 * necessary data structres are inserted and the result is returned.
 */
struct vm_struct **pcpu_get_vm_areas(const unsigned long *offsets,
				     const size_t *sizes, int nr_vms,
				     size_t align, gfp_t gfp_mask)
{
	const unsigned long vmalloc_start = ALIGN(VMALLOC_START, align);
	const unsigned long vmalloc_end = VMALLOC_END & ~(align - 1);
	struct vmap_area **vas, *prev, *next;
	struct vm_struct **vms;
	int area, area2, last_area, term_area;
	unsigned long base, start, end, last_end;
	bool purged = false;

	gfp_mask &= GFP_RECLAIM_MASK;

	/* verify parameters and allocate data structures */
	BUG_ON(align & ~PAGE_MASK || !is_power_of_2(align));
	for (last_area = 0, area = 0; area < nr_vms; area++) {
		start = offsets[area];
		end = start + sizes[area];

		/* is everything aligned properly? */
		BUG_ON(!IS_ALIGNED(offsets[area], align));
		BUG_ON(!IS_ALIGNED(sizes[area], align));

		/* detect the area with the highest address */
		if (start > offsets[last_area])
			last_area = area;

		for (area2 = 0; area2 < nr_vms; area2++) {
			unsigned long start2 = offsets[area2];
			unsigned long end2 = start2 + sizes[area2];

			if (area2 == area)
				continue;

			BUG_ON(start2 >= start && start2 < end);
			BUG_ON(end2 <= end && end2 > start);
		}
	}
	last_end = offsets[last_area] + sizes[last_area];

	if (vmalloc_end - vmalloc_start < last_end) {
		WARN_ON(true);
		return NULL;
	}

	vms = kzalloc(sizeof(vms[0]) * nr_vms, gfp_mask);
	vas = kzalloc(sizeof(vas[0]) * nr_vms, gfp_mask);
	if (!vas || !vms)
		goto err_free;

	for (area = 0; area < nr_vms; area++) {
		vas[area] = kzalloc(sizeof(struct vmap_area), gfp_mask);
		vms[area] = kzalloc(sizeof(struct vm_struct), gfp_mask);
		if (!vas[area] || !vms[area])
			goto err_free;
	}
retry:
	spin_lock(&vmap_area_lock);

	/* start scanning - we scan from the top, begin with the last area */
	area = term_area = last_area;
	start = offsets[area];
	end = start + sizes[area];

	if (!pvm_find_next_prev(vmap_area_pcpu_hole, &next, &prev)) {
		base = vmalloc_end - last_end;
		goto found;
	}
	base = pvm_determine_end(&next, &prev, align) - end;

	while (true) {
		BUG_ON(next && next->va_end <= base + end);
		BUG_ON(prev && prev->va_end > base + end);

		/*
		 * base might have underflowed, add last_end before
		 * comparing.
		 */
		if (base + last_end < vmalloc_start + last_end) {
			spin_unlock(&vmap_area_lock);
			if (!purged) {
				purge_vmap_area_lazy();
				purged = true;
				goto retry;
			}
			goto err_free;
		}

		/*
		 * If next overlaps, move base downwards so that it's
		 * right below next and then recheck.
		 */
		if (next && next->va_start < base + end) {
			base = pvm_determine_end(&next, &prev, align) - end;
			term_area = area;
			continue;
		}

		/*
		 * If prev overlaps, shift down next and prev and move
		 * base so that it's right below new next and then
		 * recheck.
		 */
		if (prev && prev->va_end > base + start)  {
			next = prev;
			prev = node_to_va(rb_prev(&next->rb_node));
			base = pvm_determine_end(&next, &prev, align) - end;
			term_area = area;
			continue;
		}

		/*
		 * This area fits, move on to the previous one.  If
		 * the previous one is the terminal one, we're done.
		 */
		area = (area + nr_vms - 1) % nr_vms;
		if (area == term_area)
			break;
		start = offsets[area];
		end = start + sizes[area];
		pvm_find_next_prev(base + end, &next, &prev);
	}
found:
	/* we've found a fitting base, insert all va's */
	for (area = 0; area < nr_vms; area++) {
		struct vmap_area *va = vas[area];

		va->va_start = base + offsets[area];
		va->va_end = va->va_start + sizes[area];
		__insert_vmap_area(va);
	}

	vmap_area_pcpu_hole = base + offsets[last_area];

	spin_unlock(&vmap_area_lock);

	/* insert all vm's */
	for (area = 0; area < nr_vms; area++)
		insert_vmalloc_vm(vms[area], vas[area], VM_ALLOC,
				  pcpu_get_vm_areas);

	kfree(vas);
	return vms;

err_free:
	for (area = 0; area < nr_vms; area++) {
		if (vas)
			kfree(vas[area]);
		if (vms)
			kfree(vms[area]);
	}
	kfree(vas);
	kfree(vms);
	return NULL;
}

/**
 * pcpu_free_vm_areas - free vmalloc areas for percpu allocator
 * @vms: vm_struct pointer array returned by pcpu_get_vm_areas()
 * @nr_vms: the number of allocated areas
 *
 * Free vm_structs and the array allocated by pcpu_get_vm_areas().
 */
void pcpu_free_vm_areas(struct vm_struct **vms, int nr_vms)
{
	int i;

	for (i = 0; i < nr_vms; i++)
		free_vm_area(vms[i]);
	kfree(vms);
}

#ifdef CONFIG_PROC_FS
static void *s_start(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;
	struct vm_struct *v;

	read_lock(&vmlist_lock);
	v = vmlist;
	while (n > 0 && v) {
		n--;
		v = v->next;
	}
	if (!n)
		return v;

	return NULL;

}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct vm_struct *v = p;

	++*pos;
	return v->next;
}

static void s_stop(struct seq_file *m, void *p)
{
	read_unlock(&vmlist_lock);
}

static void show_numa_info(struct seq_file *m, struct vm_struct *v)
{
	if (NUMA_BUILD) {
		unsigned int nr, *counters = m->private;

		if (!counters)
			return;

		memset(counters, 0, nr_node_ids * sizeof(unsigned int));

		for (nr = 0; nr < v->nr_pages; nr++)
			counters[page_to_nid(v->pages[nr])]++;

		for_each_node_state(nr, N_HIGH_MEMORY)
			if (counters[nr])
				seq_printf(m, " N%u=%u", nr, counters[nr]);
	}
}

static int s_show(struct seq_file *m, void *p)
{
	struct vm_struct *v = p;

	seq_printf(m, "0x%p-0x%p %7ld",
		v->addr, v->addr + v->size, v->size);

	if (v->caller) {
		char buff[KSYM_SYMBOL_LEN];

		seq_putc(m, ' ');
		sprint_symbol(buff, (unsigned long)v->caller);
		seq_puts(m, buff);
	}

	if (v->nr_pages)
		seq_printf(m, " pages=%d", v->nr_pages);

	if (v->phys_addr)
		seq_printf(m, " phys=%llx", (unsigned long long)v->phys_addr);

	if (v->flags & VM_IOREMAP)
		seq_printf(m, " ioremap");

	if (v->flags & VM_ALLOC)
		seq_printf(m, " vmalloc");

	if (v->flags & VM_MAP)
		seq_printf(m, " vmap");

	if (v->flags & VM_USERMAP)
		seq_printf(m, " user");

	if (v->flags & VM_VPAGES)
		seq_printf(m, " vpages");

	show_numa_info(m, v);
	seq_putc(m, '\n');
	return 0;
}

static const struct seq_operations vmalloc_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show,
};

static int vmalloc_open(struct inode *inode, struct file *file)
{
	unsigned int *ptr = NULL;
	int ret;

	if (NUMA_BUILD) {
		ptr = kmalloc(nr_node_ids * sizeof(unsigned int), GFP_KERNEL);
		if (ptr == NULL)
			return -ENOMEM;
	}
	ret = seq_open(file, &vmalloc_op);
	if (!ret) {
		struct seq_file *m = file->private_data;
		m->private = ptr;
	} else
		kfree(ptr);
	return ret;
}

static const struct file_operations proc_vmalloc_operations = {
	.open		= vmalloc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release_private,
};

static int __init proc_vmalloc_init(void)
{
	proc_create("vmallocinfo", S_IRUSR, NULL, &proc_vmalloc_operations);
	return 0;
}
module_init(proc_vmalloc_init);
#endif
