/*
 * EFI Variables - efivars.c
 *
 * Copyright (C) 2001,2003,2004 Dell <Matt_Domsch@dell.com>
 * Copyright (C) 2004 Intel Corporation <matthew.e.tolentino@intel.com>
 *
 * This code takes all variables accessible from EFI runtime and
 *  exports them via sysfs
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Changelog:
 *
 *  17 May 2004 - Matt Domsch <Matt_Domsch@dell.com>
 *   remove check for efi_enabled in exit
 *   add MODULE_VERSION
 *
 *  26 Apr 2004 - Matt Domsch <Matt_Domsch@dell.com>
 *   minor bug fixes
 *
 *  21 Apr 2004 - Matt Tolentino <matthew.e.tolentino@intel.com)
 *   converted driver to export variable information via sysfs
 *   and moved to drivers/firmware directory
 *   bumped revision number to v0.07 to reflect conversion & move
 *
 *  10 Dec 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   fix locking per Peter Chubb's findings
 *
 *  25 Mar 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   move uuid_unparse() to include/asm-ia64/efi.h:efi_guid_unparse()
 *
 *  12 Feb 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   use list_for_each_safe when deleting vars.
 *   remove ifdef CONFIG_SMP around include <linux/smp.h>
 *   v0.04 release to linux-ia64@linuxia64.org
 *
 *  20 April 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   Moved vars from /proc/efi to /proc/efi/vars, and made
 *   efi.c own the /proc/efi directory.
 *   v0.03 release to linux-ia64@linuxia64.org
 *
 *  26 March 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   At the request of Stephane, moved ownership of /proc/efi
 *   to efi.c, and now efivars lives under /proc/efi/vars.
 *
 *  12 March 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   Feedback received from Stephane Eranian incorporated.
 *   efivar_write() checks copy_from_user() return value.
 *   efivar_read/write() returns proper errno.
 *   v0.02 release to linux-ia64@linuxia64.org
 *
 *  26 February 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   v0.01 release to linux-ia64@linuxia64.org
 */

#include <linux/capability.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <linux/efi.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#define EFIVARS_VERSION "0.08"
#define EFIVARS_DATE "2004-May-17"

MODULE_AUTHOR("Matt Domsch <Matt_Domsch@Dell.com>");
MODULE_DESCRIPTION("sysfs interface to EFI Variables");
MODULE_LICENSE("GPL");
MODULE_VERSION(EFIVARS_VERSION);

/*
 * efivars_lock protects two things:
 * 1) efivar_list - adds, removals, reads, writes
 * 2) efi.[gs]et_variable() calls.
 * It must not be held when creating sysfs entries or calling kmalloc.
 * efi.get_next_variable() is only called from efivars_init(),
 * which is protected by the BKL, so that path is safe.
 */
static DEFINE_SPINLOCK(efivars_lock);
static LIST_HEAD(efivar_list);

/*
 * The maximum size of VariableName + Data = 1024
 * Therefore, it's reasonable to save that much
 * space in each part of the structure,
 * and we use a page for reading/writing.
 */

struct efi_variable {
	efi_char16_t  VariableName[1024/sizeof(efi_char16_t)];
	efi_guid_t    VendorGuid;
	unsigned long DataSize;
	__u8          Data[1024];
	efi_status_t  Status;
	__u32         Attributes;
} __attribute__((packed));


struct efivar_entry {
	struct efi_variable var;
	struct list_head list;
	struct kobject kobj;
};

struct efivar_attribute {
	struct attribute attr;
	ssize_t (*show) (struct efivar_entry *entry, char *buf);
	ssize_t (*store)(struct efivar_entry *entry, const char *buf, size_t count);
};


#define EFIVAR_ATTR(_name, _mode, _show, _store) \
struct efivar_attribute efivar_attr_##_name = { \
	.attr = {.name = __stringify(_name), .mode = _mode}, \
	.show = _show, \
	.store = _store, \
};

#define to_efivar_attr(_attr) container_of(_attr, struct efivar_attribute, attr)
#define to_efivar_entry(obj)  container_of(obj, struct efivar_entry, kobj)

/*
 * Prototype for sysfs creation function
 */
static int
efivar_create_sysfs_entry(unsigned long variable_name_size,
				efi_char16_t *variable_name,
				efi_guid_t *vendor_guid);

/* Return the number of unicode characters in data */
static unsigned long
utf8_strlen(efi_char16_t *data, unsigned long maxlength)
{
	unsigned long length = 0;

	while (*data++ != 0 && length < maxlength)
		length++;
	return length;
}

