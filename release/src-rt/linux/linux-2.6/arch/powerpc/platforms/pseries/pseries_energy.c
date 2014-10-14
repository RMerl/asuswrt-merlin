/*
 * POWER platform energy management driver
 * Copyright (C) 2010 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This pseries platform device driver provides access to
 * platform energy management capabilities.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <asm/cputhreads.h>
#include <asm/page.h>
#include <asm/hvcall.h>


#define MODULE_VERS "1.0"
#define MODULE_NAME "pseries_energy"

/* Driver flags */

static int sysfs_entries;

/* Helper routines */

/*
 * Routine to detect firmware support for hcall
 * return 1 if H_BEST_ENERGY is supported
 * else return 0
 */

static int check_for_h_best_energy(void)
{
	struct device_node *rtas = NULL;
	const char *hypertas, *s;
	int length;
	int rc = 0;

	rtas = of_find_node_by_path("/rtas");
	if (!rtas)
		return 0;

	hypertas = of_get_property(rtas, "ibm,hypertas-functions", &length);
	if (!hypertas) {
		of_node_put(rtas);
		return 0;
	}

	/* hypertas will have list of strings with hcall names */
	for (s = hypertas; s < hypertas + length; s += strlen(s) + 1) {
		if (!strncmp("hcall-best-energy-1", s, 19)) {
			rc = 1; /* Found the string */
			break;
		}
	}
	of_node_put(rtas);
	return rc;
}

/* Helper Routines to convert between drc_index to cpu numbers */

static u32 cpu_to_drc_index(int cpu)
{
	struct device_node *dn = NULL;
	const int *indexes;
	int i;
	int rc = 1;
	u32 ret = 0;

	dn = of_find_node_by_path("/cpus");
	if (dn == NULL)
		goto err;
	indexes = of_get_property(dn, "ibm,drc-indexes", NULL);
	if (indexes == NULL)
		goto err_of_node_put;
	/* Convert logical cpu number to core number */
	i = cpu_core_index_of_thread(cpu);
	/*
	 * The first element indexes[0] is the number of drc_indexes
	 * returned in the list.  Hence i+1 will get the drc_index
	 * corresponding to core number i.
	 */
	WARN_ON(i > indexes[0]);
	ret = indexes[i + 1];
	rc = 0;

err_of_node_put:
	of_node_put(dn);
err:
	if (rc)
		printk(KERN_WARNING "cpu_to_drc_index(%d) failed", cpu);
	return ret;
}

static int drc_index_to_cpu(u32 drc_index)
{
	struct device_node *dn = NULL;
	const int *indexes;
	int i, cpu = 0;
	int rc = 1;

	dn = of_find_node_by_path("/cpus");
	if (dn == NULL)
		goto err;
	indexes = of_get_property(dn, "ibm,drc-indexes", NULL);
	if (indexes == NULL)
		goto err_of_node_put;
	/*
	 * First element in the array is the number of drc_indexes
	 * returned.  Search through the list to find the matching
	 * drc_index and get the core number
	 */
	for (i = 0; i < indexes[0]; i++) {
		if (indexes[i + 1] == drc_index)
			break;
	}
	/* Convert core number to logical cpu number */
	cpu = cpu_first_thread_of_core(i);
	rc = 0;

err_of_node_put:
	of_node_put(dn);
err:
	if (rc)
		printk(KERN_WARNING "drc_index_to_cpu(%d) failed", drc_index);
	return cpu;
}

/*
 * pseries hypervisor call H_BEST_ENERGY provides hints to OS on
 * preferred logical cpus to activate or deactivate for optimized
 * energy consumption.
 */

#define FLAGS_MODE1	0x004E200000080E01
#define FLAGS_MODE2	0x004E200000080401
#define FLAGS_ACTIVATE  0x100

static ssize_t get_best_energy_list(char *page, int activate)
{
	int rc, cnt, i, cpu;
	unsigned long retbuf[PLPAR_HCALL9_BUFSIZE];
	unsigned long flags = 0;
	u32 *buf_page;
	char *s = page;

	buf_page = (u32 *) get_zeroed_page(GFP_KERNEL);
	if (!buf_page)
		return -ENOMEM;

	flags = FLAGS_MODE1;
	if (activate)
		flags |= FLAGS_ACTIVATE;

	rc = plpar_hcall9(H_BEST_ENERGY, retbuf, flags, 0, __pa(buf_page),
				0, 0, 0, 0, 0, 0);
	if (rc != H_SUCCESS) {
		free_page((unsigned long) buf_page);
		return -EINVAL;
	}

	cnt = retbuf[0];
	for (i = 0; i < cnt; i++) {
		cpu = drc_index_to_cpu(buf_page[2*i+1]);
		if ((cpu_online(cpu) && !activate) ||
		    (!cpu_online(cpu) && activate))
			s += sprintf(s, "%d,", cpu);
	}
	if (s > page) { /* Something to show */
		s--; /* Suppress last comma */
		s += sprintf(s, "\n");
	}

	free_page((unsigned long) buf_page);
	return s-page;
}

