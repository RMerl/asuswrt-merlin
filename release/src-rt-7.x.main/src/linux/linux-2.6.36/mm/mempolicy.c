

/* Notebook:
   fix mmap readahead to honour policy and enable policy for any page cache
   object
   statistics for bigpages
   global policy for page cache? currently it uses process policy. Requires
   first item above.
   handle mremap for shared memory (currently ignored for the policy)
   grows down?
   make bind policy root only? It can trigger oom much faster and the
   kernel is not always grateful with that.
*/

#include <linux/mempolicy.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/hugetlb.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/nodemask.h>
#include <linux/cpuset.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/nsproxy.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/compat.h>
#include <linux/swap.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/migrate.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/ctype.h>
#include <linux/mm_inline.h>

#include <asm/tlbflush.h>
#include <asm/uaccess.h>

#include "internal.h"

/* Internal flags */
#define MPOL_MF_DISCONTIG_OK (MPOL_MF_INTERNAL << 0)	/* Skip checks for continuous vmas */
#define MPOL_MF_INVERT (MPOL_MF_INTERNAL << 1)		/* Invert check for nodemask */
#define MPOL_MF_STATS (MPOL_MF_INTERNAL << 2)		/* Gather statistics */

static struct kmem_cache *policy_cache;
static struct kmem_cache *sn_cache;

/* Highest zone. An specific allocation for a zone below that is not
   policied. */
enum zone_type policy_zone = 0;

/*
 * run-time system-wide default policy => local allocation
 */
struct mempolicy default_policy = {
	.refcnt = ATOMIC_INIT(1), /* never free it */
	.mode = MPOL_PREFERRED,
	.flags = MPOL_F_LOCAL,
};

static const struct mempolicy_operations {
	int (*create)(struct mempolicy *pol, const nodemask_t *nodes);
	/*
	 * If read-side task has no lock to protect task->mempolicy, write-side
	 * task will rebind the task->mempolicy by two step. The first step is
	 * setting all the newly nodes, and the second step is cleaning all the
	 * disallowed nodes. In this way, we can avoid finding no node to alloc
	 * page.
	 * If we have a lock to protect task->mempolicy in read-side, we do
	 * rebind directly.
	 *
	 * step:
	 * 	MPOL_REBIND_ONCE - do rebind work at once
	 * 	MPOL_REBIND_STEP1 - set all the newly nodes
	 * 	MPOL_REBIND_STEP2 - clean all the disallowed nodes
	 */
	void (*rebind)(struct mempolicy *pol, const nodemask_t *nodes,
			enum mpol_rebind_step step);
} mpol_ops[MPOL_MAX];

/* Check that the nodemask contains at least one populated zone */
static int is_valid_nodemask(const nodemask_t *nodemask)
{
	int nd, k;

	for_each_node_mask(nd, *nodemask) {
		struct zone *z;

		for (k = 0; k <= policy_zone; k++) {
			z = &NODE_DATA(nd)->node_zones[k];
			if (z->present_pages > 0)
				return 1;
		}
	}

	return 0;
}

static inline int mpol_store_user_nodemask(const struct mempolicy *pol)
{
	return pol->flags & MPOL_MODE_FLAGS;
}

static void mpol_relative_nodemask(nodemask_t *ret, const nodemask_t *orig,
				   const nodemask_t *rel)
{
	nodemask_t tmp;
	nodes_fold(tmp, *orig, nodes_weight(*rel));
	nodes_onto(*ret, tmp, *rel);
}

static int mpol_new_interleave(struct mempolicy *pol, const nodemask_t *nodes)
{
	if (nodes_empty(*nodes))
		return -EINVAL;
	pol->v.nodes = *nodes;
	return 0;
}

static int mpol_new_preferred(struct mempolicy *pol, const nodemask_t *nodes)
{
	if (!nodes)
		pol->flags |= MPOL_F_LOCAL;	/* local allocation */
	else if (nodes_empty(*nodes))
		return -EINVAL;			/*  no allowed nodes */
	else
		pol->v.preferred_node = first_node(*nodes);
	return 0;
}

static int mpol_new_bind(struct mempolicy *pol, const nodemask_t *nodes)
{
	if (!is_valid_nodemask(nodes))
		return -EINVAL;
	pol->v.nodes = *nodes;
	return 0;
}

/*
 * mpol_set_nodemask is called after mpol_new() to set up the nodemask, if
 * any, for the new policy.  mpol_new() has already validated the nodes
 * parameter with respect to the policy mode and flags.  But, we need to
 * handle an empty nodemask with MPOL_PREFERRED here.
 *
 * Must be called holding task's alloc_lock to protect task's mems_allowed
 * and mempolicy.  May also be called holding the mmap_semaphore for write.
 */
static int mpol_set_nodemask(struct mempolicy *pol,
		     const nodemask_t *nodes, struct nodemask_scratch *nsc)
{
	int ret;

	/* if mode is MPOL_DEFAULT, pol is NULL. This is right. */
	if (pol == NULL)
		return 0;
	/* Check N_HIGH_MEMORY */
	nodes_and(nsc->mask1,
		  cpuset_current_mems_allowed, node_states[N_HIGH_MEMORY]);

	VM_BUG_ON(!nodes);
	if (pol->mode == MPOL_PREFERRED && nodes_empty(*nodes))
		nodes = NULL;	/* explicit local allocation */
	else {
		if (pol->flags & MPOL_F_RELATIVE_NODES)
			mpol_relative_nodemask(&nsc->mask2, nodes,&nsc->mask1);
		else
			nodes_and(nsc->mask2, *nodes, nsc->mask1);

		if (mpol_store_user_nodemask(pol))
			pol->w.user_nodemask = *nodes;
		else
			pol->w.cpuset_mems_allowed =
						cpuset_current_mems_allowed;
	}

	if (nodes)
		ret = mpol_ops[pol->mode].create(pol, &nsc->mask2);
	else
		ret = mpol_ops[pol->mode].create(pol, NULL);
	return ret;
}

/*
 * This function just creates a new policy, does some check and simple
 * initialization. You must invoke mpol_set_nodemask() to set nodes.
 */
static struct mempolicy *mpol_new(unsigned short mode, unsigned short flags,
				  nodemask_t *nodes)
{
	struct mempolicy *policy;

	pr_debug("setting mode %d flags %d nodes[0] %lx\n",
		 mode, flags, nodes ? nodes_addr(*nodes)[0] : -1);

	if (mode == MPOL_DEFAULT) {
		if (nodes && !nodes_empty(*nodes))
			return ERR_PTR(-EINVAL);
		return NULL;	/* simply delete any existing policy */
	}
	VM_BUG_ON(!nodes);

	/*
	 * MPOL_PREFERRED cannot be used with MPOL_F_STATIC_NODES or
	 * MPOL_F_RELATIVE_NODES if the nodemask is empty (local allocation).
	 * All other modes require a valid pointer to a non-empty nodemask.
	 */
	if (mode == MPOL_PREFERRED) {
		if (nodes_empty(*nodes)) {
			if (((flags & MPOL_F_STATIC_NODES) ||
			     (flags & MPOL_F_RELATIVE_NODES)))
				return ERR_PTR(-EINVAL);
		}
	} else if (nodes_empty(*nodes))
		return ERR_PTR(-EINVAL);
	policy = kmem_cache_alloc(policy_cache, GFP_KERNEL);
	if (!policy)
		return ERR_PTR(-ENOMEM);
	atomic_set(&policy->refcnt, 1);
	policy->mode = mode;
	policy->flags = flags;

	return policy;
}

/* Slow path of a mpol destructor. */
void __mpol_put(struct mempolicy *p)
{
	if (!atomic_dec_and_test(&p->refcnt))
		return;
	kmem_cache_free(policy_cache, p);
}

static void mpol_rebind_default(struct mempolicy *pol, const nodemask_t *nodes,
				enum mpol_rebind_step step)
{
}

/*
 * step:
 * 	MPOL_REBIND_ONCE  - do rebind work at once
 * 	MPOL_REBIND_STEP1 - set all the newly nodes
 * 	MPOL_REBIND_STEP2 - clean all the disallowed nodes
 */
static void mpol_rebind_nodemask(struct mempolicy *pol, const nodemask_t *nodes,
				 enum mpol_rebind_step step)
{
	nodemask_t tmp;

	if (pol->flags & MPOL_F_STATIC_NODES)
		nodes_and(tmp, pol->w.user_nodemask, *nodes);
	else if (pol->flags & MPOL_F_RELATIVE_NODES)
		mpol_relative_nodemask(&tmp, &pol->w.user_nodemask, nodes);
	else {
		/*
		 * if step == 1, we use ->w.cpuset_mems_allowed to cache the
		 * result
		 */
		if (step == MPOL_REBIND_ONCE || step == MPOL_REBIND_STEP1) {
			nodes_remap(tmp, pol->v.nodes,
					pol->w.cpuset_mems_allowed, *nodes);
			pol->w.cpuset_mems_allowed = step ? tmp : *nodes;
		} else if (step == MPOL_REBIND_STEP2) {
			tmp = pol->w.cpuset_mems_allowed;
			pol->w.cpuset_mems_allowed = *nodes;
		} else
			BUG();
	}

	if (nodes_empty(tmp))
		tmp = *nodes;

	if (step == MPOL_REBIND_STEP1)
		nodes_or(pol->v.nodes, pol->v.nodes, tmp);
	else if (step == MPOL_REBIND_ONCE || step == MPOL_REBIND_STEP2)
		pol->v.nodes = tmp;
	else
		BUG();

	if (!node_isset(current->il_next, tmp)) {
		current->il_next = next_node(current->il_next, tmp);
		if (current->il_next >= MAX_NUMNODES)
			current->il_next = first_node(tmp);
		if (current->il_next >= MAX_NUMNODES)
			current->il_next = numa_node_id();
	}
}

static void mpol_rebind_preferred(struct mempolicy *pol,
				  const nodemask_t *nodes,
				  enum mpol_rebind_step step)
{
	nodemask_t tmp;

