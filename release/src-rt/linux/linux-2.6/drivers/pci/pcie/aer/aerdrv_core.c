/*
 * drivers/pci/pcie/aer/aerdrv_core.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * This file implements the core part of PCI-Express AER. When an pci-express
 * error is delivered, an error message will be collected and printed to
 * console, then, an error recovery procedure will be executed by following
 * the pci error recovery rules.
 *
 * Copyright (C) 2006 Intel Corp.
 *	Tom Long Nguyen (tom.l.nguyen@intel.com)
 *	Zhang Yanmin (yanmin.zhang@intel.com)
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "aerdrv.h"

static int forceload;
static int nosourceid;
module_param(forceload, bool, 0);
module_param(nosourceid, bool, 0);

int pci_enable_pcie_error_reporting(struct pci_dev *dev)
{
	u16 reg16 = 0;
	int pos;

	if (pcie_aer_get_firmware_first(dev))
		return -EIO;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
	if (!pos)
		return -EIO;

	pos = pci_pcie_cap(dev);
	if (!pos)
		return -EIO;

	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &reg16);
	reg16 |= (PCI_EXP_DEVCTL_CERE |
		PCI_EXP_DEVCTL_NFERE |
		PCI_EXP_DEVCTL_FERE |
		PCI_EXP_DEVCTL_URRE);
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, reg16);

	return 0;
}
EXPORT_SYMBOL_GPL(pci_enable_pcie_error_reporting);

int pci_disable_pcie_error_reporting(struct pci_dev *dev)
{
	u16 reg16 = 0;
	int pos;

	if (pcie_aer_get_firmware_first(dev))
		return -EIO;

	pos = pci_pcie_cap(dev);
	if (!pos)
		return -EIO;

	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &reg16);
	reg16 &= ~(PCI_EXP_DEVCTL_CERE |
		PCI_EXP_DEVCTL_NFERE |
		PCI_EXP_DEVCTL_FERE |
		PCI_EXP_DEVCTL_URRE);
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, reg16);

	return 0;
}
EXPORT_SYMBOL_GPL(pci_disable_pcie_error_reporting);

int pci_cleanup_aer_uncorrect_error_status(struct pci_dev *dev)
{
	int pos;
	u32 status;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
	if (!pos)
		return -EIO;

	pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS, &status);
	if (status)
		pci_write_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS, status);

	return 0;
}
EXPORT_SYMBOL_GPL(pci_cleanup_aer_uncorrect_error_status);

/**
 * add_error_device - list device to be handled
 * @e_info: pointer to error info
 * @dev: pointer to pci_dev to be added
 */
static int add_error_device(struct aer_err_info *e_info, struct pci_dev *dev)
{
	if (e_info->error_dev_num < AER_MAX_MULTI_ERR_DEVICES) {
		e_info->dev[e_info->error_dev_num] = dev;
		e_info->error_dev_num++;
		return 0;
	}
	return -ENOSPC;
}

#define	PCI_BUS(x)	(((x) >> 8) & 0xff)

/**
 * is_error_source - check whether the device is source of reported error
 * @dev: pointer to pci_dev to be checked
 * @e_info: pointer to reported error info
 */
