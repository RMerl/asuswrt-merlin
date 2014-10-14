/*
 * Copyright 2004-2009 Analog Devices Inc.
 *                2002 David McCullough <davidm@snapgear.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef __MMU_H
#define __MMU_H

struct sram_list_struct {
	struct sram_list_struct *next;
	void *addr;
	size_t length;
};

typedef struct {
	unsigned long end_brk;
	unsigned long stack_start;

	/* Points to the location in SDRAM where the L1 stack is normally
	   saved, or NULL if the stack is always in SDRAM.  */
	void *l1_stack_save;

	struct sram_list_struct *sram_list;

#ifdef CONFIG_BINFMT_ELF_FDPIC
	unsigned long	exec_fdpic_loadmap;
	unsigned long	interp_fdpic_loadmap;
#endif
#ifdef CONFIG_MPU
	unsigned long *page_rwx_mask;
#endif
} mm_context_t;

#endif