/*
 * Return the number of bytes is the length of this string
 * Note: this is NOT the same as the number of unicode characters
 */
static inline unsigned long
utf8_strsize(efi_char16_t *data, unsigned long maxlength)
{
	return utf8_strlen(data, maxlength/sizeof(efi_char16_t)) * sizeof(efi_char16_t);
}

static efi_status_t
get_var_data(struct efi_variable *var)
{
	efi_status_t status;

	spin_lock(&efivars_lock);
	var->DataSize = 1024;
	status = efi.get_variable(var->VariableName,
				&var->VendorGuid,
				&var->Attributes,
				&var->DataSize,
				var->Data);
	spin_unlock(&efivars_lock);
	if (status != EFI_SUCCESS) {
		printk(KERN_WARNING "efivars: get_variable() failed 0x%lx!\n",
			status);
	}
	return status;
}

static ssize_t
efivar_guid_read(struct efivar_entry *entry, char *buf)
{
	struct efi_variable *var = &entry->var;
	char *str = buf;

	if (!entry || !buf)
		return 0;

	efi_guid_unparse(&var->VendorGuid, str);
	str += strlen(str);
	str += sprintf(str, "\n");

	return str - buf;
}

static ssize_t
efivar_attr_read(struct efivar_entry *entry, char *buf)
{
	struct efi_variable *var = &entry->var;
	char *str = buf;
	efi_status_t status;

	if (!entry || !buf)
		return -EINVAL;

	status = get_var_data(var);
	if (status != EFI_SUCCESS)
		return -EIO;

	if (var->Attributes & 0x1)
		str += sprintf(str, "EFI_VARIABLE_NON_VOLATILE\n");
	if (var->Attributes & 0x2)
		str += sprintf(str, "EFI_VARIABLE_BOOTSERVICE_ACCESS\n");
	if (var->Attributes & 0x4)
		str += sprintf(str, "EFI_VARIABLE_RUNTIME_ACCESS\n");
	return str - buf;
}

static ssize_t
efivar_size_read(struct efivar_entry *entry, char *buf)
{
	struct efi_variable *var = &entry->var;
	char *str = buf;
	efi_status_t status;

	if (!entry || !buf)
		return -EINVAL;

	status = get_var_data(var);
	if (status != EFI_SUCCESS)
		return -EIO;

	str += sprintf(str, "0x%lx\n", var->DataSize);
	return str - buf;
}

static ssize_t
efivar_data_read(struct efivar_entry *entry, char *buf)
{
	struct efi_variable *var = &entry->var;
	efi_status_t status;

	if (!entry || !buf)
		return -EINVAL;

	status = get_var_data(var);
	if (status != EFI_SUCCESS)
		return -EIO;

	memcpy(buf, var->Data, var->DataSize);
	return var->DataSize;
}
/*
 * We allow each variable to be edited via rewriting the
 * entire efi variable structure.
 */
static ssize_t
efivar_store_raw(struct efivar_entry *entry, const char *buf, size_t count)
{
	struct efi_variable *new_var, *var = &entry->var;
	efi_status_t status = EFI_NOT_FOUND;

	if (count != sizeof(struct efi_variable))
		return -EINVAL;

	new_var = (struct efi_variable *)buf;
	/*
	 * If only updating the variable data, then the name
	 * and guid should remain the same
	 */
	if (memcmp(new_var->VariableName, var->VariableName, sizeof(var->VariableName)) ||
		efi_guidcmp(new_var->VendorGuid, var->VendorGuid)) {
		printk(KERN_ERR "efivars: Cannot edit the wrong variable!\n");
		return -EINVAL;
	}

	if ((new_var->DataSize <= 0) || (new_var->Attributes == 0)){
		printk(KERN_ERR "efivars: DataSize & Attributes must be valid!\n");
		return -EINVAL;
	}

	spin_lock(&efivars_lock);
	status = efi.set_variable(new_var->VariableName,
					&new_var->VendorGuid,
					new_var->Attributes,
					new_var->DataSize,
					new_var->Data);

	spin_unlock(&efivars_lock);

	if (status != EFI_SUCCESS) {
		printk(KERN_WARNING "efivars: set_variable() failed: status=%lx\n",
			status);
		return -EIO;
	}

	memcpy(&entry->var, new_var, count);
	return count;
}

