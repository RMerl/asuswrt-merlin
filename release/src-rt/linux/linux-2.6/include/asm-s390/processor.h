/*
 *  include/asm-s390/processor.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/processor.h"
 *    Copyright (C) 1994, Linus Torvalds
 */

#ifndef __ASM_S390_PROCESSOR_H
#define __ASM_S390_PROCESSOR_H

#include <asm/ptrace.h>

#ifdef __KERNEL__
/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; asm("basr %0,0" : "=a" (pc)); pc; })

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 *  Members of this structure are referenced in head.S, so think twice
 *  before touching them. [mj]
 */

typedef struct
{
        unsigned int version :  8;
        unsigned int ident   : 24;
        unsigned int machine : 16;
        unsigned int unused  : 16;
} __attribute__ ((packed)) cpuid_t;

static inline void get_cpu_id(cpuid_t *ptr)
{
	asm volatile("stidp 0(%1)" : "=m" (*ptr) : "a" (ptr));
}

struct cpuinfo_S390
{
        cpuid_t  cpu_id;
        __u16    cpu_addr;
        __u16    cpu_nr;
        unsigned long loops_per_jiffy;
        unsigned long *pgd_quick;
#ifdef __s390x__
        unsigned long *pmd_quick;
#endif /* __s390x__ */
        unsigned long *pte_quick;
        unsigned long pgtable_cache_sz;
};

extern void s390_adjust_jiffies(void);
extern void print_cpu_info(struct cpuinfo_S390 *);
extern int get_cpu_capability(unsigned int *);

/* Lazy FPU handling on uni-processor */
extern struct task_struct *last_task_used_math;

/*
 * User space process size: 2GB for 31 bit, 4TB for 64 bit.
 */
#ifndef __s390x__

# define TASK_SIZE		(0x80000000UL)
# define TASK_UNMAPPED_BASE	(TASK_SIZE / 2)
# define DEFAULT_TASK_SIZE	(0x80000000UL)

#else /* __s390x__ */

# define TASK_SIZE		(test_thread_flag(TIF_31BIT) ? \
					(0x80000000UL) : (0x40000000000UL))
# define TASK_UNMAPPED_BASE	(TASK_SIZE / 2)
# define DEFAULT_TASK_SIZE	(0x40000000000UL)

#endif /* __s390x__ */

#define HAVE_ARCH_PICK_MMAP_LAYOUT

typedef struct {
        __u32 ar4;
} mm_segment_t;

/*
 * Thread structure
 */
struct thread_struct {
	s390_fp_regs fp_regs;
	unsigned int  acrs[NUM_ACRS];
        unsigned long ksp;              /* kernel stack pointer             */
        unsigned long user_seg;         /* HSTD                             */
	mm_segment_t mm_segment;
        unsigned long prot_addr;        /* address of protection-excep.     */
        unsigned int error_code;        /* error-code of last prog-excep.   */
        unsigned int trap_no;
        per_struct per_info;
	/* Used to give failing instruction back to user for ieee exceptions */
	unsigned long ieee_instruction_pointer; 
        /* pfault_wait is used to block the process on a pfault event */
	unsigned long pfault_wait;
};

typedef struct thread_struct thread_struct;

/*
 * Stack layout of a C stack frame.
 */
#ifndef __PACK_STACK
struct stack_frame {
	unsigned long back_chain;
	unsigned long empty1[5];
	unsigned long gprs[10];
	unsigned int  empty2[8];
};
#else
struct stack_frame {
	unsigned long empty1[5];
	unsigned int  empty2[8];
	unsigned long gprs[10];
	unsigned long back_chain;
};
#endif

#define ARCH_MIN_TASKALIGN	8

#ifndef __s390x__
# define __SWAPPER_PG_DIR __pa(&swapper_pg_dir[0]) + _SEGMENT_TABLE
#else /* __s390x__ */
# define __SWAPPER_PG_DIR __pa(&swapper_pg_dir[0]) + _REGION_TABLE
#endif /* __s390x__ */

#define INIT_THREAD {{0,{{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},	       \
			    {0},{0},{0},{0},{0},{0}}},			       \
		     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	       \
		     sizeof(init_stack) + (unsigned long) &init_stack,	       \
		     __SWAPPER_PG_DIR,					       \
		     {0},						       \
		     0,0,0,						       \
		     (per_struct) {{{{0,}}},0,0,0,0,{{0,}}},		       \
		     0, 0						       \
} 

