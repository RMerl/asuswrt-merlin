/*
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Copyright (C) 2007, 2011 MIPS Technologies, Inc.
 *    Chris Dearman (chris@mips.com)
 *    Steven J. Hill (sjhill@mips.com)
 */

#undef DEBUG

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>

#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/smp.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/mips_mt.h>
#include <asm/amon.h>
#include <asm/gic.h>

static void ipi_call_function(unsigned int cpu)
{
	pr_debug("CPU%d: %s cpu %d status %08x\n",
		 smp_processor_id(), __func__, cpu, read_c0_status());

	gic_send_ipi(plat_ipi_call_int_xlate(cpu));
}


static void ipi_resched(unsigned int cpu)
{
	pr_debug("CPU%d: %s cpu %d status %08x\n",
		 smp_processor_id(), __func__, cpu, read_c0_status());

	gic_send_ipi(plat_ipi_resched_int_xlate(cpu));
}

void cmp_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;

	local_irq_save(flags);

	switch (action) {
	case SMP_CALL_FUNCTION:
		ipi_call_function(cpu);
		break;

	case SMP_RESCHEDULE_YOURSELF:
		ipi_resched(cpu);
		break;
	}

	local_irq_restore(flags);
}

static void cmp_send_ipi_mask(const struct cpumask *mask, unsigned int action)
{
	unsigned int i;

	for_each_cpu(i, mask)
		cmp_send_ipi_single(i, action);
}

static void __cpuinit cmp_init_secondary(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	/* Assume GIC is present */
	change_c0_status(ST0_IM, STATUSF_IP3 | STATUSF_IP4 | STATUSF_IP6 |
				 STATUSF_IP7);

	/* Enable per-cpu interrupts: platform specific */

	c->core = (read_c0_ebase() >> 1) & 0xff;
#if defined(CONFIG_MIPS_MT_SMP) || defined(CONFIG_MIPS_MT_SMTC)
	c->vpe_id = (read_c0_tcbind() >> TCBIND_CURVPE_SHIFT) & TCBIND_CURVPE;
#endif
#ifdef CONFIG_MIPS_MT_SMTC
	c->tc_id  = (read_c0_tcbind() >> TCBIND_CURTC_SHIFT) & TCBIND_CURTC;
#endif
}

static void __cpuinit cmp_smp_finish(void)
{
	pr_debug("SMPCMP: CPU%d: %s\n", smp_processor_id(), __func__);

	/* Arrange for a clock interrupt immediately */
	write_c0_compare(read_c0_count() + 200);

#ifdef CONFIG_MIPS_MT_FPAFF
	/* If we have an FPU, enroll ourselves in the FPU-full mask */
	if (cpu_has_fpu)
		cpu_set(smp_processor_id(), mt_fpu_cpumask);
#endif /* CONFIG_MIPS_MT_FPAFF */

	local_irq_enable();
}

static void __cpuinit cmp_cpus_done(void)
{
	pr_debug("SMPCMP: CPU%d: %s\n", smp_processor_id(), __func__);
}

/*
 * Setup the PC, SP, and GP of a secondary processor and start it running
 * smp_bootstrap is the place to resume from
 * __KSTK_TOS(idle) is apparently the stack pointer
 * (unsigned long)idle->thread_info the gp
 */
static void __cpuinit cmp_boot_secondary(int cpu, struct task_struct *idle)
{
	struct thread_info *gp = task_thread_info(idle);
	unsigned long sp = __KSTK_TOS(idle);
	unsigned long pc = (unsigned long)&smp_bootstrap;
	unsigned long a0 = 0;

	pr_debug("SMPCMP: CPU%d: %s cpu %d\n", smp_processor_id(),
		__func__, cpu);


	amon_cpu_start(cpu, pc, sp, (unsigned long)gp, a0);
}

/*
 * Common setup before any secondaries are started
 */
