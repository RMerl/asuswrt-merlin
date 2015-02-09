/*
 * Kernel-based Virtual Machine driver for Linux
 *
 * This module enables machines with Intel VT-x extensions to run virtual
 * machines without emulation or binary translation.
 *
 * MMU support
 *
 * Copyright (C) 2006 Qumranet, Inc.
 * Copyright 2010 Red Hat, Inc. and/or its affilates.
 *
 * Authors:
 *   Yaniv Kamay  <yaniv@qumranet.com>
 *   Avi Kivity   <avi@qumranet.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "mmu.h"
#include "x86.h"
#include "kvm_cache_regs.h"

#include <linux/kvm_host.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/hugetlb.h>
#include <linux/compiler.h>
#include <linux/srcu.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asm/page.h>
#include <asm/cmpxchg.h>
#include <asm/io.h>
#include <asm/vmx.h>

/*
 * When setting this variable to true it enables Two-Dimensional-Paging
 * where the hardware walks 2 page tables:
 * 1. the guest-virtual to guest-physical
 * 2. while doing 1. it walks guest-physical to host-physical
 * If the hardware supports that we don't need to do shadow paging.
 */
bool tdp_enabled = false;

#undef MMU_DEBUG

#undef AUDIT

#ifdef AUDIT
static void kvm_mmu_audit(struct kvm_vcpu *vcpu, const char *msg);
#else
static void kvm_mmu_audit(struct kvm_vcpu *vcpu, const char *msg) {}
#endif

#ifdef MMU_DEBUG

#define pgprintk(x...) do { if (dbg) printk(x); } while (0)
#define rmap_printk(x...) do { if (dbg) printk(x); } while (0)

#else

#define pgprintk(x...) do { } while (0)
#define rmap_printk(x...) do { } while (0)

#endif

#if defined(MMU_DEBUG) || defined(AUDIT)
static int dbg = 0;
module_param(dbg, bool, 0644);
#endif

static int oos_shadow = 1;
module_param(oos_shadow, bool, 0644);

#ifndef MMU_DEBUG
#define ASSERT(x) do { } while (0)
#else
#define ASSERT(x)							\
	if (!(x)) {							\
		printk(KERN_WARNING "assertion failed %s:%d: %s\n",	\
		       __FILE__, __LINE__, #x);				\
	}
#endif

#define PT_FIRST_AVAIL_BITS_SHIFT 9
#define PT64_SECOND_AVAIL_BITS_SHIFT 52

#define PT64_LEVEL_BITS 9

#define PT64_LEVEL_SHIFT(level) \
		(PAGE_SHIFT + (level - 1) * PT64_LEVEL_BITS)

#define PT64_LEVEL_MASK(level) \
		(((1ULL << PT64_LEVEL_BITS) - 1) << PT64_LEVEL_SHIFT(level))

#define PT64_INDEX(address, level)\
	(((address) >> PT64_LEVEL_SHIFT(level)) & ((1 << PT64_LEVEL_BITS) - 1))


#define PT32_LEVEL_BITS 10

#define PT32_LEVEL_SHIFT(level) \
		(PAGE_SHIFT + (level - 1) * PT32_LEVEL_BITS)

#define PT32_LEVEL_MASK(level) \
		(((1ULL << PT32_LEVEL_BITS) - 1) << PT32_LEVEL_SHIFT(level))
#define PT32_LVL_OFFSET_MASK(level) \
	(PT32_BASE_ADDR_MASK & ((1ULL << (PAGE_SHIFT + (((level) - 1) \
						* PT32_LEVEL_BITS))) - 1))

#define PT32_INDEX(address, level)\
	(((address) >> PT32_LEVEL_SHIFT(level)) & ((1 << PT32_LEVEL_BITS) - 1))


#define PT64_BASE_ADDR_MASK (((1ULL << 52) - 1) & ~(u64)(PAGE_SIZE-1))
#define PT64_DIR_BASE_ADDR_MASK \
	(PT64_BASE_ADDR_MASK & ~((1ULL << (PAGE_SHIFT + PT64_LEVEL_BITS)) - 1))
#define PT64_LVL_ADDR_MASK(level) \
	(PT64_BASE_ADDR_MASK & ~((1ULL << (PAGE_SHIFT + (((level) - 1) \
						* PT64_LEVEL_BITS))) - 1))
#define PT64_LVL_OFFSET_MASK(level) \
	(PT64_BASE_ADDR_MASK & ((1ULL << (PAGE_SHIFT + (((level) - 1) \
						* PT64_LEVEL_BITS))) - 1))

#define PT32_BASE_ADDR_MASK PAGE_MASK
#define PT32_DIR_BASE_ADDR_MASK \
	(PAGE_MASK & ~((1ULL << (PAGE_SHIFT + PT32_LEVEL_BITS)) - 1))
#define PT32_LVL_ADDR_MASK(level) \
	(PAGE_MASK & ~((1ULL << (PAGE_SHIFT + (((level) - 1) \
					    * PT32_LEVEL_BITS))) - 1))

#define PT64_PERM_MASK (PT_PRESENT_MASK | PT_WRITABLE_MASK | PT_USER_MASK \
			| PT64_NX_MASK)

#define RMAP_EXT 4

#define ACC_EXEC_MASK    1
#define ACC_WRITE_MASK   PT_WRITABLE_MASK
#define ACC_USER_MASK    PT_USER_MASK
#define ACC_ALL          (ACC_EXEC_MASK | ACC_WRITE_MASK | ACC_USER_MASK)

#include <trace/events/kvm.h>

#define CREATE_TRACE_POINTS
#include "mmutrace.h"

#define SPTE_HOST_WRITEABLE (1ULL << PT_FIRST_AVAIL_BITS_SHIFT)

#define SHADOW_PT_INDEX(addr, level) PT64_INDEX(addr, level)

struct kvm_rmap_desc {
	u64 *sptes[RMAP_EXT];
	struct kvm_rmap_desc *more;
};

struct kvm_shadow_walk_iterator {
	u64 addr;
	hpa_t shadow_addr;
	int level;
	u64 *sptep;
	unsigned index;
};

#define for_each_shadow_entry(_vcpu, _addr, _walker)    \
	for (shadow_walk_init(&(_walker), _vcpu, _addr);	\
	     shadow_walk_okay(&(_walker));			\
	     shadow_walk_next(&(_walker)))

typedef void (*mmu_parent_walk_fn) (struct kvm_mmu_page *sp, u64 *spte);

static struct kmem_cache *pte_chain_cache;
static struct kmem_cache *rmap_desc_cache;
static struct kmem_cache *mmu_page_header_cache;

static u64 __read_mostly shadow_trap_nonpresent_pte;
static u64 __read_mostly shadow_notrap_nonpresent_pte;
static u64 __read_mostly shadow_base_present_pte;
static u64 __read_mostly shadow_nx_mask;
static u64 __read_mostly shadow_x_mask;	/* mutual exclusive with nx_mask */
static u64 __read_mostly shadow_user_mask;
static u64 __read_mostly shadow_accessed_mask;
static u64 __read_mostly shadow_dirty_mask;

static inline u64 rsvd_bits(int s, int e)
{
	return ((1ULL << (e - s + 1)) - 1) << s;
}

void kvm_mmu_set_nonpresent_ptes(u64 trap_pte, u64 notrap_pte)
{
	shadow_trap_nonpresent_pte = trap_pte;
	shadow_notrap_nonpresent_pte = notrap_pte;
}
EXPORT_SYMBOL_GPL(kvm_mmu_set_nonpresent_ptes);

void kvm_mmu_set_base_ptes(u64 base_pte)
{
	shadow_base_present_pte = base_pte;
}
EXPORT_SYMBOL_GPL(kvm_mmu_set_base_ptes);

void kvm_mmu_set_mask_ptes(u64 user_mask, u64 accessed_mask,
		u64 dirty_mask, u64 nx_mask, u64 x_mask)
{
	shadow_user_mask = user_mask;
	shadow_accessed_mask = accessed_mask;
	shadow_dirty_mask = dirty_mask;
	shadow_nx_mask = nx_mask;
	shadow_x_mask = x_mask;
}
EXPORT_SYMBOL_GPL(kvm_mmu_set_mask_ptes);

static bool is_write_protection(struct kvm_vcpu *vcpu)
{
	return kvm_read_cr0_bits(vcpu, X86_CR0_WP);
}

static int is_cpuid_PSE36(void)
{
	return 1;
}

static int is_nx(struct kvm_vcpu *vcpu)
{
	return vcpu->arch.efer & EFER_NX;
}

static int is_shadow_present_pte(u64 pte)
{
	return pte != shadow_trap_nonpresent_pte
		&& pte != shadow_notrap_nonpresent_pte;
}

static int is_large_pte(u64 pte)
{
	return pte & PT_PAGE_SIZE_MASK;
}

static int is_writable_pte(unsigned long pte)
{
	return pte & PT_WRITABLE_MASK;
}

static int is_dirty_gpte(unsigned long pte)
{
	return pte & PT_DIRTY_MASK;
}

static int is_rmap_spte(u64 pte)
{
	return is_shadow_present_pte(pte);
}

static int is_last_spte(u64 pte, int level)
{
	if (level == PT_PAGE_TABLE_LEVEL)
		return 1;
	if (is_large_pte(pte))
		return 1;
	return 0;
}

static pfn_t spte_to_pfn(u64 pte)
{
	return (pte & PT64_BASE_ADDR_MASK) >> PAGE_SHIFT;
}

static gfn_t pse36_gfn_delta(u32 gpte)
{
	int shift = 32 - PT32_DIR_PSE36_SHIFT - PAGE_SHIFT;

	return (gpte & PT32_DIR_PSE36_MASK) << shift;
}

static void __set_spte(u64 *sptep, u64 spte)
{
	set_64bit(sptep, spte);
}

static u64 __xchg_spte(u64 *sptep, u64 new_spte)
{
#ifdef CONFIG_X86_64
	return xchg(sptep, new_spte);
#else
	u64 old_spte;

	do {
		old_spte = *sptep;
	} while (cmpxchg64(sptep, old_spte, new_spte) != old_spte);

	return old_spte;
#endif
}

static void update_spte(u64 *sptep, u64 new_spte)
{
	u64 old_spte;

	if (!shadow_accessed_mask || (new_spte & shadow_accessed_mask) ||
	      !is_rmap_spte(*sptep))
		__set_spte(sptep, new_spte);
	else {
		old_spte = __xchg_spte(sptep, new_spte);
		if (old_spte & shadow_accessed_mask)
			mark_page_accessed(pfn_to_page(spte_to_pfn(old_spte)));
	}
}

static int mmu_topup_memory_cache(struct kvm_mmu_memory_cache *cache,
				  struct kmem_cache *base_cache, int min)
{
	void *obj;

	if (cache->nobjs >= min)
		return 0;
	while (cache->nobjs < ARRAY_SIZE(cache->objects)) {
		obj = kmem_cache_zalloc(base_cache, GFP_KERNEL);
		if (!obj)
			return -ENOMEM;
		cache->objects[cache->nobjs++] = obj;
	}
	return 0;
}

static void mmu_free_memory_cache(struct kvm_mmu_memory_cache *mc,
				  struct kmem_cache *cache)
{
	while (mc->nobjs)
		kmem_cache_free(cache, mc->objects[--mc->nobjs]);
}

static int mmu_topup_memory_cache_page(struct kvm_mmu_memory_cache *cache,
				       int min)
{
	struct page *page;

	if (cache->nobjs >= min)
		return 0;
	while (cache->nobjs < ARRAY_SIZE(cache->objects)) {
		page = alloc_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		cache->objects[cache->nobjs++] = page_address(page);
	}
	return 0;
}

static void mmu_free_memory_cache_page(struct kvm_mmu_memory_cache *mc)
{
	while (mc->nobjs)
		free_page((unsigned long)mc->objects[--mc->nobjs]);
}

static int mmu_topup_memory_caches(struct kvm_vcpu *vcpu)
{
	int r;

	r = mmu_topup_memory_cache(&vcpu->arch.mmu_pte_chain_cache,
				   pte_chain_cache, 4);
	if (r)
		goto out;
	r = mmu_topup_memory_cache(&vcpu->arch.mmu_rmap_desc_cache,
				   rmap_desc_cache, 4);
	if (r)
		goto out;
	r = mmu_topup_memory_cache_page(&vcpu->arch.mmu_page_cache, 8);
	if (r)
		goto out;
	r = mmu_topup_memory_cache(&vcpu->arch.mmu_page_header_cache,
				   mmu_page_header_cache, 4);
out:
	return r;
}

static void mmu_free_memory_caches(struct kvm_vcpu *vcpu)
{
	mmu_free_memory_cache(&vcpu->arch.mmu_pte_chain_cache, pte_chain_cache);
	mmu_free_memory_cache(&vcpu->arch.mmu_rmap_desc_cache, rmap_desc_cache);
	mmu_free_memory_cache_page(&vcpu->arch.mmu_page_cache);
	mmu_free_memory_cache(&vcpu->arch.mmu_page_header_cache,
				mmu_page_header_cache);
}

static void *mmu_memory_cache_alloc(struct kvm_mmu_memory_cache *mc,
				    size_t size)
{
	void *p;

	BUG_ON(!mc->nobjs);
	p = mc->objects[--mc->nobjs];
	return p;
}

static struct kvm_pte_chain *mmu_alloc_pte_chain(struct kvm_vcpu *vcpu)
{
	return mmu_memory_cache_alloc(&vcpu->arch.mmu_pte_chain_cache,
				      sizeof(struct kvm_pte_chain));
}

static void mmu_free_pte_chain(struct kvm_pte_chain *pc)
{
	kmem_cache_free(pte_chain_cache, pc);
}

static struct kvm_rmap_desc *mmu_alloc_rmap_desc(struct kvm_vcpu *vcpu)
{
	return mmu_memory_cache_alloc(&vcpu->arch.mmu_rmap_desc_cache,
				      sizeof(struct kvm_rmap_desc));
}

static void mmu_free_rmap_desc(struct kvm_rmap_desc *rd)
{
	kmem_cache_free(rmap_desc_cache, rd);
}

static gfn_t kvm_mmu_page_get_gfn(struct kvm_mmu_page *sp, int index)
{
	if (!sp->role.direct)
		return sp->gfns[index];

	return sp->gfn + (index << ((sp->role.level - 1) * PT64_LEVEL_BITS));
}

static void kvm_mmu_page_set_gfn(struct kvm_mmu_page *sp, int index, gfn_t gfn)
{
	if (sp->role.direct)
		BUG_ON(gfn != kvm_mmu_page_get_gfn(sp, index));
	else
		sp->gfns[index] = gfn;
}

/*
 * Return the pointer to the largepage write count for a given
 * gfn, handling slots that are not large page aligned.
 */
static int *slot_largepage_idx(gfn_t gfn,
			       struct kvm_memory_slot *slot,
			       int level)
{
	unsigned long idx;

	idx = (gfn >> KVM_HPAGE_GFN_SHIFT(level)) -
	      (slot->base_gfn >> KVM_HPAGE_GFN_SHIFT(level));
	return &slot->lpage_info[level - 2][idx].write_count;
}

static void account_shadowed(struct kvm *kvm, gfn_t gfn)
{
	struct kvm_memory_slot *slot;
	int *write_count;
	int i;

	slot = gfn_to_memslot(kvm, gfn);
	for (i = PT_DIRECTORY_LEVEL;
	     i < PT_PAGE_TABLE_LEVEL + KVM_NR_PAGE_SIZES; ++i) {
		write_count   = slot_largepage_idx(gfn, slot, i);
		*write_count += 1;
	}
}

