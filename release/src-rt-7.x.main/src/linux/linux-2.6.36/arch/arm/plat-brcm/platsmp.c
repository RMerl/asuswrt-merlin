/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  based on linux/arch/arm/mach-realview/platsmp.c
 */
#ifdef	CONFIG_SMP
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/mach-types.h>
#include <asm/localtimer.h>
#include <asm/unified.h>
#include <asm/smp.h>
#include <asm/smp_scu.h>
#include <asm/cacheflush.h>

#include <plat/mpcore.h>
#include <mach/hardware.h>
#include <mach/smp.h>

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int __cpuinitdata pen_release = -1;

static inline unsigned int get_core_count(void)
{
	void __iomem *scu_base = scu_base_addr();
	if (scu_base)
		return scu_get_core_count(scu_base);
	return 1;
}

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();

	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	mpcore_cpu_init();

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	pen_release = -1;
	smp_wmb();
	clean_dcache_area((void *)  &pen_release, sizeof(pen_release));

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	pen_release = cpu;
	smp_wmb();

	clean_dcache_area((void *)  &pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 
			sizeof(pen_release)));

	dsb_sev();

	/*
	 * Timeout set on purpose in jiffies so that on slow processors
	 * that must also have low HZ it will wait longer.
	 */
	timeout = jiffies + 128;

	udelay(100);

	/*
	 * If the secondary CPU was waiting on WFE, it should
	 * be already watching <pen_release>, or it could be
	 * waiting in WFI, send it an IPI to be sure it wakes.
	 */

	if( pen_release != -1 )
		{
		smp_cross_call(cpumask_of(cpu));
		}

	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	if (arch_is_coherent()) {
		outer_cache.inv_range = NULL;
		outer_cache.clean_range = NULL;
		outer_cache.flush_range = NULL;
		outer_cache.sync = NULL;
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int ncores = get_core_count();
	unsigned int cpu = smp_processor_id();
	int i;

	/* sanity check */
	if (ncores == 0) {
		printk(KERN_ERR
		       "MPCORE: strange CPU count of 0? Default to 1\n");

		ncores = 1;
	}

	if (ncores > NR_CPUS) {
		printk(KERN_WARNING
		       "MPCORE: no. of cores (%d) greater than configured "
		       "maximum of %d - clipping\n",
		       ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	smp_store_cpu_info(cpu);

	/*
	 * are we trying to boot more cores than exist?
	 */
	if (max_cpus > ncores)
		max_cpus = ncores;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	/*
	 * Initialise the SCU if there are more than one CPU and let
	 * them know where to start. Note that, on modern versions of
	 * MILO, the "poke" doesn't actually do anything until each
	 * individual core is sent a soft interrupt to get it out of
	 * WFI
	 */
	if (max_cpus > 1) {
		/* nobody is to be released from the pen yet */
		pen_release = -1;

		/*
		 * Enable the local timer or broadcast device for the
		 * boot CPU, but only if we have more than one CPU.
		 */
		percpu_timer_setup();

		scu_enable(scu_base_addr());

		/* Wakeup other cores in an SoC-specific manner */
		plat_wake_secondary_cpu( max_cpus, platform_secondary_startup );

	}
}
#endif	/* CONFIG_SMP */
