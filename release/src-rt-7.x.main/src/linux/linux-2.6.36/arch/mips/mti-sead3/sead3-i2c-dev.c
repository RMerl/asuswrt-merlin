#define DEBUG
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>

static struct i2c_board_info sead3_i2c_info2[] __initdata = {
	{
		I2C_BOARD_INFO("adt7476",	0x2c),
		.irq = 0,
	},
	{
		I2C_BOARD_INFO("m41t80",	0x68),
		.irq = 0,
	},
};

static int __init sead3_i2c_init(void)
{
	int err;

	pr_debug("sead3_i2c_init\n");

	err = i2c_register_board_info(2, sead3_i2c_info2,
					ARRAY_SIZE(sead3_i2c_info2));
	if (err < 0)
		printk(KERN_ERR
			"sead3-i2c-dev: cannot register board I2C devices\n");
	return err;
}

arch_initcall(sead3_i2c_init);