static bool is_error_source(struct pci_dev *dev, struct aer_err_info *e_info)
{
	int pos;
	u32 status, mask;
	u16 reg16;

	/*
	 * When bus id is equal to 0, it might be a bad id
	 * reported by root port.
	 */
	if (!nosourceid && (PCI_BUS(e_info->id) != 0)) {
		/* Device ID match? */
		if (e_info->id == ((dev->bus->number << 8) | dev->devfn))
			return true;

		/* Continue id comparing if there is no multiple error */
		if (!e_info->multi_error_valid)
			return false;
	}

	/*
	 * When either
	 *      1) nosourceid==y;
	 *      2) bus id is equal to 0. Some ports might lose the bus
	 *              id of error source id;
	 *      3) There are multiple errors and prior id comparing fails;
	 * We check AER status registers to find possible reporter.
	 */
	if (atomic_read(&dev->enable_cnt) == 0)
		return false;
	pos = pci_pcie_cap(dev);
	if (!pos)
		return false;

	/* Check if AER is enabled */
	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &reg16);
	if (!(reg16 & (
		PCI_EXP_DEVCTL_CERE |
		PCI_EXP_DEVCTL_NFERE |
		PCI_EXP_DEVCTL_FERE |
		PCI_EXP_DEVCTL_URRE)))
		return false;
	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
	if (!pos)
		return false;

	/* Check if error is recorded */
	if (e_info->severity == AER_CORRECTABLE) {
		pci_read_config_dword(dev, pos + PCI_ERR_COR_STATUS, &status);
		pci_read_config_dword(dev, pos + PCI_ERR_COR_MASK, &mask);
	} else {
		pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS, &status);
		pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_MASK, &mask);
	}
	if (status & ~mask)
		return true;

	return false;
}

static int find_device_iter(struct pci_dev *dev, void *data)
{
	struct aer_err_info *e_info = (struct aer_err_info *)data;

	if (is_error_source(dev, e_info)) {
		/* List this device */
		if (add_error_device(e_info, dev)) {
			/* We cannot handle more... Stop iteration */
			/* TODO: Should print error message here? */
			return 1;
		}

		/* If there is only a single error, stop iteration */
		if (!e_info->multi_error_valid)
			return 1;
	}
	return 0;
}

/**
 * find_source_device - search through device hierarchy for source device
 * @parent: pointer to Root Port pci_dev data structure
 * @e_info: including detailed error information such like id
 *
 * Return true if found.
 *
 * Invoked by DPC when error is detected at the Root Port.
 * Caller of this function must set id, severity, and multi_error_valid of
 * struct aer_err_info pointed by @e_info properly.  This function must fill
 * e_info->error_dev_num and e_info->dev[], based on the given information.
 */
static bool find_source_device(struct pci_dev *parent,
		struct aer_err_info *e_info)
{
	struct pci_dev *dev = parent;
	int result;

	/* Must reset in this function */
	e_info->error_dev_num = 0;

	/* Is Root Port an agent that sends error message? */
	result = find_device_iter(dev, e_info);
	if (result)
		return true;

	pci_walk_bus(parent->subordinate, find_device_iter, e_info);

	if (!e_info->error_dev_num) {
		dev_printk(KERN_DEBUG, &parent->dev,
				"can't find device of ID%04x\n",
				e_info->id);
		return false;
	}
	return true;
}

static int report_error_detected(struct pci_dev *dev, void *data)
{
	pci_ers_result_t vote;
	struct pci_error_handlers *err_handler;
	struct aer_broadcast_data *result_data;
	result_data = (struct aer_broadcast_data *) data;

	dev->error_state = result_data->state;

	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->error_detected) {
		if (result_data->state == pci_channel_io_frozen &&
			!(dev->hdr_type & PCI_HEADER_TYPE_BRIDGE)) {
			/*
			 * In case of fatal recovery, if one of down-
			 * stream device has no driver. We might be
			 * unable to recover because a later insmod
			 * of a driver for this device is unaware of
			 * its hw state.
			 */
			dev_printk(KERN_DEBUG, &dev->dev, "device has %s\n",
				   dev->driver ?
				   "no AER-aware driver" : "no driver");
		}
		return 0;
	}

	err_handler = dev->driver->err_handler;
	vote = err_handler->error_detected(dev, result_data->state);
	result_data->result = merge_result(result_data->result, vote);
	return 0;
}

