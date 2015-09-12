/*
 * Northstar DMU (Device Management Unit),
 * i.e. clocks, I/O pads, GPIO etc.
 * SoC-specific hardware support features.
 *
 * Documents:
 * Northstar_top_power_uarch_v1_0.pdf
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
 * $Id: $
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#include <asm/clkdev.h>
#include <mach/clkdev.h>
#include <mach/io_map.h>
#include <plat/plat-bcm5301x.h>

#include <typedefs.h>
#include <siutils.h>
#include <bcmdevs.h>

#ifdef CONFIG_PROC_FS
#define DMU_PROC_NAME	"dmu"
#endif /* CONFIG_PROC_FS */

/* Global SB handle */
extern si_t *bcm947xx_sih;

/* Convenience */
#define sih bcm947xx_sih

static struct resource dmu_regs = {
	.name = "dmu_regs",
	.start = SOC_DMU_BASE_PA,
	.end = SOC_DMU_BASE_PA + SZ_4K -1,
	.flags = IORESOURCE_MEM,
};

static struct resource pmu_regs = {
	.name = "pmu_regs",
	.start = SOC_PMU_BASE_PA,
	.end = SOC_PMU_BASE_PA + SZ_4K - 1,
	.flags = IORESOURCE_MEM,
};

/*
 * Clock management scheme is a provisional implementation
 * only intended to retreive the pre-set frequencies for each
 * of the clocks.
 * Better handling of post-dividers and fractional part of
 * feedbeck dividers need to be added.
 * Need to understand what diagnostics from CRU registers could
 * be handy, and export that via a sysfs interface.
 */

/*
 * The CRU contains two similar PLLs: LCPLL and GENPLL,
 * both with several output channels divided from the PLL
 * output
 */

/*
 * Get PLL running status and update output frequency
 */
static int lcpll_status(struct clk * clk)
{
	u32 reg;
	u64 x;
	unsigned pdiv, ndiv_int, ndiv_frac;

	if (clk->type != CLK_PLL)
		return -EINVAL;

	/* read status register */
	reg = readl(clk->regs_base + 0x10);

	/* bit 12 is "lock" signal, has to be "1" for proper PLL operation */
	if ((reg & (1 << 12)) == 0) {
		clk->rate = 0;
	}

	/* Update PLL frequency */

	/* control1 register */
	reg = readl(clk->regs_base + 0x04);

	/* feedback divider integer and fraction parts */
	pdiv = (reg >> 28) & 7;
	ndiv_int = (reg >> 20) & 0xff;
	ndiv_frac = reg & ((1<<20)-1);

	if (pdiv == 0)
		return -EIO;

	x = clk->parent->rate / pdiv;

	x = x * ((u64) ndiv_int << 20 | ndiv_frac);

	clk->rate = x >> 20;

	return 0;
}

static const struct clk_ops lcpll_ops = {
	.status = lcpll_status,
};

static int lcpll_chan_status(struct clk * clk)
{
	void * __iomem base;
	u32 reg;
	unsigned enable;
	unsigned mdiv;

	if (clk->parent == NULL || clk->type != CLK_DIV)
		return -EINVAL;

	/* Register address is only stored in PLL structure */
	base = clk->parent->regs_base;
	BUG_ON(base == NULL);

	/* enable bit is in enableb_ch[] inversed */
	enable = ((readl(base + 0) >> 6) & 7) ^ 7;

	if ((enable & (1 << clk->chan)) == 0) {
		clk->rate = 0;
		return -EIO;
	}

	/* get divider */
	reg = readl(base + 0x08);

	mdiv = 0xff & (reg >> ((0x3^clk->chan) << 3));

	/* when divisor is 0, it behaves as max+1 */
	if (mdiv == 0)
		mdiv = 1 << 8;

	clk->rate = (clk->parent->rate / mdiv);
	return 0;
}


static const struct clk_ops lcpll_chan_ops = {
	.status = lcpll_chan_status,
};

/*
 * LCPLL has 4 output channels
 */
static struct clk clk_lcpll = {
	.ops 	= &lcpll_ops,
	.name 	= "LCPLL",
	.type	= CLK_PLL,
	.chan	=	4,
};

