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
 * MIPS does have an arch_pick_mmap_layout()
 */
#define HAVE_ARCH_PICK_MMAP_LAYOUT 1

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

#ifdef __KERNEL__
#define STACK_TOP_MAX	TASK_SIZE
#endif

#define TASK_IS_32BIT_ADDR 1

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
#define TASK_SIZE64	0x10000000000UL
#define TASK_SIZE (test_thread_flag(TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)

#ifdef __KERNEL__
#define STACK_TOP_MAX	TASK_SIZE64
#endif


#define TASK_SIZE_OF(tsk)						\
	(test_tsk_thread_flag(tsk, TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)

#define TASK_IS_32BIT_ADDR test_thread_flag(TIF_32BIT_ADDR)

#endif

#define STACK_TOP	((TASK_SIZE & PAGE_MASK) - SPECIAL_PAGES_SIZE)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE PAGE_ALIGN(TASK_SIZE / 3)


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

#define NUM_DSP_REGS   6

typedef __u32 dspreg_t;

struct mips_dsp_state {
	dspreg_t        dspr[NUM_DSP_REGS];
	unsigned int    dspcontrol;
};

#define INIT_CPUMASK { \
	{0,} \
}

struct mips3264_watch_reg_state {
	/* The width of watchlo is 32 in a 32 bit kernel and 64 in a
	   64 bit kernel.  We use unsigned long as it has the same
	   property. */
	unsigned long watchlo[NUM_WATCH_REGS];
	/* Only the mask and IRW bits from watchhi. */
	u16 watchhi[NUM_WATCH_REGS];
};

union mips_watch_reg_state {
	struct mips3264_watch_reg_state mips3264;
};

#ifdef CONFIG_CPU_CAVIUM_OCTEON

struct octeon_cop2_state {
	/* DMFC2 rt, 0x0201 */
	unsigned long   cop2_crc_iv;
	/* DMFC2 rt, 0x0202 (Set with DMTC2 rt, 0x1202) */
	unsigned long   cop2_crc_length;
	/* DMFC2 rt, 0x0200 (set with DMTC2 rt, 0x4200) */
	unsigned long   cop2_crc_poly;
	/* DMFC2 rt, 0x0402; DMFC2 rt, 0x040A */
	unsigned long   cop2_llm_dat[2];
       /* DMFC2 rt, 0x0084 */
	unsigned long   cop2_3des_iv;
	/* DMFC2 rt, 0x0080; DMFC2 rt, 0x0081; DMFC2 rt, 0x0082 */
	unsigned long   cop2_3des_key[3];
	/* DMFC2 rt, 0x0088 (Set with DMTC2 rt, 0x0098) */
	unsigned long   cop2_3des_result;
	/* DMFC2 rt, 0x0111 (FIXME: Read Pass1 Errata) */
	unsigned long   cop2_aes_inp0;
	/* DMFC2 rt, 0x0102; DMFC2 rt, 0x0103 */
	unsigned long   cop2_aes_iv[2];
	/* DMFC2 rt, 0x0104; DMFC2 rt, 0x0105; DMFC2 rt, 0x0106; DMFC2
	 * rt, 0x0107 */
	unsigned long   cop2_aes_key[4];
	/* DMFC2 rt, 0x0110 */
	unsigned long   cop2_aes_keylen;
	/* DMFC2 rt, 0x0100; DMFC2 rt, 0x0101 */
	unsigned long   cop2_aes_result[2];
	/* DMFC2 rt, 0x0240; DMFC2 rt, 0x0241; DMFC2 rt, 0x0242; DMFC2
	 * rt, 0x0243; DMFC2 rt, 0x0244; DMFC2 rt, 0x0245; DMFC2 rt,
	 * 0x0246; DMFC2 rt, 0x0247; DMFC2 rt, 0x0248; DMFC2 rt,
	 * 0x0249; DMFC2 rt, 0x024A; DMFC2 rt, 0x024B; DMFC2 rt,
	 * 0x024C; DMFC2 rt, 0x024D; DMFC2 rt, 0x024E - Pass2 */
	unsigned long   cop2_hsh_datw[15];
	/* DMFC2 rt, 0x0250; DMFC2 rt, 0x0251; DMFC2 rt, 0x0252; DMFC2
	 * rt, 0x0253; DMFC2 rt, 0x0254; DMFC2 rt, 0x0255; DMFC2 rt,
	 * 0x0256; DMFC2 rt, 0x0257 - Pass2 */
	unsigned long   cop2_hsh_ivw[8];
	/* DMFC2 rt, 0x0258; DMFC2 rt, 0x0259 - Pass2 */
	unsigned long   cop2_gfm_mult[2];
	/* DMFC2 rt, 0x025E - Pass2 */
	unsigned long   cop2_gfm_poly;
	/* DMFC2 rt, 0x025A; DMFC2 rt, 0x025B - Pass2 */
	unsigned long   cop2_gfm_result[2];
};
#define INIT_OCTEON_COP2 {0,}