	if (pol->flags & MPOL_F_STATIC_NODES) {
		int node = first_node(pol->w.user_nodemask);

		if (node_isset(node, *nodes)) {
			pol->v.preferred_node = node;
			pol->flags &= ~MPOL_F_LOCAL;
		} else
			pol->flags |= MPOL_F_LOCAL;
	} else if (pol->flags & MPOL_F_RELATIVE_NODES) {
		mpol_relative_nodemask(&tmp, &pol->w.user_nodemask, nodes);
		pol->v.preferred_node = first_node(tmp);
	} else if (!(pol->flags & MPOL_F_LOCAL)) {
		pol->v.preferred_node = node_remap(pol->v.preferred_node,
						   pol->w.cpuset_mems_allowed,
						   *nodes);
		pol->w.cpuset_mems_allowed = *nodes;
	}
}

/*
 * mpol_rebind_policy - Migrate a policy to a different set of nodes
 *
 * If read-side task has no lock to protect task->mempolicy, write-side
 * task will rebind the task->mempolicy by two step. The first step is
 * setting all the newly nodes, and the second step is cleaning all the
 * disallowed nodes. In this way, we can avoid finding no node to alloc
 * page.
 * If we have a lock to protect task->mempolicy in read-side, we do
 * rebind directly.
 *
 * step:
 * 	MPOL_REBIND_ONCE  - do rebind work at once
 * 	MPOL_REBIND_STEP1 - set all the newly nodes
 * 	MPOL_REBIND_STEP2 - clean all the disallowed nodes
 */
static void mpol_rebind_policy(struct mempolicy *pol, const nodemask_t *newmask,
				enum mpol_rebind_step step)
{
	if (!pol)
		return;
	if (!mpol_store_user_nodemask(pol) && step == 0 &&
	    nodes_equal(pol->w.cpuset_mems_allowed, *newmask))
		return;

	if (step == MPOL_REBIND_STEP1 && (pol->flags & MPOL_F_REBINDING))
		return;

	if (step == MPOL_REBIND_STEP2 && !(pol->flags & MPOL_F_REBINDING))
		BUG();

	if (step == MPOL_REBIND_STEP1)
		pol->flags |= MPOL_F_REBINDING;
	else if (step == MPOL_REBIND_STEP2)
		pol->flags &= ~MPOL_F_REBINDING;
	else if (step >= MPOL_REBIND_NSTEP)
		BUG();

	mpol_ops[pol->mode].rebind(pol, newmask, step);
}

/*
 * Wrapper for mpol_rebind_policy() that just requires task
 * pointer, and updates task mempolicy.
 *
 * Called with task's alloc_lock held.
 */

void mpol_rebind_task(struct task_struct *tsk, const nodemask_t *new,
			enum mpol_rebind_step step)
{
	mpol_rebind_policy(tsk->mempolicy, new, step);
}

/*
 * Rebind each vma in mm to new nodemask.
 *
 * Call holding a reference to mm.  Takes mm->mmap_sem during call.
 */

void mpol_rebind_mm(struct mm_struct *mm, nodemask_t *new)
{
	struct vm_area_struct *vma;

	down_write(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next)
		mpol_rebind_policy(vma->vm_policy, new, MPOL_REBIND_ONCE);
	up_write(&mm->mmap_sem);
}

static const struct mempolicy_operations mpol_ops[MPOL_MAX] = {
	[MPOL_DEFAULT] = {
		.rebind = mpol_rebind_default,
	},
	[MPOL_INTERLEAVE] = {
		.create = mpol_new_interleave,
		.rebind = mpol_rebind_nodemask,
	},
	[MPOL_PREFERRED] = {
		.create = mpol_new_preferred,
		.rebind = mpol_rebind_preferred,
	},
	[MPOL_BIND] = {
		.create = mpol_new_bind,
		.rebind = mpol_rebind_nodemask,
	},
};

static void gather_stats(struct page *, void *, int pte_dirty);
static void migrate_page_add(struct page *page, struct list_head *pagelist,
				unsigned long flags);

/* Scan through pages checking if pages follow certain conditions. */
static int check_pte_range(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long addr, unsigned long end,
		const nodemask_t *nodes, unsigned long flags,
		void *private)
{
	pte_t *orig_pte;
	pte_t *pte;
	spinlock_t *ptl;

	orig_pte = pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	do {
		struct page *page;
		int nid;

		if (!pte_present(*pte))
			continue;
		page = vm_normal_page(vma, addr, *pte);
		if (!page)
			continue;
		/*
		 * vm_normal_page() filters out zero pages, but there might
		 * still be PageReserved pages to skip, perhaps in a VDSO.
		 * And we cannot move PageKsm pages sensibly or safely yet.
		 */
		if (PageReserved(page) || PageKsm(page))
			continue;
		nid = page_to_nid(page);
		if (node_isset(nid, *nodes) == !!(flags & MPOL_MF_INVERT))
			continue;

		if (flags & MPOL_MF_STATS)
			gather_stats(page, private, pte_dirty(*pte));
		else if (flags & (MPOL_MF_MOVE | MPOL_MF_MOVE_ALL))
			migrate_page_add(page, private, flags);
		else
			break;
	} while (pte++, addr += PAGE_SIZE, addr != end);
	pte_unmap_unlock(orig_pte, ptl);
	return addr != end;
}

static inline int check_pmd_range(struct vm_area_struct *vma, pud_t *pud,
		unsigned long addr, unsigned long end,
		const nodemask_t *nodes, unsigned long flags,
		void *private)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none_or_clear_bad(pmd))
			continue;
		if (check_pte_range(vma, pmd, addr, next, nodes,
				    flags, private))
			return -EIO;
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static inline int check_pud_range(struct vm_area_struct *vma, pgd_t *pgd,
		unsigned long addr, unsigned long end,
		const nodemask_t *nodes, unsigned long flags,
		void *private)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		if (check_pmd_range(vma, pud, addr, next, nodes,
				    flags, private))
			return -EIO;
	} while (pud++, addr = next, addr != end);
	return 0;
}

static inline int check_pgd_range(struct vm_area_struct *vma,
		unsigned long addr, unsigned long end,
		const nodemask_t *nodes, unsigned long flags,
		void *private)
{
	pgd_t *pgd;
	unsigned long next;

	pgd = pgd_offset(vma->vm_mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		if (check_pud_range(vma, pgd, addr, next, nodes,
				    flags, private))
			return -EIO;
	} while (pgd++, addr = next, addr != end);
	return 0;
}

/*
 * Check if all pages in a range are on a set of nodes.
 * If pagelist != NULL then isolate pages from the LRU and
 * put them on the pagelist.
 */
static struct vm_area_struct *
check_range(struct mm_struct *mm, unsigned long start, unsigned long end,
		const nodemask_t *nodes, unsigned long flags, void *private)
{
	int err;
	struct vm_area_struct *first, *vma, *prev;


	first = find_vma(mm, start);
	if (!first)
		return ERR_PTR(-EFAULT);
	prev = NULL;
	for (vma = first; vma && vma->vm_start < end; vma = vma->vm_next) {
		if (!(flags & MPOL_MF_DISCONTIG_OK)) {
			if (!vma->vm_next && vma->vm_end < end)
				return ERR_PTR(-EFAULT);
			if (prev && prev->vm_end < vma->vm_start)
				return ERR_PTR(-EFAULT);
		}
		if (!is_vm_hugetlb_page(vma) &&
		    ((flags & MPOL_MF_STRICT) ||
		     ((flags & (MPOL_MF_MOVE | MPOL_MF_MOVE_ALL)) &&
				vma_migratable(vma)))) {
			unsigned long endvma = vma->vm_end;

			if (endvma > end)
				endvma = end;
			if (vma->vm_start > start)
				start = vma->vm_start;
			err = check_pgd_range(vma, start, endvma, nodes,
						flags, private);
			if (err) {
				first = ERR_PTR(err);
				break;
			}
		}
		prev = vma;
	}
	return first;
}

/* Apply policy to a single VMA */
static int policy_vma(struct vm_area_struct *vma, struct mempolicy *new)
{
	int err = 0;
	struct mempolicy *old = vma->vm_policy;

	pr_debug("vma %lx-%lx/%lx vm_ops %p vm_file %p set_policy %p\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff,
		 vma->vm_ops, vma->vm_file,
		 vma->vm_ops ? vma->vm_ops->set_policy : NULL);

	if (vma->vm_ops && vma->vm_ops->set_policy)
		err = vma->vm_ops->set_policy(vma, new);
	if (!err) {
		mpol_get(new);
		vma->vm_policy = new;
		mpol_put(old);
	}
	return err;
}

/* Step 2: apply policy to a range and do splits. */
static int mbind_range(struct mm_struct *mm, unsigned long start,
		       unsigned long end, struct mempolicy *new_pol)
{
	struct vm_area_struct *next;
	struct vm_area_struct *prev;
	struct vm_area_struct *vma;
	int err = 0;
	pgoff_t pgoff;
	unsigned long vmstart;
	unsigned long vmend;

	vma = find_vma_prev(mm, start, &prev);
	if (!vma || vma->vm_start > start)
		return -EFAULT;

	for (; vma && vma->vm_start < end; prev = vma, vma = next) {
		next = vma->vm_next;
		vmstart = max(start, vma->vm_start);
		vmend   = min(end, vma->vm_end);

		pgoff = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
		prev = vma_merge(mm, prev, vmstart, vmend, vma->vm_flags,
				  vma->anon_vma, vma->vm_file, pgoff, new_pol);
		if (prev) {
			vma = prev;
			next = vma->vm_next;
			continue;
		}
		if (vma->vm_start != vmstart) {
			err = split_vma(vma->vm_mm, vma, vmstart, 1);
			if (err)
				goto out;
		}
		if (vma->vm_end != vmend) {
			err = split_vma(vma->vm_mm, vma, vmend, 0);
			if (err)
				goto out;
		}
		err = policy_vma(vma, new_pol);
		if (err)
			goto out;
	}

 out:
	return err;
}

/*
 * Update task->flags PF_MEMPOLICY bit: set iff non-default
 * mempolicy.  Allows more rapid checking of this (combined perhaps
 * with other PF_* flag bits) on memory allocation hot code paths.
 *
 * If called from outside this file, the task 'p' should -only- be
 * a newly forked child not yet visible on the task list, because
 * manipulating the task flags of a visible task is not safe.
 *
 * The above limitation is why this routine has the funny name
 * mpol_fix_fork_child_flag().
 *
 * It is also safe to call this with a task pointer of current,
 * which the static wrapper mpol_set_task_struct_flag() does,
 * for use within this file.
 */

