#ifndef __ASM_MACH_CLKDEV_H
#define __ASM_MACH_CLKDEV_H	__FILE__

#include <asm/atomic.h>
#include <plat/clock.h>

struct clk {
	const struct clk_ops *	ops;
	const char * 		name;
	atomic_t		ena_cnt;
	atomic_t		use_cnt;
	unsigned long		rate;
	unsigned		gated :1;
	unsigned		fixed :1;
	unsigned		chan  :6;
	void __iomem *		regs_base;
	struct clk *		parent;
	/* TBD: could it have multiple parents to select from ? */
	enum {
		CLK_XTAL, CLK_GATE, CLK_PLL, CLK_DIV, CLK_PHA
	} type;
};

extern int __clk_get(struct clk *clk);
extern void __clk_put(struct clk *clk);

#endif
