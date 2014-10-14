/* linux/arch/arm/mach-s5p64x0/include/mach/s5p64x0-clock.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for s5p64x0 clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_CLOCK_H
#define __ASM_ARCH_CLOCK_H __FILE__

#include <linux/clk.h>

extern struct clksrc_clk clk_mout_apll;
extern struct clksrc_clk clk_mout_mpll;
extern struct clksrc_clk clk_mout_epll;

extern int s5p64x0_epll_enable(struct clk *clk, int enable);
extern unsigned long s5p64x0_epll_get_rate(struct clk *clk);

extern unsigned long s5p64x0_armclk_get_rate(struct clk *clk);
extern unsigned long s5p64x0_armclk_round_rate(struct clk *clk, unsigned long rate);
extern int s5p64x0_armclk_set_rate(struct clk *clk, unsigned long rate);

extern struct clk_ops s5p64x0_clkarm_ops;

extern struct clksrc_clk clk_armclk;
extern struct clksrc_clk clk_dout_mpll;

extern struct clk *clkset_hclk_low_list[];
extern struct clksrc_sources clkset_hclk_low;

extern int s5p64x0_pclk_ctrl(struct clk *clk, int enable);
extern int s5p64x0_hclk0_ctrl(struct clk *clk, int enable);
extern int s5p64x0_hclk1_ctrl(struct clk *clk, int enable);
extern int s5p64x0_sclk_ctrl(struct clk *clk, int enable);
extern int s5p64x0_sclk1_ctrl(struct clk *clk, int enable);
extern int s5p64x0_mem_ctrl(struct clk *clk, int enable);

extern int s5p64x0_clk48m_ctrl(struct clk *clk, int enable);

#endif /* __ASM_ARCH_CLOCK_H */