static void unaccount_shadowed(struct kvm *kvm, gfn_t gfn)
{
	struct kvm_memory_slot *slot;
	int *write_count;
	int i;

	slot = gfn_to_memslot(kvm, gfn);
	for (i = PT_DIRECTORY_LEVEL;
	     i < PT_PAGE_TABLE_LEVEL + KVM_NR_PAGE_SIZES; ++i) {
		write_count   = slot_largepage_idx(gfn, slot, i);
		*write_count -= 1;
		WARN_ON(*write_count < 0);
	}
}

static int has_wrprotected_page(struct kvm *kvm,
				gfn_t gfn,
				int level)
{
	struct kvm_memory_slot *slot;
	int *largepage_idx;

	slot = gfn_to_memslot(kvm, gfn);
	if (slot) {
		largepage_idx = slot_largepage_idx(gfn, slot, level);
		return *largepage_idx;
	}

	return 1;
}

static int host_mapping_level(struct kvm *kvm, gfn_t gfn)
{
	unsigned long page_size;
	int i, ret = 0;

	page_size = kvm_host_page_size(kvm, gfn);

	for (i = PT_PAGE_TABLE_LEVEL;
	     i < (PT_PAGE_TABLE_LEVEL + KVM_NR_PAGE_SIZES); ++i) {
		if (page_size >= KVM_HPAGE_SIZE(i))
			ret = i;
		else
			break;
	}

	return ret;
}

static int mapping_level(struct kvm_vcpu *vcpu, gfn_t large_gfn)
{
	struct kvm_memory_slot *slot;
	int host_level, level, max_level;

	slot = gfn_to_memslot(vcpu->kvm, large_gfn);
	if (slot && slot->dirty_bitmap)
		return PT_PAGE_TABLE_LEVEL;

	host_level = host_mapping_level(vcpu->kvm, large_gfn);

	if (host_level == PT_PAGE_TABLE_LEVEL)
		return host_level;

	max_level = kvm_x86_ops->get_lpage_level() < host_level ?
		kvm_x86_ops->get_lpage_level() : host_level;

	for (level = PT_DIRECTORY_LEVEL; level <= max_level; ++level)
		if (has_wrprotected_page(vcpu->kvm, large_gfn, level))
			break;

	return level - 1;
}

/*
 * Take gfn and return the reverse mapping to it.
 */

static unsigned long *gfn_to_rmap(struct kvm *kvm, gfn_t gfn, int level)
{
	struct kvm_memory_slot *slot;
	unsigned long idx;

	slot = gfn_to_memslot(kvm, gfn);
	if (likely(level == PT_PAGE_TABLE_LEVEL))
		return &slot->rmap[gfn - slot->base_gfn];

	idx = (gfn >> KVM_HPAGE_GFN_SHIFT(level)) -
		(slot->base_gfn >> KVM_HPAGE_GFN_SHIFT(level));

	return &slot->lpage_info[level - 2][idx].rmap_pde;
}

/*
 * Reverse mapping data structures:
 *
 * If rmapp bit zero is zero, then rmapp point to the shadw page table entry
 * that points to page_address(page).
 *
 * If rmapp bit zero is one, (then rmap & ~1) points to a struct kvm_rmap_desc
 * containing more mappings.
 *
 * Returns the number of rmap entries before the spte was added or zero if
 * the spte was not added.
 *
 */
static int rmap_add(struct kvm_vcpu *vcpu, u64 *spte, gfn_t gfn)
{
	struct kvm_mmu_page *sp;
	struct kvm_rmap_desc *desc;
	unsigned long *rmapp;
	int i, count = 0;

	if (!is_rmap_spte(*spte))
		return count;
	sp = page_header(__pa(spte));
	kvm_mmu_page_set_gfn(sp, spte - sp->spt, gfn);
	rmapp = gfn_to_rmap(vcpu->kvm, gfn, sp->role.level);
	if (!*rmapp) {
		rmap_printk("rmap_add: %p %llx 0->1\n", spte, *spte);
		*rmapp = (unsigned long)spte;
	} else if (!(*rmapp & 1)) {
		rmap_printk("rmap_add: %p %llx 1->many\n", spte, *spte);
		desc = mmu_alloc_rmap_desc(vcpu);
		desc->sptes[0] = (u64 *)*rmapp;
		desc->sptes[1] = spte;
		*rmapp = (unsigned long)desc | 1;
	} else {
		rmap_printk("rmap_add: %p %llx many->many\n", spte, *spte);
		desc = (struct kvm_rmap_desc *)(*rmapp & ~1ul);
		while (desc->sptes[RMAP_EXT-1] && desc->more) {
			desc = desc->more;
			count += RMAP_EXT;
		}
		if (desc->sptes[RMAP_EXT-1]) {
			desc->more = mmu_alloc_rmap_desc(vcpu);
			desc = desc->more;
		}
		for (i = 0; desc->sptes[i]; ++i)
			;
		desc->sptes[i] = spte;
	}
	return count;
}

static void rmap_desc_remove_entry(unsigned long *rmapp,
				   struct kvm_rmap_desc *desc,
				   int i,
				   struct kvm_rmap_desc *prev_desc)
{
	int j;

	for (j = RMAP_EXT - 1; !desc->sptes[j] && j > i; --j)
		;
	desc->sptes[i] = desc->sptes[j];
	desc->sptes[j] = NULL;
	if (j != 0)
		return;
	if (!prev_desc && !desc->more)
		*rmapp = (unsigned long)desc->sptes[0];
	else
		if (prev_desc)
			prev_desc->more = desc->more;
		else
			*rmapp = (unsigned long)desc->more | 1;
	mmu_free_rmap_desc(desc);
}

static void rmap_remove(struct kvm *kvm, u64 *spte)
{
	struct kvm_rmap_desc *desc;
	struct kvm_rmap_desc *prev_desc;
	struct kvm_mmu_page *sp;
	gfn_t gfn;
	unsigned long *rmapp;
	int i;

	sp = page_header(__pa(spte));
	gfn = kvm_mmu_page_get_gfn(sp, spte - sp->spt);
	rmapp = gfn_to_rmap(kvm, gfn, sp->role.level);
	if (!*rmapp) {
		printk(KERN_ERR "rmap_remove: %p %llx 0->BUG\n", spte, *spte);
		BUG();
	} else if (!(*rmapp & 1)) {
		rmap_printk("rmap_remove:  %p %llx 1->0\n", spte, *spte);
		if ((u64 *)*rmapp != spte) {
			printk(KERN_ERR "rmap_remove:  %p %llx 1->BUG\n",
			       spte, *spte);
			BUG();
		}
		*rmapp = 0;
	} else {
		rmap_printk("rmap_remove:  %p %llx many->many\n", spte, *spte);
		desc = (struct kvm_rmap_desc *)(*rmapp & ~1ul);
		prev_desc = NULL;
		while (desc) {
			for (i = 0; i < RMAP_EXT && desc->sptes[i]; ++i)
				if (desc->sptes[i] == spte) {
					rmap_desc_remove_entry(rmapp,
							       desc, i,
							       prev_desc);
					return;
				}
			prev_desc = desc;
			desc = desc->more;
		}
		pr_err("rmap_remove: %p %llx many->many\n", spte, *spte);
		BUG();
	}
}

static void set_spte_track_bits(u64 *sptep, u64 new_spte)
{
	pfn_t pfn;
	u64 old_spte = *sptep;

	if (!shadow_accessed_mask || !is_shadow_present_pte(old_spte) ||
	      old_spte & shadow_accessed_mask) {
		__set_spte(sptep, new_spte);
	} else
		old_spte = __xchg_spte(sptep, new_spte);

	if (!is_rmap_spte(old_spte))
		return;
	pfn = spte_to_pfn(old_spte);
	if (!shadow_accessed_mask || old_spte & shadow_accessed_mask)
		kvm_set_pfn_accessed(pfn);
	if (is_writable_pte(old_spte))
		kvm_set_pfn_dirty(pfn);
}

static void drop_spte(struct kvm *kvm, u64 *sptep, u64 new_spte)
{
	set_spte_track_bits(sptep, new_spte);
	rmap_remove(kvm, sptep);
}

static u64 *rmap_next(struct kvm *kvm, unsigned long *rmapp, u64 *spte)
{
	struct kvm_rmap_desc *desc;
	u64 *prev_spte;
	int i;

	if (!*rmapp)
		return NULL;
	else if (!(*rmapp & 1)) {
		if (!spte)
			return (u64 *)*rmapp;
		return NULL;
	}
	desc = (struct kvm_rmap_desc *)(*rmapp & ~1ul);
	prev_spte = NULL;
	while (desc) {
		for (i = 0; i < RMAP_EXT && desc->sptes[i]; ++i) {
			if (prev_spte == spte)
				return desc->sptes[i];
			prev_spte = desc->sptes[i];
		}
		desc = desc->more;
	}
	return NULL;
}

static int rmap_write_protect(struct kvm *kvm, u64 gfn)
{
	unsigned long *rmapp;
	u64 *spte;
	int i, write_protected = 0;

	rmapp = gfn_to_rmap(kvm, gfn, PT_PAGE_TABLE_LEVEL);

	spte = rmap_next(kvm, rmapp, NULL);
	while (spte) {
		BUG_ON(!spte);
		BUG_ON(!(*spte & PT_PRESENT_MASK));
		rmap_printk("rmap_write_protect: spte %p %llx\n", spte, *spte);
		if (is_writable_pte(*spte)) {
			update_spte(spte, *spte & ~PT_WRITABLE_MASK);
			write_protected = 1;
		}
		spte = rmap_next(kvm, rmapp, spte);
	}
	if (write_protected) {
		pfn_t pfn;

		spte = rmap_next(kvm, rmapp, NULL);
		pfn = spte_to_pfn(*spte);
		kvm_set_pfn_dirty(pfn);
	}

	/* check for huge page mappings */
	for (i = PT_DIRECTORY_LEVEL;
	     i < PT_PAGE_TABLE_LEVEL + KVM_NR_PAGE_SIZES; ++i) {
		rmapp = gfn_to_rmap(kvm, gfn, i);
		spte = rmap_next(kvm, rmapp, NULL);
		while (spte) {
			BUG_ON(!spte);
			BUG_ON(!(*spte & PT_PRESENT_MASK));
			BUG_ON((*spte & (PT_PAGE_SIZE_MASK|PT_PRESENT_MASK)) != (PT_PAGE_SIZE_MASK|PT_PRESENT_MASK));
			pgprintk("rmap_write_protect(large): spte %p %llx %lld\n", spte, *spte, gfn);
			if (is_writable_pte(*spte)) {
				drop_spte(kvm, spte,
					  shadow_trap_nonpresent_pte);
				--kvm->stat.lpages;
				spte = NULL;
				write_protected = 1;
			}
			spte = rmap_next(kvm, rmapp, spte);
		}
	}

	return write_protected;
}

static int kvm_unmap_rmapp(struct kvm *kvm, unsigned long *rmapp,
			   unsigned long data)
{
	u64 *spte;
	int need_tlb_flush = 0;

	while ((spte = rmap_next(kvm, rmapp, NULL))) {
		BUG_ON(!(*spte & PT_PRESENT_MASK));
		rmap_printk("kvm_rmap_unmap_hva: spte %p %llx\n", spte, *spte);
		drop_spte(kvm, spte, shadow_trap_nonpresent_pte);
		need_tlb_flush = 1;
	}
	return need_tlb_flush;
}

static int kvm_set_pte_rmapp(struct kvm *kvm, unsigned long *rmapp,
			     unsigned long data)
{
	int need_flush = 0;
	u64 *spte, new_spte;
	pte_t *ptep = (pte_t *)data;
	pfn_t new_pfn;

	WARN_ON(pte_huge(*ptep));
	new_pfn = pte_pfn(*ptep);
	spte = rmap_next(kvm, rmapp, NULL);
	while (spte) {
		BUG_ON(!is_shadow_present_pte(*spte));
		rmap_printk("kvm_set_pte_rmapp: spte %p %llx\n", spte, *spte);
		need_flush = 1;
		if (pte_write(*ptep)) {
			drop_spte(kvm, spte, shadow_trap_nonpresent_pte);
			spte = rmap_next(kvm, rmapp, NULL);
		} else {
			new_spte = *spte &~ (PT64_BASE_ADDR_MASK);
			new_spte |= (u64)new_pfn << PAGE_SHIFT;

			new_spte &= ~PT_WRITABLE_MASK;
			new_spte &= ~SPTE_HOST_WRITEABLE;
			new_spte &= ~shadow_accessed_mask;
			set_spte_track_bits(spte, new_spte);
			spte = rmap_next(kvm, rmapp, spte);
		}
	}
	if (need_flush)
		kvm_flush_remote_tlbs(kvm);

	return 0;
}

static int kvm_handle_hva(struct kvm *kvm, unsigned long hva,
			  unsigned long data,
			  int (*handler)(struct kvm *kvm, unsigned long *rmapp,
					 unsigned long data))
{
	int i, j;
	int ret;
	int retval = 0;
	struct kvm_memslots *slots;

	slots = kvm_memslots(kvm);

	for (i = 0; i < slots->nmemslots; i++) {
		struct kvm_memory_slot *memslot = &slots->memslots[i];
		unsigned long start = memslot->userspace_addr;
		unsigned long end;

		end = start + (memslot->npages << PAGE_SHIFT);
		if (hva >= start && hva < end) {
			gfn_t gfn_offset = (hva - start) >> PAGE_SHIFT;

			ret = handler(kvm, &memslot->rmap[gfn_offset], data);

			for (j = 0; j < KVM_NR_PAGE_SIZES - 1; ++j) {
				unsigned long idx;
				int sh;

				sh = KVM_HPAGE_GFN_SHIFT(PT_DIRECTORY_LEVEL+j);
				idx = ((memslot->base_gfn+gfn_offset) >> sh) -
					(memslot->base_gfn >> sh);
				ret |= handler(kvm,
					&memslot->lpage_info[j][idx].rmap_pde,
					data);
			}
			trace_kvm_age_page(hva, memslot, ret);
			retval |= ret;
		}
	}

	return retval;
}

int kvm_unmap_hva(struct kvm *kvm, unsigned long hva)
{
	return kvm_handle_hva(kvm, hva, 0, kvm_unmap_rmapp);
}

void kvm_set_spte_hva(struct kvm *kvm, unsigned long hva, pte_t pte)
{
	kvm_handle_hva(kvm, hva, (unsigned long)&pte, kvm_set_pte_rmapp);
}

static int kvm_age_rmapp(struct kvm *kvm, unsigned long *rmapp,
			 unsigned long data)
{
	u64 *spte;
	int young = 0;

	/*
	 * Emulate the accessed bit for EPT, by checking if this page has
	 * an EPT mapping, and clearing it if it does. On the next access,
	 * a new EPT mapping will be established.
	 * This has some overhead, but not as much as the cost of swapping
	 * out actively used pages or breaking up actively used hugepages.
	 */
	if (!shadow_accessed_mask)
		return kvm_unmap_rmapp(kvm, rmapp, data);

	spte = rmap_next(kvm, rmapp, NULL);
	while (spte) {
		int _young;
		u64 _spte = *spte;
		BUG_ON(!(_spte & PT_PRESENT_MASK));
		_young = _spte & PT_ACCESSED_MASK;
		if (_young) {
			young = 1;
			clear_bit(PT_ACCESSED_SHIFT, (unsigned long *)spte);
		}
		spte = rmap_next(kvm, rmapp, spte);
	}
	return young;
}