static ssize_t
efivar_show_raw(struct efivar_entry *entry, char *buf)
{
	struct efi_variable *var = &entry->var;
	efi_status_t status;

	if (!entry || !buf)
		return 0;

	status = get_var_data(var);
	if (status != EFI_SUCCESS)
		return -EIO;

	memcpy(buf, var, sizeof(*var));
	return sizeof(*var);
}

/*
 * Generic read/write functions that call the specific functions of
 * the atttributes...
 */
static ssize_t efivar_attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct efivar_entry *var = to_efivar_entry(kobj);
	struct efivar_attribute *efivar_attr = to_efivar_attr(attr);
	ssize_t ret = -EIO;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (efivar_attr->show) {
		ret = efivar_attr->show(var, buf);
	}
	return ret;
}

static ssize_t efivar_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct efivar_entry *var = to_efivar_entry(kobj);
	struct efivar_attribute *efivar_attr = to_efivar_attr(attr);
	ssize_t ret = -EIO;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (efivar_attr->store)
		ret = efivar_attr->store(var, buf, count);

	return ret;
}

static const struct sysfs_ops efivar_attr_ops = {
	.show = efivar_attr_show,
	.store = efivar_attr_store,
};

static void efivar_release(struct kobject *kobj)
{
	struct efivar_entry *var = container_of(kobj, struct efivar_entry, kobj);
	kfree(var);
}

static EFIVAR_ATTR(guid, 0400, efivar_guid_read, NULL);
static EFIVAR_ATTR(attributes, 0400, efivar_attr_read, NULL);
static EFIVAR_ATTR(size, 0400, efivar_size_read, NULL);
static EFIVAR_ATTR(data, 0400, efivar_data_read, NULL);
static EFIVAR_ATTR(raw_var, 0600, efivar_show_raw, efivar_store_raw);

static struct attribute *def_attrs[] = {
	&efivar_attr_guid.attr,
	&efivar_attr_size.attr,
	&efivar_attr_attributes.attr,
	&efivar_attr_data.attr,
	&efivar_attr_raw_var.attr,
	NULL,
};

static struct kobj_type efivar_ktype = {
	.release = efivar_release,
	.sysfs_ops = &efivar_attr_ops,
	.default_attrs = def_attrs,
};

static inline void
efivar_unregister(struct efivar_entry *var)
{
	kobject_put(&var->kobj);
}


static ssize_t efivar_create(struct file *filp, struct kobject *kobj,
			     struct bin_attribute *bin_attr,
			     char *buf, loff_t pos, size_t count)
{
	struct efi_variable *new_var = (struct efi_variable *)buf;
	struct efivar_entry *search_efivar, *n;
	unsigned long strsize1, strsize2;
	efi_status_t status = EFI_NOT_FOUND;
	int found = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock(&efivars_lock);

	/*
	 * Does this variable already exist?
	 */
	list_for_each_entry_safe(search_efivar, n, &efivar_list, list) {
		strsize1 = utf8_strsize(search_efivar->var.VariableName, 1024);
		strsize2 = utf8_strsize(new_var->VariableName, 1024);
		if (strsize1 == strsize2 &&
			!memcmp(&(search_efivar->var.VariableName),
				new_var->VariableName, strsize1) &&
			!efi_guidcmp(search_efivar->var.VendorGuid,
				new_var->VendorGuid)) {
			found = 1;
			break;
		}
	}
	if (found) {
		spin_unlock(&efivars_lock);
		return -EINVAL;
	}

	/* now *really* create the variable via EFI */
	status = efi.set_variable(new_var->VariableName,
			&new_var->VendorGuid,
			new_var->Attributes,
			new_var->DataSize,
			new_var->Data);

	if (status != EFI_SUCCESS) {
		printk(KERN_WARNING "efivars: set_variable() failed: status=%lx\n",
			status);
		spin_unlock(&efivars_lock);
		return -EIO;
	}
	spin_unlock(&efivars_lock);

	/* Create the entry in sysfs.  Locking is not required here */
	status = efivar_create_sysfs_entry(utf8_strsize(new_var->VariableName,
			1024), new_var->VariableName, &new_var->VendorGuid);
	if (status) {
		printk(KERN_WARNING "efivars: variable created, but sysfs entry wasn't.\n");
	}
	return count;
}

