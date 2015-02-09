/* irq.c: UltraSparc IRQ handling/init/registry.
 *
 * Copyright (C) 1997, 2007, 2008 David S. Miller (davem@davemloft.net)
 * Copyright (C) 1998  Eddie C. Dost    (ecd@skynet.be)
 * Copyright (C) 1998  Jakub Jelinek    (jj@ultra.linux.cz)
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/linkage.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ftrace.h>
#include <linux/irq.h>
#include <linux/kmemleak.h>

#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/iommu.h>
#include <asm/upa.h>
#include <asm/oplib.h>
#include <asm/prom.h>
#include <asm/timer.h>
#include <asm/smp.h>
#include <asm/starfire.h>
#include <asm/uaccess.h>
#include <asm/cache.h>
#include <asm/cpudata.h>
#include <asm/auxio.h>
#include <asm/head.h>
#include <asm/hypervisor.h>
#include <asm/cacheflush.h>

#include "entry.h"
#include "cpumap.h"
#include "kstack.h"

#define NUM_IVECS	(IMAP_INR + 1)

struct ino_bucket *ivector_table;
unsigned long ivector_table_pa;

/* On several sun4u processors, it is illegal to mix bypass and
 * non-bypass accesses.  Therefore we access all INO buckets
 * using bypass accesses only.
 */
static unsigned long bucket_get_chain_pa(unsigned long bucket_pa)
{
	unsigned long ret;

	__asm__ __volatile__("ldxa	[%1] %2, %0"
			     : "=&r" (ret)
			     : "r" (bucket_pa +
				    offsetof(struct ino_bucket,
					     __irq_chain_pa)),
			       "i" (ASI_PHYS_USE_EC));

	return ret;
}

static void bucket_clear_chain_pa(unsigned long bucket_pa)
{
	__asm__ __volatile__("stxa	%%g0, [%0] %1"
			     : /* no outputs */
			     : "r" (bucket_pa +
				    offsetof(struct ino_bucket,
					     __irq_chain_pa)),
			       "i" (ASI_PHYS_USE_EC));
}

static unsigned int bucket_get_virt_irq(unsigned long bucket_pa)
{
	unsigned int ret;

	__asm__ __volatile__("lduwa	[%1] %2, %0"
			     : "=&r" (ret)
			     : "r" (bucket_pa +
				    offsetof(struct ino_bucket,
					     __virt_irq)),
			       "i" (ASI_PHYS_USE_EC));

	return ret;
}

static void bucket_set_virt_irq(unsigned long bucket_pa,
				unsigned int virt_irq)
{
	__asm__ __volatile__("stwa	%0, [%1] %2"
			     : /* no outputs */
			     : "r" (virt_irq),
			       "r" (bucket_pa +
				    offsetof(struct ino_bucket,
					     __virt_irq)),
			       "i" (ASI_PHYS_USE_EC));
}

#define irq_work_pa(__cpu)	&(trap_block[(__cpu)].irq_worklist_pa)

static struct {
	unsigned int dev_handle;
	unsigned int dev_ino;
	unsigned int in_use;
} virt_irq_table[NR_IRQS];
static DEFINE_SPINLOCK(virt_irq_alloc_lock);

unsigned char virt_irq_alloc(unsigned int dev_handle,
			     unsigned int dev_ino)
{
	unsigned long flags;
	unsigned char ent;

	BUILD_BUG_ON(NR_IRQS >= 256);

	spin_lock_irqsave(&virt_irq_alloc_lock, flags);

	for (ent = 1; ent < NR_IRQS; ent++) {
		if (!virt_irq_table[ent].in_use)
			break;
	}
	if (ent >= NR_IRQS) {
		printk(KERN_ERR "IRQ: Out of virtual IRQs.\n");
		ent = 0;
	} else {
		virt_irq_table[ent].dev_handle = dev_handle;
		virt_irq_table[ent].dev_ino = dev_ino;
		virt_irq_table[ent].in_use = 1;
	}

	spin_unlock_irqrestore(&virt_irq_alloc_lock, flags);

	return ent;
}