#define RMAP_RECYCLE_THRESHOLD 1000

static void rmap_recycle(struct kvm_vcpu *vcpu, u64 *spte, gfn_t gfn)
{
	unsigned long *rmapp;
	struct kvm_mmu_page *sp;

	sp = page_header(__pa(spte));

	rmapp = gfn_to_rmap(vcpu->kvm, gfn, sp->role.level);

	kvm_unmap_rmapp(vcpu->kvm, rmapp, 0);
	kvm_flush_remote_tlbs(vcpu->kvm);
}

int kvm_age_hva(struct kvm *kvm, unsigned long hva)
{
	return kvm_handle_hva(kvm, hva, 0, kvm_age_rmapp);
}

#ifdef MMU_DEBUG
static int is_empty_shadow_page(u64 *spt)
{
	u64 *pos;
	u64 *end;

	for (pos = spt, end = pos + PAGE_SIZE / sizeof(u64); pos != end; pos++)
		if (is_shadow_present_pte(*pos)) {
			printk(KERN_ERR "%s: %p %llx\n", __func__,
			       pos, *pos);
			return 0;
		}
	return 1;
}
#endif

static void kvm_mmu_free_page(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	ASSERT(is_empty_shadow_page(sp->spt));
	hlist_del(&sp->hash_link);
	list_del(&sp->link);
	__free_page(virt_to_page(sp->spt));
	if (!sp->role.direct)
		__free_page(virt_to_page(sp->gfns));
	kmem_cache_free(mmu_page_header_cache, sp);
	++kvm->arch.n_free_mmu_pages;
}

static unsigned kvm_page_table_hashfn(gfn_t gfn)
{
	return gfn & ((1 << KVM_MMU_HASH_SHIFT) - 1);
}

static struct kvm_mmu_page *kvm_mmu_alloc_page(struct kvm_vcpu *vcpu,
					       u64 *parent_pte, int direct)
{
	struct kvm_mmu_page *sp;

	sp = mmu_memory_cache_alloc(&vcpu->arch.mmu_page_header_cache, sizeof *sp);
	sp->spt = mmu_memory_cache_alloc(&vcpu->arch.mmu_page_cache, PAGE_SIZE);
	if (!direct)
		sp->gfns = mmu_memory_cache_alloc(&vcpu->arch.mmu_page_cache,
						  PAGE_SIZE);
	set_page_private(virt_to_page(sp->spt), (unsigned long)sp);
	list_add(&sp->link, &vcpu->kvm->arch.active_mmu_pages);
	bitmap_zero(sp->slot_bitmap, KVM_MEMORY_SLOTS + KVM_PRIVATE_MEM_SLOTS);
	sp->multimapped = 0;
	sp->parent_pte = parent_pte;
	--vcpu->kvm->arch.n_free_mmu_pages;
	return sp;
}

static void mmu_page_add_parent_pte(struct kvm_vcpu *vcpu,
				    struct kvm_mmu_page *sp, u64 *parent_pte)
{
	struct kvm_pte_chain *pte_chain;
	struct hlist_node *node;
	int i;

	if (!parent_pte)
		return;
	if (!sp->multimapped) {
		u64 *old = sp->parent_pte;

		if (!old) {
			sp->parent_pte = parent_pte;
			return;
		}
		sp->multimapped = 1;
		pte_chain = mmu_alloc_pte_chain(vcpu);
		INIT_HLIST_HEAD(&sp->parent_ptes);
		hlist_add_head(&pte_chain->link, &sp->parent_ptes);
		pte_chain->parent_ptes[0] = old;
	}
	hlist_for_each_entry(pte_chain, node, &sp->parent_ptes, link) {
		if (pte_chain->parent_ptes[NR_PTE_CHAIN_ENTRIES-1])
			continue;
		for (i = 0; i < NR_PTE_CHAIN_ENTRIES; ++i)
			if (!pte_chain->parent_ptes[i]) {
				pte_chain->parent_ptes[i] = parent_pte;
				return;
			}
	}
	pte_chain = mmu_alloc_pte_chain(vcpu);
	BUG_ON(!pte_chain);
	hlist_add_head(&pte_chain->link, &sp->parent_ptes);
	pte_chain->parent_ptes[0] = parent_pte;
}

static void mmu_page_remove_parent_pte(struct kvm_mmu_page *sp,
				       u64 *parent_pte)
{
	struct kvm_pte_chain *pte_chain;
	struct hlist_node *node;
	int i;

	if (!sp->multimapped) {
		BUG_ON(sp->parent_pte != parent_pte);
		sp->parent_pte = NULL;
		return;
	}
	hlist_for_each_entry(pte_chain, node, &sp->parent_ptes, link)
		for (i = 0; i < NR_PTE_CHAIN_ENTRIES; ++i) {
			if (!pte_chain->parent_ptes[i])
				break;
			if (pte_chain->parent_ptes[i] != parent_pte)
				continue;
			while (i + 1 < NR_PTE_CHAIN_ENTRIES
				&& pte_chain->parent_ptes[i + 1]) {
				pte_chain->parent_ptes[i]
					= pte_chain->parent_ptes[i + 1];
				++i;
			}
			pte_chain->parent_ptes[i] = NULL;
			if (i == 0) {
				hlist_del(&pte_chain->link);
				mmu_free_pte_chain(pte_chain);
				if (hlist_empty(&sp->parent_ptes)) {
					sp->multimapped = 0;
					sp->parent_pte = NULL;
				}
			}
			return;
		}
	BUG();
}

static void mmu_parent_walk(struct kvm_mmu_page *sp, mmu_parent_walk_fn fn)
{
	struct kvm_pte_chain *pte_chain;
	struct hlist_node *node;
	struct kvm_mmu_page *parent_sp;
	int i;

	if (!sp->multimapped && sp->parent_pte) {
		parent_sp = page_header(__pa(sp->parent_pte));
		fn(parent_sp, sp->parent_pte);
		return;
	}

	hlist_for_each_entry(pte_chain, node, &sp->parent_ptes, link)
		for (i = 0; i < NR_PTE_CHAIN_ENTRIES; ++i) {
			u64 *spte = pte_chain->parent_ptes[i];

			if (!spte)
				break;
			parent_sp = page_header(__pa(spte));
			fn(parent_sp, spte);
		}
}

static void mark_unsync(struct kvm_mmu_page *sp, u64 *spte);
static void kvm_mmu_mark_parents_unsync(struct kvm_mmu_page *sp)
{
	mmu_parent_walk(sp, mark_unsync);
}

static void mark_unsync(struct kvm_mmu_page *sp, u64 *spte)
{
	unsigned int index;

	index = spte - sp->spt;
	if (__test_and_set_bit(index, sp->unsync_child_bitmap))
		return;
	if (sp->unsync_children++)
		return;
	kvm_mmu_mark_parents_unsync(sp);
}

static void nonpaging_prefetch_page(struct kvm_vcpu *vcpu,
				    struct kvm_mmu_page *sp)
{
	int i;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i)
		sp->spt[i] = shadow_trap_nonpresent_pte;
}

static int nonpaging_sync_page(struct kvm_vcpu *vcpu,
			       struct kvm_mmu_page *sp, bool clear_unsync)
{
	return 1;
}

static void nonpaging_invlpg(struct kvm_vcpu *vcpu, gva_t gva)
{
}

#define KVM_PAGE_ARRAY_NR 16

struct kvm_mmu_pages {
	struct mmu_page_and_offset {
		struct kvm_mmu_page *sp;
		unsigned int idx;
	} page[KVM_PAGE_ARRAY_NR];
	unsigned int nr;
};

#define for_each_unsync_children(bitmap, idx)		\
	for (idx = find_first_bit(bitmap, 512);		\
	     idx < 512;					\
	     idx = find_next_bit(bitmap, 512, idx+1))

static int mmu_pages_add(struct kvm_mmu_pages *pvec, struct kvm_mmu_page *sp,
			 int idx)
{
	int i;

	if (sp->unsync)
		for (i=0; i < pvec->nr; i++)
			if (pvec->page[i].sp == sp)
				return 0;

	pvec->page[pvec->nr].sp = sp;
	pvec->page[pvec->nr].idx = idx;
	pvec->nr++;
	return (pvec->nr == KVM_PAGE_ARRAY_NR);
}

static int __mmu_unsync_walk(struct kvm_mmu_page *sp,
			   struct kvm_mmu_pages *pvec)
{
	int i, ret, nr_unsync_leaf = 0;

	for_each_unsync_children(sp->unsync_child_bitmap, i) {
		struct kvm_mmu_page *child;
		u64 ent = sp->spt[i];

		if (!is_shadow_present_pte(ent) || is_large_pte(ent))
			goto clear_child_bitmap;

		child = page_header(ent & PT64_BASE_ADDR_MASK);

		if (child->unsync_children) {
			if (mmu_pages_add(pvec, child, i))
				return -ENOSPC;

			ret = __mmu_unsync_walk(child, pvec);
			if (!ret)
				goto clear_child_bitmap;
			else if (ret > 0)
				nr_unsync_leaf += ret;
			else
				return ret;
		} else if (child->unsync) {
			nr_unsync_leaf++;
			if (mmu_pages_add(pvec, child, i))
				return -ENOSPC;
		} else
			 goto clear_child_bitmap;

		continue;

clear_child_bitmap:
		__clear_bit(i, sp->unsync_child_bitmap);
		sp->unsync_children--;
		WARN_ON((int)sp->unsync_children < 0);
	}


	return nr_unsync_leaf;
}

static int mmu_unsync_walk(struct kvm_mmu_page *sp,
			   struct kvm_mmu_pages *pvec)
{
	if (!sp->unsync_children)
		return 0;

	mmu_pages_add(pvec, sp, 0);
	return __mmu_unsync_walk(sp, pvec);
}

static void kvm_unlink_unsync_page(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	WARN_ON(!sp->unsync);
	trace_kvm_mmu_sync_page(sp);
	sp->unsync = 0;
	--kvm->stat.mmu_unsync;
}

static int kvm_mmu_prepare_zap_page(struct kvm *kvm, struct kvm_mmu_page *sp,
				    struct list_head *invalid_list);
static void kvm_mmu_commit_zap_page(struct kvm *kvm,
				    struct list_head *invalid_list);

#define for_each_gfn_sp(kvm, sp, gfn, pos)				\
  hlist_for_each_entry(sp, pos,						\
   &(kvm)->arch.mmu_page_hash[kvm_page_table_hashfn(gfn)], hash_link)	\
	if ((sp)->gfn != (gfn)) {} else

#define for_each_gfn_indirect_valid_sp(kvm, sp, gfn, pos)		\
  hlist_for_each_entry(sp, pos,						\
   &(kvm)->arch.mmu_page_hash[kvm_page_table_hashfn(gfn)], hash_link)	\
		if ((sp)->gfn != (gfn) || (sp)->role.direct ||		\
			(sp)->role.invalid) {} else

/* @sp->gfn should be write-protected at the call site */
static int __kvm_sync_page(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp,
			   struct list_head *invalid_list, bool clear_unsync)
{
	if (sp->role.cr4_pae != !!is_pae(vcpu)) {
		kvm_mmu_prepare_zap_page(vcpu->kvm, sp, invalid_list);
		return 1;
	}

	if (clear_unsync)
		kvm_unlink_unsync_page(vcpu->kvm, sp);

	if (vcpu->arch.mmu.sync_page(vcpu, sp, clear_unsync)) {
		kvm_mmu_prepare_zap_page(vcpu->kvm, sp, invalid_list);
		return 1;
	}

	kvm_mmu_flush_tlb(vcpu);
	return 0;
}

static int kvm_sync_page_transient(struct kvm_vcpu *vcpu,
				   struct kvm_mmu_page *sp)
{
	LIST_HEAD(invalid_list);
	int ret;

	ret = __kvm_sync_page(vcpu, sp, &invalid_list, false);
	if (ret)
		kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);

	return ret;
}

static int kvm_sync_page(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp,
			 struct list_head *invalid_list)
{
	return __kvm_sync_page(vcpu, sp, invalid_list, true);
}

/* @gfn should be write-protected at the call site */
static void kvm_sync_pages(struct kvm_vcpu *vcpu,  gfn_t gfn)
{
	struct kvm_mmu_page *s;
	struct hlist_node *node;
	LIST_HEAD(invalid_list);
	bool flush = false;

	for_each_gfn_indirect_valid_sp(vcpu->kvm, s, gfn, node) {
		if (!s->unsync)
			continue;

		WARN_ON(s->role.level != PT_PAGE_TABLE_LEVEL);
		if ((s->role.cr4_pae != !!is_pae(vcpu)) ||
			(vcpu->arch.mmu.sync_page(vcpu, s, true))) {
			kvm_mmu_prepare_zap_page(vcpu->kvm, s, &invalid_list);
			continue;
		}
		kvm_unlink_unsync_page(vcpu->kvm, s);
		flush = true;
	}

	kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
	if (flush)
		kvm_mmu_flush_tlb(vcpu);
}

struct mmu_page_path {
	struct kvm_mmu_page *parent[PT64_ROOT_LEVEL-1];
	unsigned int idx[PT64_ROOT_LEVEL-1];
};

#define for_each_sp(pvec, sp, parents, i)			\
		for (i = mmu_pages_next(&pvec, &parents, -1),	\
			sp = pvec.page[i].sp;			\
			i < pvec.nr && ({ sp = pvec.page[i].sp; 1;});	\
			i = mmu_pages_next(&pvec, &parents, i))

static int mmu_pages_next(struct kvm_mmu_pages *pvec,
			  struct mmu_page_path *parents,
			  int i)
{
	int n;

	for (n = i+1; n < pvec->nr; n++) {
		struct kvm_mmu_page *sp = pvec->page[n].sp;

		if (sp->role.level == PT_PAGE_TABLE_LEVEL) {
			parents->idx[0] = pvec->page[n].idx;
			return n;
		}

		parents->parent[sp->role.level-2] = sp;
		parents->idx[sp->role.level-1] = pvec->page[n].idx;
	}

	return n;
}

static void mmu_pages_clear_parents(struct mmu_page_path *parents)
{
	struct kvm_mmu_page *sp;
	unsigned int level = 0;

	do {
		unsigned int idx = parents->idx[level];

		sp = parents->parent[level];
		if (!sp)
			return;

		--sp->unsync_children;
		WARN_ON((int)sp->unsync_children < 0);
		__clear_bit(idx, sp->unsync_child_bitmap);
		level++;
	} while (level < PT64_ROOT_LEVEL-1 && !sp->unsync_children);
}

static void kvm_mmu_pages_init(struct kvm_mmu_page *parent,
			       struct mmu_page_path *parents,
			       struct kvm_mmu_pages *pvec)
{
	parents->parent[parent->role.level-1] = NULL;
	pvec->nr = 0;
}

static void mmu_sync_children(struct kvm_vcpu *vcpu,
			      struct kvm_mmu_page *parent)
{
	int i;
	struct kvm_mmu_page *sp;
	struct mmu_page_path parents;
	struct kvm_mmu_pages pages;
	LIST_HEAD(invalid_list);

