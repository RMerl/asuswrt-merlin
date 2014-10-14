#ifndef __SH_CLOCK_H
#define __SH_CLOCK_H

#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/cpufreq.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/clk.h>
#include <linux/err.h>

struct clk;

struct clk_mapping {
	phys_addr_t		phys;
	void __iomem		*base;
	unsigned long		len;
	struct kref		ref;
};

struct clk_ops {
#ifdef CONFIG_SH_CLK_CPG_LEGACY
	void (*init)(struct clk *clk);
#endif
	int (*enable)(struct clk *clk);
	void (*disable)(struct clk *clk);
	unsigned long (*recalc)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	long (*round_rate)(struct clk *clk, unsigned long rate);
};

struct clk {
	struct list_head	node;
	struct clk		*parent;
	struct clk		**parent_table;	/* list of parents to */
	unsigned short		parent_num;	/* choose between */
	unsigned char		src_shift;	/* source clock field in the */
	unsigned char		src_width;	/* configuration register */
	struct clk_ops		*ops;

	struct list_head	children;
	struct list_head	sibling;	/* node for children */

	int			usecount;

	unsigned long		rate;
	unsigned long		flags;

	void __iomem		*enable_reg;
	unsigned int		enable_bit;

	unsigned long		arch_flags;
	void			*priv;
	struct dentry		*dentry;
	struct clk_mapping	*mapping;
	struct cpufreq_frequency_table *freq_table;
	unsigned int		nr_freqs;
};

#define CLK_ENABLE_ON_INIT	(1 << 0)

/* drivers/sh/clk.c */
unsigned long followparent_recalc(struct clk *);
void recalculate_root_clocks(void);
void propagate_rate(struct clk *);
int clk_reparent(struct clk *child, struct clk *parent);
int clk_register(struct clk *);
void clk_unregister(struct clk *);
void clk_enable_init_clocks(void);

struct clk_div_mult_table {
	unsigned int *divisors;
	unsigned int nr_divisors;
	unsigned int *multipliers;
	unsigned int nr_multipliers;
};

struct cpufreq_frequency_table;
void clk_rate_table_build(struct clk *clk,
			  struct cpufreq_frequency_table *freq_table,
			  int nr_freqs,
			  struct clk_div_mult_table *src_table,
			  unsigned long *bitmap);

long clk_rate_table_round(struct clk *clk,
			  struct cpufreq_frequency_table *freq_table,
			  unsigned long rate);

int clk_rate_table_find(struct clk *clk,
			struct cpufreq_frequency_table *freq_table,
			unsigned long rate);

long clk_rate_div_range_round(struct clk *clk, unsigned int div_min,
			      unsigned int div_max, unsigned long rate);

long clk_round_parent(struct clk *clk, unsigned long target,
		      unsigned long *best_freq, unsigned long *parent_freq,
		      unsigned int div_min, unsigned int div_max);

#define SH_CLK_MSTP32(_parent, _enable_reg, _enable_bit, _flags)	\
{									\
	.parent		= _parent,					\
	.enable_reg	= (void __iomem *)_enable_reg,			\
	.enable_bit	= _enable_bit,					\
	.flags		= _flags,					\
}

int sh_clk_mstp32_register(struct clk *clks, int nr);

#define SH_CLK_DIV4(_parent, _reg, _shift, _div_bitmap, _flags)	\
{								\
	.parent = _parent,					\
	.enable_reg = (void __iomem *)_reg,			\
	.enable_bit = _shift,					\
	.arch_flags = _div_bitmap,				\
	.flags = _flags,					\
}

struct clk_div4_table {
	struct clk_div_mult_table *div_mult_table;
	void (*kick)(struct clk *clk);
};

int sh_clk_div4_register(struct clk *clks, int nr,
			 struct clk_div4_table *table);
int sh_clk_div4_enable_register(struct clk *clks, int nr,
			 struct clk_div4_table *table);
int sh_clk_div4_reparent_register(struct clk *clks, int nr,
			 struct clk_div4_table *table);

#define SH_CLK_DIV6_EXT(_parent, _reg, _flags, _parents,	\
			_num_parents, _src_shift, _src_width)	\
{								\
	.parent = _parent,					\
	.enable_reg = (void __iomem *)_reg,			\
	.flags = _flags,					\
	.parent_table = _parents,				\
	.parent_num = _num_parents,				\
	.src_shift = _src_shift,				\
	.src_width = _src_width,				\
}

#define SH_CLK_DIV6(_parent, _reg, _flags)			\
	SH_CLK_DIV6_EXT(_parent, _reg, _flags, NULL, 0, 0, 0)

int sh_clk_div6_register(struct clk *clks, int nr);
int sh_clk_div6_reparent_register(struct clk *clks, int nr);

#endif /* __SH_CLOCK_H */
