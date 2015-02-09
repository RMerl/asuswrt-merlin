#ifndef __ASM_MIPS_CLOCK_H
#define __ASM_MIPS_CLOCK_H

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/clk.h>

extern void (*cpu_wait) (void);

struct clk;

struct clk_ops {
	void (*init) (struct clk *clk);
	void (*enable) (struct clk *clk);
	void (*disable) (struct clk *clk);
	void (*recalc) (struct clk *clk);
	int (*set_rate) (struct clk *clk, unsigned long rate, int algo_id);
	long (*round_rate) (struct clk *clk, unsigned long rate);
};

struct clk {
	struct list_head node;
	const char *name;
	int id;
	struct module *owner;

	struct clk *parent;
	struct clk_ops *ops;

	struct kref kref;

	unsigned long rate;
	unsigned long flags;
};

#define CLK_ALWAYS_ENABLED	(1 << 0)
#define CLK_RATE_PROPAGATES	(1 << 1)

/* Should be defined by processor-specific code */
void arch_init_clk_ops(struct clk_ops **, int type);

int clk_init(void);

int __clk_enable(struct clk *);
void __clk_disable(struct clk *);

void clk_recalc_rate(struct clk *);

int clk_register(struct clk *);
void clk_unregister(struct clk *);

/* the exported API, in addition to clk_set_rate */
/**
 * clk_set_rate_ex - set the clock rate for a clock source, with additional parameter
 * @clk: clock source
 * @rate: desired clock rate in Hz
 * @algo_id: algorithm id to be passed down to ops->set_rate
 *
 * Returns success (0) or negative errno.
 */
int clk_set_rate_ex(struct clk *clk, unsigned long rate, int algo_id);

#endif				/* __ASM_MIPS_CLOCK_H */
