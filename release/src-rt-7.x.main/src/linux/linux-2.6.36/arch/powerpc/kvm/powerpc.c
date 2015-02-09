/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright IBM Corp. 2007
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 *          Christian Ehrhardt <ehrhardt@linux.vnet.ibm.com>
 */

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kvm_host.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/cputable.h>
#include <asm/uaccess.h>
#include <asm/kvm_ppc.h>
#include <asm/tlbflush.h>
#include "timing.h"
#include "../mm/mmu_decl.h"

#define CREATE_TRACE_POINTS
#include "trace.h"

int kvm_arch_vcpu_runnable(struct kvm_vcpu *v)
{
	return !(v->arch.msr & MSR_WE) || !!(v->arch.pending_exceptions);
}


int kvmppc_emulate_mmio(struct kvm_run *run, struct kvm_vcpu *vcpu)
{
	enum emulation_result er;
	int r;

	er = kvmppc_emulate_instruction(run, vcpu);
	switch (er) {
	case EMULATE_DONE:
		/* Future optimization: only reload non-volatiles if they were
		 * actually modified. */
		r = RESUME_GUEST_NV;
		break;
	case EMULATE_DO_MMIO:
		run->exit_reason = KVM_EXIT_MMIO;
		/* We must reload nonvolatiles because "update" load/store
		 * instructions modify register state. */
		/* Future optimization: only reload non-volatiles if they were
		 * actually modified. */
		r = RESUME_HOST_NV;
		break;
	case EMULATE_FAIL:
		printk(KERN_EMERG "%s: emulation failed (%08x)\n", __func__,
		       kvmppc_get_last_inst(vcpu));
		r = RESUME_HOST;
		break;
	default:
		BUG();
	}

	return r;
}

int kvm_arch_hardware_enable(void *garbage)
{
	return 0;
}

void kvm_arch_hardware_disable(void *garbage)
{
}

int kvm_arch_hardware_setup(void)
{
	return 0;
}

void kvm_arch_hardware_unsetup(void)
{
}

void kvm_arch_check_processor_compat(void *rtn)
{
	*(int *)rtn = kvmppc_core_check_processor_compat();
}

struct kvm *kvm_arch_create_vm(void)
{
	struct kvm *kvm;

	kvm = kzalloc(sizeof(struct kvm), GFP_KERNEL);
	if (!kvm)
		return ERR_PTR(-ENOMEM);

	return kvm;
}

static void kvmppc_free_vcpus(struct kvm *kvm)
{
	unsigned int i;
	struct kvm_vcpu *vcpu;

	kvm_for_each_vcpu(i, vcpu, kvm)
		kvm_arch_vcpu_free(vcpu);

	mutex_lock(&kvm->lock);
	for (i = 0; i < atomic_read(&kvm->online_vcpus); i++)
		kvm->vcpus[i] = NULL;

	atomic_set(&kvm->online_vcpus, 0);
	mutex_unlock(&kvm->lock);
}

void kvm_arch_sync_events(struct kvm *kvm)
{
}

void kvm_arch_destroy_vm(struct kvm *kvm)
{
	kvmppc_free_vcpus(kvm);
	kvm_free_physmem(kvm);
	cleanup_srcu_struct(&kvm->srcu);
	kfree(kvm);
}

int kvm_dev_ioctl_check_extension(long ext)
{
	int r;

	switch (ext) {
	case KVM_CAP_PPC_SEGSTATE:
	case KVM_CAP_PPC_PAIRED_SINGLES:
	case KVM_CAP_PPC_UNSET_IRQ:
	case KVM_CAP_ENABLE_CAP:
	case KVM_CAP_PPC_OSI:
		r = 1;
		break;
	case KVM_CAP_COALESCED_MMIO:
		r = KVM_COALESCED_MMIO_PAGE_OFFSET;
		break;
	default:
		r = 0;
		break;
	}
	return r;

}

long kvm_arch_dev_ioctl(struct file *filp,
                        unsigned int ioctl, unsigned long arg)
{
	return -EINVAL;
}