static int report_mmio_enabled(struct pci_dev *dev, void *data)
{
	pci_ers_result_t vote;
	struct pci_error_handlers *err_handler;
	struct aer_broadcast_data *result_data;
	result_data = (struct aer_broadcast_data *) data;

	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->mmio_enabled)
		return 0;

	err_handler = dev->driver->err_handler;
	vote = err_handler->mmio_enabled(dev);
	result_data->result = merge_result(result_data->result, vote);
	return 0;
}

static int report_slot_reset(struct pci_dev *dev, void *data)
{
	pci_ers_result_t vote;
	struct pci_error_handlers *err_handler;
	struct aer_broadcast_data *result_data;
	result_data = (struct aer_broadcast_data *) data;

	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->slot_reset)
		return 0;

	err_handler = dev->driver->err_handler;
	vote = err_handler->slot_reset(dev);
	result_data->result = merge_result(result_data->result, vote);
	return 0;
}

static int report_resume(struct pci_dev *dev, void *data)
{
	struct pci_error_handlers *err_handler;

	dev->error_state = pci_channel_io_normal;

	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->resume)
		return 0;

	err_handler = dev->driver->err_handler;
	err_handler->resume(dev);
	return 0;
}

/**
 * broadcast_error_message - handle message broadcast to downstream drivers
 * @dev: pointer to from where in a hierarchy message is broadcasted down
 * @state: error state
 * @error_mesg: message to print
 * @cb: callback to be broadcasted
 *
 * Invoked during error recovery process. Once being invoked, the content
 * of error severity will be broadcasted to all downstream drivers in a
 * hierarchy in question.
 */
static pci_ers_result_t broadcast_error_message(struct pci_dev *dev,
	enum pci_channel_state state,
	char *error_mesg,
	int (*cb)(struct pci_dev *, void *))
{
	struct aer_broadcast_data result_data;

	dev_printk(KERN_DEBUG, &dev->dev, "broadcast %s message\n", error_mesg);
	result_data.state = state;
	if (cb == report_error_detected)
		result_data.result = PCI_ERS_RESULT_CAN_RECOVER;
	else
		result_data.result = PCI_ERS_RESULT_RECOVERED;

	if (dev->hdr_type & PCI_HEADER_TYPE_BRIDGE) {
		/*
		 * If the error is reported by a bridge, we think this error
		 * is related to the downstream link of the bridge, so we
		 * do error recovery on all subordinates of the bridge instead
		 * of the bridge and clear the error status of the bridge.
		 */
		if (cb == report_error_detected)
			dev->error_state = state;
		pci_walk_bus(dev->subordinate, cb, &result_data);
		if (cb == report_resume) {
			pci_cleanup_aer_uncorrect_error_status(dev);
			dev->error_state = pci_channel_io_normal;
		}
	} else {
		/*
		 * If the error is reported by an end point, we think this
		 * error is related to the upstream link of the end point.
		 */
		pci_walk_bus(dev->bus, cb, &result_data);
	}

	return result_data.result;
}

/**
 * aer_do_secondary_bus_reset - perform secondary bus reset
 * @dev: pointer to bridge's pci_dev data structure
 *
 * Invoked when performing link reset at Root Port or Downstream Port.
 */
void aer_do_secondary_bus_reset(struct pci_dev *dev)
{
	u16 p2p_ctrl;

	/* Assert Secondary Bus Reset */
	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &p2p_ctrl);
	p2p_ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, p2p_ctrl);

	/*
	 * we should send hot reset message for 2ms to allow it time to
	 * propagate to all downstream ports
	 */
	msleep(2);

	/* De-assert Secondary Bus Reset */
	p2p_ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, p2p_ctrl);

	/*
	 * System software must wait for at least 100ms from the end
	 * of a reset of one or more device before it is permitted
	 * to issue Configuration Requests to those devices.
	 */
	msleep(200);
}

/**
 * default_downstream_reset_link - default reset function for Downstream Port
 * @dev: pointer to downstream port's pci_dev data structure
 *
 * Invoked when performing link reset at Downstream Port w/ no aer driver.
 */
