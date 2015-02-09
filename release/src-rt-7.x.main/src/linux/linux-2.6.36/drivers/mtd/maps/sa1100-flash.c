/*
 * Flash memory access on SA11x0 based devices
 *
 * (C) 2000 Nicolas Pitre <nico@fluxnic.net>
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>

#include <mach/hardware.h>
#include <asm/sizes.h>
#include <asm/mach/flash.h>


struct sa_subdev_info {
	char name[16];
	struct map_info map;
	struct mtd_info *mtd;
	struct flash_platform_data *plat;
};

struct sa_info {
	struct mtd_partition	*parts;
	struct mtd_info		*mtd;
	int			num_subdev;
	unsigned int		nr_parts;
	struct sa_subdev_info	subdev[0];
};

static void sa1100_set_vpp(struct map_info *map, int on)
{
	struct sa_subdev_info *subdev = container_of(map, struct sa_subdev_info, map);
	subdev->plat->set_vpp(on);
}

static void sa1100_destroy_subdev(struct sa_subdev_info *subdev)
{
	if (subdev->mtd)
		map_destroy(subdev->mtd);
	if (subdev->map.virt)
		iounmap(subdev->map.virt);
	release_mem_region(subdev->map.phys, subdev->map.size);
}

static int sa1100_probe_subdev(struct sa_subdev_info *subdev, struct resource *res)
{
	unsigned long phys;
	unsigned int size;
	int ret;

	phys = res->start;
	size = res->end - phys + 1;

	/*
	 * Retrieve the bankwidth from the MSC registers.
	 * We currently only implement CS0 and CS1 here.
	 */
	switch (phys) {
	default:
		printk(KERN_WARNING "SA1100 flash: unknown base address "
		       "0x%08lx, assuming CS0\n", phys);

	case SA1100_CS0_PHYS:
		subdev->map.bankwidth = (MSC0 & MSC_RBW) ? 2 : 4;
		break;

	case SA1100_CS1_PHYS:
		subdev->map.bankwidth = ((MSC0 >> 16) & MSC_RBW) ? 2 : 4;
		break;
	}

	if (!request_mem_region(phys, size, subdev->name)) {
		ret = -EBUSY;
		goto out;
	}

	if (subdev->plat->set_vpp)
		subdev->map.set_vpp = sa1100_set_vpp;

	subdev->map.phys = phys;
	subdev->map.size = size;
	subdev->map.virt = ioremap(phys, size);
	if (!subdev->map.virt) {
		ret = -ENOMEM;
		goto err;
	}

	simple_map_init(&subdev->map);

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	subdev->mtd = do_map_probe(subdev->plat->map_name, &subdev->map);
	if (subdev->mtd == NULL) {
		ret = -ENXIO;
		goto err;
	}
	subdev->mtd->owner = THIS_MODULE;

	printk(KERN_INFO "SA1100 flash: CFI device at 0x%08lx, %uMiB, %d-bit\n",
		phys, (unsigned)(subdev->mtd->size >> 20),
		subdev->map.bankwidth * 8);

	return 0;

 err:
	sa1100_destroy_subdev(subdev);
 out:
	return ret;
}

static void sa1100_destroy(struct sa_info *info, struct flash_platform_data *plat)
{
	int i;

	if (info->mtd) {
		if (info->nr_parts == 0)
			del_mtd_device(info->mtd);
#ifdef CONFIG_MTD_PARTITIONS
		else
			del_mtd_partitions(info->mtd);
#endif
#ifdef CONFIG_MTD_CONCAT
		if (info->mtd != info->subdev[0].mtd)
			mtd_concat_destroy(info->mtd);
#endif
	}

	kfree(info->parts);

	for (i = info->num_subdev - 1; i >= 0; i--)
		sa1100_destroy_subdev(&info->subdev[i]);
	kfree(info);

	if (plat->exit)
		plat->exit();
}