#ifdef CONFIG_PCI_MSI
void virt_irq_free(unsigned int virt_irq)
{
	unsigned long flags;

	if (virt_irq >= NR_IRQS)
		return;

	spin_lock_irqsave(&virt_irq_alloc_lock, flags);

	virt_irq_table[virt_irq].in_use = 0;

	spin_unlock_irqrestore(&virt_irq_alloc_lock, flags);
}
#endif

/*
 * /proc/interrupts printing:
 */

int show_interrupts(struct seq_file *p, void *v)
{
	int i = *(loff_t *) v, j;
	struct irqaction * action;
	unsigned long flags;

	if (i == 0) {
		seq_printf(p, "           ");
		for_each_online_cpu(j)
			seq_printf(p, "CPU%d       ",j);
		seq_putc(p, '\n');
	}

	if (i < NR_IRQS) {
		raw_spin_lock_irqsave(&irq_desc[i].lock, flags);
		action = irq_desc[i].action;
		if (!action)
			goto skip;
		seq_printf(p, "%3d: ",i);
#ifndef CONFIG_SMP
		seq_printf(p, "%10u ", kstat_irqs(i));
#else
		for_each_online_cpu(j)
			seq_printf(p, "%10u ", kstat_irqs_cpu(i, j));
#endif
		seq_printf(p, " %9s", irq_desc[i].chip->name);
		seq_printf(p, "  %s", action->name);

		for (action=action->next; action; action = action->next)
			seq_printf(p, ", %s", action->name);

		seq_putc(p, '\n');
skip:
		raw_spin_unlock_irqrestore(&irq_desc[i].lock, flags);
	} else if (i == NR_IRQS) {
		seq_printf(p, "NMI: ");
		for_each_online_cpu(j)
			seq_printf(p, "%10u ", cpu_data(j).__nmi_count);
		seq_printf(p, "     Non-maskable interrupts\n");
	}
	return 0;
}

static unsigned int sun4u_compute_tid(unsigned long imap, unsigned long cpuid)
{
	unsigned int tid;

	if (this_is_starfire) {
		tid = starfire_translate(imap, cpuid);
		tid <<= IMAP_TID_SHIFT;
		tid &= IMAP_TID_UPA;
	} else {
		if (tlb_type == cheetah || tlb_type == cheetah_plus) {
			unsigned long ver;

			__asm__ ("rdpr %%ver, %0" : "=r" (ver));
			if ((ver >> 32UL) == __JALAPENO_ID ||
			    (ver >> 32UL) == __SERRANO_ID) {
				tid = cpuid << IMAP_TID_SHIFT;
				tid &= IMAP_TID_JBUS;
			} else {
				unsigned int a = cpuid & 0x1f;
				unsigned int n = (cpuid >> 5) & 0x1f;

				tid = ((a << IMAP_AID_SHIFT) |
				       (n << IMAP_NID_SHIFT));
				tid &= (IMAP_AID_SAFARI |
					IMAP_NID_SAFARI);
			}
		} else {
			tid = cpuid << IMAP_TID_SHIFT;
			tid &= IMAP_TID_UPA;
		}
	}

	return tid;
}

struct irq_handler_data {
	unsigned long	iclr;
	unsigned long	imap;

	void		(*pre_handler)(unsigned int, void *, void *);
	void		*arg1;
	void		*arg2;
};

#ifdef CONFIG_SMP
static int irq_choose_cpu(unsigned int virt_irq, const struct cpumask *affinity)
{
	cpumask_t mask;
	int cpuid;

	cpumask_copy(&mask, affinity);
	if (cpus_equal(mask, cpu_online_map)) {
		cpuid = map_to_cpu(virt_irq);
	} else {
		cpumask_t tmp;

		cpus_and(tmp, cpu_online_map, mask);
		cpuid = cpus_empty(tmp) ? map_to_cpu(virt_irq) : first_cpu(tmp);
	}

	return cpuid;
}
#else
#define irq_choose_cpu(virt_irq, affinity)	\
	real_hard_smp_processor_id()
#endif

