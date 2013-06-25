/* 
 * Copyright (C) 2001 Chris Emerson (cemerson@chiark.greenend.org.uk)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "linux/smp.h"
#include "asm/ptrace.h"
#include "sysrq.h"

void show_regs(struct pt_regs_subarch *regs)
{
	printk("\n");
	printk("show_regs(): insert regs here.\n");

        show_trace(current, &regs->gpr[1]);
}