	kvm_mmu_pages_init(parent, &parents, &pages);
	while (mmu_unsync_walk(parent, &pages)) {
		int protected = 0;

		for_each_sp(pages, sp, parents, i)
			protected |= rmap_write_protect(vcpu->kvm, sp->gfn);

		if (protected)
			kvm_flush_remote_tlbs(vcpu->kvm);

		for_each_sp(pages, sp, parents, i) {
			kvm_sync_page(vcpu, sp, &invalid_list);
			mmu_pages_clear_parents(&parents);
		}
		kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
		cond_resched_lock(&vcpu->kvm->mmu_lock);
		kvm_mmu_pages_init(parent, &parents, &pages);
	}
}

static struct kvm_mmu_page *kvm_mmu_get_page(struct kvm_vcpu *vcpu,
					     gfn_t gfn,
					     gva_t gaddr,
					     unsigned level,
					     int direct,
					     unsigned access,
					     u64 *parent_pte)
{
	union kvm_mmu_page_role role;
	unsigned quadrant;
	struct kvm_mmu_page *sp;
	struct hlist_node *node;
	bool need_sync = false;

	role = vcpu->arch.mmu.base_role;
	role.level = level;
	role.direct = direct;
	if (role.direct)
		role.cr4_pae = 0;
	role.access = access;
	if (!tdp_enabled && vcpu->arch.mmu.root_level <= PT32_ROOT_LEVEL) {
		quadrant = gaddr >> (PAGE_SHIFT + (PT64_PT_BITS * level));
		quadrant &= (1 << ((PT32_PT_BITS - PT64_PT_BITS) * level)) - 1;
		role.quadrant = quadrant;
	}
	for_each_gfn_sp(vcpu->kvm, sp, gfn, node) {
		if (!need_sync && sp->unsync)
			need_sync = true;

		if (sp->role.word != role.word)
			continue;

		if (sp->unsync && kvm_sync_page_transient(vcpu, sp))
			break;

		mmu_page_add_parent_pte(vcpu, sp, parent_pte);
		if (sp->unsync_children) {
			kvm_make_request(KVM_REQ_MMU_SYNC, vcpu);
			kvm_mmu_mark_parents_unsync(sp);
		} else if (sp->unsync)
			kvm_mmu_mark_parents_unsync(sp);

		trace_kvm_mmu_get_page(sp, false);
		return sp;
	}
	++vcpu->kvm->stat.mmu_cache_miss;
	sp = kvm_mmu_alloc_page(vcpu, parent_pte, direct);
	if (!sp)
		return sp;
	sp->gfn = gfn;
	sp->role = role;
	hlist_add_head(&sp->hash_link,
		&vcpu->kvm->arch.mmu_page_hash[kvm_page_table_hashfn(gfn)]);
	if (!direct) {
		if (rmap_write_protect(vcpu->kvm, gfn))
			kvm_flush_remote_tlbs(vcpu->kvm);
		if (level > PT_PAGE_TABLE_LEVEL && need_sync)
			kvm_sync_pages(vcpu, gfn);

		account_shadowed(vcpu->kvm, gfn);
	}
	if (shadow_trap_nonpresent_pte != shadow_notrap_nonpresent_pte)
		vcpu->arch.mmu.prefetch_page(vcpu, sp);
	else
		nonpaging_prefetch_page(vcpu, sp);
	trace_kvm_mmu_get_page(sp, true);
	return sp;
}

static void shadow_walk_init(struct kvm_shadow_walk_iterator *iterator,
			     struct kvm_vcpu *vcpu, u64 addr)
{
	iterator->addr = addr;
	iterator->shadow_addr = vcpu->arch.mmu.root_hpa;
	iterator->level = vcpu->arch.mmu.shadow_root_level;
	if (iterator->level == PT32E_ROOT_LEVEL) {
		iterator->shadow_addr
			= vcpu->arch.mmu.pae_root[(addr >> 30) & 3];
		iterator->shadow_addr &= PT64_BASE_ADDR_MASK;
		--iterator->level;
		if (!iterator->shadow_addr)
			iterator->level = 0;
	}
}

static bool shadow_walk_okay(struct kvm_shadow_walk_iterator *iterator)
{
	if (iterator->level < PT_PAGE_TABLE_LEVEL)
		return false;

	if (iterator->level == PT_PAGE_TABLE_LEVEL)
		if (is_large_pte(*iterator->sptep))
			return false;

	iterator->index = SHADOW_PT_INDEX(iterator->addr, iterator->level);
	iterator->sptep	= ((u64 *)__va(iterator->shadow_addr)) + iterator->index;
	return true;
}

static void shadow_walk_next(struct kvm_shadow_walk_iterator *iterator)
{
	iterator->shadow_addr = *iterator->sptep & PT64_BASE_ADDR_MASK;
	--iterator->level;
}

static void link_shadow_page(u64 *sptep, struct kvm_mmu_page *sp)
{
	u64 spte;

	spte = __pa(sp->spt)
		| PT_PRESENT_MASK | PT_ACCESSED_MASK
		| PT_WRITABLE_MASK | PT_USER_MASK;
	__set_spte(sptep, spte);
}

static void drop_large_spte(struct kvm_vcpu *vcpu, u64 *sptep)
{
	if (is_large_pte(*sptep)) {
		drop_spte(vcpu->kvm, sptep, shadow_trap_nonpresent_pte);
		kvm_flush_remote_tlbs(vcpu->kvm);
	}
}

static void validate_direct_spte(struct kvm_vcpu *vcpu, u64 *sptep,
				   unsigned direct_access)
{
	if (is_shadow_present_pte(*sptep) && !is_large_pte(*sptep)) {
		struct kvm_mmu_page *child;

		/*
		 * For the direct sp, if the guest pte's dirty bit
		 * changed form clean to dirty, it will corrupt the
		 * sp's access: allow writable in the read-only sp,
		 * so we should update the spte at this point to get
		 * a new sp with the correct access.
		 */
		child = page_header(*sptep & PT64_BASE_ADDR_MASK);
		if (child->role.access == direct_access)
			return;

		mmu_page_remove_parent_pte(child, sptep);
		__set_spte(sptep, shadow_trap_nonpresent_pte);
		kvm_flush_remote_tlbs(vcpu->kvm);
	}
}

static void kvm_mmu_page_unlink_children(struct kvm *kvm,
					 struct kvm_mmu_page *sp)
{
	unsigned i;
	u64 *pt;
	u64 ent;

	pt = sp->spt;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
		ent = pt[i];

		if (is_shadow_present_pte(ent)) {
			if (!is_last_spte(ent, sp->role.level)) {
				ent &= PT64_BASE_ADDR_MASK;
				mmu_page_remove_parent_pte(page_header(ent),
							   &pt[i]);
			} else {
				if (is_large_pte(ent))
					--kvm->stat.lpages;
				drop_spte(kvm, &pt[i],
					  shadow_trap_nonpresent_pte);
			}
		}
		pt[i] = shadow_trap_nonpresent_pte;
	}
}

static void kvm_mmu_put_page(struct kvm_mmu_page *sp, u64 *parent_pte)
{
	mmu_page_remove_parent_pte(sp, parent_pte);
}

static void kvm_mmu_reset_last_pte_updated(struct kvm *kvm)
{
	int i;
	struct kvm_vcpu *vcpu;

	kvm_for_each_vcpu(i, vcpu, kvm)
		vcpu->arch.last_pte_updated = NULL;
}

static void kvm_mmu_unlink_parents(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	u64 *parent_pte;

	while (sp->multimapped || sp->parent_pte) {
		if (!sp->multimapped)
			parent_pte = sp->parent_pte;
		else {
			struct kvm_pte_chain *chain;

			chain = container_of(sp->parent_ptes.first,
					     struct kvm_pte_chain, link);
			parent_pte = chain->parent_ptes[0];
		}
		BUG_ON(!parent_pte);
		kvm_mmu_put_page(sp, parent_pte);
		__set_spte(parent_pte, shadow_trap_nonpresent_pte);
	}
}

static int mmu_zap_unsync_children(struct kvm *kvm,
				   struct kvm_mmu_page *parent,
				   struct list_head *invalid_list)
{
	int i, zapped = 0;
	struct mmu_page_path parents;
	struct kvm_mmu_pages pages;

	if (parent->role.level == PT_PAGE_TABLE_LEVEL)
		return 0;

	kvm_mmu_pages_init(parent, &parents, &pages);
	while (mmu_unsync_walk(parent, &pages)) {
		struct kvm_mmu_page *sp;

		for_each_sp(pages, sp, parents, i) {
			kvm_mmu_prepare_zap_page(kvm, sp, invalid_list);
			mmu_pages_clear_parents(&parents);
			zapped++;
		}
		kvm_mmu_pages_init(parent, &parents, &pages);
	}

	return zapped;
}

static int kvm_mmu_prepare_zap_page(struct kvm *kvm, struct kvm_mmu_page *sp,
				    struct list_head *invalid_list)
{
	int ret;

	trace_kvm_mmu_prepare_zap_page(sp);
	++kvm->stat.mmu_shadow_zapped;
	ret = mmu_zap_unsync_children(kvm, sp, invalid_list);
	kvm_mmu_page_unlink_children(kvm, sp);
	kvm_mmu_unlink_parents(kvm, sp);
	if (!sp->role.invalid && !sp->role.direct)
		unaccount_shadowed(kvm, sp->gfn);
	if (sp->unsync)
		kvm_unlink_unsync_page(kvm, sp);
	if (!sp->root_count) {
		/* Count self */
		ret++;
		list_move(&sp->link, invalid_list);
	} else {
		list_move(&sp->link, &kvm->arch.active_mmu_pages);
		kvm_reload_remote_mmus(kvm);
	}

	sp->role.invalid = 1;
	kvm_mmu_reset_last_pte_updated(kvm);
	return ret;
}

static void kvm_mmu_commit_zap_page(struct kvm *kvm,
				    struct list_head *invalid_list)
{
	struct kvm_mmu_page *sp;

	if (list_empty(invalid_list))
		return;

	kvm_flush_remote_tlbs(kvm);

	do {
		sp = list_first_entry(invalid_list, struct kvm_mmu_page, link);
		WARN_ON(!sp->role.invalid || sp->root_count);
		kvm_mmu_free_page(kvm, sp);
	} while (!list_empty(invalid_list));

}

/*
 * Changing the number of mmu pages allocated to the vm
 * Note: if kvm_nr_mmu_pages is too small, you will get dead lock
 */
void kvm_mmu_change_mmu_pages(struct kvm *kvm, unsigned int kvm_nr_mmu_pages)
{
	int used_pages;
	LIST_HEAD(invalid_list);

	used_pages = kvm->arch.n_alloc_mmu_pages - kvm->arch.n_free_mmu_pages;
	used_pages = max(0, used_pages);

	/*
	 * If we set the number of mmu pages to be smaller be than the
	 * number of actived pages , we must to free some mmu pages before we
	 * change the value
	 */

	if (used_pages > kvm_nr_mmu_pages) {
		while (used_pages > kvm_nr_mmu_pages &&
			!list_empty(&kvm->arch.active_mmu_pages)) {
			struct kvm_mmu_page *page;

			page = container_of(kvm->arch.active_mmu_pages.prev,
					    struct kvm_mmu_page, link);
			used_pages -= kvm_mmu_prepare_zap_page(kvm, page,
							       &invalid_list);
		}
		kvm_mmu_commit_zap_page(kvm, &invalid_list);
		kvm_nr_mmu_pages = used_pages;
		kvm->arch.n_free_mmu_pages = 0;
	}
	else
		kvm->arch.n_free_mmu_pages += kvm_nr_mmu_pages
					 - kvm->arch.n_alloc_mmu_pages;

	kvm->arch.n_alloc_mmu_pages = kvm_nr_mmu_pages;
}

static int kvm_mmu_unprotect_page(struct kvm *kvm, gfn_t gfn)
{
	struct kvm_mmu_page *sp;
	struct hlist_node *node;
	LIST_HEAD(invalid_list);
	int r;

	pgprintk("%s: looking for gfn %lx\n", __func__, gfn);
	r = 0;

	for_each_gfn_indirect_valid_sp(kvm, sp, gfn, node) {
		pgprintk("%s: gfn %lx role %x\n", __func__, gfn,
			 sp->role.word);
		r = 1;
		kvm_mmu_prepare_zap_page(kvm, sp, &invalid_list);
	}
	kvm_mmu_commit_zap_page(kvm, &invalid_list);
	return r;
}

static void mmu_unshadow(struct kvm *kvm, gfn_t gfn)
{
	struct kvm_mmu_page *sp;
	struct hlist_node *node;
	LIST_HEAD(invalid_list);

	for_each_gfn_indirect_valid_sp(kvm, sp, gfn, node) {
		pgprintk("%s: zap %lx %x\n",
			 __func__, gfn, sp->role.word);
		kvm_mmu_prepare_zap_page(kvm, sp, &invalid_list);
	}
	kvm_mmu_commit_zap_page(kvm, &invalid_list);
}

static void page_header_update_slot(struct kvm *kvm, void *pte, gfn_t gfn)
{
	int slot = memslot_id(kvm, gfn);
	struct kvm_mmu_page *sp = page_header(__pa(pte));

	__set_bit(slot, sp->slot_bitmap);
}

static void mmu_convert_notrap(struct kvm_mmu_page *sp)
{
	int i;
	u64 *pt = sp->spt;

	if (shadow_trap_nonpresent_pte == shadow_notrap_nonpresent_pte)
		return;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
		if (pt[i] == shadow_notrap_nonpresent_pte)
			__set_spte(&pt[i], shadow_trap_nonpresent_pte);
	}
}

/*
 * The function is based on mtrr_type_lookup() in
 * arch/x86/kernel/cpu/mtrr/generic.c
 */
static int get_mtrr_type(struct mtrr_state_type *mtrr_state,
			 u64 start, u64 end)
{
	int i;
	u64 base, mask;
	u8 prev_match, curr_match;
	int num_var_ranges = KVM_NR_VAR_MTRR;

	if (!mtrr_state->enabled)
		return 0xFF;

	/* Make end inclusive end, instead of exclusive */
	end--;

	/* Look in fixed ranges. Just return the type as per start */
	if (mtrr_state->have_fixed && (start < 0x100000)) {
		int idx;

		if (start < 0x80000) {
			idx = 0;
			idx += (start >> 16);
			return mtrr_state->fixed_ranges[idx];
		} else if (start < 0xC0000) {
			idx = 1 * 8;
			idx += ((start - 0x80000) >> 14);
			return mtrr_state->fixed_ranges[idx];
		} else if (start < 0x1000000) {
			idx = 3 * 8;
			idx += ((start - 0xC0000) >> 12);
			return mtrr_state->fixed_ranges[idx];
		}
	}

	/*
	 * Look in variable ranges
	 * Look of multiple ranges matching this address and pick type
	 * as per MTRR precedence
	 */
	if (!(mtrr_state->enabled & 2))
		return mtrr_state->def_type;

	prev_match = 0xFF;
	for (i = 0; i < num_var_ranges; ++i) {
		unsigned short start_state, end_state;

		if (!(mtrr_state->var_ranges[i].mask_lo & (1 << 11)))
			continue;

		base = (((u64)mtrr_state->var_ranges[i].base_hi) << 32) +
		       (mtrr_state->var_ranges[i].base_lo & PAGE_MASK);
		mask = (((u64)mtrr_state->var_ranges[i].mask_hi) << 32) +
		       (mtrr_state->var_ranges[i].mask_lo & PAGE_MASK);

		start_state = ((start & mask) == (base & mask));
		end_state = ((end & mask) == (base & mask));
		if (start_state != end_state)
			return 0xFE;

		if ((start & mask) != (base & mask))
			continue;

		curr_match = mtrr_state->var_ranges[i].base_lo & 0xff;
		if (prev_match == 0xFF) {
			prev_match = curr_match;
			continue;
		}

		if (prev_match == MTRR_TYPE_UNCACHABLE ||
		    curr_match == MTRR_TYPE_UNCACHABLE)
			return MTRR_TYPE_UNCACHABLE;

		if ((prev_match == MTRR_TYPE_WRBACK &&
		     curr_match == MTRR_TYPE_WRTHROUGH) ||
		    (prev_match == MTRR_TYPE_WRTHROUGH &&
		     curr_match == MTRR_TYPE_WRBACK)) {
			prev_match = MTRR_TYPE_WRTHROUGH;
			curr_match = MTRR_TYPE_WRTHROUGH;
		}

		if (prev_match != curr_match)
			return MTRR_TYPE_UNCACHABLE;
	}

	if (prev_match != 0xFF)
		return prev_match;

	return mtrr_state->def_type;
}

