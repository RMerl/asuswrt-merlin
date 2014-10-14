/*
 * mmu_audit.c:
 *
 * Audit code for KVM MMU
 *
 * Copyright (C) 2006 Qumranet, Inc.
 * Copyright 2010 Red Hat, Inc. and/or its affiliates.
 *
 * Authors:
 *   Yaniv Kamay  <yaniv@qumranet.com>
 *   Avi Kivity   <avi@qumranet.com>
 *   Marcelo Tosatti <mtosatti@redhat.com>
 *   Xiao Guangrong <xiaoguangrong@cn.fujitsu.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include <linux/ratelimit.h>

#define audit_printk(kvm, fmt, args...)		\
	printk(KERN_ERR "audit: (%s) error: "	\
		fmt, audit_point_name[kvm->arch.audit_point], ##args)

typedef void (*inspect_spte_fn) (struct kvm_vcpu *vcpu, u64 *sptep, int level);

static void __mmu_spte_walk(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp,
			    inspect_spte_fn fn, int level)
{
	int i;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
		u64 *ent = sp->spt;

		fn(vcpu, ent + i, level);

		if (is_shadow_present_pte(ent[i]) &&
		      !is_last_spte(ent[i], level)) {
			struct kvm_mmu_page *child;

			child = page_header(ent[i] & PT64_BASE_ADDR_MASK);
			__mmu_spte_walk(vcpu, child, fn, level - 1);
		}
	}
}

static void mmu_spte_walk(struct kvm_vcpu *vcpu, inspect_spte_fn fn)
{
	int i;
	struct kvm_mmu_page *sp;

	if (!VALID_PAGE(vcpu->arch.mmu.root_hpa))
		return;

	if (vcpu->arch.mmu.root_level == PT64_ROOT_LEVEL) {
		hpa_t root = vcpu->arch.mmu.root_hpa;

		sp = page_header(root);
		__mmu_spte_walk(vcpu, sp, fn, PT64_ROOT_LEVEL);
		return;
	}

	for (i = 0; i < 4; ++i) {
		hpa_t root = vcpu->arch.mmu.pae_root[i];

		if (root && VALID_PAGE(root)) {
			root &= PT64_BASE_ADDR_MASK;
			sp = page_header(root);
			__mmu_spte_walk(vcpu, sp, fn, 2);
		}
	}

	return;
}

typedef void (*sp_handler) (struct kvm *kvm, struct kvm_mmu_page *sp);

static void walk_all_active_sps(struct kvm *kvm, sp_handler fn)
{
	struct kvm_mmu_page *sp;

	list_for_each_entry(sp, &kvm->arch.active_mmu_pages, link)
		fn(kvm, sp);
}

static void audit_mappings(struct kvm_vcpu *vcpu, u64 *sptep, int level)
{
	struct kvm_mmu_page *sp;
	gfn_t gfn;
	pfn_t pfn;
	hpa_t hpa;

	sp = page_header(__pa(sptep));

	if (sp->unsync) {
		if (level != PT_PAGE_TABLE_LEVEL) {
			audit_printk(vcpu->kvm, "unsync sp: %p "
				     "level = %d\n", sp, level);
			return;
		}

		if (*sptep == shadow_notrap_nonpresent_pte) {
			audit_printk(vcpu->kvm, "notrap spte in unsync "
				     "sp: %p\n", sp);
			return;
		}
	}

	if (sp->role.direct && *sptep == shadow_notrap_nonpresent_pte) {
		audit_printk(vcpu->kvm, "notrap spte in direct sp: %p\n",
			     sp);
		return;
	}

	if (!is_shadow_present_pte(*sptep) || !is_last_spte(*sptep, level))
		return;

	gfn = kvm_mmu_page_get_gfn(sp, sptep - sp->spt);
	pfn = gfn_to_pfn_atomic(vcpu->kvm, gfn);

	if (is_error_pfn(pfn)) {
		kvm_release_pfn_clean(pfn);
		return;
	}

	hpa =  pfn << PAGE_SHIFT;
	if ((*sptep & PT64_BASE_ADDR_MASK) != hpa)
		audit_printk(vcpu->kvm, "levels %d pfn %llx hpa %llx "
			     "ent %llxn", vcpu->arch.mmu.root_level, pfn,
			     hpa, *sptep);
}

static void inspect_spte_has_rmap(struct kvm *kvm, u64 *sptep)
{
	unsigned long *rmapp;
	struct kvm_mmu_page *rev_sp;
	gfn_t gfn;


	rev_sp = page_header(__pa(sptep));
	gfn = kvm_mmu_page_get_gfn(rev_sp, sptep - rev_sp->spt);

	if (!gfn_to_memslot(kvm, gfn)) {
		if (!printk_ratelimit())
			return;
		audit_printk(kvm, "no memslot for gfn %llx\n", gfn);
		audit_printk(kvm, "index %ld of sp (gfn=%llx)\n",
		       (long int)(sptep - rev_sp->spt), rev_sp->gfn);
		dump_stack();
		return;
	}

	rmapp = gfn_to_rmap(kvm, gfn, rev_sp->role.level);
	if (!*rmapp) {
		if (!printk_ratelimit())
			return;
		audit_printk(kvm, "no rmap for writable spte %llx\n",
			     *sptep);
		dump_stack();
	}
}

static void audit_sptes_have_rmaps(struct kvm_vcpu *vcpu, u64 *sptep, int level)
{
	if (is_shadow_present_pte(*sptep) && is_last_spte(*sptep, level))
		inspect_spte_has_rmap(vcpu->kvm, sptep);
}

static void audit_spte_after_sync(struct kvm_vcpu *vcpu, u64 *sptep, int level)
{
	struct kvm_mmu_page *sp = page_header(__pa(sptep));

	if (vcpu->kvm->arch.audit_point == AUDIT_POST_SYNC && sp->unsync)
		audit_printk(vcpu->kvm, "meet unsync sp(%p) after sync "
			     "root.\n", sp);
}

static void check_mappings_rmap(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	int i;

	if (sp->role.level != PT_PAGE_TABLE_LEVEL)
		return;

	for (i = 0; i < PT64_ENT_PER_PAGE; ++i) {
		if (!is_rmap_spte(sp->spt[i]))
			continue;

		inspect_spte_has_rmap(kvm, sp->spt + i);
	}
}

static void audit_write_protection(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	struct kvm_memory_slot *slot;
	unsigned long *rmapp;
	u64 *spte;

	if (sp->role.direct || sp->unsync || sp->role.invalid)
		return;

	slot = gfn_to_memslot(kvm, sp->gfn);
	rmapp = &slot->rmap[sp->gfn - slot->base_gfn];

	spte = rmap_next(kvm, rmapp, NULL);
	while (spte) {
		if (is_writable_pte(*spte))
			audit_printk(kvm, "shadow page has writable "
				     "mappings: gfn %llx role %x\n",
				     sp->gfn, sp->role.word);
		spte = rmap_next(kvm, rmapp, spte);
	}
}

static void audit_sp(struct kvm *kvm, struct kvm_mmu_page *sp)
{
	check_mappings_rmap(kvm, sp);
	audit_write_protection(kvm, sp);
}

static void audit_all_active_sps(struct kvm *kvm)
{
	walk_all_active_sps(kvm, audit_sp);
}

static void audit_spte(struct kvm_vcpu *vcpu, u64 *sptep, int level)
{
	audit_sptes_have_rmaps(vcpu, sptep, level);
	audit_mappings(vcpu, sptep, level);
	audit_spte_after_sync(vcpu, sptep, level);
}

static void audit_vcpu_spte(struct kvm_vcpu *vcpu)
{
	mmu_spte_walk(vcpu, audit_spte);
}

static void kvm_mmu_audit(void *ignore, struct kvm_vcpu *vcpu, int point)
{
	static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 10);

	if (!__ratelimit(&ratelimit_state))
		return;

	vcpu->kvm->arch.audit_point = point;
	audit_all_active_sps(vcpu->kvm);
	audit_vcpu_spte(vcpu);
}

static bool mmu_audit;

static void mmu_audit_enable(void)
{
	int ret;

	if (mmu_audit)
		return;

	ret = register_trace_kvm_mmu_audit(kvm_mmu_audit, NULL);
	WARN_ON(ret);

	mmu_audit = true;
}

static void mmu_audit_disable(void)
{
	if (!mmu_audit)
		return;

	unregister_trace_kvm_mmu_audit(kvm_mmu_audit, NULL);
	tracepoint_synchronize_unregister();
	mmu_audit = false;
}

static int mmu_audit_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned long enable;

	ret = strict_strtoul(val, 10, &enable);
	if (ret < 0)
		return -EINVAL;

	switch (enable) {
	case 0:
		mmu_audit_disable();
		break;
	case 1:
		mmu_audit_enable();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct kernel_param_ops audit_param_ops = {
	.set = mmu_audit_set,
	.get = param_get_bool,
};

module_param_cb(mmu_audit, &audit_param_ops, &mmu_audit, 0644);
