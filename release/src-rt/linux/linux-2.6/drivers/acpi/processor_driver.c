/*
 * acpi_processor.c - ACPI Processor Driver ($Revision: 71 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2004       Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2004  Anil S Keshavamurthy <anil.s.keshavamurthy@intel.com>
 *  			- Added processor hotplug support
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  TBD:
 *	1. Make # power states dynamic.
 *	2. Support duty_cycle values that span bit 4.
 *	3. Optimize by having scheduler determine business instead of
 *	   having us try to calculate it here.
 *	4. Need C1 timing -- must modify kernel (IRQ handler) to get this.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/dmi.h>
#include <linux/moduleparam.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/cpu.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/smp.h>
#include <asm/acpi.h>

#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <acpi/processor.h>

#define PREFIX "ACPI: "

#define ACPI_PROCESSOR_CLASS		"processor"
#define ACPI_PROCESSOR_DEVICE_NAME	"Processor"
#define ACPI_PROCESSOR_FILE_INFO	"info"
#define ACPI_PROCESSOR_FILE_THROTTLING	"throttling"
#define ACPI_PROCESSOR_FILE_LIMIT	"limit"
#define ACPI_PROCESSOR_NOTIFY_PERFORMANCE 0x80
#define ACPI_PROCESSOR_NOTIFY_POWER	0x81
#define ACPI_PROCESSOR_NOTIFY_THROTTLING	0x82

#define ACPI_PROCESSOR_LIMIT_USER	0
#define ACPI_PROCESSOR_LIMIT_THERMAL	1

#define _COMPONENT		ACPI_PROCESSOR_COMPONENT
ACPI_MODULE_NAME("processor_driver");

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION("ACPI Processor Driver");
MODULE_LICENSE("GPL");

static int acpi_processor_add(struct acpi_device *device);
static int acpi_processor_remove(struct acpi_device *device, int type);
static void acpi_processor_notify(struct acpi_device *device, u32 event);
static acpi_status acpi_processor_hotadd_init(acpi_handle handle, int *p_cpu);
static int acpi_processor_handle_eject(struct acpi_processor *pr);


static const struct acpi_device_id processor_device_ids[] = {
	{ACPI_PROCESSOR_OBJECT_HID, 0},
	{"ACPI0007", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, processor_device_ids);

static struct acpi_driver acpi_processor_driver = {
	.name = "processor",
	.class = ACPI_PROCESSOR_CLASS,
	.ids = processor_device_ids,
	.ops = {
		.add = acpi_processor_add,
		.remove = acpi_processor_remove,
		.suspend = acpi_processor_suspend,
		.resume = acpi_processor_resume,
		.notify = acpi_processor_notify,
		},
};

#define INSTALL_NOTIFY_HANDLER		1
#define UNINSTALL_NOTIFY_HANDLER	2

DEFINE_PER_CPU(struct acpi_processor *, processors);
EXPORT_PER_CPU_SYMBOL(processors);

struct acpi_processor_errata errata __read_mostly;

/* --------------------------------------------------------------------------
                                Errata Handling
   -------------------------------------------------------------------------- */

