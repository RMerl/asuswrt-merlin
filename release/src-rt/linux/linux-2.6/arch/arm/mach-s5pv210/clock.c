/* linux/arch/arm/mach-s5pv210/clock.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/sysdev.h>
#include <linux/io.h>

#include <mach/map.h>

#include <plat/cpu-freq.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/s5pv210.h>

static unsigned long xtal;

static struct clksrc_clk clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
		.id		= -1,
	},
	.sources	= &clk_src_apll,
	.reg_src	= { .reg = S5P_CLK_SRC0, .shift = 0, .size = 1 },
};

static struct clksrc_clk clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
		.id		= -1,
	},
	.sources	= &clk_src_epll,
	.reg_src	= { .reg = S5P_CLK_SRC0, .shift = 8, .size = 1 },
};

static struct clksrc_clk clk_mout_mpll = {
	.clk = {
		.name		= "mout_mpll",
		.id		= -1,
	},
	.sources	= &clk_src_mpll,
	.reg_src	= { .reg = S5P_CLK_SRC0, .shift = 4, .size = 1 },
};

static struct clk *clkset_armclk_list[] = {
	[0] = &clk_mout_apll.clk,
	[1] = &clk_mout_mpll.clk,
};

static struct clksrc_sources clkset_armclk = {
	.sources	= clkset_armclk_list,
	.nr_sources	= ARRAY_SIZE(clkset_armclk_list),
};

static struct clksrc_clk clk_armclk = {
	.clk	= {
		.name		= "armclk",
		.id		= -1,
	},
	.sources	= &clkset_armclk,
	.reg_src	= { .reg = S5P_CLK_SRC0, .shift = 16, .size = 1 },
	.reg_div	= { .reg = S5P_CLK_DIV0, .shift = 0, .size = 3 },
};

static struct clksrc_clk clk_hclk_msys = {
	.clk	= {
		.name		= "hclk_msys",
		.id		= -1,
		.parent		= &clk_armclk.clk,
	},
	.reg_div	= { .reg = S5P_CLK_DIV0, .shift = 8, .size = 3 },
};

static struct clksrc_clk clk_pclk_msys = {
	.clk	= {
		.name		= "pclk_msys",
		.id		= -1,
		.parent		= &clk_hclk_msys.clk,
	},
	.reg_div        = { .reg = S5P_CLK_DIV0, .shift = 12, .size = 3 },
};

static struct clksrc_clk clk_sclk_a2m = {
	.clk	= {
		.name		= "sclk_a2m",
		.id		= -1,
		.parent		= &clk_mout_apll.clk,
	},
	.reg_div	= { .reg = S5P_CLK_DIV0, .shift = 4, .size = 3 },
};

static struct clk *clkset_hclk_sys_list[] = {
	[0] = &clk_mout_mpll.clk,
	[1] = &clk_sclk_a2m.clk,
};

static struct clksrc_sources clkset_hclk_sys = {
	.sources	= clkset_hclk_sys_list,
	.nr_sources	= ARRAY_SIZE(clkset_hclk_sys_list),
};

static struct clksrc_clk clk_hclk_dsys = {
	.clk	= {
		.name	= "hclk_dsys",
		.id	= -1,
	},
	.sources	= &clkset_hclk_sys,
	.reg_src        = { .reg = S5P_CLK_SRC0, .shift = 20, .size = 1 },
	.reg_div        = { .reg = S5P_CLK_DIV0, .shift = 16, .size = 4 },
};

static struct clksrc_clk clk_pclk_dsys = {
	.clk	= {
		.name	= "pclk_dsys",
		.id	= -1,
		.parent	= &clk_hclk_dsys.clk,
	},
	.reg_div = { .reg = S5P_CLK_DIV0, .shift = 20, .size = 3 },
};

static struct clksrc_clk clk_hclk_psys = {
	.clk	= {
		.name	= "hclk_psys",
		.id	= -1,
	},
	.sources	= &clkset_hclk_sys,
	.reg_src        = { .reg = S5P_CLK_SRC0, .shift = 24, .size = 1 },
	.reg_div        = { .reg = S5P_CLK_DIV0, .shift = 24, .size = 4 },
};

static struct clksrc_clk clk_pclk_psys = {
	.clk	= {
		.name	= "pclk_psys",
		.id	= -1,
		.parent	= &clk_hclk_psys.clk,
	},
	.reg_div        = { .reg = S5P_CLK_DIV0, .shift = 28, .size = 3 },
};

static int s5pv210_clk_ip0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLKGATE_IP0, clk, enable);
}

static int s5pv210_clk_ip1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLKGATE_IP1, clk, enable);
}

static int s5pv210_clk_ip2_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLKGATE_IP2, clk, enable);
}

static int s5pv210_clk_ip3_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLKGATE_IP3, clk, enable);
}

static int s5pv210_clk_mask0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLK_SRC_MASK0, clk, enable);
}

static int s5pv210_clk_mask1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLK_SRC_MASK1, clk, enable);
}

static struct clk clk_sclk_hdmi27m = {
	.name		= "sclk_hdmi27m",
	.id		= -1,
	.rate		= 27000000,
};

static struct clk clk_sclk_hdmiphy = {
	.name		= "sclk_hdmiphy",
	.id		= -1,
};

static struct clk clk_sclk_usbphy0 = {
	.name		= "sclk_usbphy0",
	.id		= -1,
};

static struct clk clk_sclk_usbphy1 = {
	.name		= "sclk_usbphy1",
	.id		= -1,
};

static struct clk clk_pcmcdclk0 = {
	.name		= "pcmcdclk",
	.id		= -1,
};

static struct clk clk_pcmcdclk1 = {
	.name		= "pcmcdclk",
	.id		= -1,
};

static struct clk clk_pcmcdclk2 = {
	.name		= "pcmcdclk",
	.id		= -1,
};

static struct clk *clkset_vpllsrc_list[] = {
	[0] = &clk_fin_vpll,
	[1] = &clk_sclk_hdmi27m,
};

static struct clksrc_sources clkset_vpllsrc = {
	.sources	= clkset_vpllsrc_list,
	.nr_sources	= ARRAY_SIZE(clkset_vpllsrc_list),
};

static struct clksrc_clk clk_vpllsrc = {
	.clk	= {
		.name		= "vpll_src",
		.id		= -1,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 7),
	},
	.sources	= &clkset_vpllsrc,
	.reg_src	= { .reg = S5P_CLK_SRC1, .shift = 28, .size = 1 },
};

static struct clk *clkset_sclk_vpll_list[] = {
	[0] = &clk_vpllsrc.clk,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources clkset_sclk_vpll = {
	.sources	= clkset_sclk_vpll_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_vpll_list),
};

static struct clksrc_clk clk_sclk_vpll = {
	.clk	= {
		.name		= "sclk_vpll",
		.id		= -1,
	},
	.sources	= &clkset_sclk_vpll,
	.reg_src	= { .reg = S5P_CLK_SRC0, .shift = 12, .size = 1 },
};

static struct clk *clkset_moutdmc0src_list[] = {
	[0] = &clk_sclk_a2m.clk,
	[1] = &clk_mout_mpll.clk,
	[2] = NULL,
	[3] = NULL,
};

static struct clksrc_sources clkset_moutdmc0src = {
	.sources	= clkset_moutdmc0src_list,
	.nr_sources	= ARRAY_SIZE(clkset_moutdmc0src_list),
};

static struct clksrc_clk clk_mout_dmc0 = {
	.clk	= {
		.name		= "mout_dmc0",
		.id		= -1,
	},
	.sources	= &clkset_moutdmc0src,
	.reg_src	= { .reg = S5P_CLK_SRC6, .shift = 24, .size = 2 },
};

static struct clksrc_clk clk_sclk_dmc0 = {
	.clk	= {
		.name		= "sclk_dmc0",
		.id		= -1,
		.parent		= &clk_mout_dmc0.clk,
	},
	.reg_div	= { .reg = S5P_CLK_DIV6, .shift = 28, .size = 4 },
};

static unsigned long s5pv210_clk_imem_get_rate(struct clk *clk)
{
	return clk_get_rate(clk->parent) / 2;
}

static struct clk_ops clk_hclk_imem_ops = {
	.get_rate	= s5pv210_clk_imem_get_rate,
};

static unsigned long s5pv210_clk_fout_apll_get_rate(struct clk *clk)
{
	return s5p_get_pll45xx(xtal, __raw_readl(S5P_APLL_CON), pll_4508);
}

static struct clk_ops clk_fout_apll_ops = {
	.get_rate	= s5pv210_clk_fout_apll_get_rate,
};

static struct clk init_clocks_off[] = {
	{
		.name		= "pdma",
		.id		= 0,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "pdma",
		.id		= 1,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "rot",
		.id		= -1,
		.parent		= &clk_hclk_dsys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1<<29),
	}, {
		.name		= "fimc",
		.id		= 0,
		.parent		= &clk_hclk_dsys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "fimc",
		.id		= 1,
		.parent		= &clk_hclk_dsys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1 << 25),
	}, {
		.name		= "fimc",
		.id		= 2,
		.parent		= &clk_hclk_dsys.clk,
		.enable		= s5pv210_clk_ip0_ctrl,
		.ctrlbit	= (1 << 26),
	}, {
		.name		= "otg",
		.id		= -1,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip1_ctrl,
		.ctrlbit	= (1<<16),
	}, {
		.name		= "usb-host",
		.id		= -1,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip1_ctrl,
		.ctrlbit	= (1<<17),
	}, {
		.name		= "lcd",
		.id		= -1,
		.parent		= &clk_hclk_dsys.clk,
		.enable		= s5pv210_clk_ip1_ctrl,
		.ctrlbit	= (1<<0),
	}, {
		.name		= "cfcon",
		.id		= 0,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip1_ctrl,
		.ctrlbit	= (1<<25),
	}, {
		.name		= "hsmmc",
		.id		= 0,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip2_ctrl,
		.ctrlbit	= (1<<16),
	}, {
		.name		= "hsmmc",
		.id		= 1,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip2_ctrl,
		.ctrlbit	= (1<<17),
	}, {
		.name		= "hsmmc",
		.id		= 2,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip2_ctrl,
		.ctrlbit	= (1<<18),
	}, {
		.name		= "hsmmc",
		.id		= 3,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip2_ctrl,
		.ctrlbit	= (1<<19),
	}, {
		.name		= "systimer",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<16),
	}, {
		.name		= "watchdog",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<22),
	}, {
		.name		= "rtc",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<15),
	}, {
		.name		= "i2c",
		.id		= 0,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<7),
	}, {
		.name		= "i2c",
		.id		= 1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "i2c",
		.id		= 2,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<9),
	}, {
		.name		= "spi",
		.id		= 0,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<12),
	}, {
		.name		= "spi",
		.id		= 1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<13),
	}, {
		.name		= "spi",
		.id		= 2,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<14),
	}, {
		.name		= "timers",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<23),
	}, {
		.name		= "adc",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<24),
	}, {
		.name		= "keypad",
		.id		= -1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<21),
	}, {
		.name		= "iis",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1<<4),
	}, {
		.name		= "iis",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "iis",
		.id		= 2,
		.parent		= &clk_p,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "spdif",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 0),
	},
};

static struct clk init_clocks[] = {
	{
		.name		= "hclk_imem",
		.id		= -1,
		.parent		= &clk_hclk_msys.clk,
		.ctrlbit	= (1 << 5),
		.enable		= s5pv210_clk_ip0_ctrl,
		.ops		= &clk_hclk_imem_ops,
	}, {
		.name		= "uart",
		.id		= 0,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "uart",
		.id		= 1,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "uart",
		.id		= 2,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 19),
	}, {
		.name		= "uart",
		.id		= 3,
		.parent		= &clk_pclk_psys.clk,
		.enable		= s5pv210_clk_ip3_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "sromc",
		.id		= -1,
		.parent		= &clk_hclk_psys.clk,
		.enable		= s5pv210_clk_ip1_ctrl,
		.ctrlbit	= (1 << 26),
	},
};

static struct clk *clkset_uart_list[] = {
	[6] = &clk_mout_mpll.clk,
	[7] = &clk_mout_epll.clk,
};

static struct clksrc_sources clkset_uart = {
	.sources	= clkset_uart_list,
	.nr_sources	= ARRAY_SIZE(clkset_uart_list),
};

static struct clk *clkset_group1_list[] = {
	[0] = &clk_sclk_a2m.clk,
	[1] = &clk_mout_mpll.clk,
	[2] = &clk_mout_epll.clk,
	[3] = &clk_sclk_vpll.clk,
};

static struct clksrc_sources clkset_group1 = {
	.sources	= clkset_group1_list,
	.nr_sources	= ARRAY_SIZE(clkset_group1_list),
};

static struct clk *clkset_sclk_onenand_list[] = {
	[0] = &clk_hclk_psys.clk,
	[1] = &clk_hclk_dsys.clk,
};

static struct clksrc_sources clkset_sclk_onenand = {
	.sources	= clkset_sclk_onenand_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_onenand_list),
};

static struct clk *clkset_sclk_dac_list[] = {
	[0] = &clk_sclk_vpll.clk,
	[1] = &clk_sclk_hdmiphy,
};

static struct clksrc_sources clkset_sclk_dac = {
	.sources	= clkset_sclk_dac_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_dac_list),
};

static struct clksrc_clk clk_sclk_dac = {
	.clk		= {
		.name		= "sclk_dac",
		.id		= -1,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 2),
	},
	.sources	= &clkset_sclk_dac,
	.reg_src	= { .reg = S5P_CLK_SRC1, .shift = 8, .size = 1 },
};

static struct clksrc_clk clk_sclk_pixel = {
	.clk		= {
		.name		= "sclk_pixel",
		.id		= -1,
		.parent		= &clk_sclk_vpll.clk,
	},
	.reg_div	= { .reg = S5P_CLK_DIV1, .shift = 0, .size = 4},
};

static struct clk *clkset_sclk_hdmi_list[] = {
	[0] = &clk_sclk_pixel.clk,
	[1] = &clk_sclk_hdmiphy,
};

static struct clksrc_sources clkset_sclk_hdmi = {
	.sources	= clkset_sclk_hdmi_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_hdmi_list),
};

static struct clksrc_clk clk_sclk_hdmi = {
	.clk		= {
		.name		= "sclk_hdmi",
		.id		= -1,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources	= &clkset_sclk_hdmi,
	.reg_src	= { .reg = S5P_CLK_SRC1, .shift = 0, .size = 1 },
};

static struct clk *clkset_sclk_mixer_list[] = {
	[0] = &clk_sclk_dac.clk,
	[1] = &clk_sclk_hdmi.clk,
};

static struct clksrc_sources clkset_sclk_mixer = {
	.sources	= clkset_sclk_mixer_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_mixer_list),
};

static struct clk *clkset_sclk_audio0_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_pcmcdclk0,
	[2] = &clk_sclk_hdmi27m,
	[3] = &clk_sclk_usbphy0,
	[4] = &clk_sclk_usbphy1,
	[5] = &clk_sclk_hdmiphy,
	[6] = &clk_mout_mpll.clk,
	[7] = &clk_mout_epll.clk,
	[8] = &clk_sclk_vpll.clk,
};

static struct clksrc_sources clkset_sclk_audio0 = {
	.sources	= clkset_sclk_audio0_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_audio0_list),
};

static struct clksrc_clk clk_sclk_audio0 = {
	.clk		= {
		.name		= "sclk_audio",
		.id		= 0,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 24),
	},
	.sources = &clkset_sclk_audio0,
	.reg_src = { .reg = S5P_CLK_SRC6, .shift = 0, .size = 4 },
	.reg_div = { .reg = S5P_CLK_DIV6, .shift = 0, .size = 4 },
};

static struct clk *clkset_sclk_audio1_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_pcmcdclk1,
	[2] = &clk_sclk_hdmi27m,
	[3] = &clk_sclk_usbphy0,
	[4] = &clk_sclk_usbphy1,
	[5] = &clk_sclk_hdmiphy,
	[6] = &clk_mout_mpll.clk,
	[7] = &clk_mout_epll.clk,
	[8] = &clk_sclk_vpll.clk,
};

static struct clksrc_sources clkset_sclk_audio1 = {
	.sources	= clkset_sclk_audio1_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_audio1_list),
};

static struct clksrc_clk clk_sclk_audio1 = {
	.clk		= {
		.name		= "sclk_audio",
		.id		= 1,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 25),
	},
	.sources = &clkset_sclk_audio1,
	.reg_src = { .reg = S5P_CLK_SRC6, .shift = 4, .size = 4 },
	.reg_div = { .reg = S5P_CLK_DIV6, .shift = 4, .size = 4 },
};

static struct clk *clkset_sclk_audio2_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_pcmcdclk0,
	[2] = &clk_sclk_hdmi27m,
	[3] = &clk_sclk_usbphy0,
	[4] = &clk_sclk_usbphy1,
	[5] = &clk_sclk_hdmiphy,
	[6] = &clk_mout_mpll.clk,
	[7] = &clk_mout_epll.clk,
	[8] = &clk_sclk_vpll.clk,
};

static struct clksrc_sources clkset_sclk_audio2 = {
	.sources	= clkset_sclk_audio2_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_audio2_list),
};

static struct clksrc_clk clk_sclk_audio2 = {
	.clk		= {
		.name		= "sclk_audio",
		.id		= 2,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 26),
	},
	.sources = &clkset_sclk_audio2,
	.reg_src = { .reg = S5P_CLK_SRC6, .shift = 8, .size = 4 },
	.reg_div = { .reg = S5P_CLK_DIV6, .shift = 8, .size = 4 },
};

static struct clk *clkset_sclk_spdif_list[] = {
	[0] = &clk_sclk_audio0.clk,
	[1] = &clk_sclk_audio1.clk,
	[2] = &clk_sclk_audio2.clk,
};

static struct clksrc_sources clkset_sclk_spdif = {
	.sources	= clkset_sclk_spdif_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_spdif_list),
};

static int s5pv210_spdif_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk *pclk;
	int ret;

	pclk = clk_get_parent(clk);
	if (IS_ERR(pclk))
		return -EINVAL;

	ret = pclk->ops->set_rate(pclk, rate);
	clk_put(pclk);

	return ret;
}

static unsigned long s5pv210_spdif_get_rate(struct clk *clk)
{
	struct clk *pclk;
	int rate;

	pclk = clk_get_parent(clk);
	if (IS_ERR(pclk))
		return -EINVAL;

	rate = pclk->ops->get_rate(clk);
	clk_put(pclk);

	return rate;
}

static struct clk_ops s5pv210_sclk_spdif_ops = {
	.set_rate	= s5pv210_spdif_set_rate,
	.get_rate	= s5pv210_spdif_get_rate,
};

static struct clksrc_clk clk_sclk_spdif = {
	.clk		= {
		.name		= "sclk_spdif",
		.id		= -1,
		.enable		= s5pv210_clk_mask0_ctrl,
		.ctrlbit	= (1 << 27),
		.ops		= &s5pv210_sclk_spdif_ops,
	},
	.sources = &clkset_sclk_spdif,
	.reg_src = { .reg = S5P_CLK_SRC6, .shift = 12, .size = 2 },
};

static struct clk *clkset_group2_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = &clk_sclk_hdmi27m,
	[3] = &clk_sclk_usbphy0,
	[4] = &clk_sclk_usbphy1,
	[5] = &clk_sclk_hdmiphy,
	[6] = &clk_mout_mpll.clk,
	[7] = &clk_mout_epll.clk,
	[8] = &clk_sclk_vpll.clk,
};

static struct clksrc_sources clkset_group2 = {
	.sources	= clkset_group2_list,
	.nr_sources	= ARRAY_SIZE(clkset_group2_list),
};

static struct clksrc_clk clksrcs[] = {
	{
		.clk	= {
			.name		= "sclk_dmc",
			.id		= -1,
		},
		.sources = &clkset_group1,
		.reg_src = { .reg = S5P_CLK_SRC6, .shift = 24, .size = 2 },
		.reg_div = { .reg = S5P_CLK_DIV6, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_onenand",
			.id		= -1,
		},
		.sources = &clkset_sclk_onenand,
		.reg_src = { .reg = S5P_CLK_SRC0, .shift = 28, .size = 1 },
		.reg_div = { .reg = S5P_CLK_DIV6, .shift = 12, .size = 3 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.id		= 0,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &clkset_uart,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 16, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 16, .size = 4 },
	}, {
		.clk		= {
			.name		= "uclk1",
			.id		= 1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 13),
		},
		.sources = &clkset_uart,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 20, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 20, .size = 4 },
	}, {
		.clk		= {
			.name		= "uclk1",
			.id		= 2,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 14),
		},
		.sources = &clkset_uart,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 24, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 24, .size = 4 },
	}, {
		.clk		= {
			.name		= "uclk1",
			.id		= 3,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 15),
		},
		.sources = &clkset_uart,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 28, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mixer",
			.id		= -1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 1),
		},
		.sources = &clkset_sclk_mixer,
		.reg_src = { .reg = S5P_CLK_SRC1, .shift = 4, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.id		= 0,
			.enable		= s5pv210_clk_mask1_ctrl,
			.ctrlbit	= (1 << 2),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC3, .shift = 12, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV3, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.id		= 1,
			.enable		= s5pv210_clk_mask1_ctrl,
			.ctrlbit	= (1 << 3),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC3, .shift = 16, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV3, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.id		= 2,
			.enable		= s5pv210_clk_mask1_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC3, .shift = 20, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV3, .shift = 20, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_cam",
			.id		= 0,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 3),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC1, .shift = 12, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV1, .shift = 12, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_cam",
			.id		= 1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC1, .shift = 16, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV1, .shift = 16, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_fimd",
			.id		= -1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 5),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC1, .shift = 20, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV1, .shift = 20, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_mmc",
			.id		= 0,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 0, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 0, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_mmc",
			.id		= 1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 9),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 4, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 4, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_mmc",
			.id		= 2,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 10),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 8, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 8, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_mmc",
			.id		= 3,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 11),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC4, .shift = 12, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV4, .shift = 12, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_mfc",
			.id		= -1,
			.enable		= s5pv210_clk_ip0_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.sources = &clkset_group1,
		.reg_src = { .reg = S5P_CLK_SRC2, .shift = 4, .size = 2 },
		.reg_div = { .reg = S5P_CLK_DIV2, .shift = 4, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_g2d",
			.id		= -1,
			.enable		= s5pv210_clk_ip0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &clkset_group1,
		.reg_src = { .reg = S5P_CLK_SRC2, .shift = 8, .size = 2 },
		.reg_div = { .reg = S5P_CLK_DIV2, .shift = 8, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_g3d",
			.id		= -1,
			.enable		= s5pv210_clk_ip0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &clkset_group1,
		.reg_src = { .reg = S5P_CLK_SRC2, .shift = 0, .size = 2 },
		.reg_div = { .reg = S5P_CLK_DIV2, .shift = 0, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_csis",
			.id		= -1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 6),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC1, .shift = 24, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV1, .shift = 28, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_spi",
			.id		= 0,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC5, .shift = 0, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV5, .shift = 0, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_spi",
			.id		= 1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 17),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC5, .shift = 4, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV5, .shift = 4, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_pwi",
			.id		= -1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 29),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC6, .shift = 20, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV6, .shift = 24, .size = 4 },
	}, {
		.clk		= {
			.name		= "sclk_pwm",
			.id		= -1,
			.enable		= s5pv210_clk_mask0_ctrl,
			.ctrlbit	= (1 << 19),
		},
		.sources = &clkset_group2,
		.reg_src = { .reg = S5P_CLK_SRC5, .shift = 12, .size = 4 },
		.reg_div = { .reg = S5P_CLK_DIV5, .shift = 12, .size = 4 },
	},
};

/* Clock initialisation code */
static struct clksrc_clk *sysclks[] = {
	&clk_mout_apll,
	&clk_mout_epll,
	&clk_mout_mpll,
	&clk_armclk,
	&clk_hclk_msys,
	&clk_sclk_a2m,
	&clk_hclk_dsys,
	&clk_hclk_psys,
	&clk_pclk_msys,
	&clk_pclk_dsys,
	&clk_pclk_psys,
	&clk_vpllsrc,
	&clk_sclk_vpll,
	&clk_sclk_dac,
	&clk_sclk_pixel,
	&clk_sclk_hdmi,
	&clk_mout_dmc0,
	&clk_sclk_dmc0,
	&clk_sclk_audio0,
	&clk_sclk_audio1,
	&clk_sclk_audio2,
	&clk_sclk_spdif,
};