static void sun4u_irq_enable(unsigned int virt_irq)
{
	struct irq_handler_data *data = get_irq_chip_data(virt_irq);

	if (likely(data)) {
		unsigned long cpuid, imap, val;
		unsigned int tid;

		cpuid = irq_choose_cpu(virt_irq,
				       irq_desc[virt_irq].affinity);
		imap = data->imap;

		tid = sun4u_compute_tid(imap, cpuid);

		val = upa_readq(imap);
		val &= ~(IMAP_TID_UPA | IMAP_TID_JBUS |
			 IMAP_AID_SAFARI | IMAP_NID_SAFARI);
		val |= tid | IMAP_VALID;
		upa_writeq(val, imap);
		upa_writeq(ICLR_IDLE, data->iclr);
	}
}

static int sun4u_set_affinity(unsigned int virt_irq,
			       const struct cpumask *mask)
{
	struct irq_handler_data *data = get_irq_chip_data(virt_irq);

	if (likely(data)) {
		unsigned long cpuid, imap, val;
		unsigned int tid;

		cpuid = irq_choose_cpu(virt_irq, mask);
		imap = data->imap;

		tid = sun4u_compute_tid(imap, cpuid);

		val = upa_readq(imap);
		val &= ~(IMAP_TID_UPA | IMAP_TID_JBUS |
			 IMAP_AID_SAFARI | IMAP_NID_SAFARI);
		val |= tid | IMAP_VALID;
		upa_writeq(val, imap);
		upa_writeq(ICLR_IDLE, data->iclr);
	}

	return 0;
}

/* Don't do anything.  The desc->status check for IRQ_DISABLED in
 * handler_irq() will skip the handler call and that will leave the
 * interrupt in the sent state.  The next ->enable() call will hit the
 * ICLR register to reset the state machine.
 *
 * This scheme is necessary, instead of clearing the Valid bit in the
 * IMAP register, to handle the case of IMAP registers being shared by
 * multiple INOs (and thus ICLR registers).  Since we use a different
 * virtual IRQ for each shared IMAP instance, the generic code thinks
 * there is only one user so it prematurely calls ->disable() on
 * free_irq().
 *
 * We have to provide an explicit ->disable() method instead of using
 * NULL to get the default.  The reason is that if the generic code
 * sees that, it also hooks up a default ->shutdown method which
 * invokes ->mask() which we do not want.  See irq_chip_set_defaults().
 */
static void sun4u_irq_disable(unsigned int virt_irq)
{
}

static void sun4u_irq_eoi(unsigned int virt_irq)
{
	struct irq_handler_data *data = get_irq_chip_data(virt_irq);
	struct irq_desc *desc = irq_desc + virt_irq;

	if (unlikely(desc->status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		return;

	if (likely(data))
		upa_writeq(ICLR_IDLE, data->iclr);
}

static void sun4v_irq_enable(unsigned int virt_irq)
{
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;
	unsigned long cpuid = irq_choose_cpu(virt_irq,
					     irq_desc[virt_irq].affinity);
	int err;

	err = sun4v_intr_settarget(ino, cpuid);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_settarget(%x,%lu): "
		       "err(%d)\n", ino, cpuid, err);
	err = sun4v_intr_setstate(ino, HV_INTR_STATE_IDLE);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_setstate(%x): "
		       "err(%d)\n", ino, err);
	err = sun4v_intr_setenabled(ino, HV_INTR_ENABLED);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_setenabled(%x): err(%d)\n",
		       ino, err);
}

static int sun4v_set_affinity(unsigned int virt_irq,
			       const struct cpumask *mask)
{
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;
	unsigned long cpuid = irq_choose_cpu(virt_irq, mask);
	int err;

	err = sun4v_intr_settarget(ino, cpuid);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_settarget(%x,%lu): "
		       "err(%d)\n", ino, cpuid, err);

	return 0;
}

static void sun4v_irq_disable(unsigned int virt_irq)
{
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;
	int err;

	err = sun4v_intr_setenabled(ino, HV_INTR_DISABLED);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_setenabled(%x): "
		       "err(%d)\n", ino, err);
}

