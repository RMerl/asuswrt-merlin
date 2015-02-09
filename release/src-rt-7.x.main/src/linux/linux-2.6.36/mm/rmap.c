/*
 * mm/rmap.c - physical to virtual reverse mappings
 *
 * Copyright 2001, Rik van Riel <riel@conectiva.com.br>
 * Released under the General Public License (GPL).
 *
 * Simple, low overhead reverse mapping scheme.
 * Please try to keep this thing as modular as possible.
 *
 * Provides methods for unmapping each kind of mapped page:
 * the anon methods track anonymous pages, and
 * the file methods track pages belonging to an inode.
 *
 * Original design by Rik van Riel <riel@conectiva.com.br> 2001
 * File methods by Dave McCracken <dmccr@us.ibm.com> 2003, 2004
 * Anonymous methods by Andrea Arcangeli <andrea@suse.de> 2004
 * Contributions by Hugh Dickins 2003, 2004
 */

/*
 * Lock ordering in mm:
 *
 * inode->i_mutex	(while writing or truncating, not reading or faulting)
 *   inode->i_alloc_sem (vmtruncate_range)
 *   mm->mmap_sem
 *     page->flags PG_locked (lock_page)
 *       mapping->i_mmap_lock
 *         anon_vma->lock
 *           mm->page_table_lock or pte_lock
 *             zone->lru_lock (in mark_page_accessed, isolate_lru_page)
 *             swap_lock (in swap_duplicate, swap_info_get)
 *               mmlist_lock (in mmput, drain_mmlist and others)
 *               mapping->private_lock (in __set_page_dirty_buffers)
 *               inode_lock (in set_page_dirty's __mark_inode_dirty)
 *                 sb_lock (within inode_lock in fs/fs-writeback.c)
 *                 mapping->tree_lock (widely used, in set_page_dirty,
 *                           in arch-dependent flush_dcache_mmap_lock,
 *                           within inode_lock in __sync_single_inode)
 *
 * (code doesn't rely on that order so it could be switched around)
 * ->tasklist_lock
 *   anon_vma->lock      (memory_failure, collect_procs_anon)
 *     pte map lock
 */

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/rcupdate.h>
#include <linux/module.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/migrate.h>
#include <linux/hugetlb.h>

#include <asm/tlbflush.h>

#include "internal.h"

static struct kmem_cache *anon_vma_cachep;
static struct kmem_cache *anon_vma_chain_cachep;

static inline struct anon_vma *anon_vma_alloc(void)
{
	return kmem_cache_alloc(anon_vma_cachep, GFP_KERNEL);
}

void anon_vma_free(struct anon_vma *anon_vma)
{
	kmem_cache_free(anon_vma_cachep, anon_vma);
}

static inline struct anon_vma_chain *anon_vma_chain_alloc(void)
{
	return kmem_cache_alloc(anon_vma_chain_cachep, GFP_KERNEL);
}

void anon_vma_chain_free(struct anon_vma_chain *anon_vma_chain)
{
	kmem_cache_free(anon_vma_chain_cachep, anon_vma_chain);
}

/**
 * anon_vma_prepare - attach an anon_vma to a memory region
 * @vma: the memory region in question
 *
 * This makes sure the memory mapping described by 'vma' has
 * an 'anon_vma' attached to it, so that we can associate the
 * anonymous pages mapped into it with that anon_vma.
 *
 * The common case will be that we already have one, but if
 * if not we either need to find an adjacent mapping that we
 * can re-use the anon_vma from (very common when the only
 * reason for splitting a vma has been mprotect()), or we
 * allocate a new one.
 *
 * Anon-vma allocations are very subtle, because we may have
 * optimistically looked up an anon_vma in page_lock_anon_vma()
 * and that may actually touch the spinlock even in the newly
 * allocated vma (it depends on RCU to make sure that the
 * anon_vma isn't actually destroyed).
 *
 * As a result, we need to do proper anon_vma locking even
 * for the new allocation. At the same time, we do not want
 * to do any locking for the common case of already having
 * an anon_vma.
 *
 * This must be called with the mmap_sem held for reading.
 */
int anon_vma_prepare(struct vm_area_struct *vma)
{
	struct anon_vma *anon_vma = vma->anon_vma;
	struct anon_vma_chain *avc;

	might_sleep();
	if (unlikely(!anon_vma)) {
		struct mm_struct *mm = vma->vm_mm;
		struct anon_vma *allocated;

		avc = anon_vma_chain_alloc();
		if (!avc)
			goto out_enomem;

		anon_vma = find_mergeable_anon_vma(vma);
		allocated = NULL;
		if (!anon_vma) {
			anon_vma = anon_vma_alloc();
			if (unlikely(!anon_vma))
				goto out_enomem_free_avc;
			allocated = anon_vma;
			/*
			 * This VMA had no anon_vma yet.  This anon_vma is
			 * the root of any anon_vma tree that might form.
			 */
			anon_vma->root = anon_vma;
		}

		anon_vma_lock(anon_vma);
		/* page_table_lock to protect against threads */
		spin_lock(&mm->page_table_lock);
		if (likely(!vma->anon_vma)) {
			vma->anon_vma = anon_vma;
			avc->anon_vma = anon_vma;
			avc->vma = vma;
			list_add(&avc->same_vma, &vma->anon_vma_chain);
			list_add_tail(&avc->same_anon_vma, &anon_vma->head);
			allocated = NULL;
			avc = NULL;
		}
		spin_unlock(&mm->page_table_lock);
		anon_vma_unlock(anon_vma);

		if (unlikely(allocated))
			anon_vma_free(allocated);
		if (unlikely(avc))
			anon_vma_chain_free(avc);
	}
	return 0;

 out_enomem_free_avc:
	anon_vma_chain_free(avc);
 out_enomem:
	return -ENOMEM;
}

static void anon_vma_chain_link(struct vm_area_struct *vma,
				struct anon_vma_chain *avc,
				struct anon_vma *anon_vma)
{
	avc->vma = vma;
	avc->anon_vma = anon_vma;
	list_add(&avc->same_vma, &vma->anon_vma_chain);

	anon_vma_lock(anon_vma);
	list_add_tail(&avc->same_anon_vma, &anon_vma->head);
	anon_vma_unlock(anon_vma);
}

/*
 * Attach the anon_vmas from src to dst.
 * Returns 0 on success, -ENOMEM on failure.
 */
int anon_vma_clone(struct vm_area_struct *dst, struct vm_area_struct *src)
{
	struct anon_vma_chain *avc, *pavc;

	list_for_each_entry_reverse(pavc, &src->anon_vma_chain, same_vma) {
		avc = anon_vma_chain_alloc();
		if (!avc)
			goto enomem_failure;
		anon_vma_chain_link(dst, avc, pavc->anon_vma);
	}
	return 0;

 enomem_failure:
	unlink_anon_vmas(dst);
	return -ENOMEM;
}

