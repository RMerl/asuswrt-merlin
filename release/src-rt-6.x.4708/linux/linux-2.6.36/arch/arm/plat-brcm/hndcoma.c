/*
 * Northstar coma mode.
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndcoma.c 400139 2013-05-03 02:31:23Z $
 */

#include <linux/proc_fs.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <asm/uaccess.h>

#define DMU_BASE 0x1800c000
#define CHIP_COMMON_A 0x18000000
/* Offset */
#define CRU_INTERRUPT 0x18c
#define PCU_AOPC_CONTROL 0x20
#define PVTMON_CONTROL0 0x2c0
#define CRU_RESET 0x184
#define CHIP_COMMON_A_GPIOOUT 0x64
#define CHIP_COMMON_A_GPIOOUTEN 0x68

#define MAX_COMA_SLEEP_TIME 0x003fffe0

static void coma_gpioout(uint32 gpio);

static void
coma_gpioout(uint32 gpio)
{
	uint32 val;
	uint32 chip_common_a;

	chip_common_a = (uint32)REG_MAP(CHIP_COMMON_A, 4096);

	gpio = 1 << gpio;
	val = readl(chip_common_a + CHIP_COMMON_A_GPIOOUT);
	val &= ~gpio;
	writel(val, chip_common_a + CHIP_COMMON_A_GPIOOUT);
	val = readl(chip_common_a + CHIP_COMMON_A_GPIOOUTEN);
	val |= gpio;
	writel(val, chip_common_a + CHIP_COMMON_A_GPIOOUTEN);
	REG_UNMAP((void *)chip_common_a);
}

static int coma_read_proc(char * buffer, char **start,
off_t offset, int length, int * eof, void * data)
{
	uint32 cru_interrupt;
	uint32 reg_base;
	int len;

	len = 0;

	reg_base = (uint32)REG_MAP(DMU_BASE, 4096);
	cru_interrupt = readl(reg_base + CRU_INTERRUPT);
	REG_UNMAP((void *)reg_base);
	if (cru_interrupt & 0x01)
		len += sprintf(buffer + len, "1\n");
	else
		len += sprintf(buffer + len, "0\n");

	return len;
}

static int coma_write_proc(struct file *file, const char __user *buf,
	unsigned long count, void *data)
{
	char *buffer, *var;
	uint32 reg_base;
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

	if (buffer[0] == '1') {
		/* PVT monitor power-down */
		reg_base = (uint32)REG_MAP(DMU_BASE, 4096);
		pvtmon_control0 = readl(reg_base + PVTMON_CONTROL0) | 0x1;
		writel(pvtmon_control0, reg_base + PVTMON_CONTROL0);

		/* Put ROBOSW, USBPHY, OPT, PVTMON in reset mode */
		writel(0x6, reg_base + CRU_RESET);

		/* Drive GPIO pin to low to disable on-board switching regulator */
		gpio = getgpiopin(NULL, "coma_swreg", GPIO_PIN_NOTDEFINED);
		if (gpio != GPIO_PIN_NOTDEFINED) {
			coma_gpioout(gpio);
		}

		/* Clear bit0 (PWR_DIS_LATCH_EN) of PCU_AOPC_CONTROL */
		pcu_aopc_control = readl(reg_base + PCU_AOPC_CONTROL);
		pcu_aopc_control &= 0xfffffffe;
		writel(pcu_aopc_control, reg_base + PCU_AOPC_CONTROL);

		/* set timer to auto-wakeup */
		pcu_aopc_control = readl(reg_base + PCU_AOPC_CONTROL);
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
		writel(pcu_aopc_control, reg_base + PCU_AOPC_CONTROL);
	}
out:
	if (buffer)
		kfree(buffer);

	return count;
}

static void __init coma_proc_init(void)
{
	struct proc_dir_entry *coma_proc_dir, *coma;

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

	coma->read_proc = coma_read_proc;
	coma->write_proc = coma_write_proc;
}
fs_initcall(coma_proc_init);