static void sun4v_irq_eoi(unsigned int virt_irq)
{
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;
	struct irq_desc *desc = irq_desc + virt_irq;
	int err;

	if (unlikely(desc->status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		return;

	err = sun4v_intr_setstate(ino, HV_INTR_STATE_IDLE);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_intr_setstate(%x): "
		       "err(%d)\n", ino, err);
}

static void sun4v_virq_enable(unsigned int virt_irq)
{
	unsigned long cpuid, dev_handle, dev_ino;
	int err;

	cpuid = irq_choose_cpu(virt_irq, irq_desc[virt_irq].affinity);

	dev_handle = virt_irq_table[virt_irq].dev_handle;
	dev_ino = virt_irq_table[virt_irq].dev_ino;

	err = sun4v_vintr_set_target(dev_handle, dev_ino, cpuid);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_target(%lx,%lx,%lu): "
		       "err(%d)\n",
		       dev_handle, dev_ino, cpuid, err);
	err = sun4v_vintr_set_state(dev_handle, dev_ino,
				    HV_INTR_STATE_IDLE);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_state(%lx,%lx,"
		       "HV_INTR_STATE_IDLE): err(%d)\n",
		       dev_handle, dev_ino, err);
	err = sun4v_vintr_set_valid(dev_handle, dev_ino,
				    HV_INTR_ENABLED);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_state(%lx,%lx,"
		       "HV_INTR_ENABLED): err(%d)\n",
		       dev_handle, dev_ino, err);
}

static int sun4v_virt_set_affinity(unsigned int virt_irq,
				    const struct cpumask *mask)
{
	unsigned long cpuid, dev_handle, dev_ino;
	int err;

	cpuid = irq_choose_cpu(virt_irq, mask);

	dev_handle = virt_irq_table[virt_irq].dev_handle;
	dev_ino = virt_irq_table[virt_irq].dev_ino;

	err = sun4v_vintr_set_target(dev_handle, dev_ino, cpuid);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_target(%lx,%lx,%lu): "
		       "err(%d)\n",
		       dev_handle, dev_ino, cpuid, err);

	return 0;
}

static void sun4v_virq_disable(unsigned int virt_irq)
{
	unsigned long dev_handle, dev_ino;
	int err;

	dev_handle = virt_irq_table[virt_irq].dev_handle;
	dev_ino = virt_irq_table[virt_irq].dev_ino;

	err = sun4v_vintr_set_valid(dev_handle, dev_ino,
				    HV_INTR_DISABLED);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_state(%lx,%lx,"
		       "HV_INTR_DISABLED): err(%d)\n",
		       dev_handle, dev_ino, err);
}