/*
 * LCPLL output clocks -
 * chan 0 - PCIe ref clock, should be 1 GHz,
 * chan 1 - SDIO clock, e.g. 200 MHz,
 * chan 2 - DDR clock, typical 166.667 MHz for DDR667,
 * chan 3 - Unknown
 */
static struct clk clk_lcpll_ch[4] = {
	{.ops = &lcpll_chan_ops, .parent = &clk_lcpll, .type = CLK_DIV,
	.name = "lcpll_ch0", .chan = 0},
	{.ops = &lcpll_chan_ops, .parent = &clk_lcpll, .type = CLK_DIV,
	.name = "lcpll_ch1", .chan = 1},
	{.ops = &lcpll_chan_ops, .parent = &clk_lcpll, .type = CLK_DIV,
	.name = "lcpll_ch2", .chan = 2},
	{.ops = &lcpll_chan_ops, .parent = &clk_lcpll, .type = CLK_DIV,
	.name = "lcpll_ch3", .chan = 3},
};

/*
 * Get PLL running status and update output frequency
 */
static int genpll_status(struct clk * clk)
{
	u32 reg;
	u64 x;
	unsigned pdiv, ndiv_int, ndiv_frac;

	if (clk->type != CLK_PLL)
		return -EINVAL;

	/* Offset of the PLL status register */
	reg = readl(clk->regs_base + 0x20);

	/* bit 12 is "lock" signal, has to be "1" for proper PLL operation */
	if ((reg & (1 << 12)) == 0) {
		clk->rate = 0;
		return -EIO;
	}

	/* Update PLL frequency */

	/* get PLL feedback divider values from control5 */
	reg = readl(clk->regs_base + 0x14);

	/* feedback divider integer and fraction parts */
	ndiv_int = reg >> 20;
	ndiv_frac = reg & ((1<<20)-1);

	/* get pdiv */
	reg = readl(clk->regs_base + 0x18);
	pdiv = (reg >> 24) & 7;

	if (pdiv == 0)
		return -EIO;

	x = clk->parent->rate / pdiv;

	x = x * ((u64)ndiv_int << 20 | ndiv_frac);

	clk->rate = x >> 20;

	return 0;
}

static const struct clk_ops genpll_ops = {
	.status = genpll_status,
};

static int genpll_chan_status(struct clk * clk)
{
	void * __iomem base;
	u32 reg;
	unsigned enable;
	unsigned mdiv;
	unsigned off, shift;

	if (clk->parent == NULL || clk->type != CLK_DIV)
		return -EINVAL;

	/* Register address is only stored in PLL structure */
	base = clk->parent->regs_base;

	BUG_ON(base == NULL);

	/* enable bit is in enableb_ch[0..5] inversed */
	enable = ((readl(base + 0x04) >> 12) & 0x3f) ^ 0x3f;

	if ((enable & (1 << clk->chan)) == 0) {
		clk->rate = 0;
		return -EIO;
	}

	/* GENPLL has the 6 channels spread over two regs */
	switch (clk->chan)
		{
		case 0:
			off = 0x18; shift = 16;
			break;

		case 1:
			off = 0x18; shift = 8;
			break;

		case 2:
			off = 0x18; shift = 0;
			break;

		case 3:
			off = 0x1c; shift = 16;
			break;

		case 4:
			off = 0x1c; shift = 8;
			break;

		case 5:
			off = 0x1c; shift = 0;
			break;

		default:
			BUG_ON(clk->chan);
			off = shift = 0;	/* fend off warnings */
		}

	reg = readl(base + off);

	mdiv = 0xff & (reg >> shift);

	/* when divisor is 0, it behaves as max+1 */
	if (mdiv == 0)
		mdiv = 1 << 8;

	clk->rate = clk->parent->rate / mdiv;
	return 0;
}


static const struct clk_ops genpll_chan_ops = {
	.status = genpll_chan_status,
};


/*
 * GENPLL has 6 output channels
 */
