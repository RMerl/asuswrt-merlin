/*
 * mmconfig-shared.c - Low-level direct PCI config space access via
 *                     MMCONFIG - common code between i386 and x86-64.
 *
 * This code does:
 * - known chipset handling
 * - ACPI decoding and validation
 *
 * Per-architecture code takes care of the mappings and accesses
 * themselves.
 */

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/bitmap.h>
#include <asm/e820.h>

#include "pci.h"

/* aperture is up to 256MB but BIOS may reserve less */
#define MMCONFIG_APER_MIN	(2 * 1024*1024)
#define MMCONFIG_APER_MAX	(256 * 1024*1024)

DECLARE_BITMAP(pci_mmcfg_fallback_slots, 32*PCI_MMCFG_MAX_CHECK_BUS);

/* K8 systems have some devices (typically in the builtin northbridge)
   that are only accessible using type1
   Normally this can be expressed in the MCFG by not listing them
   and assigning suitable _SEGs, but this isn't implemented in some BIOS.
   Instead try to discover all devices on bus 0 that are unreachable using MM
   and fallback for them. */
static void __init unreachable_devices(void)
{
	int i, bus;
	/* Use the max bus number from ACPI here? */
	for (bus = 0; bus < PCI_MMCFG_MAX_CHECK_BUS; bus++) {
		for (i = 0; i < 32; i++) {
			unsigned int devfn = PCI_DEVFN(i, 0);
			u32 val1, val2;

			pci_conf1_read(0, bus, devfn, 0, 4, &val1);
			if (val1 == 0xffffffff)
				continue;

			if (pci_mmcfg_arch_reachable(0, bus, devfn)) {
				raw_pci_ops->read(0, bus, devfn, 0, 4, &val2);
				if (val1 == val2)
					continue;
			}
			set_bit(i + 32 * bus, pci_mmcfg_fallback_slots);
			printk(KERN_NOTICE "PCI: No mmconfig possible on device"
			       " %02x:%02x\n", bus, i);
		}
	}
}

static const char __init *pci_mmcfg_e7520(void)
{
	u32 win;
	pci_conf1_read(0, 0, PCI_DEVFN(0,0), 0xce, 2, &win);

	win = win & 0xf000;
	if(win == 0x0000 || win == 0xf000)
		pci_mmcfg_config_num = 0;
	else {
		pci_mmcfg_config_num = 1;
		pci_mmcfg_config = kzalloc(sizeof(pci_mmcfg_config[0]), GFP_KERNEL);
		if (!pci_mmcfg_config)
			return NULL;
		pci_mmcfg_config[0].address = win << 16;
		pci_mmcfg_config[0].pci_segment = 0;
		pci_mmcfg_config[0].start_bus_number = 0;
		pci_mmcfg_config[0].end_bus_number = 255;
	}

	return "Intel Corporation E7520 Memory Controller Hub";
}

static const char __init *pci_mmcfg_intel_945(void)
{
	u32 pciexbar, mask = 0, len = 0;

	pci_mmcfg_config_num = 1;

	pci_conf1_read(0, 0, PCI_DEVFN(0,0), 0x48, 4, &pciexbar);

	/* Enable bit */
	if (!(pciexbar & 1))
		pci_mmcfg_config_num = 0;

	/* Size bits */
	switch ((pciexbar >> 1) & 3) {
	case 0:
		mask = 0xf0000000U;
		len  = 0x10000000U;
		break;
	case 1:
		mask = 0xf8000000U;
		len  = 0x08000000U;
		break;
	case 2:
		mask = 0xfc000000U;
		len  = 0x04000000U;
		break;
	default:
		pci_mmcfg_config_num = 0;
	}

	/* Errata #2, things break when not aligned on a 256Mb boundary */
	/* Can only happen in 64M/128M mode */

	if ((pciexbar & mask) & 0x0fffffffU)
		pci_mmcfg_config_num = 0;

	/* Don't hit the APIC registers and their friends */
	if ((pciexbar & mask) >= 0xf0000000U)
		pci_mmcfg_config_num = 0;

	if (pci_mmcfg_config_num) {
		pci_mmcfg_config = kzalloc(sizeof(pci_mmcfg_config[0]), GFP_KERNEL);
		if (!pci_mmcfg_config)
			return NULL;
		pci_mmcfg_config[0].address = pciexbar & mask;
		pci_mmcfg_config[0].pci_segment = 0;
		pci_mmcfg_config[0].start_bus_number = 0;
		pci_mmcfg_config[0].end_bus_number = (len >> 20) - 1;
	}

	return "Intel Corporation 945G/GZ/P/PL Express Memory Controller Hub";
}

struct pci_mmcfg_hostbridge_probe {
	u32 vendor;
	u32 device;
	const char *(*probe)(void);
};

