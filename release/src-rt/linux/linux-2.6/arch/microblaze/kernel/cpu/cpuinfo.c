/*
 * Copyright (C) 2007-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2007-2009 PetaLogix
 * Copyright (C) 2007 John Williams <john.williams@petalogix.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <asm/cpuinfo.h>
#include <asm/pvr.h>

const struct cpu_ver_key cpu_ver_lookup[] = {
	/* These key value are as per MBV field in PVR0 */
	{"5.00.a", 0x01},
	{"5.00.b", 0x02},
	{"5.00.c", 0x03},
	{"6.00.a", 0x04},
	{"6.00.b", 0x06},
	{"7.00.a", 0x05},
	{"7.00.b", 0x07},
	{"7.10.a", 0x08},
	{"7.10.b", 0x09},
	{"7.10.c", 0x0a},
	{"7.10.d", 0x0b},
	{"7.20.a", 0x0c},
	{"7.20.b", 0x0d},
	{"7.20.c", 0x0e},
	{"7.20.d", 0x0f},
	{"7.30.a", 0x10},
	{"7.30.b", 0x11},
	{"8.00.a", 0x12},
	{"8.00.b", 0x13},
	{"8.10.a", 0x14},
	{NULL, 0},
};

/*
 * FIXME Not sure if the actual key is defined by Xilinx in the PVR
 */
const struct family_string_key family_string_lookup[] = {
	{"virtex2", 0x4},
	{"virtex2pro", 0x5},
	{"spartan3", 0x6},
	{"virtex4", 0x7},
	{"virtex5", 0x8},
	{"spartan3e", 0x9},
	{"spartan3a", 0xa},
	{"spartan3an", 0xb},
	{"spartan3adsp", 0xc},
	{"spartan6", 0xd},
	{"virtex6", 0xe},
	/* FIXME There is no key code defined for spartan2 */
	{"spartan2", 0xf0},
	{NULL, 0},
};

struct cpuinfo cpuinfo;

void __init setup_cpuinfo(void)
{
	struct device_node *cpu = NULL;

	cpu = (struct device_node *) of_find_node_by_type(NULL, "cpu");
	if (!cpu)
		printk(KERN_ERR "You don't have cpu!!!\n");

	printk(KERN_INFO "%s: initialising\n", __func__);

	switch (cpu_has_pvr()) {
	case 0:
		printk(KERN_WARNING
			"%s: No PVR support. Using static CPU info from FDT\n",
			__func__);
		set_cpuinfo_static(&cpuinfo, cpu);
		break;
/* FIXME I found weird behavior with MB 7.00.a/b 7.10.a
 * please do not use FULL PVR with MMU */
	case 1:
		printk(KERN_INFO "%s: Using full CPU PVR support\n",
			__func__);
		set_cpuinfo_static(&cpuinfo, cpu);
		set_cpuinfo_pvr_full(&cpuinfo, cpu);
		break;
	default:
		printk(KERN_WARNING "%s: Unsupported PVR setting\n", __func__);
		set_cpuinfo_static(&cpuinfo, cpu);
	}
}
