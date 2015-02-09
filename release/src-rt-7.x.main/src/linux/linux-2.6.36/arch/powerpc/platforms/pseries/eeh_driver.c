/*
 * PCI Error Recovery Driver for RPA-compliant PPC64 platform.
 * Copyright IBM Corp. 2004 2005
 * Copyright Linas Vepstas <linas@linas.org> 2004, 2005
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send comments and feedback to Linas Vepstas <linas@austin.ibm.com>
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <asm/eeh.h>
#include <asm/eeh_event.h>
#include <asm/ppc-pci.h>
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <asm/rtas.h>


static inline const char * pcid_name (struct pci_dev *pdev)
{
	if (pdev && pdev->dev.driver)
		return pdev->dev.driver->name;
	return "";
}


/**
 * eeh_disable_irq - disable interrupt for the recovering device
 */
static void eeh_disable_irq(struct pci_dev *dev)
{
	struct device_node *dn = pci_device_to_OF_node(dev);

	/* Don't disable MSI and MSI-X interrupts. They are
	 * effectively disabled by the DMA Stopped state
	 * when an EEH error occurs.
	*/
	if (dev->msi_enabled || dev->msix_enabled)
		return;

	if (!irq_has_action(dev->irq))
		return;

	PCI_DN(dn)->eeh_mode |= EEH_MODE_IRQ_DISABLED;
	disable_irq_nosync(dev->irq);
}

/**
 * eeh_enable_irq - enable interrupt for the recovering device
 */
static void eeh_enable_irq(struct pci_dev *dev)
{
	struct device_node *dn = pci_device_to_OF_node(dev);

	if ((PCI_DN(dn)->eeh_mode) & EEH_MODE_IRQ_DISABLED) {
		PCI_DN(dn)->eeh_mode &= ~EEH_MODE_IRQ_DISABLED;
		enable_irq(dev->irq);
	}
}

/* ------------------------------------------------------- */
/**
 * eeh_report_error - report pci error to each device driver
 * 
 * Report an EEH error to each device driver, collect up and 
 * merge the device driver responses. Cumulative response 
 * passed back in "userdata".
 */

static int eeh_report_error(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_frozen;

	if (!driver)
		return 0;

	eeh_disable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->error_detected)
		return 0;

	rc = driver->err_handler->error_detected (dev, pci_channel_io_frozen);

	/* A driver that needs a reset trumps all others */
	if (rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;
	if (*res == PCI_ERS_RESULT_NONE) *res = rc;

	return 0;
}

/**
 * eeh_report_mmio_enabled - tell drivers that MMIO has been enabled
 *
 * Tells each device driver that IO ports, MMIO and config space I/O
 * are now enabled. Collects up and merges the device driver responses.
 * Cumulative response passed back in "userdata".
 */

static int eeh_report_mmio_enabled(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	if (!driver ||
	    !driver->err_handler ||
	    !driver->err_handler->mmio_enabled)
		return 0;

	rc = driver->err_handler->mmio_enabled (dev);

	/* A driver that needs a reset trumps all others */
	if (rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;
	if (*res == PCI_ERS_RESULT_NONE) *res = rc;

	return 0;
}

/**
 * eeh_report_reset - tell device that slot has been reset
 */

static int eeh_report_reset(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	if (!driver)
		return 0;

	dev->error_state = pci_channel_io_normal;

	eeh_enable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->slot_reset)
		return 0;

	rc = driver->err_handler->slot_reset(dev);
	if ((*res == PCI_ERS_RESULT_NONE) ||
	    (*res == PCI_ERS_RESULT_RECOVERED)) *res = rc;
	if (*res == PCI_ERS_RESULT_DISCONNECT &&
	     rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;

	return 0;
}

/**
 * eeh_report_resume - tell device to resume normal operations
 */

static int eeh_report_resume(struct pci_dev *dev, void *userdata)
{
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_normal;

	if (!driver)
		return 0;

	eeh_enable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->resume)
		return 0;

	driver->err_handler->resume(dev);

	return 0;
}

/**
 * eeh_report_failure - tell device driver that device is dead.
 *
 * This informs the device driver that the device is permanently
 * dead, and that no further recovery attempts will be made on it.
 */

static int eeh_report_failure(struct pci_dev *dev, void *userdata)
{
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_perm_failure;

	if (!driver)
		return 0;

	eeh_disable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->error_detected)
		return 0;

	driver->err_handler->error_detected(dev, pci_channel_io_perm_failure);

	return 0;
}

