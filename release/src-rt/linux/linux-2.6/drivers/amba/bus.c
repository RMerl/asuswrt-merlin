/*
 *  linux/arch/arm/common/amba.c
 *
 *  Copyright (C) 2003 Deep Blue Solutions Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/amba/bus.h>

#include <asm/irq.h>
#include <asm/sizes.h>

#define to_amba_driver(d)	container_of(d, struct amba_driver, drv)

static const struct amba_id *
amba_lookup(const struct amba_id *table, struct amba_device *dev)
{
	int ret = 0;

	while (table->mask) {
		ret = (dev->periphid & table->mask) == table->id;
		if (ret)
			break;
		table++;
	}

	return ret ? table : NULL;
}

static int amba_match(struct device *dev, struct device_driver *drv)
{
	struct amba_device *pcdev = to_amba_device(dev);
	struct amba_driver *pcdrv = to_amba_driver(drv);

	return amba_lookup(pcdrv->id_table, pcdev) != NULL;
}

#ifdef CONFIG_HOTPLUG
static int amba_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct amba_device *pcdev = to_amba_device(dev);
	int retval = 0;

	retval = add_uevent_var(env, "AMBA_ID=%08x", pcdev->periphid);
	return retval;
}
#else
#define amba_uevent NULL
#endif

#define amba_attr_func(name,fmt,arg...)					\
static ssize_t name##_show(struct device *_dev,				\
			   struct device_attribute *attr, char *buf)	\
{									\
	struct amba_device *dev = to_amba_device(_dev);			\
	return sprintf(buf, fmt, arg);					\
}

#define amba_attr(name,fmt,arg...)	\
amba_attr_func(name,fmt,arg)		\
static DEVICE_ATTR(name, S_IRUGO, name##_show, NULL)

amba_attr_func(id, "%08x\n", dev->periphid);
amba_attr(irq0, "%u\n", dev->irq[0]);
amba_attr(irq1, "%u\n", dev->irq[1]);
amba_attr_func(resource, "\t%016llx\t%016llx\t%016lx\n",
	 (unsigned long long)dev->res.start, (unsigned long long)dev->res.end,
	 dev->res.flags);

static struct device_attribute amba_dev_attrs[] = {
	__ATTR_RO(id),
	__ATTR_RO(resource),
	__ATTR_NULL,
};

#ifdef CONFIG_PM_SLEEP

static int amba_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	struct amba_driver *adrv = to_amba_driver(dev->driver);
	struct amba_device *adev = to_amba_device(dev);
	int ret = 0;

	if (dev->driver && adrv->suspend)
		ret = adrv->suspend(adev, mesg);

	return ret;
}

static int amba_legacy_resume(struct device *dev)
{
	struct amba_driver *adrv = to_amba_driver(dev->driver);
	struct amba_device *adev = to_amba_device(dev);
	int ret = 0;

	if (dev->driver && adrv->resume)
		ret = adrv->resume(adev);

	return ret;
}

static int amba_pm_prepare(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (drv && drv->pm && drv->pm->prepare)
		ret = drv->pm->prepare(dev);

	return ret;
}

static void amba_pm_complete(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv && drv->pm && drv->pm->complete)
		drv->pm->complete(dev);
}

#else /* !CONFIG_PM_SLEEP */

#define amba_pm_prepare		NULL
#define amba_pm_complete		NULL

#endif /* !CONFIG_PM_SLEEP */

#ifdef CONFIG_SUSPEND

static int amba_pm_suspend(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->suspend)
			ret = drv->pm->suspend(dev);
	} else {
		ret = amba_legacy_suspend(dev, PMSG_SUSPEND);
	}

	return ret;
}

static int amba_pm_suspend_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->suspend_noirq)
			ret = drv->pm->suspend_noirq(dev);
	}

	return ret;
}

static int amba_pm_resume(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->resume)
			ret = drv->pm->resume(dev);
	} else {
		ret = amba_legacy_resume(dev);
	}

	return ret;
}

static int amba_pm_resume_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->resume_noirq)
			ret = drv->pm->resume_noirq(dev);
	}

	return ret;
}

#else /* !CONFIG_SUSPEND */

#define amba_pm_suspend		NULL
#define amba_pm_resume		NULL
#define amba_pm_suspend_noirq	NULL
#define amba_pm_resume_noirq	NULL

#endif /* !CONFIG_SUSPEND */

