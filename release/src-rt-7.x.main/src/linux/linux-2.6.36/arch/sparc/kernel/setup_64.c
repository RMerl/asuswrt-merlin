/*
 *  linux/arch/sparc64/kernel/setup.c
 *
 *  Copyright (C) 1995,1996  David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1997       Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <asm/smp.h>
#include <linux/user.h>
#include <linux/screen_info.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <linux/console.h>
#include <linux/root_dev.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/initrd.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/oplib.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/idprom.h>
#include <asm/head.h>
#include <asm/starfire.h>
#include <asm/mmu_context.h>
#include <asm/timer.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/mmu.h>
#include <asm/ns87303.h>
#include <asm/btext.h>

#ifdef CONFIG_IP_PNP
#include <net/ipconfig.h>
#endif

#include "entry.h"
#include "kernel.h"

/* Used to synchronize accesses to NatSemi SUPER I/O chip configure
 * operations in asm/ns87303.h
 */
DEFINE_SPINLOCK(ns87303_lock);
EXPORT_SYMBOL(ns87303_lock);

struct screen_info screen_info = {
	0, 0,			/* orig-x, orig-y */
	0,			/* unused */
	0,			/* orig-video-page */
	0,			/* orig-video-mode */
	128,			/* orig-video-cols */
	0, 0, 0,		/* unused, ega_bx, unused */
	54,			/* orig-video-lines */
	0,                      /* orig-video-isVGA */
	16                      /* orig-video-points */
};

static void
prom_console_write(struct console *con, const char *s, unsigned n)
{
	prom_write(s, n);
}

/* Exported for mm/init.c:paging_init. */
unsigned long cmdline_memory_size = 0;

static struct console prom_early_console = {
	.name =		"earlyprom",
	.write =	prom_console_write,
	.flags =	CON_PRINTBUFFER | CON_BOOT | CON_ANYTIME,
	.index =	-1,
};

/* 
 * Process kernel command line switches that are specific to the
 * SPARC or that require special low-level processing.
 */
static void __init process_switch(char c)
{
	switch (c) {
	case 'd':
	case 's':
		break;
	case 'h':
		prom_printf("boot_flags_init: Halt!\n");
		prom_halt();
		break;
	case 'p':
		/* Just ignore, this behavior is now the default.  */
		break;
	case 'P':
		/* Force UltraSPARC-III P-Cache on. */
		if (tlb_type != cheetah) {
			printk("BOOT: Ignoring P-Cache force option.\n");
			break;
		}
		cheetah_pcache_forced_on = 1;
		add_taint(TAINT_MACHINE_CHECK);
		cheetah_enable_pcache();
		break;

	default:
		printk("Unknown boot switch (-%c)\n", c);
		break;
	}
}

static void __init boot_flags_init(char *commands)
{
	while (*commands) {
		/* Move to the start of the next "argument". */
		while (*commands && *commands == ' ')
			commands++;

		/* Process any command switches, otherwise skip it. */
		if (*commands == '\0')
			break;
		if (*commands == '-') {
			commands++;
			while (*commands && *commands != ' ')
				process_switch(*commands++);
			continue;
		}
		if (!strncmp(commands, "mem=", 4)) {
			cmdline_memory_size = simple_strtoul(commands + 4,
							     &commands, 0);
			if (*commands == 'K' || *commands == 'k') {
				cmdline_memory_size <<= 10;
				commands++;
			} else if (*commands=='M' || *commands=='m') {
				cmdline_memory_size <<= 20;
				commands++;
			}
		}
		while (*commands && *commands != ' ')
			commands++;
	}
}

extern unsigned short root_flags;
extern unsigned short root_dev;
extern unsigned short ram_flags;
#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000

extern int root_mountflags;

char reboot_command[COMMAND_LINE_SIZE];

static struct pt_regs fake_swapper_regs = { { 0, }, 0, 0, 0, 0 };

void __init per_cpu_patch(void)
{
	struct cpuid_patch_entry *p;
	unsigned long ver;
	int is_jbus;

	if (tlb_type == spitfire && !this_is_starfire)
		return;

	is_jbus = 0;
	if (tlb_type != hypervisor) {
		__asm__ ("rdpr %%ver, %0" : "=r" (ver));
		is_jbus = ((ver >> 32UL) == __JALAPENO_ID ||
			   (ver >> 32UL) == __SERRANO_ID);
	}

	p = &__cpuid_patch;
	while (p < &__cpuid_patch_end) {
		unsigned long addr = p->addr;
		unsigned int *insns;

		switch (tlb_type) {
		case spitfire:
			insns = &p->starfire[0];
			break;
		case cheetah:
		case cheetah_plus:
			if (is_jbus)
				insns = &p->cheetah_jbus[0];
			else
				insns = &p->cheetah_safari[0];
			break;
		case hypervisor:
			insns = &p->sun4v[0];
			break;
		default:
			prom_printf("Unknown cpu type, halting.\n");
			prom_halt();
		};

		*(unsigned int *) (addr +  0) = insns[0];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  0));

		*(unsigned int *) (addr +  4) = insns[1];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  4));

		*(unsigned int *) (addr +  8) = insns[2];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  8));

		*(unsigned int *) (addr + 12) = insns[3];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr + 12));

		p++;
	}
}

