/*
 * PCI HotPlug Core Functions
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
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
 * Send feedback to <kristen.c.accardi@intel.com>
 *
 */
#ifndef _PCI_HOTPLUG_H
#define _PCI_HOTPLUG_H

/* These values come from the PCI Express Spec */
enum pcie_link_width {
	PCIE_LNK_WIDTH_RESRV	= 0x00,
	PCIE_LNK_X1		= 0x01,
	PCIE_LNK_X2		= 0x02,
	PCIE_LNK_X4		= 0x04,
	PCIE_LNK_X8		= 0x08,
	PCIE_LNK_X12		= 0x0C,
	PCIE_LNK_X16		= 0x10,
	PCIE_LNK_X32		= 0x20,
	PCIE_LNK_WIDTH_UNKNOWN  = 0xFF,
};

/**
 * struct hotplug_slot_ops -the callbacks that the hotplug pci core can use
 * @owner: The module owner of this structure
 * @mod_name: The module name (KBUILD_MODNAME) of this structure
 * @enable_slot: Called when the user wants to enable a specific pci slot
 * @disable_slot: Called when the user wants to disable a specific pci slot
 * @set_attention_status: Called to set the specific slot's attention LED to
 * the specified value
 * @hardware_test: Called to run a specified hardware test on the specified
 * slot.
 * @get_power_status: Called to get the current power status of a slot.
 * 	If this field is NULL, the value passed in the struct hotplug_slot_info
 * 	will be used when this value is requested by a user.
 * @get_attention_status: Called to get the current attention status of a slot.
 *	If this field is NULL, the value passed in the struct hotplug_slot_info
 *	will be used when this value is requested by a user.
 * @get_latch_status: Called to get the current latch status of a slot.
 *	If this field is NULL, the value passed in the struct hotplug_slot_info
 *	will be used when this value is requested by a user.
 * @get_adapter_status: Called to get see if an adapter is present in the slot or not.
 *	If this field is NULL, the value passed in the struct hotplug_slot_info
 *	will be used when this value is requested by a user.
 *
 * The table of function pointers that is passed to the hotplug pci core by a
 * hotplug pci driver.  These functions are called by the hotplug pci core when
 * the user wants to do something to a specific slot (query it for information,
 * set an LED, enable / disable power, etc.)
 */
struct hotplug_slot_ops {
	struct module *owner;
	const char *mod_name;
	int (*enable_slot)		(struct hotplug_slot *slot);
	int (*disable_slot)		(struct hotplug_slot *slot);
	int (*set_attention_status)	(struct hotplug_slot *slot, u8 value);
	int (*hardware_test)		(struct hotplug_slot *slot, u32 value);
	int (*get_power_status)		(struct hotplug_slot *slot, u8 *value);
	int (*get_attention_status)	(struct hotplug_slot *slot, u8 *value);
	int (*get_latch_status)		(struct hotplug_slot *slot, u8 *value);
	int (*get_adapter_status)	(struct hotplug_slot *slot, u8 *value);
};

/**
 * struct hotplug_slot_info - used to notify the hotplug pci core of the state of the slot
 * @power_status: if power is enabled or not (1/0)
 * @attention_status: if the attention light is enabled or not (1/0)
 * @latch_status: if the latch (if any) is open or closed (1/0)
 * @adapter_status: if there is a pci board present in the slot or not (1/0)
 *
 * Used to notify the hotplug pci core of the status of a specific slot.
 */
struct hotplug_slot_info {
	u8	power_status;
	u8	attention_status;
	u8	latch_status;
	u8	adapter_status;
};

/**
 * struct hotplug_slot - used to register a physical slot with the hotplug pci core
 * @ops: pointer to the &struct hotplug_slot_ops to be used for this slot
 * @info: pointer to the &struct hotplug_slot_info for the initial values for
 * this slot.
 * @release: called during pci_hp_deregister to free memory allocated in a
 * hotplug_slot structure.
 * @private: used by the hotplug pci controller driver to store whatever it
 * needs.
 */
struct hotplug_slot {
	struct hotplug_slot_ops		*ops;
	struct hotplug_slot_info	*info;
	void (*release) (struct hotplug_slot *slot);
	void				*private;

	/* Variables below this are for use only by the hotplug pci core. */
	struct list_head		slot_list;
	struct pci_slot			*pci_slot;
};
#define to_hotplug_slot(n) container_of(n, struct hotplug_slot, kobj)

static inline const char *hotplug_slot_name(const struct hotplug_slot *slot)
{
	return pci_slot_name(slot->pci_slot);
}

extern int __pci_hp_register(struct hotplug_slot *slot, struct pci_bus *pbus,
			     int nr, const char *name,
			     struct module *owner, const char *mod_name);
extern int pci_hp_deregister(struct hotplug_slot *slot);
extern int __must_check pci_hp_change_slot_info	(struct hotplug_slot *slot,
						 struct hotplug_slot_info *info);

static inline int pci_hp_register(struct hotplug_slot *slot,
				  struct pci_bus *pbus,
				  int devnr, const char *name)
{
	return __pci_hp_register(slot, pbus, devnr, name,
				 THIS_MODULE, KBUILD_MODNAME);
}

/* PCI Setting Record (Type 0) */
struct hpp_type0 {
	u32 revision;
	u8  cache_line_size;
	u8  latency_timer;
	u8  enable_serr;
	u8  enable_perr;
};

/* PCI-X Setting Record (Type 1) */
struct hpp_type1 {
	u32 revision;
	u8  max_mem_read;
	u8  avg_max_split;
	u16 tot_max_split;
};

/* PCI Express Setting Record (Type 2) */
struct hpp_type2 {
	u32 revision;
	u32 unc_err_mask_and;
	u32 unc_err_mask_or;
	u32 unc_err_sever_and;
	u32 unc_err_sever_or;
	u32 cor_err_mask_and;
	u32 cor_err_mask_or;
	u32 adv_err_cap_and;
	u32 adv_err_cap_or;
	u16 pci_exp_devctl_and;
	u16 pci_exp_devctl_or;
	u16 pci_exp_lnkctl_and;
	u16 pci_exp_lnkctl_or;
	u32 sec_unc_err_sever_and;
	u32 sec_unc_err_sever_or;
	u32 sec_unc_err_mask_and;
	u32 sec_unc_err_mask_or;
};

struct hotplug_params {
	struct hpp_type0 *t0;		/* Type0: NULL if not available */
	struct hpp_type1 *t1;		/* Type1: NULL if not available */
	struct hpp_type2 *t2;		/* Type2: NULL if not available */
	struct hpp_type0 type0_data;
	struct hpp_type1 type1_data;
	struct hpp_type2 type2_data;
};

#ifdef CONFIG_ACPI
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>
int pci_get_hp_params(struct pci_dev *dev, struct hotplug_params *hpp);
int acpi_get_hp_hw_control_from_firmware(struct pci_dev *dev, u32 flags);
int acpi_pci_check_ejectable(struct pci_bus *pbus, acpi_handle handle);
int acpi_pci_detect_ejectable(acpi_handle handle);
#else
static inline int pci_get_hp_params(struct pci_dev *dev,
				    struct hotplug_params *hpp)
{
	return -ENODEV;
}
#endif

void pci_configure_slot(struct pci_dev *dev);
#endif