/* ------------------------------------------------------- */
/**
 * handle_eeh_events -- reset a PCI device after hard lockup.
 *
 * pSeries systems will isolate a PCI slot if the PCI-Host
 * bridge detects address or data parity errors, DMA's
 * occurring to wild addresses (which usually happen due to
 * bugs in device drivers or in PCI adapter firmware).
 * Slot isolations also occur if #SERR, #PERR or other misc
 * PCI-related errors are detected.
 *
 * Recovery process consists of unplugging the device driver
 * (which generated hotplug events to userspace), then issuing
 * a PCI #RST to the device, then reconfiguring the PCI config
 * space for all bridges & devices under this slot, and then
 * finally restarting the device drivers (which cause a second
 * set of hotplug events to go out to userspace).
 */

/**
 * eeh_reset_device() -- perform actual reset of a pci slot
 * @bus: pointer to the pci bus structure corresponding
 *            to the isolated slot. A non-null value will
 *            cause all devices under the bus to be removed
 *            and then re-added.
 * @pe_dn: pointer to a "Partionable Endpoint" device node.
 *            This is the top-level structure on which pci
 *            bus resets can be performed.
 */

static int eeh_reset_device (struct pci_dn *pe_dn, struct pci_bus *bus)
{
	struct device_node *dn;
	int cnt, rc;

	/* pcibios will clear the counter; save the value */
	cnt = pe_dn->eeh_freeze_count;

	if (bus)
		pcibios_remove_pci_devices(bus);

	/* Reset the pci controller. (Asserts RST#; resets config space).
	 * Reconfigure bridges and devices. Don't try to bring the system
	 * up if the reset failed for some reason. */
	rc = rtas_set_slot_reset(pe_dn);
	if (rc)
		return rc;

	/* Walk over all functions on this device.  */
	dn = pe_dn->node;
	if (!pcibios_find_pci_bus(dn) && PCI_DN(dn->parent))
		dn = dn->parent->child;

	while (dn) {
		struct pci_dn *ppe = PCI_DN(dn);
		/* On Power4, always true because eeh_pe_config_addr=0 */
		if (pe_dn->eeh_pe_config_addr == ppe->eeh_pe_config_addr) {
			rtas_configure_bridge(ppe);
			eeh_restore_bars(ppe);
 		}
		dn = dn->sibling;
	}

	/* Give the system 5 seconds to finish running the user-space
	 * hotplug shutdown scripts, e.g. ifdown for ethernet.  Yes, 
	 * this is a hack, but if we don't do this, and try to bring 
	 * the device up before the scripts have taken it down, 
	 * potentially weird things happen.
	 */
	if (bus) {
		ssleep (5);
		pcibios_add_pci_devices(bus);
	}
	pe_dn->eeh_freeze_count = cnt;

	return 0;
}

/* The longest amount of time to wait for a pci device
 * to come back on line, in seconds.
 */
#define MAX_WAIT_FOR_RECOVERY 150

struct pci_dn * handle_eeh_events (struct eeh_event *event)
{
	struct device_node *frozen_dn;
	struct pci_dn *frozen_pdn;
	struct pci_bus *frozen_bus;
	int rc = 0;
	enum pci_ers_result result = PCI_ERS_RESULT_NONE;
	const char *location, *pci_str, *drv_str;

	frozen_dn = find_device_pe(event->dn);
	if (!frozen_dn) {

		location = of_get_property(event->dn, "ibm,loc-code", NULL);
		location = location ? location : "unknown";
		printk(KERN_ERR "EEH: Error: Cannot find partition endpoint "
		                "for location=%s pci addr=%s\n",
		        location, eeh_pci_name(event->dev));
		return NULL;
	}

	frozen_bus = pcibios_find_pci_bus(frozen_dn);
	location = of_get_property(frozen_dn, "ibm,loc-code", NULL);
	location = location ? location : "unknown";

	/* There are two different styles for coming up with the PE.
	 * In the old style, it was the highest EEH-capable device
	 * which was always an EADS pci bridge.  In the new style,
	 * there might not be any EADS bridges, and even when there are,
	 * the firmware marks them as "EEH incapable". So another
	 * two-step is needed to find the pci bus.. */
	if (!frozen_bus)
		frozen_bus = pcibios_find_pci_bus (frozen_dn->parent);

	if (!frozen_bus) {
		printk(KERN_ERR "EEH: Cannot find PCI bus "
		        "for location=%s dn=%s\n",
		        location, frozen_dn->full_name);
		return NULL;
	}

	frozen_pdn = PCI_DN(frozen_dn);
	frozen_pdn->eeh_freeze_count++;

	if (frozen_pdn->pcidev) {
		pci_str = pci_name (frozen_pdn->pcidev);
		drv_str = pcid_name (frozen_pdn->pcidev);
	} else {
		pci_str = eeh_pci_name(event->dev);
		drv_str = pcid_name (event->dev);
	}
	
	if (frozen_pdn->eeh_freeze_count > EEH_MAX_ALLOWED_FREEZES)
		goto excess_failures;