int kvm_arch_prepare_memory_region(struct kvm *kvm,
                                   struct kvm_memory_slot *memslot,
                                   struct kvm_memory_slot old,
                                   struct kvm_userspace_memory_region *mem,
                                   int user_alloc)
{
	return 0;
}

void kvm_arch_commit_memory_region(struct kvm *kvm,
               struct kvm_userspace_memory_region *mem,
               struct kvm_memory_slot old,
               int user_alloc)
{
       return;
}


void kvm_arch_flush_shadow(struct kvm *kvm)
{
}

struct kvm_vcpu *kvm_arch_vcpu_create(struct kvm *kvm, unsigned int id)
{
	struct kvm_vcpu *vcpu;
	vcpu = kvmppc_core_vcpu_create(kvm, id);
	if (!IS_ERR(vcpu))
		kvmppc_create_vcpu_debugfs(vcpu, id);
	return vcpu;
}

void kvm_arch_vcpu_free(struct kvm_vcpu *vcpu)
{
	/* Make sure we're not using the vcpu anymore */
	hrtimer_cancel(&vcpu->arch.dec_timer);
	tasklet_kill(&vcpu->arch.tasklet);

	kvmppc_remove_vcpu_debugfs(vcpu);
	kvmppc_core_vcpu_free(vcpu);
}

void kvm_arch_vcpu_destroy(struct kvm_vcpu *vcpu)
{
	kvm_arch_vcpu_free(vcpu);
}

int kvm_cpu_has_pending_timer(struct kvm_vcpu *vcpu)
{
	return kvmppc_core_pending_dec(vcpu);
}

static void kvmppc_decrementer_func(unsigned long data)
{
	struct kvm_vcpu *vcpu = (struct kvm_vcpu *)data;

	kvmppc_core_queue_dec(vcpu);

	if (waitqueue_active(&vcpu->wq)) {
		wake_up_interruptible(&vcpu->wq);
		vcpu->stat.halt_wakeup++;
	}
}

/*
 * low level hrtimer wake routine. Because this runs in hardirq context
 * we schedule a tasklet to do the real work.
 */
enum hrtimer_restart kvmppc_decrementer_wakeup(struct hrtimer *timer)
{
	struct kvm_vcpu *vcpu;

	vcpu = container_of(timer, struct kvm_vcpu, arch.dec_timer);
	tasklet_schedule(&vcpu->arch.tasklet);

	return HRTIMER_NORESTART;
}