void mpol_fix_fork_child_flag(struct task_struct *p)
{
	if (p->mempolicy)
		p->flags |= PF_MEMPOLICY;
	else
		p->flags &= ~PF_MEMPOLICY;
}

static void mpol_set_task_struct_flag(void)
{
	mpol_fix_fork_child_flag(current);
}

/* Set the process memory policy */
static long do_set_mempolicy(unsigned short mode, unsigned short flags,
			     nodemask_t *nodes)
{
	struct mempolicy *new, *old;
	struct mm_struct *mm = current->mm;
	NODEMASK_SCRATCH(scratch);
	int ret;

	if (!scratch)
		return -ENOMEM;

	new = mpol_new(mode, flags, nodes);
	if (IS_ERR(new)) {
		ret = PTR_ERR(new);
		goto out;
	}
	/*
	 * prevent changing our mempolicy while show_numa_maps()
	 * is using it.
	 * Note:  do_set_mempolicy() can be called at init time
	 * with no 'mm'.
	 */
	if (mm)
		down_write(&mm->mmap_sem);
	task_lock(current);
	ret = mpol_set_nodemask(new, nodes, scratch);
	if (ret) {
		task_unlock(current);
		if (mm)
			up_write(&mm->mmap_sem);
		mpol_put(new);
		goto out;
	}
	old = current->mempolicy;
	current->mempolicy = new;
	mpol_set_task_struct_flag();
	if (new && new->mode == MPOL_INTERLEAVE &&
	    nodes_weight(new->v.nodes))
		current->il_next = first_node(new->v.nodes);
	task_unlock(current);
	if (mm)
		up_write(&mm->mmap_sem);

	mpol_put(old);
	ret = 0;
out:
	NODEMASK_SCRATCH_FREE(scratch);
	return ret;
}

/*
 * Return nodemask for policy for get_mempolicy() query
 *
 * Called with task's alloc_lock held
 */
static void get_policy_nodemask(struct mempolicy *p, nodemask_t *nodes)
{
	nodes_clear(*nodes);
	if (p == &default_policy)
		return;

	switch (p->mode) {
	case MPOL_BIND:
		/* Fall through */
	case MPOL_INTERLEAVE:
		*nodes = p->v.nodes;
		break;
	case MPOL_PREFERRED:
		if (!(p->flags & MPOL_F_LOCAL))
			node_set(p->v.preferred_node, *nodes);
		/* else return empty node mask for local allocation */
		break;
	default:
		BUG();
	}
}

static int lookup_node(struct mm_struct *mm, unsigned long addr)
{
	struct page *p;
	int err;

	err = get_user_pages(current, mm, addr & PAGE_MASK, 1, 0, 0, &p, NULL);
	if (err >= 0) {
		err = page_to_nid(p);
		put_page(p);
	}
	return err;
}

/* Retrieve NUMA policy */
static long do_get_mempolicy(int *policy, nodemask_t *nmask,
			     unsigned long addr, unsigned long flags)
{
	int err;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	struct mempolicy *pol = current->mempolicy;

	if (flags &
		~(unsigned long)(MPOL_F_NODE|MPOL_F_ADDR|MPOL_F_MEMS_ALLOWED))
		return -EINVAL;

	if (flags & MPOL_F_MEMS_ALLOWED) {
		if (flags & (MPOL_F_NODE|MPOL_F_ADDR))
			return -EINVAL;
		*policy = 0;	/* just so it's initialized */
		task_lock(current);
		*nmask  = cpuset_current_mems_allowed;
		task_unlock(current);
		return 0;
	}

	if (flags & MPOL_F_ADDR) {
		/*
		 * Do NOT fall back to task policy if the
		 * vma/shared policy at addr is NULL.  We
		 * want to return MPOL_DEFAULT in this case.
		 */
		down_read(&mm->mmap_sem);
		vma = find_vma_intersection(mm, addr, addr+1);
		if (!vma) {
			up_read(&mm->mmap_sem);
			return -EFAULT;
		}
		if (vma->vm_ops && vma->vm_ops->get_policy)
			pol = vma->vm_ops->get_policy(vma, addr);
		else
			pol = vma->vm_policy;
	} else if (addr)
		return -EINVAL;

	if (!pol)
		pol = &default_policy;	/* indicates default behavior */

	if (flags & MPOL_F_NODE) {
		if (flags & MPOL_F_ADDR) {
			err = lookup_node(mm, addr);
			if (err < 0)
				goto out;
			*policy = err;
		} else if (pol == current->mempolicy &&
				pol->mode == MPOL_INTERLEAVE) {
			*policy = current->il_next;
		} else {
			err = -EINVAL;
			goto out;
		}
	} else {
		*policy = pol == &default_policy ? MPOL_DEFAULT :
						pol->mode;
		/*
		 * Internal mempolicy flags must be masked off before exposing
		 * the policy to userspace.
		 */
		*policy |= (pol->flags & MPOL_MODE_FLAGS);
	}

	if (vma) {
		up_read(&current->mm->mmap_sem);
		vma = NULL;
	}

	err = 0;
	if (nmask) {
		if (mpol_store_user_nodemask(pol)) {
			*nmask = pol->w.user_nodemask;
		} else {
			task_lock(current);
			get_policy_nodemask(pol, nmask);
			task_unlock(current);
		}
	}

 out:
	mpol_cond_put(pol);
	if (vma)
		up_read(&current->mm->mmap_sem);
	return err;
}

#ifdef CONFIG_MIGRATION
/*
 * page migration
 */
static void migrate_page_add(struct page *page, struct list_head *pagelist,
				unsigned long flags)
{
	/*
	 * Avoid migrating a page that is shared with others.
	 */
	if ((flags & MPOL_MF_MOVE_ALL) || page_mapcount(page) == 1) {
		if (!isolate_lru_page(page)) {
			list_add_tail(&page->lru, pagelist);
			inc_zone_page_state(page, NR_ISOLATED_ANON +
					    page_is_file_cache(page));
		}
	}
}

static struct page *new_node_page(struct page *page, unsigned long node, int **x)
{
	return alloc_pages_exact_node(node, GFP_HIGHUSER_MOVABLE, 0);
}

/*
 * Migrate pages from one node to a target node.
 * Returns error or the number of pages not migrated.
 */
static int migrate_to_node(struct mm_struct *mm, int source, int dest,
			   int flags)
{
	nodemask_t nmask;
	LIST_HEAD(pagelist);
	int err = 0;

	nodes_clear(nmask);
	node_set(source, nmask);

	check_range(mm, mm->mmap->vm_start, mm->task_size, &nmask,
			flags | MPOL_MF_DISCONTIG_OK, &pagelist);

	if (!list_empty(&pagelist))
		err = migrate_pages(&pagelist, new_node_page, dest, 0);

	return err;
}

/*
 * Move pages between the two nodesets so as to preserve the physical
 * layout as much as possible.
 *
 * Returns the number of page that could not be moved.
 */
int do_migrate_pages(struct mm_struct *mm,
	const nodemask_t *from_nodes, const nodemask_t *to_nodes, int flags)
{
	int busy = 0;
	int err;
	nodemask_t tmp;

	err = migrate_prep();
	if (err)
		return err;

	down_read(&mm->mmap_sem);

	err = migrate_vmas(mm, from_nodes, to_nodes, flags);
	if (err)
		goto out;

	/*
	 * Find a 'source' bit set in 'tmp' whose corresponding 'dest'
	 * bit in 'to' is not also set in 'tmp'.  Clear the found 'source'
	 * bit in 'tmp', and return that <source, dest> pair for migration.
	 * The pair of nodemasks 'to' and 'from' define the map.
	 *
	 * If no pair of bits is found that way, fallback to picking some
	 * pair of 'source' and 'dest' bits that are not the same.  If the
	 * 'source' and 'dest' bits are the same, this represents a node
	 * that will be migrating to itself, so no pages need move.
	 *
	 * If no bits are left in 'tmp', or if all remaining bits left
	 * in 'tmp' correspond to the same bit in 'to', return false
	 * (nothing left to migrate).
	 *
	 * This lets us pick a pair of nodes to migrate between, such that
	 * if possible the dest node is not already occupied by some other
	 * source node, minimizing the risk of overloading the memory on a
	 * node that would happen if we migrated incoming memory to a node
	 * before migrating outgoing memory source that same node.
	 *
	 * A single scan of tmp is sufficient.  As we go, we remember the
	 * most recent <s, d> pair that moved (s != d).  If we find a pair
	 * that not only moved, but what's better, moved to an empty slot
	 * (d is not set in tmp), then we break out then, with that pair.
	 * Otherwise when we finish scannng from_tmp, we at least have the
	 * most recent <s, d> pair that moved.  If we get all the way through
	 * the scan of tmp without finding any node that moved, much less
	 * moved to an empty node, then there is nothing left worth migrating.
	 */

	tmp = *from_nodes;
	while (!nodes_empty(tmp)) {
		int s,d;
		int source = -1;
		int dest = 0;

		for_each_node_mask(s, tmp) {
			d = node_remap(s, *from_nodes, *to_nodes);
			if (s == d)
				continue;

			source = s;	/* Node moved. Memorize */
			dest = d;

			/* dest not in remaining from nodes? */
			if (!node_isset(dest, tmp))
				break;
		}
		if (source == -1)
			break;

		node_clear(source, tmp);
		err = migrate_to_node(mm, source, dest, flags);
		if (err > 0)
			busy += err;
		if (err < 0)
			break;
	}
out:
	up_read(&mm->mmap_sem);
	if (err < 0)
		return err;
	return busy;

}

/*
 * Allocate a new page for page migration based on vma policy.
 * Start assuming that page is mapped by vma pointed to by @private.
 * Search forward from there, if not.  N.B., this assumes that the
 * list of pages handed to migrate_pages()--which is how we get here--
 * is in virtual address order.
 */