static int acpi_processor_errata_piix4(struct pci_dev *dev)
{
	u8 value1 = 0;
	u8 value2 = 0;


	if (!dev)
		return -EINVAL;

	/*
	 * Note that 'dev' references the PIIX4 ACPI Controller.
	 */

	switch (dev->revision) {
	case 0:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found PIIX4 A-step\n"));
		break;
	case 1:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found PIIX4 B-step\n"));
		break;
	case 2:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found PIIX4E\n"));
		break;
	case 3:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found PIIX4M\n"));
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found unknown PIIX4\n"));
		break;
	}

	switch (dev->revision) {

	case 0:		/* PIIX4 A-step */
	case 1:		/* PIIX4 B-step */
		/*
		 * See specification changes #13 ("Manual Throttle Duty Cycle")
		 * and #14 ("Enabling and Disabling Manual Throttle"), plus
		 * erratum #5 ("STPCLK# Deassertion Time") from the January
		 * 2002 PIIX4 specification update.  Applies to only older
		 * PIIX4 models.
		 */
		errata.piix4.throttle = 1;

	case 2:		/* PIIX4E */
	case 3:		/* PIIX4M */
		/*
		 * See erratum #18 ("C3 Power State/BMIDE and Type-F DMA
		 * Livelock") from the January 2002 PIIX4 specification update.
		 * Applies to all PIIX4 models.
		 */

		/*
		 * BM-IDE
		 * ------
		 * Find the PIIX4 IDE Controller and get the Bus Master IDE
		 * Status register address.  We'll use this later to read
		 * each IDE controller's DMA status to make sure we catch all
		 * DMA activity.
		 */
		dev = pci_get_subsys(PCI_VENDOR_ID_INTEL,
				     PCI_DEVICE_ID_INTEL_82371AB,
				     PCI_ANY_ID, PCI_ANY_ID, NULL);
		if (dev) {
			errata.piix4.bmisx = pci_resource_start(dev, 4);
			pci_dev_put(dev);
		}

		/*
		 * Type-F DMA
		 * ----------
		 * Find the PIIX4 ISA Controller and read the Motherboard
		 * DMA controller's status to see if Type-F (Fast) DMA mode
		 * is enabled (bit 7) on either channel.  Note that we'll
		 * disable C3 support if this is enabled, as some legacy
		 * devices won't operate well if fast DMA is disabled.
		 */
		dev = pci_get_subsys(PCI_VENDOR_ID_INTEL,
				     PCI_DEVICE_ID_INTEL_82371AB_0,
				     PCI_ANY_ID, PCI_ANY_ID, NULL);
		if (dev) {
			pci_read_config_byte(dev, 0x76, &value1);
			pci_read_config_byte(dev, 0x77, &value2);
			if ((value1 & 0x80) || (value2 & 0x80))
				errata.piix4.fdma = 1;
			pci_dev_put(dev);
		}

		break;
	}

	if (errata.piix4.bmisx)
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Bus master activity detection (BM-IDE) erratum enabled\n"));
	if (errata.piix4.fdma)
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Type-F DMA livelock erratum (C3 disabled)\n"));

	return 0;
}

static int acpi_processor_errata(struct acpi_processor *pr)
{
	int result = 0;
	struct pci_dev *dev = NULL;


	if (!pr)
		return -EINVAL;

	/*
	 * PIIX4
	 */
	dev = pci_get_subsys(PCI_VENDOR_ID_INTEL,
			     PCI_DEVICE_ID_INTEL_82371AB_3, PCI_ANY_ID,
			     PCI_ANY_ID, NULL);
	if (dev) {
		result = acpi_processor_errata_piix4(dev);
		pci_dev_put(dev);
	}

	return result;
}

/* --------------------------------------------------------------------------
                                 Driver Interface
   -------------------------------------------------------------------------- */

