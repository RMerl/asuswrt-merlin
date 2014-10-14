/*
 * Kernel-based Virtual Machine driver for Linux
 *
 * This module enables machines with Intel VT-x extensions to run virtual
 * machines without emulation or binary translation.
 *
 * MMU support
 *
 * Copyright (C) 2006 Qumranet, Inc.
 * Copyright 2010 Red Hat, Inc. and/or its affiliates.
 *
 * Authors:
 *   Yaniv Kamay  <yaniv@qumranet.com>
 *   Avi Kivity   <avi@qumranet.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

/*
 * We need the mmu code to access both 32-bit and 64-bit guest ptes,
 * so the code in this file is compiled twice, once per pte size.
 */

#if PTTYPE == 64
	#define pt_element_t u64
	#define guest_walker guest_walker64
	#define FNAME(name) paging##64_##name
	#define PT_BASE_ADDR_MASK PT64_BASE_ADDR_MASK
	#define PT_LVL_ADDR_MASK(lvl) PT64_LVL_ADDR_MASK(lvl)
	#define PT_LVL_OFFSET_MASK(lvl) PT64_LVL_OFFSET_MASK(lvl)
	#define PT_INDEX(addr, level) PT64_INDEX(addr, level)
	#define PT_LEVEL_BITS PT64_LEVEL_BITS
	#ifdef CONFIG_X86_64
	#define PT_MAX_FULL_LEVELS 4
	#define CMPXCHG cmpxchg
	#else
	#define CMPXCHG cmpxchg64
	#define PT_MAX_FULL_LEVELS 2
	#endif
#elif PTTYPE == 32
	#define pt_element_t u32
	#define guest_walker guest_walker32
	#define FNAME(name) paging##32_##name
	#define PT_BASE_ADDR_MASK PT32_BASE_ADDR_MASK
	#define PT_LVL_ADDR_MASK(lvl) PT32_LVL_ADDR_MASK(lvl)
	#define PT_LVL_OFFSET_MASK(lvl) PT32_LVL_OFFSET_MASK(lvl)
	#define PT_INDEX(addr, level) PT32_INDEX(addr, level)
	#define PT_LEVEL_BITS PT32_LEVEL_BITS
	#define PT_MAX_FULL_LEVELS 2
	#define CMPXCHG cmpxchg
#else
	#error Invalid PTTYPE value
#endif

#define gpte_to_gfn_lvl FNAME(gpte_to_gfn_lvl)
#define gpte_to_gfn(pte) gpte_to_gfn_lvl((pte), PT_PAGE_TABLE_LEVEL)

/*
 * The guest_walker structure emulates the behavior of the hardware page
 * table walker.
 */
struct guest_walker {
	int level;
	gfn_t table_gfn[PT_MAX_FULL_LEVELS];
	pt_element_t ptes[PT_MAX_FULL_LEVELS];
	pt_element_t prefetch_ptes[PTE_PREFETCH_NUM];
	gpa_t pte_gpa[PT_MAX_FULL_LEVELS];
	unsigned pt_access;
	unsigned pte_access;
	gfn_t gfn;
	struct x86_exception fault;
};

static gfn_t gpte_to_gfn_lvl(pt_element_t gpte, int lvl)
{
	return (gpte & PT_LVL_ADDR_MASK(lvl)) >> PAGE_SHIFT;
}

static bool FNAME(cmpxchg_gpte)(struct kvm *kvm,
			 gfn_t table_gfn, unsigned index,
			 pt_element_t orig_pte, pt_element_t new_pte)
{
	pt_element_t ret;
	pt_element_t *table;
	struct page *page;

	page = gfn_to_page(kvm, table_gfn);

	table = kmap_atomic(page, KM_USER0);
	ret = CMPXCHG(&table[index], orig_pte, new_pte);
	kunmap_atomic(table, KM_USER0);

	kvm_release_page_dirty(page);

	return (ret != orig_pte);
}

static unsigned FNAME(gpte_access)(struct kvm_vcpu *vcpu, pt_element_t gpte)
{
	unsigned access;

	access = (gpte & (PT_WRITABLE_MASK | PT_USER_MASK)) | ACC_EXEC_MASK;
#if PTTYPE == 64
	if (vcpu->arch.mmu.nx)
		access &= ~(gpte >> PT64_NX_SHIFT);
#endif
	return access;
}