static struct page *new_vma_page(struct page *page, unsigned long private, int **x)
{
	struct vm_area_struct *vma = (struct vm_area_struct *)private;
	unsigned long uninitialized_var(address);

	while (vma) {
		address = page_address_in_vma(page, vma);
		if (address != -EFAULT)
			break;
		vma = vma->vm_next;
	}

	/*
	 * if !vma, alloc_page_vma() will use task or system default policy
	 */
	return alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, address);
}
#else

static void migrate_page_add(struct page *page, struct list_head *pagelist,
				unsigned long flags)
{
}

int do_migrate_pages(struct mm_struct *mm,
	const nodemask_t *from_nodes, const nodemask_t *to_nodes, int flags)
{
	return -ENOSYS;
}

static struct page *new_vma_page(struct page *page, unsigned long private, int **x)
{
	return NULL;
}
#endif

static long do_mbind(unsigned long start, unsigned long len,
		     unsigned short mode, unsigned short mode_flags,
		     nodemask_t *nmask, unsigned long flags)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	struct mempolicy *new;
	unsigned long end;
	int err;
	LIST_HEAD(pagelist);

	if (flags & ~(unsigned long)(MPOL_MF_STRICT |
				     MPOL_MF_MOVE | MPOL_MF_MOVE_ALL))
		return -EINVAL;
	if ((flags & MPOL_MF_MOVE_ALL) && !capable(CAP_SYS_NICE))
		return -EPERM;

	if (start & ~PAGE_MASK)
		return -EINVAL;

	if (mode == MPOL_DEFAULT)
		flags &= ~MPOL_MF_STRICT;

	len = (len + PAGE_SIZE - 1) & PAGE_MASK;
	end = start + len;

	if (end < start)
		return -EINVAL;
	if (end == start)
		return 0;

	new = mpol_new(mode, mode_flags, nmask);
	if (IS_ERR(new))
		return PTR_ERR(new);

	/*
	 * If we are using the default policy then operation
	 * on discontinuous address spaces is okay after all
	 */
	if (!new)
		flags |= MPOL_MF_DISCONTIG_OK;

	pr_debug("mbind %lx-%lx mode:%d flags:%d nodes:%lx\n",
		 start, start + len, mode, mode_flags,
		 nmask ? nodes_addr(*nmask)[0] : -1);

	if (flags & (MPOL_MF_MOVE | MPOL_MF_MOVE_ALL)) {

		err = migrate_prep();
		if (err)
			goto mpol_out;
	}
	{
		NODEMASK_SCRATCH(scratch);
		if (scratch) {
			down_write(&mm->mmap_sem);
			task_lock(current);
			err = mpol_set_nodemask(new, nmask, scratch);
			task_unlock(current);
			if (err)
				up_write(&mm->mmap_sem);
		} else
			err = -ENOMEM;
		NODEMASK_SCRATCH_FREE(scratch);
	}
	if (err)
		goto mpol_out;

	vma = check_range(mm, start, end, nmask,
			  flags | MPOL_MF_INVERT, &pagelist);

	err = PTR_ERR(vma);
	if (!IS_ERR(vma)) {
		int nr_failed = 0;

		err = mbind_range(mm, start, end, new);

		if (!list_empty(&pagelist))
			nr_failed = migrate_pages(&pagelist, new_vma_page,
						(unsigned long)vma, 0);

		if (!err && nr_failed && (flags & MPOL_MF_STRICT))
			err = -EIO;
	} else
		putback_lru_pages(&pagelist);

	up_write(&mm->mmap_sem);
 mpol_out:
	mpol_put(new);
	return err;
}

/*
 * User space interface with variable sized bitmaps for nodelists.
 */

/* Copy a node mask from user space. */
static int get_nodes(nodemask_t *nodes, const unsigned long __user *nmask,
		     unsigned long maxnode)
{
	unsigned long k;
	unsigned long nlongs;
	unsigned long endmask;

	--maxnode;
	nodes_clear(*nodes);
	if (maxnode == 0 || !nmask)
		return 0;
	if (maxnode > PAGE_SIZE*BITS_PER_BYTE)
		return -EINVAL;

	nlongs = BITS_TO_LONGS(maxnode);
	if ((maxnode % BITS_PER_LONG) == 0)
		endmask = ~0UL;
	else
		endmask = (1UL << (maxnode % BITS_PER_LONG)) - 1;

	/* When the user specified more nodes than supported just check
	   if the non supported part is all zero. */
	if (nlongs > BITS_TO_LONGS(MAX_NUMNODES)) {
		if (nlongs > PAGE_SIZE/sizeof(long))
			return -EINVAL;
		for (k = BITS_TO_LONGS(MAX_NUMNODES); k < nlongs; k++) {
			unsigned long t;
			if (get_user(t, nmask + k))
				return -EFAULT;
			if (k == nlongs - 1) {
				if (t & endmask)
					return -EINVAL;
			} else if (t)
				return -EINVAL;
		}
		nlongs = BITS_TO_LONGS(MAX_NUMNODES);
		endmask = ~0UL;
	}

	if (copy_from_user(nodes_addr(*nodes), nmask, nlongs*sizeof(unsigned long)))
		return -EFAULT;
	nodes_addr(*nodes)[nlongs-1] &= endmask;
	return 0;
}

/* Copy a kernel node mask to user space */
static int copy_nodes_to_user(unsigned long __user *mask, unsigned long maxnode,
			      nodemask_t *nodes)
{
	unsigned long copy = ALIGN(maxnode-1, 64) / 8;
	const int nbytes = BITS_TO_LONGS(MAX_NUMNODES) * sizeof(long);

	if (copy > nbytes) {
		if (copy > PAGE_SIZE)
			return -EINVAL;
		if (clear_user((char __user *)mask + nbytes, copy - nbytes))
			return -EFAULT;
		copy = nbytes;
	}
	return copy_to_user(mask, nodes_addr(*nodes), copy) ? -EFAULT : 0;
}

SYSCALL_DEFINE6(mbind, unsigned long, start, unsigned long, len,
		unsigned long, mode, unsigned long __user *, nmask,
		unsigned long, maxnode, unsigned, flags)
{
	nodemask_t nodes;
	int err;
	unsigned short mode_flags;

	mode_flags = mode & MPOL_MODE_FLAGS;
	mode &= ~MPOL_MODE_FLAGS;
	if (mode >= MPOL_MAX)
		return -EINVAL;
	if ((mode_flags & MPOL_F_STATIC_NODES) &&
	    (mode_flags & MPOL_F_RELATIVE_NODES))
		return -EINVAL;
	err = get_nodes(&nodes, nmask, maxnode);
	if (err)
		return err;
	return do_mbind(start, len, mode, mode_flags, &nodes, flags);
}

/* Set the process memory policy */
SYSCALL_DEFINE3(set_mempolicy, int, mode, unsigned long __user *, nmask,
		unsigned long, maxnode)
{
	int err;
	nodemask_t nodes;
	unsigned short flags;

	flags = mode & MPOL_MODE_FLAGS;
	mode &= ~MPOL_MODE_FLAGS;
	if ((unsigned int)mode >= MPOL_MAX)
		return -EINVAL;
	if ((flags & MPOL_F_STATIC_NODES) && (flags & MPOL_F_RELATIVE_NODES))
		return -EINVAL;
	err = get_nodes(&nodes, nmask, maxnode);
	if (err)
		return err;
	return do_set_mempolicy(mode, flags, &nodes);
}

SYSCALL_DEFINE4(migrate_pages, pid_t, pid, unsigned long, maxnode,
		const unsigned long __user *, old_nodes,
		const unsigned long __user *, new_nodes)
{
	const struct cred *cred = current_cred(), *tcred;
	struct mm_struct *mm = NULL;
	struct task_struct *task;
	nodemask_t task_nodes;
	int err;
	nodemask_t *old;
	nodemask_t *new;
	NODEMASK_SCRATCH(scratch);

	if (!scratch)
		return -ENOMEM;

	old = &scratch->mask1;
	new = &scratch->mask2;

	err = get_nodes(old, old_nodes, maxnode);
	if (err)
		goto out;

	err = get_nodes(new, new_nodes, maxnode);
	if (err)
		goto out;

	/* Find the mm_struct */
	read_lock(&tasklist_lock);
	task = pid ? find_task_by_vpid(pid) : current;
	if (!task) {
		read_unlock(&tasklist_lock);
		err = -ESRCH;
		goto out;
	}
	mm = get_task_mm(task);
	read_unlock(&tasklist_lock);

	err = -EINVAL;
	if (!mm)
		goto out;

	/*
	 * Check if this process has the right to modify the specified
	 * process. The right exists if the process has administrative
	 * capabilities, superuser privileges or the same
	 * userid as the target process.
	 */
	rcu_read_lock();
	tcred = __task_cred(task);
	if (cred->euid != tcred->suid && cred->euid != tcred->uid &&
	    cred->uid  != tcred->suid && cred->uid  != tcred->uid &&
	    !capable(CAP_SYS_NICE)) {
		rcu_read_unlock();
		err = -EPERM;
		goto out;
	}
	rcu_read_unlock();

	task_nodes = cpuset_mems_allowed(task);
	/* Is the user allowed to access the target nodes? */
	if (!nodes_subset(*new, task_nodes) && !capable(CAP_SYS_NICE)) {
		err = -EPERM;
		goto out;
	}

	if (!nodes_subset(*new, node_states[N_HIGH_MEMORY])) {
		err = -EINVAL;
		goto out;
	}

	err = security_task_movememory(task);
	if (err)
		goto out;

	err = do_migrate_pages(mm, old, new,
		capable(CAP_SYS_NICE) ? MPOL_MF_MOVE_ALL : MPOL_MF_MOVE);
out:
	if (mm)
		mmput(mm);
	NODEMASK_SCRATCH_FREE(scratch);

	return err;
}


/* Retrieve NUMA policy */
SYSCALL_DEFINE5(get_mempolicy, int __user *, policy,
		unsigned long __user *, nmask, unsigned long, maxnode,
		unsigned long, addr, unsigned long, flags)
{
	int err;
	int uninitialized_var(pval);
	nodemask_t nodes;

	if (nmask != NULL && maxnode < MAX_NUMNODES)
		return -EINVAL;

	err = do_get_mempolicy(&pval, &nodes, addr, flags);

	if (err)
		return err;

	if (policy && put_user(pval, policy))
		return -EFAULT;

	if (nmask)
		err = copy_nodes_to_user(nmask, maxnode, &nodes);

	return err;
}

