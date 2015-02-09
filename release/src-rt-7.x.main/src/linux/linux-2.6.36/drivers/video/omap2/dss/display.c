/*
 * linux/drivers/video/omap2/dss/display.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "DISPLAY"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <plat/display.h>
#include "dss.h"

static LIST_HEAD(display_list);

static ssize_t display_enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	bool enabled = dssdev->state != OMAP_DSS_DISPLAY_DISABLED;

	return snprintf(buf, PAGE_SIZE, "%d\n", enabled);
}

static ssize_t display_enabled_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	bool enabled, r;

	enabled = simple_strtoul(buf, NULL, 10);

	if (enabled != (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)) {
		if (enabled) {
			r = dssdev->driver->enable(dssdev);
			if (r)
				return r;
		} else {
			dssdev->driver->disable(dssdev);
		}
	}

	return size;
}

static ssize_t display_upd_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	enum omap_dss_update_mode mode = OMAP_DSS_UPDATE_AUTO;
	if (dssdev->driver->get_update_mode)
		mode = dssdev->driver->get_update_mode(dssdev);
	return snprintf(buf, PAGE_SIZE, "%d\n", mode);
}

static ssize_t display_upd_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	int val, r;
	enum omap_dss_update_mode mode;

	if (!dssdev->driver->set_update_mode)
		return -EINVAL;

	val = simple_strtoul(buf, NULL, 10);

	switch (val) {
	case OMAP_DSS_UPDATE_DISABLED:
	case OMAP_DSS_UPDATE_AUTO:
	case OMAP_DSS_UPDATE_MANUAL:
		mode = (enum omap_dss_update_mode)val;
		break;
	default:
		return -EINVAL;
	}

	r = dssdev->driver->set_update_mode(dssdev, mode);
	if (r)
		return r;

	return size;
}

static ssize_t display_tear_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",
			dssdev->driver->get_te ?
			dssdev->driver->get_te(dssdev) : 0);
}

static ssize_t display_tear_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	unsigned long te;
	int r;

	if (!dssdev->driver->enable_te || !dssdev->driver->get_te)
		return -ENOENT;

	te = simple_strtoul(buf, NULL, 0);

	r = dssdev->driver->enable_te(dssdev, te);
	if (r)
		return r;

	return size;
}

static ssize_t display_timings_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	struct omap_video_timings t;

	if (!dssdev->driver->get_timings)
		return -ENOENT;

	dssdev->driver->get_timings(dssdev, &t);

	return snprintf(buf, PAGE_SIZE, "%u,%u/%u/%u/%u,%u/%u/%u/%u\n",
			t.pixel_clock,
			t.x_res, t.hfp, t.hbp, t.hsw,
			t.y_res, t.vfp, t.vbp, t.vsw);
}

static ssize_t display_timings_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	struct omap_video_timings t;
	int r, found;

	if (!dssdev->driver->set_timings || !dssdev->driver->check_timings)
		return -ENOENT;

	found = 0;
#ifdef CONFIG_OMAP2_DSS_VENC
	if (strncmp("pal", buf, 3) == 0) {
		t = omap_dss_pal_timings;
		found = 1;
	} else if (strncmp("ntsc", buf, 4) == 0) {
		t = omap_dss_ntsc_timings;
		found = 1;
	}
#endif
	if (!found && sscanf(buf, "%u,%hu/%hu/%hu/%hu,%hu/%hu/%hu/%hu",
				&t.pixel_clock,
				&t.x_res, &t.hfp, &t.hbp, &t.hsw,
				&t.y_res, &t.vfp, &t.vbp, &t.vsw) != 9)
		return -EINVAL;

	r = dssdev->driver->check_timings(dssdev, &t);
	if (r)
		return r;

	dssdev->driver->set_timings(dssdev, &t);

	return size;
}

static ssize_t display_rotate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	int rotate;
	if (!dssdev->driver->get_rotate)
		return -ENOENT;
	rotate = dssdev->driver->get_rotate(dssdev);
	return snprintf(buf, PAGE_SIZE, "%u\n", rotate);
}

static ssize_t display_rotate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	unsigned long rot;
	int r;

	if (!dssdev->driver->set_rotate || !dssdev->driver->get_rotate)
		return -ENOENT;

	rot = simple_strtoul(buf, NULL, 0);

	r = dssdev->driver->set_rotate(dssdev, rot);
	if (r)
		return r;

	return size;
}

static ssize_t display_mirror_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	int mirror;
	if (!dssdev->driver->get_mirror)
		return -ENOENT;
	mirror = dssdev->driver->get_mirror(dssdev);
	return snprintf(buf, PAGE_SIZE, "%u\n", mirror);
}

static ssize_t display_mirror_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	unsigned long mirror;
	int r;

	if (!dssdev->driver->set_mirror || !dssdev->driver->get_mirror)
		return -ENOENT;

	mirror = simple_strtoul(buf, NULL, 0);

	r = dssdev->driver->set_mirror(dssdev, mirror);
	if (r)
		return r;

	return size;
}

static ssize_t display_wss_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	unsigned int wss;

	if (!dssdev->driver->get_wss)
		return -ENOENT;

	wss = dssdev->driver->get_wss(dssdev);

	return snprintf(buf, PAGE_SIZE, "0x%05x\n", wss);
}

static ssize_t display_wss_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	unsigned long wss;
	int r;

	if (!dssdev->driver->get_wss || !dssdev->driver->set_wss)
		return -ENOENT;

	if (strict_strtoul(buf, 0, &wss))
		return -EINVAL;

	if (wss > 0xfffff)
		return -EINVAL;

	r = dssdev->driver->set_wss(dssdev, wss);
	if (r)
		return r;

	return size;
}

static DEVICE_ATTR(enabled, S_IRUGO|S_IWUSR,
		display_enabled_show, display_enabled_store);
static DEVICE_ATTR(update_mode, S_IRUGO|S_IWUSR,
		display_upd_mode_show, display_upd_mode_store);
static DEVICE_ATTR(tear_elim, S_IRUGO|S_IWUSR,
		display_tear_show, display_tear_store);
static DEVICE_ATTR(timings, S_IRUGO|S_IWUSR,
		display_timings_show, display_timings_store);
static DEVICE_ATTR(rotate, S_IRUGO|S_IWUSR,
		display_rotate_show, display_rotate_store);
static DEVICE_ATTR(mirror, S_IRUGO|S_IWUSR,
		display_mirror_show, display_mirror_store);
static DEVICE_ATTR(wss, S_IRUGO|S_IWUSR,
		display_wss_show, display_wss_store);

static struct device_attribute *display_sysfs_attrs[] = {
	&dev_attr_enabled,
	&dev_attr_update_mode,
	&dev_attr_tear_elim,
	&dev_attr_timings,
	&dev_attr_rotate,
	&dev_attr_mirror,
	&dev_attr_wss,
	NULL
};

void omapdss_default_get_resolution(struct omap_dss_device *dssdev,
			u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}
EXPORT_SYMBOL(omapdss_default_get_resolution);

void default_get_overlay_fifo_thresholds(enum omap_plane plane,
		u32 fifo_size, enum omap_burst_size *burst_size,
		u32 *fifo_low, u32 *fifo_high)
{
	unsigned burst_size_bytes;

	*burst_size = OMAP_DSS_BURST_16x32;
	burst_size_bytes = 16 * 32 / 8;

	*fifo_high = fifo_size - 1;
	*fifo_low = fifo_size - burst_size_bytes;
}

int omapdss_default_get_recommended_bpp(struct omap_dss_device *dssdev)
{
	switch (dssdev->type) {
	case OMAP_DISPLAY_TYPE_DPI:
		if (dssdev->phy.dpi.data_lines == 24)
			return 24;
		else
			return 16;

	case OMAP_DISPLAY_TYPE_DBI:
	case OMAP_DISPLAY_TYPE_DSI:
		if (dssdev->ctrl.pixel_size == 24)
			return 24;
		else
			return 16;
	case OMAP_DISPLAY_TYPE_VENC:
	case OMAP_DISPLAY_TYPE_SDI:
		return 24;
	default:
		BUG();
	}
}
EXPORT_SYMBOL(omapdss_default_get_recommended_bpp);

/* Checks if replication logic should be used. Only use for active matrix,
 * when overlay is in RGB12U or RGB16 mode, and LCD interface is
 * 18bpp or 24bpp */
