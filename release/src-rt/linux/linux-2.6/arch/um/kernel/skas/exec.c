/*
 * Copyright (C) 2002 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "asm/current.h"
#include "asm/page.h"
#include "asm/signal.h"
#include "asm/ptrace.h"
#include "asm/uaccess.h"
#include "asm/mmu_context.h"
#include "tlb.h"
#include "skas.h"
#include "um_mmu.h"
#include "os.h"

void flush_thread_skas(void)
{
	void *data = NULL;
	unsigned long end = proc_mm ? task_size : CONFIG_STUB_START;
	int ret;

	ret = unmap(&current->mm->context.skas.id, 0, end, 1, &data);
	if(ret){
		printk("flush_thread_skas - clearing address space failed, "
		       "err = %d\n", ret);
		force_sig(SIGKILL, current);
	}

	switch_mm_skas(&current->mm->context.skas.id);
}

void start_thread_skas(struct pt_regs *regs, unsigned long eip,
		       unsigned long esp)
{
	set_fs(USER_DS);
	PT_REGS_IP(regs) = eip;
	PT_REGS_SP(regs) = esp;
}