#ifdef CONFIG_COMPAT

asmlinkage long compat_sys_get_mempolicy(int __user *policy,
				     compat_ulong_t __user *nmask,
				     compat_ulong_t maxnode,
				     compat_ulong_t addr, compat_ulong_t flags)
{
	long err;
	unsigned long __user *nm = NULL;
	unsigned long nr_bits, alloc_size;
	DECLARE_BITMAP(bm, MAX_NUMNODES);

	nr_bits = min_t(unsigned long, maxnode-1, MAX_NUMNODES);
	alloc_size = ALIGN(nr_bits, BITS_PER_LONG) / 8;

	if (nmask)
		nm = compat_alloc_user_space(alloc_size);

	err = sys_get_mempolicy(policy, nm, nr_bits+1, addr, flags);

	if (!err && nmask) {
		err = copy_from_user(bm, nm, alloc_size);
		/* ensure entire bitmap is zeroed */
		err |= clear_user(nmask, ALIGN(maxnode-1, 8) / 8);
		err |= compat_put_bitmap(nmask, bm, nr_bits);
	}

	return err;
}

asmlinkage long compat_sys_set_mempolicy(int mode, compat_ulong_t __user *nmask,
				     compat_ulong_t maxnode)
{
	long err = 0;
	unsigned long __user *nm = NULL;
	unsigned long nr_bits, alloc_size;
	DECLARE_BITMAP(bm, MAX_NUMNODES);

	nr_bits = min_t(unsigned long, maxnode-1, MAX_NUMNODES);
	alloc_size = ALIGN(nr_bits, BITS_PER_LONG) / 8;

	if (nmask) {
		err = compat_get_bitmap(bm, nmask, nr_bits);
		nm = compat_alloc_user_space(alloc_size);
		err |= copy_to_user(nm, bm, alloc_size);
	}

	if (err)
		return -EFAULT;

	return sys_set_mempolicy(mode, nm, nr_bits+1);
}

asmlinkage long compat_sys_mbind(compat_ulong_t start, compat_ulong_t len,
			     compat_ulong_t mode, compat_ulong_t __user *nmask,
			     compat_ulong_t maxnode, compat_ulong_t flags)
{
	long err = 0;
	unsigned long __user *nm = NULL;
	unsigned long nr_bits, alloc_size;
	nodemask_t bm;

	nr_bits = min_t(unsigned long, maxnode-1, MAX_NUMNODES);
	alloc_size = ALIGN(nr_bits, BITS_PER_LONG) / 8;

	if (nmask) {
		err = compat_get_bitmap(nodes_addr(bm), nmask, nr_bits);
		nm = compat_alloc_user_space(alloc_size);
		err |= copy_to_user(nm, nodes_addr(bm), alloc_size);
	}

	if (err)
		return -EFAULT;

	return sys_mbind(start, len, mode, nm, nr_bits+1, flags);
}

#endif

/*
 * get_vma_policy(@task, @vma, @addr)
 * @task - task for fallback if vma policy == default
 * @vma   - virtual memory area whose policy is sought
 * @addr  - address in @vma for shared policy lookup
 *
 * Returns effective policy for a VMA at specified address.
 * Falls back to @task or system default policy, as necessary.
 * Current or other task's task mempolicy and non-shared vma policies
 * are protected by the task's mmap_sem, which must be held for read by
 * the caller.
 * Shared policies [those marked as MPOL_F_SHARED] require an extra reference
 * count--added by the get_policy() vm_op, as appropriate--to protect against
 * freeing by another task.  It is the caller's responsibility to free the
 * extra reference for shared policies.
 */
static struct mempolicy *get_vma_policy(struct task_struct *task,
		struct vm_area_struct *vma, unsigned long addr)
{
	struct mempolicy *pol = task->mempolicy;

	if (vma) {
		if (vma->vm_ops && vma->vm_ops->get_policy) {
			struct mempolicy *vpol = vma->vm_ops->get_policy(vma,
									addr);
			if (vpol)
				pol = vpol;
		} else if (vma->vm_policy)
			pol = vma->vm_policy;
	}
	if (!pol)
		pol = &default_policy;
	return pol;
}

/*
 * Return a nodemask representing a mempolicy for filtering nodes for
 * page allocation
 */
static nodemask_t *policy_nodemask(gfp_t gfp, struct mempolicy *policy)
{
	/* Lower zones don't get a nodemask applied for MPOL_BIND */
	if (unlikely(policy->mode == MPOL_BIND) &&
			gfp_zone(gfp) >= policy_zone &&
			cpuset_nodemask_valid_mems_allowed(&policy->v.nodes))
		return &policy->v.nodes;

	return NULL;
}

/* Return a zonelist indicated by gfp for node representing a mempolicy */
static struct zonelist *policy_zonelist(gfp_t gfp, struct mempolicy *policy)
{
	int nd = numa_node_id();

	switch (policy->mode) {
	case MPOL_PREFERRED:
		if (!(policy->flags & MPOL_F_LOCAL))
			nd = policy->v.preferred_node;
		break;
	case MPOL_BIND:
		/*
		 * Normally, MPOL_BIND allocations are node-local within the
		 * allowed nodemask.  However, if __GFP_THISNODE is set and the
		 * current node isn't part of the mask, we use the zonelist for
		 * the first node in the mask instead.
		 */
		if (unlikely(gfp & __GFP_THISNODE) &&
				unlikely(!node_isset(nd, policy->v.nodes)))
			nd = first_node(policy->v.nodes);
		break;
	default:
		BUG();
	}
	return node_zonelist(nd, gfp);
}

/* Do dynamic interleaving for a process */
static unsigned interleave_nodes(struct mempolicy *policy)
{
	unsigned nid, next;
	struct task_struct *me = current;

	nid = me->il_next;
	next = next_node(nid, policy->v.nodes);
	if (next >= MAX_NUMNODES)
		next = first_node(policy->v.nodes);
	if (next < MAX_NUMNODES)
		me->il_next = next;
	return nid;
}

/*
 * Depending on the memory policy provide a node from which to allocate the
 * next slab entry.
 * @policy must be protected by freeing by the caller.  If @policy is
 * the current task's mempolicy, this protection is implicit, as only the
 * task can change it's policy.  The system default policy requires no
 * such protection.
 */
unsigned slab_node(struct mempolicy *policy)
{
	if (!policy || policy->flags & MPOL_F_LOCAL)
		return numa_node_id();

	switch (policy->mode) {
	case MPOL_PREFERRED:
		/*
		 * handled MPOL_F_LOCAL above
		 */
		return policy->v.preferred_node;

	case MPOL_INTERLEAVE:
		return interleave_nodes(policy);

	case MPOL_BIND: {
		/*
		 * Follow bind policy behavior and start allocation at the
		 * first node.
		 */
		struct zonelist *zonelist;
		struct zone *zone;
		enum zone_type highest_zoneidx = gfp_zone(GFP_KERNEL);
		zonelist = &NODE_DATA(numa_node_id())->node_zonelists[0];
		(void)first_zones_zonelist(zonelist, highest_zoneidx,
							&policy->v.nodes,
							&zone);
		return zone ? zone->node : numa_node_id();
	}

	default:
		BUG();
	}
}

/* Do static interleaving for a VMA with known offset. */
static unsigned offset_il_node(struct mempolicy *pol,
		struct vm_area_struct *vma, unsigned long off)
{
	unsigned nnodes = nodes_weight(pol->v.nodes);
	unsigned target;
	int c;
	int nid = -1;

	if (!nnodes)
		return numa_node_id();
	target = (unsigned int)off % nnodes;
	c = 0;
	do {
		nid = next_node(nid, pol->v.nodes);
		c++;
	} while (c <= target);
	return nid;
}

/* Determine a node number for interleave */
static inline unsigned interleave_nid(struct mempolicy *pol,
		 struct vm_area_struct *vma, unsigned long addr, int shift)
{
	if (vma) {
		unsigned long off;

		/*
		 * for small pages, there is no difference between
		 * shift and PAGE_SHIFT, so the bit-shift is safe.
		 * for huge pages, since vm_pgoff is in units of small
		 * pages, we need to shift off the always 0 bits to get
		 * a useful offset.
		 */
		BUG_ON(shift < PAGE_SHIFT);
		off = vma->vm_pgoff >> (shift - PAGE_SHIFT);
		off += (addr - vma->vm_start) >> shift;
		return offset_il_node(pol, vma, off);
	} else
		return interleave_nodes(pol);
}

#ifdef CONFIG_HUGETLBFS
/*
 * huge_zonelist(@vma, @addr, @gfp_flags, @mpol)
 * @vma = virtual memory area whose policy is sought
 * @addr = address in @vma for shared policy lookup and interleave policy
 * @gfp_flags = for requested zone
 * @mpol = pointer to mempolicy pointer for reference counted mempolicy
 * @nodemask = pointer to nodemask pointer for MPOL_BIND nodemask
 *
 * Returns a zonelist suitable for a huge page allocation and a pointer
 * to the struct mempolicy for conditional unref after allocation.
 * If the effective policy is 'BIND, returns a pointer to the mempolicy's
 * @nodemask for filtering the zonelist.
 *
 * Must be protected by get_mems_allowed()
 */
struct zonelist *huge_zonelist(struct vm_area_struct *vma, unsigned long addr,
				gfp_t gfp_flags, struct mempolicy **mpol,
				nodemask_t **nodemask)
{
	struct zonelist *zl;

	*mpol = get_vma_policy(current, vma, addr);
	*nodemask = NULL;	/* assume !MPOL_BIND */

	if (unlikely((*mpol)->mode == MPOL_INTERLEAVE)) {
		zl = node_zonelist(interleave_nid(*mpol, vma, addr,
				huge_page_shift(hstate_vma(vma))), gfp_flags);
	} else {
		zl = policy_zonelist(gfp_flags, *mpol);
		if ((*mpol)->mode == MPOL_BIND)
			*nodemask = &(*mpol)->v.nodes;
	}
	return zl;
}

