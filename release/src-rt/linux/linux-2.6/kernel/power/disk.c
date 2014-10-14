/*
 * kernel/power/disk.c - Suspend-to-disk support.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 * Copyright (c) 2004 Pavel Machek <pavel@suse.cz>
 *
 * This file is released under the GPLv2.
 *
 */

#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/pm.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/freezer.h>

#include "power.h"


static int noresume = 0;
char resume_file[256] = CONFIG_PM_STD_PARTITION;
dev_t swsusp_resume_device;
sector_t swsusp_resume_block;

enum {
	HIBERNATION_INVALID,
	HIBERNATION_PLATFORM,
	HIBERNATION_TEST,
	HIBERNATION_TESTPROC,
	HIBERNATION_SHUTDOWN,
	HIBERNATION_REBOOT,
	/* keep last */
	__HIBERNATION_AFTER_LAST
};
#define HIBERNATION_MAX (__HIBERNATION_AFTER_LAST-1)
#define HIBERNATION_FIRST (HIBERNATION_INVALID + 1)

static int hibernation_mode = HIBERNATION_SHUTDOWN;

struct hibernation_ops *hibernation_ops;

/**
 * hibernation_set_ops - set the global hibernate operations
 * @ops: the hibernation operations to use in subsequent hibernation transitions
 */

void hibernation_set_ops(struct hibernation_ops *ops)
{
	if (ops && !(ops->prepare && ops->enter && ops->finish)) {
		WARN_ON(1);
		return;
	}
	mutex_lock(&pm_mutex);
	hibernation_ops = ops;
	if (ops)
		hibernation_mode = HIBERNATION_PLATFORM;
	else if (hibernation_mode == HIBERNATION_PLATFORM)
		hibernation_mode = HIBERNATION_SHUTDOWN;

	mutex_unlock(&pm_mutex);
}


/**
 *	platform_prepare - prepare the machine for hibernation using the
 *	platform driver if so configured and return an error code if it fails
 */

static int platform_prepare(void)
{
	return (hibernation_mode == HIBERNATION_PLATFORM && hibernation_ops) ?
		hibernation_ops->prepare() : 0;
}

/**
 *	platform_finish - switch the machine to the normal mode of operation
 *	using the platform driver (must be called after platform_prepare())
 */

static void platform_finish(void)
{
	if (hibernation_mode == HIBERNATION_PLATFORM && hibernation_ops)
		hibernation_ops->finish();
}

/**
 *	power_down - Shut the machine down for hibernation.
 *
 *	Use the platform driver, if configured so; otherwise try
 *	to power off or reboot.
 */

static void power_down(void)
{
	switch (hibernation_mode) {
	case HIBERNATION_TEST:
	case HIBERNATION_TESTPROC:
		break;
	case HIBERNATION_SHUTDOWN:
		kernel_power_off();
		break;
	case HIBERNATION_REBOOT:
		kernel_restart(NULL);
		break;
	case HIBERNATION_PLATFORM:
		if (hibernation_ops) {
			kernel_shutdown_prepare(SYSTEM_SUSPEND_DISK);
			hibernation_ops->enter();
			break;
		}
	}
	kernel_halt();
	/*
	 * Valid image is on the disk, if we continue we risk serious data
	 * corruption after resume.
	 */
	printk(KERN_CRIT "Please power me down manually\n");
	while(1);
}

static void unprepare_processes(void)
{
	thaw_processes();
	pm_restore_console();
}

static int prepare_processes(void)
{
	int error = 0;

	pm_prepare_console();
	if (freeze_processes()) {
		error = -EBUSY;
		unprepare_processes();
	}
	return error;
}

/**
 *	hibernate - The granpappy of the built-in hibernation management
 */