/*
 * Attach vma to its own anon_vma, as well as to the anon_vmas that
 * the corresponding VMA in the parent process is attached to.
 * Returns 0 on success, non-zero on failure.
 */
int anon_vma_fork(struct vm_area_struct *vma, struct vm_area_struct *pvma)
{
	struct anon_vma_chain *avc;
	struct anon_vma *anon_vma;

	/* Don't bother if the parent process has no anon_vma here. */
	if (!pvma->anon_vma)
		return 0;

	/*
	 * First, attach the new VMA to the parent VMA's anon_vmas,
	 * so rmap can find non-COWed pages in child processes.
	 */
	if (anon_vma_clone(vma, pvma))
		return -ENOMEM;

	/* Then add our own anon_vma. */
	anon_vma = anon_vma_alloc();
	if (!anon_vma)
		goto out_error;
	avc = anon_vma_chain_alloc();
	if (!avc)
		goto out_error_free_anon_vma;

	/*
	 * The root anon_vma's spinlock is the lock actually used when we
	 * lock any of the anon_vmas in this anon_vma tree.
	 */
	anon_vma->root = pvma->anon_vma->root;
	/*
	 * With KSM refcounts, an anon_vma can stay around longer than the
	 * process it belongs to.  The root anon_vma needs to be pinned
	 * until this anon_vma is freed, because the lock lives in the root.
	 */
	get_anon_vma(anon_vma->root);
	/* Mark this anon_vma as the one where our new (COWed) pages go. */
	vma->anon_vma = anon_vma;
	anon_vma_chain_link(vma, avc, anon_vma);

	return 0;

 out_error_free_anon_vma:
	anon_vma_free(anon_vma);
 out_error:
	unlink_anon_vmas(vma);
	return -ENOMEM;
}

static void anon_vma_unlink(struct anon_vma_chain *anon_vma_chain)
{
	struct anon_vma *anon_vma = anon_vma_chain->anon_vma;
	int empty;

	/* If anon_vma_fork fails, we can get an empty anon_vma_chain. */
	if (!anon_vma)
		return;

	anon_vma_lock(anon_vma);
	list_del(&anon_vma_chain->same_anon_vma);

	/* We must garbage collect the anon_vma if it's empty */
	empty = list_empty(&anon_vma->head) && !anonvma_external_refcount(anon_vma);
	anon_vma_unlock(anon_vma);

	if (empty) {
		/* We no longer need the root anon_vma */
		if (anon_vma->root != anon_vma)
			drop_anon_vma(anon_vma->root);
		anon_vma_free(anon_vma);
	}
}

void unlink_anon_vmas(struct vm_area_struct *vma)
{
	struct anon_vma_chain *avc, *next;

	/*
	 * Unlink each anon_vma chained to the VMA.  This list is ordered
	 * from newest to oldest, ensuring the root anon_vma gets freed last.
	 */
	list_for_each_entry_safe(avc, next, &vma->anon_vma_chain, same_vma) {
		anon_vma_unlink(avc);
		list_del(&avc->same_vma);
		anon_vma_chain_free(avc);
	}
}

static void anon_vma_ctor(void *data)
{
	struct anon_vma *anon_vma = data;

	spin_lock_init(&anon_vma->lock);
	anonvma_external_refcount_init(anon_vma);
	INIT_LIST_HEAD(&anon_vma->head);
}

void __init anon_vma_init(void)
{
	anon_vma_cachep = kmem_cache_create("anon_vma", sizeof(struct anon_vma),
			0, SLAB_DESTROY_BY_RCU|SLAB_PANIC, anon_vma_ctor);
	anon_vma_chain_cachep = KMEM_CACHE(anon_vma_chain, SLAB_PANIC);
}

/*
 * Getting a lock on a stable anon_vma from a page off the LRU is
 * tricky: page_lock_anon_vma rely on RCU to guard against the races.
 */
struct anon_vma *page_lock_anon_vma(struct page *page)
{
	struct anon_vma *anon_vma, *root_anon_vma;
	unsigned long anon_mapping;

	rcu_read_lock();
	anon_mapping = (unsigned long) ACCESS_ONCE(page->mapping);
	if ((anon_mapping & PAGE_MAPPING_FLAGS) != PAGE_MAPPING_ANON)
		goto out;
	if (!page_mapped(page))
		goto out;

	anon_vma = (struct anon_vma *) (anon_mapping - PAGE_MAPPING_ANON);
	root_anon_vma = ACCESS_ONCE(anon_vma->root);
	spin_lock(&root_anon_vma->lock);

	/*
	 * If this page is still mapped, then its anon_vma cannot have been
	 * freed.  But if it has been unmapped, we have no security against
	 * the anon_vma structure being freed and reused (for another anon_vma:
	 * SLAB_DESTROY_BY_RCU guarantees that - so the spin_lock above cannot
	 * corrupt): with anon_vma_prepare() or anon_vma_fork() redirecting
	 * anon_vma->root before page_unlock_anon_vma() is called to unlock.
	 */
	if (page_mapped(page))
		return anon_vma;

	spin_unlock(&root_anon_vma->lock);
out:
	rcu_read_unlock();
	return NULL;
}

void page_unlock_anon_vma(struct anon_vma *anon_vma)
{
	anon_vma_unlock(anon_vma);
	rcu_read_unlock();
}

/*
 * At what user virtual address is page expected in @vma?
 * Returns virtual address or -EFAULT if page's index/offset is not
 * within the range mapped the @vma.
 */
static inline unsigned long
vma_address(struct page *page, struct vm_area_struct *vma)
{
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	unsigned long address;

	if (unlikely(is_vm_hugetlb_page(vma)))
		pgoff = page->index << huge_page_order(page_hstate(page));
	address = vma->vm_start + ((pgoff - vma->vm_pgoff) << PAGE_SHIFT);
	if (unlikely(address < vma->vm_start || address >= vma->vm_end)) {
		/* page should be within @vma mapping range */
		return -EFAULT;
	}
	return address;
}

/*
 * At what user virtual address is page expected in vma?
 * Caller should check the page is actually part of the vma.
 */
unsigned long page_address_in_vma(struct page *page, struct vm_area_struct *vma)
{
	if (PageAnon(page)) {
		struct anon_vma *page__anon_vma = page_anon_vma(page);
		/*
		 * Note: swapoff's unuse_vma() is more efficient with this
		 * check, and needs it to match anon_vma when KSM is active.
		 */
		if (!vma->anon_vma || !page__anon_vma ||
		    vma->anon_vma->root != page__anon_vma->root)
			return -EFAULT;
	} else if (page->mapping && !(vma->vm_flags & VM_NONLINEAR)) {
		if (!vma->vm_file ||
		    vma->vm_file->f_mapping != page->mapping)
			return -EFAULT;
	} else
		return -EFAULT;
	return vma_address(page, vma);
}