/*
 * init_nodemask_of_mempolicy
 *
 * If the current task's mempolicy is "default" [NULL], return 'false'
 * to indicate default policy.  Otherwise, extract the policy nodemask
 * for 'bind' or 'interleave' policy into the argument nodemask, or
 * initialize the argument nodemask to contain the single node for
 * 'preferred' or 'local' policy and return 'true' to indicate presence
 * of non-default mempolicy.
 *
 * We don't bother with reference counting the mempolicy [mpol_get/put]
 * because the current task is examining it's own mempolicy and a task's
 * mempolicy is only ever changed by the task itself.
 *
 * N.B., it is the caller's responsibility to free a returned nodemask.
 */
bool init_nodemask_of_mempolicy(nodemask_t *mask)
{
	struct mempolicy *mempolicy;
	int nid;

	if (!(mask && current->mempolicy))
		return false;

	task_lock(current);
	mempolicy = current->mempolicy;
	switch (mempolicy->mode) {
	case MPOL_PREFERRED:
		if (mempolicy->flags & MPOL_F_LOCAL)
			nid = numa_node_id();
		else
			nid = mempolicy->v.preferred_node;
		init_nodemask_of_node(mask, nid);
		break;

	case MPOL_BIND:
		/* Fall through */
	case MPOL_INTERLEAVE:
		*mask =  mempolicy->v.nodes;
		break;

	default:
		BUG();
	}
	task_unlock(current);

	return true;
}
#endif

/*
 * mempolicy_nodemask_intersects
 *
 * If tsk's mempolicy is "default" [NULL], return 'true' to indicate default
 * policy.  Otherwise, check for intersection between mask and the policy
 * nodemask for 'bind' or 'interleave' policy.  For 'perferred' or 'local'
 * policy, always return true since it may allocate elsewhere on fallback.
 *
 * Takes task_lock(tsk) to prevent freeing of its mempolicy.
 */
bool mempolicy_nodemask_intersects(struct task_struct *tsk,
					const nodemask_t *mask)
{
	struct mempolicy *mempolicy;
	bool ret = true;

	if (!mask)
		return ret;
	task_lock(tsk);
	mempolicy = tsk->mempolicy;
	if (!mempolicy)
		goto out;

	switch (mempolicy->mode) {
	case MPOL_PREFERRED:
		/*
		 * MPOL_PREFERRED and MPOL_F_LOCAL are only preferred nodes to
		 * allocate from, they may fallback to other nodes when oom.
		 * Thus, it's possible for tsk to have allocated memory from
		 * nodes in mask.
		 */
		break;
	case MPOL_BIND:
	case MPOL_INTERLEAVE:
		ret = nodes_intersects(mempolicy->v.nodes, *mask);
		break;
	default:
		BUG();
	}
out:
	task_unlock(tsk);
	return ret;
}

/* Allocate a page in interleaved policy.
   Own path because it needs to do special accounting. */
static struct page *alloc_page_interleave(gfp_t gfp, unsigned order,
					unsigned nid)
{
	struct zonelist *zl;
	struct page *page;

	zl = node_zonelist(nid, gfp);
	page = __alloc_pages(gfp, order, zl);
	if (page && page_zone(page) == zonelist_zone(&zl->_zonerefs[0]))
		inc_zone_page_state(page, NUMA_INTERLEAVE_HIT);
	return page;
}

/**
 * 	alloc_page_vma	- Allocate a page for a VMA.
 *
 * 	@gfp:
 *      %GFP_USER    user allocation.
 *      %GFP_KERNEL  kernel allocations,
 *      %GFP_HIGHMEM highmem/user allocations,
 *      %GFP_FS      allocation should not call back into a file system.
 *      %GFP_ATOMIC  don't sleep.
 *
 * 	@vma:  Pointer to VMA or NULL if not available.
 *	@addr: Virtual Address of the allocation. Must be inside the VMA.
 *
 * 	This function allocates a page from the kernel page pool and applies
 *	a NUMA policy associated with the VMA or the current process.
 *	When VMA is not NULL caller must hold down_read on the mmap_sem of the
 *	mm_struct of the VMA to prevent it from going away. Should be used for
 *	all allocations for pages that will be mapped into
 * 	user space. Returns NULL when no page can be allocated.
 *
 *	Should be called with the mm_sem of the vma hold.
 */
struct page *
alloc_page_vma(gfp_t gfp, struct vm_area_struct *vma, unsigned long addr)
{
	struct mempolicy *pol = get_vma_policy(current, vma, addr);
	struct zonelist *zl;
	struct page *page;

	get_mems_allowed();
	if (unlikely(pol->mode == MPOL_INTERLEAVE)) {
		unsigned nid;

		nid = interleave_nid(pol, vma, addr, PAGE_SHIFT);
		mpol_cond_put(pol);
		page = alloc_page_interleave(gfp, 0, nid);
		put_mems_allowed();
		return page;
	}
	zl = policy_zonelist(gfp, pol);
	if (unlikely(mpol_needs_cond_ref(pol))) {
		/*
		 * slow path: ref counted shared policy
		 */
		struct page *page =  __alloc_pages_nodemask(gfp, 0,
						zl, policy_nodemask(gfp, pol));
		__mpol_put(pol);
		put_mems_allowed();
		return page;
	}
	/*
	 * fast path:  default or task policy
	 */
	page = __alloc_pages_nodemask(gfp, 0, zl, policy_nodemask(gfp, pol));
	put_mems_allowed();
	return page;
}

/**
 * 	alloc_pages_current - Allocate pages.
 *
 *	@gfp:
 *		%GFP_USER   user allocation,
 *      	%GFP_KERNEL kernel allocation,
 *      	%GFP_HIGHMEM highmem allocation,
 *      	%GFP_FS     don't call back into a file system.
 *      	%GFP_ATOMIC don't sleep.
 *	@order: Power of two of allocation size in pages. 0 is a single page.
 *
 *	Allocate a page from the kernel page pool.  When not in
 *	interrupt context and apply the current process NUMA policy.
 *	Returns NULL when no page can be allocated.
 *
 *	Don't call cpuset_update_task_memory_state() unless
 *	1) it's ok to take cpuset_sem (can WAIT), and
 *	2) allocating for current task (not interrupt).
 */
struct page *alloc_pages_current(gfp_t gfp, unsigned order)
{
	struct mempolicy *pol = current->mempolicy;
	struct page *page;

	if (!pol || in_interrupt() || (gfp & __GFP_THISNODE))
		pol = &default_policy;

	get_mems_allowed();
	/*
	 * No reference counting needed for current->mempolicy
	 * nor system default_policy
	 */
	if (pol->mode == MPOL_INTERLEAVE)
		page = alloc_page_interleave(gfp, order, interleave_nodes(pol));
	else
		page = __alloc_pages_nodemask(gfp, order,
			policy_zonelist(gfp, pol), policy_nodemask(gfp, pol));
	put_mems_allowed();
	return page;
}
EXPORT_SYMBOL(alloc_pages_current);

/*
 * If mpol_dup() sees current->cpuset == cpuset_being_rebound, then it
 * rebinds the mempolicy its copying by calling mpol_rebind_policy()
 * with the mems_allowed returned by cpuset_mems_allowed().  This
 * keeps mempolicies cpuset relative after its cpuset moves.  See
 * further kernel/cpuset.c update_nodemask().
 *
 * current's mempolicy may be rebinded by the other task(the task that changes
 * cpuset's mems), so we needn't do rebind work for current task.
 */

/* Slow path of a mempolicy duplicate */
struct mempolicy *__mpol_dup(struct mempolicy *old)
{
	struct mempolicy *new = kmem_cache_alloc(policy_cache, GFP_KERNEL);

	if (!new)
		return ERR_PTR(-ENOMEM);

	/* task's mempolicy is protected by alloc_lock */
	if (old == current->mempolicy) {
		task_lock(current);
		*new = *old;
		task_unlock(current);
	} else
		*new = *old;

	rcu_read_lock();
	if (current_cpuset_is_being_rebound()) {
		nodemask_t mems = cpuset_mems_allowed(current);
		if (new->flags & MPOL_F_REBINDING)
			mpol_rebind_policy(new, &mems, MPOL_REBIND_STEP2);
		else
			mpol_rebind_policy(new, &mems, MPOL_REBIND_ONCE);
	}
	rcu_read_unlock();
	atomic_set(&new->refcnt, 1);
	return new;
}

/*
 * If *frompol needs [has] an extra ref, copy *frompol to *tompol ,
 * eliminate the * MPOL_F_* flags that require conditional ref and
 * [NOTE!!!] drop the extra ref.  Not safe to reference *frompol directly
 * after return.  Use the returned value.
 *
 * Allows use of a mempolicy for, e.g., multiple allocations with a single
 * policy lookup, even if the policy needs/has extra ref on lookup.
 * shmem_readahead needs this.
 */
struct mempolicy *__mpol_cond_copy(struct mempolicy *tompol,
						struct mempolicy *frompol)
{
	if (!mpol_needs_cond_ref(frompol))
		return frompol;

	*tompol = *frompol;
	tompol->flags &= ~MPOL_F_SHARED;	/* copy doesn't need unref */
	__mpol_put(frompol);
	return tompol;
}

/* Slow path of a mempolicy comparison */
int __mpol_equal(struct mempolicy *a, struct mempolicy *b)
{
	if (!a || !b)
		return 0;
	if (a->mode != b->mode)
		return 0;
	if (a->flags != b->flags)
		return 0;
	if (mpol_store_user_nodemask(a))
		if (!nodes_equal(a->w.user_nodemask, b->w.user_nodemask))
			return 0;

	switch (a->mode) {
	case MPOL_BIND:
		/* Fall through */
	case MPOL_INTERLEAVE:
		return nodes_equal(a->v.nodes, b->v.nodes);
	case MPOL_PREFERRED:
		return a->v.preferred_node == b->v.preferred_node &&
			a->flags == b->flags;
	default:
		BUG();
		return 0;
	}
}