static u32 epll_div[][6] = {
	{  48000000, 0, 48, 3, 3, 0 },
	{  96000000, 0, 48, 3, 2, 0 },
	{ 144000000, 1, 72, 3, 2, 0 },
	{ 192000000, 0, 48, 3, 1, 0 },
	{ 288000000, 1, 72, 3, 1, 0 },
	{  32750000, 1, 65, 3, 4, 35127 },
	{  32768000, 1, 65, 3, 4, 35127 },
	{  45158400, 0, 45, 3, 3, 10355 },
	{  45000000, 0, 45, 3, 3, 10355 },
	{  45158000, 0, 45, 3, 3, 10355 },
	{  49125000, 0, 49, 3, 3, 9961 },
	{  49152000, 0, 49, 3, 3, 9961 },
	{  67737600, 1, 67, 3, 3, 48366 },
	{  67738000, 1, 67, 3, 3, 48366 },
	{  73800000, 1, 73, 3, 3, 47710 },
	{  73728000, 1, 73, 3, 3, 47710 },
	{  36000000, 1, 32, 3, 4, 0 },
	{  60000000, 1, 60, 3, 3, 0 },
	{  72000000, 1, 72, 3, 3, 0 },
	{  80000000, 1, 80, 3, 3, 0 },
	{  84000000, 0, 42, 3, 2, 0 },
	{  50000000, 0, 50, 3, 3, 0 },
};