void __init cmp_smp_setup(void)
{
	int i;
	int ncpu = 0;

	pr_debug("SMPCMP: CPU%d: %s\n", smp_processor_id(), __func__);

#ifdef CONFIG_MIPS_MT_FPAFF
	/* If we have an FPU, enroll ourselves in the FPU-full mask */
	if (cpu_has_fpu)
		cpu_set(0, mt_fpu_cpumask);
#endif /* CONFIG_MIPS_MT_FPAFF */

	for (i = 1; i < NR_CPUS; i++) {
		if (amon_cpu_avail(i)) {
			set_cpu_possible(i, true);
			__cpu_number_map[i]	= ++ncpu;
			__cpu_logical_map[ncpu]	= i;
		}
	}
	cpu_present_map = cpu_possible_map;

	if (cpu_has_mipsmt) {
		unsigned int nvpe, mvpconf0 = read_c0_mvpconf0();

		nvpe = ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
		smp_num_siblings = nvpe;
	}
	pr_info("Detected %i available secondary CPU(s)\n", ncpu);
}

void __init cmp_prepare_cpus(unsigned int max_cpus)
{
	pr_debug("SMPCMP: CPU%d: %s max_cpus=%d\n",
		 smp_processor_id(), __func__, max_cpus);

	mips_mt_set_cpuoptions();
}

#ifdef CONFIG_HOTPLUG_CPU

/* State of each CPU. */
DEFINE_PER_CPU(int, cpu_state);

extern void fixup_irqs(void);

static DEFINE_SPINLOCK(smp_reserve_lock);

static int cmp_cpu_disable(void)
{
	unsigned int cpu = smp_processor_id();

	if (cpu == 0)
		return -EBUSY;

	spin_lock(&smp_reserve_lock);

	cpu_clear(cpu, cpu_online_map);
	cpu_clear(cpu, cpu_callin_map);
	local_irq_disable();
	fixup_irqs();
	local_irq_enable();

	flush_cache_all();
	local_flush_tlb_all();

	spin_unlock(&smp_reserve_lock);

	return 0;
}

static void cmp_cpu_die(unsigned int cpu)
{
	while (per_cpu(cpu_state, cpu) != CPU_DEAD)
		cpu_relax();
	/*
	 * If all of the CPU's on the other core are now disabled
	 * the core can be powered down
	 */
}

void play_dead(void)
{
	int coreid = smp_processor_id();

	idle_task_exit();

	per_cpu(cpu_state, coreid) = CPU_DEAD;

	/*
	 * This CPU is now inactive. Other CPU's (VPE's) on the
	 * same core may still be active
	 */
	amon_cpu_dead();	/* This will not return */
}

static int __cpuinit cmp_cpu_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
		pr_debug("CPU%d: prepare\n", cpu);
		break;
	case CPU_ONLINE:
		pr_debug("CPU%d: online\n", cpu);
		break;
	case CPU_DEAD:
		pr_debug("CPU%d: dead\n", cpu);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cmp_cpu_notifier = {
	.notifier_call = cmp_cpu_callback,
};

static int __cpuinit register_cmp_notifier(void)
{
	register_hotcpu_notifier(&cmp_cpu_notifier);

	return 0;
}

late_initcall(register_cmp_notifier);

#endif  /* CONFIG_HOTPLUG_CPU */


struct plat_smp_ops cmp_smp_ops = {
	.send_ipi_single	= cmp_send_ipi_single,
	.send_ipi_mask		= cmp_send_ipi_mask,
	.init_secondary		= cmp_init_secondary,
	.smp_finish		= cmp_smp_finish,
	.cpus_done		= cmp_cpus_done,
	.boot_secondary		= cmp_boot_secondary,
	.smp_setup		= cmp_smp_setup,
	.prepare_cpus		= cmp_prepare_cpus,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= cmp_cpu_disable,
	.cpu_die		= cmp_cpu_die,
#endif
};