/*
 * Do necessary setup to start up a new thread.
 */
#ifndef __s390x__

#define start_thread(regs, new_psw, new_stackp) do {            \
	set_fs(USER_DS);					\
	regs->psw.mask	= psw_user_bits;			\
        regs->psw.addr  = new_psw | PSW_ADDR_AMODE;             \
        regs->gprs[15]  = new_stackp ;                          \
} while (0)

#else /* __s390x__ */

#define start_thread(regs, new_psw, new_stackp) do {            \
	set_fs(USER_DS);					\
	regs->psw.mask	= psw_user_bits;			\
        regs->psw.addr  = new_psw;                              \
        regs->gprs[15]  = new_stackp;                           \
} while (0)

#define start_thread31(regs, new_psw, new_stackp) do {          \
	set_fs(USER_DS);					\
	regs->psw.mask	= psw_user32_bits;			\
        regs->psw.addr  = new_psw;                              \
        regs->gprs[15]  = new_stackp;                           \
} while (0)

#endif /* __s390x__ */

/* Forward declaration, a strange C thing */
struct task_struct;
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)	do { } while (0)

/*
 * Return saved PC of a blocked thread.
 */
extern unsigned long thread_saved_pc(struct task_struct *t);

/*
 * Print register of task into buffer. Used in fs/proc/array.c.
 */
extern char *task_show_regs(struct task_struct *task, char *buffer);

extern void show_registers(struct pt_regs *regs);
extern void show_code(struct pt_regs *regs);
extern void show_trace(struct task_struct *task, unsigned long *sp);

unsigned long get_wchan(struct task_struct *p);
#define task_pt_regs(tsk) ((struct pt_regs *) \
        (task_stack_page(tsk) + THREAD_SIZE) - 1)
#define KSTK_EIP(tsk)	(task_pt_regs(tsk)->psw.addr)
#define KSTK_ESP(tsk)	(task_pt_regs(tsk)->gprs[15])

/*
 * Give up the time slice of the virtual PU.
 */
static inline void cpu_relax(void)
{
	if (MACHINE_HAS_DIAG44)
		asm volatile("diag 0,0,68");
	barrier();
}

static inline void psw_set_key(unsigned int key)
{
	asm volatile("spka 0(%0)" : : "d" (key));
}

/*
 * Set PSW to specified value.
 */
static inline void __load_psw(psw_t psw)
{
#ifndef __s390x__
	asm volatile("lpsw  0(%0)" : : "a" (&psw), "m" (psw) : "cc");
#else
	asm volatile("lpswe 0(%0)" : : "a" (&psw), "m" (psw) : "cc");
#endif
}

/*
 * Set PSW mask to specified value, while leaving the
 * PSW addr pointing to the next instruction.
 */

static inline void __load_psw_mask (unsigned long mask)
{
	unsigned long addr;
	psw_t psw;

	psw.mask = mask;

#ifndef __s390x__
	asm volatile(
		"	basr	%0,0\n"
		"0:	ahi	%0,1f-0b\n"
		"	st	%0,4(%1)\n"
		"	lpsw	0(%1)\n"
		"1:"
		: "=&d" (addr) : "a" (&psw), "m" (psw) : "memory", "cc");
#else /* __s390x__ */
	asm volatile(
		"	larl	%0,1f\n"
		"	stg	%0,8(%1)\n"
		"	lpswe	0(%1)\n"
		"1:"
		: "=&d" (addr) : "a" (&psw), "m" (psw) : "memory", "cc");
#endif /* __s390x__ */
}
 
/*
 * Function to stop a processor until an interruption occurred
 */
static inline void enabled_wait(void)
{
	__load_psw_mask(PSW_BASE_BITS | PSW_MASK_IO | PSW_MASK_EXT |
			PSW_MASK_MCHECK | PSW_MASK_WAIT | PSW_DEFAULT_KEY);
}

/*
 * Function to drop a processor into disabled wait state
 */

