/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <asm/processor.h>
#include <asm/sections.h>
#include <asm/byteorder.h>
#include <asm/hv_driver.h>
#include <hv/drv_pcie_rc_intf.h>


/*
 * Initialization flow and process
 * -------------------------------
 *
 * This files contains the routines to search for PCI buses,
 * enumerate the buses, and configure any attached devices.
 *
 * There are two entry points here:
 * 1) tile_pci_init
 *    This sets up the pci_controller structs, and opens the
 *    FDs to the hypervisor.  This is called from setup_arch() early
 *    in the boot process.
 * 2) pcibios_init
 *    This probes the PCI bus(es) for any attached hardware.  It's
 *    called by subsys_initcall.  All of the real work is done by the
 *    generic Linux PCI layer.
 *
 */

/*
 * This flag tells if the platform is TILEmpower that needs
 * special configuration for the PLX switch chip.
 */
int __write_once tile_plx_gen1;

static struct pci_controller controllers[TILE_NUM_PCIE];
static int num_controllers;

static struct pci_ops tile_cfg_ops;


/*
 * We don't need to worry about the alignment of resources.
 */
resource_size_t pcibios_align_resource(void *data, const struct resource *res,
			    resource_size_t size, resource_size_t align)
{
	return res->start;
}
EXPORT_SYMBOL(pcibios_align_resource);

/*
 * Open a FD to the hypervisor PCI device.
 *
 * controller_id is the controller number, config type is 0 or 1 for
 * config0 or config1 operations.
 */
static int __init tile_pcie_open(int controller_id, int config_type)
{
	char filename[32];
	int fd;

	sprintf(filename, "pcie/%d/config%d", controller_id, config_type);

	fd = hv_dev_open((HV_VirtAddr)filename, 0);

	return fd;
}


/*
 * Get the IRQ numbers from the HV and set up the handlers for them.
 */
static int __init tile_init_irqs(int controller_id,
				 struct pci_controller *controller)
{
	char filename[32];
	int fd;
	int ret;
	int x;
	struct pcie_rc_config rc_config;

	sprintf(filename, "pcie/%d/ctl", controller_id);
	fd = hv_dev_open((HV_VirtAddr)filename, 0);
	if (fd < 0) {
		pr_err("PCI: hv_dev_open(%s) failed\n", filename);
		return -1;
	}
	ret = hv_dev_pread(fd, 0, (HV_VirtAddr)(&rc_config),
			   sizeof(rc_config), PCIE_RC_CONFIG_MASK_OFF);
	hv_dev_close(fd);
	if (ret != sizeof(rc_config)) {
		pr_err("PCI: wanted %zd bytes, got %d\n",
		       sizeof(rc_config), ret);
		return -1;
	}
	/* Record irq_base so that we can map INTx to IRQ # later. */
	controller->irq_base = rc_config.intr;

	for (x = 0; x < 4; x++)
		tile_irq_activate(rc_config.intr + x,
				  TILE_IRQ_HW_CLEAR);

	if (rc_config.plx_gen1)
		controller->plx_gen1 = 1;

	return 0;
}

/*
 * First initialization entry point, called from setup_arch().
 *
 * Find valid controllers and fill in pci_controller structs for each
 * of them.
 *
 * Returns the number of controllers discovered.
 */
