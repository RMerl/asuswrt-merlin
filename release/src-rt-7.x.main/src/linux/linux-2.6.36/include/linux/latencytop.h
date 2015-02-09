/*
 * latencytop.h: Infrastructure for displaying latency
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 */

#ifndef _INCLUDE_GUARD_LATENCYTOP_H_
#define _INCLUDE_GUARD_LATENCYTOP_H_

#include <linux/compiler.h>
#ifdef CONFIG_LATENCYTOP

#define LT_SAVECOUNT		32
#define LT_BACKTRACEDEPTH	12

struct latency_record {
	unsigned long	backtrace[LT_BACKTRACEDEPTH];
	unsigned int	count;
	unsigned long	time;
	unsigned long	max;
};


struct task_struct;

extern int latencytop_enabled;
void __account_scheduler_latency(struct task_struct *task, int usecs, int inter);
static inline void
account_scheduler_latency(struct task_struct *task, int usecs, int inter)
{
	if (unlikely(latencytop_enabled))
		__account_scheduler_latency(task, usecs, inter);
}

void clear_all_latency_tracing(struct task_struct *p);

#else

static inline void
account_scheduler_latency(struct task_struct *task, int usecs, int inter)
{
}

static inline void clear_all_latency_tracing(struct task_struct *p)
{
}

#endif

#endif
