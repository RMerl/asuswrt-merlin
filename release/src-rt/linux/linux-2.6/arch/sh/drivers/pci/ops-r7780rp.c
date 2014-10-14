/*
 * Author:  Ian DaSilva (idasilva@mvista.com)
 *
 * Highly leveraged from pci-bigsur.c, written by Dustin McIntire.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * PCI initialization for the Renesas SH7780 Highlander R7780RP-1 board
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/r7780rp.h>
#include <asm/io.h>
#include "pci-sh4.h"

static char r7780rp_irq_tab[] __initdata = {
	0, 1, 2, 3,
};

static char r7780mp_irq_tab[] __initdata = {
	65, 66, 67, 68,
};

int __init pcibios_map_platform_irq(struct pci_dev *pdev, u8 slot, u8 pin)
{
	if (mach_is_r7780rp())
		return r7780rp_irq_tab[slot];
	if (mach_is_r7780mp() || mach_is_r7785rp())
		return r7780mp_irq_tab[slot];

	printk(KERN_ERR "PCI: Bad IRQ mapping "
	       "request for slot %d, func %d\n", slot, pin-1);

	return -1;
}

static struct resource sh7780_io_resource = {
	.name	= "SH7780_IO",
	.start	= 0x2000,
	.end	= 0x2000 + SH7780_PCI_IO_SIZE - 1,
	.flags	= IORESOURCE_IO
};

static struct resource sh7780_mem_resource = {
	.name	= "SH7780_mem",
	.start	= SH7780_PCI_MEMORY_BASE,
	.end	= SH7780_PCI_MEMORY_BASE + SH7780_PCI_MEM_SIZE - 1,
	.flags	= IORESOURCE_MEM
};

extern struct pci_ops sh7780_pci_ops;

struct pci_channel board_pci_channels[] = {
	{ &sh4_pci_ops, &sh7780_io_resource, &sh7780_mem_resource, 0, 0xff },
	{ NULL, NULL, NULL, 0, 0 },
};
EXPORT_SYMBOL(board_pci_channels);

static struct sh4_pci_address_map sh7780_pci_map = {
	.window0	= {
		.base	= SH7780_CS2_BASE_ADDR,
		.size	= 0x04000000,
	},

	.window1	= {
		.base	= SH7780_CS3_BASE_ADDR,
		.size	= 0x04000000,
	},

	.flags	= SH4_PCIC_NO_RESET,
};

int __init pcibios_init_platform(void)
{
	return sh7780_pcic_init(&sh7780_pci_map);
}
