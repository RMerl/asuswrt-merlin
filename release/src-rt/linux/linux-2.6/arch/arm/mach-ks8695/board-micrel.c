/*
 * arch/arm/mach-ks8695/board-micrel.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/gpio.h>
#include <mach/devices.h>

#include "generic.h"

#ifdef CONFIG_PCI
static int micrel_pci_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return KS8695_IRQ_EXTERN0;
}

static struct ks8695_pci_cfg __initdata micrel_pci = {
	.mode		= KS8695_MODE_MINIPCI,
	.map_irq	= micrel_pci_map_irq,
};
#endif


static void __init micrel_init(void)
{
	printk(KERN_INFO "Micrel KS8695 Development Board initializing\n");

	ks8695_register_gpios();

#ifdef CONFIG_PCI
	ks8695_init_pci(&micrel_pci);
#endif

	/* Add devices */
	ks8695_add_device_wan();	/* eth0 = WAN */
	ks8695_add_device_lan();	/* eth1 = LAN */
}

MACHINE_START(KS8695, "KS8695 Centaur Development Board")
	/* Maintainer: Micrel Semiconductor Inc. */
	.boot_params	= KS8695_SDRAM_PA + 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