static int acpi_processor_get_info(struct acpi_device *device)
{
	acpi_status status = 0;
	union acpi_object object = { 0 };
	struct acpi_buffer buffer = { sizeof(union acpi_object), &object };
	struct acpi_processor *pr;
	int cpu_index, device_declaration = 0;
	static int cpu0_initialized;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	if (num_online_cpus() > 1)
		errata.smp = TRUE;

	acpi_processor_errata(pr);

	/*
	 * Check to see if we have bus mastering arbitration control.  This
	 * is required for proper C3 usage (to maintain cache coherency).
	 */
	if (acpi_gbl_FADT.pm2_control_block && acpi_gbl_FADT.pm2_control_length) {
		pr->flags.bm_control = 1;
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Bus mastering arbitration control present\n"));
	} else
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "No bus mastering arbitration control\n"));

	if (!strcmp(acpi_device_hid(device), ACPI_PROCESSOR_OBJECT_HID)) {
		/* Declared with "Processor" statement; match ProcessorID */
		status = acpi_evaluate_object(pr->handle, NULL, NULL, &buffer);
		if (ACPI_FAILURE(status)) {
			printk(KERN_ERR PREFIX "Evaluating processor object\n");
			return -ENODEV;
		}

		/*
		 * TBD: Synch processor ID (via LAPIC/LSAPIC structures) on SMP.
		 *      >>> 'acpi_get_processor_id(acpi_id, &id)' in
		 *      arch/xxx/acpi.c
		 */
		pr->acpi_id = object.processor.proc_id;
	} else {
		/*
		 * Declared with "Device" statement; match _UID.
		 * Note that we don't handle string _UIDs yet.
		 */
		unsigned long long value;
		status = acpi_evaluate_integer(pr->handle, METHOD_NAME__UID,
						NULL, &value);
		if (ACPI_FAILURE(status)) {
			printk(KERN_ERR PREFIX
			    "Evaluating processor _UID [%#x]\n", status);
			return -ENODEV;
		}
		device_declaration = 1;
		pr->acpi_id = value;
	}
	cpu_index = acpi_get_cpuid(pr->handle, device_declaration, pr->acpi_id);

	/* Handle UP system running SMP kernel, with no LAPIC in MADT */
	if (!cpu0_initialized && (cpu_index == -1) &&
	    (num_online_cpus() == 1)) {
		cpu_index = 0;
	}

	cpu0_initialized = 1;

	pr->id = cpu_index;

	/*
	 *  Extra Processor objects may be enumerated on MP systems with
	 *  less than the max # of CPUs. They should be ignored _iff
	 *  they are physically not present.
	 */
	if (pr->id == -1) {
		if (ACPI_FAILURE
		    (acpi_processor_hotadd_init(pr->handle, &pr->id))) {
			return -ENODEV;
		}
	}
	/*
	 * On some boxes several processors use the same processor bus id.
	 * But they are located in different scope. For example:
	 * \_SB.SCK0.CPU0
	 * \_SB.SCK1.CPU0
	 * Rename the processor device bus id. And the new bus id will be
	 * generated as the following format:
	 * CPU+CPU ID.
	 */
	sprintf(acpi_device_bid(device), "CPU%X", pr->id);
	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Processor [%d:%d]\n", pr->id,
			  pr->acpi_id));

	if (!object.processor.pblk_address)
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No PBLK (NULL address)\n"));
	else if (object.processor.pblk_length != 6)
		printk(KERN_ERR PREFIX "Invalid PBLK length [%d]\n",
			    object.processor.pblk_length);
	else {
		pr->throttling.address = object.processor.pblk_address;
		pr->throttling.duty_offset = acpi_gbl_FADT.duty_offset;
		pr->throttling.duty_width = acpi_gbl_FADT.duty_width;

		pr->pblk = object.processor.pblk_address;

		/*
		 * We don't care about error returns - we just try to mark
		 * these reserved so that nobody else is confused into thinking
		 * that this region might be unused..
		 *
		 * (In particular, allocating the IO range for Cardbus)
		 */
		request_region(pr->throttling.address, 6, "ACPI CPU throttle");
	}

	/*
	 * If ACPI describes a slot number for this CPU, we can use it
	 * ensure we get the right value in the "physical id" field
	 * of /proc/cpuinfo
	 */
	status = acpi_evaluate_object(pr->handle, "_SUN", NULL, &buffer);
	if (ACPI_SUCCESS(status))
		arch_fix_phys_package_id(pr->id, object.integer.value);

	return 0;
}

static DEFINE_PER_CPU(void *, processor_device_array);

static void acpi_processor_notify(struct acpi_device *device, u32 event)
{
	struct acpi_processor *pr = acpi_driver_data(device);
	int saved;

	if (!pr)
		return;

	switch (event) {
	case ACPI_PROCESSOR_NOTIFY_PERFORMANCE:
		saved = pr->performance_platform_limit;
		acpi_processor_ppc_has_changed(pr, 1);
		if (saved == pr->performance_platform_limit)
			break;
		acpi_bus_generate_proc_event(device, event,
					pr->performance_platform_limit);
		acpi_bus_generate_netlink_event(device->pnp.device_class,
						  dev_name(&device->dev), event,
						  pr->performance_platform_limit);
		break;
	case ACPI_PROCESSOR_NOTIFY_POWER:
		acpi_processor_cst_has_changed(pr);
		acpi_bus_generate_proc_event(device, event, 0);
		acpi_bus_generate_netlink_event(device->pnp.device_class,
						  dev_name(&device->dev), event, 0);
		break;
	case ACPI_PROCESSOR_NOTIFY_THROTTLING:
		acpi_processor_tstate_has_changed(pr);
		acpi_bus_generate_proc_event(device, event, 0);
		acpi_bus_generate_netlink_event(device->pnp.device_class,
						  dev_name(&device->dev), event, 0);
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event));
		break;
	}

	return;
}