/*
 * Fetch a guest pte for a guest virtual address
 */
static int FNAME(walk_addr_generic)(struct guest_walker *walker,
				    struct kvm_vcpu *vcpu, struct kvm_mmu *mmu,
				    gva_t addr, u32 access)
{
	pt_element_t pte;
	gfn_t table_gfn;
	unsigned index, pt_access, uninitialized_var(pte_access);
	gpa_t pte_gpa;
	bool eperm, present, rsvd_fault;
	int offset, write_fault, user_fault, fetch_fault;

	write_fault = access & PFERR_WRITE_MASK;
	user_fault = access & PFERR_USER_MASK;
	fetch_fault = access & PFERR_FETCH_MASK;

	trace_kvm_mmu_pagetable_walk(addr, write_fault, user_fault,
				     fetch_fault);
walk:
	present = true;
	eperm = rsvd_fault = false;
	walker->level = mmu->root_level;
	pte           = mmu->get_cr3(vcpu);

#if PTTYPE == 64
	if (walker->level == PT32E_ROOT_LEVEL) {
		pte = kvm_pdptr_read_mmu(vcpu, mmu, (addr >> 30) & 3);
		trace_kvm_mmu_paging_element(pte, walker->level);
		if (!is_present_gpte(pte)) {
			present = false;
			goto error;
		}
		--walker->level;
	}
#endif
	ASSERT((!is_long_mode(vcpu) && is_pae(vcpu)) ||
	       (mmu->get_cr3(vcpu) & CR3_NONPAE_RESERVED_BITS) == 0);

	pt_access = ACC_ALL;

	for (;;) {
		index = PT_INDEX(addr, walker->level);

		table_gfn = gpte_to_gfn(pte);
		offset    = index * sizeof(pt_element_t);
		pte_gpa   = gfn_to_gpa(table_gfn) + offset;
		walker->table_gfn[walker->level - 1] = table_gfn;
		walker->pte_gpa[walker->level - 1] = pte_gpa;

		if (kvm_read_guest_page_mmu(vcpu, mmu, table_gfn, &pte,
					    offset, sizeof(pte),
					    PFERR_USER_MASK|PFERR_WRITE_MASK)) {
			present = false;
			break;
		}

		trace_kvm_mmu_paging_element(pte, walker->level);

		if (!is_present_gpte(pte)) {
			present = false;
			break;
		}

		if (is_rsvd_bits_set(&vcpu->arch.mmu, pte, walker->level)) {
			rsvd_fault = true;
			break;
		}

		if (write_fault && !is_writable_pte(pte))
			if (user_fault || is_write_protection(vcpu))
				eperm = true;

		if (user_fault && !(pte & PT_USER_MASK))
			eperm = true;

#if PTTYPE == 64
		if (fetch_fault && (pte & PT64_NX_MASK))
			eperm = true;
#endif

		if (!eperm && !rsvd_fault && !(pte & PT_ACCESSED_MASK)) {
			trace_kvm_mmu_set_accessed_bit(table_gfn, index,
						       sizeof(pte));
			if (FNAME(cmpxchg_gpte)(vcpu->kvm, table_gfn,
			    index, pte, pte|PT_ACCESSED_MASK))
				goto walk;
			mark_page_dirty(vcpu->kvm, table_gfn);
			pte |= PT_ACCESSED_MASK;
		}

		pte_access = pt_access & FNAME(gpte_access)(vcpu, pte);

		walker->ptes[walker->level - 1] = pte;

		if ((walker->level == PT_PAGE_TABLE_LEVEL) ||
		    ((walker->level == PT_DIRECTORY_LEVEL) &&
				is_large_pte(pte) &&
				(PTTYPE == 64 || is_pse(vcpu))) ||
		    ((walker->level == PT_PDPE_LEVEL) &&
				is_large_pte(pte) &&
				mmu->root_level == PT64_ROOT_LEVEL)) {
			int lvl = walker->level;
			gpa_t real_gpa;
			gfn_t gfn;
			u32 ac;

			gfn = gpte_to_gfn_lvl(pte, lvl);
			gfn += (addr & PT_LVL_OFFSET_MASK(lvl)) >> PAGE_SHIFT;

			if (PTTYPE == 32 &&
			    walker->level == PT_DIRECTORY_LEVEL &&
			    is_cpuid_PSE36())
				gfn += pse36_gfn_delta(pte);

			ac = write_fault | fetch_fault | user_fault;

			real_gpa = mmu->translate_gpa(vcpu, gfn_to_gpa(gfn),
						      ac);
			if (real_gpa == UNMAPPED_GVA)
				return 0;

			walker->gfn = real_gpa >> PAGE_SHIFT;

			break;
		}

		pt_access = pte_access;
		--walker->level;
	}

	if (!present || eperm || rsvd_fault)
		goto error;

	if (write_fault && !is_dirty_gpte(pte)) {
		bool ret;

		trace_kvm_mmu_set_dirty_bit(table_gfn, index, sizeof(pte));
		ret = FNAME(cmpxchg_gpte)(vcpu->kvm, table_gfn, index, pte,
			    pte|PT_DIRTY_MASK);
		if (ret)
			goto walk;
		mark_page_dirty(vcpu->kvm, table_gfn);
		pte |= PT_DIRTY_MASK;
		walker->ptes[walker->level - 1] = pte;
	}

	walker->pt_access = pt_access;
	walker->pte_access = pte_access;
	pgprintk("%s: pte %llx pte_access %x pt_access %x\n",
		 __func__, (u64)pte, pte_access, pt_access);
	return 1;

error:
	walker->fault.vector = PF_VECTOR;
	walker->fault.error_code_valid = true;
	walker->fault.error_code = 0;
	if (present)
		walker->fault.error_code |= PFERR_PRESENT_MASK;

	walker->fault.error_code |= write_fault | user_fault;

	if (fetch_fault && mmu->nx)
		walker->fault.error_code |= PFERR_FETCH_MASK;
	if (rsvd_fault)
		walker->fault.error_code |= PFERR_RSVD_MASK;

	walker->fault.address = addr;
	walker->fault.nested_page_fault = mmu != vcpu->arch.walk_mmu;

	trace_kvm_mmu_walker_error(walker->fault.error_code);
	return 0;
}