static void sun4v_virq_eoi(unsigned int virt_irq)
{
	struct irq_desc *desc = irq_desc + virt_irq;
	unsigned long dev_handle, dev_ino;
	int err;

	if (unlikely(desc->status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		return;

	dev_handle = virt_irq_table[virt_irq].dev_handle;
	dev_ino = virt_irq_table[virt_irq].dev_ino;

	err = sun4v_vintr_set_state(dev_handle, dev_ino,
				    HV_INTR_STATE_IDLE);
	if (err != HV_EOK)
		printk(KERN_ERR "sun4v_vintr_set_state(%lx,%lx,"
		       "HV_INTR_STATE_IDLE): err(%d)\n",
		       dev_handle, dev_ino, err);
}

static struct irq_chip sun4u_irq = {
	.name		= "sun4u",
	.enable		= sun4u_irq_enable,
	.disable	= sun4u_irq_disable,
	.eoi		= sun4u_irq_eoi,
	.set_affinity	= sun4u_set_affinity,
};

static struct irq_chip sun4v_irq = {
	.name		= "sun4v",
	.enable		= sun4v_irq_enable,
	.disable	= sun4v_irq_disable,
	.eoi		= sun4v_irq_eoi,
	.set_affinity	= sun4v_set_affinity,
};

static struct irq_chip sun4v_virq = {
	.name		= "vsun4v",
	.enable		= sun4v_virq_enable,
	.disable	= sun4v_virq_disable,
	.eoi		= sun4v_virq_eoi,
	.set_affinity	= sun4v_virt_set_affinity,
};

static void pre_flow_handler(unsigned int virt_irq,
				      struct irq_desc *desc)
{
	struct irq_handler_data *data = get_irq_chip_data(virt_irq);
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;

	data->pre_handler(ino, data->arg1, data->arg2);

	handle_fasteoi_irq(virt_irq, desc);
}

void irq_install_pre_handler(int virt_irq,
			     void (*func)(unsigned int, void *, void *),
			     void *arg1, void *arg2)
{
	struct irq_handler_data *data = get_irq_chip_data(virt_irq);
	struct irq_desc *desc = irq_desc + virt_irq;

	data->pre_handler = func;
	data->arg1 = arg1;
	data->arg2 = arg2;

	desc->handle_irq = pre_flow_handler;
}

unsigned int build_irq(int inofixup, unsigned long iclr, unsigned long imap)
{
	struct ino_bucket *bucket;
	struct irq_handler_data *data;
	unsigned int virt_irq;
	int ino;

	BUG_ON(tlb_type == hypervisor);

	ino = (upa_readq(imap) & (IMAP_IGN | IMAP_INO)) + inofixup;
	bucket = &ivector_table[ino];
	virt_irq = bucket_get_virt_irq(__pa(bucket));
	if (!virt_irq) {
		virt_irq = virt_irq_alloc(0, ino);
		bucket_set_virt_irq(__pa(bucket), virt_irq);
		set_irq_chip_and_handler_name(virt_irq,
					      &sun4u_irq,
					      handle_fasteoi_irq,
					      "IVEC");
	}

	data = get_irq_chip_data(virt_irq);
	if (unlikely(data))
		goto out;

	data = kzalloc(sizeof(struct irq_handler_data), GFP_ATOMIC);
	if (unlikely(!data)) {
		prom_printf("IRQ: kzalloc(irq_handler_data) failed.\n");
		prom_halt();
	}
	set_irq_chip_data(virt_irq, data);

	data->imap  = imap;
	data->iclr  = iclr;

out:
	return virt_irq;
}

static unsigned int sun4v_build_common(unsigned long sysino,
				       struct irq_chip *chip)
{
	struct ino_bucket *bucket;
	struct irq_handler_data *data;
	unsigned int virt_irq;

	BUG_ON(tlb_type != hypervisor);

	bucket = &ivector_table[sysino];
	virt_irq = bucket_get_virt_irq(__pa(bucket));
	if (!virt_irq) {
		virt_irq = virt_irq_alloc(0, sysino);
		bucket_set_virt_irq(__pa(bucket), virt_irq);
		set_irq_chip_and_handler_name(virt_irq, chip,
					      handle_fasteoi_irq,
					      "IVEC");
	}

	data = get_irq_chip_data(virt_irq);
	if (unlikely(data))
		goto out;

	data = kzalloc(sizeof(struct irq_handler_data), GFP_ATOMIC);
	if (unlikely(!data)) {
		prom_printf("IRQ: kzalloc(irq_handler_data) failed.\n");
		prom_halt();
	}
	set_irq_chip_data(virt_irq, data);

	/* Catch accidental accesses to these things.  IMAP/ICLR handling
	 * is done by hypervisor calls on sun4v platforms, not by direct
	 * register accesses.
	 */
	data->imap = ~0UL;
	data->iclr = ~0UL;

out:
	return virt_irq;
}

unsigned int sun4v_build_irq(u32 devhandle, unsigned int devino)
{
	unsigned long sysino = sun4v_devino_to_sysino(devhandle, devino);

	return sun4v_build_common(sysino, &sun4v_irq);
}

unsigned int sun4v_build_virq(u32 devhandle, unsigned int devino)
{
	struct irq_handler_data *data;
	unsigned long hv_err, cookie;
	struct ino_bucket *bucket;
	struct irq_desc *desc;
	unsigned int virt_irq;

	bucket = kzalloc(sizeof(struct ino_bucket), GFP_ATOMIC);
	if (unlikely(!bucket))
		return 0;

	/* The only reference we store to the IRQ bucket is
	 * by physical address which kmemleak can't see, tell
	 * it that this object explicitly is not a leak and
	 * should be scanned.
	 */
	kmemleak_not_leak(bucket);

	__flush_dcache_range((unsigned long) bucket,
			     ((unsigned long) bucket +
			      sizeof(struct ino_bucket)));

	virt_irq = virt_irq_alloc(devhandle, devino);
	bucket_set_virt_irq(__pa(bucket), virt_irq);

	set_irq_chip_and_handler_name(virt_irq, &sun4v_virq,
				      handle_fasteoi_irq,
				      "IVEC");

	data = kzalloc(sizeof(struct irq_handler_data), GFP_ATOMIC);
	if (unlikely(!data))
		return 0;

	/* In order to make the LDC channel startup sequence easier,
	 * especially wrt. locking, we do not let request_irq() enable
	 * the interrupt.
	 */
	desc = irq_desc + virt_irq;
	desc->status |= IRQ_NOAUTOEN;

	set_irq_chip_data(virt_irq, data);

	/* Catch accidental accesses to these things.  IMAP/ICLR handling
	 * is done by hypervisor calls on sun4v platforms, not by direct
	 * register accesses.
	 */
	data->imap = ~0UL;
	data->iclr = ~0UL;

	cookie = ~__pa(bucket);
	hv_err = sun4v_vintr_set_cookie(devhandle, devino, cookie);
	if (hv_err) {
		prom_printf("IRQ: Fatal, cannot set cookie for [%x:%x] "
			    "err=%lu\n", devhandle, devino, hv_err);
		prom_halt();
	}

	return virt_irq;
}

void ack_bad_irq(unsigned int virt_irq)
{
	unsigned int ino = virt_irq_table[virt_irq].dev_ino;

	if (!ino)
		ino = 0xdeadbeef;

	printk(KERN_CRIT "Unexpected IRQ from ino[%x] virt_irq[%u]\n",
	       ino, virt_irq);
}

void *hardirq_stack[NR_CPUS];
void *softirq_stack[NR_CPUS];

void __irq_entry handler_irq(int irq, struct pt_regs *regs)
{
	unsigned long pstate, bucket_pa;
	struct pt_regs *old_regs;
	void *orig_sp;

	clear_softint(1 << irq);

	old_regs = set_irq_regs(regs);
	irq_enter();

	/* Grab an atomic snapshot of the pending IVECs.  */
	__asm__ __volatile__("rdpr	%%pstate, %0\n\t"
			     "wrpr	%0, %3, %%pstate\n\t"
			     "ldx	[%2], %1\n\t"
			     "stx	%%g0, [%2]\n\t"
			     "wrpr	%0, 0x0, %%pstate\n\t"
			     : "=&r" (pstate), "=&r" (bucket_pa)
			     : "r" (irq_work_pa(smp_processor_id())),
			       "i" (PSTATE_IE)
			     : "memory");

	orig_sp = set_hardirq_stack();

	while (bucket_pa) {
		struct irq_desc *desc;
		unsigned long next_pa;
		unsigned int virt_irq;

		next_pa = bucket_get_chain_pa(bucket_pa);
		virt_irq = bucket_get_virt_irq(bucket_pa);
		bucket_clear_chain_pa(bucket_pa);

		desc = irq_desc + virt_irq;

		if (!(desc->status & IRQ_DISABLED))
			desc->handle_irq(virt_irq, desc);

		bucket_pa = next_pa;
	}

	restore_hardirq_stack(orig_sp);

	irq_exit();
	set_irq_regs(old_regs);
}

void do_softirq(void)
{
	unsigned long flags;

	if (in_interrupt())
		return;

	local_irq_save(flags);

	if (local_softirq_pending()) {
		void *orig_sp, *sp = softirq_stack[smp_processor_id()];

		sp += THREAD_SIZE - 192 - STACK_BIAS;

		__asm__ __volatile__("mov %%sp, %0\n\t"
				     "mov %1, %%sp"
				     : "=&r" (orig_sp)
				     : "r" (sp));
		__do_softirq();
		__asm__ __volatile__("mov %0, %%sp"
				     : : "r" (orig_sp));
	}

	local_irq_restore(flags);
}

#ifdef CONFIG_HOTPLUG_CPU
void fixup_irqs(void)
{
	unsigned int irq;

	for (irq = 0; irq < NR_IRQS; irq++) {
		unsigned long flags;

		raw_spin_lock_irqsave(&irq_desc[irq].lock, flags);
		if (irq_desc[irq].action &&
		    !(irq_desc[irq].status & IRQ_PER_CPU)) {
			if (irq_desc[irq].chip->set_affinity)
				irq_desc[irq].chip->set_affinity(irq,
					irq_desc[irq].affinity);
		}
		raw_spin_unlock_irqrestore(&irq_desc[irq].lock, flags);
	}

	tick_ops->disable_irq();
}
#endif

struct sun5_timer {
	u64	count0;
	u64	limit0;
	u64	count1;
	u64	limit1;
};

static struct sun5_timer *prom_timers;
static u64 prom_limit0, prom_limit1;

static void map_prom_timers(void)
{
	struct device_node *dp;
	const unsigned int *addr;

	/* PROM timer node hangs out in the top level of device siblings... */
	dp = of_find_node_by_path("/");
	dp = dp->child;
	while (dp) {
		if (!strcmp(dp->name, "counter-timer"))
			break;
		dp = dp->sibling;
	}

	/* Assume if node is not present, PROM uses different tick mechanism
	 * which we should not care about.
	 */
	if (!dp) {
		prom_timers = (struct sun5_timer *) 0;
		return;
	}

	/* If PROM is really using this, it must be mapped by him. */
	addr = of_get_property(dp, "address", NULL);
	if (!addr) {
		prom_printf("PROM does not have timer mapped, trying to continue.\n");
		prom_timers = (struct sun5_timer *) 0;
		return;
	}
	prom_timers = (struct sun5_timer *) ((unsigned long)addr[0]);
}

static void kill_prom_timer(void)
{
	if (!prom_timers)
		return;

	/* Save them away for later. */
	prom_limit0 = prom_timers->limit0;
	prom_limit1 = prom_timers->limit1;

	/* Just as in sun4c/sun4m PROM uses timer which ticks at IRQ 14.
	 * We turn both off here just to be paranoid.
	 */
	prom_timers->limit0 = 0;
	prom_timers->limit1 = 0;

	/* Wheee, eat the interrupt packet too... */
	__asm__ __volatile__(
"	mov	0x40, %%g2\n"
"	ldxa	[%%g0] %0, %%g1\n"
"	ldxa	[%%g2] %1, %%g1\n"
"	stxa	%%g0, [%%g0] %0\n"
"	membar	#Sync\n"
	: /* no outputs */
	: "i" (ASI_INTR_RECEIVE), "i" (ASI_INTR_R)
	: "g1", "g2");
}

void notrace init_irqwork_curcpu(void)
{
	int cpu = hard_smp_processor_id();

	trap_block[cpu].irq_worklist_pa = 0UL;
}

/* Please be very careful with register_one_mondo() and
 * sun4v_register_mondo_queues().
 *
 * On SMP this gets invoked from the CPU trampoline before
 * the cpu has fully taken over the trap table from OBP,
 * and it's kernel stack + %g6 thread register state is
 * not fully cooked yet.
 *
 * Therefore you cannot make any OBP calls, not even prom_printf,
 * from these two routines.
 */
static void __cpuinit notrace register_one_mondo(unsigned long paddr, unsigned long type, unsigned long qmask)
{
	unsigned long num_entries = (qmask + 1) / 64;
	unsigned long status;

	status = sun4v_cpu_qconf(type, paddr, num_entries);
	if (status != HV_EOK) {
		prom_printf("SUN4V: sun4v_cpu_qconf(%lu:%lx:%lu) failed, "
			    "err %lu\n", type, paddr, num_entries, status);
		prom_halt();
	}
}

void __cpuinit notrace sun4v_register_mondo_queues(int this_cpu)
{
	struct trap_per_cpu *tb = &trap_block[this_cpu];

	register_one_mondo(tb->cpu_mondo_pa, HV_CPU_QUEUE_CPU_MONDO,
			   tb->cpu_mondo_qmask);
	register_one_mondo(tb->dev_mondo_pa, HV_CPU_QUEUE_DEVICE_MONDO,
			   tb->dev_mondo_qmask);
	register_one_mondo(tb->resum_mondo_pa, HV_CPU_QUEUE_RES_ERROR,
			   tb->resum_qmask);
	register_one_mondo(tb->nonresum_mondo_pa, HV_CPU_QUEUE_NONRES_ERROR,
			   tb->nonresum_qmask);
}

/* Each queue region must be a power of 2 multiple of 64 bytes in
 * size.  The base real address must be aligned to the size of the
 * region.  Thus, an 8KB queue must be 8KB aligned, for example.
 */
static void __init alloc_one_queue(unsigned long *pa_ptr, unsigned long qmask)
{
	unsigned long size = PAGE_ALIGN(qmask + 1);
	unsigned long order = get_order(size);
	unsigned long p;

	p = __get_free_pages(GFP_KERNEL, order);
	if (!p) {
		prom_printf("SUN4V: Error, cannot allocate queue.\n");
		prom_halt();
	}

	*pa_ptr = __pa(p);
}

static void __init init_cpu_send_mondo_info(struct trap_per_cpu *tb)
{
#ifdef CONFIG_SMP
	unsigned long page;

	BUILD_BUG_ON((NR_CPUS * sizeof(u16)) > (PAGE_SIZE - 64));

	page = get_zeroed_page(GFP_KERNEL);
	if (!page) {
		prom_printf("SUN4V: Error, cannot allocate cpu mondo page.\n");
		prom_halt();
	}

	tb->cpu_mondo_block_pa = __pa(page);
	tb->cpu_list_pa = __pa(page + 64);
#endif
}

/* Allocate mondo and error queues for all possible cpus.  */
static void __init sun4v_init_mondo_queues(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct trap_per_cpu *tb = &trap_block[cpu];

		alloc_one_queue(&tb->cpu_mondo_pa, tb->cpu_mondo_qmask);
		alloc_one_queue(&tb->dev_mondo_pa, tb->dev_mondo_qmask);
		alloc_one_queue(&tb->resum_mondo_pa, tb->resum_qmask);
		alloc_one_queue(&tb->resum_kernel_buf_pa, tb->resum_qmask);
		alloc_one_queue(&tb->nonresum_mondo_pa, tb->nonresum_qmask);
		alloc_one_queue(&tb->nonresum_kernel_buf_pa,
				tb->nonresum_qmask);
	}
}

static void __init init_send_mondo_info(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct trap_per_cpu *tb = &trap_block[cpu];

		init_cpu_send_mondo_info(tb);
	}
}