static pci_ers_result_t default_downstream_reset_link(struct pci_dev *dev)
{
	aer_do_secondary_bus_reset(dev);
	dev_printk(KERN_DEBUG, &dev->dev,
		"Downstream Port link has been reset\n");
	return PCI_ERS_RESULT_RECOVERED;
}

static int find_aer_service_iter(struct device *device, void *data)
{
	struct pcie_port_service_driver *service_driver, **drv;

	drv = (struct pcie_port_service_driver **) data;

	if (device->bus == &pcie_port_bus_type && device->driver) {
		service_driver = to_service_driver(device->driver);
		if (service_driver->service == PCIE_PORT_SERVICE_AER) {
			*drv = service_driver;
			return 1;
		}
	}

	return 0;
}

static struct pcie_port_service_driver *find_aer_service(struct pci_dev *dev)
{
	struct pcie_port_service_driver *drv = NULL;

	device_for_each_child(&dev->dev, &drv, find_aer_service_iter);

	return drv;
}

static pci_ers_result_t reset_link(struct pcie_device *aerdev,
		struct pci_dev *dev)
{
	struct pci_dev *udev;
	pci_ers_result_t status;
	struct pcie_port_service_driver *driver;

	if (dev->hdr_type & PCI_HEADER_TYPE_BRIDGE) {
		/* Reset this port for all subordinates */
		udev = dev;
	} else {
		/* Reset the upstream component (likely downstream port) */
		udev = dev->bus->self;
	}

	/* Use the aer driver of the component firstly */
	driver = find_aer_service(udev);

	if (driver && driver->reset_link) {
		status = driver->reset_link(udev);
	} else if (udev->pcie_type == PCI_EXP_TYPE_DOWNSTREAM) {
		status = default_downstream_reset_link(udev);
	} else {
		dev_printk(KERN_DEBUG, &dev->dev,
			"no link-reset support at upstream device %s\n",
			pci_name(udev));
		return PCI_ERS_RESULT_DISCONNECT;
	}

	if (status != PCI_ERS_RESULT_RECOVERED) {
		dev_printk(KERN_DEBUG, &dev->dev,
			"link reset at upstream device %s failed\n",
			pci_name(udev));
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return status;
}

/**
 * do_recovery - handle nonfatal/fatal error recovery process
 * @aerdev: pointer to a pcie_device data structure of root port
 * @dev: pointer to a pci_dev data structure of agent detecting an error
 * @severity: error severity type
 *
 * Invoked when an error is nonfatal/fatal. Once being invoked, broadcast
 * error detected message to all downstream drivers within a hierarchy in
 * question and return the returned code.
 */
static void do_recovery(struct pcie_device *aerdev, struct pci_dev *dev,
		int severity)
{
	pci_ers_result_t status, result = PCI_ERS_RESULT_RECOVERED;
	enum pci_channel_state state;

	if (severity == AER_FATAL)
		state = pci_channel_io_frozen;
	else
		state = pci_channel_io_normal;

	status = broadcast_error_message(dev,
			state,
			"error_detected",
			report_error_detected);

	if (severity == AER_FATAL) {
		result = reset_link(aerdev, dev);
		if (result != PCI_ERS_RESULT_RECOVERED)
			goto failed;
	}

	if (status == PCI_ERS_RESULT_CAN_RECOVER)
		status = broadcast_error_message(dev,
				state,
				"mmio_enabled",
				report_mmio_enabled);

	if (status == PCI_ERS_RESULT_NEED_RESET) {
		/*
		 * TODO: Should call platform-specific
		 * functions to reset slot before calling
		 * drivers' slot_reset callbacks?
		 */
		status = broadcast_error_message(dev,
				state,
				"slot_reset",
				report_slot_reset);
	}

	if (status != PCI_ERS_RESULT_RECOVERED)
		goto failed;

	broadcast_error_message(dev,
				state,
				"resume",
				report_resume);

	dev_printk(KERN_DEBUG, &dev->dev,
		"AER driver successfully recovered\n");
	return;

failed:
	/* TODO: Should kernel panic here? */
	dev_printk(KERN_DEBUG, &dev->dev,
		"AER driver didn't recover\n");
}

/**
 * handle_error_source - handle logging error into an event log
 * @aerdev: pointer to pcie_device data structure of the root port
 * @dev: pointer to pci_dev data structure of error source device
 * @info: comprehensive error information
 *
 * Invoked when an error being detected by Root Port.
 */
static void handle_error_source(struct pcie_device *aerdev,
	struct pci_dev *dev,
	struct aer_err_info *info)
{
	int pos;

