/* MN10300 Arch-specific initialisation
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <asm/processor.h>
#include <linux/console.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <proc/proc.h>
#include <asm/busctl-regs.h>
#include <asm/fpu.h>
#include <asm/sections.h>

struct mn10300_cpuinfo boot_cpu_data;

/* For PCI or other memory-mapped resources */
unsigned long pci_mem_start = 0x18000000;

char redboot_command_line[COMMAND_LINE_SIZE] =
	"console=ttyS0,115200 root=/dev/mtdblock3 rw";

char __initdata redboot_platform_name[COMMAND_LINE_SIZE];

static struct resource code_resource = {
	.start	= 0x100000,
	.end	= 0,
	.name	= "Kernel code",
};

static struct resource data_resource = {
	.start	= 0,
	.end	= 0,
	.name	= "Kernel data",
};

static unsigned long __initdata phys_memory_base;
static unsigned long __initdata phys_memory_end;
static unsigned long __initdata memory_end;
unsigned long memory_size;

struct thread_info *__current_ti = &init_thread_union.thread_info;
struct task_struct *__current = &init_task;

#define mn10300_known_cpus 3
static const char *const mn10300_cputypes[] = {
	"am33v1",
	"am33v2",
	"am34v1",
	"unknown"
};

/*
 *
 */
static void __init parse_mem_cmdline(char **cmdline_p)
{
	char *from, *to, c;

	/* save unparsed command line copy for /proc/cmdline */
	strcpy(boot_command_line, redboot_command_line);

	/* see if there's an explicit memory size option */
	from = redboot_command_line;
	to = redboot_command_line;
	c = ' ';

	for (;;) {
		if (c == ' ' && !memcmp(from, "mem=", 4)) {
			if (to != redboot_command_line)
				to--;
			memory_size = memparse(from + 4, &from);
		}

		c = *(from++);
		if (!c)
			break;

		*(to++) = c;
	}

	*to = '\0';
	*cmdline_p = redboot_command_line;

	if (memory_size == 0)
		panic("Memory size not known\n");

	memory_end = (unsigned long) CONFIG_KERNEL_RAM_BASE_ADDRESS +
		memory_size;
	if (memory_end > phys_memory_end)
		memory_end = phys_memory_end;
}

/*
 * architecture specific setup
 */
void __init setup_arch(char **cmdline_p)
{
	unsigned long bootmap_size;
	unsigned long kstart_pfn, start_pfn, free_pfn, end_pfn;

	cpu_init();
	unit_setup();
	parse_mem_cmdline(cmdline_p);

	init_mm.start_code = (unsigned long)&_text;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = (unsigned long) &_end;

	code_resource.start = virt_to_bus(&_text);
	code_resource.end = virt_to_bus(&_etext)-1;
	data_resource.start = virt_to_bus(&_etext);
	data_resource.end = virt_to_bus(&_edata)-1;

	start_pfn = (CONFIG_KERNEL_RAM_BASE_ADDRESS >> PAGE_SHIFT);
	kstart_pfn = PFN_UP(__pa(&_text));
	free_pfn = PFN_UP(__pa(&_end));
	end_pfn = PFN_DOWN(__pa(memory_end));

	bootmap_size = init_bootmem_node(&contig_page_data,
					 free_pfn,
					 start_pfn,
					 end_pfn);

	if (kstart_pfn > start_pfn)
		free_bootmem(PFN_PHYS(start_pfn),
			     PFN_PHYS(kstart_pfn - start_pfn));

	free_bootmem(PFN_PHYS(free_pfn),
		     PFN_PHYS(end_pfn - free_pfn));

	/* If interrupt vector table is in main ram, then we need to
	   reserve the page it is occupying. */
	if (CONFIG_INTERRUPT_VECTOR_BASE >= CONFIG_KERNEL_RAM_BASE_ADDRESS &&
	    CONFIG_INTERRUPT_VECTOR_BASE < memory_end)
		reserve_bootmem(CONFIG_INTERRUPT_VECTOR_BASE, PAGE_SIZE,
				BOOTMEM_DEFAULT);

	reserve_bootmem(PAGE_ALIGN(PFN_PHYS(free_pfn)), bootmap_size,
			BOOTMEM_DEFAULT);

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif

	paging_init();
}

/*
 * perform CPU initialisation
 */
void __init cpu_init(void)
{
	unsigned long cpurev = CPUREV, type;
	unsigned long base, size;

	type = (CPUREV & CPUREV_TYPE) >> CPUREV_TYPE_S;
	if (type > mn10300_known_cpus)
		type = mn10300_known_cpus;

	printk(KERN_INFO "Matsushita %s, rev %ld\n",
	       mn10300_cputypes[type],
	       (cpurev & CPUREV_REVISION) >> CPUREV_REVISION_S);

	/* determine the memory size and base from the memory controller regs */
	memory_size = 0;

	base = SDBASE(0);
	if (base & SDBASE_CE) {
		size = (base & SDBASE_CBAM) << SDBASE_CBAM_SHIFT;
		size = ~size + 1;
		base &= SDBASE_CBA;

		printk(KERN_INFO "SDRAM[0]: %luMb @%08lx\n", size >> 20, base);
		memory_size += size;
		phys_memory_base = base;
	}

	base = SDBASE(1);
	if (base & SDBASE_CE) {
		size = (base & SDBASE_CBAM) << SDBASE_CBAM_SHIFT;
		size = ~size + 1;
		base &= SDBASE_CBA;

		printk(KERN_INFO "SDRAM[1]: %luMb @%08lx\n", size >> 20, base);
		memory_size += size;
		if (phys_memory_base == 0)
			phys_memory_base = base;
	}

	phys_memory_end = phys_memory_base + memory_size;

#ifdef CONFIG_FPU
	fpu_init_state();
#endif
}

/*
 * Get CPU information for use by the procfs.
 */
static int show_cpuinfo(struct seq_file *m, void *v)
{
	unsigned long cpurev = CPUREV, type, icachesz, dcachesz;

	type = (CPUREV & CPUREV_TYPE) >> CPUREV_TYPE_S;
	if (type > mn10300_known_cpus)
		type = mn10300_known_cpus;

	icachesz =
		((cpurev & CPUREV_ICWAY ) >> CPUREV_ICWAY_S)  *
		((cpurev & CPUREV_ICSIZE) >> CPUREV_ICSIZE_S) *
		1024;

	dcachesz =
		((cpurev & CPUREV_DCWAY ) >> CPUREV_DCWAY_S)  *
		((cpurev & CPUREV_DCSIZE) >> CPUREV_DCSIZE_S) *
		1024;

	seq_printf(m,
		   "processor  : 0\n"
		   "vendor_id  : Matsushita\n"
		   "cpu core   : %s\n"
		   "cpu rev    : %lu\n"
		   "model name : " PROCESSOR_MODEL_NAME		"\n"
		   "icache size: %lu\n"
		   "dcache size: %lu\n",
		   mn10300_cputypes[type],
		   (cpurev & CPUREV_REVISION) >> CPUREV_REVISION_S,
		   icachesz,
		   dcachesz
		   );

	seq_printf(m,
		   "ioclk speed: %lu.%02luMHz\n"
		   "bogomips   : %lu.%02lu\n\n",
		   MN10300_IOCLK / 1000000,
		   (MN10300_IOCLK / 10000) % 100,
		   loops_per_jiffy / (500000 / HZ),
		   (loops_per_jiffy / (5000 / HZ)) % 100
		   );

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? cpu_data + *pos : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo,
};
