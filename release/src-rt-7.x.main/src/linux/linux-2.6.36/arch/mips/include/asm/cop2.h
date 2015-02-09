/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 Wind River Systems,
 *   written by Ralf Baechle <ralf@linux-mips.org>
 */
#ifndef __ASM_COP2_H
#define __ASM_COP2_H

#include <linux/notifier.h>

enum cu2_ops {
	CU2_EXCEPTION,
	CU2_LWC2_OP,
	CU2_LDC2_OP,
	CU2_SWC2_OP,
	CU2_SDC2_OP,
};

extern int register_cu2_notifier(struct notifier_block *nb);
extern int cu2_notifier_call_chain(unsigned long val, void *v);

#define cu2_notifier(fn, pri)						\
({									\
	static struct notifier_block fn##_nb = {			\
		.notifier_call = fn,					\
		.priority = pri						\
	};								\
									\
	register_cu2_notifier(&fn##_nb);				\
})

#endif /* __ASM_COP2_H */