bool dss_use_replication(struct omap_dss_device *dssdev,
		enum omap_color_mode mode)
{
	int bpp;

	if (mode != OMAP_DSS_COLOR_RGB12U && mode != OMAP_DSS_COLOR_RGB16)
		return false;

	if (dssdev->type == OMAP_DISPLAY_TYPE_DPI &&
			(dssdev->panel.config & OMAP_DSS_LCD_TFT) == 0)
		return false;

	switch (dssdev->type) {
	case OMAP_DISPLAY_TYPE_DPI:
		bpp = dssdev->phy.dpi.data_lines;
		break;
	case OMAP_DISPLAY_TYPE_VENC:
	case OMAP_DISPLAY_TYPE_SDI:
		bpp = 24;
		break;
	case OMAP_DISPLAY_TYPE_DBI:
	case OMAP_DISPLAY_TYPE_DSI:
		bpp = dssdev->ctrl.pixel_size;
		break;
	default:
		BUG();
	}

	return bpp > 16;
}

void dss_init_device(struct platform_device *pdev,
		struct omap_dss_device *dssdev)
{
	struct device_attribute *attr;
	int i;
	int r;

	switch (dssdev->type) {
#ifdef CONFIG_OMAP2_DSS_DPI
	case OMAP_DISPLAY_TYPE_DPI:
#endif
#ifdef CONFIG_OMAP2_DSS_RFBI
	case OMAP_DISPLAY_TYPE_DBI:
#endif
#ifdef CONFIG_OMAP2_DSS_SDI
	case OMAP_DISPLAY_TYPE_SDI:
#endif
#ifdef CONFIG_OMAP2_DSS_DSI
	case OMAP_DISPLAY_TYPE_DSI:
#endif
#ifdef CONFIG_OMAP2_DSS_VENC
	case OMAP_DISPLAY_TYPE_VENC:
#endif
		break;
	default:
		DSSERR("Support for display '%s' not compiled in.\n",
				dssdev->name);
		return;
	}