#ifdef CONFIG_HIBERNATE_CALLBACKS

static int amba_pm_freeze(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->freeze)
			ret = drv->pm->freeze(dev);
	} else {
		ret = amba_legacy_suspend(dev, PMSG_FREEZE);
	}

	return ret;
}

static int amba_pm_freeze_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->freeze_noirq)
			ret = drv->pm->freeze_noirq(dev);
	}

	return ret;
}

static int amba_pm_thaw(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->thaw)
			ret = drv->pm->thaw(dev);
	} else {
		ret = amba_legacy_resume(dev);
	}

	return ret;
}

static int amba_pm_thaw_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->thaw_noirq)
			ret = drv->pm->thaw_noirq(dev);
	}

	return ret;
}

static int amba_pm_poweroff(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->poweroff)
			ret = drv->pm->poweroff(dev);
	} else {
		ret = amba_legacy_suspend(dev, PMSG_HIBERNATE);
	}

	return ret;
}

static int amba_pm_poweroff_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->poweroff_noirq)
			ret = drv->pm->poweroff_noirq(dev);
	}

	return ret;
}

static int amba_pm_restore(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->restore)
			ret = drv->pm->restore(dev);
	} else {
		ret = amba_legacy_resume(dev);
	}

	return ret;
}

static int amba_pm_restore_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->restore_noirq)
			ret = drv->pm->restore_noirq(dev);
	}

	return ret;
}

#else /* !CONFIG_HIBERNATE_CALLBACKS */

#define amba_pm_freeze		NULL
#define amba_pm_thaw		NULL
#define amba_pm_poweroff		NULL
#define amba_pm_restore		NULL
#define amba_pm_freeze_noirq	NULL
#define amba_pm_thaw_noirq		NULL
#define amba_pm_poweroff_noirq	NULL
#define amba_pm_restore_noirq	NULL

#endif /* !CONFIG_HIBERNATE_CALLBACKS */

#ifdef CONFIG_PM

static const struct dev_pm_ops amba_pm = {
	.prepare	= amba_pm_prepare,
	.complete	= amba_pm_complete,
	.suspend	= amba_pm_suspend,
	.resume		= amba_pm_resume,
	.freeze		= amba_pm_freeze,
	.thaw		= amba_pm_thaw,
	.poweroff	= amba_pm_poweroff,
	.restore	= amba_pm_restore,
	.suspend_noirq	= amba_pm_suspend_noirq,
	.resume_noirq	= amba_pm_resume_noirq,
	.freeze_noirq	= amba_pm_freeze_noirq,
	.thaw_noirq	= amba_pm_thaw_noirq,
	.poweroff_noirq	= amba_pm_poweroff_noirq,
	.restore_noirq	= amba_pm_restore_noirq,
	SET_RUNTIME_PM_OPS(
		pm_generic_runtime_suspend,
		pm_generic_runtime_resume,
		pm_generic_runtime_idle
	)
};

#define AMBA_PM (&amba_pm)

#else /* !CONFIG_PM */

#define AMBA_PM	NULL

#endif /* !CONFIG_PM */

/*
 * Primecells are part of the Advanced Microcontroller Bus Architecture,
 * so we call the bus "amba".
 */
struct bus_type amba_bustype = {
	.name		= "amba",
	.dev_attrs	= amba_dev_attrs,
	.match		= amba_match,
	.uevent		= amba_uevent,
	.pm		= AMBA_PM,
};

static int __init amba_init(void)
{
	return bus_register(&amba_bustype);
}

postcore_initcall(amba_init);

static int amba_get_enable_pclk(struct amba_device *pcdev)
{
	struct clk *pclk = clk_get(&pcdev->dev, "apb_pclk");
	int ret;

	pcdev->pclk = pclk;

	if (IS_ERR(pclk))
		return PTR_ERR(pclk);

	ret = clk_enable(pclk);
	if (ret)
		clk_put(pclk);

	return ret;
}

static void amba_put_disable_pclk(struct amba_device *pcdev)
{
	struct clk *pclk = pcdev->pclk;

	clk_disable(pclk);
	clk_put(pclk);
}

static int amba_get_enable_vcore(struct amba_device *pcdev)
{
	struct regulator *vcore = regulator_get(&pcdev->dev, "vcore");
	int ret;

	pcdev->vcore = vcore;

	if (IS_ERR(vcore)) {
		/* It is OK not to supply a vcore regulator */
		if (PTR_ERR(vcore) == -ENODEV)
			return 0;
		return PTR_ERR(vcore);
	}

	ret = regulator_enable(vcore);
	if (ret) {
		regulator_put(vcore);
		pcdev->vcore = ERR_PTR(-ENODEV);
	}

	return ret;
}

