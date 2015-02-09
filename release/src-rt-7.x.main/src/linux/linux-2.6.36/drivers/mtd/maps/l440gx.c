/*
 * BIOS Flash chip on Intel 440GX board.
 *
 * Bugs this currently does not work under linuxBIOS.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>

#define PIIXE_IOBASE_RESOURCE	11

#define WINDOW_ADDR 0xfff00000
#define WINDOW_SIZE 0x00100000
#define BUSWIDTH 1

static u32 iobase;
#define IOBASE iobase
#define TRIBUF_PORT (IOBASE+0x37)
#define VPP_PORT (IOBASE+0x28)

static struct mtd_info *mymtd;


/* Is this really the vpp port? */
static void l440gx_set_vpp(struct map_info *map, int vpp)
{
	unsigned long l;

	l = inl(VPP_PORT);
	if (vpp) {
		l |= 1;
	} else {
		l &= ~1;
	}
	outl(l, VPP_PORT);
}

static struct map_info l440gx_map = {
	.name = "L440GX BIOS",
	.size = WINDOW_SIZE,
	.bankwidth = BUSWIDTH,
	.phys = WINDOW_ADDR,
};

static int __init init_l440gx(void)
{
	struct pci_dev *dev, *pm_dev;
	struct resource *pm_iobase;
	__u16 word;

	dev = pci_get_device(PCI_VENDOR_ID_INTEL,
		PCI_DEVICE_ID_INTEL_82371AB_0, NULL);

	pm_dev = pci_get_device(PCI_VENDOR_ID_INTEL,
		PCI_DEVICE_ID_INTEL_82371AB_3, NULL);

	pci_dev_put(dev);

	if (!dev || !pm_dev) {
		printk(KERN_NOTICE "L440GX flash mapping: failed to find PIIX4 ISA bridge, cannot continue\n");
		pci_dev_put(pm_dev);
		return -ENODEV;
	}

	l440gx_map.virt = ioremap_nocache(WINDOW_ADDR, WINDOW_SIZE);

	if (!l440gx_map.virt) {
		printk(KERN_WARNING "Failed to ioremap L440GX flash region\n");
		pci_dev_put(pm_dev);
		return -ENOMEM;
	}
	simple_map_init(&l440gx_map);
	printk(KERN_NOTICE "window_addr = 0x%08lx\n", (unsigned long)l440gx_map.virt);

	/* Setup the pm iobase resource
	 * This code should move into some kind of generic bridge
	 * driver but for the moment I'm content with getting the
	 * allocation correct.
	 */
	pm_iobase = &pm_dev->resource[PIIXE_IOBASE_RESOURCE];
	if (!(pm_iobase->flags & IORESOURCE_IO)) {
		pm_iobase->name = "pm iobase";
		pm_iobase->start = 0;
		pm_iobase->end = 63;
		pm_iobase->flags = IORESOURCE_IO;

		/* Put the current value in the resource */
		pci_read_config_dword(pm_dev, 0x40, &iobase);
		iobase &= ~1;
		pm_iobase->start += iobase & ~1;
		pm_iobase->end += iobase & ~1;

		pci_dev_put(pm_dev);

		/* Allocate the resource region */
		if (pci_assign_resource(pm_dev, PIIXE_IOBASE_RESOURCE) != 0) {
			pci_dev_put(dev);
			pci_dev_put(pm_dev);
			printk(KERN_WARNING "Could not allocate pm iobase resource\n");
			iounmap(l440gx_map.virt);
			return -ENXIO;
		}
	}
	/* Set the iobase */
	iobase = pm_iobase->start;
	pci_write_config_dword(pm_dev, 0x40, iobase | 1);


	/* Set XBCS# */
	pci_read_config_word(dev, 0x4e, &word);
	word |= 0x4;
        pci_write_config_word(dev, 0x4e, word);

	/* Supply write voltage to the chip */
	l440gx_set_vpp(&l440gx_map, 1);

	/* Enable the gate on the WE line */
	outb(inb(TRIBUF_PORT) & ~1, TRIBUF_PORT);

       	printk(KERN_NOTICE "Enabled WE line to L440GX BIOS flash chip.\n");

	mymtd = do_map_probe("jedec_probe", &l440gx_map);
	if (!mymtd) {
		printk(KERN_NOTICE "JEDEC probe on BIOS chip failed. Using ROM\n");
		mymtd = do_map_probe("map_rom", &l440gx_map);
	}
	if (mymtd) {
		mymtd->owner = THIS_MODULE;

		add_mtd_device(mymtd);
		return 0;
	}

	iounmap(l440gx_map.virt);
	return -ENXIO;
}

static void __exit cleanup_l440gx(void)
{
	del_mtd_device(mymtd);
	map_destroy(mymtd);

	iounmap(l440gx_map.virt);
}

module_init(init_l440gx);
module_exit(cleanup_l440gx);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("MTD map driver for BIOS chips on Intel L440GX motherboards");