/*
 * Shared memory backing store policy support.
 *
 * Remember policies even when nobody has shared memory mapped.
 * The policies are kept in Red-Black tree linked from the inode.
 * They are protected by the sp->lock spinlock, which should be held
 * for any accesses to the tree.
 */

/* lookup first element intersecting start-end */
/* Caller holds sp->lock */
static struct sp_node *
sp_lookup(struct shared_policy *sp, unsigned long start, unsigned long end)
{
	struct rb_node *n = sp->root.rb_node;

	while (n) {
		struct sp_node *p = rb_entry(n, struct sp_node, nd);

		if (start >= p->end)
			n = n->rb_right;
		else if (end <= p->start)
			n = n->rb_left;
		else
			break;
	}
	if (!n)
		return NULL;
	for (;;) {
		struct sp_node *w = NULL;
		struct rb_node *prev = rb_prev(n);
		if (!prev)
			break;
		w = rb_entry(prev, struct sp_node, nd);
		if (w->end <= start)
			break;
		n = prev;
	}
	return rb_entry(n, struct sp_node, nd);
}

/* Insert a new shared policy into the list. */
/* Caller holds sp->lock */
static void sp_insert(struct shared_policy *sp, struct sp_node *new)
{
	struct rb_node **p = &sp->root.rb_node;
	struct rb_node *parent = NULL;
	struct sp_node *nd;

	while (*p) {
		parent = *p;
		nd = rb_entry(parent, struct sp_node, nd);
		if (new->start < nd->start)
			p = &(*p)->rb_left;
		else if (new->end > nd->end)
			p = &(*p)->rb_right;
		else
			BUG();
	}
	rb_link_node(&new->nd, parent, p);
	rb_insert_color(&new->nd, &sp->root);
	pr_debug("inserting %lx-%lx: %d\n", new->start, new->end,
		 new->policy ? new->policy->mode : 0);
}

/* Find shared policy intersecting idx */
struct mempolicy *
mpol_shared_policy_lookup(struct shared_policy *sp, unsigned long idx)
{
	struct mempolicy *pol = NULL;
	struct sp_node *sn;

	if (!sp->root.rb_node)
		return NULL;
	spin_lock(&sp->lock);
	sn = sp_lookup(sp, idx, idx+1);
	if (sn) {
		mpol_get(sn->policy);
		pol = sn->policy;
	}
	spin_unlock(&sp->lock);
	return pol;
}

static void sp_delete(struct shared_policy *sp, struct sp_node *n)
{
	pr_debug("deleting %lx-l%lx\n", n->start, n->end);
	rb_erase(&n->nd, &sp->root);
	mpol_put(n->policy);
	kmem_cache_free(sn_cache, n);
}

static struct sp_node *sp_alloc(unsigned long start, unsigned long end,
				struct mempolicy *pol)
{
	struct sp_node *n = kmem_cache_alloc(sn_cache, GFP_KERNEL);

	if (!n)
		return NULL;
	n->start = start;
	n->end = end;
	mpol_get(pol);
	pol->flags |= MPOL_F_SHARED;	/* for unref */
	n->policy = pol;
	return n;
}

/* Replace a policy range. */
static int shared_policy_replace(struct shared_policy *sp, unsigned long start,
				 unsigned long end, struct sp_node *new)
{
	struct sp_node *n, *new2 = NULL;

restart:
	spin_lock(&sp->lock);
	n = sp_lookup(sp, start, end);
	/* Take care of old policies in the same range. */
	while (n && n->start < end) {
		struct rb_node *next = rb_next(&n->nd);
		if (n->start >= start) {
			if (n->end <= end)
				sp_delete(sp, n);
			else
				n->start = end;
		} else {
			/* Old policy spanning whole new range. */
			if (n->end > end) {
				if (!new2) {
					spin_unlock(&sp->lock);
					new2 = sp_alloc(end, n->end, n->policy);
					if (!new2)
						return -ENOMEM;
					goto restart;
				}
				n->end = start;
				sp_insert(sp, new2);
				new2 = NULL;
				break;
			} else
				n->end = start;
		}
		if (!next)
			break;
		n = rb_entry(next, struct sp_node, nd);
	}
	if (new)
		sp_insert(sp, new);
	spin_unlock(&sp->lock);
	if (new2) {
		mpol_put(new2->policy);
		kmem_cache_free(sn_cache, new2);
	}
	return 0;
}

/**
 * mpol_shared_policy_init - initialize shared policy for inode
 * @sp: pointer to inode shared policy
 * @mpol:  struct mempolicy to install
 *
 * Install non-NULL @mpol in inode's shared policy rb-tree.
 * On entry, the current task has a reference on a non-NULL @mpol.
 * This must be released on exit.
 * This is called at get_inode() calls and we can use GFP_KERNEL.
 */
void mpol_shared_policy_init(struct shared_policy *sp, struct mempolicy *mpol)
{
	int ret;

	sp->root = RB_ROOT;		/* empty tree == default mempolicy */
	spin_lock_init(&sp->lock);

	if (mpol) {
		struct vm_area_struct pvma;
		struct mempolicy *new;
		NODEMASK_SCRATCH(scratch);

		if (!scratch)
			goto put_mpol;
		/* contextualize the tmpfs mount point mempolicy */
		new = mpol_new(mpol->mode, mpol->flags, &mpol->w.user_nodemask);
		if (IS_ERR(new))
			goto free_scratch; /* no valid nodemask intersection */

		task_lock(current);
		ret = mpol_set_nodemask(new, &mpol->w.user_nodemask, scratch);
		task_unlock(current);
		if (ret)
			goto put_new;

		/* Create pseudo-vma that contains just the policy */
		memset(&pvma, 0, sizeof(struct vm_area_struct));
		pvma.vm_end = TASK_SIZE;	/* policy covers entire file */
		mpol_set_shared_policy(sp, &pvma, new); /* adds ref */

put_new:
		mpol_put(new);			/* drop initial ref */
free_scratch:
		NODEMASK_SCRATCH_FREE(scratch);
put_mpol:
		mpol_put(mpol);	/* drop our incoming ref on sb mpol */
	}
}

int mpol_set_shared_policy(struct shared_policy *info,
			struct vm_area_struct *vma, struct mempolicy *npol)
{
	int err;
	struct sp_node *new = NULL;
	unsigned long sz = vma_pages(vma);

	pr_debug("set_shared_policy %lx sz %lu %d %d %lx\n",
		 vma->vm_pgoff,
		 sz, npol ? npol->mode : -1,
		 npol ? npol->flags : -1,
		 npol ? nodes_addr(npol->v.nodes)[0] : -1);

	if (npol) {
		new = sp_alloc(vma->vm_pgoff, vma->vm_pgoff + sz, npol);
		if (!new)
			return -ENOMEM;
	}
	err = shared_policy_replace(info, vma->vm_pgoff, vma->vm_pgoff+sz, new);
	if (err && new)
		kmem_cache_free(sn_cache, new);
	return err;
}

/* Free a backing policy store on inode delete. */
void mpol_free_shared_policy(struct shared_policy *p)
{
	struct sp_node *n;
	struct rb_node *next;

	if (!p->root.rb_node)
		return;
	spin_lock(&p->lock);
	next = rb_first(&p->root);
	while (next) {
		n = rb_entry(next, struct sp_node, nd);
		next = rb_next(&n->nd);
		rb_erase(&n->nd, &p->root);
		mpol_put(n->policy);
		kmem_cache_free(sn_cache, n);
	}
	spin_unlock(&p->lock);
}

/* assumes fs == KERNEL_DS */
void __init numa_policy_init(void)
{
	nodemask_t interleave_nodes;
	unsigned long largest = 0;
	int nid, prefer = 0;

	policy_cache = kmem_cache_create("numa_policy",
					 sizeof(struct mempolicy),
					 0, SLAB_PANIC, NULL);

	sn_cache = kmem_cache_create("shared_policy_node",
				     sizeof(struct sp_node),
				     0, SLAB_PANIC, NULL);

	/*
	 * Set interleaving policy for system init. Interleaving is only
	 * enabled across suitably sized nodes (default is >= 16MB), or
	 * fall back to the largest node if they're all smaller.
	 */
	nodes_clear(interleave_nodes);
	for_each_node_state(nid, N_HIGH_MEMORY) {
		unsigned long total_pages = node_present_pages(nid);

		/* Preserve the largest node */
		if (largest < total_pages) {
			largest = total_pages;
			prefer = nid;
		}

		/* Interleave this node? */
		if ((total_pages << PAGE_SHIFT) >= (16 << 20))
			node_set(nid, interleave_nodes);
	}

	/* All too small, use the largest */
	if (unlikely(nodes_empty(interleave_nodes)))
		node_set(prefer, interleave_nodes);

	if (do_set_mempolicy(MPOL_INTERLEAVE, 0, &interleave_nodes))
		printk("numa_policy_init: interleaving failed\n");
}

/* Reset policy of current process to default */
void numa_default_policy(void)
{
	do_set_mempolicy(MPOL_DEFAULT, 0, NULL);
}

/*
 * Parse and format mempolicy from/to strings
 */

/*
 * "local" is pseudo-policy:  MPOL_PREFERRED with MPOL_F_LOCAL flag
 * Used only for mpol_parse_str() and mpol_to_str()
 */
#define MPOL_LOCAL MPOL_MAX
static const char * const policy_modes[] =
{
	[MPOL_DEFAULT]    = "default",
	[MPOL_PREFERRED]  = "prefer",
	[MPOL_BIND]       = "bind",
	[MPOL_INTERLEAVE] = "interleave",
	[MPOL_LOCAL]      = "local"
};


#ifdef CONFIG_TMPFS
/**
 * mpol_parse_str - parse string to mempolicy
 * @str:  string containing mempolicy to parse
 * @mpol:  pointer to struct mempolicy pointer, returned on success.
 * @no_context:  flag whether to "contextualize" the mempolicy
 *
 * Format of input:
 *	<mode>[=<flags>][:<nodelist>]
 *
 * if @no_context is true, save the input nodemask in w.user_nodemask in
 * the returned mempolicy.  This will be used to "clone" the mempolicy in
 * a specific context [cpuset] at a later time.  Used to parse tmpfs mpol
 * mount option.  Note that if 'static' or 'relative' mode flags were
 * specified, the input nodemask will already have been saved.  Saving
 * it again is redundant, but safe.
 *
 * On success, returns 0, else 1
 */