static int s5pv210_epll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int epll_con, epll_con_k;
	unsigned int i;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	epll_con = __raw_readl(S5P_EPLL_CON);
	epll_con_k = __raw_readl(S5P_EPLL_CON1);

	epll_con_k &= ~PLL46XX_KDIV_MASK;
	epll_con &= ~(1 << 27 |
			PLL46XX_MDIV_MASK << PLL46XX_MDIV_SHIFT |
			PLL46XX_PDIV_MASK << PLL46XX_PDIV_SHIFT |
			PLL46XX_SDIV_MASK << PLL46XX_SDIV_SHIFT);

	for (i = 0; i < ARRAY_SIZE(epll_div); i++) {
		if (epll_div[i][0] == rate) {
			epll_con_k |= epll_div[i][5] << 0;
			epll_con |= (epll_div[i][1] << 27 |
					epll_div[i][2] << PLL46XX_MDIV_SHIFT |
					epll_div[i][3] << PLL46XX_PDIV_SHIFT |
					epll_div[i][4] << PLL46XX_SDIV_SHIFT);
			break;
		}
	}

	if (i == ARRAY_SIZE(epll_div)) {
		printk(KERN_ERR "%s: Invalid Clock EPLL Frequency\n",
				__func__);
		return -EINVAL;
	}

	__raw_writel(epll_con, S5P_EPLL_CON);
	__raw_writel(epll_con_k, S5P_EPLL_CON1);

	printk(KERN_WARNING "EPLL Rate changes from %lu to %lu\n",
			clk->rate, rate);

	clk->rate = rate;

	return 0;
}

