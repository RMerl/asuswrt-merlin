/* sun4m_smp.c: Sparc SUN4M SMP support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <asm/head.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/profile.h>
#include <linux/delay.h>
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/irq_regs.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/oplib.h>
#include <asm/cpudata.h>

#include "irq.h"

#define IRQ_CROSS_CALL		15

extern ctxd_t *srmmu_ctx_table_phys;

extern volatile unsigned long cpu_callin_map[NR_CPUS];
extern unsigned char boot_cpu_id;

extern cpumask_t smp_commenced_mask;

extern int __smp4m_processor_id(void);

/*#define SMP_DEBUG*/

#ifdef SMP_DEBUG
#define SMP_PRINTK(x)	printk x
#else
#define SMP_PRINTK(x)
#endif

static inline unsigned long
swap_ulong(volatile unsigned long *ptr, unsigned long val)
{
	__asm__ __volatile__("swap [%1], %0\n\t" :
			     "=&r" (val), "=&r" (ptr) :
			     "0" (val), "1" (ptr));
	return val;
}

static void smp_setup_percpu_timer(void);
extern void cpu_probe(void);

void __cpuinit smp4m_callin(void)
{
	int cpuid = hard_smp_processor_id();

	local_flush_cache_all();
	local_flush_tlb_all();

	notify_cpu_starting(cpuid);

	/* Get our local ticker going. */
	smp_setup_percpu_timer();

	calibrate_delay();
	smp_store_cpu_info(cpuid);

	local_flush_cache_all();
	local_flush_tlb_all();

	/*
	 * Unblock the master CPU _only_ when the scheduler state
	 * of all secondary CPUs will be up-to-date, so after
	 * the SMP initialization the master will be just allowed
	 * to call the scheduler code.
	 */
	/* Allow master to continue. */
	swap_ulong(&cpu_callin_map[cpuid], 1);

	local_flush_cache_all();
	local_flush_tlb_all();
	
	cpu_probe();

	/* Fix idle thread fields. */
	__asm__ __volatile__("ld [%0], %%g6\n\t"
			     : : "r" (&current_set[cpuid])
			     : "memory" /* paranoid */);

	/* Attach to the address space of init_task. */
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	while (!cpu_isset(cpuid, smp_commenced_mask))
		mb();

	local_irq_enable();

	set_cpu_online(cpuid, true);
}

/*
 *	Cycle through the processors asking the PROM to start each one.
 */
 
extern struct linux_prom_registers smp_penguin_ctable;

void __init smp4m_boot_cpus(void)
{
	smp_setup_percpu_timer();
	local_flush_cache_all();
}

int __cpuinit smp4m_boot_one_cpu(int i)
{
	extern unsigned long sun4m_cpu_startup;
	unsigned long *entry = &sun4m_cpu_startup;
	struct task_struct *p;
	int timeout;
	int cpu_node;

	cpu_find_by_mid(i, &cpu_node);

	/* Cook up an idler for this guy. */
	p = fork_idle(i);
	current_set[i] = task_thread_info(p);
	/* See trampoline.S for details... */
	entry += ((i-1) * 3);

	/*
	 * Initialize the contexts table
	 * Since the call to prom_startcpu() trashes the structure,
	 * we need to re-initialize it for each cpu
	 */
	smp_penguin_ctable.which_io = 0;
	smp_penguin_ctable.phys_addr = (unsigned int) srmmu_ctx_table_phys;
	smp_penguin_ctable.reg_size = 0;

	/* whirrr, whirrr, whirrrrrrrrr... */
	printk("Starting CPU %d at %p\n", i, entry);
	local_flush_cache_all();
	prom_startcpu(cpu_node,
		      &smp_penguin_ctable, 0, (char *)entry);

	/* wheee... it's going... */
	for(timeout = 0; timeout < 10000; timeout++) {
		if(cpu_callin_map[i])
			break;
		udelay(200);
	}

	if (!(cpu_callin_map[i])) {
		printk("Processor %d is stuck.\n", i);
		return -ENODEV;
	}

	local_flush_cache_all();
	return 0;
}

void __init smp4m_smp_done(void)
{
	int i, first;
	int *prev;

	/* setup cpu list for irq rotation */
	first = 0;
	prev = &first;
	for_each_online_cpu(i) {
		*prev = i;
		prev = &cpu_data(i).next;
	}
	*prev = first;
	local_flush_cache_all();

	/* Ok, they are spinning and ready to go. */
}