/*
 * Check that @page is mapped at @address into @mm.
 *
 * If @sync is false, page_check_address may perform a racy check to avoid
 * the page table lock when the pte is not present (helpful when reclaiming
 * highly shared pages).
 *
 * On success returns with pte mapped and locked.
 */
pte_t *page_check_address(struct page *page, struct mm_struct *mm,
			  unsigned long address, spinlock_t **ptlp, int sync)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	spinlock_t *ptl;

	if (unlikely(PageHuge(page))) {
		pte = huge_pte_offset(mm, address);
		ptl = &mm->page_table_lock;
		goto check;
	}

	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		return NULL;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		return NULL;

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return NULL;

	pte = pte_offset_map(pmd, address);
	/* Make a quick check before getting the lock */
	if (!sync && !pte_present(*pte)) {
		pte_unmap(pte);
		return NULL;
	}

	ptl = pte_lockptr(mm, pmd);
check:
	spin_lock(ptl);
	if (pte_present(*pte) && page_to_pfn(page) == pte_pfn(*pte)) {
		*ptlp = ptl;
		return pte;
	}
	pte_unmap_unlock(pte, ptl);
	return NULL;
}

/**
 * page_mapped_in_vma - check whether a page is really mapped in a VMA
 * @page: the page to test
 * @vma: the VMA to test
 *
 * Returns 1 if the page is mapped into the page tables of the VMA, 0
 * if the page is not mapped into the page tables of this VMA.  Only
 * valid for normal file or anonymous VMAs.
 */
int page_mapped_in_vma(struct page *page, struct vm_area_struct *vma)
{
	unsigned long address;
	pte_t *pte;
	spinlock_t *ptl;

	address = vma_address(page, vma);
	if (address == -EFAULT)		/* out of vma range */
		return 0;
	pte = page_check_address(page, vma->vm_mm, address, &ptl, 1);
	if (!pte)			/* the page is not in this mm */
		return 0;
	pte_unmap_unlock(pte, ptl);

	return 1;
}

/*
 * Subfunctions of page_referenced: page_referenced_one called
 * repeatedly from either page_referenced_anon or page_referenced_file.
 */
int page_referenced_one(struct page *page, struct vm_area_struct *vma,
			unsigned long address, unsigned int *mapcount,
			unsigned long *vm_flags)
{
	struct mm_struct *mm = vma->vm_mm;
	pte_t *pte;
	spinlock_t *ptl;
	int referenced = 0;

	pte = page_check_address(page, mm, address, &ptl, 0);
	if (!pte)
		goto out;

	/*
	 * Don't want to elevate referenced for mlocked page that gets this far,
	 * in order that it progresses to try_to_unmap and is moved to the
	 * unevictable list.
	 */
	if (vma->vm_flags & VM_LOCKED) {
		*mapcount = 1;	/* break early from loop */
		*vm_flags |= VM_LOCKED;
		goto out_unmap;
	}

	if (ptep_clear_flush_young_notify(vma, address, pte)) {
		/*
		 * Don't treat a reference through a sequentially read
		 * mapping as such.  If the page has been used in
		 * another mapping, we will catch it; if this other
		 * mapping is already gone, the unmap path will have
		 * set PG_referenced or activated the page.
		 */
		if (likely(!VM_SequentialReadHint(vma)))
			referenced++;
	}

	/* Pretend the page is referenced if the task has the
	   swap token and is in the middle of a page fault. */
	if (mm != current->mm && has_swap_token(mm) &&
			rwsem_is_locked(&mm->mmap_sem))
		referenced++;

out_unmap:
	(*mapcount)--;
	pte_unmap_unlock(pte, ptl);

	if (referenced)
		*vm_flags |= vma->vm_flags;
out:
	return referenced;
}

static int page_referenced_anon(struct page *page,
				struct mem_cgroup *mem_cont,
				unsigned long *vm_flags)
{
	unsigned int mapcount;
	struct anon_vma *anon_vma;
	struct anon_vma_chain *avc;
	int referenced = 0;

	anon_vma = page_lock_anon_vma(page);
	if (!anon_vma)
		return referenced;

	mapcount = page_mapcount(page);
	list_for_each_entry(avc, &anon_vma->head, same_anon_vma) {
		struct vm_area_struct *vma = avc->vma;
		unsigned long address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		/*
		 * If we are reclaiming on behalf of a cgroup, skip
		 * counting on behalf of references from different
		 * cgroups
		 */
		if (mem_cont && !mm_match_cgroup(vma->vm_mm, mem_cont))
			continue;
		referenced += page_referenced_one(page, vma, address,
						  &mapcount, vm_flags);
		if (!mapcount)
			break;
	}

	page_unlock_anon_vma(anon_vma);
	return referenced;
}

/**
 * page_referenced_file - referenced check for object-based rmap
 * @page: the page we're checking references on.
 * @mem_cont: target memory controller
 * @vm_flags: collect encountered vma->vm_flags who actually referenced the page
 *
 * For an object-based mapped page, find all the places it is mapped and
 * check/clear the referenced flag.  This is done by following the page->mapping
 * pointer, then walking the chain of vmas it holds.  It returns the number
 * of references it found.
 *
 * This function is only called from page_referenced for object-based pages.
 */
static int page_referenced_file(struct page *page,
				struct mem_cgroup *mem_cont,
				unsigned long *vm_flags)
{
	unsigned int mapcount;
	struct address_space *mapping = page->mapping;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	struct prio_tree_iter iter;
	int referenced = 0;

	/*
	 * The caller's checks on page->mapping and !PageAnon have made
	 * sure that this is a file page: the check for page->mapping
	 * excludes the case just before it gets set on an anon page.
	 */
	BUG_ON(PageAnon(page));

	/*
	 * The page lock not only makes sure that page->mapping cannot
	 * suddenly be NULLified by truncation, it makes sure that the
	 * structure at mapping cannot be freed and reused yet,
	 * so we can safely take mapping->i_mmap_lock.
	 */
	BUG_ON(!PageLocked(page));

	spin_lock(&mapping->i_mmap_lock);

	/*
	 * i_mmap_lock does not stabilize mapcount at all, but mapcount
	 * is more likely to be accurate if we note it after spinning.
	 */
	mapcount = page_mapcount(page);

	vma_prio_tree_foreach(vma, &iter, &mapping->i_mmap, pgoff, pgoff) {
		unsigned long address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		/*
		 * If we are reclaiming on behalf of a cgroup, skip
		 * counting on behalf of references from different
		 * cgroups
		 */
		if (mem_cont && !mm_match_cgroup(vma->vm_mm, mem_cont))
			continue;
		referenced += page_referenced_one(page, vma, address,
						  &mapcount, vm_flags);
		if (!mapcount)
			break;
	}

	spin_unlock(&mapping->i_mmap_lock);
	return referenced;
}

