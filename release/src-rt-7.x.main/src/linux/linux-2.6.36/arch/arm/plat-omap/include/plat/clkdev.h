#ifndef __MACH_CLKDEV_H
#define __MACH_CLKDEV_H

static inline int __clk_get(struct clk *clk)
{
	return 1;
}

static inline void __clk_put(struct clk *clk)
{
}

#endif