static int FNAME(walk_addr)(struct guest_walker *walker,
			    struct kvm_vcpu *vcpu, gva_t addr, u32 access)
{
	return FNAME(walk_addr_generic)(walker, vcpu, &vcpu->arch.mmu, addr,
					access);
}

static int FNAME(walk_addr_nested)(struct guest_walker *walker,
				   struct kvm_vcpu *vcpu, gva_t addr,
				   u32 access)
{
	return FNAME(walk_addr_generic)(walker, vcpu, &vcpu->arch.nested_mmu,
					addr, access);
}

static bool FNAME(prefetch_invalid_gpte)(struct kvm_vcpu *vcpu,
				    struct kvm_mmu_page *sp, u64 *spte,
				    pt_element_t gpte)
{
	u64 nonpresent = shadow_trap_nonpresent_pte;

	if (is_rsvd_bits_set(&vcpu->arch.mmu, gpte, PT_PAGE_TABLE_LEVEL))
		goto no_present;

	if (!is_present_gpte(gpte)) {
		if (!sp->unsync)
			nonpresent = shadow_notrap_nonpresent_pte;
		goto no_present;
	}

	if (!(gpte & PT_ACCESSED_MASK))
		goto no_present;

	return false;

no_present:
	drop_spte(vcpu->kvm, spte, nonpresent);
	return true;
}

static void FNAME(update_pte)(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp,
			      u64 *spte, const void *pte, unsigned long mmu_seq)
{
	pt_element_t gpte;
	unsigned pte_access;
	pfn_t pfn;

	gpte = *(const pt_element_t *)pte;
	if (FNAME(prefetch_invalid_gpte)(vcpu, sp, spte, gpte))
		return;

	pgprintk("%s: gpte %llx spte %p\n", __func__, (u64)gpte, spte);
	pte_access = sp->role.access & FNAME(gpte_access)(vcpu, gpte);
	pfn = gfn_to_pfn_atomic(vcpu->kvm, gpte_to_gfn(gpte));
	if (is_error_pfn(pfn)) {
		kvm_release_pfn_clean(pfn);
		return;
	}
	if (mmu_notifier_retry(vcpu, mmu_seq))
		return;

	/*
	 * we call mmu_set_spte() with host_writable = true because that
	 * vcpu->arch.update_pte.pfn was fetched from get_user_pages(write = 1).
	 */
	mmu_set_spte(vcpu, spte, sp->role.access, pte_access, 0, 0,
		     is_dirty_gpte(gpte), NULL, PT_PAGE_TABLE_LEVEL,
		     gpte_to_gfn(gpte), pfn, true, true);
}