u8 kvm_get_guest_memory_type(struct kvm_vcpu *vcpu, gfn_t gfn)
{
	u8 mtrr;

	mtrr = get_mtrr_type(&vcpu->arch.mtrr_state, gfn << PAGE_SHIFT,
			     (gfn << PAGE_SHIFT) + PAGE_SIZE);
	if (mtrr == 0xfe || mtrr == 0xff)
		mtrr = MTRR_TYPE_WRBACK;
	return mtrr;
}
EXPORT_SYMBOL_GPL(kvm_get_guest_memory_type);

static void __kvm_unsync_page(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp)
{
	trace_kvm_mmu_unsync_page(sp);
	++vcpu->kvm->stat.mmu_unsync;
	sp->unsync = 1;

	kvm_mmu_mark_parents_unsync(sp);
	mmu_convert_notrap(sp);
}

static void kvm_unsync_pages(struct kvm_vcpu *vcpu,  gfn_t gfn)
{
	struct kvm_mmu_page *s;
	struct hlist_node *node;

	for_each_gfn_indirect_valid_sp(vcpu->kvm, s, gfn, node) {
		if (s->unsync)
			continue;
		WARN_ON(s->role.level != PT_PAGE_TABLE_LEVEL);
		__kvm_unsync_page(vcpu, s);
	}
}

static int mmu_need_write_protect(struct kvm_vcpu *vcpu, gfn_t gfn,
				  bool can_unsync)
{
	struct kvm_mmu_page *s;
	struct hlist_node *node;
	bool need_unsync = false;

	for_each_gfn_indirect_valid_sp(vcpu->kvm, s, gfn, node) {
		if (!can_unsync)
			return 1;

		if (s->role.level != PT_PAGE_TABLE_LEVEL)
			return 1;

		if (!need_unsync && !s->unsync) {
			if (!oos_shadow)
				return 1;
			need_unsync = true;
		}
	}
	if (need_unsync)
		kvm_unsync_pages(vcpu, gfn);
	return 0;
}

static int set_spte(struct kvm_vcpu *vcpu, u64 *sptep,
		    unsigned pte_access, int user_fault,
		    int write_fault, int dirty, int level,
		    gfn_t gfn, pfn_t pfn, bool speculative,
		    bool can_unsync, bool reset_host_protection)
{
	u64 spte;
	int ret = 0;

	/*
	 * We don't set the accessed bit, since we sometimes want to see
	 * whether the guest actually used the pte (in order to detect
	 * demand paging).
	 */
	spte = shadow_base_present_pte | shadow_dirty_mask;
	if (!speculative)
		spte |= shadow_accessed_mask;
	if (!dirty)
		pte_access &= ~ACC_WRITE_MASK;
	if (pte_access & ACC_EXEC_MASK)
		spte |= shadow_x_mask;
	else
		spte |= shadow_nx_mask;
	if (pte_access & ACC_USER_MASK)
		spte |= shadow_user_mask;
	if (level > PT_PAGE_TABLE_LEVEL)
		spte |= PT_PAGE_SIZE_MASK;
	if (tdp_enabled)
		spte |= kvm_x86_ops->get_mt_mask(vcpu, gfn,
			kvm_is_mmio_pfn(pfn));

	if (reset_host_protection)
		spte |= SPTE_HOST_WRITEABLE;

	spte |= (u64)pfn << PAGE_SHIFT;

	if ((pte_access & ACC_WRITE_MASK)
	    || (!tdp_enabled && write_fault && !is_write_protection(vcpu)
		&& !user_fault)) {

		if (level > PT_PAGE_TABLE_LEVEL &&
		    has_wrprotected_page(vcpu->kvm, gfn, level)) {
			ret = 1;
			drop_spte(vcpu->kvm, sptep, shadow_trap_nonpresent_pte);
			goto done;
		}

		spte |= PT_WRITABLE_MASK;

		if (!tdp_enabled && !(pte_access & ACC_WRITE_MASK))
			spte &= ~PT_USER_MASK;

		/*
		 * Optimization: for pte sync, if spte was writable the hash
		 * lookup is unnecessary (and expensive). Write protection
		 * is responsibility of mmu_get_page / kvm_sync_page.
		 * Same reasoning can be applied to dirty page accounting.
		 */
		if (!can_unsync && is_writable_pte(*sptep))
			goto set_pte;

		if (mmu_need_write_protect(vcpu, gfn, can_unsync)) {
			pgprintk("%s: found shadow page for %lx, marking ro\n",
				 __func__, gfn);
			ret = 1;
			pte_access &= ~ACC_WRITE_MASK;
			if (is_writable_pte(spte))
				spte &= ~PT_WRITABLE_MASK;
		}
	}

	if (pte_access & ACC_WRITE_MASK)
		mark_page_dirty(vcpu->kvm, gfn);

set_pte:
	if (is_writable_pte(*sptep) && !is_writable_pte(spte))
		kvm_set_pfn_dirty(pfn);
	update_spte(sptep, spte);
done:
	return ret;
}

static void mmu_set_spte(struct kvm_vcpu *vcpu, u64 *sptep,
			 unsigned pt_access, unsigned pte_access,
			 int user_fault, int write_fault, int dirty,
			 int *ptwrite, int level, gfn_t gfn,
			 pfn_t pfn, bool speculative,
			 bool reset_host_protection)
{
	int was_rmapped = 0;
	int rmap_count;

	pgprintk("%s: spte %llx access %x write_fault %d"
		 " user_fault %d gfn %lx\n",
		 __func__, *sptep, pt_access,
		 write_fault, user_fault, gfn);

	if (is_rmap_spte(*sptep)) {
		/*
		 * If we overwrite a PTE page pointer with a 2MB PMD, unlink
		 * the parent of the now unreachable PTE.
		 */
		if (level > PT_PAGE_TABLE_LEVEL &&
		    !is_large_pte(*sptep)) {
			struct kvm_mmu_page *child;
			u64 pte = *sptep;

			child = page_header(pte & PT64_BASE_ADDR_MASK);
			mmu_page_remove_parent_pte(child, sptep);
			__set_spte(sptep, shadow_trap_nonpresent_pte);
			kvm_flush_remote_tlbs(vcpu->kvm);
		} else if (pfn != spte_to_pfn(*sptep)) {
			pgprintk("hfn old %lx new %lx\n",
				 spte_to_pfn(*sptep), pfn);
			drop_spte(vcpu->kvm, sptep, shadow_trap_nonpresent_pte);
			kvm_flush_remote_tlbs(vcpu->kvm);
		} else
			was_rmapped = 1;
	}

	if (set_spte(vcpu, sptep, pte_access, user_fault, write_fault,
		      dirty, level, gfn, pfn, speculative, true,
		      reset_host_protection)) {
		if (write_fault)
			*ptwrite = 1;
		kvm_mmu_flush_tlb(vcpu);
	}

	pgprintk("%s: setting spte %llx\n", __func__, *sptep);
	pgprintk("instantiating %s PTE (%s) at %ld (%llx) addr %p\n",
		 is_large_pte(*sptep)? "2MB" : "4kB",
		 *sptep & PT_PRESENT_MASK ?"RW":"R", gfn,
		 *sptep, sptep);
	if (!was_rmapped && is_large_pte(*sptep))
		++vcpu->kvm->stat.lpages;

	page_header_update_slot(vcpu->kvm, sptep, gfn);
	if (!was_rmapped) {
		rmap_count = rmap_add(vcpu, sptep, gfn);
		if (rmap_count > RMAP_RECYCLE_THRESHOLD)
			rmap_recycle(vcpu, sptep, gfn);
	}
	kvm_release_pfn_clean(pfn);
	if (speculative) {
		vcpu->arch.last_pte_updated = sptep;
		vcpu->arch.last_pte_gfn = gfn;
	}
}

static void nonpaging_new_cr3(struct kvm_vcpu *vcpu)
{
}

static int __direct_map(struct kvm_vcpu *vcpu, gpa_t v, int write,
			int level, gfn_t gfn, pfn_t pfn)
{
	struct kvm_shadow_walk_iterator iterator;
	struct kvm_mmu_page *sp;
	int pt_write = 0;
	gfn_t pseudo_gfn;

	for_each_shadow_entry(vcpu, (u64)gfn << PAGE_SHIFT, iterator) {
		if (iterator.level == level) {
			mmu_set_spte(vcpu, iterator.sptep, ACC_ALL, ACC_ALL,
				     0, write, 1, &pt_write,
				     level, gfn, pfn, false, true);
			++vcpu->stat.pf_fixed;
			break;
		}

		if (*iterator.sptep == shadow_trap_nonpresent_pte) {
			u64 base_addr = iterator.addr;

			base_addr &= PT64_LVL_ADDR_MASK(iterator.level);
			pseudo_gfn = base_addr >> PAGE_SHIFT;
			sp = kvm_mmu_get_page(vcpu, pseudo_gfn, iterator.addr,
					      iterator.level - 1,
					      1, ACC_ALL, iterator.sptep);
			if (!sp) {
				pgprintk("nonpaging_map: ENOMEM\n");
				kvm_release_pfn_clean(pfn);
				return -ENOMEM;
			}

			__set_spte(iterator.sptep,
				   __pa(sp->spt)
				   | PT_PRESENT_MASK | PT_WRITABLE_MASK
				   | shadow_user_mask | shadow_x_mask);
		}
	}
	return pt_write;
}

static void kvm_send_hwpoison_signal(struct kvm *kvm, gfn_t gfn)
{
	char buf[1];
	void __user *hva;
	int r;

	/* Touch the page, so send SIGBUS */
	hva = (void __user *)gfn_to_hva(kvm, gfn);
	r = copy_from_user(buf, hva, 1);
}

static int kvm_handle_bad_page(struct kvm *kvm, gfn_t gfn, pfn_t pfn)
{
	kvm_release_pfn_clean(pfn);
	if (is_hwpoison_pfn(pfn)) {
		kvm_send_hwpoison_signal(kvm, gfn);
		return 0;
	} else if (is_fault_pfn(pfn))
		return -EFAULT;

	return 1;
}

static int nonpaging_map(struct kvm_vcpu *vcpu, gva_t v, int write, gfn_t gfn)
{
	int r;
	int level;
	pfn_t pfn;
	unsigned long mmu_seq;

	level = mapping_level(vcpu, gfn);

	/*
	 * This path builds a PAE pagetable - so we can map 2mb pages at
	 * maximum. Therefore check if the level is larger than that.
	 */
	if (level > PT_DIRECTORY_LEVEL)
		level = PT_DIRECTORY_LEVEL;

	gfn &= ~(KVM_PAGES_PER_HPAGE(level) - 1);

	mmu_seq = vcpu->kvm->mmu_notifier_seq;
	smp_rmb();
	pfn = gfn_to_pfn(vcpu->kvm, gfn);

	/* mmio */
	if (is_error_pfn(pfn))
		return kvm_handle_bad_page(vcpu->kvm, gfn, pfn);

	spin_lock(&vcpu->kvm->mmu_lock);
	if (mmu_notifier_retry(vcpu, mmu_seq))
		goto out_unlock;
	kvm_mmu_free_some_pages(vcpu);
	r = __direct_map(vcpu, v, write, level, gfn, pfn);
	spin_unlock(&vcpu->kvm->mmu_lock);


	return r;

out_unlock:
	spin_unlock(&vcpu->kvm->mmu_lock);
	kvm_release_pfn_clean(pfn);
	return 0;
}


static void mmu_free_roots(struct kvm_vcpu *vcpu)
{
	int i;
	struct kvm_mmu_page *sp;
	LIST_HEAD(invalid_list);

	if (!VALID_PAGE(vcpu->arch.mmu.root_hpa))
		return;
	spin_lock(&vcpu->kvm->mmu_lock);
	if (vcpu->arch.mmu.shadow_root_level == PT64_ROOT_LEVEL) {
		hpa_t root = vcpu->arch.mmu.root_hpa;

		sp = page_header(root);
		--sp->root_count;
		if (!sp->root_count && sp->role.invalid) {
			kvm_mmu_prepare_zap_page(vcpu->kvm, sp, &invalid_list);
			kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
		}
		vcpu->arch.mmu.root_hpa = INVALID_PAGE;
		spin_unlock(&vcpu->kvm->mmu_lock);
		return;
	}
	for (i = 0; i < 4; ++i) {
		hpa_t root = vcpu->arch.mmu.pae_root[i];

		if (root) {
			root &= PT64_BASE_ADDR_MASK;
			sp = page_header(root);
			--sp->root_count;
			if (!sp->root_count && sp->role.invalid)
				kvm_mmu_prepare_zap_page(vcpu->kvm, sp,
							 &invalid_list);
		}
		vcpu->arch.mmu.pae_root[i] = INVALID_PAGE;
	}
	kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
	spin_unlock(&vcpu->kvm->mmu_lock);
	vcpu->arch.mmu.root_hpa = INVALID_PAGE;
}

static int mmu_check_root(struct kvm_vcpu *vcpu, gfn_t root_gfn)
{
	int ret = 0;

	if (!kvm_is_visible_gfn(vcpu->kvm, root_gfn)) {
		kvm_make_request(KVM_REQ_TRIPLE_FAULT, vcpu);
		ret = 1;
	}

	return ret;
}

