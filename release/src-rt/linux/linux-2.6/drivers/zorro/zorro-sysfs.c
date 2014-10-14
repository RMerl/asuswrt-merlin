/*
 *  File Attributes for Zorro Devices
 *
 *  Copyright (C) 2003 Geert Uytterhoeven
 *
 *  Loosely based on drivers/pci/pci-sysfs.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */


#include <linux/kernel.h>
#include <linux/zorro.h>
#include <linux/stat.h>
#include <linux/string.h>

#include "zorro.h"


/* show configuration fields */
#define zorro_config_attr(name, field, format_string)			\
static ssize_t								\
show_##name(struct device *dev, struct device_attribute *attr, char *buf)				\
{									\
	struct zorro_dev *z;						\
									\
	z = to_zorro_dev(dev);						\
	return sprintf(buf, format_string, z->field);			\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_##name, NULL);

zorro_config_attr(id, id, "0x%08x\n");
zorro_config_attr(type, rom.er_Type, "0x%02x\n");
zorro_config_attr(serial, rom.er_SerialNumber, "0x%08x\n");
zorro_config_attr(slotaddr, slotaddr, "0x%04x\n");
zorro_config_attr(slotsize, slotsize, "0x%04x\n");

static ssize_t zorro_show_resource(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zorro_dev *z = to_zorro_dev(dev);

	return sprintf(buf, "0x%08lx 0x%08lx 0x%08lx\n",
		       (unsigned long)zorro_resource_start(z),
		       (unsigned long)zorro_resource_end(z),
		       zorro_resource_flags(z));
}

static DEVICE_ATTR(resource, S_IRUGO, zorro_show_resource, NULL);

static ssize_t zorro_read_config(struct file *filp, struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buf, loff_t off, size_t count)
{
	struct zorro_dev *z = to_zorro_dev(container_of(kobj, struct device,
					   kobj));
	struct ConfigDev cd;

	/* Construct a ConfigDev */
	memset(&cd, 0, sizeof(cd));
	cd.cd_Rom = z->rom;
	cd.cd_SlotAddr = z->slotaddr;
	cd.cd_SlotSize = z->slotsize;
	cd.cd_BoardAddr = (void *)zorro_resource_start(z);
	cd.cd_BoardSize = zorro_resource_len(z);

	return memory_read_from_buffer(buf, count, &off, &cd, sizeof(cd));
}

static struct bin_attribute zorro_config_attr = {
	.attr =	{
		.name = "config",
		.mode = S_IRUGO,
	},
	.size = sizeof(struct ConfigDev),
	.read = zorro_read_config,
};

static ssize_t modalias_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct zorro_dev *z = to_zorro_dev(dev);

	return sprintf(buf, ZORRO_DEVICE_MODALIAS_FMT "\n", z->id);
}

static DEVICE_ATTR(modalias, S_IRUGO, modalias_show, NULL);

int zorro_create_sysfs_dev_files(struct zorro_dev *z)
{
	struct device *dev = &z->dev;
	int error;

	/* current configuration's attributes */
	if ((error = device_create_file(dev, &dev_attr_id)) ||
	    (error = device_create_file(dev, &dev_attr_type)) ||
	    (error = device_create_file(dev, &dev_attr_serial)) ||
	    (error = device_create_file(dev, &dev_attr_slotaddr)) ||
	    (error = device_create_file(dev, &dev_attr_slotsize)) ||
	    (error = device_create_file(dev, &dev_attr_resource)) ||
	    (error = device_create_file(dev, &dev_attr_modalias)) ||
	    (error = sysfs_create_bin_file(&dev->kobj, &zorro_config_attr)))
		return error;

	return 0;
}