static struct clk_ops s5pv210_epll_ops = {
	.set_rate = s5pv210_epll_set_rate,
	.get_rate = s5p_epll_get_rate,
};

void __init_or_cpufreq s5pv210_setup_clocks(void)
{
	struct clk *xtal_clk;
	unsigned long vpllsrc;
	unsigned long armclk;
	unsigned long hclk_msys;
	unsigned long hclk_dsys;
	unsigned long hclk_psys;
	unsigned long pclk_msys;
	unsigned long pclk_dsys;
	unsigned long pclk_psys;
	unsigned long apll;
	unsigned long mpll;
	unsigned long epll;
	unsigned long vpll;
	unsigned int ptr;
	u32 clkdiv0, clkdiv1;

	/* Set functions for clk_fout_epll */
	clk_fout_epll.enable = s5p_epll_enable;
	clk_fout_epll.ops = &s5pv210_epll_ops;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	clkdiv0 = __raw_readl(S5P_CLK_DIV0);
	clkdiv1 = __raw_readl(S5P_CLK_DIV1);

	printk(KERN_DEBUG "%s: clkdiv0 = %08x, clkdiv1 = %08x\n",
				__func__, clkdiv0, clkdiv1);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);
	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	apll = s5p_get_pll45xx(xtal, __raw_readl(S5P_APLL_CON), pll_4508);
	mpll = s5p_get_pll45xx(xtal, __raw_readl(S5P_MPLL_CON), pll_4502);
	epll = s5p_get_pll46xx(xtal, __raw_readl(S5P_EPLL_CON),
				__raw_readl(S5P_EPLL_CON1), pll_4600);
	vpllsrc = clk_get_rate(&clk_vpllsrc.clk);
	vpll = s5p_get_pll45xx(vpllsrc, __raw_readl(S5P_VPLL_CON), pll_4502);

	clk_fout_apll.ops = &clk_fout_apll_ops;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.rate = vpll;

	printk(KERN_INFO "S5PV210: PLL settings, A=%ld, M=%ld, E=%ld V=%ld",
			apll, mpll, epll, vpll);

	armclk = clk_get_rate(&clk_armclk.clk);
	hclk_msys = clk_get_rate(&clk_hclk_msys.clk);
	hclk_dsys = clk_get_rate(&clk_hclk_dsys.clk);
	hclk_psys = clk_get_rate(&clk_hclk_psys.clk);
	pclk_msys = clk_get_rate(&clk_pclk_msys.clk);
	pclk_dsys = clk_get_rate(&clk_pclk_dsys.clk);
	pclk_psys = clk_get_rate(&clk_pclk_psys.clk);

	printk(KERN_INFO "S5PV210: ARMCLK=%ld, HCLKM=%ld, HCLKD=%ld\n"
			 "HCLKP=%ld, PCLKM=%ld, PCLKD=%ld, PCLKP=%ld\n",
			armclk, hclk_msys, hclk_dsys, hclk_psys,
			pclk_msys, pclk_dsys, pclk_psys);

	clk_f.rate = armclk;
	clk_h.rate = hclk_psys;
	clk_p.rate = pclk_psys;

	for (ptr = 0; ptr < ARRAY_SIZE(clksrcs); ptr++)
		s3c_set_clksrc(&clksrcs[ptr], true);
}

static struct clk *clks[] __initdata = {
	&clk_sclk_hdmi27m,
	&clk_sclk_hdmiphy,
	&clk_sclk_usbphy0,
	&clk_sclk_usbphy1,
	&clk_pcmcdclk0,
	&clk_pcmcdclk1,
	&clk_pcmcdclk2,
};

void __init s5pv210_register_clocks(void)
{
	int ptr;

	s3c24xx_register_clocks(clks, ARRAY_SIZE(clks));

	for (ptr = 0; ptr < ARRAY_SIZE(sysclks); ptr++)
		s3c_register_clksrc(sysclks[ptr], 1);

	s3c_register_clksrc(clksrcs, ARRAY_SIZE(clksrcs));
	s3c_register_clocks(init_clocks, ARRAY_SIZE(init_clocks));

	s3c_register_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));
	s3c_disable_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));

	s3c_pwmclk_init();
}
