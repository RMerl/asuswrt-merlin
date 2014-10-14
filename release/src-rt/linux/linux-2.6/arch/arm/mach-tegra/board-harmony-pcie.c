/*
 * arch/arm/mach-tegra/board-harmony-pcie.c
 *
 * Copyright (C) 2010 CompuLab, Ltd.
 * Mike Rapoport <mike@compulab.co.il>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include <asm/mach-types.h>

#include <mach/pinmux.h>
#include "board.h"

#ifdef CONFIG_TEGRA_PCI

/* GPIO 3 of the PMIC */
#define EN_VDD_1V05_GPIO	(TEGRA_NR_GPIOS + 2)

static int __init harmony_pcie_init(void)
{
	struct regulator *regulator = NULL;
	int err;

	if (!machine_is_harmony())
		return 0;

	err = gpio_request(EN_VDD_1V05_GPIO, "EN_VDD_1V05");
	if (err)
		return err;

	gpio_direction_output(EN_VDD_1V05_GPIO, 1);

	regulator = regulator_get(NULL, "pex_clk");
	if (IS_ERR_OR_NULL(regulator))
		goto err_reg;

	regulator_enable(regulator);

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_GPV, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_SLXA, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_SLXK, TEGRA_TRI_NORMAL);

	err = tegra_pcie_init(true, true);
	if (err)
		goto err_pcie;

	return 0;

err_pcie:
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_GPV, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_SLXA, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_SLXK, TEGRA_TRI_TRISTATE);

	regulator_disable(regulator);
	regulator_put(regulator);
err_reg:
	gpio_free(EN_VDD_1V05_GPIO);

	return err;
}

/* PCI should be initialized after I2C, mfd and regulators */
subsys_initcall_sync(harmony_pcie_init);

#endif