	if (info->severity == AER_CORRECTABLE) {
		/*
		 * Correctable error does not need software intevention.
		 * No need to go through error recovery process.
		 */
		pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
		if (pos)
			pci_write_config_dword(dev, pos + PCI_ERR_COR_STATUS,
					info->status);
	} else
		do_recovery(aerdev, dev, info->severity);
}

/**
 * get_device_error_info - read error status from dev and store it to info
 * @dev: pointer to the device expected to have a error record
 * @info: pointer to structure to store the error record
 *
 * Return 1 on success, 0 on error.
 *
 * Note that @info is reused among all error devices. Clear fields properly.
 */
static int get_device_error_info(struct pci_dev *dev, struct aer_err_info *info)
{
	int pos, temp;

	/* Must reset in this function */
	info->status = 0;
	info->tlp_header_valid = 0;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);

	/* The device might not support AER */
	if (!pos)
		return 1;

	if (info->severity == AER_CORRECTABLE) {
		pci_read_config_dword(dev, pos + PCI_ERR_COR_STATUS,
			&info->status);
		pci_read_config_dword(dev, pos + PCI_ERR_COR_MASK,
			&info->mask);
		if (!(info->status & ~info->mask))
			return 0;
	} else if (dev->hdr_type & PCI_HEADER_TYPE_BRIDGE ||
		info->severity == AER_NONFATAL) {

		/* Link is still healthy for IO reads */
		pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS,
			&info->status);
		pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_MASK,
			&info->mask);
		if (!(info->status & ~info->mask))
			return 0;

		/* Get First Error Pointer */
		pci_read_config_dword(dev, pos + PCI_ERR_CAP, &temp);
		info->first_error = PCI_ERR_CAP_FEP(temp);

		if (info->status & AER_LOG_TLP_MASKS) {
			info->tlp_header_valid = 1;
			pci_read_config_dword(dev,
				pos + PCI_ERR_HEADER_LOG, &info->tlp.dw0);
			pci_read_config_dword(dev,
				pos + PCI_ERR_HEADER_LOG + 4, &info->tlp.dw1);
			pci_read_config_dword(dev,
				pos + PCI_ERR_HEADER_LOG + 8, &info->tlp.dw2);
			pci_read_config_dword(dev,
				pos + PCI_ERR_HEADER_LOG + 12, &info->tlp.dw3);
		}
	}

	return 1;
}

static inline void aer_process_err_devices(struct pcie_device *p_device,
			struct aer_err_info *e_info)
{
	int i;

	/* Report all before handle them, not to lost records by reset etc. */
	for (i = 0; i < e_info->error_dev_num && e_info->dev[i]; i++) {
		if (get_device_error_info(e_info->dev[i], e_info))
			aer_print_error(e_info->dev[i], e_info);
	}
	for (i = 0; i < e_info->error_dev_num && e_info->dev[i]; i++) {
		if (get_device_error_info(e_info->dev[i], e_info))
			handle_error_source(p_device, e_info->dev[i], e_info);
	}
}

/**
 * aer_isr_one_error - consume an error detected by root port
 * @p_device: pointer to error root port service device
 * @e_src: pointer to an error source
 */