int hibernate(void)
{
	int error;

	/* The snapshot device should not be opened while we're running */
	if (!atomic_add_unless(&snapshot_device_available, -1, 0))
		return -EBUSY;

	/* Allocate memory management structures */
	error = create_basic_memory_bitmaps();
	if (error)
		goto Exit;

	error = prepare_processes();
	if (error)
		goto Finish;

	mutex_lock(&pm_mutex);
	if (hibernation_mode == HIBERNATION_TESTPROC) {
		printk("swsusp debug: Waiting for 5 seconds.\n");
		mdelay(5000);
		goto Thaw;
	}

	/* Free memory before shutting down devices. */
	error = swsusp_shrink_memory();
	if (error)
		goto Thaw;

	error = platform_prepare();
	if (error)
		goto Thaw;

	suspend_console();
	error = device_suspend(PMSG_FREEZE);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to suspend\n");
		goto Resume_devices;
	}
	error = disable_nonboot_cpus();
	if (error)
		goto Enable_cpus;

	if (hibernation_mode == HIBERNATION_TEST) {
		printk("swsusp debug: Waiting for 5 seconds.\n");
		mdelay(5000);
		goto Enable_cpus;
	}

	pr_debug("PM: snapshotting memory.\n");
	in_suspend = 1;
	error = swsusp_suspend();
	if (error)
		goto Enable_cpus;

	if (in_suspend) {
		enable_nonboot_cpus();
		platform_finish();
		device_resume();
		resume_console();
		pr_debug("PM: writing image.\n");
		error = swsusp_write();
		if (!error)
			power_down();
		else {
			swsusp_free();
			goto Thaw;
		}
	} else {
		pr_debug("PM: Image restored successfully.\n");
	}

	swsusp_free();
 Enable_cpus:
	enable_nonboot_cpus();
 Resume_devices:
	platform_finish();
	device_resume();
	resume_console();
 Thaw:
	mutex_unlock(&pm_mutex);
	unprepare_processes();
 Finish:
	free_basic_memory_bitmaps();
 Exit:
	atomic_inc(&snapshot_device_available);
	return error;
}


/**
 *	software_resume - Resume from a saved image.
 *
 *	Called as a late_initcall (so all devices are discovered and
 *	initialized), we call swsusp to see if we have a saved image or not.
 *	If so, we quiesce devices, the restore the saved image. We will
 *	return above (in hibernate() ) if everything goes well.
 *	Otherwise, we fail gracefully and return to the normally
 *	scheduled program.
 *
 */

static int software_resume(void)
{
	int error;

	mutex_lock(&pm_mutex);
	if (!swsusp_resume_device) {
		if (!strlen(resume_file)) {
			mutex_unlock(&pm_mutex);
			return -ENOENT;
		}
		swsusp_resume_device = name_to_dev_t(resume_file);
		pr_debug("swsusp: Resume From Partition %s\n", resume_file);
	} else {
		pr_debug("swsusp: Resume From Partition %d:%d\n",
			 MAJOR(swsusp_resume_device), MINOR(swsusp_resume_device));
	}

	if (noresume) {
		/**
		 * FIXME: If noresume is specified, we need to find the partition
		 * and reset it back to normal swap space.
		 */
		mutex_unlock(&pm_mutex);
		return 0;
	}

	pr_debug("PM: Checking swsusp image.\n");
	error = swsusp_check();
	if (error)
		goto Unlock;

	/* The snapshot device should not be opened while we're running */
	if (!atomic_add_unless(&snapshot_device_available, -1, 0)) {
		error = -EBUSY;
		goto Unlock;
	}

	error = create_basic_memory_bitmaps();
	if (error)
		goto Finish;

	pr_debug("PM: Preparing processes for restore.\n");
	error = prepare_processes();
	if (error) {
		swsusp_close();
		goto Done;
	}

	pr_debug("PM: Reading swsusp image.\n");

	error = swsusp_read();
	if (error) {
		swsusp_free();
		goto Thaw;
	}

	pr_debug("PM: Preparing devices for restore.\n");

	suspend_console();
	error = device_suspend(PMSG_PRETHAW);
	if (error)
		goto Free;

	error = disable_nonboot_cpus();
	if (!error)
		swsusp_resume();

	enable_nonboot_cpus();
 Free:
	swsusp_free();
	device_resume();
	resume_console();
 Thaw:
	printk(KERN_ERR "PM: Restore failed, recovering.\n");
	unprepare_processes();
 Done:
	free_basic_memory_bitmaps();
 Finish:
	atomic_inc(&snapshot_device_available);
	/* For success case, the suspend path will release the lock */
 Unlock:
	mutex_unlock(&pm_mutex);
	pr_debug("PM: Resume from disk failed.\n");
	return 0;
}

late_initcall(software_resume);


static const char * const hibernation_modes[] = {
	[HIBERNATION_PLATFORM]	= "platform",
	[HIBERNATION_SHUTDOWN]	= "shutdown",
	[HIBERNATION_REBOOT]	= "reboot",
	[HIBERNATION_TEST]	= "test",
	[HIBERNATION_TESTPROC]	= "testproc",
};

/**
 *	disk - Control hibernation mode
 *
 *	Suspend-to-disk can be handled in several ways. We have a few options
 *	for putting the system to sleep - using the platform driver (e.g. ACPI
 *	or other hibernation_ops), powering off the system or rebooting the
 *	system (for testing) as well as the two test modes.
 *
 *	The system can support 'platform', and that is known a priori (and
 *	encoded by the presence of hibernation_ops). However, the user may
 *	choose 'shutdown' or 'reboot' as alternatives, as well as one fo the
 *	test modes, 'test' or 'testproc'.
 *
 *	show() will display what the mode is currently set to.
 *	store() will accept one of
 *
 *	'platform'
 *	'shutdown'
 *	'reboot'
 *	'test'
 *	'testproc'
 *
 *	It will only change to 'platform' if the system
 *	supports it (as determined by having hibernation_ops).
 */