/**
 * page_referenced - test if the page was referenced
 * @page: the page to test
 * @is_locked: caller holds lock on the page
 * @mem_cont: target memory controller
 * @vm_flags: collect encountered vma->vm_flags who actually referenced the page
 *
 * Quick test_and_clear_referenced for all mappings to a page,
 * returns the number of ptes which referenced the page.
 */
int page_referenced(struct page *page,
		    int is_locked,
		    struct mem_cgroup *mem_cont,
		    unsigned long *vm_flags)
{
	int referenced = 0;
	int we_locked = 0;

	*vm_flags = 0;
	if (page_mapped(page) && page_rmapping(page)) {
		if (!is_locked && (!PageAnon(page) || PageKsm(page))) {
			we_locked = trylock_page(page);
			if (!we_locked) {
				referenced++;
				goto out;
			}
		}
		if (unlikely(PageKsm(page)))
			referenced += page_referenced_ksm(page, mem_cont,
								vm_flags);
		else if (PageAnon(page))
			referenced += page_referenced_anon(page, mem_cont,
								vm_flags);
		else if (page->mapping)
			referenced += page_referenced_file(page, mem_cont,
								vm_flags);
		if (we_locked)
			unlock_page(page);
	}
out:
	if (page_test_and_clear_young(page))
		referenced++;

	return referenced;
}

static int page_mkclean_one(struct page *page, struct vm_area_struct *vma,
			    unsigned long address)
{
	struct mm_struct *mm = vma->vm_mm;
	pte_t *pte;
	spinlock_t *ptl;
	int ret = 0;

	pte = page_check_address(page, mm, address, &ptl, 1);
	if (!pte)
		goto out;

	if (pte_dirty(*pte) || pte_write(*pte)) {
		pte_t entry;

		flush_cache_page(vma, address, pte_pfn(*pte));
		entry = ptep_clear_flush_notify(vma, address, pte);
		entry = pte_wrprotect(entry);
		entry = pte_mkclean(entry);
		set_pte_at(mm, address, pte, entry);
		ret = 1;
	}

	pte_unmap_unlock(pte, ptl);
out:
	return ret;
}

static int page_mkclean_file(struct address_space *mapping, struct page *page)
{
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	struct prio_tree_iter iter;
	int ret = 0;

	BUG_ON(PageAnon(page));

	spin_lock(&mapping->i_mmap_lock);
	vma_prio_tree_foreach(vma, &iter, &mapping->i_mmap, pgoff, pgoff) {
		if (vma->vm_flags & VM_SHARED) {
			unsigned long address = vma_address(page, vma);
			if (address == -EFAULT)
				continue;
			ret += page_mkclean_one(page, vma, address);
		}
	}
	spin_unlock(&mapping->i_mmap_lock);
	return ret;
}

int page_mkclean(struct page *page)
{
	int ret = 0;

	BUG_ON(!PageLocked(page));

	if (page_mapped(page)) {
		struct address_space *mapping = page_mapping(page);
		if (mapping) {
			ret = page_mkclean_file(mapping, page);
			if (page_test_dirty(page)) {
				page_clear_dirty(page);
				ret = 1;
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(page_mkclean);

/**
 * page_move_anon_rmap - move a page to our anon_vma
 * @page:	the page to move to our anon_vma
 * @vma:	the vma the page belongs to
 * @address:	the user virtual address mapped
 *
 * When a page belongs exclusively to one process after a COW event,
 * that page can be moved into the anon_vma that belongs to just that
 * process, so the rmap code will not search the parent or sibling
 * processes.
 */
void page_move_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address)
{
	struct anon_vma *anon_vma = vma->anon_vma;

	VM_BUG_ON(!PageLocked(page));
	VM_BUG_ON(!anon_vma);
	VM_BUG_ON(page->index != linear_page_index(vma, address));

	anon_vma = (void *) anon_vma + PAGE_MAPPING_ANON;
	page->mapping = (struct address_space *) anon_vma;
}

/**
 * __page_set_anon_rmap - setup new anonymous rmap
 * @page:	the page to add the mapping to
 * @vma:	the vm area in which the mapping is added
 * @address:	the user virtual address mapped
 * @exclusive:	the page is exclusively owned by the current process
 */
static void __page_set_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address, int exclusive)
{
	struct anon_vma *anon_vma = vma->anon_vma;

	BUG_ON(!anon_vma);

	/*
	 * If the page isn't exclusively mapped into this vma,
	 * we must use the _oldest_ possible anon_vma for the
	 * page mapping!
	 */
	if (!exclusive) {
		if (PageAnon(page))
			return;
		anon_vma = anon_vma->root;
	} else {
		/*
		 * In this case, swapped-out-but-not-discarded swap-cache
		 * is remapped. So, no need to update page->mapping here.
		 * We convice anon_vma poitned by page->mapping is not obsolete
		 * because vma->anon_vma is necessary to be a family of it.
		 */
		if (PageAnon(page))
			return;
	}

	anon_vma = (void *) anon_vma + PAGE_MAPPING_ANON;
	page->mapping = (struct address_space *) anon_vma;
	page->index = linear_page_index(vma, address);
}

/**
 * __page_check_anon_rmap - sanity check anonymous rmap addition
 * @page:	the page to add the mapping to
 * @vma:	the vm area in which the mapping is added
 * @address:	the user virtual address mapped
 */
static void __page_check_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address)
{
#ifdef CONFIG_DEBUG_VM
	/*
	 * The page's anon-rmap details (mapping and index) are guaranteed to
	 * be set up correctly at this point.
	 *
	 * We have exclusion against page_add_anon_rmap because the caller
	 * always holds the page locked, except if called from page_dup_rmap,
	 * in which case the page is already known to be setup.
	 *
	 * We have exclusion against page_add_new_anon_rmap because those pages
	 * are initially only visible via the pagetables, and the pte is locked
	 * over the call to page_add_new_anon_rmap.
	 */
	BUG_ON(page_anon_vma(page)->root != vma->anon_vma->root);
	BUG_ON(page->index != linear_page_index(vma, address));
#endif
}

/**
 * page_add_anon_rmap - add pte mapping to an anonymous page
 * @page:	the page to add the mapping to
 * @vma:	the vm area in which the mapping is added
 * @address:	the user virtual address mapped
 *
 * The caller needs to hold the pte lock, and the page must be locked in
 * the anon_vma case: to serialize mapping,index checking after setting,
 * and to ensure that PageAnon is not being upgraded racily to PageKsm
 * (but PageKsm is never downgraded to PageAnon).
 */
void page_add_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address)
{
	do_page_add_anon_rmap(page, vma, address, 0);
}