static bool FNAME(gpte_changed)(struct kvm_vcpu *vcpu,
				struct guest_walker *gw, int level)
{
	pt_element_t curr_pte;
	gpa_t base_gpa, pte_gpa = gw->pte_gpa[level - 1];
	u64 mask;
	int r, index;

	if (level == PT_PAGE_TABLE_LEVEL) {
		mask = PTE_PREFETCH_NUM * sizeof(pt_element_t) - 1;
		base_gpa = pte_gpa & ~mask;
		index = (pte_gpa - base_gpa) / sizeof(pt_element_t);

		r = kvm_read_guest_atomic(vcpu->kvm, base_gpa,
				gw->prefetch_ptes, sizeof(gw->prefetch_ptes));
		curr_pte = gw->prefetch_ptes[index];
	} else
		r = kvm_read_guest_atomic(vcpu->kvm, pte_gpa,
				  &curr_pte, sizeof(curr_pte));

	return r || curr_pte != gw->ptes[level - 1];
}

static void FNAME(pte_prefetch)(struct kvm_vcpu *vcpu, struct guest_walker *gw,
				u64 *sptep)
{
	struct kvm_mmu_page *sp;
	pt_element_t *gptep = gw->prefetch_ptes;
	u64 *spte;
	int i;

	sp = page_header(__pa(sptep));

	if (sp->role.level > PT_PAGE_TABLE_LEVEL)
		return;

	if (sp->role.direct)
		return __direct_pte_prefetch(vcpu, sp, sptep);

	i = (sptep - sp->spt) & ~(PTE_PREFETCH_NUM - 1);
	spte = sp->spt + i;

	for (i = 0; i < PTE_PREFETCH_NUM; i++, spte++) {
		pt_element_t gpte;
		unsigned pte_access;
		gfn_t gfn;
		pfn_t pfn;
		bool dirty;

		if (spte == sptep)
			continue;

		if (*spte != shadow_trap_nonpresent_pte)
			continue;

		gpte = gptep[i];

		if (FNAME(prefetch_invalid_gpte)(vcpu, sp, spte, gpte))
			continue;

		pte_access = sp->role.access & FNAME(gpte_access)(vcpu, gpte);
		gfn = gpte_to_gfn(gpte);
		dirty = is_dirty_gpte(gpte);
		pfn = pte_prefetch_gfn_to_pfn(vcpu, gfn,
				      (pte_access & ACC_WRITE_MASK) && dirty);
		if (is_error_pfn(pfn)) {
			kvm_release_pfn_clean(pfn);
			break;
		}

		mmu_set_spte(vcpu, spte, sp->role.access, pte_access, 0, 0,
			     dirty, NULL, PT_PAGE_TABLE_LEVEL, gfn,
			     pfn, true, true);
	}
}

/*
 * Fetch a shadow pte for a specific level in the paging hierarchy.
 */