static void aer_isr_one_error(struct pcie_device *p_device,
		struct aer_err_source *e_src)
{
	struct aer_err_info *e_info;

	/* struct aer_err_info might be big, so we allocate it with slab */
	e_info = kmalloc(sizeof(struct aer_err_info), GFP_KERNEL);
	if (!e_info) {
		dev_printk(KERN_DEBUG, &p_device->port->dev,
			"Can't allocate mem when processing AER errors\n");
		return;
	}

	/*
	 * There is a possibility that both correctable error and
	 * uncorrectable error being logged. Report correctable error first.
	 */
	if (e_src->status & PCI_ERR_ROOT_COR_RCV) {
		e_info->id = ERR_COR_ID(e_src->id);
		e_info->severity = AER_CORRECTABLE;

		if (e_src->status & PCI_ERR_ROOT_MULTI_COR_RCV)
			e_info->multi_error_valid = 1;
		else
			e_info->multi_error_valid = 0;

		aer_print_port_info(p_device->port, e_info);

		if (find_source_device(p_device->port, e_info))
			aer_process_err_devices(p_device, e_info);
	}

	if (e_src->status & PCI_ERR_ROOT_UNCOR_RCV) {
		e_info->id = ERR_UNCOR_ID(e_src->id);

		if (e_src->status & PCI_ERR_ROOT_FATAL_RCV)
			e_info->severity = AER_FATAL;
		else
			e_info->severity = AER_NONFATAL;

		if (e_src->status & PCI_ERR_ROOT_MULTI_UNCOR_RCV)
			e_info->multi_error_valid = 1;
		else
			e_info->multi_error_valid = 0;

		aer_print_port_info(p_device->port, e_info);

		if (find_source_device(p_device->port, e_info))
			aer_process_err_devices(p_device, e_info);
	}

	kfree(e_info);
}

/**
 * get_e_source - retrieve an error source
 * @rpc: pointer to the root port which holds an error
 * @e_src: pointer to store retrieved error source
 *
 * Return 1 if an error source is retrieved, otherwise 0.
 *
 * Invoked by DPC handler to consume an error.
 */
static int get_e_source(struct aer_rpc *rpc, struct aer_err_source *e_src)
{
	unsigned long flags;

	/* Lock access to Root error producer/consumer index */
	spin_lock_irqsave(&rpc->e_lock, flags);
	if (rpc->prod_idx == rpc->cons_idx) {
		spin_unlock_irqrestore(&rpc->e_lock, flags);
		return 0;
	}

	*e_src = rpc->e_sources[rpc->cons_idx];
	rpc->cons_idx++;
	if (rpc->cons_idx == AER_ERROR_SOURCES_MAX)
		rpc->cons_idx = 0;
	spin_unlock_irqrestore(&rpc->e_lock, flags);

	return 1;
}

/**
 * aer_isr - consume errors detected by root port
 * @work: definition of this work item
 *
 * Invoked, as DPC, when root port records new detected error
 */
void aer_isr(struct work_struct *work)
{
	struct aer_rpc *rpc = container_of(work, struct aer_rpc, dpc_handler);
	struct pcie_device *p_device = rpc->rpd;
	struct aer_err_source uninitialized_var(e_src);

	mutex_lock(&rpc->rpc_mutex);
	while (get_e_source(rpc, &e_src))
		aer_isr_one_error(p_device, &e_src);
	mutex_unlock(&rpc->rpc_mutex);

	wake_up(&rpc->wait_release);
}

/**
 * aer_init - provide AER initialization
 * @dev: pointer to AER pcie device
 *
 * Invoked when AER service driver is loaded.
 */
int aer_init(struct pcie_device *dev)
{
	if (forceload) {
		dev_printk(KERN_DEBUG, &dev->device,
			   "aerdrv forceload requested.\n");
		pcie_aer_force_firmware_first(dev->port, 0);
	}
	return 0;
}
