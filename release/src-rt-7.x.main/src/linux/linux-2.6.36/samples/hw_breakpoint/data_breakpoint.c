/*
 * data_breakpoint.c - Sample HW Breakpoint file to watch kernel data address
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * usage: insmod data_breakpoint.ko ksym=<ksym_name>
 *
 * This file is a kernel module that places a breakpoint over ksym_name kernel
 * variable using Hardware Breakpoint register. The corresponding handler which
 * prints a backtrace is invoked everytime a write operation is performed on
 * that variable.
 *
 * Copyright (C) IBM Corporation, 2009
 *
 * Author: K.Prasad <prasad@linux.vnet.ibm.com>
 */
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/kallsyms.h>

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

struct perf_event * __percpu *sample_hbp;

static char ksym_name[KSYM_NAME_LEN] = "pid_max";
module_param_string(ksym, ksym_name, KSYM_NAME_LEN, S_IRUGO);
MODULE_PARM_DESC(ksym, "Kernel symbol to monitor; this module will report any"
			" write operations on the kernel symbol");

static void sample_hbp_handler(struct perf_event *bp, int nmi,
			       struct perf_sample_data *data,
			       struct pt_regs *regs)
{
	printk(KERN_INFO "%s value is changed\n", ksym_name);
	dump_stack();
	printk(KERN_INFO "Dump stack from sample_hbp_handler\n");
}

static int __init hw_break_module_init(void)
{
	int ret;
	struct perf_event_attr attr;

	hw_breakpoint_init(&attr);
	attr.bp_addr = kallsyms_lookup_name(ksym_name);
	attr.bp_len = HW_BREAKPOINT_LEN_4;
	attr.bp_type = HW_BREAKPOINT_W | HW_BREAKPOINT_R;

	sample_hbp = register_wide_hw_breakpoint(&attr, sample_hbp_handler);
	if (IS_ERR((void __force *)sample_hbp)) {
		ret = PTR_ERR((void __force *)sample_hbp);
		goto fail;
	}

	printk(KERN_INFO "HW Breakpoint for %s write installed\n", ksym_name);

	return 0;

fail:
	printk(KERN_INFO "Breakpoint registration failed\n");

	return ret;
}

static void __exit hw_break_module_exit(void)
{
	unregister_wide_hw_breakpoint(sample_hbp);
	printk(KERN_INFO "HW Breakpoint for %s write uninstalled\n", ksym_name);
}

module_init(hw_break_module_init);
module_exit(hw_break_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("K.Prasad");
MODULE_DESCRIPTION("ksym breakpoint");