/*
 * Special version of the above for do_swap_page, which often runs
 * into pages that are exclusively owned by the current process.
 * Everybody else should continue to use page_add_anon_rmap above.
 */
void do_page_add_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address, int exclusive)
{
	int first = atomic_inc_and_test(&page->_mapcount);
	if (first)
		__inc_zone_page_state(page, NR_ANON_PAGES);
	if (unlikely(PageKsm(page)))
		return;

	VM_BUG_ON(!PageLocked(page));
	VM_BUG_ON(address < vma->vm_start || address >= vma->vm_end);
	if (first)
		__page_set_anon_rmap(page, vma, address, exclusive);
	else
		__page_check_anon_rmap(page, vma, address);
}

/**
 * page_add_new_anon_rmap - add pte mapping to a new anonymous page
 * @page:	the page to add the mapping to
 * @vma:	the vm area in which the mapping is added
 * @address:	the user virtual address mapped
 *
 * Same as page_add_anon_rmap but must only be called on *new* pages.
 * This means the inc-and-test can be bypassed.
 * Page does not have to be locked.
 */
void page_add_new_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address)
{
	VM_BUG_ON(address < vma->vm_start || address >= vma->vm_end);
	SetPageSwapBacked(page);
	atomic_set(&page->_mapcount, 0); /* increment count (starts at -1) */
	__inc_zone_page_state(page, NR_ANON_PAGES);
	__page_set_anon_rmap(page, vma, address, 1);
	if (page_evictable(page, vma))
		lru_cache_add_lru(page, LRU_ACTIVE_ANON);
	else
		add_page_to_unevictable_list(page);
}

/**
 * page_add_file_rmap - add pte mapping to a file page
 * @page: the page to add the mapping to
 *
 * The caller needs to hold the pte lock.
 */
void page_add_file_rmap(struct page *page)
{
	if (atomic_inc_and_test(&page->_mapcount)) {
		__inc_zone_page_state(page, NR_FILE_MAPPED);
		mem_cgroup_update_file_mapped(page, 1);
	}
}

/**
 * page_remove_rmap - take down pte mapping from a page
 * @page: page to remove mapping from
 *
 * The caller needs to hold the pte lock.
 */
void page_remove_rmap(struct page *page)
{
	/* page still mapped by someone else? */
	if (!atomic_add_negative(-1, &page->_mapcount))
		return;

	/*
	 * Now that the last pte has gone, s390 must transfer dirty
	 * flag from storage key to struct page.  We can usually skip
	 * this if the page is anon, so about to be freed; but perhaps
	 * not if it's in swapcache - there might be another pte slot
	 * containing the swap entry, but page not yet written to swap.
	 */
	if ((!PageAnon(page) || PageSwapCache(page)) && page_test_dirty(page)) {
		page_clear_dirty(page);
		set_page_dirty(page);
	}
	/*
	 * Hugepages are not counted in NR_ANON_PAGES nor NR_FILE_MAPPED
	 * and not charged by memcg for now.
	 */
	if (unlikely(PageHuge(page)))
		return;
	if (PageAnon(page)) {
		mem_cgroup_uncharge_page(page);
		__dec_zone_page_state(page, NR_ANON_PAGES);
	} else {
		__dec_zone_page_state(page, NR_FILE_MAPPED);
		mem_cgroup_update_file_mapped(page, -1);
	}
	/*
	 * It would be tidy to reset the PageAnon mapping here,
	 * but that might overwrite a racing page_add_anon_rmap
	 * which increments mapcount after us but sets mapping
	 * before us: so leave the reset to free_hot_cold_page,
	 * and remember that it's only reliable while mapped.
	 * Leaving it set also helps swapoff to reinstate ptes
	 * faster for those pages still in swapcache.
	 */
}

/*
 * Subfunctions of try_to_unmap: try_to_unmap_one called
 * repeatedly from either try_to_unmap_anon or try_to_unmap_file.
 */
int try_to_unmap_one(struct page *page, struct vm_area_struct *vma,
		     unsigned long address, enum ttu_flags flags)
{
	struct mm_struct *mm = vma->vm_mm;
	pte_t *pte;
	pte_t pteval;
	spinlock_t *ptl;
	int ret = SWAP_AGAIN;

	pte = page_check_address(page, mm, address, &ptl, 0);
	if (!pte)
		goto out;

	/*
	 * If the page is mlock()d, we cannot swap it out.
	 * If it's recently referenced (perhaps page_referenced
	 * skipped over this mm) then we should reactivate it.
	 */
	if (!(flags & TTU_IGNORE_MLOCK)) {
		if (vma->vm_flags & VM_LOCKED)
			goto out_mlock;

		if (TTU_ACTION(flags) == TTU_MUNLOCK)
			goto out_unmap;
	}
	if (!(flags & TTU_IGNORE_ACCESS)) {
		if (ptep_clear_flush_young_notify(vma, address, pte)) {
			ret = SWAP_FAIL;
			goto out_unmap;
		}
  	}

	/* Nuke the page table entry. */
	flush_cache_page(vma, address, page_to_pfn(page));
	pteval = ptep_clear_flush_notify(vma, address, pte);

	/* Move the dirty bit to the physical page now the pte is gone. */
	if (pte_dirty(pteval))
		set_page_dirty(page);

	/* Update high watermark before we lower rss */
	update_hiwater_rss(mm);

	if (PageHWPoison(page) && !(flags & TTU_IGNORE_HWPOISON)) {
		if (PageAnon(page))
			dec_mm_counter(mm, MM_ANONPAGES);
		else
			dec_mm_counter(mm, MM_FILEPAGES);
		set_pte_at(mm, address, pte,
				swp_entry_to_pte(make_hwpoison_entry(page)));
	} else if (PageAnon(page)) {
		swp_entry_t entry = { .val = page_private(page) };

		if (PageSwapCache(page)) {
			/*
			 * Store the swap location in the pte.
			 * See handle_pte_fault() ...
			 */
			if (swap_duplicate(entry) < 0) {
				set_pte_at(mm, address, pte, pteval);
				ret = SWAP_FAIL;
				goto out_unmap;
			}
			if (list_empty(&mm->mmlist)) {
				spin_lock(&mmlist_lock);
				if (list_empty(&mm->mmlist))
					list_add(&mm->mmlist, &init_mm.mmlist);
				spin_unlock(&mmlist_lock);
			}
			dec_mm_counter(mm, MM_ANONPAGES);
			inc_mm_counter(mm, MM_SWAPENTS);
		} else if (PAGE_MIGRATION) {
			/*
			 * Store the pfn of the page in a special migration
			 * pte. do_swap_page() will wait until the migration
			 * pte is removed and then restart fault handling.
			 */
			BUG_ON(TTU_ACTION(flags) != TTU_MIGRATION);
			entry = make_migration_entry(page, pte_write(pteval));
		}
		set_pte_at(mm, address, pte, swp_entry_to_pte(entry));
		BUG_ON(pte_file(*pte));
	} else if (PAGE_MIGRATION && (TTU_ACTION(flags) == TTU_MIGRATION)) {
		/* Establish migration entry for a file page */
		swp_entry_t entry;
		entry = make_migration_entry(page, pte_write(pteval));
		set_pte_at(mm, address, pte, swp_entry_to_pte(entry));
	} else
		dec_mm_counter(mm, MM_FILEPAGES);

	page_remove_rmap(page);
	page_cache_release(page);

out_unmap:
	pte_unmap_unlock(pte, ptl);
out:
	return ret;

out_mlock:
	pte_unmap_unlock(pte, ptl);


	/*
	 * We need mmap_sem locking, Otherwise VM_LOCKED check makes
	 * unstable result and race. Plus, We can't wait here because
	 * we now hold anon_vma->lock or mapping->i_mmap_lock.
	 * if trylock failed, the page remain in evictable lru and later
	 * vmscan could retry to move the page to unevictable lru if the
	 * page is actually mlocked.
	 */
	if (down_read_trylock(&vma->vm_mm->mmap_sem)) {
		if (vma->vm_flags & VM_LOCKED) {
			mlock_vma_page(page);
			ret = SWAP_MLOCK;
		}
		up_read(&vma->vm_mm->mmap_sem);
	}
	return ret;
}