static u64 *FNAME(fetch)(struct kvm_vcpu *vcpu, gva_t addr,
			 struct guest_walker *gw,
			 int user_fault, int write_fault, int hlevel,
			 int *ptwrite, pfn_t pfn, bool map_writable,
			 bool prefault)
{
	unsigned access = gw->pt_access;
	struct kvm_mmu_page *sp = NULL;
	bool dirty = is_dirty_gpte(gw->ptes[gw->level - 1]);
	int top_level;
	unsigned direct_access;
	struct kvm_shadow_walk_iterator it;

	if (!is_present_gpte(gw->ptes[gw->level - 1]))
		return NULL;

	direct_access = gw->pt_access & gw->pte_access;
	if (!dirty)
		direct_access &= ~ACC_WRITE_MASK;

	top_level = vcpu->arch.mmu.root_level;
	if (top_level == PT32E_ROOT_LEVEL)
		top_level = PT32_ROOT_LEVEL;
	/*
	 * Verify that the top-level gpte is still there.  Since the page
	 * is a root page, it is either write protected (and cannot be
	 * changed from now on) or it is invalid (in which case, we don't
	 * really care if it changes underneath us after this point).
	 */
	if (FNAME(gpte_changed)(vcpu, gw, top_level))
		goto out_gpte_changed;

	for (shadow_walk_init(&it, vcpu, addr);
	     shadow_walk_okay(&it) && it.level > gw->level;
	     shadow_walk_next(&it)) {
		gfn_t table_gfn;

		drop_large_spte(vcpu, it.sptep);

		sp = NULL;
		if (!is_shadow_present_pte(*it.sptep)) {
			table_gfn = gw->table_gfn[it.level - 2];
			sp = kvm_mmu_get_page(vcpu, table_gfn, addr, it.level-1,
					      false, access, it.sptep);
		}

		/*
		 * Verify that the gpte in the page we've just write
		 * protected is still there.
		 */
		if (FNAME(gpte_changed)(vcpu, gw, it.level - 1))
			goto out_gpte_changed;

		if (sp)
			link_shadow_page(it.sptep, sp);
	}

	for (;
	     shadow_walk_okay(&it) && it.level > hlevel;
	     shadow_walk_next(&it)) {
		gfn_t direct_gfn;

		validate_direct_spte(vcpu, it.sptep, direct_access);

		drop_large_spte(vcpu, it.sptep);

		if (is_shadow_present_pte(*it.sptep))
			continue;

		direct_gfn = gw->gfn & ~(KVM_PAGES_PER_HPAGE(it.level) - 1);

		sp = kvm_mmu_get_page(vcpu, direct_gfn, addr, it.level-1,
				      true, direct_access, it.sptep);
		link_shadow_page(it.sptep, sp);
	}

	mmu_set_spte(vcpu, it.sptep, access, gw->pte_access & access,
		     user_fault, write_fault, dirty, ptwrite, it.level,
		     gw->gfn, pfn, prefault, map_writable);
	FNAME(pte_prefetch)(vcpu, gw, it.sptep);

	return it.sptep;

out_gpte_changed:
	if (sp)
		kvm_mmu_put_page(sp, it.sptep);
	kvm_release_pfn_clean(pfn);
	return NULL;
}

/*
 * Page fault handler.  There are several causes for a page fault:
 *   - there is no shadow pte for the guest pte
 *   - write access through a shadow pte marked read only so that we can set
 *     the dirty bit
 *   - write access to a shadow pte marked read only so we can update the page
 *     dirty bitmap, when userspace requests it
 *   - mmio access; in this case we will never install a present shadow pte
 *   - normal guest page fault due to the guest pte marked not present, not
 *     writable, or not executable
 *
 *  Returns: 1 if we need to emulate the instruction, 0 otherwise, or
 *           a negative value on error.
 */
static int FNAME(page_fault)(struct kvm_vcpu *vcpu, gva_t addr, u32 error_code,
			     bool prefault)
{
	int write_fault = error_code & PFERR_WRITE_MASK;
	int user_fault = error_code & PFERR_USER_MASK;
	struct guest_walker walker;
	u64 *sptep;
	int write_pt = 0;
	int r;
	pfn_t pfn;
	int level = PT_PAGE_TABLE_LEVEL;
	int force_pt_level;
	unsigned long mmu_seq;
	bool map_writable;

	pgprintk("%s: addr %lx err %x\n", __func__, addr, error_code);

	r = mmu_topup_memory_caches(vcpu);
	if (r)
		return r;

	/*
	 * Look up the guest pte for the faulting address.
	 */
	r = FNAME(walk_addr)(&walker, vcpu, addr, error_code);

	/*
	 * The page is not mapped by the guest.  Let the guest handle it.
	 */
	if (!r) {
		pgprintk("%s: guest page fault\n", __func__);
		if (!prefault) {
			inject_page_fault(vcpu, &walker.fault);
			/* reset fork detector */
			vcpu->arch.last_pt_write_count = 0;
		}
		return 0;
	}

	if (walker.level >= PT_DIRECTORY_LEVEL)
		force_pt_level = mapping_level_dirty_bitmap(vcpu, walker.gfn);
	else
		force_pt_level = 1;
	if (!force_pt_level) {
		level = min(walker.level, mapping_level(vcpu, walker.gfn));
		walker.gfn = walker.gfn & ~(KVM_PAGES_PER_HPAGE(level) - 1);
	}

	mmu_seq = vcpu->kvm->mmu_notifier_seq;
	smp_rmb();

	if (try_async_pf(vcpu, prefault, walker.gfn, addr, &pfn, write_fault,
			 &map_writable))
		return 0;

	/* mmio */
	if (is_error_pfn(pfn))
		return kvm_handle_bad_page(vcpu->kvm, walker.gfn, pfn);

	spin_lock(&vcpu->kvm->mmu_lock);
	if (mmu_notifier_retry(vcpu, mmu_seq))
		goto out_unlock;

	trace_kvm_mmu_audit(vcpu, AUDIT_PRE_PAGE_FAULT);
	kvm_mmu_free_some_pages(vcpu);
	if (!force_pt_level)
		transparent_hugepage_adjust(vcpu, &walker.gfn, &pfn, &level);
	sptep = FNAME(fetch)(vcpu, addr, &walker, user_fault, write_fault,
			     level, &write_pt, pfn, map_writable, prefault);
	(void)sptep;
	pgprintk("%s: shadow pte %p %llx ptwrite %d\n", __func__,
		 sptep, *sptep, write_pt);

	if (!write_pt)
		vcpu->arch.last_pt_write_count = 0; /* reset fork detector */

	++vcpu->stat.pf_fixed;
	trace_kvm_mmu_audit(vcpu, AUDIT_POST_PAGE_FAULT);
	spin_unlock(&vcpu->kvm->mmu_lock);

	return write_pt;

out_unlock:
	spin_unlock(&vcpu->kvm->mmu_lock);
	kvm_release_pfn_clean(pfn);
	return 0;
}

