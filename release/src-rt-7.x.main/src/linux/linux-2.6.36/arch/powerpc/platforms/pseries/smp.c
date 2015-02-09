/*
 * SMP support for pSeries machines.
 *
 * Dave Engebretsen, Peter Bergner, and
 * Mike Corrigan {engebret|bergner|mikec}@us.ibm.com
 *
 * Plus various changes from other IBM teams...
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/cache.h>
#include <linux/err.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/paca.h>
#include <asm/machdep.h>
#include <asm/cputable.h>
#include <asm/firmware.h>
#include <asm/system.h>
#include <asm/rtas.h>
#include <asm/pSeries_reconfig.h>
#include <asm/mpic.h>
#include <asm/vdso_datapage.h>
#include <asm/cputhreads.h>

#include "plpar_wrappers.h"
#include "pseries.h"
#include "xics.h"
#include "offline_states.h"


/*
 * The Primary thread of each non-boot processor was started from the OF client
 * interface by prom_hold_cpus and is spinning on secondary_hold_spinloop.
 */
static cpumask_var_t of_spin_mask;

/* Query where a cpu is now.  Return codes #defined in plpar_wrappers.h */
int smp_query_cpu_stopped(unsigned int pcpu)
{
	int cpu_status, status;
	int qcss_tok = rtas_token("query-cpu-stopped-state");

	if (qcss_tok == RTAS_UNKNOWN_SERVICE) {
		printk(KERN_INFO "Firmware doesn't support "
				"query-cpu-stopped-state\n");
		return QCSS_HARDWARE_ERROR;
	}

	status = rtas_call(qcss_tok, 1, 2, &cpu_status, pcpu);
	if (status != 0) {
		printk(KERN_ERR
		       "RTAS query-cpu-stopped-state failed: %i\n", status);
		return status;
	}

	return cpu_status;
}

/**
 * smp_startup_cpu() - start the given cpu
 *
 * At boot time, there is nothing to do for primary threads which were
 * started from Open Firmware.  For anything else, call RTAS with the
 * appropriate start location.
 *
 * Returns:
 *	0	- failure
 *	1	- success
 */
static inline int __devinit smp_startup_cpu(unsigned int lcpu)
{
	int status;
	unsigned long start_here = __pa((u32)*((unsigned long *)
					       generic_secondary_smp_init));
	unsigned int pcpu;
	int start_cpu;

	if (cpumask_test_cpu(lcpu, of_spin_mask))
		/* Already started by OF and sitting in spin loop */
		return 1;

	pcpu = get_hard_smp_processor_id(lcpu);

	/* Check to see if the CPU out of FW already for kexec */
	if (smp_query_cpu_stopped(pcpu) == QCSS_NOT_STOPPED){
		cpumask_set_cpu(lcpu, of_spin_mask);
		return 1;
	}

	/* Fixup atomic count: it exited inside IRQ handler. */
	task_thread_info(paca[lcpu].__current)->preempt_count	= 0;

	if (get_cpu_current_state(lcpu) == CPU_STATE_INACTIVE)
		goto out;

	/* 
	 * If the RTAS start-cpu token does not exist then presume the
	 * cpu is already spinning.
	 */
	start_cpu = rtas_token("start-cpu");
	if (start_cpu == RTAS_UNKNOWN_SERVICE)
		return 1;

	status = rtas_call(start_cpu, 3, 1, NULL, pcpu, start_here, pcpu);
	if (status != 0) {
		printk(KERN_ERR "start-cpu failed: %i\n", status);
		return 0;
	}

out:
	return 1;
}

#ifdef CONFIG_XICS
static void __devinit smp_xics_setup_cpu(int cpu)
{
	if (cpu != boot_cpuid)
		xics_setup_cpu();

	if (firmware_has_feature(FW_FEATURE_SPLPAR))
		vpa_init(cpu);

	cpumask_clear_cpu(cpu, of_spin_mask);
	set_cpu_current_state(cpu, CPU_STATE_ONLINE);
	set_default_offline_state(cpu);

}
#endif /* CONFIG_XICS */

static void __devinit smp_pSeries_kick_cpu(int nr)
{
	long rc;
	unsigned long hcpuid;
	BUG_ON(nr < 0 || nr >= NR_CPUS);

	if (!smp_startup_cpu(nr))
		return;

	/*
	 * The processor is currently spinning, waiting for the
	 * cpu_start field to become non-zero After we set cpu_start,
	 * the processor will continue on to secondary_start
	 */
	paca[nr].cpu_start = 1;

	set_preferred_offline_state(nr, CPU_STATE_ONLINE);

	if (get_cpu_current_state(nr) == CPU_STATE_INACTIVE) {
		hcpuid = get_hard_smp_processor_id(nr);
		rc = plpar_hcall_norets(H_PROD, hcpuid);
		if (rc != H_SUCCESS)
			printk(KERN_ERR "Error: Prod to wake up processor %d "
						"Ret= %ld\n", nr, rc);
	}
}

static int smp_pSeries_cpu_bootable(unsigned int nr)
{
	/* Special case - we inhibit secondary thread startup
	 * during boot if the user requests it.
	 */
	if (system_state < SYSTEM_RUNNING && cpu_has_feature(CPU_FTR_SMT)) {
		if (!smt_enabled_at_boot && cpu_thread_in_core(nr) != 0)
			return 0;
		if (smt_enabled_at_boot
		    && cpu_thread_in_core(nr) >= smt_enabled_at_boot)
			return 0;
	}

	return 1;
}
#ifdef CONFIG_MPIC
static struct smp_ops_t pSeries_mpic_smp_ops = {
	.message_pass	= smp_mpic_message_pass,
	.probe		= smp_mpic_probe,
	.kick_cpu	= smp_pSeries_kick_cpu,
	.setup_cpu	= smp_mpic_setup_cpu,
};
#endif
#ifdef CONFIG_XICS
static struct smp_ops_t pSeries_xics_smp_ops = {
	.message_pass	= smp_xics_message_pass,
	.probe		= smp_xics_probe,
	.kick_cpu	= smp_pSeries_kick_cpu,
	.setup_cpu	= smp_xics_setup_cpu,
	.cpu_bootable	= smp_pSeries_cpu_bootable,
};
#endif

/* This is called very early */
static void __init smp_init_pseries(void)
{
	int i;

	pr_debug(" -> smp_init_pSeries()\n");

	alloc_bootmem_cpumask_var(&of_spin_mask);

	/* Mark threads which are still spinning in hold loops. */
	if (cpu_has_feature(CPU_FTR_SMT)) {
		for_each_present_cpu(i) { 
			if (cpu_thread_in_core(i) == 0)
				cpumask_set_cpu(i, of_spin_mask);
		}
	} else {
		cpumask_copy(of_spin_mask, cpu_present_mask);
	}

	cpumask_clear_cpu(boot_cpuid, of_spin_mask);

	/* Non-lpar has additional take/give timebase */
	if (rtas_token("freeze-time-base") != RTAS_UNKNOWN_SERVICE) {
		smp_ops->give_timebase = rtas_give_timebase;
		smp_ops->take_timebase = rtas_take_timebase;
	}

	pr_debug(" <- smp_init_pSeries()\n");
}

#ifdef CONFIG_MPIC
void __init smp_init_pseries_mpic(void)
{
	smp_ops = &pSeries_mpic_smp_ops;

	smp_init_pseries();
}
#endif

void __init smp_init_pseries_xics(void)
{
	smp_ops = &pSeries_xics_smp_ops;

	smp_init_pseries();
}