/*
 * objrmap doesn't work for nonlinear VMAs because the assumption that
 * offset-into-file correlates with offset-into-virtual-addresses does not hold.
 * Consequently, given a particular page and its ->index, we cannot locate the
 * ptes which are mapping that page without an exhaustive linear search.
 *
 * So what this code does is a mini "virtual scan" of each nonlinear VMA which
 * maps the file to which the target page belongs.  The ->vm_private_data field
 * holds the current cursor into that scan.  Successive searches will circulate
 * around the vma's virtual address space.
 *
 * So as more replacement pressure is applied to the pages in a nonlinear VMA,
 * more scanning pressure is placed against them as well.   Eventually pages
 * will become fully unmapped and are eligible for eviction.
 *
 * For very sparsely populated VMAs this is a little inefficient - chances are
 * there there won't be many ptes located within the scan cluster.  In this case
 * maybe we could scan further - to the end of the pte page, perhaps.
 *
 * Mlocked pages:  check VM_LOCKED under mmap_sem held for read, if we can
 * acquire it without blocking.  If vma locked, mlock the pages in the cluster,
 * rather than unmapping them.  If we encounter the "check_page" that vmscan is
 * trying to unmap, return SWAP_MLOCK, else default SWAP_AGAIN.
 */
#define CLUSTER_SIZE	min(32*PAGE_SIZE, PMD_SIZE)
#define CLUSTER_MASK	(~(CLUSTER_SIZE - 1))

static int try_to_unmap_cluster(unsigned long cursor, unsigned int *mapcount,
		struct vm_area_struct *vma, struct page *check_page)
{
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t pteval;
	spinlock_t *ptl;
	struct page *page;
	unsigned long address;
	unsigned long end;
	int ret = SWAP_AGAIN;
	int locked_vma = 0;

	address = (vma->vm_start + cursor) & CLUSTER_MASK;
	end = address + CLUSTER_SIZE;
	if (address < vma->vm_start)
		address = vma->vm_start;
	if (end > vma->vm_end)
		end = vma->vm_end;

	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		return ret;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		return ret;

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return ret;

	/*
	 * If we can acquire the mmap_sem for read, and vma is VM_LOCKED,
	 * keep the sem while scanning the cluster for mlocking pages.
	 */
	if (down_read_trylock(&vma->vm_mm->mmap_sem)) {
		locked_vma = (vma->vm_flags & VM_LOCKED);
		if (!locked_vma)
			up_read(&vma->vm_mm->mmap_sem); /* don't need it */
	}

	pte = pte_offset_map_lock(mm, pmd, address, &ptl);

	/* Update high watermark before we lower rss */
	update_hiwater_rss(mm);

	for (; address < end; pte++, address += PAGE_SIZE) {
		if (!pte_present(*pte))
			continue;
		page = vm_normal_page(vma, address, *pte);
		BUG_ON(!page || PageAnon(page));

		if (locked_vma) {
			mlock_vma_page(page);   /* no-op if already mlocked */
			if (page == check_page)
				ret = SWAP_MLOCK;
			continue;	/* don't unmap */
		}

		if (ptep_clear_flush_young_notify(vma, address, pte))
			continue;

		/* Nuke the page table entry. */
		flush_cache_page(vma, address, pte_pfn(*pte));
		pteval = ptep_clear_flush_notify(vma, address, pte);

		/* If nonlinear, store the file page offset in the pte. */
		if (page->index != linear_page_index(vma, address))
			set_pte_at(mm, address, pte, pgoff_to_pte(page->index));

		/* Move the dirty bit to the physical page now the pte is gone. */
		if (pte_dirty(pteval))
			set_page_dirty(page);

		page_remove_rmap(page);
		page_cache_release(page);
		dec_mm_counter(mm, MM_FILEPAGES);
		(*mapcount)--;
	}
	pte_unmap_unlock(pte - 1, ptl);
	if (locked_vma)
		up_read(&vma->vm_mm->mmap_sem);
	return ret;
}

static bool is_vma_temporary_stack(struct vm_area_struct *vma)
{
	int maybe_stack = vma->vm_flags & (VM_GROWSDOWN | VM_GROWSUP);

	if (!maybe_stack)
		return false;

	if ((vma->vm_flags & VM_STACK_INCOMPLETE_SETUP) ==
						VM_STACK_INCOMPLETE_SETUP)
		return true;

	return false;
}

/**
 * try_to_unmap_anon - unmap or unlock anonymous page using the object-based
 * rmap method
 * @page: the page to unmap/unlock
 * @flags: action and flags
 *
 * Find all the mappings of a page using the mapping pointer and the vma chains
 * contained in the anon_vma struct it points to.
 *
 * This function is only called from try_to_unmap/try_to_munlock for
 * anonymous pages.
 * When called from try_to_munlock(), the mmap_sem of the mm containing the vma
 * where the page was found will be held for write.  So, we won't recheck
 * vm_flags for that VMA.  That should be OK, because that vma shouldn't be
 * 'LOCKED.
 */