static void FNAME(invlpg)(struct kvm_vcpu *vcpu, gva_t gva)
{
	struct kvm_shadow_walk_iterator iterator;
	struct kvm_mmu_page *sp;
	gpa_t pte_gpa = -1;
	int level;
	u64 *sptep;
	int need_flush = 0;

	spin_lock(&vcpu->kvm->mmu_lock);

	for_each_shadow_entry(vcpu, gva, iterator) {
		level = iterator.level;
		sptep = iterator.sptep;

		sp = page_header(__pa(sptep));
		if (is_last_spte(*sptep, level)) {
			int offset, shift;

			if (!sp->unsync)
				break;

			shift = PAGE_SHIFT -
				  (PT_LEVEL_BITS - PT64_LEVEL_BITS) * level;
			offset = sp->role.quadrant << shift;

			pte_gpa = (sp->gfn << PAGE_SHIFT) + offset;
			pte_gpa += (sptep - sp->spt) * sizeof(pt_element_t);

			if (is_shadow_present_pte(*sptep)) {
				if (is_large_pte(*sptep))
					--vcpu->kvm->stat.lpages;
				drop_spte(vcpu->kvm, sptep,
					  shadow_trap_nonpresent_pte);
				need_flush = 1;
			} else
				__set_spte(sptep, shadow_trap_nonpresent_pte);
			break;
		}

		if (!is_shadow_present_pte(*sptep) || !sp->unsync_children)
			break;
	}

	if (need_flush)
		kvm_flush_remote_tlbs(vcpu->kvm);

	atomic_inc(&vcpu->kvm->arch.invlpg_counter);

	spin_unlock(&vcpu->kvm->mmu_lock);

	if (pte_gpa == -1)
		return;

	if (mmu_topup_memory_caches(vcpu))
		return;
	kvm_mmu_pte_write(vcpu, pte_gpa, NULL, sizeof(pt_element_t), 0);
}

static gpa_t FNAME(gva_to_gpa)(struct kvm_vcpu *vcpu, gva_t vaddr, u32 access,
			       struct x86_exception *exception)
{
	struct guest_walker walker;
	gpa_t gpa = UNMAPPED_GVA;
	int r;

	r = FNAME(walk_addr)(&walker, vcpu, vaddr, access);

	if (r) {
		gpa = gfn_to_gpa(walker.gfn);
		gpa |= vaddr & ~PAGE_MASK;
	} else if (exception)
		*exception = walker.fault;

	return gpa;
}

static gpa_t FNAME(gva_to_gpa_nested)(struct kvm_vcpu *vcpu, gva_t vaddr,
				      u32 access,
				      struct x86_exception *exception)
{
	struct guest_walker walker;
	gpa_t gpa = UNMAPPED_GVA;
	int r;

	r = FNAME(walk_addr_nested)(&walker, vcpu, vaddr, access);

	if (r) {
		gpa = gfn_to_gpa(walker.gfn);
		gpa |= vaddr & ~PAGE_MASK;
	} else if (exception)
		*exception = walker.fault;

	return gpa;
}