static struct clk clk_genpll = {
	.ops	= &genpll_ops,
	.name	= "GENPLL",
	.type	= CLK_PLL,
	.chan	= 6,
};

/*
 * chan 0 - Ethernet switch and MAC, RGMII, need 250 MHz
 * chan 1 - Ethernet switch slow clock, 150 Mhz
 * chan 2 - USB PHY clock, need 30 MHz
 * chan 3 - iProc N MHz clock, set from OTP
 * chan 4 - iProc N/2 MHz clock, set from OTP
 * chan 5 - iProc N/4 MHz clock, set from OTP
 *
 */
static struct clk clk_genpll_ch[6] = {
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch0", .chan = 0},
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch1", .chan = 1},
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch2", .chan = 2},
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch3", .chan = 3},
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch4", .chan = 4},
	{.ops = &genpll_chan_ops, .parent = &clk_genpll, .type = CLK_DIV,
	.name = "genpll_ch5", .chan = 5}
};

/*
 * This table is used to locate clock sources
 * from device drivers
 */
static struct clk_lookup soc_clk_lookups[] = {
	/* a.k.a. "c_clk100" */
	{.con_id = "pcie_clk", .clk = &clk_lcpll_ch[0]},
	/* a.k.a. "c_clk200" */
	{.con_id = "sdio_clk", .clk = &clk_lcpll_ch[1]},
	/* a.k.a. "c_clk400" */
	{.con_id = "ddr_clk", .clk = &clk_lcpll_ch[2]},
	/* unassigned ? */
	{.con_id = "c_clk120", .clk = &clk_lcpll_ch[3]},
	/* "c_clk250" */
	{.con_id = "en_phy_clk", .clk = &clk_genpll_ch[0]},
	/* "c_clk150" */
	{.con_id = "en_clk", .clk = &clk_genpll_ch[1]},
	/* "c_clk30" */
	{.con_id = "usb_phy_clk", .clk = &clk_genpll_ch[2]},
	/* "c_clk500" */
	{.con_id = "iproc_fast_clk", .clk = &clk_genpll_ch[3]},
	/* "c_clk250" */
	{.con_id = "iproc_med_clk", .clk = &clk_genpll_ch[4]},
	/* "c_clk125" */
	{.con_id = "iproc_slow_clk", .clk = &clk_genpll_ch[5]}
};

/*
 * Get PMU CPUPLL running status and update PLL VCO output frequency
 * Fvco = ([ndiv_int + (ndiv_frac / 2^20)] * Freq * 2^pll_ctrl) / pdiv
 */
static int cpupll_status(struct clk * clk)
{
	u32 reg;
	u64 x;
	unsigned pdiv, ndiv_int, ndiv_frac;

	if (clk->type != CLK_PLL) {
		return -EINVAL;
	}

	reg = readl(IO_BASE_VA + 0x2c);

	/* bit 0 is cpupll "lock" signal, it has to be "1" for proper PLL operation */
	if ((reg & 0x1) == 0) {
		early_printk(KERN_ERR "CPUPLL is unlocked!\n");
		clk->rate = 0;
		return -EIO;
	}

	/* Get pdiv from PLL control register 13 */
	writel(13, clk->regs_base + PMU_CONTROL_ADDR_OFF);
	isb();
	reg = readl(clk->regs_base + PMU_CONTROL_DATA_OFF);

	pdiv = (reg >> 24) & 0x7;

	/* Get ndiv and ndiv_frac from PLL control register 14 */
	writel(14, clk->regs_base + PMU_CONTROL_ADDR_OFF);
	isb();
	reg = readl(clk->regs_base + PMU_CONTROL_DATA_OFF);

	ndiv_int = (reg >> 20) & 0x3ff;
	ndiv_frac = reg & ((1 << 20) - 1);

	if (pdiv == 0) {
		return -EIO;
	}

	x = clk->parent->rate / pdiv;

	x = x * ((u64) ndiv_int << 20 | ndiv_frac);

	/* Get pll_ctrl from bit[20] of PLL control register 15 */
	writel(15, clk->regs_base + PMU_CONTROL_ADDR_OFF);
	isb();
	reg = readl(clk->regs_base + PMU_CONTROL_DATA_OFF);
	reg = (reg >> 20) & 0x1;
	x = x << reg;

	clk->rate = x >> 20;

	return 0;
}