static void amba_put_disable_vcore(struct amba_device *pcdev)
{
	struct regulator *vcore = pcdev->vcore;

	if (!IS_ERR(vcore)) {
		regulator_disable(vcore);
		regulator_put(vcore);
	}
}

/*
 * These are the device model conversion veneers; they convert the
 * device model structures to our more specific structures.
 */
static int amba_probe(struct device *dev)
{
	struct amba_device *pcdev = to_amba_device(dev);
	struct amba_driver *pcdrv = to_amba_driver(dev->driver);
	const struct amba_id *id = amba_lookup(pcdrv->id_table, pcdev);
	int ret;

	do {
		ret = amba_get_enable_vcore(pcdev);
		if (ret)
			break;

		ret = amba_get_enable_pclk(pcdev);
		if (ret)
			break;

		ret = pcdrv->probe(pcdev, id);
		if (ret == 0)
			break;

		amba_put_disable_pclk(pcdev);
		amba_put_disable_vcore(pcdev);
	} while (0);

	return ret;
}

static int amba_remove(struct device *dev)
{
	struct amba_device *pcdev = to_amba_device(dev);
	struct amba_driver *drv = to_amba_driver(dev->driver);
	int ret = drv->remove(pcdev);

	amba_put_disable_pclk(pcdev);
	amba_put_disable_vcore(pcdev);

	return ret;
}

static void amba_shutdown(struct device *dev)
{
	struct amba_driver *drv = to_amba_driver(dev->driver);
	drv->shutdown(to_amba_device(dev));
}

/**
 *	amba_driver_register - register an AMBA device driver
 *	@drv: amba device driver structure
 *
 *	Register an AMBA device driver with the Linux device model
 *	core.  If devices pre-exist, the drivers probe function will
 *	be called.
 */
int amba_driver_register(struct amba_driver *drv)
{
	drv->drv.bus = &amba_bustype;

#define SETFN(fn)	if (drv->fn) drv->drv.fn = amba_##fn
	SETFN(probe);
	SETFN(remove);
	SETFN(shutdown);

	return driver_register(&drv->drv);
}

/**
 *	amba_driver_unregister - remove an AMBA device driver
 *	@drv: AMBA device driver structure to remove
 *
 *	Unregister an AMBA device driver from the Linux device
 *	model.  The device model will call the drivers remove function
 *	for each device the device driver is currently handling.
 */
void amba_driver_unregister(struct amba_driver *drv)
{
	driver_unregister(&drv->drv);
}


static void amba_device_release(struct device *dev)
{
	struct amba_device *d = to_amba_device(dev);

	if (d->res.parent)
		release_resource(&d->res);
	kfree(d);
}

/**
 *	amba_device_register - register an AMBA device
 *	@dev: AMBA device to register
 *	@parent: parent memory resource
 *
 *	Setup the AMBA device, reading the cell ID if present.
 *	Claim the resource, and register the AMBA device with
 *	the Linux device manager.
 */