static int try_to_unmap_anon(struct page *page, enum ttu_flags flags)
{
	struct anon_vma *anon_vma;
	struct anon_vma_chain *avc;
	int ret = SWAP_AGAIN;

	anon_vma = page_lock_anon_vma(page);
	if (!anon_vma)
		return ret;

	list_for_each_entry(avc, &anon_vma->head, same_anon_vma) {
		struct vm_area_struct *vma = avc->vma;
		unsigned long address;

		/*
		 * During exec, a temporary VMA is setup and later moved.
		 * The VMA is moved under the anon_vma lock but not the
		 * page tables leading to a race where migration cannot
		 * find the migration ptes. Rather than increasing the
		 * locking requirements of exec(), migration skips
		 * temporary VMAs until after exec() completes.
		 */
		if (PAGE_MIGRATION && (flags & TTU_MIGRATION) &&
				is_vma_temporary_stack(vma))
			continue;

		address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		ret = try_to_unmap_one(page, vma, address, flags);
		if (ret != SWAP_AGAIN || !page_mapped(page))
			break;
	}

	page_unlock_anon_vma(anon_vma);
	return ret;
}

/**
 * try_to_unmap_file - unmap/unlock file page using the object-based rmap method
 * @page: the page to unmap/unlock
 * @flags: action and flags
 *
 * Find all the mappings of a page using the mapping pointer and the vma chains
 * contained in the address_space struct it points to.
 *
 * This function is only called from try_to_unmap/try_to_munlock for
 * object-based pages.
 * When called from try_to_munlock(), the mmap_sem of the mm containing the vma
 * where the page was found will be held for write.  So, we won't recheck
 * vm_flags for that VMA.  That should be OK, because that vma shouldn't be
 * 'LOCKED.
 */
static int try_to_unmap_file(struct page *page, enum ttu_flags flags)
{
	struct address_space *mapping = page->mapping;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	struct prio_tree_iter iter;
	int ret = SWAP_AGAIN;
	unsigned long cursor;
	unsigned long max_nl_cursor = 0;
	unsigned long max_nl_size = 0;
	unsigned int mapcount;

	spin_lock(&mapping->i_mmap_lock);
	vma_prio_tree_foreach(vma, &iter, &mapping->i_mmap, pgoff, pgoff) {
		unsigned long address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		ret = try_to_unmap_one(page, vma, address, flags);
		if (ret != SWAP_AGAIN || !page_mapped(page))
			goto out;
	}

	if (list_empty(&mapping->i_mmap_nonlinear))
		goto out;

	/*
	 * We don't bother to try to find the munlocked page in nonlinears.
	 * It's costly. Instead, later, page reclaim logic may call
	 * try_to_unmap(TTU_MUNLOCK) and recover PG_mlocked lazily.
	 */
	if (TTU_ACTION(flags) == TTU_MUNLOCK)
		goto out;

	list_for_each_entry(vma, &mapping->i_mmap_nonlinear,
						shared.vm_set.list) {
		cursor = (unsigned long) vma->vm_private_data;
		if (cursor > max_nl_cursor)
			max_nl_cursor = cursor;
		cursor = vma->vm_end - vma->vm_start;
		if (cursor > max_nl_size)
			max_nl_size = cursor;
	}

	if (max_nl_size == 0) {	/* all nonlinears locked or reserved ? */
		ret = SWAP_FAIL;
		goto out;
	}

	/*
	 * We don't try to search for this page in the nonlinear vmas,
	 * and page_referenced wouldn't have found it anyway.  Instead
	 * just walk the nonlinear vmas trying to age and unmap some.
	 * The mapcount of the page we came in with is irrelevant,
	 * but even so use it as a guide to how hard we should try?
	 */
	mapcount = page_mapcount(page);
	if (!mapcount)
		goto out;
	cond_resched_lock(&mapping->i_mmap_lock);

	max_nl_size = (max_nl_size + CLUSTER_SIZE - 1) & CLUSTER_MASK;
	if (max_nl_cursor == 0)
		max_nl_cursor = CLUSTER_SIZE;

	do {
		list_for_each_entry(vma, &mapping->i_mmap_nonlinear,
						shared.vm_set.list) {
			cursor = (unsigned long) vma->vm_private_data;
			while ( cursor < max_nl_cursor &&
				cursor < vma->vm_end - vma->vm_start) {
				if (try_to_unmap_cluster(cursor, &mapcount,
						vma, page) == SWAP_MLOCK)
					ret = SWAP_MLOCK;
				cursor += CLUSTER_SIZE;
				vma->vm_private_data = (void *) cursor;
				if ((int)mapcount <= 0)
					goto out;
			}
			vma->vm_private_data = (void *) max_nl_cursor;
		}
		cond_resched_lock(&mapping->i_mmap_lock);
		max_nl_cursor += CLUSTER_SIZE;
	} while (max_nl_cursor <= max_nl_size);

	/*
	 * Don't loop forever (perhaps all the remaining pages are
	 * in locked vmas).  Reset cursor on all unreserved nonlinear
	 * vmas, now forgetting on which ones it had fallen behind.
	 */
	list_for_each_entry(vma, &mapping->i_mmap_nonlinear, shared.vm_set.list)
		vma->vm_private_data = NULL;
out:
	spin_unlock(&mapping->i_mmap_lock);
	return ret;
}

/**
 * try_to_unmap - try to remove all page table mappings to a page
 * @page: the page to get unmapped
 * @flags: action and flags
 *
 * Tries to remove all the page table entries which are mapping this
 * page, used in the pageout path.  Caller must hold the page lock.
 * Return values are:
 *
 * SWAP_SUCCESS	- we succeeded in removing all mappings
 * SWAP_AGAIN	- we missed a mapping, try again later
 * SWAP_FAIL	- the page is unswappable
 * SWAP_MLOCK	- page is mlocked.
 */
int try_to_unmap(struct page *page, enum ttu_flags flags)
{
	int ret;

	BUG_ON(!PageLocked(page));

	if (unlikely(PageKsm(page)))
		ret = try_to_unmap_ksm(page, flags);
	else if (PageAnon(page))
		ret = try_to_unmap_anon(page, flags);
	else
		ret = try_to_unmap_file(page, flags);
	if (ret != SWAP_MLOCK && !page_mapped(page))
		ret = SWAP_SUCCESS;
	return ret;
}

