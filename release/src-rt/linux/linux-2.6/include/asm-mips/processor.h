/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 Waldorf GMBH
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_PROCESSOR_H
#define _ASM_PROCESSOR_H

#include <linux/cpumask.h>
#include <linux/threads.h>

#include <asm/cachectl.h>
#include <asm/cpu.h>
#include <asm/cpu-info.h>
#include <asm/mipsregs.h>
#include <asm/prefetch.h>
#include <asm/system.h>

/*
 * Return current * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

/*
 * System setup and hardware flags..
 */
extern void (*cpu_wait)(void);

extern unsigned int vced_count, vcei_count;

/*
 * A special page (the vdso) is mapped into all processes at the very
 * top of the virtual memory space.
 */
#define SPECIAL_PAGES_SIZE PAGE_SIZE

#ifdef CONFIG_32BIT
/*
 * User space process size: 2GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */
#define TASK_SIZE	0x7fff8000UL
#define STACK_TOP	((TASK_SIZE & PAGE_MASK) - SPECIAL_PAGES_SIZE)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE / 3))
#endif

#ifdef CONFIG_64BIT
/*
 * User space process size: 1TB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.  TASK_SIZE
 * is limited to 1TB by the R4000 architecture; R10000 and better can
 * support 16TB; the architectural reserve for future expansion is
 * 8192EB ...
 */
#define TASK_SIZE32	0x7fff8000UL
#define TASK_SIZE	0x10000000000UL
#define STACK_TOP	\
	(((test_thread_flag(TIF_32BIT_ADDR) ?				\
	   TASK_SIZE32 : TASK_SIZE) & PAGE_MASK) - SPECIAL_PAGES_SIZE)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	((current->thread.mflags & MF_32BIT_ADDR) ? \
	PAGE_ALIGN(TASK_SIZE32 / 3) : PAGE_ALIGN(TASK_SIZE / 3))
#endif

#define NUM_FPU_REGS	32

typedef __u64 fpureg_t;

/*
 * It would be nice to add some more fields for emulator statistics, but there
 * are a number of fixed offsets in offset.h and elsewhere that would have to
 * be recalculated by hand.  So the additional information will be private to
 * the FPU emulator for now.  See asm-mips/fpu_emulator.h.
 */

struct mips_fpu_struct {
	fpureg_t	fpr[NUM_FPU_REGS];
	unsigned int	fcr31;
};

#define INIT_FPU { \
	{0,} \
}

#define NUM_DSP_REGS   6

typedef __u32 dspreg_t;

struct mips_dsp_state {
	dspreg_t        dspr[NUM_DSP_REGS];
	unsigned int    dspcontrol;
};

#define INIT_DSP {{0,},}

#define INIT_CPUMASK { \
	{0,} \
}

typedef struct {
	unsigned long seg;
} mm_segment_t;

#define ARCH_MIN_TASKALIGN	8

struct mips_abi;

/*
 * If you change thread_struct remember to change the #defines below too!
 */
struct thread_struct {
	/* Saved main processor registers. */
	unsigned long reg16;
	unsigned long reg17, reg18, reg19, reg20, reg21, reg22, reg23;
	unsigned long reg29, reg30, reg31;

	/* Saved cp0 stuff. */
	unsigned long cp0_status;

	/* Saved fpu/fpu emulator stuff. */
	struct mips_fpu_struct fpu;
#ifdef CONFIG_MIPS_MT_FPAFF
	/* Emulated instruction count */
	unsigned long emulated_fp;
	/* Saved per-thread scheduler affinity mask */
	cpumask_t user_cpus_allowed;
#endif /* CONFIG_MIPS_MT_FPAFF */

	/* Saved state of the DSP ASE, if available. */
	struct mips_dsp_state dsp;