int kvm_arch_vcpu_init(struct kvm_vcpu *vcpu)
{
	hrtimer_init(&vcpu->arch.dec_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	tasklet_init(&vcpu->arch.tasklet, kvmppc_decrementer_func, (ulong)vcpu);
	vcpu->arch.dec_timer.function = kvmppc_decrementer_wakeup;

	return 0;
}

void kvm_arch_vcpu_uninit(struct kvm_vcpu *vcpu)
{
	kvmppc_mmu_destroy(vcpu);
}

void kvm_arch_vcpu_load(struct kvm_vcpu *vcpu, int cpu)
{
	kvmppc_core_vcpu_load(vcpu, cpu);
}

void kvm_arch_vcpu_put(struct kvm_vcpu *vcpu)
{
	kvmppc_core_vcpu_put(vcpu);
}

int kvm_arch_vcpu_ioctl_set_guest_debug(struct kvm_vcpu *vcpu,
                                        struct kvm_guest_debug *dbg)
{
	return -EINVAL;
}

static void kvmppc_complete_dcr_load(struct kvm_vcpu *vcpu,
                                     struct kvm_run *run)
{
	kvmppc_set_gpr(vcpu, vcpu->arch.io_gpr, run->dcr.data);
}

static void kvmppc_complete_mmio_load(struct kvm_vcpu *vcpu,
                                      struct kvm_run *run)
{
	u64 uninitialized_var(gpr);

	if (run->mmio.len > sizeof(gpr)) {
		printk(KERN_ERR "bad MMIO length: %d\n", run->mmio.len);
		return;
	}

	if (vcpu->arch.mmio_is_bigendian) {
		switch (run->mmio.len) {
		case 8: gpr = *(u64 *)run->mmio.data; break;
		case 4: gpr = *(u32 *)run->mmio.data; break;
		case 2: gpr = *(u16 *)run->mmio.data; break;
		case 1: gpr = *(u8 *)run->mmio.data; break;
		}
	} else {
		/* Convert BE data from userland back to LE. */
		switch (run->mmio.len) {
		case 4: gpr = ld_le32((u32 *)run->mmio.data); break;
		case 2: gpr = ld_le16((u16 *)run->mmio.data); break;
		case 1: gpr = *(u8 *)run->mmio.data; break;
		}
	}

	if (vcpu->arch.mmio_sign_extend) {
		switch (run->mmio.len) {
#ifdef CONFIG_PPC64
		case 4:
			gpr = (s64)(s32)gpr;
			break;
#endif
		case 2:
			gpr = (s64)(s16)gpr;
			break;
		case 1:
			gpr = (s64)(s8)gpr;
			break;
		}
	}

	kvmppc_set_gpr(vcpu, vcpu->arch.io_gpr, gpr);

	switch (vcpu->arch.io_gpr & KVM_REG_EXT_MASK) {
	case KVM_REG_GPR:
		kvmppc_set_gpr(vcpu, vcpu->arch.io_gpr, gpr);
		break;
	case KVM_REG_FPR:
		vcpu->arch.fpr[vcpu->arch.io_gpr & KVM_REG_MASK] = gpr;
		break;
#ifdef CONFIG_PPC_BOOK3S
	case KVM_REG_QPR:
		vcpu->arch.qpr[vcpu->arch.io_gpr & KVM_REG_MASK] = gpr;
		break;
	case KVM_REG_FQPR:
		vcpu->arch.fpr[vcpu->arch.io_gpr & KVM_REG_MASK] = gpr;
		vcpu->arch.qpr[vcpu->arch.io_gpr & KVM_REG_MASK] = gpr;
		break;
#endif
	default:
		BUG();
	}
}

int kvmppc_handle_load(struct kvm_run *run, struct kvm_vcpu *vcpu,
                       unsigned int rt, unsigned int bytes, int is_bigendian)
{
	if (bytes > sizeof(run->mmio.data)) {
		printk(KERN_ERR "%s: bad MMIO length: %d\n", __func__,
		       run->mmio.len);
	}

	run->mmio.phys_addr = vcpu->arch.paddr_accessed;
	run->mmio.len = bytes;
	run->mmio.is_write = 0;

	vcpu->arch.io_gpr = rt;
	vcpu->arch.mmio_is_bigendian = is_bigendian;
	vcpu->mmio_needed = 1;
	vcpu->mmio_is_write = 0;
	vcpu->arch.mmio_sign_extend = 0;

	return EMULATE_DO_MMIO;
}

/* Same as above, but sign extends */
int kvmppc_handle_loads(struct kvm_run *run, struct kvm_vcpu *vcpu,
                        unsigned int rt, unsigned int bytes, int is_bigendian)
{
	int r;

	r = kvmppc_handle_load(run, vcpu, rt, bytes, is_bigendian);
	vcpu->arch.mmio_sign_extend = 1;

	return r;
}

int kvmppc_handle_store(struct kvm_run *run, struct kvm_vcpu *vcpu,
                        u64 val, unsigned int bytes, int is_bigendian)
{
	void *data = run->mmio.data;

	if (bytes > sizeof(run->mmio.data)) {
		printk(KERN_ERR "%s: bad MMIO length: %d\n", __func__,
		       run->mmio.len);
	}

	run->mmio.phys_addr = vcpu->arch.paddr_accessed;
	run->mmio.len = bytes;
	run->mmio.is_write = 1;
	vcpu->mmio_needed = 1;
	vcpu->mmio_is_write = 1;

	/* Store the value at the lowest bytes in 'data'. */
	if (is_bigendian) {
		switch (bytes) {
		case 8: *(u64 *)data = val; break;
		case 4: *(u32 *)data = val; break;
		case 2: *(u16 *)data = val; break;
		case 1: *(u8  *)data = val; break;
		}
	} else {
		/* Store LE value into 'data'. */
		switch (bytes) {
		case 4: st_le32(data, val); break;
		case 2: st_le16(data, val); break;
		case 1: *(u8 *)data = val; break;
		}
	}

	return EMULATE_DO_MMIO;
}

int kvm_arch_vcpu_ioctl_run(struct kvm_vcpu *vcpu, struct kvm_run *run)
{
	int r;
	sigset_t sigsaved;

	if (vcpu->sigset_active)
		sigprocmask(SIG_SETMASK, &vcpu->sigset, &sigsaved);

	if (vcpu->mmio_needed) {
		if (!vcpu->mmio_is_write)
			kvmppc_complete_mmio_load(vcpu, run);
		vcpu->mmio_needed = 0;
	} else if (vcpu->arch.dcr_needed) {
		if (!vcpu->arch.dcr_is_write)
			kvmppc_complete_dcr_load(vcpu, run);
		vcpu->arch.dcr_needed = 0;
	} else if (vcpu->arch.osi_needed) {
		u64 *gprs = run->osi.gprs;
		int i;

		for (i = 0; i < 32; i++)
			kvmppc_set_gpr(vcpu, i, gprs[i]);
		vcpu->arch.osi_needed = 0;
	}

	kvmppc_core_deliver_interrupts(vcpu);

	local_irq_disable();
	kvm_guest_enter();
	r = __kvmppc_vcpu_run(run, vcpu);
	kvm_guest_exit();
	local_irq_enable();

	if (vcpu->sigset_active)
		sigprocmask(SIG_SETMASK, &sigsaved, NULL);

	return r;
}

int kvm_vcpu_ioctl_interrupt(struct kvm_vcpu *vcpu, struct kvm_interrupt *irq)
{
	if (irq->irq == KVM_INTERRUPT_UNSET)
		kvmppc_core_dequeue_external(vcpu, irq);
	else
		kvmppc_core_queue_external(vcpu, irq);

	if (waitqueue_active(&vcpu->wq)) {
		wake_up_interruptible(&vcpu->wq);
		vcpu->stat.halt_wakeup++;
	}

	return 0;
}

static int kvm_vcpu_ioctl_enable_cap(struct kvm_vcpu *vcpu,
				     struct kvm_enable_cap *cap)
{
	int r;

	if (cap->flags)
		return -EINVAL;

	switch (cap->cap) {
	case KVM_CAP_PPC_OSI:
		r = 0;
		vcpu->arch.osi_enabled = true;
		break;
	default:
		r = -EINVAL;
		break;
	}

	return r;
}

int kvm_arch_vcpu_ioctl_get_mpstate(struct kvm_vcpu *vcpu,
                                    struct kvm_mp_state *mp_state)
{
	return -EINVAL;
}

int kvm_arch_vcpu_ioctl_set_mpstate(struct kvm_vcpu *vcpu,
                                    struct kvm_mp_state *mp_state)
{
	return -EINVAL;
}

long kvm_arch_vcpu_ioctl(struct file *filp,
                         unsigned int ioctl, unsigned long arg)
{
	struct kvm_vcpu *vcpu = filp->private_data;
	void __user *argp = (void __user *)arg;
	long r;

	switch (ioctl) {
	case KVM_INTERRUPT: {
		struct kvm_interrupt irq;
		r = -EFAULT;
		if (copy_from_user(&irq, argp, sizeof(irq)))
			goto out;
		r = kvm_vcpu_ioctl_interrupt(vcpu, &irq);
		goto out;
	}

	case KVM_ENABLE_CAP:
	{
		struct kvm_enable_cap cap;
		r = -EFAULT;
		if (copy_from_user(&cap, argp, sizeof(cap)))
			goto out;
		r = kvm_vcpu_ioctl_enable_cap(vcpu, &cap);
		break;
	}
	default:
		r = -EINVAL;
	}

out:
	return r;
}

long kvm_arch_vm_ioctl(struct file *filp,
                       unsigned int ioctl, unsigned long arg)
{
	long r;

	switch (ioctl) {
	default:
		r = -ENOTTY;
	}

	return r;
}

int kvm_arch_init(void *opaque)
{
	return 0;
}

void kvm_arch_exit(void)
{
}