int amba_device_register(struct amba_device *dev, struct resource *parent)
{
	u32 size;
	void __iomem *tmp;
	int i, ret;

	device_initialize(&dev->dev);

	/*
	 * Copy from device_add
	 */
	if (dev->dev.init_name) {
		dev_set_name(&dev->dev, "%s", dev->dev.init_name);
		dev->dev.init_name = NULL;
	}

	dev->dev.release = amba_device_release;
	dev->dev.bus = &amba_bustype;
	dev->dev.dma_mask = &dev->dma_mask;
	dev->res.name = dev_name(&dev->dev);

	if (!dev->dev.coherent_dma_mask && dev->dma_mask)
		dev_warn(&dev->dev, "coherent dma mask is unset\n");

	ret = request_resource(parent, &dev->res);
	if (ret)
		goto err_out;

	/*
	 * Dynamically calculate the size of the resource
	 * and use this for iomap
	 */
	size = resource_size(&dev->res);
	tmp = ioremap(dev->res.start, size);
	if (!tmp) {
		ret = -ENOMEM;
		goto err_release;
	}

	ret = amba_get_enable_pclk(dev);
	if (ret == 0) {
		u32 pid, cid;

		/*
		 * Read pid and cid based on size of resource
		 * they are located at end of region
		 */
		for (pid = 0, i = 0; i < 4; i++)
			pid |= (readl(tmp + size - 0x20 + 4 * i) & 255) <<
				(i * 8);
		for (cid = 0, i = 0; i < 4; i++)
			cid |= (readl(tmp + size - 0x10 + 4 * i) & 255) <<
				(i * 8);

		amba_put_disable_pclk(dev);

		if (cid == AMBA_CID)
			dev->periphid = pid;

		if (!dev->periphid)
			ret = -ENODEV;
	}

	iounmap(tmp);

	if (ret)
		goto err_release;

	ret = device_add(&dev->dev);
	if (ret)
		goto err_release;

	if (dev->irq[0] != NO_IRQ)
		ret = device_create_file(&dev->dev, &dev_attr_irq0);
	if (ret == 0 && dev->irq[1] != NO_IRQ)
		ret = device_create_file(&dev->dev, &dev_attr_irq1);
	if (ret == 0)
		return ret;

	device_unregister(&dev->dev);

 err_release:
	release_resource(&dev->res);
 err_out:
	return ret;
}

/**
 *	amba_device_unregister - unregister an AMBA device
 *	@dev: AMBA device to remove
 *
 *	Remove the specified AMBA device from the Linux device
 *	manager.  All files associated with this object will be
 *	destroyed, and device drivers notified that the device has
 *	been removed.  The AMBA device's resources including
 *	the amba_device structure will be freed once all
 *	references to it have been dropped.
 */
void amba_device_unregister(struct amba_device *dev)
{
	device_unregister(&dev->dev);
}


struct find_data {
	struct amba_device *dev;
	struct device *parent;
	const char *busid;
	unsigned int id;
	unsigned int mask;
};

static int amba_find_match(struct device *dev, void *data)
{
	struct find_data *d = data;
	struct amba_device *pcdev = to_amba_device(dev);
	int r;

	r = (pcdev->periphid & d->mask) == d->id;
	if (d->parent)
		r &= d->parent == dev->parent;
	if (d->busid)
		r &= strcmp(dev_name(dev), d->busid) == 0;

	if (r) {
		get_device(dev);
		d->dev = pcdev;
	}

	return r;
}

/**
 *	amba_find_device - locate an AMBA device given a bus id
 *	@busid: bus id for device (or NULL)
 *	@parent: parent device (or NULL)
 *	@id: peripheral ID (or 0)
 *	@mask: peripheral ID mask (or 0)
 *
 *	Return the AMBA device corresponding to the supplied parameters.
 *	If no device matches, returns NULL.
 *
 *	NOTE: When a valid device is found, its refcount is
 *	incremented, and must be decremented before the returned
 *	reference.
 */
struct amba_device *
amba_find_device(const char *busid, struct device *parent, unsigned int id,
		 unsigned int mask)
{
	struct find_data data;

	data.dev = NULL;
	data.parent = parent;
	data.busid = busid;
	data.id = id;
	data.mask = mask;

	bus_for_each_dev(&amba_bustype, NULL, &data, amba_find_match);

	return data.dev;
}

/**
 *	amba_request_regions - request all mem regions associated with device
 *	@dev: amba_device structure for device
 *	@name: name, or NULL to use driver name
 */
int amba_request_regions(struct amba_device *dev, const char *name)
{
	int ret = 0;
	u32 size;

	if (!name)
		name = dev->dev.driver->name;

	size = resource_size(&dev->res);

	if (!request_mem_region(dev->res.start, size, name))
		ret = -EBUSY;

	return ret;
}

/**
 *	amba_release_regions - release mem regions associated with device
 *	@dev: amba_device structure for device
 *
 *	Release regions claimed by a successful call to amba_request_regions.
 */
void amba_release_regions(struct amba_device *dev)
{
	u32 size;

	size = resource_size(&dev->res);
	release_mem_region(dev->res.start, size);
}

EXPORT_SYMBOL(amba_driver_register);
EXPORT_SYMBOL(amba_driver_unregister);
EXPORT_SYMBOL(amba_device_register);
EXPORT_SYMBOL(amba_device_unregister);
EXPORT_SYMBOL(amba_find_device);
EXPORT_SYMBOL(amba_request_regions);
EXPORT_SYMBOL(amba_release_regions);