static ssize_t efivar_delete(struct file *filp, struct kobject *kobj,
			     struct bin_attribute *bin_attr,
			     char *buf, loff_t pos, size_t count)
{
	struct efi_variable *del_var = (struct efi_variable *)buf;
	struct efivar_entry *search_efivar, *n;
	unsigned long strsize1, strsize2;
	efi_status_t status = EFI_NOT_FOUND;
	int found = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock(&efivars_lock);

	/*
	 * Does this variable already exist?
	 */
	list_for_each_entry_safe(search_efivar, n, &efivar_list, list) {
		strsize1 = utf8_strsize(search_efivar->var.VariableName, 1024);
		strsize2 = utf8_strsize(del_var->VariableName, 1024);
		if (strsize1 == strsize2 &&
			!memcmp(&(search_efivar->var.VariableName),
				del_var->VariableName, strsize1) &&
			!efi_guidcmp(search_efivar->var.VendorGuid,
				del_var->VendorGuid)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		spin_unlock(&efivars_lock);
		return -EINVAL;
	}
	/* force the Attributes/DataSize to 0 to ensure deletion */
	del_var->Attributes = 0;
	del_var->DataSize = 0;

	status = efi.set_variable(del_var->VariableName,
			&del_var->VendorGuid,
			del_var->Attributes,
			del_var->DataSize,
			del_var->Data);

	if (status != EFI_SUCCESS) {
		printk(KERN_WARNING "efivars: set_variable() failed: status=%lx\n",
			status);
		spin_unlock(&efivars_lock);
		return -EIO;
	}
	list_del(&search_efivar->list);
	/* We need to release this lock before unregistering. */
	spin_unlock(&efivars_lock);
	efivar_unregister(search_efivar);

	/* It's dead Jim.... */
	return count;
}

static struct bin_attribute var_subsys_attr_new_var = {
	.attr = {.name = "new_var", .mode = 0200},
	.write = efivar_create,
};

static struct bin_attribute var_subsys_attr_del_var = {
	.attr = {.name = "del_var", .mode = 0200},
	.write = efivar_delete,
};

/*
 * Let's not leave out systab information that snuck into
 * the efivars driver
 */
static ssize_t systab_show(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	char *str = buf;

	if (!kobj || !buf)
		return -EINVAL;

	if (efi.mps != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "MPS=0x%lx\n", efi.mps);
	if (efi.acpi20 != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "ACPI20=0x%lx\n", efi.acpi20);
	if (efi.acpi != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "ACPI=0x%lx\n", efi.acpi);
	if (efi.smbios != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "SMBIOS=0x%lx\n", efi.smbios);
	if (efi.hcdp != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "HCDP=0x%lx\n", efi.hcdp);
	if (efi.boot_info != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "BOOTINFO=0x%lx\n", efi.boot_info);
	if (efi.uga != EFI_INVALID_TABLE_ADDR)
		str += sprintf(str, "UGA=0x%lx\n", efi.uga);

	return str - buf;
}

static struct kobj_attribute efi_attr_systab =
			__ATTR(systab, 0400, systab_show, NULL);

static struct attribute *efi_subsys_attrs[] = {
	&efi_attr_systab.attr,
	NULL,	/* maybe more in the future? */
};

static struct attribute_group efi_subsys_attr_group = {
	.attrs = efi_subsys_attrs,
};


static struct kset *vars_kset;
static struct kobject *efi_kobj;

/*
 * efivar_create_sysfs_entry()
 * Requires:
 *    variable_name_size = number of bytes required to hold
 *                         variable_name (not counting the NULL
 *                         character at the end.
 *    efivars_lock is not held on entry or exit.
 * Returns 1 on failure, 0 on success
 */
static int
efivar_create_sysfs_entry(unsigned long variable_name_size,
			efi_char16_t *variable_name,
			efi_guid_t *vendor_guid)
{
	int i, short_name_size = variable_name_size / sizeof(efi_char16_t) + 38;
	char *short_name;
	struct efivar_entry *new_efivar;

	short_name = kzalloc(short_name_size + 1, GFP_KERNEL);
	new_efivar = kzalloc(sizeof(struct efivar_entry), GFP_KERNEL);

	if (!short_name || !new_efivar)  {
		kfree(short_name);
		kfree(new_efivar);
		return 1;
	}

	memcpy(new_efivar->var.VariableName, variable_name,
		variable_name_size);
	memcpy(&(new_efivar->var.VendorGuid), vendor_guid, sizeof(efi_guid_t));

	/* Convert Unicode to normal chars (assume top bits are 0),
	   ala UTF-8 */
	for (i=0; i < (int)(variable_name_size / sizeof(efi_char16_t)); i++) {
		short_name[i] = variable_name[i] & 0xFF;
	}
	/* This is ugly, but necessary to separate one vendor's
	   private variables from another's.         */

	*(short_name + strlen(short_name)) = '-';
	efi_guid_unparse(vendor_guid, short_name + strlen(short_name));

	new_efivar->kobj.kset = vars_kset;
	i = kobject_init_and_add(&new_efivar->kobj, &efivar_ktype, NULL,
				 "%s", short_name);
	if (i) {
		kfree(short_name);
		kfree(new_efivar);
		return 1;
	}

	kobject_uevent(&new_efivar->kobj, KOBJ_ADD);
	kfree(short_name);
	short_name = NULL;

	spin_lock(&efivars_lock);
	list_add(&new_efivar->list, &efivar_list);
	spin_unlock(&efivars_lock);

	return 0;
}
/*
 * For now we register the efi subsystem with the firmware subsystem
 * and the vars subsystem with the efi subsystem.  In the future, it
 * might make sense to split off the efi subsystem into its own
 * driver, but for now only efivars will register with it, so just
 * include it here.
 */

static int __init
efivars_init(void)
{
	efi_status_t status = EFI_NOT_FOUND;
	efi_guid_t vendor_guid;
	efi_char16_t *variable_name;
	unsigned long variable_name_size = 1024;
	int error = 0;

	if (!efi_enabled)
		return -ENODEV;

	variable_name = kzalloc(variable_name_size, GFP_KERNEL);
	if (!variable_name) {
		printk(KERN_ERR "efivars: Memory allocation failed.\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "EFI Variables Facility v%s %s\n", EFIVARS_VERSION,
	       EFIVARS_DATE);

	/* For now we'll register the efi directory at /sys/firmware/efi */
	efi_kobj = kobject_create_and_add("efi", firmware_kobj);
	if (!efi_kobj) {
		printk(KERN_ERR "efivars: Firmware registration failed.\n");
		error = -ENOMEM;
		goto out_free;
	}

	vars_kset = kset_create_and_add("vars", NULL, efi_kobj);
	if (!vars_kset) {
		printk(KERN_ERR "efivars: Subsystem registration failed.\n");
		error = -ENOMEM;
		goto out_firmware_unregister;
	}

	/*
	 * Per EFI spec, the maximum storage allocated for both
	 * the variable name and variable data is 1024 bytes.
	 */

	do {
		variable_name_size = 1024;

		status = efi.get_next_variable(&variable_name_size,
						variable_name,
						&vendor_guid);
		switch (status) {
		case EFI_SUCCESS:
			efivar_create_sysfs_entry(variable_name_size,
							variable_name,
							&vendor_guid);
			break;
		case EFI_NOT_FOUND:
			break;
		default:
			printk(KERN_WARNING "efivars: get_next_variable: status=%lx\n",
				status);
			status = EFI_NOT_FOUND;
			break;
		}
	} while (status != EFI_NOT_FOUND);

	/*
	 * Now add attributes to allow creation of new vars
	 * and deletion of existing ones...
	 */
	error = sysfs_create_bin_file(&vars_kset->kobj,
				      &var_subsys_attr_new_var);
	if (error)
		printk(KERN_ERR "efivars: unable to create new_var sysfs file"
			" due to error %d\n", error);
	error = sysfs_create_bin_file(&vars_kset->kobj,
				      &var_subsys_attr_del_var);
	if (error)
		printk(KERN_ERR "efivars: unable to create del_var sysfs file"
			" due to error %d\n", error);

	/* Don't forget the systab entry */
	error = sysfs_create_group(efi_kobj, &efi_subsys_attr_group);
	if (error)
		printk(KERN_ERR "efivars: Sysfs attribute export failed with error %d.\n", error);
	else
		goto out_free;

	kset_unregister(vars_kset);

out_firmware_unregister:
	kobject_put(efi_kobj);

out_free:
	kfree(variable_name);

	return error;
}

static void __exit
efivars_exit(void)
{
	struct efivar_entry *entry, *n;

	list_for_each_entry_safe(entry, n, &efivar_list, list) {
		spin_lock(&efivars_lock);
		list_del(&entry->list);
		spin_unlock(&efivars_lock);
		efivar_unregister(entry);
	}

	kset_unregister(vars_kset);
	kobject_put(efi_kobj);
}

module_init(efivars_init);
module_exit(efivars_exit);