static ssize_t get_best_energy_data(struct sys_device *dev,
					char *page, int activate)
{
	int rc;
	unsigned long retbuf[PLPAR_HCALL9_BUFSIZE];
	unsigned long flags = 0;

	flags = FLAGS_MODE2;
	if (activate)
		flags |= FLAGS_ACTIVATE;

	rc = plpar_hcall9(H_BEST_ENERGY, retbuf, flags,
				cpu_to_drc_index(dev->id),
				0, 0, 0, 0, 0, 0, 0);

	if (rc != H_SUCCESS)
		return -EINVAL;

	return sprintf(page, "%lu\n", retbuf[1] >> 32);
}

/* Wrapper functions */

static ssize_t cpu_activate_hint_list_show(struct sysdev_class *class,
			struct sysdev_class_attribute *attr, char *page)
{
	return get_best_energy_list(page, 1);
}

static ssize_t cpu_deactivate_hint_list_show(struct sysdev_class *class,
			struct sysdev_class_attribute *attr, char *page)
{
	return get_best_energy_list(page, 0);
}

static ssize_t percpu_activate_hint_show(struct sys_device *dev,
			struct sysdev_attribute *attr, char *page)
{
	return get_best_energy_data(dev, page, 1);
}

static ssize_t percpu_deactivate_hint_show(struct sys_device *dev,
			struct sysdev_attribute *attr, char *page)
{
	return get_best_energy_data(dev, page, 0);
}

/*
 * Create sysfs interface:
 * /sys/devices/system/cpu/pseries_activate_hint_list
 * /sys/devices/system/cpu/pseries_deactivate_hint_list
 *	Comma separated list of cpus to activate or deactivate
 * /sys/devices/system/cpu/cpuN/pseries_activate_hint
 * /sys/devices/system/cpu/cpuN/pseries_deactivate_hint
 *	Per-cpu value of the hint
 */

struct sysdev_class_attribute attr_cpu_activate_hint_list =
		_SYSDEV_CLASS_ATTR(pseries_activate_hint_list, 0444,
		cpu_activate_hint_list_show, NULL);

struct sysdev_class_attribute attr_cpu_deactivate_hint_list =
		_SYSDEV_CLASS_ATTR(pseries_deactivate_hint_list, 0444,
		cpu_deactivate_hint_list_show, NULL);

struct sysdev_attribute attr_percpu_activate_hint =
		_SYSDEV_ATTR(pseries_activate_hint, 0444,
		percpu_activate_hint_show, NULL);

struct sysdev_attribute attr_percpu_deactivate_hint =
		_SYSDEV_ATTR(pseries_deactivate_hint, 0444,
		percpu_deactivate_hint_show, NULL);

static int __init pseries_energy_init(void)
{
	int cpu, err;
	struct sys_device *cpu_sys_dev;

	if (!check_for_h_best_energy()) {
		printk(KERN_INFO "Hypercall H_BEST_ENERGY not supported\n");
		return 0;
	}
	/* Create the sysfs files */
	err = sysfs_create_file(&cpu_sysdev_class.kset.kobj,
				&attr_cpu_activate_hint_list.attr);
	if (!err)
		err = sysfs_create_file(&cpu_sysdev_class.kset.kobj,
				&attr_cpu_deactivate_hint_list.attr);

	if (err)
		return err;
	for_each_possible_cpu(cpu) {
		cpu_sys_dev = get_cpu_sysdev(cpu);
		err = sysfs_create_file(&cpu_sys_dev->kobj,
				&attr_percpu_activate_hint.attr);
		if (err)
			break;
		err = sysfs_create_file(&cpu_sys_dev->kobj,
				&attr_percpu_deactivate_hint.attr);
		if (err)
			break;
	}

	if (err)
		return err;

	sysfs_entries = 1; /* Removed entries on cleanup */
	return 0;

}

static void __exit pseries_energy_cleanup(void)
{
	int cpu;
	struct sys_device *cpu_sys_dev;

	if (!sysfs_entries)
		return;

	/* Remove the sysfs files */
	sysfs_remove_file(&cpu_sysdev_class.kset.kobj,
				&attr_cpu_activate_hint_list.attr);

	sysfs_remove_file(&cpu_sysdev_class.kset.kobj,
				&attr_cpu_deactivate_hint_list.attr);

	for_each_possible_cpu(cpu) {
		cpu_sys_dev = get_cpu_sysdev(cpu);
		sysfs_remove_file(&cpu_sys_dev->kobj,
				&attr_percpu_activate_hint.attr);
		sysfs_remove_file(&cpu_sys_dev->kobj,
				&attr_percpu_deactivate_hint.attr);
	}
}

module_init(pseries_energy_init);
module_exit(pseries_energy_cleanup);
MODULE_DESCRIPTION("Driver for pSeries platform energy management");
MODULE_AUTHOR("Vaidyanathan Srinivasan");
MODULE_LICENSE("GPL");
