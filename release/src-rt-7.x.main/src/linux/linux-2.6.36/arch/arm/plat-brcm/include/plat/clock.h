#ifndef PLAT_CLOCK_H
#define PLAT_CLOCK_H

/*
 * Operations on clocks -
 * See <linux/clk.h> for description
 */
struct clk_ops {
	int	(* enable)(struct clk *);
	void	(* disable)(struct clk *);
	long	(* round)(struct clk *, unsigned long);
	int	(* setrate)(struct clk *, unsigned long);
	/* Update current rate and return running status */
	int	(* status)(struct clk *);
};


#endif
