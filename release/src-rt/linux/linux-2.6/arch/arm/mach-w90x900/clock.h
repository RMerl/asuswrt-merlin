/*
 * linux/arch/arm/mach-w90x900/clock.h
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

#include <linux/clkdev.h>

void nuc900_clk_enable(struct clk *clk, int enable);
void nuc900_subclk_enable(struct clk *clk, int enable);

struct clk {
	unsigned long		cken;
	unsigned int		enabled;
	void			(*enable)(struct clk *, int enable);
};

#define DEFINE_CLK(_name, _ctrlbit)			\
struct clk clk_##_name = {				\
		.enable	= nuc900_clk_enable,		\
		.cken	= (1 << _ctrlbit),		\
	}

#define DEFINE_SUBCLK(_name, _ctrlbit)			\
struct clk clk_##_name = {				\
		.enable	= nuc900_subclk_enable,	\
		.cken	= (1 << _ctrlbit),		\
	}


#define DEF_CLKLOOK(_clk, _devname, _conname)		\
	{						\
		.clk		= _clk,			\
		.dev_id		= _devname,		\
		.con_id		= _conname,		\
	}