static int acpi_cpu_soft_notify(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct acpi_processor *pr = per_cpu(processors, cpu);

	if (action == CPU_ONLINE && pr) {
		acpi_processor_ppc_has_changed(pr, 0);
		acpi_processor_cst_has_changed(pr);
		acpi_processor_reevaluate_tstate(pr, action);
		acpi_processor_tstate_has_changed(pr);
	}
	if (action == CPU_DEAD && pr) {
		/* invalidate the flag.throttling after one CPU is offline */
		acpi_processor_reevaluate_tstate(pr, action);
	}
	return NOTIFY_OK;
}

static struct notifier_block acpi_cpu_notifier =
{
	    .notifier_call = acpi_cpu_soft_notify,
};

static int __cpuinit acpi_processor_add(struct acpi_device *device)
{
	struct acpi_processor *pr = NULL;
	int result = 0;
	struct sys_device *sysdev;

	pr = kzalloc(sizeof(struct acpi_processor), GFP_KERNEL);
	if (!pr)
		return -ENOMEM;

	if (!zalloc_cpumask_var(&pr->throttling.shared_cpu_map, GFP_KERNEL)) {
		kfree(pr);
		return -ENOMEM;
	}

	pr->handle = device->handle;
	strcpy(acpi_device_name(device), ACPI_PROCESSOR_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_PROCESSOR_CLASS);
	device->driver_data = pr;

	result = acpi_processor_get_info(device);
	if (result) {
		/* Processor is physically not present */
		return 0;
	}

#ifdef CONFIG_SMP
	if (pr->id >= setup_max_cpus && pr->id != 0)
		return 0;
#endif

	BUG_ON((pr->id >= nr_cpu_ids) || (pr->id < 0));

	/*
	 * Buggy BIOS check
	 * ACPI id of processors can be reported wrongly by the BIOS.
	 * Don't trust it blindly
	 */
	if (per_cpu(processor_device_array, pr->id) != NULL &&
	    per_cpu(processor_device_array, pr->id) != device) {
		printk(KERN_WARNING "BIOS reported wrong ACPI id "
			"for the processor\n");
		result = -ENODEV;
		goto err_free_cpumask;
	}
	per_cpu(processor_device_array, pr->id) = device;

	per_cpu(processors, pr->id) = pr;

	sysdev = get_cpu_sysdev(pr->id);
	if (sysfs_create_link(&device->dev.kobj, &sysdev->kobj, "sysdev")) {
		result = -EFAULT;
		goto err_free_cpumask;
	}

#ifdef CONFIG_CPU_FREQ
	acpi_processor_ppc_has_changed(pr, 0);
#endif
	acpi_processor_get_throttling_info(pr);
	acpi_processor_get_limit_info(pr);


	if (cpuidle_get_driver() == &acpi_idle_driver)
		acpi_processor_power_init(pr, device);

	pr->cdev = thermal_cooling_device_register("Processor", device,
						&processor_cooling_ops);
	if (IS_ERR(pr->cdev)) {
		result = PTR_ERR(pr->cdev);
		goto err_power_exit;
	}

	dev_dbg(&device->dev, "registered as cooling_device%d\n",
		 pr->cdev->id);

	result = sysfs_create_link(&device->dev.kobj,
				   &pr->cdev->device.kobj,
				   "thermal_cooling");
	if (result) {
		printk(KERN_ERR PREFIX "Create sysfs link\n");
		goto err_thermal_unregister;
	}
	result = sysfs_create_link(&pr->cdev->device.kobj,
				   &device->dev.kobj,
				   "device");
	if (result) {
		printk(KERN_ERR PREFIX "Create sysfs link\n");
		goto err_remove_sysfs;
	}

	return 0;

err_remove_sysfs:
	sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
err_thermal_unregister:
	thermal_cooling_device_unregister(pr->cdev);
err_power_exit:
	acpi_processor_power_exit(pr, device);
err_free_cpumask:
	free_cpumask_var(pr->throttling.shared_cpu_map);

	return result;
}