int __init tile_pci_init(void)
{
	int i;

	pr_info("PCI: Searching for controllers...\n");

	/* Do any configuration we need before using the PCIe */

	for (i = 0; i < TILE_NUM_PCIE; i++) {
		int hv_cfg_fd0 = -1;
		int hv_cfg_fd1 = -1;
		int hv_mem_fd = -1;
		char name[32];
		struct pci_controller *controller;

		/*
		 * Open the fd to the HV.  If it fails then this
		 * device doesn't exist.
		 */
		hv_cfg_fd0 = tile_pcie_open(i, 0);
		if (hv_cfg_fd0 < 0)
			continue;
		hv_cfg_fd1 = tile_pcie_open(i, 1);
		if (hv_cfg_fd1 < 0) {
			pr_err("PCI: Couldn't open config fd to HV "
			    "for controller %d\n", i);
			goto err_cont;
		}

		sprintf(name, "pcie/%d/mem", i);
		hv_mem_fd = hv_dev_open((HV_VirtAddr)name, 0);
		if (hv_mem_fd < 0) {
			pr_err("PCI: Could not open mem fd to HV!\n");
			goto err_cont;
		}

		pr_info("PCI: Found PCI controller #%d\n", i);

		controller = &controllers[num_controllers];

		controller->index = num_controllers;
		controller->hv_cfg_fd[0] = hv_cfg_fd0;
		controller->hv_cfg_fd[1] = hv_cfg_fd1;
		controller->hv_mem_fd = hv_mem_fd;
		controller->first_busno = 0;
		controller->last_busno = 0xff;
		controller->ops = &tile_cfg_ops;

		num_controllers++;
		continue;

err_cont:
		if (hv_cfg_fd0 >= 0)
			hv_dev_close(hv_cfg_fd0);
		if (hv_cfg_fd1 >= 0)
			hv_dev_close(hv_cfg_fd1);
		if (hv_mem_fd >= 0)
			hv_dev_close(hv_mem_fd);
		continue;
	}

	/*
	 * Before using the PCIe, see if we need to do any platform-specific
	 * configuration, such as the PLX switch Gen 1 issue on TILEmpower.
	 */
	for (i = 0; i < num_controllers; i++) {
		struct pci_controller *controller = &controllers[i];

		if (controller->plx_gen1)
			tile_plx_gen1 = 1;
	}

	return num_controllers;
}

/*
 * (pin - 1) converts from the PCI standard's [1:4] convention to
 * a normal [0:3] range.
 */
static int tile_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	struct pci_controller *controller =
		(struct pci_controller *)dev->sysdata;
	return (pin - 1) + controller->irq_base;
}


static void __init fixup_read_and_payload_sizes(void)
{
	struct pci_dev *dev = NULL;
	int smallest_max_payload = 0x1; /* Tile maxes out at 256 bytes. */
	int max_read_size = 0x2; /* Limit to 512 byte reads. */
	u16 new_values;

	/* Scan for the smallest maximum payload size. */
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		int pcie_caps_offset;
		u32 devcap;
		int max_payload;

		pcie_caps_offset = pci_find_capability(dev, PCI_CAP_ID_EXP);
		if (pcie_caps_offset == 0)
			continue;

		pci_read_config_dword(dev, pcie_caps_offset + PCI_EXP_DEVCAP,
				      &devcap);
		max_payload = devcap & PCI_EXP_DEVCAP_PAYLOAD;
		if (max_payload < smallest_max_payload)
			smallest_max_payload = max_payload;
	}

	/* Now, set the max_payload_size for all devices to that value. */
	new_values = (max_read_size << 12) | (smallest_max_payload << 5);
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		int pcie_caps_offset;
		u16 devctl;

		pcie_caps_offset = pci_find_capability(dev, PCI_CAP_ID_EXP);
		if (pcie_caps_offset == 0)
			continue;

		pci_read_config_word(dev, pcie_caps_offset + PCI_EXP_DEVCTL,
				     &devctl);
		devctl &= ~(PCI_EXP_DEVCTL_PAYLOAD | PCI_EXP_DEVCTL_READRQ);
		devctl |= new_values;
		pci_write_config_word(dev, pcie_caps_offset + PCI_EXP_DEVCTL,
				      devctl);
	}
}


/*
 * Second PCI initialization entry point, called by subsys_initcall.
 *
 * The controllers have been set up by the time we get here, by a call to
 * tile_pci_init.
 */