static const struct clk_ops cpupll_ops = {
	.status = cpupll_status,
};

/* PMU CPUPLL for BCM53573 */
static struct clk clk_cpupll = {
	.ops	= &cpupll_ops,
	.name	= "CPUPLL",
	.type	= CLK_PLL,
	.chan	= 3,
};

static int cpupll_chan_status(struct clk * clk)
{
	void * __iomem base;
	u32 reg;
	unsigned enable;
	unsigned mdiv;

	if (clk->parent == NULL || clk->type != CLK_DIV) {
		return -EINVAL;
	}

	/* Register address is only stored in PLL structure */
	base = clk->parent->regs_base;
	BUG_ON(base == NULL);

	/* Get enable bit in enableb_ch[] inversed */
	writel(12, base + PMU_CONTROL_ADDR_OFF);
	isb();
	reg = readl(base + PMU_CONTROL_DATA_OFF);

	enable = ((reg >> 16) & 7) ^ 7;

	if ((enable & (1 << clk->chan)) == 0) {
		early_printk(KERN_ERR "CPUPLL channel %d is disabled!\n", clk->chan);
		clk->rate = 0;
		return -EIO;
	}

	/* Get mdiv from PLL control register 13 */
	writel(13, base + PMU_CONTROL_ADDR_OFF);
	isb();
	reg = readl(base + PMU_CONTROL_DATA_OFF);

	mdiv = 0xff & (reg >> ((0x3 & clk->chan) << 3));

	if (mdiv == 0) {
		mdiv = 1 << 8;
	}

	clk->rate = (clk->parent->rate / mdiv);

	return 0;
}

static const struct clk_ops cpupll_chan_ops = {
	.status = cpupll_chan_status,
};

/*
 * PMU CPUPLL for BCM53573
 * Channel 0: CPU clock, Channel 1: CCI-400 clock, Channel 2: NIC-400 clock
 */
static struct clk clk_cpupll_ch[3] = {
	{.ops = &cpupll_chan_ops, .parent = &clk_cpupll, .type = CLK_DIV,
	.name = "cpupll_ch0", .chan = 0},
	{.ops = &cpupll_chan_ops, .parent = &clk_cpupll, .type = CLK_DIV,
	.name = "cpupll_ch1", .chan = 1},
	{.ops = &cpupll_chan_ops, .parent = &clk_cpupll, .type = CLK_DIV,
	.name = "cpupll_ch2", .chan = 2},
};

static struct clk_lookup pmu_clk_lookups[] = {
	{.con_id = "cpu_clk", .clk = &clk_cpupll_ch[0]},
	{.con_id = "cci400_clk", .clk = &clk_cpupll_ch[1]},
	{.con_id = "nic400_clk", .clk = &clk_cpupll_ch[2]},
};

void dmu_gpiomux_init(void)
{
#if defined(CONFIG_PLAT_MUX_CONSOLE) || defined(CONFIG_PLAT_MUX_CONSOLE_CCB) || \
	defined(CONFIG_SOUND) || defined(CONFIG_SPI_BCM5301X) || defined(CONFIG_I2C_BCM5301X)
	void * __iomem reg_addr;
	u32 reg;

	/* CRU_RESET register */
	reg_addr = (void *)(SOC_DMU_BASE_VA + 0x1c0);
#endif /* CONFIG_PLAT_MUX_CONSOLE || CONFIG_PLAT_MUX_CONSOLE_CCB || CONFIG_SOUND || CONFIG_SPI_BCM5301X || CONFIG_I2C_BCM5301X */

#ifdef CONFIG_PLAT_MUX_CONSOLE
	/* set iproc_reset_n to 0 to use UART1, but it never comes back */
	reg = readl(reg_addr);
	reg &= ~((u32)0xf << 12);
	writel(reg, reg_addr);
#endif /* CONFIG_PLAT_MUX_CONSOLE */
#ifdef CONFIG_PLAT_MUX_CONSOLE_CCB
	/* UART port 2 (ChipCommonB UART0) is multiplexed with GPIO[16:17] */
	reg = readl(reg_addr);
	reg &= ~((u32)0x3 << 16);
	writel(reg, reg_addr);
#endif /* CONFIG_PLAT_MUX_CONSOLE_CCB */	
#ifdef CONFIG_SOUND
	/* I2S XTAL out */
	reg = readl(reg_addr);
	reg &= ~((u32)0x1 << 18);
	writel(reg, reg_addr);
#endif /* CONFIG_SOUND */
#ifdef CONFIG_SPI_BCM5301X
	/* SPI out */
	reg = readl(reg_addr);
	reg &= ~((u32)0xF << 0);
	writel(reg, reg_addr);
#endif /* CONFIG_SPI_BCM5301X */
#ifdef CONFIG_I2C_BCM5301X
	/* I2C out */
	reg = readl(reg_addr);
	reg &= ~((u32)0x3 << 4);
	writel(reg, reg_addr);
#endif /* CONFIG_I2C_BCM5301X */
}