static int acpi_processor_remove(struct acpi_device *device, int type)
{
	struct acpi_processor *pr = NULL;


	if (!device || !acpi_driver_data(device))
		return -EINVAL;

	pr = acpi_driver_data(device);

	if (pr->id >= nr_cpu_ids)
		goto free;

	if (type == ACPI_BUS_REMOVAL_EJECT) {
		if (acpi_processor_handle_eject(pr))
			return -EINVAL;
	}

	acpi_processor_power_exit(pr, device);

	sysfs_remove_link(&device->dev.kobj, "sysdev");

	if (pr->cdev) {
		sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
		sysfs_remove_link(&pr->cdev->device.kobj, "device");
		thermal_cooling_device_unregister(pr->cdev);
		pr->cdev = NULL;
	}

	per_cpu(processors, pr->id) = NULL;
	per_cpu(processor_device_array, pr->id) = NULL;

free:
	free_cpumask_var(pr->throttling.shared_cpu_map);
	kfree(pr);

	return 0;
}

#ifdef CONFIG_ACPI_HOTPLUG_CPU
/****************************************************************************
 * 	Acpi processor hotplug support 				       	    *
 ****************************************************************************/

static int is_processor_present(acpi_handle handle)
{
	acpi_status status;
	unsigned long long sta = 0;


	status = acpi_evaluate_integer(handle, "_STA", NULL, &sta);

	if (ACPI_SUCCESS(status) && (sta & ACPI_STA_DEVICE_PRESENT))
		return 1;

	/*
	 * _STA is mandatory for a processor that supports hot plug
	 */
	if (status == AE_NOT_FOUND)
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				"Processor does not support hot plug\n"));
	else
		ACPI_EXCEPTION((AE_INFO, status,
				"Processor Device is not present"));
	return 0;
}

static
int acpi_processor_device_add(acpi_handle handle, struct acpi_device **device)
{
	acpi_handle phandle;
	struct acpi_device *pdev;


	if (acpi_get_parent(handle, &phandle)) {
		return -ENODEV;
	}

	if (acpi_bus_get_device(phandle, &pdev)) {
		return -ENODEV;
	}

	if (acpi_bus_add(device, pdev, handle, ACPI_BUS_TYPE_PROCESSOR)) {
		return -ENODEV;
	}

	return 0;
}

static void acpi_processor_hotplug_notify(acpi_handle handle,
					  u32 event, void *data)
{
	struct acpi_processor *pr;
	struct acpi_device *device = NULL;
	int result;


	switch (event) {
	case ACPI_NOTIFY_BUS_CHECK:
	case ACPI_NOTIFY_DEVICE_CHECK:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"Processor driver received %s event\n",
		       (event == ACPI_NOTIFY_BUS_CHECK) ?
		       "ACPI_NOTIFY_BUS_CHECK" : "ACPI_NOTIFY_DEVICE_CHECK"));

		if (!is_processor_present(handle))
			break;

		if (acpi_bus_get_device(handle, &device)) {
			result = acpi_processor_device_add(handle, &device);
			if (result)
				printk(KERN_ERR PREFIX
					    "Unable to add the device\n");
			break;
		}
		break;
	case ACPI_NOTIFY_EJECT_REQUEST:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "received ACPI_NOTIFY_EJECT_REQUEST\n"));

		if (acpi_bus_get_device(handle, &device)) {
			printk(KERN_ERR PREFIX
				    "Device don't exist, dropping EJECT\n");
			break;
		}
		pr = acpi_driver_data(device);
		if (!pr) {
			printk(KERN_ERR PREFIX
				    "Driver data is NULL, dropping EJECT\n");
			return;
		}
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event));
		break;
	}

	return;
}