static int __init pcibios_init(void)
{
	int i;

	pr_info("PCI: Probing PCI hardware\n");

	/*
	 * Delay a bit in case devices aren't ready.  Some devices are
	 * known to require at least 20ms here, but we use a more
	 * conservative value.
	 */
	mdelay(250);

	/* Scan all of the recorded PCI controllers.  */
	for (i = 0; i < num_controllers; i++) {
		struct pci_controller *controller = &controllers[i];
		struct pci_bus *bus;

		if (tile_init_irqs(i, controller)) {
			pr_err("PCI: Could not initialize IRQS\n");
			continue;
		}

		pr_info("PCI: initializing controller #%d\n", i);

		/*
		 * This comes from the generic Linux PCI driver.
		 *
		 * It reads the PCI tree for this bus into the Linux
		 * data structures.
		 *
		 * This is inlined in linux/pci.h and calls into
		 * pci_scan_bus_parented() in probe.c.
		 */
		bus = pci_scan_bus(0, controller->ops, controller);
		controller->root_bus = bus;
		controller->last_busno = bus->subordinate;

	}

	/* Do machine dependent PCI interrupt routing */
	pci_fixup_irqs(pci_common_swizzle, tile_map_irq);

	/*
	 * This comes from the generic Linux PCI driver.
	 *
	 * It allocates all of the resources (I/O memory, etc)
	 * associated with the devices read in above.
	 */

	pci_assign_unassigned_resources();

	/* Configure the max_read_size and max_payload_size values. */
	fixup_read_and_payload_sizes();

	/* Record the I/O resources in the PCI controller structure. */
	for (i = 0; i < num_controllers; i++) {
		struct pci_bus *root_bus = controllers[i].root_bus;
		struct pci_bus *next_bus;
		struct pci_dev *dev;

		list_for_each_entry(dev, &root_bus->devices, bus_list) {
			/* Find the PCI host controller, ie. the 1st bridge. */
			if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI &&
				(PCI_SLOT(dev->devfn) == 0)) {
				next_bus = dev->subordinate;
				controllers[i].mem_resources[0] =
					*next_bus->resource[0];
				controllers[i].mem_resources[1] =
					 *next_bus->resource[1];
				controllers[i].mem_resources[2] =
					 *next_bus->resource[2];

				break;
			}
		}

	}

	return 0;
}
subsys_initcall(pcibios_init);

/*
 * No bus fixups needed.
 */
void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
	/* Nothing needs to be done. */
}

/*
 * This can be called from the generic PCI layer, but doesn't need to
 * do anything.
 */
char __devinit *pcibios_setup(char *str)
{
	/* Nothing needs to be done. */
	return str;
}

/*
 * This is called from the generic Linux layer.
 */
void __init pcibios_update_irq(struct pci_dev *dev, int irq)
{
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

/*
 * Enable memory and/or address decoding, as appropriate, for the
 * device described by the 'dev' struct.
 *
 * This is called from the generic PCI layer, and can be called
 * for bridges or endpoints.
 */
int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	u8 header_type;
	int i;
	struct resource *r;

	pci_read_config_byte(dev, PCI_HEADER_TYPE, &header_type);

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
		/*
		 * For bridges, we enable both memory and I/O decoding
		 * in call cases.
		 */
		cmd |= PCI_COMMAND_IO;
		cmd |= PCI_COMMAND_MEMORY;
	} else {
		/*
		 * For endpoints, we enable memory and/or I/O decoding
		 * only if they have a memory resource of that type.
		 */
		for (i = 0; i < 6; i++) {
			r = &dev->resource[i];
			if (r->flags & IORESOURCE_UNSET) {
				pr_err("PCI: Device %s not available "
				       "because of resource collisions\n",
				       pci_name(dev));
				return -EINVAL;
			}
			if (r->flags & IORESOURCE_IO)
				cmd |= PCI_COMMAND_IO;
			if (r->flags & IORESOURCE_MEM)
				cmd |= PCI_COMMAND_MEMORY;
		}
	}

	/*
	 * We only write the command if it changed.
	 */
	if (cmd != old_cmd)
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	return 0;
}

void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long max)
{
	unsigned long start = pci_resource_start(dev, bar);
	unsigned long len = pci_resource_len(dev, bar);
	unsigned long flags = pci_resource_flags(dev, bar);

	if (!len)
		return NULL;
	if (max && len > max)
		len = max;

	if (!(flags & IORESOURCE_MEM)) {
		pr_info("PCI: Trying to map invalid resource %#lx\n", flags);
		start = 0;
	}

	return (void __iomem *)start;
}
EXPORT_SYMBOL(pci_iomap);


/****************************************************************
 *
 * Tile PCI config space read/write routines
 *
 ****************************************************************/

/*
 * These are the normal read and write ops
 * These are expanded with macros from  pci_bus_read_config_byte() etc.
 *
 * devfn is the combined PCI slot & function.
 *
 * offset is in bytes, from the start of config space for the
 * specified bus & slot.
 */