static int mmu_alloc_roots(struct kvm_vcpu *vcpu)
{
	int i;
	gfn_t root_gfn;
	struct kvm_mmu_page *sp;
	int direct = 0;
	u64 pdptr;

	root_gfn = vcpu->arch.cr3 >> PAGE_SHIFT;

	if (vcpu->arch.mmu.shadow_root_level == PT64_ROOT_LEVEL) {
		hpa_t root = vcpu->arch.mmu.root_hpa;

		ASSERT(!VALID_PAGE(root));
		if (mmu_check_root(vcpu, root_gfn))
			return 1;
		if (tdp_enabled) {
			direct = 1;
			root_gfn = 0;
		}
		spin_lock(&vcpu->kvm->mmu_lock);
		kvm_mmu_free_some_pages(vcpu);
		sp = kvm_mmu_get_page(vcpu, root_gfn, 0,
				      PT64_ROOT_LEVEL, direct,
				      ACC_ALL, NULL);
		root = __pa(sp->spt);
		++sp->root_count;
		spin_unlock(&vcpu->kvm->mmu_lock);
		vcpu->arch.mmu.root_hpa = root;
		return 0;
	}
	direct = !is_paging(vcpu);

	if (mmu_check_root(vcpu, root_gfn))
		return 1;

	for (i = 0; i < 4; ++i) {
		hpa_t root = vcpu->arch.mmu.pae_root[i];

		ASSERT(!VALID_PAGE(root));
		if (vcpu->arch.mmu.root_level == PT32E_ROOT_LEVEL) {
			pdptr = kvm_pdptr_read(vcpu, i);
			if (!is_present_gpte(pdptr)) {
				vcpu->arch.mmu.pae_root[i] = 0;
				continue;
			}
			root_gfn = pdptr >> PAGE_SHIFT;
			if (mmu_check_root(vcpu, root_gfn))
				return 1;
		} else if (vcpu->arch.mmu.root_level == 0)
			root_gfn = 0;
		if (tdp_enabled) {
			direct = 1;
			root_gfn = i << (30 - PAGE_SHIFT);
		}
		spin_lock(&vcpu->kvm->mmu_lock);
		kvm_mmu_free_some_pages(vcpu);
		sp = kvm_mmu_get_page(vcpu, root_gfn, i << 30,
				      PT32_ROOT_LEVEL, direct,
				      ACC_ALL, NULL);
		root = __pa(sp->spt);
		++sp->root_count;
		spin_unlock(&vcpu->kvm->mmu_lock);

		vcpu->arch.mmu.pae_root[i] = root | PT_PRESENT_MASK;
	}
	vcpu->arch.mmu.root_hpa = __pa(vcpu->arch.mmu.pae_root);
	return 0;
}

static void mmu_sync_roots(struct kvm_vcpu *vcpu)
{
	int i;
	struct kvm_mmu_page *sp;

	if (!VALID_PAGE(vcpu->arch.mmu.root_hpa))
		return;
	if (vcpu->arch.mmu.shadow_root_level == PT64_ROOT_LEVEL) {
		hpa_t root = vcpu->arch.mmu.root_hpa;
		sp = page_header(root);
		mmu_sync_children(vcpu, sp);
		return;
	}
	for (i = 0; i < 4; ++i) {
		hpa_t root = vcpu->arch.mmu.pae_root[i];

		if (root && VALID_PAGE(root)) {
			root &= PT64_BASE_ADDR_MASK;
			sp = page_header(root);
			mmu_sync_children(vcpu, sp);
		}
	}
}

void kvm_mmu_sync_roots(struct kvm_vcpu *vcpu)
{
	spin_lock(&vcpu->kvm->mmu_lock);
	mmu_sync_roots(vcpu);
	spin_unlock(&vcpu->kvm->mmu_lock);
}

static gpa_t nonpaging_gva_to_gpa(struct kvm_vcpu *vcpu, gva_t vaddr,
				  u32 access, u32 *error)
{
	if (error)
		*error = 0;
	return vaddr;
}

static int nonpaging_page_fault(struct kvm_vcpu *vcpu, gva_t gva,
				u32 error_code)
{
	gfn_t gfn;
	int r;

	pgprintk("%s: gva %lx error %x\n", __func__, gva, error_code);
	r = mmu_topup_memory_caches(vcpu);
	if (r)
		return r;

	ASSERT(vcpu);
	ASSERT(VALID_PAGE(vcpu->arch.mmu.root_hpa));

	gfn = gva >> PAGE_SHIFT;

	return nonpaging_map(vcpu, gva & PAGE_MASK,
			     error_code & PFERR_WRITE_MASK, gfn);
}

static int tdp_page_fault(struct kvm_vcpu *vcpu, gva_t gpa,
				u32 error_code)
{
	pfn_t pfn;
	int r;
	int level;
	gfn_t gfn = gpa >> PAGE_SHIFT;
	unsigned long mmu_seq;

	ASSERT(vcpu);
	ASSERT(VALID_PAGE(vcpu->arch.mmu.root_hpa));

	r = mmu_topup_memory_caches(vcpu);
	if (r)
		return r;

	level = mapping_level(vcpu, gfn);

	gfn &= ~(KVM_PAGES_PER_HPAGE(level) - 1);

	mmu_seq = vcpu->kvm->mmu_notifier_seq;
	smp_rmb();
	pfn = gfn_to_pfn(vcpu->kvm, gfn);
	if (is_error_pfn(pfn))
		return kvm_handle_bad_page(vcpu->kvm, gfn, pfn);
	spin_lock(&vcpu->kvm->mmu_lock);
	if (mmu_notifier_retry(vcpu, mmu_seq))
		goto out_unlock;
	kvm_mmu_free_some_pages(vcpu);
	r = __direct_map(vcpu, gpa, error_code & PFERR_WRITE_MASK,
			 level, gfn, pfn);
	spin_unlock(&vcpu->kvm->mmu_lock);

	return r;

out_unlock:
	spin_unlock(&vcpu->kvm->mmu_lock);
	kvm_release_pfn_clean(pfn);
	return 0;
}

static void nonpaging_free(struct kvm_vcpu *vcpu)
{
	mmu_free_roots(vcpu);
}

static int nonpaging_init_context(struct kvm_vcpu *vcpu)
{
	struct kvm_mmu *context = &vcpu->arch.mmu;

	context->new_cr3 = nonpaging_new_cr3;
	context->page_fault = nonpaging_page_fault;
	context->gva_to_gpa = nonpaging_gva_to_gpa;
	context->free = nonpaging_free;
	context->prefetch_page = nonpaging_prefetch_page;
	context->sync_page = nonpaging_sync_page;
	context->invlpg = nonpaging_invlpg;
	context->root_level = 0;
	context->shadow_root_level = PT32E_ROOT_LEVEL;
	context->root_hpa = INVALID_PAGE;
	return 0;
}

void kvm_mmu_flush_tlb(struct kvm_vcpu *vcpu)
{
	++vcpu->stat.tlb_flush;
	kvm_make_request(KVM_REQ_TLB_FLUSH, vcpu);
}

static void paging_new_cr3(struct kvm_vcpu *vcpu)
{
	pgprintk("%s: cr3 %lx\n", __func__, vcpu->arch.cr3);
	mmu_free_roots(vcpu);
}

static void inject_page_fault(struct kvm_vcpu *vcpu,
			      u64 addr,
			      u32 err_code)
{
	kvm_inject_page_fault(vcpu, addr, err_code);
}

static void paging_free(struct kvm_vcpu *vcpu)
{
	nonpaging_free(vcpu);
}

static bool is_rsvd_bits_set(struct kvm_vcpu *vcpu, u64 gpte, int level)
{
	int bit7;

	bit7 = (gpte >> 7) & 1;
	return (gpte & vcpu->arch.mmu.rsvd_bits_mask[bit7][level-1]) != 0;
}

#define PTTYPE 64
#include "paging_tmpl.h"
#undef PTTYPE

#define PTTYPE 32
#include "paging_tmpl.h"
#undef PTTYPE

static void reset_rsvds_bits_mask(struct kvm_vcpu *vcpu, int level)
{
	struct kvm_mmu *context = &vcpu->arch.mmu;
	int maxphyaddr = cpuid_maxphyaddr(vcpu);
	u64 exb_bit_rsvd = 0;

	if (!is_nx(vcpu))
		exb_bit_rsvd = rsvd_bits(63, 63);
	switch (level) {
	case PT32_ROOT_LEVEL:
		/* no rsvd bits for 2 level 4K page table entries */
		context->rsvd_bits_mask[0][1] = 0;
		context->rsvd_bits_mask[0][0] = 0;
		context->rsvd_bits_mask[1][0] = context->rsvd_bits_mask[0][0];

		if (!is_pse(vcpu)) {
			context->rsvd_bits_mask[1][1] = 0;
			break;
		}

		if (is_cpuid_PSE36())
			/* 36bits PSE 4MB page */
			context->rsvd_bits_mask[1][1] = rsvd_bits(17, 21);
		else
			/* 32 bits PSE 4MB page */
			context->rsvd_bits_mask[1][1] = rsvd_bits(13, 21);
		break;
	case PT32E_ROOT_LEVEL:
		context->rsvd_bits_mask[0][2] =
			rsvd_bits(maxphyaddr, 63) |
			rsvd_bits(7, 8) | rsvd_bits(1, 2);	/* PDPTE */
		context->rsvd_bits_mask[0][1] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 62);	/* PDE */
		context->rsvd_bits_mask[0][0] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 62); 	/* PTE */
		context->rsvd_bits_mask[1][1] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 62) |
			rsvd_bits(13, 20);		/* large page */
		context->rsvd_bits_mask[1][0] = context->rsvd_bits_mask[0][0];
		break;
	case PT64_ROOT_LEVEL:
		context->rsvd_bits_mask[0][3] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51) | rsvd_bits(7, 8);
		context->rsvd_bits_mask[0][2] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51) | rsvd_bits(7, 8);
		context->rsvd_bits_mask[0][1] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51);
		context->rsvd_bits_mask[0][0] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51);
		context->rsvd_bits_mask[1][3] = context->rsvd_bits_mask[0][3];
		context->rsvd_bits_mask[1][2] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51) |
			rsvd_bits(13, 29);
		context->rsvd_bits_mask[1][1] = exb_bit_rsvd |
			rsvd_bits(maxphyaddr, 51) |
			rsvd_bits(13, 20);		/* large page */
		context->rsvd_bits_mask[1][0] = context->rsvd_bits_mask[0][0];
		break;
	}
}

static int paging64_init_context_common(struct kvm_vcpu *vcpu, int level)
{
	struct kvm_mmu *context = &vcpu->arch.mmu;

	ASSERT(is_pae(vcpu));
	context->new_cr3 = paging_new_cr3;
	context->page_fault = paging64_page_fault;
	context->gva_to_gpa = paging64_gva_to_gpa;
	context->prefetch_page = paging64_prefetch_page;
	context->sync_page = paging64_sync_page;
	context->invlpg = paging64_invlpg;
	context->free = paging_free;
	context->root_level = level;
	context->shadow_root_level = level;
	context->root_hpa = INVALID_PAGE;
	return 0;
}

static int paging64_init_context(struct kvm_vcpu *vcpu)
{
	reset_rsvds_bits_mask(vcpu, PT64_ROOT_LEVEL);
	return paging64_init_context_common(vcpu, PT64_ROOT_LEVEL);
}

static int paging32_init_context(struct kvm_vcpu *vcpu)
{
	struct kvm_mmu *context = &vcpu->arch.mmu;

	reset_rsvds_bits_mask(vcpu, PT32_ROOT_LEVEL);
	context->new_cr3 = paging_new_cr3;
	context->page_fault = paging32_page_fault;
	context->gva_to_gpa = paging32_gva_to_gpa;
	context->free = paging_free;
	context->prefetch_page = paging32_prefetch_page;
	context->sync_page = paging32_sync_page;
	context->invlpg = paging32_invlpg;
	context->root_level = PT32_ROOT_LEVEL;
	context->shadow_root_level = PT32E_ROOT_LEVEL;
	context->root_hpa = INVALID_PAGE;
	return 0;
}

static int paging32E_init_context(struct kvm_vcpu *vcpu)
{
	reset_rsvds_bits_mask(vcpu, PT32E_ROOT_LEVEL);
	return paging64_init_context_common(vcpu, PT32E_ROOT_LEVEL);
}

static int init_kvm_tdp_mmu(struct kvm_vcpu *vcpu)
{
	struct kvm_mmu *context = &vcpu->arch.mmu;

	context->new_cr3 = nonpaging_new_cr3;
	context->page_fault = tdp_page_fault;
	context->free = nonpaging_free;
	context->prefetch_page = nonpaging_prefetch_page;
	context->sync_page = nonpaging_sync_page;
	context->invlpg = nonpaging_invlpg;
	context->shadow_root_level = kvm_x86_ops->get_tdp_level();
	context->root_hpa = INVALID_PAGE;

	if (!is_paging(vcpu)) {
		context->gva_to_gpa = nonpaging_gva_to_gpa;
		context->root_level = 0;
	} else if (is_long_mode(vcpu)) {
		reset_rsvds_bits_mask(vcpu, PT64_ROOT_LEVEL);
		context->gva_to_gpa = paging64_gva_to_gpa;
		context->root_level = PT64_ROOT_LEVEL;
	} else if (is_pae(vcpu)) {
		reset_rsvds_bits_mask(vcpu, PT32E_ROOT_LEVEL);
		context->gva_to_gpa = paging64_gva_to_gpa;
		context->root_level = PT32E_ROOT_LEVEL;
	} else {
		reset_rsvds_bits_mask(vcpu, PT32_ROOT_LEVEL);
		context->gva_to_gpa = paging32_gva_to_gpa;
		context->root_level = PT32_ROOT_LEVEL;
	}

	return 0;
}

static int init_kvm_softmmu(struct kvm_vcpu *vcpu)
{
	int r;

	ASSERT(vcpu);
	ASSERT(!VALID_PAGE(vcpu->arch.mmu.root_hpa));

	if (!is_paging(vcpu))
		r = nonpaging_init_context(vcpu);
	else if (is_long_mode(vcpu))
		r = paging64_init_context(vcpu);
	else if (is_pae(vcpu))
		r = paging32E_init_context(vcpu);
	else
		r = paging32_init_context(vcpu);

	vcpu->arch.mmu.base_role.cr4_pae = !!is_pae(vcpu);
	vcpu->arch.mmu.base_role.cr0_wp = is_write_protection(vcpu);

	return r;
}

static int init_kvm_mmu(struct kvm_vcpu *vcpu)
{
	vcpu->arch.update_pte.pfn = bad_pfn;

	if (tdp_enabled)
		return init_kvm_tdp_mmu(vcpu);
	else
		return init_kvm_softmmu(vcpu);
}

static void destroy_kvm_mmu(struct kvm_vcpu *vcpu)
{
	ASSERT(vcpu);
	if (VALID_PAGE(vcpu->arch.mmu.root_hpa))
		/* mmu.free() should set root_hpa = INVALID_PAGE */
		vcpu->arch.mmu.free(vcpu);
}

int kvm_mmu_reset_context(struct kvm_vcpu *vcpu)
{
	destroy_kvm_mmu(vcpu);
	return init_kvm_mmu(vcpu);
}
EXPORT_SYMBOL_GPL(kvm_mmu_reset_context);

int kvm_mmu_load(struct kvm_vcpu *vcpu)
{
	int r;

	r = mmu_topup_memory_caches(vcpu);
	if (r)
		goto out;
	r = mmu_alloc_roots(vcpu);
	spin_lock(&vcpu->kvm->mmu_lock);
	mmu_sync_roots(vcpu);
	spin_unlock(&vcpu->kvm->mmu_lock);
	if (r)
		goto out;
	/* set_cr3() should ensure TLB has been flushed */
	kvm_x86_ops->set_cr3(vcpu, vcpu->arch.mmu.root_hpa);
out:
	return r;
}
EXPORT_SYMBOL_GPL(kvm_mmu_load);

void kvm_mmu_unload(struct kvm_vcpu *vcpu)
{
	mmu_free_roots(vcpu);
}

static void mmu_pte_write_zap_pte(struct kvm_vcpu *vcpu,
				  struct kvm_mmu_page *sp,
				  u64 *spte)
{
	u64 pte;
	struct kvm_mmu_page *child;

	pte = *spte;
	if (is_shadow_present_pte(pte)) {
		if (is_last_spte(pte, sp->role.level))
			drop_spte(vcpu->kvm, spte, shadow_trap_nonpresent_pte);
		else {
			child = page_header(pte & PT64_BASE_ADDR_MASK);
			mmu_page_remove_parent_pte(child, spte);
		}
	}
	__set_spte(spte, shadow_trap_nonpresent_pte);
	if (is_large_pte(pte))
		--vcpu->kvm->stat.lpages;
}