/**
 * try_to_munlock - try to munlock a page
 * @page: the page to be munlocked
 *
 * Called from munlock code.  Checks all of the VMAs mapping the page
 * to make sure nobody else has this page mlocked. The page will be
 * returned with PG_mlocked cleared if no other vmas have it mlocked.
 *
 * Return values are:
 *
 * SWAP_AGAIN	- no vma is holding page mlocked, or,
 * SWAP_AGAIN	- page mapped in mlocked vma -- couldn't acquire mmap sem
 * SWAP_FAIL	- page cannot be located at present
 * SWAP_MLOCK	- page is now mlocked.
 */
int try_to_munlock(struct page *page)
{
	VM_BUG_ON(!PageLocked(page) || PageLRU(page));

	if (unlikely(PageKsm(page)))
		return try_to_unmap_ksm(page, TTU_MUNLOCK);
	else if (PageAnon(page))
		return try_to_unmap_anon(page, TTU_MUNLOCK);
	else
		return try_to_unmap_file(page, TTU_MUNLOCK);
}

#if defined(CONFIG_KSM) || defined(CONFIG_MIGRATION)
/*
 * Drop an anon_vma refcount, freeing the anon_vma and anon_vma->root
 * if necessary.  Be careful to do all the tests under the lock.  Once
 * we know we are the last user, nobody else can get a reference and we
 * can do the freeing without the lock.
 */
void drop_anon_vma(struct anon_vma *anon_vma)
{
	BUG_ON(atomic_read(&anon_vma->external_refcount) <= 0);
	if (atomic_dec_and_lock(&anon_vma->external_refcount, &anon_vma->root->lock)) {
		struct anon_vma *root = anon_vma->root;
		int empty = list_empty(&anon_vma->head);
		int last_root_user = 0;
		int root_empty = 0;

		/*
		 * The refcount on a non-root anon_vma got dropped.  Drop
		 * the refcount on the root and check if we need to free it.
		 */
		if (empty && anon_vma != root) {
			BUG_ON(atomic_read(&root->external_refcount) <= 0);
			last_root_user = atomic_dec_and_test(&root->external_refcount);
			root_empty = list_empty(&root->head);
		}
		anon_vma_unlock(anon_vma);

		if (empty) {
			anon_vma_free(anon_vma);
			if (root_empty && last_root_user)
				anon_vma_free(root);
		}
	}
}
#endif

#ifdef CONFIG_MIGRATION
/*
 * rmap_walk() and its helpers rmap_walk_anon() and rmap_walk_file():
 * Called by migrate.c to remove migration ptes, but might be used more later.
 */
static int rmap_walk_anon(struct page *page, int (*rmap_one)(struct page *,
		struct vm_area_struct *, unsigned long, void *), void *arg)
{
	struct anon_vma *anon_vma;
	struct anon_vma_chain *avc;
	int ret = SWAP_AGAIN;

	/*
	 * Note: remove_migration_ptes() cannot use page_lock_anon_vma()
	 * because that depends on page_mapped(); but not all its usages
	 * are holding mmap_sem. Users without mmap_sem are required to
	 * take a reference count to prevent the anon_vma disappearing
	 */
	anon_vma = page_anon_vma(page);
	if (!anon_vma)
		return ret;
	anon_vma_lock(anon_vma);
	list_for_each_entry(avc, &anon_vma->head, same_anon_vma) {
		struct vm_area_struct *vma = avc->vma;
		unsigned long address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		ret = rmap_one(page, vma, address, arg);
		if (ret != SWAP_AGAIN)
			break;
	}
	anon_vma_unlock(anon_vma);
	return ret;
}

static int rmap_walk_file(struct page *page, int (*rmap_one)(struct page *,
		struct vm_area_struct *, unsigned long, void *), void *arg)
{
	struct address_space *mapping = page->mapping;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	struct prio_tree_iter iter;
	int ret = SWAP_AGAIN;

	if (!mapping)
		return ret;
	spin_lock(&mapping->i_mmap_lock);
	vma_prio_tree_foreach(vma, &iter, &mapping->i_mmap, pgoff, pgoff) {
		unsigned long address = vma_address(page, vma);
		if (address == -EFAULT)
			continue;
		ret = rmap_one(page, vma, address, arg);
		if (ret != SWAP_AGAIN)
			break;
	}
	/*
	 * No nonlinear handling: being always shared, nonlinear vmas
	 * never contain migration ptes.  Decide what to do about this
	 * limitation to linear when we need rmap_walk() on nonlinear.
	 */
	spin_unlock(&mapping->i_mmap_lock);
	return ret;
}

int rmap_walk(struct page *page, int (*rmap_one)(struct page *,
		struct vm_area_struct *, unsigned long, void *), void *arg)
{
	VM_BUG_ON(!PageLocked(page));

	if (unlikely(PageKsm(page)))
		return rmap_walk_ksm(page, rmap_one, arg);
	else if (PageAnon(page))
		return rmap_walk_anon(page, rmap_one, arg);
	else
		return rmap_walk_file(page, rmap_one, arg);
}
#endif /* CONFIG_MIGRATION */

#ifdef CONFIG_HUGETLB_PAGE
/*
 * The following three functions are for anonymous (private mapped) hugepages.
 * Unlike common anonymous pages, anonymous hugepages have no accounting code
 * and no lru code, because we handle hugepages differently from common pages.
 */
static void __hugepage_set_anon_rmap(struct page *page,
	struct vm_area_struct *vma, unsigned long address, int exclusive)
{
	struct anon_vma *anon_vma = vma->anon_vma;

	BUG_ON(!anon_vma);

	if (PageAnon(page))
		return;
	if (!exclusive)
		anon_vma = anon_vma->root;

	anon_vma = (void *) anon_vma + PAGE_MAPPING_ANON;
	page->mapping = (struct address_space *) anon_vma;
	page->index = linear_page_index(vma, address);
}

void hugepage_add_anon_rmap(struct page *page,
			    struct vm_area_struct *vma, unsigned long address)
{
	struct anon_vma *anon_vma = vma->anon_vma;
	int first;

	BUG_ON(!PageLocked(page));
	BUG_ON(!anon_vma);
	BUG_ON(address < vma->vm_start || address >= vma->vm_end);
	first = atomic_inc_and_test(&page->_mapcount);
	if (first)
		__hugepage_set_anon_rmap(page, vma, address, 0);
}

void hugepage_add_new_anon_rmap(struct page *page,
			struct vm_area_struct *vma, unsigned long address)
{
	BUG_ON(address < vma->vm_start || address >= vma->vm_end);
	atomic_set(&page->_mapcount, 0);
	__hugepage_set_anon_rmap(page, vma, address, 1);
}
#endif /* CONFIG_HUGETLB_PAGE */