static ssize_t disk_show(struct kset *kset, char *buf)
{
	int i;
	char *start = buf;

	for (i = HIBERNATION_FIRST; i <= HIBERNATION_MAX; i++) {
		if (!hibernation_modes[i])
			continue;
		switch (i) {
		case HIBERNATION_SHUTDOWN:
		case HIBERNATION_REBOOT:
		case HIBERNATION_TEST:
		case HIBERNATION_TESTPROC:
			break;
		case HIBERNATION_PLATFORM:
			if (hibernation_ops)
				break;
			/* not a valid mode, continue with loop */
			continue;
		}
		if (i == hibernation_mode)
			buf += sprintf(buf, "[%s] ", hibernation_modes[i]);
		else
			buf += sprintf(buf, "%s ", hibernation_modes[i]);
	}
	buf += sprintf(buf, "\n");
	return buf-start;
}


static ssize_t disk_store(struct kset *kset, const char *buf, size_t n)
{
	int error = 0;
	int i;
	int len;
	char *p;
	int mode = HIBERNATION_INVALID;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	mutex_lock(&pm_mutex);
	for (i = HIBERNATION_FIRST; i <= HIBERNATION_MAX; i++) {
		if (len == strlen(hibernation_modes[i])
		    && !strncmp(buf, hibernation_modes[i], len)) {
			mode = i;
			break;
		}
	}
	if (mode != HIBERNATION_INVALID) {
		switch (mode) {
		case HIBERNATION_SHUTDOWN:
		case HIBERNATION_REBOOT:
		case HIBERNATION_TEST:
		case HIBERNATION_TESTPROC:
			hibernation_mode = mode;
			break;
		case HIBERNATION_PLATFORM:
			if (hibernation_ops)
				hibernation_mode = mode;
			else
				error = -EINVAL;
		}
	} else
		error = -EINVAL;

	if (!error)
		pr_debug("PM: suspend-to-disk mode set to '%s'\n",
			 hibernation_modes[mode]);
	mutex_unlock(&pm_mutex);
	return error ? error : n;
}

power_attr(disk);

static ssize_t resume_show(struct kset *kset, char *buf)
{
	return sprintf(buf,"%d:%d\n", MAJOR(swsusp_resume_device),
		       MINOR(swsusp_resume_device));
}

static ssize_t resume_store(struct kset *kset, const char *buf, size_t n)
{
	unsigned int maj, min;
	dev_t res;
	int ret = -EINVAL;

	if (sscanf(buf, "%u:%u", &maj, &min) != 2)
		goto out;

	res = MKDEV(maj,min);
	if (maj != MAJOR(res) || min != MINOR(res))
		goto out;

	mutex_lock(&pm_mutex);
	swsusp_resume_device = res;
	mutex_unlock(&pm_mutex);
	printk("Attempting manual resume\n");
	noresume = 0;
	software_resume();
	ret = n;
 out:
	return ret;
}

power_attr(resume);

static ssize_t image_size_show(struct kset *kset, char *buf)
{
	return sprintf(buf, "%lu\n", image_size);
}

static ssize_t image_size_store(struct kset *kset, const char *buf, size_t n)
{
	unsigned long size;

	if (sscanf(buf, "%lu", &size) == 1) {
		image_size = size;
		return n;
	}

	return -EINVAL;
}

power_attr(image_size);

static struct attribute * g[] = {
	&disk_attr.attr,
	&resume_attr.attr,
	&image_size_attr.attr,
	NULL,
};


static struct attribute_group attr_group = {
	.attrs = g,
};


static int __init pm_disk_init(void)
{
	return sysfs_create_group(&power_subsys.kobj, &attr_group);
}

core_initcall(pm_disk_init);


static int __init resume_setup(char *str)
{
	if (noresume)
		return 1;

	strncpy( resume_file, str, 255 );
	return 1;
}

static int __init resume_offset_setup(char *str)
{
	unsigned long long offset;

	if (noresume)
		return 1;

	if (sscanf(str, "%llu", &offset) == 1)
		swsusp_resume_block = offset;

	return 1;
}

static int __init noresume_setup(char *str)
{
	noresume = 1;
	return 1;
}

__setup("noresume", noresume_setup);
__setup("resume_offset=", resume_offset_setup);
__setup("resume=", resume_setup);