static acpi_status
processor_walk_namespace_cb(acpi_handle handle,
			    u32 lvl, void *context, void **rv)
{
	acpi_status status;
	int *action = context;
	acpi_object_type type = 0;

	status = acpi_get_type(handle, &type);
	if (ACPI_FAILURE(status))
		return (AE_OK);

	if (type != ACPI_TYPE_PROCESSOR)
		return (AE_OK);

	switch (*action) {
	case INSTALL_NOTIFY_HANDLER:
		acpi_install_notify_handler(handle,
					    ACPI_SYSTEM_NOTIFY,
					    acpi_processor_hotplug_notify,
					    NULL);
		break;
	case UNINSTALL_NOTIFY_HANDLER:
		acpi_remove_notify_handler(handle,
					   ACPI_SYSTEM_NOTIFY,
					   acpi_processor_hotplug_notify);
		break;
	default:
		break;
	}

	return (AE_OK);
}

static acpi_status acpi_processor_hotadd_init(acpi_handle handle, int *p_cpu)
{

	if (!is_processor_present(handle)) {
		return AE_ERROR;
	}

	if (acpi_map_lsapic(handle, p_cpu))
		return AE_ERROR;

	if (arch_register_cpu(*p_cpu)) {
		acpi_unmap_lsapic(*p_cpu);
		return AE_ERROR;
	}

	return AE_OK;
}

static int acpi_processor_handle_eject(struct acpi_processor *pr)
{
	if (cpu_online(pr->id))
		cpu_down(pr->id);

	arch_unregister_cpu(pr->id);
	acpi_unmap_lsapic(pr->id);
	return (0);
}
#else
static acpi_status acpi_processor_hotadd_init(acpi_handle handle, int *p_cpu)
{
	return AE_ERROR;
}
static int acpi_processor_handle_eject(struct acpi_processor *pr)
{
	return (-EINVAL);
}
#endif

static
void acpi_processor_install_hotplug_notify(void)
{
#ifdef CONFIG_ACPI_HOTPLUG_CPU
	int action = INSTALL_NOTIFY_HANDLER;
	acpi_walk_namespace(ACPI_TYPE_PROCESSOR,
			    ACPI_ROOT_OBJECT,
			    ACPI_UINT32_MAX,
			    processor_walk_namespace_cb, NULL, &action, NULL);
#endif
	register_hotcpu_notifier(&acpi_cpu_notifier);
}

static
void acpi_processor_uninstall_hotplug_notify(void)
{
#ifdef CONFIG_ACPI_HOTPLUG_CPU
	int action = UNINSTALL_NOTIFY_HANDLER;
	acpi_walk_namespace(ACPI_TYPE_PROCESSOR,
			    ACPI_ROOT_OBJECT,
			    ACPI_UINT32_MAX,
			    processor_walk_namespace_cb, NULL, &action, NULL);
#endif
	unregister_hotcpu_notifier(&acpi_cpu_notifier);
}

/*
 * We keep the driver loaded even when ACPI is not running.
 * This is needed for the powernow-k8 driver, that works even without
 * ACPI, but needs symbols from this driver
 */

static int __init acpi_processor_init(void)
{
	int result = 0;

	if (acpi_disabled)
		return 0;

	memset(&errata, 0, sizeof(errata));

	if (!cpuidle_register_driver(&acpi_idle_driver)) {
		printk(KERN_DEBUG "ACPI: %s registered with cpuidle\n",
			acpi_idle_driver.name);
	} else {
		printk(KERN_DEBUG "ACPI: acpi_idle yielding to %s\n",
			cpuidle_get_driver()->name);
	}

	result = acpi_bus_register_driver(&acpi_processor_driver);
	if (result < 0)
		goto out_cpuidle;

	acpi_processor_install_hotplug_notify();

	acpi_thermal_cpufreq_init();

	acpi_processor_ppc_init();

	acpi_processor_throttling_init();

	return 0;

out_cpuidle:
	cpuidle_unregister_driver(&acpi_idle_driver);

	return result;
}

static void __exit acpi_processor_exit(void)
{
	if (acpi_disabled)
		return;

	acpi_processor_ppc_exit();

	acpi_thermal_cpufreq_exit();

	acpi_processor_uninstall_hotplug_notify();

	acpi_bus_unregister_driver(&acpi_processor_driver);

	cpuidle_unregister_driver(&acpi_idle_driver);

	return;
}

module_init(acpi_processor_init);
module_exit(acpi_processor_exit);

MODULE_ALIAS("processor");
