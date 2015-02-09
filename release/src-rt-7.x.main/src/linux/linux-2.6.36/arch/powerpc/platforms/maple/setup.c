/*
 *  Maple (970 eval board) setup code
 *
 *  (c) Copyright 2004 Benjamin Herrenschmidt (benh@kernel.crashing.org),
 *                     IBM Corp. 
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#undef DEBUG

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/major.h>
#include <linux/initrd.h>
#include <linux/vt_kern.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/root_dev.h>
#include <linux/serial.h>
#include <linux/smp.h>
#include <linux/bitops.h>
#include <linux/of_device.h>
#include <linux/memblock.h>

#include <asm/processor.h>
#include <asm/sections.h>
#include <asm/prom.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/pci-bridge.h>
#include <asm/iommu.h>
#include <asm/machdep.h>
#include <asm/dma.h>
#include <asm/cputable.h>
#include <asm/time.h>
#include <asm/mpic.h>
#include <asm/rtas.h>
#include <asm/udbg.h>
#include <asm/nvram.h>

#include "maple.h"

#ifdef DEBUG
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

static unsigned long maple_find_nvram_base(void)
{
	struct device_node *rtcs;
	unsigned long result = 0;

	/* find NVRAM device */
	rtcs = of_find_compatible_node(NULL, "nvram", "AMD8111");
	if (rtcs) {
		struct resource r;
		if (of_address_to_resource(rtcs, 0, &r)) {
			printk(KERN_EMERG "Maple: Unable to translate NVRAM"
			       " address\n");
			goto bail;
		}
		if (!(r.flags & IORESOURCE_IO)) {
			printk(KERN_EMERG "Maple: NVRAM address isn't PIO!\n");
			goto bail;
		}
		result = r.start;
	} else
		printk(KERN_EMERG "Maple: Unable to find NVRAM\n");
 bail:
	of_node_put(rtcs);
	return result;
}

static void maple_restart(char *cmd)
{
	unsigned int maple_nvram_base;
	const unsigned int *maple_nvram_offset, *maple_nvram_command;
	struct device_node *sp;

	maple_nvram_base = maple_find_nvram_base();
	if (maple_nvram_base == 0)
		goto fail;

	/* find service processor device */
	sp = of_find_node_by_name(NULL, "service-processor");
	if (!sp) {
		printk(KERN_EMERG "Maple: Unable to find Service Processor\n");
		goto fail;
	}
	maple_nvram_offset = of_get_property(sp, "restart-addr", NULL);
	maple_nvram_command = of_get_property(sp, "restart-value", NULL);
	of_node_put(sp);

	/* send command */
	outb_p(*maple_nvram_command, maple_nvram_base + *maple_nvram_offset);
	for (;;) ;
 fail:
	printk(KERN_EMERG "Maple: Manual Restart Required\n");
}

static void maple_power_off(void)
{
	unsigned int maple_nvram_base;
	const unsigned int *maple_nvram_offset, *maple_nvram_command;
	struct device_node *sp;

	maple_nvram_base = maple_find_nvram_base();
	if (maple_nvram_base == 0)
		goto fail;

	/* find service processor device */
	sp = of_find_node_by_name(NULL, "service-processor");
	if (!sp) {
		printk(KERN_EMERG "Maple: Unable to find Service Processor\n");
		goto fail;
	}
	maple_nvram_offset = of_get_property(sp, "power-off-addr", NULL);
	maple_nvram_command = of_get_property(sp, "power-off-value", NULL);
	of_node_put(sp);

	/* send command */
	outb_p(*maple_nvram_command, maple_nvram_base + *maple_nvram_offset);
	for (;;) ;
 fail:
	printk(KERN_EMERG "Maple: Manual Power-Down Required\n");
}

static void maple_halt(void)
{
	maple_power_off();
}

#ifdef CONFIG_SMP
struct smp_ops_t maple_smp_ops = {
	.probe		= smp_mpic_probe,
	.message_pass	= smp_mpic_message_pass,
	.kick_cpu	= smp_generic_kick_cpu,
	.setup_cpu	= smp_mpic_setup_cpu,
	.give_timebase	= smp_generic_give_timebase,
	.take_timebase	= smp_generic_take_timebase,
};
#endif /* CONFIG_SMP */

static void __init maple_use_rtas_reboot_and_halt_if_present(void)
{
	if (rtas_service_present("system-reboot") &&
	    rtas_service_present("power-off")) {
		ppc_md.restart = rtas_restart;
		ppc_md.power_off = rtas_power_off;
		ppc_md.halt = rtas_halt;
	}
}

void __init maple_setup_arch(void)
{
	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000;

	/* Setup SMP callback */
#ifdef CONFIG_SMP
	smp_ops = &maple_smp_ops;
#endif
	/* Lookup PCI hosts */
       	maple_pci_init();

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif
	maple_use_rtas_reboot_and_halt_if_present();

	printk(KERN_DEBUG "Using native/NAP idle loop\n");

	mmio_nvram_init();
}

/* 
 * Early initialization.
 */
static void __init maple_init_early(void)
{
	DBG(" -> maple_init_early\n");

	iommu_init_early_dart();

	DBG(" <- maple_init_early\n");
}

/*
 * This is almost identical to pSeries and CHRP. We need to make that
 * code generic at one point, with appropriate bits in the device-tree to
 * identify the presence of an HT APIC
 */
