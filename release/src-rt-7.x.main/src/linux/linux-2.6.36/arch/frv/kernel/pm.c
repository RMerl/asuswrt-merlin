/*
 * FR-V Power Management Routines
 *
 * Copyright (c) 2004 Red Hat, Inc.
 *
 * Based on SA1100 version:
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include <asm/mb86943a.h>

#include "local.h"

/*
 * Debug macros
 */
#define DEBUG

int pm_do_suspend(void)
{
	local_irq_disable();

	__set_LEDS(0xb1);

	/* go zzz */
	frv_cpu_suspend(pdm_suspend_mode);

	__set_LEDS(0xb2);

	local_irq_enable();

	return 0;
}

static unsigned long __irq_mask;

/*
 * Setup interrupt masks, etc to enable wakeup by power switch
 */
static void __default_power_switch_setup(void)
{
	/* default is to mask all interrupt sources. */
	__irq_mask = *(unsigned long *)0xfeff9820;
	*(unsigned long *)0xfeff9820 = 0xfffe0000;
}

/*
 * Cleanup interrupt masks, etc after wakeup by power switch
 */
static void __default_power_switch_cleanup(void)
{
	*(unsigned long *)0xfeff9820 = __irq_mask;
}

/*
 * Return non-zero if wakeup irq was caused by power switch
 */
static int __default_power_switch_check(void)
{
	return 1;
}

void (*__power_switch_wake_setup)(void) = __default_power_switch_setup;
int  (*__power_switch_wake_check)(void) = __default_power_switch_check;
void (*__power_switch_wake_cleanup)(void) = __default_power_switch_cleanup;

int pm_do_bus_sleep(void)
{
	local_irq_disable();

	/*
         * Here is where we need some platform-dependent setup
	 * of the interrupt state so that appropriate wakeup
	 * sources are allowed and all others are masked.
	 */
	__power_switch_wake_setup();

	__set_LEDS(0xa1);

	/* go zzz
	 *
	 * This is in a loop in case power switch shares an irq with other
	 * devices. The wake_check() tells us if we need to finish waking
	 * or go back to sleep.
	 */
	do {
		frv_cpu_suspend(HSR0_PDM_BUS_SLEEP);
	} while (__power_switch_wake_check && !__power_switch_wake_check());

	__set_LEDS(0xa2);

	/*
         * Here is where we need some platform-dependent restore
	 * of the interrupt state prior to being called.
	 */
	__power_switch_wake_cleanup();

	local_irq_enable();

	return 0;
}

unsigned long sleep_phys_sp(void *sp)
{
	return virt_to_phys(sp);
}

#ifdef CONFIG_SYSCTL
/*
 * Use a temporary sysctl number. Horrid, but will be cleaned up in 2.6
 * when all the PM interfaces exist nicely.
 */
#define CTL_PM_SUSPEND 1
#define CTL_PM_CMODE 2
#define CTL_PM_P0 4
#define CTL_PM_CM 5

static int user_atoi(char __user *ubuf, size_t len)
{
	char buf[16];
	unsigned long ret;

	if (len > 15)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	buf[len] = 0;
	ret = simple_strtoul(buf, NULL, 0);
	if (ret > INT_MAX)
		return -ERANGE;
	return ret;
}

/*
 * Send us to sleep.
 */
static int sysctl_pm_do_suspend(ctl_table *ctl, int write,
				void __user *buffer, size_t *lenp, loff_t *fpos)
{
	int retval, mode;

	if (*lenp <= 0)
		return -EIO;

	mode = user_atoi(buffer, *lenp);
	if ((mode != 1) && (mode != 5))
		return -EINVAL;

	if (retval == 0) {
		if (mode == 5)
		    retval = pm_do_bus_sleep();
		else
		    retval = pm_do_suspend();
	}

	return retval;
}

static int try_set_cmode(int new_cmode)
{
	if (new_cmode > 15)
		return -EINVAL;
	if (!(clock_cmodes_permitted & (1<<new_cmode)))
		return -EINVAL;

	/* now change cmode */
	local_irq_disable();
	frv_dma_pause_all();

	frv_change_cmode(new_cmode);

	determine_clocks(0);
	time_divisor_init();

#ifdef DEBUG
	determine_clocks(1);
#endif
	frv_dma_resume_all();
	local_irq_enable();

	return 0;
}


static int cmode_procctl(ctl_table *ctl, int write,
			 void __user *buffer, size_t *lenp, loff_t *fpos)
{
	int new_cmode;

	if (!write)
		return proc_dointvec(ctl, write, buffer, lenp, fpos);

	new_cmode = user_atoi(buffer, *lenp);

	return try_set_cmode(new_cmode)?:*lenp;
}

static int try_set_p0(int new_p0)
{
	unsigned long flags, clkc;

	if (new_p0 < 0 || new_p0 > 1)
		return -EINVAL;

	local_irq_save(flags);
	__set_PSR(flags & ~PSR_ET);

	frv_dma_pause_all();

	clkc = __get_CLKC();
	if (new_p0)
		clkc |= CLKC_P0;
	else
		clkc &= ~CLKC_P0;
	__set_CLKC(clkc);

	determine_clocks(0);
	time_divisor_init();

#ifdef DEBUG
	determine_clocks(1);
#endif
	frv_dma_resume_all();
	local_irq_restore(flags);
	return 0;
}

static int try_set_cm(int new_cm)
{
	unsigned long flags, clkc;

	if (new_cm < 0 || new_cm > 1)
		return -EINVAL;

	local_irq_save(flags);
	__set_PSR(flags & ~PSR_ET);

	frv_dma_pause_all();

	clkc = __get_CLKC();
	clkc &= ~CLKC_CM;
	clkc |= new_cm;
	__set_CLKC(clkc);

	determine_clocks(0);
	time_divisor_init();

	determine_clocks(1);

	frv_dma_resume_all();
	local_irq_restore(flags);
	return 0;
}

static int p0_procctl(ctl_table *ctl, int write,
		      void __user *buffer, size_t *lenp, loff_t *fpos)
{
	int new_p0;

	if (!write)
		return proc_dointvec(ctl, write, buffer, lenp, fpos);

	new_p0 = user_atoi(buffer, *lenp);

	return try_set_p0(new_p0)?:*lenp;
}

static int cm_procctl(ctl_table *ctl, int write,
		      void __user *buffer, size_t *lenp, loff_t *fpos)
{
	int new_cm;

	if (!write)
		return proc_dointvec(ctl, write, buffer, lenp, fpos);

	new_cm = user_atoi(buffer, *lenp);

	return try_set_cm(new_cm)?:*lenp;
}

static struct ctl_table pm_table[] =
{
	{
		.procname	= "suspend",
		.data		= NULL,
		.maxlen		= 0,
		.mode		= 0200,
		.proc_handler	= sysctl_pm_do_suspend,
	},
	{
		.procname	= "cmode",
		.data		= &clock_cmode_current,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= cmode_procctl,
	},
	{
		.procname	= "p0",
		.data		= &clock_p0_current,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= p0_procctl,
	},
	{
		.procname	= "cm",
		.data		= &clock_cm_current,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= cm_procctl,
	},
	{ }
};

static struct ctl_table pm_dir_table[] =
{
	{
		.procname	= "pm",
		.mode		= 0555,
		.child		= pm_table,
	},
	{ }
};

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table);
	return 0;
}

__initcall(pm_init);

#endif
