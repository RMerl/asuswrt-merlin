/*
 * Broadcom BCM47xx Performance Counters
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: perfcntr.c 241435 2011-02-18 04:04:21Z $
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <typedefs.h>
#include <osl.h>
#include <asm/mipsregs.h>
#include <mipsinc.h>

asmlinkage uint
read_perf_cntr(uint sel)
{
	/* Beware: Don't collapse this into a single call to read_c0_perf,
	 * it is not really a function, it is a macro that generates a
	 * different assembly instruction in each case.
	 */
	switch (sel) {
#ifdef	read_c0_perf
	case 0:
		return read_c0_perf(0);
	case 1:
		return read_c0_perf(1);
	case 2:
		return read_c0_perf(2);
	case 3:
		return read_c0_perf(3);
	case 4:
		return read_c0_perf(4);
	case 5:
		return read_c0_perf(5);
	case 6:
		return read_c0_perf(6);
	case 7:
		return read_c0_perf(7);
#else
	/* New style register access macros - LR */
	case 0:	return read_c0_perfctrl0();	/* Control */
	case 2:	return read_c0_perfctrl1();
	case 4:	return read_c0_perfctrl2();
	case 6:	return read_c0_perfctrl3();
	case 1: return read_c0_perfcntr0();	/* Counter */
	case 3: return read_c0_perfcntr1();
	case 5: return read_c0_perfcntr2();
	case 7: return read_c0_perfcntr3();
#endif
	default:
		return 0;
	}
}

/* HACK: map the old-style CP0 access macro */
#ifndef	write_c0_perf
#define write_c0_perf(sel,val) __write_32bit_c0_register($25, sel, val)
#endif

static uint perf_ctrl = 0;

static int
ctrl_read(char *page, char **start, off_t off,
          int count, int *eof, void *data)
{
	size_t len = 0;

	/* we have done once so stop */
	if (off)
		return 0;

	if (data == NULL) {
		/* return the value in hex string */
		len = sprintf(page, "0x%08x\n", perf_ctrl);
	} else {
		len = sprintf(page, "0x%08x 0x%08x 0x%08x 0x%08x\n",
		              read_perf_cntr(0), read_perf_cntr(2),
		              read_perf_cntr(4), read_perf_cntr(6));
	}
	*start = page;
	return len;
}

static void
set_ctrl(uint value)
{
	uint event;

	event = (value >> 24) & 0x7f;
	if (event != ((perf_ctrl >> 24) & 0x7f)) {
		write_c0_perf(0, 0);
		write_c0_perf(1, 0);
		if (event != 0x7f)
			write_c0_perf(0, (event << 5) | 0xf);
	}
	event = (value >> 16) & 0x7f;
	if (event != ((perf_ctrl >> 16) & 0x7f)) {
		write_c0_perf(2, 0);
		write_c0_perf(3, 0);
		if (event != 0x7f)
			write_c0_perf(2, (event << 5) | 0xf);
	}
	event = (value >> 8) & 0x7f;
	if (event != ((perf_ctrl >> 8) & 0x7f)) {
		write_c0_perf(4, 0);
		write_c0_perf(5, 0);
		if (event != 0x7f)
			write_c0_perf(4, (event << 5) | 0xf);
	}
	event = value & 0x7f;
	if (event != (perf_ctrl & 0x7f)) {
		write_c0_perf(6, 0);
		write_c0_perf(7, 0);
		if (event != 0x7f)
			write_c0_perf(6, (event << 5) | 0xf);
	}
	perf_ctrl = value & 0x7f7f7f7f;
}

static int
ctrl_write(struct file *file, const char *buf,
           unsigned long count, void *data)
{
	uint value;

	value = simple_strtoul(buf, NULL, 0);

	set_ctrl(value);

	return count;
}


static int
cntrs_read(char *page, char **start, off_t off,
          int count, int *eof, void *data)
{
	size_t len = 0;

	/* we have done once so stop */
	if (off)
		return 0;

	/* return the values in hex string */
	len = sprintf(page, "%10u %10u %10u %10u\n",
	              read_perf_cntr(1), read_perf_cntr(3),
	              read_perf_cntr(5), read_perf_cntr(7));
	*start = page;
	return len;
}

static void
cntrs_clear(void)
{
	write_c0_perf(1, 0);
	write_c0_perf(3, 0);
	write_c0_perf(5, 0);
	write_c0_perf(7, 0);
}

static int
clear_write(struct file *file, const char *buf,
           unsigned long count, void *data)
{
	cntrs_clear();

	return count;
}

static struct proc_dir_entry *perf_proc = NULL;

static int __init
perf_init(void)
{
	struct proc_dir_entry * ctrl_proc, * ctrlraw_proc ;
	struct proc_dir_entry * cntrs_proc, *clear_proc;
	struct proc_dir_entry * proc_root_ptr ;
	uint prid;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	proc_root_ptr = NULL ;
#else
	proc_root_ptr = &proc_root ;
#endif

	/* create proc entries for enabling cache hit/miss counting */
	prid = read_c0_prid();
	if (((prid & PRID_COMP_MASK) == PRID_COMP_MIPS) &&
	    ((prid & PRID_IMP_MASK) == PRID_IMP_74K)) {

		/* create proc entry cp0 in root */
		perf_proc = create_proc_entry("perf", 0444 | S_IFDIR, 
			proc_root_ptr);
		if (!perf_proc)
			return -ENOMEM;

		ctrl_proc = create_proc_entry("ctrl", 0644, perf_proc);
		if (!ctrl_proc)
			goto noctrl;
		ctrl_proc->data = NULL;
		ctrl_proc->read_proc = ctrl_read;
		ctrl_proc->write_proc = ctrl_write;

		ctrlraw_proc = create_proc_entry("ctrlraw", 0444, perf_proc);
		if (!ctrlraw_proc)
			goto noctrlraw;
		ctrlraw_proc->data = (void *)1;
		ctrlraw_proc->read_proc = ctrl_read;

		cntrs_proc = create_proc_entry("cntrs", 0444, perf_proc);
		if (!cntrs_proc)
			goto nocntrs;
		cntrs_proc->read_proc = cntrs_read;

		clear_proc = create_proc_entry("clear", 0444, perf_proc);
		if (!clear_proc)
			goto noclear;
		clear_proc->write_proc = clear_write;
	}

	/* Initialize off */
	set_ctrl(0x7f7f7f7f);
	return 0;
noclear:
	remove_proc_entry("cntrs", perf_proc);
nocntrs:
	remove_proc_entry("ctrlraw", perf_proc);
noctrlraw:
	remove_proc_entry("ctrl", perf_proc);
noctrl:
	remove_proc_entry("perf", proc_root_ptr);

	return -ENOMEM;
}

static void __exit perf_cleanup(void)
{
	struct proc_dir_entry * proc_root_ptr ;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	proc_root_ptr = NULL ;
#else
	proc_root_ptr = &proc_root ;
#endif

	remove_proc_entry("clear", perf_proc);
	remove_proc_entry("cntrs", perf_proc);
	remove_proc_entry("ctrlraw", perf_proc);
	remove_proc_entry("ctrl", perf_proc);
	remove_proc_entry("perf", proc_root_ptr);
	perf_proc = NULL;
}

/* hook it up with system at boot time */
module_init(perf_init);
module_exit(perf_cleanup);

#endif	/* CONFIG_PROC_FS */