void smp4m_irq_rotate(int cpu)
{
	int next = cpu_data(cpu).next;
	if (next != cpu)
		set_irq_udt(next);
}

static struct smp_funcall {
	smpfunc_t func;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
	unsigned long arg4;
	unsigned long arg5;
	unsigned long processors_in[SUN4M_NCPUS];  /* Set when ipi entered. */
	unsigned long processors_out[SUN4M_NCPUS]; /* Set when ipi exited. */
} ccall_info;

static DEFINE_SPINLOCK(cross_call_lock);

/* Cross calls must be serialized, at least currently. */
static void smp4m_cross_call(smpfunc_t func, cpumask_t mask, unsigned long arg1,
			     unsigned long arg2, unsigned long arg3,
			     unsigned long arg4)
{
		register int ncpus = SUN4M_NCPUS;
		unsigned long flags;

		spin_lock_irqsave(&cross_call_lock, flags);

		/* Init function glue. */
		ccall_info.func = func;
		ccall_info.arg1 = arg1;
		ccall_info.arg2 = arg2;
		ccall_info.arg3 = arg3;
		ccall_info.arg4 = arg4;
		ccall_info.arg5 = 0;

		/* Init receive/complete mapping, plus fire the IPI's off. */
		{
			register int i;

			cpu_clear(smp_processor_id(), mask);
			cpus_and(mask, cpu_online_map, mask);
			for(i = 0; i < ncpus; i++) {
				if (cpu_isset(i, mask)) {
					ccall_info.processors_in[i] = 0;
					ccall_info.processors_out[i] = 0;
					set_cpu_int(i, IRQ_CROSS_CALL);
				} else {
					ccall_info.processors_in[i] = 1;
					ccall_info.processors_out[i] = 1;
				}
			}
		}

		{
			register int i;

			i = 0;
			do {
				if (!cpu_isset(i, mask))
					continue;
				while(!ccall_info.processors_in[i])
					barrier();
			} while(++i < ncpus);

			i = 0;
			do {
				if (!cpu_isset(i, mask))
					continue;
				while(!ccall_info.processors_out[i])
					barrier();
			} while(++i < ncpus);
		}

		spin_unlock_irqrestore(&cross_call_lock, flags);
}

/* Running cross calls. */
void smp4m_cross_call_irq(void)
{
	int i = smp_processor_id();

	ccall_info.processors_in[i] = 1;
	ccall_info.func(ccall_info.arg1, ccall_info.arg2, ccall_info.arg3,
			ccall_info.arg4, ccall_info.arg5);
	ccall_info.processors_out[i] = 1;
}

extern void sun4m_clear_profile_irq(int cpu);

void smp4m_percpu_timer_interrupt(struct pt_regs *regs)
{
	struct pt_regs *old_regs;
	int cpu = smp_processor_id();

	old_regs = set_irq_regs(regs);

	sun4m_clear_profile_irq(cpu);

	profile_tick(CPU_PROFILING);

	if(!--prof_counter(cpu)) {
		int user = user_mode(regs);

		irq_enter();
		update_process_times(user);
		irq_exit();

		prof_counter(cpu) = prof_multiplier(cpu);
	}
	set_irq_regs(old_regs);
}

extern unsigned int lvl14_resolution;

static void __cpuinit smp_setup_percpu_timer(void)
{
	int cpu = smp_processor_id();

	prof_counter(cpu) = prof_multiplier(cpu) = 1;
	load_profile_irq(cpu, lvl14_resolution);

	if(cpu == boot_cpu_id)
		enable_pil_irq(14);
}

static void __init smp4m_blackbox_id(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[1] = 0x8130200c | rd | rs1;    	/* srl reg, 0xc, reg */
	addr[2] = 0x80082003 | rd | rs1;	/* and reg, 3, reg */
}

static void __init smp4m_blackbox_current(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[2] = 0x8130200a | rd | rs1;    	/* srl reg, 0xa, reg */
	addr[4] = 0x8008200c | rd | rs1;	/* and reg, 0xc, reg */
}

void __init sun4m_init_smp(void)
{
	BTFIXUPSET_BLACKBOX(hard_smp_processor_id, smp4m_blackbox_id);
	BTFIXUPSET_BLACKBOX(load_current, smp4m_blackbox_current);
	BTFIXUPSET_CALL(smp_cross_call, smp4m_cross_call, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(__hard_smp_processor_id, __smp4m_processor_id, BTFIXUPCALL_NORM);
}
