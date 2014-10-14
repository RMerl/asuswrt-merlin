/*
 * hangcheck-timer.c
 *
 * Driver for a little io fencing timer.
 *
 * Copyright (C) 2002, 2003 Oracle.  All rights reserved.
 *
 * Author: Joel Becker <joel.becker@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

/*
 * The hangcheck-timer driver uses the TSC to catch delays that
 * jiffies does not notice.  A timer is set.  When the timer fires, it
 * checks whether it was delayed and if that delay exceeds a given
 * margin of error.  The hangcheck_tick module parameter takes the timer
 * duration in seconds.  The hangcheck_margin parameter defines the
 * margin of error, in seconds.  The defaults are 60 seconds for the
 * timer and 180 seconds for the margin of error.  IOW, a timer is set
 * for 60 seconds.  When the timer fires, the callback checks the
 * actual duration that the timer waited.  If the duration exceeds the
 * alloted time and margin (here 60 + 180, or 240 seconds), the machine
 * is restarted.  A healthy machine will have the duration match the
 * expected timeout very closely.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/sysrq.h>
#include <linux/timer.h>
#include <linux/time.h>

#define VERSION_STR "0.9.1"

#define DEFAULT_IOFENCE_MARGIN 60	/* Default fudge factor, in seconds */
#define DEFAULT_IOFENCE_TICK 180	/* Default timer timeout, in seconds */

static int hangcheck_tick = DEFAULT_IOFENCE_TICK;
static int hangcheck_margin = DEFAULT_IOFENCE_MARGIN;
static int hangcheck_reboot;  /* Defaults to not reboot */
static int hangcheck_dump_tasks;  /* Defaults to not dumping SysRQ T */

/* options - modular */
module_param(hangcheck_tick, int, 0);
MODULE_PARM_DESC(hangcheck_tick, "Timer delay.");
module_param(hangcheck_margin, int, 0);
MODULE_PARM_DESC(hangcheck_margin, "If the hangcheck timer has been delayed more than hangcheck_margin seconds, the driver will fire.");
module_param(hangcheck_reboot, int, 0);
MODULE_PARM_DESC(hangcheck_reboot, "If nonzero, the machine will reboot when the timer margin is exceeded.");
module_param(hangcheck_dump_tasks, int, 0);
MODULE_PARM_DESC(hangcheck_dump_tasks, "If nonzero, the machine will dump the system task state when the timer margin is exceeded.");

MODULE_AUTHOR("Oracle");
MODULE_DESCRIPTION("Hangcheck-timer detects when the system has gone out to lunch past a certain margin.");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION_STR);

/* options - nonmodular */
#ifndef MODULE

static int __init hangcheck_parse_tick(char *str)
{
	int par;
	if (get_option(&str,&par))
		hangcheck_tick = par;
	return 1;
}

static int __init hangcheck_parse_margin(char *str)
{
	int par;
	if (get_option(&str,&par))
		hangcheck_margin = par;
	return 1;
}

static int __init hangcheck_parse_reboot(char *str)
{
	int par;
	if (get_option(&str,&par))
		hangcheck_reboot = par;
	return 1;
}

static int __init hangcheck_parse_dump_tasks(char *str)
{
	int par;
	if (get_option(&str,&par))
		hangcheck_dump_tasks = par;
	return 1;
}

__setup("hcheck_tick", hangcheck_parse_tick);
__setup("hcheck_margin", hangcheck_parse_margin);
__setup("hcheck_reboot", hangcheck_parse_reboot);
__setup("hcheck_dump_tasks", hangcheck_parse_dump_tasks);
#endif /* not MODULE */

#if defined(CONFIG_S390)
# define HAVE_MONOTONIC
# define TIMER_FREQ 1000000000ULL
#else
# define TIMER_FREQ 1000000000ULL
#endif

#ifdef HAVE_MONOTONIC
extern unsigned long long monotonic_clock(void);
#else
static inline unsigned long long monotonic_clock(void)
{
	struct timespec ts;
	getrawmonotonic(&ts);
	return timespec_to_ns(&ts);
}
#endif  /* HAVE_MONOTONIC */


/* Last time scheduled */
static unsigned long long hangcheck_tsc, hangcheck_tsc_margin;

static void hangcheck_fire(unsigned long);

static DEFINE_TIMER(hangcheck_ticktock, hangcheck_fire, 0, 0);


static void hangcheck_fire(unsigned long data)
{
	unsigned long long cur_tsc, tsc_diff;

	cur_tsc = monotonic_clock();

	if (cur_tsc > hangcheck_tsc)
		tsc_diff = cur_tsc - hangcheck_tsc;
	else
		tsc_diff = (cur_tsc + (~0ULL - hangcheck_tsc)); /* or something */

	if (tsc_diff > hangcheck_tsc_margin) {
		if (hangcheck_dump_tasks) {
			printk(KERN_CRIT "Hangcheck: Task state:\n");
#ifdef CONFIG_MAGIC_SYSRQ
			handle_sysrq('t');
#endif  /* CONFIG_MAGIC_SYSRQ */
		}
		if (hangcheck_reboot) {
			printk(KERN_CRIT "Hangcheck: hangcheck is restarting the machine.\n");
			emergency_restart();
		} else {
			printk(KERN_CRIT "Hangcheck: hangcheck value past margin!\n");
		}
	}
#if 0
	/*
	 * Enable to investigate delays in detail
	 */
	printk("Hangcheck: called %Ld ns since last time (%Ld ns overshoot)\n",
			tsc_diff, tsc_diff - hangcheck_tick*TIMER_FREQ);
#endif
	mod_timer(&hangcheck_ticktock, jiffies + (hangcheck_tick*HZ));
	hangcheck_tsc = monotonic_clock();
}


static int __init hangcheck_init(void)
{
	printk("Hangcheck: starting hangcheck timer %s (tick is %d seconds, margin is %d seconds).\n",
	       VERSION_STR, hangcheck_tick, hangcheck_margin);
#if defined (HAVE_MONOTONIC)
	printk("Hangcheck: Using monotonic_clock().\n");
#else
	printk("Hangcheck: Using getrawmonotonic().\n");
#endif  /* HAVE_MONOTONIC */
	hangcheck_tsc_margin =
		(unsigned long long)(hangcheck_margin + hangcheck_tick);
	hangcheck_tsc_margin *= (unsigned long long)TIMER_FREQ;

	hangcheck_tsc = monotonic_clock();
	mod_timer(&hangcheck_ticktock, jiffies + (hangcheck_tick*HZ));

	return 0;
}


static void __exit hangcheck_exit(void)
{
	del_timer_sync(&hangcheck_ticktock);
        printk("Hangcheck: Stopped hangcheck timer.\n");
}

module_init(hangcheck_init);
module_exit(hangcheck_exit);