static struct pci_mmcfg_hostbridge_probe pci_mmcfg_probes[] __initdata = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_E7520_MCH, pci_mmcfg_e7520 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82945G_HB, pci_mmcfg_intel_945 },
};

static int __init pci_mmcfg_check_hostbridge(void)
{
	u32 l;
	u16 vendor, device;
	int i;
	const char *name;

	pci_conf1_read(0, 0, PCI_DEVFN(0,0), 0, 4, &l);
	vendor = l & 0xffff;
	device = (l >> 16) & 0xffff;

	pci_mmcfg_config_num = 0;
	pci_mmcfg_config = NULL;
	name = NULL;

	for (i = 0; !name && i < ARRAY_SIZE(pci_mmcfg_probes); i++) {
		if (pci_mmcfg_probes[i].vendor == vendor &&
		    pci_mmcfg_probes[i].device == device)
			name = pci_mmcfg_probes[i].probe();
	}

	if (name) {
		printk(KERN_INFO "PCI: Found %s %s MMCONFIG support.\n",
		       name, pci_mmcfg_config_num ? "with" : "without");
	}

	return name != NULL;
}

static void __init pci_mmcfg_insert_resources(void)
{
#define PCI_MMCFG_RESOURCE_NAME_LEN 19
	int i;
	struct resource *res;
	char *names;
	unsigned num_buses;

	res = kcalloc(PCI_MMCFG_RESOURCE_NAME_LEN + sizeof(*res),
			pci_mmcfg_config_num, GFP_KERNEL);
	if (!res) {
		printk(KERN_ERR "PCI: Unable to allocate MMCONFIG resources\n");
		return;
	}

	names = (void *)&res[pci_mmcfg_config_num];
	for (i = 0; i < pci_mmcfg_config_num; i++, res++) {
		struct acpi_mcfg_allocation *cfg = &pci_mmcfg_config[i];
		num_buses = cfg->end_bus_number - cfg->start_bus_number + 1;
		res->name = names;
		snprintf(names, PCI_MMCFG_RESOURCE_NAME_LEN, "PCI MMCONFIG %u",
			 cfg->pci_segment);
		res->start = cfg->address;
		res->end = res->start + (num_buses << 20) - 1;
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		insert_resource(&iomem_resource, res);
		names += PCI_MMCFG_RESOURCE_NAME_LEN;
	}
}

static void __init pci_mmcfg_reject_broken(int type)
{
	typeof(pci_mmcfg_config[0]) *cfg;

	if ((pci_mmcfg_config_num == 0) ||
	    (pci_mmcfg_config == NULL) ||
	    (pci_mmcfg_config[0].address == 0))
		return;

	cfg = &pci_mmcfg_config[0];

	/*
	 * Handle more broken MCFG tables on Asus etc.
	 * They only contain a single entry for bus 0-0.
	 */
	if (pci_mmcfg_config_num == 1 &&
	    cfg->pci_segment == 0 &&
	    (cfg->start_bus_number | cfg->end_bus_number) == 0) {
		printk(KERN_ERR "PCI: start and end of bus number is 0. "
		       "Rejected as broken MCFG.\n");
		goto reject;
	}

	/*
	 * Only do this check when type 1 works. If it doesn't work
	 * assume we run on a Mac and always use MCFG
	 */
	if (type == 1 && !e820_all_mapped(cfg->address,
					  cfg->address + MMCONFIG_APER_MIN,
					  E820_RESERVED)) {
		printk(KERN_ERR "PCI: BIOS Bug: MCFG area at %Lx is not"
		       " E820-reserved\n", cfg->address);
		goto reject;
	}
	return;

reject:
	printk(KERN_ERR "PCI: Not using MMCONFIG.\n");
	kfree(pci_mmcfg_config);
	pci_mmcfg_config = NULL;
	pci_mmcfg_config_num = 0;
}

void __init pci_mmcfg_init(int type)
{
	int known_bridge = 0;

	if ((pci_probe & PCI_PROBE_MMCONF) == 0)
		return;

	if (type == 1 && pci_mmcfg_check_hostbridge())
		known_bridge = 1;

	if (!known_bridge) {
		acpi_table_parse(ACPI_SIG_MCFG, acpi_parse_mcfg);
		pci_mmcfg_reject_broken(type);
	}

	if ((pci_mmcfg_config_num == 0) ||
	    (pci_mmcfg_config == NULL) ||
	    (pci_mmcfg_config[0].address == 0))
		return;

	if (pci_mmcfg_arch_init()) {
		if (type == 1)
			unreachable_devices();
		if (known_bridge)
			pci_mmcfg_insert_resources();
		pci_probe = (pci_probe & ~PCI_PROBE_MASK) | PCI_PROBE_MMCONF;
	}
}