/*
 * Install above clocks into clock lookup table
 * and initialize the register base address for each
*/
static void __init soc_clocks_init(void * __iomem cru_regs_base,
	struct clk * clk_ref)
{
	void * __iomem reg;
	u32 val;

	/* registers are already mapped with the rest of DMU block */
	/* Update register base address */
	clk_lcpll.regs_base = cru_regs_base + 0x00;
	clk_genpll.regs_base = cru_regs_base + 0x40;

	/* Set parent as reference ckock */
	clk_lcpll.parent = clk_ref;
	clk_genpll.parent = clk_ref;

#ifdef	__DEPRECATED__
	{
	int i;
	/* We need to clear dev_id fields in the lookups,
	 * because if it is set, it will not match by con_id
	 */
	for (i = 0; i < ARRAY_SIZE(soc_clk_lookups); i++)
		soc_clk_lookups[i].dev_id = NULL;
	}
#endif

	/* Install clock sources into the lookup table */
	clkdev_add_table(soc_clk_lookups, ARRAY_SIZE(soc_clk_lookups));

	/* Correct GMAC 2.66G line rate issue, it should be 2Gbps */
	/* This incorrect setting only exist in OTP present 4708 chip */
	/* is a OTPed 4708 chip which Ndiv == 0x50 */
	reg = clk_genpll.regs_base + 0x14;
	val = readl(reg);
	if (((val >> 20) & 0x3ff) == 0x50) {
		/* CRU_CLKSET_KEY, unlock */
		reg = clk_genpll.regs_base + 0x40;
		val = 0x0000ea68;
		writel(val, reg);

		/* Change CH0_MDIV to 8 */
		/* After changing the CH0_MDIV to 8, the customer has been reporting that
		 * there are differences between input throughput vs. output throughput.
		 * The output throughput is slightly lower (927.537 mbps input rate vs. 927.49
		 * mbps output rate).  Below is the solution to fix it.
		 * 1. Change the oscillator on WLAN reference board from 25.000 to 25.001
		 * 2. Change the CH0_MDIV to 7
		 */
		reg = clk_genpll.regs_base + 0x18;
		val = readl(reg);
		val &= ~((u32)0xff << 16);
		val |= ((u32)0x7 << 16);
		writel(val, reg);

		/* Load Enable CH0 */
		reg = clk_genpll.regs_base + 0x4;
		val = readl(reg);
		val &= ~(u32)0x1;
		writel(val, reg);
		val |= (u32)0x1;
		writel(val, reg);
		val &= ~(u32)0x1;
		writel(val, reg);

		/* CRU_CLKSET_KEY, lock */
		reg = clk_genpll.regs_base + 0x40;
		val = 0x0;
		writel(val, reg);
	}
}