	/* Other stuff associated with the thread. */
	unsigned long cp0_badvaddr;	/* Last user fault */
	unsigned long cp0_baduaddr;	/* Last kernel fault accessing USEG */
	unsigned long error_code;
	unsigned long trap_no;
#define MF_FIXADE	1		/* Fix address errors in software */
#define MF_LOGADE	2		/* Log address errors to syslog */
#define MF_32BIT_REGS	4		/* also implies 16/32 fprs */
#define MF_32BIT_ADDR	8		/* 32-bit address space (o32/n32) */
#define MF_FPUBOUND	0x10		/* thread bound to FPU-full CPU set */
	unsigned long mflags;
	unsigned long irix_trampoline;  /* Wheee... */
	unsigned long irix_oldctx;
	struct mips_abi *abi;
};

#define MF_ABI_MASK	(MF_32BIT_REGS | MF_32BIT_ADDR)
#define MF_O32		(MF_32BIT_REGS | MF_32BIT_ADDR)
#define MF_N32		MF_32BIT_ADDR
#define MF_N64		0

#ifdef CONFIG_MIPS_MT_FPAFF
#define FPAFF_INIT 0, INIT_CPUMASK,
#else
#define FPAFF_INIT
#endif /* CONFIG_MIPS_MT_FPAFF */

#define INIT_THREAD  { \
        /* \
         * saved main processor registers \
         */ \
	0, 0, 0, 0, 0, 0, 0, 0, \
	               0, 0, 0, \
	/* \
	 * saved cp0 stuff \
	 */ \
	0, \
	/* \
	 * saved fpu/fpu emulator stuff \
	 */ \
	INIT_FPU, \
	/* \
	 * fpu affinity state (null if not FPAFF) \
	 */ \
	FPAFF_INIT \
	/* \
	 * saved dsp/dsp emulator stuff \
	 */ \
	INIT_DSP, \
	/* \
	 * Other stuff associated with the process \
	 */ \
	0, 0, 0, 0, \
	/* \
	 * For now the default is to fix address errors \
	 */ \
	MF_FIXADE, 0, 0 \
}

struct task_struct;

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while(0)

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)	do { } while (0)

extern long kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

extern unsigned long thread_saved_pc(struct task_struct *tsk);

/*
 * Do necessary setup to start up a newly executed thread.
 */
extern void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp);

unsigned long get_wchan(struct task_struct *p);

#define __KSTK_TOS(tsk) ((unsigned long)task_stack_page(tsk) + \
			 THREAD_SIZE - 32 - sizeof(struct pt_regs))
#define task_pt_regs(tsk) ((struct pt_regs *)__KSTK_TOS(tsk))
#define KSTK_EIP(tsk) (task_pt_regs(tsk)->cp0_epc)
#define KSTK_ESP(tsk) (task_pt_regs(tsk)->regs[29])
#define KSTK_STATUS(tsk) (task_pt_regs(tsk)->cp0_status)

#define cpu_relax()	barrier()

/*
 * Return_address is a replacement for __builtin_return_address(count)
 * which on certain architectures cannot reasonably be implemented in GCC
 * (MIPS, Alpha) or is unuseable with -fomit-frame-pointer (i386).
 * Note that __builtin_return_address(x>=1) is forbidden because GCC
 * aborts compilation on some CPUs.  It's simply not possible to unwind
 * some CPU's stackframes.
 *
 * __builtin_return_address works only for non-leaf functions.  We avoid the
 * overhead of a function call by forcing the compiler to save the return
 * address register on the stack.
 */
#define return_address() ({__asm__ __volatile__("":::"$31");__builtin_return_address(0);})

#ifdef CONFIG_CPU_HAS_PREFETCH

#define ARCH_HAS_PREFETCH
#define prefetch(x) __builtin_prefetch((x), 0, 1)

#define ARCH_HAS_PREFETCHW
#define prefetchw(x) __builtin_prefetch((x), 1, 1)

#endif

#endif /* _ASM_PROCESSOR_H */