static struct sa_info *__devinit
sa1100_setup_mtd(struct platform_device *pdev, struct flash_platform_data *plat)
{
	struct sa_info *info;
	int nr, size, i, ret = 0;

	/*
	 * Count number of devices.
	 */
	for (nr = 0; ; nr++)
		if (!platform_get_resource(pdev, IORESOURCE_MEM, nr))
			break;

	if (nr == 0) {
		ret = -ENODEV;
		goto out;
	}

	size = sizeof(struct sa_info) + sizeof(struct sa_subdev_info) * nr;

	/*
	 * Allocate the map_info structs in one go.
	 */
	info = kzalloc(size, GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
		goto out;
	}

	if (plat->init) {
		ret = plat->init();
		if (ret)
			goto err;
	}

	/*
	 * Claim and then map the memory regions.
	 */
	for (i = 0; i < nr; i++) {
		struct sa_subdev_info *subdev = &info->subdev[i];
		struct resource *res;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			break;

		subdev->map.name = subdev->name;
		sprintf(subdev->name, "%s-%d", plat->name, i);
		subdev->plat = plat;

		ret = sa1100_probe_subdev(subdev, res);
		if (ret)
			break;
	}

	info->num_subdev = i;

	/*
	 * ENXIO is special.  It means we didn't find a chip when we probed.
	 */
	if (ret != 0 && !(ret == -ENXIO && info->num_subdev > 0))
		goto err;

	/*
	 * If we found one device, don't bother with concat support.  If
	 * we found multiple devices, use concat if we have it available,
	 * otherwise fail.  Either way, it'll be called "sa1100".
	 */
	if (info->num_subdev == 1) {
		strcpy(info->subdev[0].name, plat->name);
		info->mtd = info->subdev[0].mtd;
		ret = 0;
	} else if (info->num_subdev > 1) {
#ifdef CONFIG_MTD_CONCAT
		struct mtd_info *cdev[nr];
		/*
		 * We detected multiple devices.  Concatenate them together.
		 */
		for (i = 0; i < info->num_subdev; i++)
			cdev[i] = info->subdev[i].mtd;

		info->mtd = mtd_concat_create(cdev, info->num_subdev,
					      plat->name);
		if (info->mtd == NULL)
			ret = -ENXIO;
#else
		printk(KERN_ERR "SA1100 flash: multiple devices "
		       "found but MTD concat support disabled.\n");
		ret = -ENXIO;
#endif
	}

	if (ret == 0)
		return info;

 err:
	sa1100_destroy(info, plat);
 out:
	return ERR_PTR(ret);
}

static const char *part_probes[] = { "cmdlinepart", "RedBoot", NULL };

static int __devinit sa1100_mtd_probe(struct platform_device *pdev)
{
	struct flash_platform_data *plat = pdev->dev.platform_data;
	struct mtd_partition *parts;
	const char *part_type = NULL;
	struct sa_info *info;
	int err, nr_parts = 0;

	if (!plat)
		return -ENODEV;

	info = sa1100_setup_mtd(pdev, plat);
	if (IS_ERR(info)) {
		err = PTR_ERR(info);
		goto out;
	}

	/*
	 * Partition selection stuff.
	 */
#ifdef CONFIG_MTD_PARTITIONS
	nr_parts = parse_mtd_partitions(info->mtd, part_probes, &parts, 0);
	if (nr_parts > 0) {
		info->parts = parts;
		part_type = "dynamic";
	} else
#endif
	{
		parts = plat->parts;
		nr_parts = plat->nr_parts;
		part_type = "static";
	}

	if (nr_parts == 0) {
		printk(KERN_NOTICE "SA1100 flash: no partition info "
			"available, registering whole flash\n");
		add_mtd_device(info->mtd);
	} else {
		printk(KERN_NOTICE "SA1100 flash: using %s partition "
			"definition\n", part_type);
		add_mtd_partitions(info->mtd, parts, nr_parts);
	}

	info->nr_parts = nr_parts;

	platform_set_drvdata(pdev, info);
	err = 0;

 out:
	return err;
}

static int __exit sa1100_mtd_remove(struct platform_device *pdev)
{
	struct sa_info *info = platform_get_drvdata(pdev);
	struct flash_platform_data *plat = pdev->dev.platform_data;

	platform_set_drvdata(pdev, NULL);
	sa1100_destroy(info, plat);

	return 0;
}

#ifdef CONFIG_PM
static void sa1100_mtd_shutdown(struct platform_device *dev)
{
	struct sa_info *info = platform_get_drvdata(dev);
	if (info && info->mtd->suspend(info->mtd) == 0)
		info->mtd->resume(info->mtd);
}
#else
#define sa1100_mtd_shutdown NULL
#endif

static struct platform_driver sa1100_mtd_driver = {
	.probe		= sa1100_mtd_probe,
	.remove		= __exit_p(sa1100_mtd_remove),
	.shutdown	= sa1100_mtd_shutdown,
	.driver		= {
		.name	= "sa1100-mtd",
		.owner	= THIS_MODULE,
	},
};

static int __init sa1100_mtd_init(void)
{
	return platform_driver_register(&sa1100_mtd_driver);
}

static void __exit sa1100_mtd_exit(void)
{
	platform_driver_unregister(&sa1100_mtd_driver);
}

module_init(sa1100_mtd_init);
module_exit(sa1100_mtd_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("SA1100 CFI map driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sa1100-mtd");