static void FNAME(prefetch_page)(struct kvm_vcpu *vcpu,
				 struct kvm_mmu_page *sp)
{
	int i, j, offset, r;
	pt_element_t pt[256 / sizeof(pt_element_t)];
	gpa_t pte_gpa;

	if (sp->role.direct
	    || (PTTYPE == 32 && sp->role.level > PT_PAGE_TABLE_LEVEL)) {
		nonpaging_prefetch_page(vcpu, sp);
		return;
	}

	pte_gpa = gfn_to_gpa(sp->gfn);
	if (PTTYPE == 32) {
		offset = sp->role.quadrant << PT64_LEVEL_BITS;
		pte_gpa += offset * sizeof(pt_element_t);
	}

	for (i = 0; i < PT64_ENT_PER_PAGE; i += ARRAY_SIZE(pt)) {
		r = kvm_read_guest_atomic(vcpu->kvm, pte_gpa, pt, sizeof pt);
		pte_gpa += ARRAY_SIZE(pt) * sizeof(pt_element_t);
		for (j = 0; j < ARRAY_SIZE(pt); ++j)
			if (r || is_present_gpte(pt[j]))
				sp->spt[i+j] = shadow_trap_nonpresent_pte;
			else
				sp->spt[i+j] = shadow_notrap_nonpresent_pte;
	}
}

/*
 * Using the cached information from sp->gfns is safe because:
 * - The spte has a reference to the struct page, so the pfn for a given gfn
 *   can't change unless all sptes pointing to it are nuked first.
 *
 * Note:
 *   We should flush all tlbs if spte is dropped even though guest is
 *   responsible for it. Since if we don't, kvm_mmu_notifier_invalidate_page
 *   and kvm_mmu_notifier_invalidate_range_start detect the mapping page isn't
 *   used by guest then tlbs are not flushed, so guest is allowed to access the
 *   freed pages.
 *   And we increase kvm->tlbs_dirty to delay tlbs flush in this case.
 */
static int FNAME(sync_page)(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp)
{
	int i, offset, nr_present;
	bool host_writable;
	gpa_t first_pte_gpa;

	offset = nr_present = 0;

	/* direct kvm_mmu_page can not be unsync. */
	BUG_ON(sp->role.direct);

	if (PTTYPE == 32)
		offset = sp->role.quadrant << PT64_LEVEL_BITS;

	first_pte_gpa = gfn_to_gpa(sp->gfn) + offset * sizeof(pt_element_t);

	for (i = 0; i < PT64_ENT_PER_PAGE; i++) {
		unsigned pte_access;
		pt_element_t gpte;
		gpa_t pte_gpa;
		gfn_t gfn;

		if (!is_shadow_present_pte(sp->spt[i]))
			continue;

		pte_gpa = first_pte_gpa + i * sizeof(pt_element_t);

		if (kvm_read_guest_atomic(vcpu->kvm, pte_gpa, &gpte,
					  sizeof(pt_element_t)))
			return -EINVAL;

		gfn = gpte_to_gfn(gpte);

		if (FNAME(prefetch_invalid_gpte)(vcpu, sp, &sp->spt[i], gpte)) {
			vcpu->kvm->tlbs_dirty++;
			continue;
		}

		if (gfn != sp->gfns[i]) {
			drop_spte(vcpu->kvm, &sp->spt[i],
				      shadow_trap_nonpresent_pte);
			vcpu->kvm->tlbs_dirty++;
			continue;
		}

		nr_present++;
		pte_access = sp->role.access & FNAME(gpte_access)(vcpu, gpte);
		host_writable = sp->spt[i] & SPTE_HOST_WRITEABLE;

		set_spte(vcpu, &sp->spt[i], pte_access, 0, 0,
			 is_dirty_gpte(gpte), PT_PAGE_TABLE_LEVEL, gfn,
			 spte_to_pfn(sp->spt[i]), true, false,
			 host_writable);
	}

	return !nr_present;
}

#undef pt_element_t
#undef guest_walker
#undef FNAME
#undef PT_BASE_ADDR_MASK
#undef PT_INDEX
#undef PT_LVL_ADDR_MASK
#undef PT_LVL_OFFSET_MASK
#undef PT_LEVEL_BITS
#undef PT_MAX_FULL_LEVELS
#undef gpte_to_gfn
#undef gpte_to_gfn_lvl
#undef CMPXCHG