static void mmu_pte_write_new_pte(struct kvm_vcpu *vcpu,
				  struct kvm_mmu_page *sp,
				  u64 *spte,
				  const void *new)
{
	if (sp->role.level != PT_PAGE_TABLE_LEVEL) {
		++vcpu->kvm->stat.mmu_pde_zapped;
		return;
        }

	if (is_rsvd_bits_set(vcpu, *(u64 *)new, PT_PAGE_TABLE_LEVEL))
		return;

	++vcpu->kvm->stat.mmu_pte_updated;
	if (!sp->role.cr4_pae)
		paging32_update_pte(vcpu, sp, spte, new);
	else
		paging64_update_pte(vcpu, sp, spte, new);
}

static bool need_remote_flush(u64 old, u64 new)
{
	if (!is_shadow_present_pte(old))
		return false;
	if (!is_shadow_present_pte(new))
		return true;
	if ((old ^ new) & PT64_BASE_ADDR_MASK)
		return true;
	old ^= PT64_NX_MASK;
	new ^= PT64_NX_MASK;
	return (old & ~new & PT64_PERM_MASK) != 0;
}

static void mmu_pte_write_flush_tlb(struct kvm_vcpu *vcpu, bool zap_page,
				    bool remote_flush, bool local_flush)
{
	if (zap_page)
		return;

	if (remote_flush)
		kvm_flush_remote_tlbs(vcpu->kvm);
	else if (local_flush)
		kvm_mmu_flush_tlb(vcpu);
}

static bool last_updated_pte_accessed(struct kvm_vcpu *vcpu)
{
	u64 *spte = vcpu->arch.last_pte_updated;

	return !!(spte && (*spte & shadow_accessed_mask));
}

static void mmu_guess_page_from_pte_write(struct kvm_vcpu *vcpu, gpa_t gpa,
					  u64 gpte)
{
	gfn_t gfn;
	pfn_t pfn;

	if (!is_present_gpte(gpte))
		return;
	gfn = (gpte & PT64_BASE_ADDR_MASK) >> PAGE_SHIFT;

	vcpu->arch.update_pte.mmu_seq = vcpu->kvm->mmu_notifier_seq;
	smp_rmb();
	pfn = gfn_to_pfn(vcpu->kvm, gfn);

	if (is_error_pfn(pfn)) {
		kvm_release_pfn_clean(pfn);
		return;
	}
	vcpu->arch.update_pte.gfn = gfn;
	vcpu->arch.update_pte.pfn = pfn;
}

static void kvm_mmu_access_page(struct kvm_vcpu *vcpu, gfn_t gfn)
{
	u64 *spte = vcpu->arch.last_pte_updated;

	if (spte
	    && vcpu->arch.last_pte_gfn == gfn
	    && shadow_accessed_mask
	    && !(*spte & shadow_accessed_mask)
	    && is_shadow_present_pte(*spte))
		set_bit(PT_ACCESSED_SHIFT, (unsigned long *)spte);
}

void kvm_mmu_pte_write(struct kvm_vcpu *vcpu, gpa_t gpa,
		       const u8 *new, int bytes,
		       bool guest_initiated)
{
	gfn_t gfn = gpa >> PAGE_SHIFT;
	union kvm_mmu_page_role mask = { .word = 0 };
	struct kvm_mmu_page *sp;
	struct hlist_node *node;
	LIST_HEAD(invalid_list);
	u64 entry, gentry;
	u64 *spte;
	unsigned offset = offset_in_page(gpa);
	unsigned pte_size;
	unsigned page_offset;
	unsigned misaligned;
	unsigned quadrant;
	int level;
	int flooded = 0;
	int npte;
	int r;
	int invlpg_counter;
	bool remote_flush, local_flush, zap_page;

	zap_page = remote_flush = local_flush = false;

	pgprintk("%s: gpa %llx bytes %d\n", __func__, gpa, bytes);

	invlpg_counter = atomic_read(&vcpu->kvm->arch.invlpg_counter);

	/*
	 * Assume that the pte write on a page table of the same type
	 * as the current vcpu paging mode.  This is nearly always true
	 * (might be false while changing modes).  Note it is verified later
	 * by update_pte().
	 */
	if ((is_pae(vcpu) && bytes == 4) || !new) {
		/* Handle a 32-bit guest writing two halves of a 64-bit gpte */
		if (is_pae(vcpu)) {
			gpa &= ~(gpa_t)7;
			bytes = 8;
		}
		r = kvm_read_guest(vcpu->kvm, gpa, &gentry, min(bytes, 8));
		if (r)
			gentry = 0;
		new = (const u8 *)&gentry;
	}

	switch (bytes) {
	case 4:
		gentry = *(const u32 *)new;
		break;
	case 8:
		gentry = *(const u64 *)new;
		break;
	default:
		gentry = 0;
		break;
	}

	mmu_guess_page_from_pte_write(vcpu, gpa, gentry);
	spin_lock(&vcpu->kvm->mmu_lock);
	if (atomic_read(&vcpu->kvm->arch.invlpg_counter) != invlpg_counter)
		gentry = 0;
	kvm_mmu_access_page(vcpu, gfn);
	kvm_mmu_free_some_pages(vcpu);
	++vcpu->kvm->stat.mmu_pte_write;
	kvm_mmu_audit(vcpu, "pre pte write");
	if (guest_initiated) {
		if (gfn == vcpu->arch.last_pt_write_gfn
		    && !last_updated_pte_accessed(vcpu)) {
			++vcpu->arch.last_pt_write_count;
			if (vcpu->arch.last_pt_write_count >= 3)
				flooded = 1;
		} else {
			vcpu->arch.last_pt_write_gfn = gfn;
			vcpu->arch.last_pt_write_count = 1;
			vcpu->arch.last_pte_updated = NULL;
		}
	}

	mask.cr0_wp = mask.cr4_pae = mask.nxe = 1;
	for_each_gfn_indirect_valid_sp(vcpu->kvm, sp, gfn, node) {
		pte_size = sp->role.cr4_pae ? 8 : 4;
		misaligned = (offset ^ (offset + bytes - 1)) & ~(pte_size - 1);
		misaligned |= bytes < 4;
		if (misaligned || flooded) {
			/*
			 * Misaligned accesses are too much trouble to fix
			 * up; also, they usually indicate a page is not used
			 * as a page table.
			 *
			 * If we're seeing too many writes to a page,
			 * it may no longer be a page table, or we may be
			 * forking, in which case it is better to unmap the
			 * page.
			 */
			pgprintk("misaligned: gpa %llx bytes %d role %x\n",
				 gpa, bytes, sp->role.word);
			zap_page |= !!kvm_mmu_prepare_zap_page(vcpu->kvm, sp,
						     &invalid_list);
			++vcpu->kvm->stat.mmu_flooded;
			continue;
		}
		page_offset = offset;
		level = sp->role.level;
		npte = 1;
		if (!sp->role.cr4_pae) {
			page_offset <<= 1;	/* 32->64 */
			/*
			 * A 32-bit pde maps 4MB while the shadow pdes map
			 * only 2MB.  So we need to double the offset again
			 * and zap two pdes instead of one.
			 */
			if (level == PT32_ROOT_LEVEL) {
				page_offset &= ~7; /* kill rounding error */
				page_offset <<= 1;
				npte = 2;
			}
			quadrant = page_offset >> PAGE_SHIFT;
			page_offset &= ~PAGE_MASK;
			if (quadrant != sp->role.quadrant)
				continue;
		}
		local_flush = true;
		spte = &sp->spt[page_offset / sizeof(*spte)];
		while (npte--) {
			entry = *spte;
			mmu_pte_write_zap_pte(vcpu, sp, spte);
			if (gentry &&
			      !((sp->role.word ^ vcpu->arch.mmu.base_role.word)
			      & mask.word))
				mmu_pte_write_new_pte(vcpu, sp, spte, &gentry);
			if (!remote_flush && need_remote_flush(entry, *spte))
				remote_flush = true;
			++spte;
		}
	}
	mmu_pte_write_flush_tlb(vcpu, zap_page, remote_flush, local_flush);
	kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
	kvm_mmu_audit(vcpu, "post pte write");
	spin_unlock(&vcpu->kvm->mmu_lock);
	if (!is_error_pfn(vcpu->arch.update_pte.pfn)) {
		kvm_release_pfn_clean(vcpu->arch.update_pte.pfn);
		vcpu->arch.update_pte.pfn = bad_pfn;
	}
}

int kvm_mmu_unprotect_page_virt(struct kvm_vcpu *vcpu, gva_t gva)
{
	gpa_t gpa;
	int r;

	if (tdp_enabled)
		return 0;

	gpa = kvm_mmu_gva_to_gpa_read(vcpu, gva, NULL);

	spin_lock(&vcpu->kvm->mmu_lock);
	r = kvm_mmu_unprotect_page(vcpu->kvm, gpa >> PAGE_SHIFT);
	spin_unlock(&vcpu->kvm->mmu_lock);
	return r;
}
EXPORT_SYMBOL_GPL(kvm_mmu_unprotect_page_virt);

void __kvm_mmu_free_some_pages(struct kvm_vcpu *vcpu)
{
	int free_pages;
	LIST_HEAD(invalid_list);

	free_pages = vcpu->kvm->arch.n_free_mmu_pages;
	while (free_pages < KVM_REFILL_PAGES &&
	       !list_empty(&vcpu->kvm->arch.active_mmu_pages)) {
		struct kvm_mmu_page *sp;

		sp = container_of(vcpu->kvm->arch.active_mmu_pages.prev,
				  struct kvm_mmu_page, link);
		free_pages += kvm_mmu_prepare_zap_page(vcpu->kvm, sp,
						       &invalid_list);
		++vcpu->kvm->stat.mmu_recycled;
	}
	kvm_mmu_commit_zap_page(vcpu->kvm, &invalid_list);
}

int kvm_mmu_page_fault(struct kvm_vcpu *vcpu, gva_t cr2, u32 error_code)
{
	int r;
	enum emulation_result er;

	r = vcpu->arch.mmu.page_fault(vcpu, cr2, error_code);
	if (r < 0)
		goto out;

	if (!r) {
		r = 1;
		goto out;
	}

	r = mmu_topup_memory_caches(vcpu);
	if (r)
		goto out;

	er = emulate_instruction(vcpu, cr2, error_code, 0);

	switch (er) {
	case EMULATE_DONE:
		return 1;
	case EMULATE_DO_MMIO:
		++vcpu->stat.mmio_exits;
		/* fall through */
	case EMULATE_FAIL:
		return 0;
	default:
		BUG();
	}
out:
	return r;
}
EXPORT_SYMBOL_GPL(kvm_mmu_page_fault);

void kvm_mmu_invlpg(struct kvm_vcpu *vcpu, gva_t gva)
{
	vcpu->arch.mmu.invlpg(vcpu, gva);
	kvm_mmu_flush_tlb(vcpu);
	++vcpu->stat.invlpg;
}
EXPORT_SYMBOL_GPL(kvm_mmu_invlpg);

void kvm_enable_tdp(void)
{
	tdp_enabled = true;
}
EXPORT_SYMBOL_GPL(kvm_enable_tdp);

void kvm_disable_tdp(void)
{
	tdp_enabled = false;
}
EXPORT_SYMBOL_GPL(kvm_disable_tdp);

static void free_mmu_pages(struct kvm_vcpu *vcpu)
{
	free_page((unsigned long)vcpu->arch.mmu.pae_root);
}

static int alloc_mmu_pages(struct kvm_vcpu *vcpu)
{
	struct page *page;
	int i;

	ASSERT(vcpu);

	/*
	 * When emulating 32-bit mode, cr3 is only 32 bits even on x86_64.
	 * Therefore we need to allocate shadow page tables in the first
	 * 4GB of memory, which happens to fit the DMA32 zone.
	 */
	page = alloc_page(GFP_KERNEL | __GFP_DMA32);
	if (!page)
		return -ENOMEM;

	vcpu->arch.mmu.pae_root = page_address(page);
	for (i = 0; i < 4; ++i)
		vcpu->arch.mmu.pae_root[i] = INVALID_PAGE;

	return 0;
}

int kvm_mmu_create(struct kvm_vcpu *vcpu)
{
	ASSERT(vcpu);
	ASSERT(!VALID_PAGE(vcpu->arch.mmu.root_hpa));

	return alloc_mmu_pages(vcpu);
}

int kvm_mmu_setup(struct kvm_vcpu *vcpu)
{
	ASSERT(vcpu);
	ASSERT(!VALID_PAGE(vcpu->arch.mmu.root_hpa));

	return init_kvm_mmu(vcpu);
}

void kvm_mmu_destroy(struct kvm_vcpu *vcpu)
{
	ASSERT(vcpu);

	destroy_kvm_mmu(vcpu);
	free_mmu_pages(vcpu);
	mmu_free_memory_caches(vcpu);
}

void kvm_mmu_slot_remove_write_access(struct kvm *kvm, int slot)
{
	struct kvm_mmu_page *sp;

	list_for_each_entry(sp, &kvm->arch.active_mmu_pages, link) {
		int i;
		u64 *pt;

		if (!test_bit(slot, sp->slot_bitmap))
			continue;

		pt = sp->spt;
		for (i = 0; i < PT64_ENT_PER_PAGE; ++i)
			/* avoid RMW */
			if (is_writable_pte(pt[i]))
				pt[i] &= ~PT_WRITABLE_MASK;
	}
	kvm_flush_remote_tlbs(kvm);
}

void kvm_mmu_zap_all(struct kvm *kvm)
{
	struct kvm_mmu_page *sp, *node;
	LIST_HEAD(invalid_list);

	spin_lock(&kvm->mmu_lock);
restart:
	list_for_each_entry_safe(sp, node, &kvm->arch.active_mmu_pages, link)
		if (kvm_mmu_prepare_zap_page(kvm, sp, &invalid_list))
			goto restart;

	kvm_mmu_commit_zap_page(kvm, &invalid_list);
	spin_unlock(&kvm->mmu_lock);
}

static int kvm_mmu_remove_some_alloc_mmu_pages(struct kvm *kvm,
					       struct list_head *invalid_list)
{
	struct kvm_mmu_page *page;

	page = container_of(kvm->arch.active_mmu_pages.prev,
			    struct kvm_mmu_page, link);
	return kvm_mmu_prepare_zap_page(kvm, page, invalid_list);
}

static int mmu_shrink(struct shrinker *shrink, int nr_to_scan, gfp_t gfp_mask)
{
	struct kvm *kvm;
	struct kvm *kvm_freed = NULL;
	int cache_count = 0;

	spin_lock(&kvm_lock);

	list_for_each_entry(kvm, &vm_list, vm_list) {
		int npages, idx, freed_pages;
		LIST_HEAD(invalid_list);

		idx = srcu_read_lock(&kvm->srcu);
		spin_lock(&kvm->mmu_lock);
		npages = kvm->arch.n_alloc_mmu_pages -
			 kvm->arch.n_free_mmu_pages;
		cache_count += npages;
		if (!kvm_freed && nr_to_scan > 0 && npages > 0) {
			freed_pages = kvm_mmu_remove_some_alloc_mmu_pages(kvm,
							  &invalid_list);
			cache_count -= freed_pages;
			kvm_freed = kvm;
		}
		nr_to_scan--;

		kvm_mmu_commit_zap_page(kvm, &invalid_list);
		spin_unlock(&kvm->mmu_lock);
		srcu_read_unlock(&kvm->srcu, idx);
	}
	if (kvm_freed)
		list_move_tail(&kvm_freed->vm_list, &vm_list);

	spin_unlock(&kvm_lock);

	return cache_count;
}