static struct irqaction timer_irq_action = {
	.name = "timer",
};

/* Only invoked on boot processor. */
void __init init_IRQ(void)
{
	unsigned long size;

	map_prom_timers();
	kill_prom_timer();

	size = sizeof(struct ino_bucket) * NUM_IVECS;
	ivector_table = kzalloc(size, GFP_KERNEL);
	if (!ivector_table) {
		prom_printf("Fatal error, cannot allocate ivector_table\n");
		prom_halt();
	}
	__flush_dcache_range((unsigned long) ivector_table,
			     ((unsigned long) ivector_table) + size);

	ivector_table_pa = __pa(ivector_table);

	if (tlb_type == hypervisor)
		sun4v_init_mondo_queues();

	init_send_mondo_info();

	if (tlb_type == hypervisor) {
		/* Load up the boot cpu's entries.  */
		sun4v_register_mondo_queues(hard_smp_processor_id());
	}

	/* We need to clear any IRQ's pending in the soft interrupt
	 * registers, a spurious one could be left around from the
	 * PROM timer which we just disabled.
	 */
	clear_softint(get_softint());

	/* Now that ivector table is initialized, it is safe
	 * to receive IRQ vector traps.  We will normally take
	 * one or two right now, in case some device PROM used
	 * to boot us wants to speak to us.  We just ignore them.
	 */
	__asm__ __volatile__("rdpr	%%pstate, %%g1\n\t"
			     "or	%%g1, %0, %%g1\n\t"
			     "wrpr	%%g1, 0x0, %%pstate"
			     : /* No outputs */
			     : "i" (PSTATE_IE)
			     : "g1");

	irq_desc[0].action = &timer_irq_action;
}
