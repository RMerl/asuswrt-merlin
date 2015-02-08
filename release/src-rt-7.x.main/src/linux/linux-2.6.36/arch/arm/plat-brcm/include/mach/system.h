/*
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <linux/io.h>

static inline void arch_idle(void)
{
	cpu_do_idle();
}

extern void arch_reset(char mode, const char *cmd);

#endif