void __init soc_dmu_init(struct clk *clk_ref)
{
	void * __iomem 	reg_base;

	if (IS_ERR_OR_NULL(clk_ref)) {
		printk(KERN_ERR "DMU no clock source - skip init\n");
		return;
	}

	BUG_ON(request_resource(&iomem_resource, &dmu_regs));

	/* DMU regs are mapped as part of the fixed mapping with CCA+CCB */
	reg_base = (void *)SOC_DMU_BASE_VA;

	BUG_ON(IS_ERR_OR_NULL(reg_base));

	/* Initialize clocks */
	soc_clocks_init(reg_base + 0x100, clk_ref); /* CRU LCPLL control0 */

	dmu_gpiomux_init();
}

/* For BCM53573 PMU PLL clocks */
void __init soc_pmu_clk_init(struct clk *clk_ref)
{
	void * __iomem reg_base;

	if (IS_ERR_OR_NULL(clk_ref)) {
		printk(KERN_ERR "PMU no clock source - skip init\n");
		return;
	}

	BUG_ON(request_resource(&iomem_resource, &pmu_regs));

	/* PMU regs are mapped as part of the fixed mapping with ChipCommon */
	reg_base = (void *)SOC_PMU_BASE_VA;

	BUG_ON(IS_ERR_OR_NULL(reg_base));

	/* For PMU CPUPLL */
	clk_cpupll.regs_base = reg_base;
	clk_cpupll.parent = clk_ref;

	/* Install clock sources into the lookup table */
	clkdev_add_table(pmu_clk_lookups, ARRAY_SIZE(pmu_clk_lookups));
}






void soc_clocks_show(void)
{
	unsigned i;
	struct clk_lookup *clk_lookup = soc_clk_lookups;
	unsigned clk_lookup_sz = ARRAY_SIZE(soc_clk_lookups);
	if (BCM53573_CHIP(sih->chip)) {
		clk_lookup = pmu_clk_lookups;
		clk_lookup_sz = ARRAY_SIZE(pmu_clk_lookups);
	}
	printk("SoC Clocks:\n");
	for (i = 0; i < clk_lookup_sz; i++) {
		printk("%s, %s: (%s) %lu\n",
			clk_lookup[i].con_id,
			clk_lookup[i].dev_id,
			clk_lookup[i].clk->name,
			clk_get_rate(clk_lookup[i].clk));
	}

	printk("SoC Clocks# %u\n", i);
}

#ifdef CONFIG_PROC_FS
static int dmu_temperature_status(char * buffer, char **start,
	off_t offset, int length, int * eof, void * data)
{
	int len;
	off_t pos, begin;
	void *__iomem pvtmon_base;
	u32 pvtmon_control0, pvtmon_status;
	int temperature;

	len = 0;
	pos = begin = 0;

	pvtmon_base = (void *)(SOC_DMU_BASE_VA + 0x2c0);	/* PVTMON control0 */

	pvtmon_control0 = readl(pvtmon_base);
	if (pvtmon_control0 & 0xf) {
		pvtmon_control0 &= ~0xf;
		writel(pvtmon_control0, pvtmon_base);
	}

	pvtmon_status = readl(pvtmon_base + 0x8);
	temperature = 418 - ((5556 * pvtmon_status) / 10000);

	len += sprintf(buffer + len, "CPU temperature\t: %d%cC\n\n",
		temperature, 0xF8);

	pos = begin + len;

	if (pos < offset) {
		len = 0;
		begin = pos;
	}

	*eof = 1;

	*start = buffer + (offset - begin);
	len -= (offset - begin);

	if (len > length)
		len = length;

	return len;
}

static void __init dmu_proc_init(void)
{
	struct proc_dir_entry *dmu, *dmu_temp;
	if (BCM53573_CHIP(sih->chip)) {
		return;
	}
	dmu = proc_mkdir(DMU_PROC_NAME, NULL);

	if (!dmu) {
		printk(KERN_ERR "DMU create proc directory failed.\n");
		return;
	}

	dmu_temp = create_proc_read_entry(DMU_PROC_NAME "/temperature", 0, NULL,
		dmu_temperature_status, NULL);

	if (!dmu_temp)
		printk(KERN_ERR "DMU create proc entry failed.\n");
}
fs_initcall(dmu_proc_init);
#endif /* CONFIG_PROC_FS */