static struct shrinker mmu_shrinker = {
	.shrink = mmu_shrink,
	.seeks = DEFAULT_SEEKS * 10,
};

static void mmu_destroy_caches(void)
{
	if (pte_chain_cache)
		kmem_cache_destroy(pte_chain_cache);
	if (rmap_desc_cache)
		kmem_cache_destroy(rmap_desc_cache);
	if (mmu_page_header_cache)
		kmem_cache_destroy(mmu_page_header_cache);
}

void kvm_mmu_module_exit(void)
{
	mmu_destroy_caches();
	unregister_shrinker(&mmu_shrinker);
}

int kvm_mmu_module_init(void)
{
	pte_chain_cache = kmem_cache_create("kvm_pte_chain",
					    sizeof(struct kvm_pte_chain),
					    0, 0, NULL);
	if (!pte_chain_cache)
		goto nomem;
	rmap_desc_cache = kmem_cache_create("kvm_rmap_desc",
					    sizeof(struct kvm_rmap_desc),
					    0, 0, NULL);
	if (!rmap_desc_cache)
		goto nomem;

	mmu_page_header_cache = kmem_cache_create("kvm_mmu_page_header",
						  sizeof(struct kvm_mmu_page),
						  0, 0, NULL);
	if (!mmu_page_header_cache)
		goto nomem;

	register_shrinker(&mmu_shrinker);

	return 0;

nomem:
	mmu_destroy_caches();
	return -ENOMEM;
}

/*
 * Caculate mmu pages needed for kvm.
 */
unsigned int kvm_mmu_calculate_mmu_pages(struct kvm *kvm)
{
	int i;
	unsigned int nr_mmu_pages;
	unsigned int  nr_pages = 0;
	struct kvm_memslots *slots;

	slots = kvm_memslots(kvm);

	for (i = 0; i < slots->nmemslots; i++)
		nr_pages += slots->memslots[i].npages;

	nr_mmu_pages = nr_pages * KVM_PERMILLE_MMU_PAGES / 1000;
	nr_mmu_pages = max(nr_mmu_pages,
			(unsigned int) KVM_MIN_ALLOC_MMU_PAGES);

	return nr_mmu_pages;
}

static void *pv_mmu_peek_buffer(struct kvm_pv_mmu_op_buffer *buffer,
				unsigned len)
{
	if (len > buffer->len)
		return NULL;
	return buffer->ptr;
}

static void *pv_mmu_read_buffer(struct kvm_pv_mmu_op_buffer *buffer,
				unsigned len)
{
	void *ret;

	ret = pv_mmu_peek_buffer(buffer, len);
	if (!ret)
		return ret;
	buffer->ptr += len;
	buffer->len -= len;
	buffer->processed += len;
	return ret;
}

static int kvm_pv_mmu_write(struct kvm_vcpu *vcpu,
			     gpa_t addr, gpa_t value)
{
	int bytes = 8;
	int r;

	if (!is_long_mode(vcpu) && !is_pae(vcpu))
		bytes = 4;

	r = mmu_topup_memory_caches(vcpu);
	if (r)
		return r;

	if (!emulator_write_phys(vcpu, addr, &value, bytes))
		return -EFAULT;

	return 1;
}

static int kvm_pv_mmu_flush_tlb(struct kvm_vcpu *vcpu)
{
	(void)kvm_set_cr3(vcpu, vcpu->arch.cr3);
	return 1;
}

static int kvm_pv_mmu_release_pt(struct kvm_vcpu *vcpu, gpa_t addr)
{
	spin_lock(&vcpu->kvm->mmu_lock);
	mmu_unshadow(vcpu->kvm, addr >> PAGE_SHIFT);
	spin_unlock(&vcpu->kvm->mmu_lock);
	return 1;
}

static int kvm_pv_mmu_op_one(struct kvm_vcpu *vcpu,
			     struct kvm_pv_mmu_op_buffer *buffer)
{
	struct kvm_mmu_op_header *header;

	header = pv_mmu_peek_buffer(buffer, sizeof *header);
	if (!header)
		return 0;
	switch (header->op) {
	case KVM_MMU_OP_WRITE_PTE: {
		struct kvm_mmu_op_write_pte *wpte;

		wpte = pv_mmu_read_buffer(buffer, sizeof *wpte);
		if (!wpte)
			return 0;
		return kvm_pv_mmu_write(vcpu, wpte->pte_phys,
					wpte->pte_val);
	}
	case KVM_MMU_OP_FLUSH_TLB: {
		struct kvm_mmu_op_flush_tlb *ftlb;

		ftlb = pv_mmu_read_buffer(buffer, sizeof *ftlb);
		if (!ftlb)
			return 0;
		return kvm_pv_mmu_flush_tlb(vcpu);
	}
	case KVM_MMU_OP_RELEASE_PT: {
		struct kvm_mmu_op_release_pt *rpt;

		rpt = pv_mmu_read_buffer(buffer, sizeof *rpt);
		if (!rpt)
			return 0;
		return kvm_pv_mmu_release_pt(vcpu, rpt->pt_phys);
	}
	default: return 0;
	}
}

int kvm_pv_mmu_op(struct kvm_vcpu *vcpu, unsigned long bytes,
		  gpa_t addr, unsigned long *ret)
{
	int r;
	struct kvm_pv_mmu_op_buffer *buffer = &vcpu->arch.mmu_op_buffer;

	buffer->ptr = buffer->buf;
	buffer->len = min_t(unsigned long, bytes, sizeof buffer->buf);
	buffer->processed = 0;

	r = kvm_read_guest(vcpu->kvm, addr, buffer->buf, buffer->len);
	if (r)
		goto out;

	while (buffer->len) {
		r = kvm_pv_mmu_op_one(vcpu, buffer);
		if (r < 0)
			goto out;
		if (r == 0)
			break;
	}

	r = 1;
out:
	*ret = buffer->processed;
	return r;
}

int kvm_mmu_get_spte_hierarchy(struct kvm_vcpu *vcpu, u64 addr, u64 sptes[4])
{
	struct kvm_shadow_walk_iterator iterator;
	int nr_sptes = 0;

	spin_lock(&vcpu->kvm->mmu_lock);
	for_each_shadow_entry(vcpu, addr, iterator) {
		sptes[iterator.level-1] = *iterator.sptep;
		nr_sptes++;
		if (!is_shadow_present_pte(*iterator.sptep))
			break;
	}
	spin_unlock(&vcpu->kvm->mmu_lock);

	return nr_sptes;
}
EXPORT_SYMBOL_GPL(kvm_mmu_get_spte_hierarchy);

#ifdef AUDIT

static const char *audit_msg;

static gva_t canonicalize(gva_t gva)
{
#ifdef CONFIG_X86_64
	gva = (long long)(gva << 16) >> 16;
#endif
	return gva;
}


typedef void (*inspect_spte_fn) (struct kvm *kvm, u64 *sptep);

static void __mmu_spte_walk(struct kvm *kvm, struct kvm_mmu_page *sp,
			    inspect_spte_fn fn)
{
	int i;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
		u64 ent = sp->spt[i];

		if (is_shadow_present_pte(ent)) {
			if (!is_last_spte(ent, sp->role.level)) {
				struct kvm_mmu_page *child;
				child = page_header(ent & PT64_BASE_ADDR_MASK);
				__mmu_spte_walk(kvm, child, fn);
			} else
				fn(kvm, &sp->spt[i]);
		}
	}
}

static void mmu_spte_walk(struct kvm_vcpu *vcpu, inspect_spte_fn fn)
{
	int i;
	struct kvm_mmu_page *sp;

	if (!VALID_PAGE(vcpu->arch.mmu.root_hpa))
		return;
	if (vcpu->arch.mmu.shadow_root_level == PT64_ROOT_LEVEL) {
		hpa_t root = vcpu->arch.mmu.root_hpa;
		sp = page_header(root);
		__mmu_spte_walk(vcpu->kvm, sp, fn);
		return;
	}
	for (i = 0; i < 4; ++i) {
		hpa_t root = vcpu->arch.mmu.pae_root[i];

		if (root && VALID_PAGE(root)) {
			root &= PT64_BASE_ADDR_MASK;
			sp = page_header(root);
			__mmu_spte_walk(vcpu->kvm, sp, fn);
		}
	}
	return;
}

static void audit_mappings_page(struct kvm_vcpu *vcpu, u64 page_pte,
				gva_t va, int level)
{
	u64 *pt = __va(page_pte & PT64_BASE_ADDR_MASK);
	int i;
	gva_t va_delta = 1ul << (PAGE_SHIFT + 9 * (level - 1));

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i, va += va_delta) {
		u64 ent = pt[i];

		if (ent == shadow_trap_nonpresent_pte)
			continue;

		va = canonicalize(va);
		if (is_shadow_present_pte(ent) && !is_last_spte(ent, level))
			audit_mappings_page(vcpu, ent, va, level - 1);
		else {
			gpa_t gpa = kvm_mmu_gva_to_gpa_read(vcpu, va, NULL);
			gfn_t gfn = gpa >> PAGE_SHIFT;
			pfn_t pfn = gfn_to_pfn(vcpu->kvm, gfn);
			hpa_t hpa = (hpa_t)pfn << PAGE_SHIFT;

			if (is_error_pfn(pfn)) {
				kvm_release_pfn_clean(pfn);
				continue;
			}

			if (is_shadow_present_pte(ent)
			    && (ent & PT64_BASE_ADDR_MASK) != hpa)
				printk(KERN_ERR "xx audit error: (%s) levels %d"
				       " gva %lx gpa %llx hpa %llx ent %llx %d\n",
				       audit_msg, vcpu->arch.mmu.root_level,
				       va, gpa, hpa, ent,
				       is_shadow_present_pte(ent));
			else if (ent == shadow_notrap_nonpresent_pte
				 && !is_error_hpa(hpa))
				printk(KERN_ERR "audit: (%s) notrap shadow,"
				       " valid guest gva %lx\n", audit_msg, va);
			kvm_release_pfn_clean(pfn);

		}
	}
}

static void audit_mappings(struct kvm_vcpu *vcpu)
{
	unsigned i;

	if (vcpu->arch.mmu.root_level == 4)
		audit_mappings_page(vcpu, vcpu->arch.mmu.root_hpa, 0, 4);
	else
		for (i = 0; i < 4; ++i)
			if (vcpu->arch.mmu.pae_root[i] & PT_PRESENT_MASK)
				audit_mappings_page(vcpu,
						    vcpu->arch.mmu.pae_root[i],
						    i << 30,
						    2);
}

static int count_rmaps(struct kvm_vcpu *vcpu)
{
	struct kvm *kvm = vcpu->kvm;
	struct kvm_memslots *slots;
	int nmaps = 0;
	int i, j, k, idx;

	idx = srcu_read_lock(&kvm->srcu);
	slots = kvm_memslots(kvm);
	for (i = 0; i < KVM_MEMORY_SLOTS; ++i) {
		struct kvm_memory_slot *m = &slots->memslots[i];
		struct kvm_rmap_desc *d;

		for (j = 0; j < m->npages; ++j) {
			unsigned long *rmapp = &m->rmap[j];

			if (!*rmapp)
				continue;
			if (!(*rmapp & 1)) {
				++nmaps;
				continue;
			}
			d = (struct kvm_rmap_desc *)(*rmapp & ~1ul);
			while (d) {
				for (k = 0; k < RMAP_EXT; ++k)
					if (d->sptes[k])
						++nmaps;
					else
						break;
				d = d->more;
			}
		}
	}
	srcu_read_unlock(&kvm->srcu, idx);
	return nmaps;
}

void inspect_spte_has_rmap(struct kvm *kvm, u64 *sptep)
{
	unsigned long *rmapp;
	struct kvm_mmu_page *rev_sp;
	gfn_t gfn;

	if (is_writable_pte(*sptep)) {
		rev_sp = page_header(__pa(sptep));
		gfn = kvm_mmu_page_get_gfn(rev_sp, sptep - rev_sp->spt);

		if (!gfn_to_memslot(kvm, gfn)) {
			if (!printk_ratelimit())
				return;
			printk(KERN_ERR "%s: no memslot for gfn %ld\n",
					 audit_msg, gfn);
			printk(KERN_ERR "%s: index %ld of sp (gfn=%lx)\n",
			       audit_msg, (long int)(sptep - rev_sp->spt),
					rev_sp->gfn);
			dump_stack();
			return;
		}

		rmapp = gfn_to_rmap(kvm, gfn, rev_sp->role.level);
		if (!*rmapp) {
			if (!printk_ratelimit())
				return;
			printk(KERN_ERR "%s: no rmap for writable spte %llx\n",
					 audit_msg, *sptep);
			dump_stack();
		}
	}

}

void audit_writable_sptes_have_rmaps(struct kvm_vcpu *vcpu)
{
	mmu_spte_walk(vcpu, inspect_spte_has_rmap);
}

static void check_writable_mappings_rmap(struct kvm_vcpu *vcpu)
{
	struct kvm_mmu_page *sp;
	int i;

	list_for_each_entry(sp, &vcpu->kvm->arch.active_mmu_pages, link) {
		u64 *pt = sp->spt;

		if (sp->role.level != PT_PAGE_TABLE_LEVEL)
			continue;

		for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
			u64 ent = pt[i];

			if (!(ent & PT_PRESENT_MASK))
				continue;
			if (!is_writable_pte(ent))
				continue;
			inspect_spte_has_rmap(vcpu->kvm, &pt[i]);
		}
	}
	return;
}

static void audit_rmap(struct kvm_vcpu *vcpu)
{
	check_writable_mappings_rmap(vcpu);
	count_rmaps(vcpu);
}

static void audit_write_protection(struct kvm_vcpu *vcpu)
{
	struct kvm_mmu_page *sp;
	struct kvm_memory_slot *slot;
	unsigned long *rmapp;
	u64 *spte;
	gfn_t gfn;

	list_for_each_entry(sp, &vcpu->kvm->arch.active_mmu_pages, link) {
		if (sp->role.direct)
			continue;
		if (sp->unsync)
			continue;

		slot = gfn_to_memslot(vcpu->kvm, sp->gfn);
		rmapp = &slot->rmap[gfn - slot->base_gfn];

		spte = rmap_next(vcpu->kvm, rmapp, NULL);
		while (spte) {
			if (is_writable_pte(*spte))
				printk(KERN_ERR "%s: (%s) shadow page has "
				"writable mappings: gfn %lx role %x\n",
			       __func__, audit_msg, sp->gfn,
			       sp->role.word);
			spte = rmap_next(vcpu->kvm, rmapp, spte);
		}
	}
}

static void kvm_mmu_audit(struct kvm_vcpu *vcpu, const char *msg)
{
	int olddbg = dbg;

	dbg = 0;
	audit_msg = msg;
	audit_rmap(vcpu);
	audit_write_protection(vcpu);
	if (strcmp("pre pte write", audit_msg) != 0)
		audit_mappings(vcpu);
	audit_writable_sptes_have_rmaps(vcpu);
	dbg = olddbg;
}

#endif