void __init sun4v_patch(void)
{
	extern void sun4v_hvapi_init(void);
	struct sun4v_1insn_patch_entry *p1;
	struct sun4v_2insn_patch_entry *p2;

	if (tlb_type != hypervisor)
		return;

	p1 = &__sun4v_1insn_patch;
	while (p1 < &__sun4v_1insn_patch_end) {
		unsigned long addr = p1->addr;

		*(unsigned int *) (addr +  0) = p1->insn;
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  0));

		p1++;
	}

	p2 = &__sun4v_2insn_patch;
	while (p2 < &__sun4v_2insn_patch_end) {
		unsigned long addr = p2->addr;

		*(unsigned int *) (addr +  0) = p2->insns[0];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  0));

		*(unsigned int *) (addr +  4) = p2->insns[1];
		wmb();
		__asm__ __volatile__("flush	%0" : : "r" (addr +  4));

		p2++;
	}

	sun4v_hvapi_init();
}

#ifdef CONFIG_SMP
void __init boot_cpu_id_too_large(int cpu)
{
	prom_printf("Serious problem, boot cpu id (%d) >= NR_CPUS (%d)\n",
		    cpu, NR_CPUS);
	prom_halt();
}
#endif

void __init setup_arch(char **cmdline_p)
{
	/* Initialize PROM console and command line. */
	*cmdline_p = prom_getbootargs();
	strcpy(boot_command_line, *cmdline_p);
	parse_early_param();

	boot_flags_init(*cmdline_p);
#ifdef CONFIG_EARLYFB
	if (btext_find_display())
#endif
		register_console(&prom_early_console);

	if (tlb_type == hypervisor)
		printk("ARCH: SUN4V\n");
	else
		printk("ARCH: SUN4U\n");

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

	idprom_init();

	if (!root_flags)
		root_mountflags &= ~MS_RDONLY;
	ROOT_DEV = old_decode_dev(root_dev);
#ifdef CONFIG_BLK_DEV_RAM
	rd_image_start = ram_flags & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((ram_flags & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((ram_flags & RAMDISK_LOAD_FLAG) != 0);	
#endif

	task_thread_info(&init_task)->kregs = &fake_swapper_regs;

#ifdef CONFIG_IP_PNP
	if (!ic_set_manually) {
		int chosen = prom_finddevice ("/chosen");
		u32 cl, sv, gw;
		
		cl = prom_getintdefault (chosen, "client-ip", 0);
		sv = prom_getintdefault (chosen, "server-ip", 0);
		gw = prom_getintdefault (chosen, "gateway-ip", 0);
		if (cl && sv) {
			ic_myaddr = cl;
			ic_servaddr = sv;
			if (gw)
				ic_gateway = gw;
#if defined(CONFIG_IP_PNP_BOOTP) || defined(CONFIG_IP_PNP_RARP)
			ic_proto_enabled = 0;
#endif
		}
	}
#endif

	/* Get boot processor trap_block[] setup.  */
	init_cur_cpu_trap(current_thread_info());

	paging_init();
}

/* BUFFER is PAGE_SIZE bytes long. */

extern void smp_info(struct seq_file *);
extern void smp_bogo(struct seq_file *);
extern void mmu_info(struct seq_file *);

unsigned int dcache_parity_tl1_occurred;
unsigned int icache_parity_tl1_occurred;

int ncpus_probed;

static int show_cpuinfo(struct seq_file *m, void *__unused)
{
	seq_printf(m, 
		   "cpu\t\t: %s\n"
		   "fpu\t\t: %s\n"
		   "pmu\t\t: %s\n"
		   "prom\t\t: %s\n"
		   "type\t\t: %s\n"
		   "ncpus probed\t: %d\n"
		   "ncpus active\t: %d\n"
		   "D$ parity tl1\t: %u\n"
		   "I$ parity tl1\t: %u\n"
#ifndef CONFIG_SMP
		   "Cpu0ClkTck\t: %016lx\n"
#endif
		   ,
		   sparc_cpu_type,
		   sparc_fpu_type,
		   sparc_pmu_type,
		   prom_version,
		   ((tlb_type == hypervisor) ?
		    "sun4v" :
		    "sun4u"),
		   ncpus_probed,
		   num_online_cpus(),
		   dcache_parity_tl1_occurred,
		   icache_parity_tl1_occurred
#ifndef CONFIG_SMP
		   , cpu_data(0).clock_tick
#endif
		);
#ifdef CONFIG_SMP
	smp_bogo(m);
#endif
	mmu_info(m);
#ifdef CONFIG_SMP
	smp_info(m);
#endif
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	/* The pointer we are returning is arbitrary,
	 * it just has to be non-NULL and not IS_ERR
	 * in the success case.
	 */
	return *pos == 0 ? &c_start : NULL;
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
	.start =c_start,
	.next =	c_next,
	.stop =	c_stop,
	.show =	show_cpuinfo,
};

extern int stop_a_enabled;

void sun_do_break(void)
{
	if (!stop_a_enabled)
		return;

	prom_printf("\n");
	flush_user_windows();

	prom_cmdline();
}
EXPORT_SYMBOL(sun_do_break);

int stop_a_enabled = 1;
EXPORT_SYMBOL(stop_a_enabled);