	printk(KERN_WARNING
	   "EEH: This PCI device has failed %d times in the last hour:\n",
		frozen_pdn->eeh_freeze_count);
	printk(KERN_WARNING
		"EEH: location=%s driver=%s pci addr=%s\n",
		location, drv_str, pci_str);

	/* Walk the various device drivers attached to this slot through
	 * a reset sequence, giving each an opportunity to do what it needs
	 * to accomplish the reset.  Each child gets a report of the
	 * status ... if any child can't handle the reset, then the entire
	 * slot is dlpar removed and added.
	 */
	pci_walk_bus(frozen_bus, eeh_report_error, &result);

	/* Get the current PCI slot state. This can take a long time,
	 * sometimes over 3 seconds for certain systems. */
	rc = eeh_wait_for_slot_status (frozen_pdn, MAX_WAIT_FOR_RECOVERY*1000);
	if (rc < 0) {
		printk(KERN_WARNING "EEH: Permanent failure\n");
		goto hard_fail;
	}

	/* Since rtas may enable MMIO when posting the error log,
	 * don't post the error log until after all dev drivers
	 * have been informed.
	 */
	eeh_slot_error_detail(frozen_pdn, EEH_LOG_TEMP_FAILURE);

	/* If all device drivers were EEH-unaware, then shut
	 * down all of the device drivers, and hope they
	 * go down willingly, without panicing the system.
	 */
	if (result == PCI_ERS_RESULT_NONE) {
		rc = eeh_reset_device(frozen_pdn, frozen_bus);
		if (rc) {
			printk(KERN_WARNING "EEH: Unable to reset, rc=%d\n", rc);
			goto hard_fail;
		}
	}

	/* If all devices reported they can proceed, then re-enable MMIO */
	if (result == PCI_ERS_RESULT_CAN_RECOVER) {
		rc = rtas_pci_enable(frozen_pdn, EEH_THAW_MMIO);

		if (rc < 0)
			goto hard_fail;
		if (rc) {
			result = PCI_ERS_RESULT_NEED_RESET;
		} else {
			result = PCI_ERS_RESULT_NONE;
			pci_walk_bus(frozen_bus, eeh_report_mmio_enabled, &result);
		}
	}

	/* If all devices reported they can proceed, then re-enable DMA */
	if (result == PCI_ERS_RESULT_CAN_RECOVER) {
		rc = rtas_pci_enable(frozen_pdn, EEH_THAW_DMA);

		if (rc < 0)
			goto hard_fail;
		if (rc)
			result = PCI_ERS_RESULT_NEED_RESET;
		else
			result = PCI_ERS_RESULT_RECOVERED;
	}

	/* If any device has a hard failure, then shut off everything. */
	if (result == PCI_ERS_RESULT_DISCONNECT) {
		printk(KERN_WARNING "EEH: Device driver gave up\n");
		goto hard_fail;
	}

	/* If any device called out for a reset, then reset the slot */
	if (result == PCI_ERS_RESULT_NEED_RESET) {
		rc = eeh_reset_device(frozen_pdn, NULL);
		if (rc) {
			printk(KERN_WARNING "EEH: Cannot reset, rc=%d\n", rc);
			goto hard_fail;
		}
		result = PCI_ERS_RESULT_NONE;
		pci_walk_bus(frozen_bus, eeh_report_reset, &result);
	}

	/* All devices should claim they have recovered by now. */
	if ((result != PCI_ERS_RESULT_RECOVERED) &&
	    (result != PCI_ERS_RESULT_NONE)) {
		printk(KERN_WARNING "EEH: Not recovered\n");
		goto hard_fail;
	}

	/* Tell all device drivers that they can resume operations */
	pci_walk_bus(frozen_bus, eeh_report_resume, NULL);

	return frozen_pdn;
	
excess_failures:
	/*
	 * About 90% of all real-life EEH failures in the field
	 * are due to poorly seated PCI cards. Only 10% or so are
	 * due to actual, failed cards.
	 */
	printk(KERN_ERR
	   "EEH: PCI device at location=%s driver=%s pci addr=%s\n"
		"has failed %d times in the last hour "
		"and has been permanently disabled.\n"
		"Please try reseating this device or replacing it.\n",
		location, drv_str, pci_str, frozen_pdn->eeh_freeze_count);
	goto perm_error;

hard_fail:
	printk(KERN_ERR
	   "EEH: Unable to recover from failure of PCI device "
	   "at location=%s driver=%s pci addr=%s\n"
	   "Please try reseating this device or replacing it.\n",
		location, drv_str, pci_str);

perm_error:
	eeh_slot_error_detail(frozen_pdn, EEH_LOG_PERM_FAILURE);

	/* Notify all devices that they're about to go down. */
	pci_walk_bus(frozen_bus, eeh_report_failure, NULL);

	/* Shut down the device drivers for good. */
	pcibios_remove_pci_devices(frozen_bus);

	return NULL;
}

/* ---------- end of file ---------- */