static void __init maple_init_IRQ(void)
{
	struct device_node *root, *np, *mpic_node = NULL;
	const unsigned int *opprop;
	unsigned long openpic_addr = 0;
	int naddr, n, i, opplen, has_isus = 0;
	struct mpic *mpic;
	unsigned int flags = MPIC_PRIMARY;

	/* Locate MPIC in the device-tree. Note that there is a bug
	 * in Maple device-tree where the type of the controller is
	 * open-pic and not interrupt-controller
	 */

	for_each_node_by_type(np, "interrupt-controller")
		if (of_device_is_compatible(np, "open-pic")) {
			mpic_node = np;
			break;
		}
	if (mpic_node == NULL)
		for_each_node_by_type(np, "open-pic") {
			mpic_node = np;
			break;
		}
	if (mpic_node == NULL) {
		printk(KERN_ERR
		       "Failed to locate the MPIC interrupt controller\n");
		return;
	}

	/* Find address list in /platform-open-pic */
	root = of_find_node_by_path("/");
	naddr = of_n_addr_cells(root);
	opprop = of_get_property(root, "platform-open-pic", &opplen);
	if (opprop != 0) {
		openpic_addr = of_read_number(opprop, naddr);
		has_isus = (opplen > naddr);
		printk(KERN_DEBUG "OpenPIC addr: %lx, has ISUs: %d\n",
		       openpic_addr, has_isus);
	}

	BUG_ON(openpic_addr == 0);

	/* Check for a big endian MPIC */
	if (of_get_property(np, "big-endian", NULL) != NULL)
		flags |= MPIC_BIG_ENDIAN;

	flags |= MPIC_U3_HT_IRQS | MPIC_WANTS_RESET;
	/* All U3/U4 are big-endian, older SLOF firmware doesn't encode this */
	flags |= MPIC_BIG_ENDIAN;

	/* Setup the openpic driver. More device-tree junks, we hard code no
	 * ISUs for now. I'll have to revisit some stuffs with the folks doing
	 * the firmware for those
	 */
	mpic = mpic_alloc(mpic_node, openpic_addr, flags,
			  /*has_isus ? 16 :*/ 0, 0, " MPIC     ");
	BUG_ON(mpic == NULL);

	/* Add ISUs */
	opplen /= sizeof(u32);
	for (n = 0, i = naddr; i < opplen; i += naddr, n++) {
		unsigned long isuaddr = of_read_number(opprop + i, naddr);
		mpic_assign_isu(mpic, n, isuaddr);
	}

	/* All ISUs are setup, complete initialization */
	mpic_init(mpic);
	ppc_md.get_irq = mpic_get_irq;
	of_node_put(mpic_node);
	of_node_put(root);
}

static void __init maple_progress(char *s, unsigned short hex)
{
	printk("*** %04x : %s\n", hex, s ? s : "");
}


/*
 * Called very early, MMU is off, device-tree isn't unflattened
 */
static int __init maple_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (!of_flat_dt_is_compatible(root, "Momentum,Maple") &&
	    !of_flat_dt_is_compatible(root, "Momentum,Apache"))
		return 0;
	/*
	 * On U3, the DART (iommu) must be allocated now since it
	 * has an impact on htab_initialize (due to the large page it
	 * occupies having to be broken up so the DART itself is not
	 * part of the cacheable linar mapping
	 */
	alloc_dart_table();

	hpte_init_native();

	return 1;
}

define_machine(maple) {
	.name			= "Maple",
	.probe			= maple_probe,
	.setup_arch		= maple_setup_arch,
	.init_early		= maple_init_early,
	.init_IRQ		= maple_init_IRQ,
	.pci_irq_fixup		= maple_pci_irq_fixup,
	.pci_get_legacy_ide_irq	= maple_pci_get_legacy_ide_irq,
	.restart		= maple_restart,
	.power_off		= maple_power_off,
	.halt			= maple_halt,
       	.get_boot_time		= maple_get_boot_time,
       	.set_rtc_time		= maple_set_rtc_time,
       	.get_rtc_time		= maple_get_rtc_time,
      	.calibrate_decr		= generic_calibrate_decr,
	.progress		= maple_progress,
	.power_save		= power4_idle,
};

#ifdef CONFIG_EDAC
/*
 * Register a platform device for CPC925 memory controller on
 * Motorola ATCA-6101 blade.
 */
#define MAPLE_CPC925_MODEL	"Motorola,ATCA-6101"
static int __init maple_cpc925_edac_setup(void)
{
	struct platform_device *pdev;
	struct device_node *np = NULL;
	struct resource r;
	const unsigned char *model;
	int ret;

	np = of_find_node_by_path("/");
	if (!np) {
		printk(KERN_ERR "%s: Unable to get root node\n", __func__);
		return -ENODEV;
	}

	model = (const unsigned char *)of_get_property(np, "model", NULL);
	if (!model) {
		printk(KERN_ERR "%s: Unabel to get model info\n", __func__);
		return -ENODEV;
	}

	ret = strcmp(model, MAPLE_CPC925_MODEL);
	of_node_put(np);

	if (ret != 0)
		return 0;

	np = of_find_node_by_type(NULL, "memory-controller");
	if (!np) {
		printk(KERN_ERR "%s: Unable to find memory-controller node\n",
			__func__);
		return -ENODEV;
	}

	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);

	if (ret < 0) {
		printk(KERN_ERR "%s: Unable to get memory-controller reg\n",
			__func__);
		return -ENODEV;
	}

	pdev = platform_device_register_simple("cpc925_edac", 0, &r, 1);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	printk(KERN_INFO "%s: CPC925 platform device created\n", __func__);

	return 0;
}
machine_device_initcall(maple, maple_cpc925_edac_setup);
#endif