static inline void disabled_wait(unsigned long code)
{
        unsigned long ctl_buf;
        psw_t dw_psw;

        dw_psw.mask = PSW_BASE_BITS | PSW_MASK_WAIT;
        dw_psw.addr = code;
        /* 
         * Store status and then load disabled wait psw,
         * the processor is dead afterwards
         */
#ifndef __s390x__
	asm volatile(
		"	stctl	0,0,0(%2)\n"
		"	ni	0(%2),0xef\n"	/* switch off protection */
		"	lctl	0,0,0(%2)\n"
		"	stpt	0xd8\n"		/* store timer */
		"	stckc	0xe0\n"		/* store clock comparator */
		"	stpx	0x108\n"	/* store prefix register */
		"	stam	0,15,0x120\n"	/* store access registers */
		"	std	0,0x160\n"	/* store f0 */
		"	std	2,0x168\n"	/* store f2 */
		"	std	4,0x170\n"	/* store f4 */
		"	std	6,0x178\n"	/* store f6 */
		"	stm	0,15,0x180\n"	/* store general registers */
		"	stctl	0,15,0x1c0\n"	/* store control registers */
		"	oi	0x1c0,0x10\n"	/* fake protection bit */
		"	lpsw	0(%1)"
		: "=m" (ctl_buf)
		: "a" (&dw_psw), "a" (&ctl_buf), "m" (dw_psw) : "cc");
#else /* __s390x__ */
	asm volatile(
		"	stctg	0,0,0(%2)\n"
		"	ni	4(%2),0xef\n"	/* switch off protection */
		"	lctlg	0,0,0(%2)\n"
		"	lghi	1,0x1000\n"
		"	stpt	0x328(1)\n"	/* store timer */
		"	stckc	0x330(1)\n"	/* store clock comparator */
		"	stpx	0x318(1)\n"	/* store prefix register */
		"	stam	0,15,0x340(1)\n"/* store access registers */
		"	stfpc	0x31c(1)\n"	/* store fpu control */
		"	std	0,0x200(1)\n"	/* store f0 */
		"	std	1,0x208(1)\n"	/* store f1 */
		"	std	2,0x210(1)\n"	/* store f2 */
		"	std	3,0x218(1)\n"	/* store f3 */
		"	std	4,0x220(1)\n"	/* store f4 */
		"	std	5,0x228(1)\n"	/* store f5 */
		"	std	6,0x230(1)\n"	/* store f6 */
		"	std	7,0x238(1)\n"	/* store f7 */
		"	std	8,0x240(1)\n"	/* store f8 */
		"	std	9,0x248(1)\n"	/* store f9 */
		"	std	10,0x250(1)\n"	/* store f10 */
		"	std	11,0x258(1)\n"	/* store f11 */
		"	std	12,0x260(1)\n"	/* store f12 */
		"	std	13,0x268(1)\n"	/* store f13 */
		"	std	14,0x270(1)\n"	/* store f14 */
		"	std	15,0x278(1)\n"	/* store f15 */
		"	stmg	0,15,0x280(1)\n"/* store general registers */
		"	stctg	0,15,0x380(1)\n"/* store control registers */
		"	oi	0x384(1),0x10\n"/* fake protection bit */
		"	lpswe	0(%1)"
		: "=m" (ctl_buf)
		: "a" (&dw_psw), "a" (&ctl_buf), "m" (dw_psw) : "cc", "0");
#endif /* __s390x__ */
}

/*
 * Basic Machine Check/Program Check Handler.
 */

extern void s390_base_mcck_handler(void);
extern void s390_base_pgm_handler(void);
extern void s390_base_ext_handler(void);

extern void (*s390_base_mcck_handler_fn)(void);
extern void (*s390_base_pgm_handler_fn)(void);
extern void (*s390_base_ext_handler_fn)(void);

/*
 * CPU idle notifier chain.
 */
#define CPU_IDLE	0
#define CPU_NOT_IDLE	1

struct notifier_block;
int register_idle_notifier(struct notifier_block *nb);
int unregister_idle_notifier(struct notifier_block *nb);

#define ARCH_LOW_ADDRESS_LIMIT	0x7fffffffUL

#endif

/*
 * Helper macro for exception table entries
 */
#ifndef __s390x__
#define EX_TABLE(_fault,_target)			\
	".section __ex_table,\"a\"\n"			\
	"	.align 4\n"				\
	"	.long  " #_fault "," #_target "\n"	\
	".previous\n"
#else
#define EX_TABLE(_fault,_target)			\
	".section __ex_table,\"a\"\n"			\
	"	.align 8\n"				\
	"	.quad  " #_fault "," #_target "\n"	\
	".previous\n"
#endif

#endif                                 /* __ASM_S390_PROCESSOR_H           */
