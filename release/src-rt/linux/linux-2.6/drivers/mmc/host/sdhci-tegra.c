/*
 * Copyright (C) 2010 Google, Inc.
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

#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include <mach/gpio.h>
#include <mach/sdhci.h>

#include "sdhci.h"
#include "sdhci-pltfm.h"

static u32 tegra_sdhci_readl(struct sdhci_host *host, int reg)
{
	u32 val;

	if (unlikely(reg == SDHCI_PRESENT_STATE)) {
		/* Use wp_gpio here instead? */
		val = readl(host->ioaddr + reg);
		return val | SDHCI_WRITE_PROTECT;
	}

	return readl(host->ioaddr + reg);
}

static u16 tegra_sdhci_readw(struct sdhci_host *host, int reg)
{
	if (unlikely(reg == SDHCI_HOST_VERSION)) {
		/* Erratum: Version register is invalid in HW. */
		return SDHCI_SPEC_200;
	}

	return readw(host->ioaddr + reg);
}

static void tegra_sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
	/* Seems like we're getting spurious timeout and crc errors, so
	 * disable signalling of them. In case of real errors software
	 * timers should take care of eventually detecting them.
	 */
	if (unlikely(reg == SDHCI_SIGNAL_ENABLE))
		val &= ~(SDHCI_INT_TIMEOUT|SDHCI_INT_CRC);

	writel(val, host->ioaddr + reg);

	if (unlikely(reg == SDHCI_INT_ENABLE)) {
		/* Erratum: Must enable block gap interrupt detection */
		u8 gap_ctrl = readb(host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
		if (val & SDHCI_INT_CARD_INT)
			gap_ctrl |= 0x8;
		else
			gap_ctrl &= ~0x8;
		writeb(gap_ctrl, host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
	}
}

static unsigned int tegra_sdhci_get_ro(struct sdhci_host *sdhci)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhci->mmc));
	struct tegra_sdhci_platform_data *plat;

	plat = pdev->dev.platform_data;

	if (!gpio_is_valid(plat->wp_gpio))
		return -1;

	return gpio_get_value(plat->wp_gpio);
}

static irqreturn_t carddetect_irq(int irq, void *data)
{
	struct sdhci_host *sdhost = (struct sdhci_host *)data;

	tasklet_schedule(&sdhost->card_tasklet);
	return IRQ_HANDLED;
};

static int tegra_sdhci_8bit(struct sdhci_host *host, int bus_width)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct tegra_sdhci_platform_data *plat;
	u32 ctrl;

	plat = pdev->dev.platform_data;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	if (plat->is_8bit && bus_width == MMC_BUS_WIDTH_8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		ctrl |= SDHCI_CTRL_8BITBUS;
	} else {
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (bus_width == MMC_BUS_WIDTH_4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
	return 0;
}


static int tegra_sdhci_pltfm_init(struct sdhci_host *host,
				  struct sdhci_pltfm_data *pdata)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct tegra_sdhci_platform_data *plat;
	struct clk *clk;
	int rc;

	plat = pdev->dev.platform_data;
	if (plat == NULL) {
		dev_err(mmc_dev(host->mmc), "missing platform data\n");
		return -ENXIO;
	}

	if (gpio_is_valid(plat->power_gpio)) {
		rc = gpio_request(plat->power_gpio, "sdhci_power");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate power gpio\n");
			goto out;
		}
		tegra_gpio_enable(plat->power_gpio);
		gpio_direction_output(plat->power_gpio, 1);
	}

	if (gpio_is_valid(plat->cd_gpio)) {
		rc = gpio_request(plat->cd_gpio, "sdhci_cd");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate cd gpio\n");
			goto out_power;
		}
		tegra_gpio_enable(plat->cd_gpio);
		gpio_direction_input(plat->cd_gpio);

		rc = request_irq(gpio_to_irq(plat->cd_gpio), carddetect_irq,
				 IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				 mmc_hostname(host->mmc), host);

		if (rc)	{
			dev_err(mmc_dev(host->mmc), "request irq error\n");
			goto out_cd;
		}

	}

	if (gpio_is_valid(plat->wp_gpio)) {
		rc = gpio_request(plat->wp_gpio, "sdhci_wp");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate wp gpio\n");
			goto out_irq;
		}
		tegra_gpio_enable(plat->wp_gpio);
		gpio_direction_input(plat->wp_gpio);
	}

	clk = clk_get(mmc_dev(host->mmc), NULL);
	if (IS_ERR(clk)) {
		dev_err(mmc_dev(host->mmc), "clk err\n");
		rc = PTR_ERR(clk);
		goto out_wp;
	}
	clk_enable(clk);
	pltfm_host->clk = clk;

	if (plat->is_8bit)
		host->mmc->caps |= MMC_CAP_8_BIT_DATA;

	return 0;

out_wp:
	if (gpio_is_valid(plat->wp_gpio)) {
		tegra_gpio_disable(plat->wp_gpio);
		gpio_free(plat->wp_gpio);
	}

out_irq:
	if (gpio_is_valid(plat->cd_gpio))
		free_irq(gpio_to_irq(plat->cd_gpio), host);
out_cd:
	if (gpio_is_valid(plat->cd_gpio)) {
		tegra_gpio_disable(plat->cd_gpio);
		gpio_free(plat->cd_gpio);
	}

out_power:
	if (gpio_is_valid(plat->power_gpio)) {
		tegra_gpio_disable(plat->power_gpio);
		gpio_free(plat->power_gpio);
	}

out:
	return rc;
}

static void tegra_sdhci_pltfm_exit(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct tegra_sdhci_platform_data *plat;

	plat = pdev->dev.platform_data;

	if (gpio_is_valid(plat->wp_gpio)) {
		tegra_gpio_disable(plat->wp_gpio);
		gpio_free(plat->wp_gpio);
	}

	if (gpio_is_valid(plat->cd_gpio)) {
		free_irq(gpio_to_irq(plat->cd_gpio), host);
		tegra_gpio_disable(plat->cd_gpio);
		gpio_free(plat->cd_gpio);
	}

	if (gpio_is_valid(plat->power_gpio)) {
		tegra_gpio_disable(plat->power_gpio);
		gpio_free(plat->power_gpio);
	}

	clk_disable(pltfm_host->clk);
	clk_put(pltfm_host->clk);
}

static struct sdhci_ops tegra_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.read_l     = tegra_sdhci_readl,
	.read_w     = tegra_sdhci_readw,
	.write_l    = tegra_sdhci_writel,
	.platform_8bit_width = tegra_sdhci_8bit,
};

struct sdhci_pltfm_data sdhci_tegra_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC,
	.ops  = &tegra_sdhci_ops,
	.init = tegra_sdhci_pltfm_init,
	.exit = tegra_sdhci_pltfm_exit,
};
