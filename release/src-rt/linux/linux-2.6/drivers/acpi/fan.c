/*
 *  acpi_fan.c - ACPI Fan Driver ($Revision: 29 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/thermal.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>

#define PREFIX "ACPI: "

#define ACPI_FAN_CLASS			"fan"
#define ACPI_FAN_FILE_STATE		"state"

#define _COMPONENT		ACPI_FAN_COMPONENT
ACPI_MODULE_NAME("fan");

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION("ACPI Fan Driver");
MODULE_LICENSE("GPL");

static int acpi_fan_add(struct acpi_device *device);
static int acpi_fan_remove(struct acpi_device *device, int type);
static int acpi_fan_suspend(struct acpi_device *device, pm_message_t state);
static int acpi_fan_resume(struct acpi_device *device);

static const struct acpi_device_id fan_device_ids[] = {
	{"PNP0C0B", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, fan_device_ids);

static struct acpi_driver acpi_fan_driver = {
	.name = "fan",
	.class = ACPI_FAN_CLASS,
	.ids = fan_device_ids,
	.ops = {
		.add = acpi_fan_add,
		.remove = acpi_fan_remove,
		.suspend = acpi_fan_suspend,
		.resume = acpi_fan_resume,
		},
};

/* thermal cooling device callbacks */
static int fan_get_max_state(struct thermal_cooling_device *cdev, unsigned long
			     *state)
{
	/* ACPI fan device only support two states: ON/OFF */
	*state = 1;
	return 0;
}

static int fan_get_cur_state(struct thermal_cooling_device *cdev, unsigned long
			     *state)
{
	struct acpi_device *device = cdev->devdata;
	int result;
	int acpi_state;

	if (!device)
		return -EINVAL;

	result = acpi_bus_update_power(device->handle, &acpi_state);
	if (result)
		return result;

	*state = (acpi_state == ACPI_STATE_D3 ? 0 :
		 (acpi_state == ACPI_STATE_D0 ? 1 : -1));
	return 0;
}

static int
fan_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct acpi_device *device = cdev->devdata;
	int result;

	if (!device || (state != 0 && state != 1))
		return -EINVAL;

	result = acpi_bus_set_power(device->handle,
				state ? ACPI_STATE_D0 : ACPI_STATE_D3);

	return result;
}

static struct thermal_cooling_device_ops fan_cooling_ops = {
	.get_max_state = fan_get_max_state,
	.get_cur_state = fan_get_cur_state,
	.set_cur_state = fan_set_cur_state,
};

/* --------------------------------------------------------------------------
                                 Driver Interface
   -------------------------------------------------------------------------- */

static int acpi_fan_add(struct acpi_device *device)
{
	int result = 0;
	struct thermal_cooling_device *cdev;

	if (!device)
		return -EINVAL;

	strcpy(acpi_device_name(device), "Fan");
	strcpy(acpi_device_class(device), ACPI_FAN_CLASS);

	result = acpi_bus_update_power(device->handle, NULL);
	if (result) {
		printk(KERN_ERR PREFIX "Setting initial power state\n");
		goto end;
	}

	cdev = thermal_cooling_device_register("Fan", device,
						&fan_cooling_ops);
	if (IS_ERR(cdev)) {
		result = PTR_ERR(cdev);
		goto end;
	}

	dev_dbg(&device->dev, "registered as cooling_device%d\n", cdev->id);

	device->driver_data = cdev;
	result = sysfs_create_link(&device->dev.kobj,
				   &cdev->device.kobj,
				   "thermal_cooling");
	if (result)
		dev_err(&device->dev, "Failed to create sysfs link "
			"'thermal_cooling'\n");

	result = sysfs_create_link(&cdev->device.kobj,
				   &device->dev.kobj,
				   "device");
	if (result)
		dev_err(&device->dev, "Failed to create sysfs link "
			"'device'\n");

	printk(KERN_INFO PREFIX "%s [%s] (%s)\n",
	       acpi_device_name(device), acpi_device_bid(device),
	       !device->power.state ? "on" : "off");

      end:
	return result;
}

static int acpi_fan_remove(struct acpi_device *device, int type)
{
	struct thermal_cooling_device *cdev = acpi_driver_data(device);

	if (!device || !cdev)
		return -EINVAL;

	sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
	sysfs_remove_link(&cdev->device.kobj, "device");
	thermal_cooling_device_unregister(cdev);

	return 0;
}

static int acpi_fan_suspend(struct acpi_device *device, pm_message_t state)
{
	if (!device)
		return -EINVAL;

	acpi_bus_set_power(device->handle, ACPI_STATE_D0);

	return AE_OK;
}

static int acpi_fan_resume(struct acpi_device *device)
{
	int result;

	if (!device)
		return -EINVAL;

	result = acpi_bus_update_power(device->handle, NULL);
	if (result)
		printk(KERN_ERR PREFIX "Error updating fan power state\n");

	return result;
}

static int __init acpi_fan_init(void)
{
	int result = 0;

	result = acpi_bus_register_driver(&acpi_fan_driver);
	if (result < 0)
		return -ENODEV;

	return 0;
}

static void __exit acpi_fan_exit(void)
{

	acpi_bus_unregister_driver(&acpi_fan_driver);

	return;
}

module_init(acpi_fan_init);
module_exit(acpi_fan_exit);
