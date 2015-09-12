/*
 * Northstar coma mode.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hndcoma.c 566672 2015-06-25 12:15:40Z $
 */

#include <linux/proc_fs.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>

#define DMU_BASE 0x1800c000
/* Offset */
#define CRU_INTERRUPT 0x18c
#define PCU_AOPC_CONTROL 0x20
#define PVTMON_CONTROL0 0x2c0
#define CRU_RESET 0x184

#define MAX_COMA_SLEEP_TIME 0x003fffe0

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

static uint32 dmu_base;
static struct delayed_work coma_work;
static struct platform_device *coma_pdev;

static void coma_gpioout(uint32 gpio);

static void
coma_gpioout(uint32 gpio)
{
	int enable_gpio_mask;

	enable_gpio_mask = 1 << gpio;

	/* low active */
	si_gpioout(sih, enable_gpio_mask, 0, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, enable_gpio_mask, enable_gpio_mask, GPIO_DRV_PRIORITY);
}

static void
coma_polling(struct work_struct *work)
{
	uint32 cru_interrupt;

	cru_interrupt = readl(dmu_base + CRU_INTERRUPT);
	if (cru_interrupt & 0x01) {
		coma_pdev = platform_device_register_simple("coma_dev", 0, NULL, 0);
		return;
	}
	schedule_delayed_work(&coma_work, msecs_to_jiffies(1000));
}

static int
coma_write_proc(struct file *file, const char __user *buf,
	unsigned long count, void *data)
{
	char *buffer, *var;
	uint32 pcu_aopc_control;
	uint32 pvtmon_control0;
	uint32 gpio;
	uint32 sleep_time;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		printk("%s: kmalloc failed.\n", __FUNCTION__);
		goto out;
	}

	if (copy_from_user(buffer, buf, count)) {
		printk("%s: copy_from_user failed.\n", __FUNCTION__);
		goto out;
	}

	/* enter coma mode */
	if (buffer[0] == '2') {
		/* PVT monitor power-down */
		pvtmon_control0 = readl(dmu_base + PVTMON_CONTROL0) | 0x1;
		writel(pvtmon_control0, dmu_base + PVTMON_CONTROL0);

		/* Put ROBOSW, USBPHY, OPT, PVTMON in reset mode */
		writel(0x6, dmu_base + CRU_RESET);

		/* Drive GPIO pin to low to disable on-board switching regulator */
		gpio = getgpiopin(NULL, "coma_swreg", GPIO_PIN_NOTDEFINED);
		if (gpio != GPIO_PIN_NOTDEFINED) {
			coma_gpioout(gpio);
		}

		/* Clear bit0 (PWR_DIS_LATCH_EN) of PCU_AOPC_CONTROL */
		pcu_aopc_control = readl(dmu_base + PCU_AOPC_CONTROL);
		pcu_aopc_control &= 0xfffffffe;
		writel(pcu_aopc_control, dmu_base + PCU_AOPC_CONTROL);

		/* set timer to auto-wakeup */
		pcu_aopc_control = readl(dmu_base + PCU_AOPC_CONTROL);
		var = getvar(NULL, "coma_sleep_time");
		if (var)
			sleep_time = bcm_strtoul(var, NULL, 0);
		else
			sleep_time = 0;
		sleep_time = sleep_time << 5;
		if ((sleep_time > 0) && (sleep_time <= MAX_COMA_SLEEP_TIME))
			pcu_aopc_control |= sleep_time;
		else
			pcu_aopc_control |= MAX_COMA_SLEEP_TIME;

		/* Config power down setting */
		pcu_aopc_control |= 0x8040001f;
		writel(pcu_aopc_control, dmu_base + PCU_AOPC_CONTROL);
	} else if (buffer[0] == '0') {
		/* cancel polling work */
		cancel_delayed_work(&coma_work);
	} else if (buffer[0] == '1') {
		/* resume polling work */
		schedule_delayed_work(&coma_work, msecs_to_jiffies(1000));
	}
out:
	if (buffer)
		kfree(buffer);

	return count;
}

static void __init
coma_proc_init(void)
{
	struct proc_dir_entry *coma_proc_dir, *coma;
	char *var;
	uint32 coma_disabled;

	if (BCM53573_CHIP(sih->chip)) {
		return;
	}

	var = getvar(NULL, "coma_disable");
	if (var) {
		coma_disabled = bcm_strtoul(var, NULL, 0);
		if (coma_disabled == 1)
			return;
	}

	coma_proc_dir = proc_mkdir("bcm947xx", NULL);

	if (!coma_proc_dir) {
		printk(KERN_ERR "%s: Create proc directory failed.\n", __FUNCTION__);
		return;
	}

	coma = create_proc_entry("bcm947xx/coma", 0, NULL);
	if (!coma) {
		printk(KERN_ERR "%s: Create proc entry failed.\n", __FUNCTION__);
		return;
	}

	dmu_base = (uint32)REG_MAP(DMU_BASE, 4096);
	coma->write_proc = coma_write_proc;

	INIT_DELAYED_WORK(&coma_work, coma_polling);
	schedule_delayed_work(&coma_work, msecs_to_jiffies(1000));
}

static void __exit
coma_proc_exit(void)
{
	REG_UNMAP((void *)dmu_base);
	cancel_delayed_work(&coma_work);
	remove_proc_entry("bcm947xx/coma", NULL);
}

module_init(coma_proc_init);
module_exit(coma_proc_exit);