	switch (dssdev->type) {
#ifdef CONFIG_OMAP2_DSS_DPI
	case OMAP_DISPLAY_TYPE_DPI:
		r = dpi_init_display(dssdev);
		break;
#endif
#ifdef CONFIG_OMAP2_DSS_RFBI
	case OMAP_DISPLAY_TYPE_DBI:
		r = rfbi_init_display(dssdev);
		break;
#endif
#ifdef CONFIG_OMAP2_DSS_VENC
	case OMAP_DISPLAY_TYPE_VENC:
		r = venc_init_display(dssdev);
		break;
#endif
#ifdef CONFIG_OMAP2_DSS_SDI
	case OMAP_DISPLAY_TYPE_SDI:
		r = sdi_init_display(dssdev);
		break;
#endif
#ifdef CONFIG_OMAP2_DSS_DSI
	case OMAP_DISPLAY_TYPE_DSI:
		r = dsi_init_display(dssdev);
		break;
#endif
	default:
		BUG();
	}

	if (r) {
		DSSERR("failed to init display %s\n", dssdev->name);
		return;
	}

	/* create device sysfs files */
	i = 0;
	while ((attr = display_sysfs_attrs[i++]) != NULL) {
		r = device_create_file(&dssdev->dev, attr);
		if (r)
			DSSERR("failed to create sysfs file\n");
	}

	/* create display? sysfs links */
	r = sysfs_create_link(&pdev->dev.kobj, &dssdev->dev.kobj,
			dev_name(&dssdev->dev));
	if (r)
		DSSERR("failed to create sysfs display link\n");
}