static int __devinit tile_cfg_read(struct pci_bus *bus,
				   unsigned int devfn,
				   int offset,
				   int size,
				   u32 *val)
{
	struct pci_controller *controller = bus->sysdata;
	int busnum = bus->number & 0xff;
	int slot = (devfn >> 3) & 0x1f;
	int function = devfn & 0x7;
	u32 addr;
	int config_mode = 1;

	/*
	 * There is no bridge between the Tile and bus 0, so we
	 * use config0 to talk to bus 0.
	 *
	 * If we're talking to a bus other than zero then we
	 * must have found a bridge.
	 */
	if (busnum == 0) {
		/*
		 * We fake an empty slot for (busnum == 0) && (slot > 0),
		 * since there is only one slot on bus 0.
		 */
		if (slot) {
			*val = 0xFFFFFFFF;
			return 0;
		}
		config_mode = 0;
	}

	addr = busnum << 20;		/* Bus in 27:20 */
	addr |= slot << 15;		/* Slot (device) in 19:15 */
	addr |= function << 12;		/* Function is in 14:12 */
	addr |= (offset & 0xFFF);	/* byte address in 0:11 */

	return hv_dev_pread(controller->hv_cfg_fd[config_mode], 0,
			    (HV_VirtAddr)(val), size, addr);
}


/*
 * See tile_cfg_read() for relevant comments.
 * Note that "val" is the value to write, not a pointer to that value.
 */
static int __devinit tile_cfg_write(struct pci_bus *bus,
				    unsigned int devfn,
				    int offset,
				    int size,
				    u32 val)
{
	struct pci_controller *controller = bus->sysdata;
	int busnum = bus->number & 0xff;
	int slot = (devfn >> 3) & 0x1f;
	int function = devfn & 0x7;
	u32 addr;
	int config_mode = 1;
	HV_VirtAddr valp = (HV_VirtAddr)&val;

	/*
	 * For bus 0 slot 0 we use config 0 accesses.
	 */
	if (busnum == 0) {
		/*
		 * We fake an empty slot for (busnum == 0) && (slot > 0),
		 * since there is only one slot on bus 0.
		 */
		if (slot)
			return 0;
		config_mode = 0;
	}

	addr = busnum << 20;		/* Bus in 27:20 */
	addr |= slot << 15;		/* Slot (device) in 19:15 */
	addr |= function << 12;		/* Function is in 14:12 */
	addr |= (offset & 0xFFF);	/* byte address in 0:11 */

#ifdef __BIG_ENDIAN
	/* Point to the correct part of the 32-bit "val". */
	valp += 4 - size;
#endif

	return hv_dev_pwrite(controller->hv_cfg_fd[config_mode], 0,
			     valp, size, addr);
}


static struct pci_ops tile_cfg_ops = {
	.read =         tile_cfg_read,
	.write =        tile_cfg_write,
};


/*
 * In the following, each PCI controller's mem_resources[1]
 * represents its (non-prefetchable) PCI memory resource.
 * mem_resources[0] and mem_resources[2] refer to its PCI I/O and
 * prefetchable PCI memory resources, respectively.
 * For more details, see pci_setup_bridge() in setup-bus.c.
 * By comparing the target PCI memory address against the
 * end address of controller 0, we can determine the controller
 * that should accept the PCI memory access.
 */
#define TILE_READ(size, type)						\
type _tile_read##size(unsigned long addr)				\
{									\
	type val;							\
	int idx = 0;							\
	if (addr > controllers[0].mem_resources[1].end &&		\
	    addr > controllers[0].mem_resources[2].end)			\
		idx = 1;                                                \
	if (hv_dev_pread(controllers[idx].hv_mem_fd, 0,			\
			 (HV_VirtAddr)(&val), sizeof(type), addr))	\
		pr_err("PCI: read %zd bytes at 0x%lX failed\n",		\
		       sizeof(type), addr);				\
	return val;							\
}									\
EXPORT_SYMBOL(_tile_read##size)

TILE_READ(b, u8);
TILE_READ(w, u16);
TILE_READ(l, u32);
TILE_READ(q, u64);

#define TILE_WRITE(size, type)						\
void _tile_write##size(type val, unsigned long addr)			\
{									\
	int idx = 0;							\
	if (addr > controllers[0].mem_resources[1].end &&		\
	    addr > controllers[0].mem_resources[2].end)			\
		idx = 1;                                                \
	if (hv_dev_pwrite(controllers[idx].hv_mem_fd, 0,		\
			  (HV_VirtAddr)(&val), sizeof(type), addr))	\
		pr_err("PCI: write %zd bytes at 0x%lX failed\n",	\
		       sizeof(type), addr);				\
}									\
EXPORT_SYMBOL(_tile_write##size)

TILE_WRITE(b, u8);
TILE_WRITE(w, u16);
TILE_WRITE(l, u32);
TILE_WRITE(q, u64);