int mpol_parse_str(char *str, struct mempolicy **mpol, int no_context)
{
	struct mempolicy *new = NULL;
	unsigned short mode;
	unsigned short uninitialized_var(mode_flags);
	nodemask_t nodes;
	char *nodelist = strchr(str, ':');
	char *flags = strchr(str, '=');
	int err = 1;

	if (nodelist) {
		/* NUL-terminate mode or flags string */
		*nodelist++ = '\0';
		if (nodelist_parse(nodelist, nodes))
			goto out;
		if (!nodes_subset(nodes, node_states[N_HIGH_MEMORY]))
			goto out;
	} else
		nodes_clear(nodes);

	if (flags)
		*flags++ = '\0';	/* terminate mode string */

	for (mode = 0; mode <= MPOL_LOCAL; mode++) {
		if (!strcmp(str, policy_modes[mode])) {
			break;
		}
	}
	if (mode > MPOL_LOCAL)
		goto out;

	switch (mode) {
	case MPOL_PREFERRED:
		/*
		 * Insist on a nodelist of one node only
		 */
		if (nodelist) {
			char *rest = nodelist;
			while (isdigit(*rest))
				rest++;
			if (*rest)
				goto out;
		}
		break;
	case MPOL_INTERLEAVE:
		/*
		 * Default to online nodes with memory if no nodelist
		 */
		if (!nodelist)
			nodes = node_states[N_HIGH_MEMORY];
		break;
	case MPOL_LOCAL:
		/*
		 * Don't allow a nodelist;  mpol_new() checks flags
		 */
		if (nodelist)
			goto out;
		mode = MPOL_PREFERRED;
		break;
	case MPOL_DEFAULT:
		/*
		 * Insist on a empty nodelist
		 */
		if (!nodelist)
			err = 0;
		goto out;
	case MPOL_BIND:
		/*
		 * Insist on a nodelist
		 */
		if (!nodelist)
			goto out;
	}

	mode_flags = 0;
	if (flags) {
		/*
		 * Currently, we only support two mutually exclusive
		 * mode flags.
		 */
		if (!strcmp(flags, "static"))
			mode_flags |= MPOL_F_STATIC_NODES;
		else if (!strcmp(flags, "relative"))
			mode_flags |= MPOL_F_RELATIVE_NODES;
		else
			goto out;
	}

	new = mpol_new(mode, mode_flags, &nodes);
	if (IS_ERR(new))
		goto out;

	if (no_context) {
		/* save for contextualization */
		new->w.user_nodemask = nodes;
	} else {
		int ret;
		NODEMASK_SCRATCH(scratch);
		if (scratch) {
			task_lock(current);
			ret = mpol_set_nodemask(new, &nodes, scratch);
			task_unlock(current);
		} else
			ret = -ENOMEM;
		NODEMASK_SCRATCH_FREE(scratch);
		if (ret) {
			mpol_put(new);
			goto out;
		}
	}
	err = 0;

out:
	/* Restore string for error message */
	if (nodelist)
		*--nodelist = ':';
	if (flags)
		*--flags = '=';
	if (!err)
		*mpol = new;
	return err;
}
#endif /* CONFIG_TMPFS */

/**
 * mpol_to_str - format a mempolicy structure for printing
 * @buffer:  to contain formatted mempolicy string
 * @maxlen:  length of @buffer
 * @pol:  pointer to mempolicy to be formatted
 * @no_context:  "context free" mempolicy - use nodemask in w.user_nodemask
 *
 * Convert a mempolicy into a string.
 * Returns the number of characters in buffer (if positive)
 * or an error (negative)
 */
int mpol_to_str(char *buffer, int maxlen, struct mempolicy *pol, int no_context)
{
	char *p = buffer;
	int l;
	nodemask_t nodes;
	unsigned short mode;
	unsigned short flags = pol ? pol->flags : 0;

	/*
	 * Sanity check:  room for longest mode, flag and some nodes
	 */
	VM_BUG_ON(maxlen < strlen("interleave") + strlen("relative") + 16);

	if (!pol || pol == &default_policy)
		mode = MPOL_DEFAULT;
	else
		mode = pol->mode;

	switch (mode) {
	case MPOL_DEFAULT:
		nodes_clear(nodes);
		break;

	case MPOL_PREFERRED:
		nodes_clear(nodes);
		if (flags & MPOL_F_LOCAL)
			mode = MPOL_LOCAL;	/* pseudo-policy */
		else
			node_set(pol->v.preferred_node, nodes);
		break;

	case MPOL_BIND:
		/* Fall through */
	case MPOL_INTERLEAVE:
		if (no_context)
			nodes = pol->w.user_nodemask;
		else
			nodes = pol->v.nodes;
		break;

	default:
		BUG();
	}

	l = strlen(policy_modes[mode]);
	if (buffer + maxlen < p + l + 1)
		return -ENOSPC;

	strcpy(p, policy_modes[mode]);
	p += l;

	if (flags & MPOL_MODE_FLAGS) {
		if (buffer + maxlen < p + 2)
			return -ENOSPC;
		*p++ = '=';

		/*
		 * Currently, the only defined flags are mutually exclusive
		 */
		if (flags & MPOL_F_STATIC_NODES)
			p += snprintf(p, buffer + maxlen - p, "static");
		else if (flags & MPOL_F_RELATIVE_NODES)
			p += snprintf(p, buffer + maxlen - p, "relative");
	}

	if (!nodes_empty(nodes)) {
		if (buffer + maxlen < p + 2)
			return -ENOSPC;
		*p++ = ':';
	 	p += nodelist_scnprintf(p, buffer + maxlen - p, nodes);
	}
	return p - buffer;
}

struct numa_maps {
	unsigned long pages;
	unsigned long anon;
	unsigned long active;
	unsigned long writeback;
	unsigned long mapcount_max;
	unsigned long dirty;
	unsigned long swapcache;
	unsigned long node[MAX_NUMNODES];
};

static void gather_stats(struct page *page, void *private, int pte_dirty)
{
	struct numa_maps *md = private;
	int count = page_mapcount(page);

	md->pages++;
	if (pte_dirty || PageDirty(page))
		md->dirty++;

	if (PageSwapCache(page))
		md->swapcache++;

	if (PageActive(page) || PageUnevictable(page))
		md->active++;

	if (PageWriteback(page))
		md->writeback++;

	if (PageAnon(page))
		md->anon++;

	if (count > md->mapcount_max)
		md->mapcount_max = count;

	md->node[page_to_nid(page)]++;
}

#ifdef CONFIG_HUGETLB_PAGE
static void check_huge_range(struct vm_area_struct *vma,
		unsigned long start, unsigned long end,
		struct numa_maps *md)
{
	unsigned long addr;
	struct page *page;
	struct hstate *h = hstate_vma(vma);
	unsigned long sz = huge_page_size(h);

	for (addr = start; addr < end; addr += sz) {
		pte_t *ptep = huge_pte_offset(vma->vm_mm,
						addr & huge_page_mask(h));
		pte_t pte;

		if (!ptep)
			continue;

		pte = *ptep;
		if (pte_none(pte))
			continue;

		page = pte_page(pte);
		if (!page)
			continue;

		gather_stats(page, md, pte_dirty(*ptep));
	}
}
#else
static inline void check_huge_range(struct vm_area_struct *vma,
		unsigned long start, unsigned long end,
		struct numa_maps *md)
{
}
#endif

/*
 * Display pages allocated per node and memory policy via /proc.
 */
int show_numa_map(struct seq_file *m, void *v)
{
	struct proc_maps_private *priv = m->private;
	struct vm_area_struct *vma = v;
	struct numa_maps *md;
	struct file *file = vma->vm_file;
	struct mm_struct *mm = vma->vm_mm;
	struct mempolicy *pol;
	int n;
	char buffer[50];

	if (!mm)
		return 0;

	md = kzalloc(sizeof(struct numa_maps), GFP_KERNEL);
	if (!md)
		return 0;

	pol = get_vma_policy(priv->task, vma, vma->vm_start);
	mpol_to_str(buffer, sizeof(buffer), pol, 0);
	mpol_cond_put(pol);

	seq_printf(m, "%08lx %s", vma->vm_start, buffer);

	if (file) {
		seq_printf(m, " file=");
		seq_path(m, &file->f_path, "\n\t= ");
	} else if (vma->vm_start <= mm->brk && vma->vm_end >= mm->start_brk) {
		seq_printf(m, " heap");
	} else if (vma->vm_start <= mm->start_stack &&
			vma->vm_end >= mm->start_stack) {
		seq_printf(m, " stack");
	}

	if (is_vm_hugetlb_page(vma)) {
		check_huge_range(vma, vma->vm_start, vma->vm_end, md);
		seq_printf(m, " huge");
	} else {
		check_pgd_range(vma, vma->vm_start, vma->vm_end,
			&node_states[N_HIGH_MEMORY], MPOL_MF_STATS, md);
	}

	if (!md->pages)
		goto out;

	if (md->anon)
		seq_printf(m," anon=%lu",md->anon);

	if (md->dirty)
		seq_printf(m," dirty=%lu",md->dirty);

	if (md->pages != md->anon && md->pages != md->dirty)
		seq_printf(m, " mapped=%lu", md->pages);

	if (md->mapcount_max > 1)
		seq_printf(m, " mapmax=%lu", md->mapcount_max);

	if (md->swapcache)
		seq_printf(m," swapcache=%lu", md->swapcache);

	if (md->active < md->pages && !is_vm_hugetlb_page(vma))
		seq_printf(m," active=%lu", md->active);

	if (md->writeback)
		seq_printf(m," writeback=%lu", md->writeback);

	for_each_node_state(n, N_HIGH_MEMORY)
		if (md->node[n])
			seq_printf(m, " N%d=%lu", n, md->node[n]);
out:
	seq_putc(m, '\n');
	kfree(md);

	if (m->count < m->size)
		m->version = (vma != priv->tail_vma) ? vma->vm_start : 0;
	return 0;
}