struct octeon_cvmseg_state {
	unsigned long cvmseg[CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE]
			    [cpu_dcache_line_size() / sizeof(unsigned long)];
};

#endif

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

	/* Saved watch register state, if available. */
	union mips_watch_reg_state watch;

	/* Other stuff associated with the thread. */
	unsigned long cp0_badvaddr;	/* Last user fault */
	unsigned long cp0_baduaddr;	/* Last kernel fault accessing USEG */
	unsigned long error_code;
	unsigned long irix_trampoline;  /* Wheee... */
	unsigned long irix_oldctx;
#ifdef CONFIG_CPU_CAVIUM_OCTEON
    struct octeon_cop2_state cp2 __attribute__ ((__aligned__(128)));
    struct octeon_cvmseg_state cvmseg __attribute__ ((__aligned__(128)));
#endif
	struct mips_abi *abi;
};

#ifdef CONFIG_MIPS_MT_FPAFF
#define FPAFF_INIT						\
	.emulated_fp			= 0,			\
	.user_cpus_allowed		= INIT_CPUMASK,
#else
#define FPAFF_INIT
#endif /* CONFIG_MIPS_MT_FPAFF */

#ifdef CONFIG_CPU_CAVIUM_OCTEON
#define OCTEON_INIT						\
	.cp2			= INIT_OCTEON_COP2,
#else
#define OCTEON_INIT
#endif /* CONFIG_CPU_CAVIUM_OCTEON */

#define INIT_THREAD  {						\
        /*							\
         * Saved main processor registers			\
         */							\
	.reg16			= 0,				\
	.reg17			= 0,				\
	.reg18			= 0,				\
	.reg19			= 0,				\
	.reg20			= 0,				\
	.reg21			= 0,				\
	.reg22			= 0,				\
	.reg23			= 0,				\
	.reg29			= 0,				\
	.reg30			= 0,				\
	.reg31			= 0,				\
	/*							\
	 * Saved cp0 stuff					\
	 */							\
	.cp0_status		= 0,				\
	/*							\
	 * Saved FPU/FPU emulator stuff				\
	 */							\
	.fpu			= {				\
		.fpr		= {0,},				\
		.fcr31		= 0,				\
	},							\
	/*							\
	 * FPU affinity state (null if not FPAFF)		\
	 */							\
	FPAFF_INIT						\
	/*							\
	 * Saved DSP stuff					\
	 */							\
	.dsp			= {				\
		.dspr		= {0, },			\
		.dspcontrol	= 0,				\
	},							\
	/*							\
	 * saved watch register stuff				\
	 */							\
	.watch = {{{0,},},},					\
	/*							\
	 * Other stuff associated with the process		\
	 */							\
	.cp0_badvaddr		= 0,				\
	.cp0_baduaddr		= 0,				\
	.error_code		= 0,				\
	.irix_trampoline	= 0,				\
	.irix_oldctx		= 0,				\
	/*							\
	 * Cavium Octeon specifics (null if not Octeon)		\
	 */							\
	OCTEON_INIT						\
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
 * (MIPS, Alpha) or is unusable with -fomit-frame-pointer (i386).
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