void dss_uninit_device(struct platform_device *pdev,
		struct omap_dss_device *dssdev)
{
	struct device_attribute *attr;
	int i = 0;

	sysfs_remove_link(&pdev->dev.kobj, dev_name(&dssdev->dev));

	while ((attr = display_sysfs_attrs[i++]) != NULL)
		device_remove_file(&dssdev->dev, attr);

	if (dssdev->manager)
		dssdev->manager->unset_device(dssdev->manager);
}

static int dss_suspend_device(struct device *dev, void *data)
{
	int r;
	struct omap_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		dssdev->activate_after_resume = false;
		return 0;
	}

	if (!dssdev->driver->suspend) {
		DSSERR("display '%s' doesn't implement suspend\n",
				dssdev->name);
		return -ENOSYS;
	}

	r = dssdev->driver->suspend(dssdev);
	if (r)
		return r;

	dssdev->activate_after_resume = true;

	return 0;
}

int dss_suspend_all_devices(void)
{
	int r;
	struct bus_type *bus = dss_get_bus();

	r = bus_for_each_dev(bus, NULL, NULL, dss_suspend_device);
	if (r) {
		/* resume all displays that were suspended */
		dss_resume_all_devices();
		return r;
	}

	return 0;
}

static int dss_resume_device(struct device *dev, void *data)
{
	int r;
	struct omap_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->activate_after_resume && dssdev->driver->resume) {
		r = dssdev->driver->resume(dssdev);
		if (r)
			return r;
	}

	dssdev->activate_after_resume = false;

	return 0;
}

int dss_resume_all_devices(void)
{
	struct bus_type *bus = dss_get_bus();

	return bus_for_each_dev(bus, NULL, NULL, dss_resume_device);
}

static int dss_disable_device(struct device *dev, void *data)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		dssdev->driver->disable(dssdev);

	return 0;
}

void dss_disable_all_devices(void)
{
	struct bus_type *bus = dss_get_bus();
	bus_for_each_dev(bus, NULL, NULL, dss_disable_device);
}


void omap_dss_get_device(struct omap_dss_device *dssdev)
{
	get_device(&dssdev->dev);
}
EXPORT_SYMBOL(omap_dss_get_device);

void omap_dss_put_device(struct omap_dss_device *dssdev)
{
	put_device(&dssdev->dev);
}
EXPORT_SYMBOL(omap_dss_put_device);

/* ref count of the found device is incremented. ref count
 * of from-device is decremented. */
struct omap_dss_device *omap_dss_get_next_device(struct omap_dss_device *from)
{
	struct device *dev;
	struct device *dev_start = NULL;
	struct omap_dss_device *dssdev = NULL;

	int match(struct device *dev, void *data)
	{
		return 1;
	}

	if (from)
		dev_start = &from->dev;
	dev = bus_find_device(dss_get_bus(), dev_start, NULL, match);
	if (dev)
		dssdev = to_dss_device(dev);
	if (from)
		put_device(&from->dev);

	return dssdev;
}
EXPORT_SYMBOL(omap_dss_get_next_device);

struct omap_dss_device *omap_dss_find_device(void *data,
		int (*match)(struct omap_dss_device *dssdev, void *data))
{
	struct omap_dss_device *dssdev = NULL;

	while ((dssdev = omap_dss_get_next_device(dssdev)) != NULL) {
		if (match(dssdev, data))
			return dssdev;
	}

	return NULL;
}
EXPORT_SYMBOL(omap_dss_find_device);

int omap_dss_start_device(struct omap_dss_device *dssdev)
{
	if (!dssdev->driver) {
		DSSDBG("no driver\n");
		return -ENODEV;
	}

	if (!try_module_get(dssdev->dev.driver->owner)) {
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(omap_dss_start_device);

void omap_dss_stop_device(struct omap_dss_device *dssdev)
{
	module_put(dssdev->dev.driver->owner);
}
EXPORT_SYMBOL(omap_dss_stop_device);
