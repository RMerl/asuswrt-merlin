/*
 * Broadcom BCM47xx Buzzz based Kernel Profiling and Debugging
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id$
 *
 * -----------------------------------------------------------------------------
 *
 * Filename     : buzzz.c
 * Description  : Implementation of buzzz.c.
 *
 * Buzzz, a work in progress, provides 4 capabilities, namely,
 *
 *  1. A character device driver for userspace command line ioctl communication
 * with proprietary Buzzz kernel debug tools.
 *  2. A logging infrastructure that may be used for:
 *      - kernel event logging using pre-instrument kernel instrumentation,
 *      - MIPS 74K performance counter monitoring of code segments,
 *      - log of all functions calls using compiler -finstrument-function stubs.
 *      - logging a past history prior to an audit assert, or
 *  3. An audit infrastucture allowing user defined audits to be invoked at
 * some well known points within the system, e.g. periodic, context switch, a
 * specific interrupt, queue overflow etc.
 *  4. Transmission of logged data to an off-target host for post-processing and
 * display, or recording into flash, etc.
 *     Currently a proc fs and netcat utility is used.
 *
 *     Target: cat /proc/buzzz/log | nc 192.168.1.10 33333
 *     Host:   nc -l 192.168.1.10 33333 > func_trace.txt
 *  BUG: use of netcat on target causes page fault.
 *
 * -----------------------------------------------------------------------------
 */

#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#if defined(CONFIG_MIPS)
#include <asm/mipsregs.h>
#include <asm/prefetch.h>
#endif	/*  CONFIG_MIPS */
#if defined(CONFIG_SMP)
#include <linux/spinlock_types.h>
#endif	/*  CONFIG_SMP */
#include <asm/buzzz.h>

#if defined(CC_BUZZZ_DEBUG)
unsigned int buzzz_debug = 0U;
EXPORT_SYMBOL(buzzz_debug);
#endif  /*  CC_BUZZZ_DEBUG */

/* defines */

/* Maximum length of a single log entry cannot exceed 64 bytes */
#define BUZZZ_LOGENTRY_MAXSZ    (64)

/* Length of the buffer available for logging */
#define BUZZZ_AVAIL_BUFSIZE     (BUZZZ_LOG_BUFSIZE - BUZZZ_LOGENTRY_MAXSZ)

/* Caution: BUZZZ may only be used on single core. Not SMP safe */
#if defined(CONFIG_SMP)
#define BUZZZ_LOCK(flags)       spin_lock_irqsave(&buzzz_g.lock, flags)
#define BUZZZ_UNLOCK(flags)     spin_unlock_irqrestore(&buzzz_g.lock, flags)
#else   /*  CONFIG_SMP */
#define BUZZZ_LOCK(flags)       local_irq_save(flags)
#define BUZZZ_UNLOCK(flags)     local_irq_restore(flags)
#endif  /* !CONFIG_SMP */

/*
 * Function Call Tracing Tool
 */
/* Number of logs prior to end of trace to be dumped on a kernel panic */
#define BUZZZ_FUNC_PANIC_LOGS     (512)
#define BUZZZ_FUNC_LIMIT_LOGS     (512)
#define BUZZZ_FUNC_INDENT_STRING   "  "

#if (BUZZZ_LOG_BUFSIZE < (4 * 4 * BUZZZ_FUNC_PANIC_LOGS))
#error "BUZZZ_FUNC_PANIC_LOGS is too large"
#endif  /* test BUZZZ_LOG_BUFSIZE */

#if (BUZZZ_LOG_BUFSIZE < (4 * 4 * BUZZZ_FUNC_LIMIT_LOGS))
#error "BUZZZ_FUNC_LIMIT_LOGS is too large"
#endif  /* test BUZZZ_LOG_BUFSIZE */

/*
 * Performance Monitoring Tool
 */
#define BUZZZ_PMON_SAMPLESZ     (10U)

typedef void (*timer_fn_t)(unsigned long);
/*
 * -----------------------------------------------------------------------------
 * Buzzz debug display support.
 *
 * KALLSYMS printk formats: %pf %pF mac(%pM %pm) (%pI4 %pi4)
 * extern int sprint_symbol(char *buffer, unsigned long address);
 * extern void __print_symbol(const char *fmt, unsigned long address);
 * #define print_ip_sym(ip) printk("[<%016lx>]", ip); print_symbol(" %s\n", ip);
 * __print_symbol(fmt, address) {
 *    char buffer[KSYM_SYMBOL_LEN = 127];
 *    sprint_symbol(buffer, address);
 *    printk(fmt, buffer);
 * }
 * #undef  IP4DOTQ
 * #define IP4DOTQ(addr, n)        (((addr) >> (24 - 8 * n)) & 0xFF)
 * -----------------------------------------------------------------------------
 */

#if defined(BUZZZ_CONFIG_SYS_KDBG)
#define BUZZZ_PRINT(fmt, arg...)                                               \
	printk(CLRg "BUZZZ %s: " fmt CLRnl, __FUNCTION__, ##arg)
#else   /* !BUZZZ_CONFIG_SYS_KDBG */
#define BUZZZ_PRINT(fmt, arg...)    BUZZZ_NULL_STMT
#endif  /* !BUZZZ_CONFIG_SYS_KDBG */

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(val)         #val,

static const char * _str_INV = "INVALID";

static const char * _str_buzzz_tool[BUZZZ_TOOL_MAXIMUM] =
{
	BUZZZ_ENUM(UNDEF)
	BUZZZ_ENUM(FUNC)        /* Function call tracing */
	BUZZZ_ENUM(PMON)        /* Algorithm performance monitoring */
	BUZZZ_ENUM(KEVT)        /* Kernel space event tracing */
};
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_tool(uint32_t tool)
{
	return (tool >= BUZZZ_TOOL_MAXIMUM) ? _str_INV :
		_str_buzzz_tool[tool];
}

static const char * _str_buzzz_status[BUZZZ_STATUS_MAXIMUM] =
{
	BUZZZ_ENUM(DISABLED)
	BUZZZ_ENUM(ENABLED)
	BUZZZ_ENUM(PAUSED)
};
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_status(uint32_t status)
{
	return (status >= BUZZZ_STATUS_MAXIMUM) ? _str_INV :
		_str_buzzz_status[status];
}

static const char * _str_buzzz_mode[BUZZZ_MODE_MAXIMUM] =
{
	BUZZZ_ENUM(UNDEF)
	BUZZZ_ENUM(WRAPOVER)
	BUZZZ_ENUM(LIMITED)
	BUZZZ_ENUM(TRANSMIT)
};
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_mode(uint32_t mode)
{
	return (mode >= BUZZZ_MODE_MAXIMUM) ? _str_INV :
		_str_buzzz_mode[mode];
}
static const char * _str_buzzz_ioctl[BUZZZ_IOCTL_MAXIMUM] =
{
	BUZZZ_ENUM(KCALL)

	BUZZZ_ENUM(CONFIG_TOOL)
	BUZZZ_ENUM(CONFIG_MODE)
	BUZZZ_ENUM(CONFIG_LIMIT)

	BUZZZ_ENUM(CONFIG_FUNC)
	BUZZZ_ENUM(CONFIG_PMON)
	BUZZZ_ENUM(CONFIG_KEVT)

	BUZZZ_ENUM(SHOW)
	BUZZZ_ENUM(START)
	BUZZZ_ENUM(STOP)
	BUZZZ_ENUM(PAUSE)
	BUZZZ_ENUM(PLAY)
	BUZZZ_ENUM(AUDIT)
	BUZZZ_ENUM(DUMP)
};
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_ioctl(uint32_t ioctl)
{
	ioctl -= BUZZZ_IOCTL_KCALL;
	return (ioctl >= BUZZZ_IOCTL_MAXIMUM) ? _str_INV :
		_str_buzzz_ioctl[ioctl];
}

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
static const char * _str_buzzz_pmon_group[BUZZZ_PMON_GROUP_MAXIMUM] =
{
	BUZZZ_ENUM(RESET)
	BUZZZ_ENUM(GENERAL)
	BUZZZ_ENUM(ICACHE)
	BUZZZ_ENUM(DCACHE)
	BUZZZ_ENUM(TLB)
	BUZZZ_ENUM(CYCLES_COMPLETED)
	BUZZZ_ENUM(CYCLES_ISSUE_OOO)
	BUZZZ_ENUM(INSTR_GENERAL)
	BUZZZ_ENUM(INSTR_MISCELLANEOUS)
	BUZZZ_ENUM(INSTR_LOAD_STORE)
	BUZZZ_ENUM(CYCLES_IDLE_FULL)
	BUZZZ_ENUM(CYCLES_IDLE_WAIT)
	/* BUZZZ_ENUM(L2_CACHE) */
};

static const char * _str_buzzz_pmon_event[BUZZZ_PMON_EVENT_MAXIMUM] =
{
	/* group  0: RESET */
	BUZZZ_ENUM(CTR0_NONE)                   /* 127 */
	BUZZZ_ENUM(CTR1_NONE)                   /* 127 */
	BUZZZ_ENUM(CTR2_NONE)                   /* 127 */
	BUZZZ_ENUM(CTR3_NONE)                   /* 127 */

	/* group  1 GENERAL */
	BUZZZ_ENUM(COMPL0_MISPRED)              /* 56: 0,  2   */
	BUZZZ_ENUM(CYCLES_ELAPSED)              /*  0: 0,1,2,3 */
	BUZZZ_ENUM(EXCEPTIONS)                  /* 58: 0,  2   */
	BUZZZ_ENUM(COMPLETED)                   /*  1: 0,1,2,3 */

	/* group  2 ICACHE */
	BUZZZ_ENUM(IC_ACCESS)                   /*  6: 0,  2   */
	BUZZZ_ENUM(IC_REFILL)                   /*  6:   1,  3 */
	BUZZZ_ENUM(CYCLES_IC_MISS)              /*  7: 0,  2   */
	BUZZZ_ENUM(CYCLES_L2_MISS)              /*  7:   1,  3 */

	/* group  3 DCACHE */
	BUZZZ_ENUM(LOAD_DC_ACCESS)              /* 23: 0,  2   */
	BUZZZ_ENUM(LSP_DC_ACCESS)               /* 23:   1,  3 */
	BUZZZ_ENUM(WB_DC_ACCESS)                /* 24: 0,  2   */
	BUZZZ_ENUM(LSP_DC_MISSES)               /* 24:   1,  3 */

	/* group  4 TLB */
	BUZZZ_ENUM(ITLB_ACCESS)                 /*  4: 0,  2   */
	BUZZZ_ENUM(ITLB_MISS)                   /*  4:   1,  3 */
	BUZZZ_ENUM(JTLB_DACCESS)                /* 25: 0,  2   */
	BUZZZ_ENUM(JTLB_XL_FAIL)                /* 25:   1,  3 */

	/* group  5 CYCLES_COMPLETED  */
	BUZZZ_ENUM(COMPL0_INSTR)                /* 53: 0,  2   */
	BUZZZ_ENUM(COMPL_LOAD_MISS)             /* 53:   1,  3 */
	BUZZZ_ENUM(COMPL1_INSTR)                /* 54: 0,  2   */
	BUZZZ_ENUM(COMPL2_INSTR)                /* 54:   1,  3 */

	/* group  6 CYCLES_ISSUE_OOO */
	BUZZZ_ENUM(ISS1_INSTR)                  /* 20: 0,  2   */
	BUZZZ_ENUM(ISS2_INSTR)                  /* 20:   1,  3 */
	BUZZZ_ENUM(OOO_ALU)                     /* 21: 0,  2   */
	BUZZZ_ENUM(OOO_AGEN)                    /* 21:   1,  3 */

	/* group  7 INSTR_GENERAL */
	BUZZZ_ENUM(CONDITIONAL)                 /* 39: 0,  2   */
	BUZZZ_ENUM(MISPREDICTED)                /* 39:   1,  3 */
	BUZZZ_ENUM(INTEGER)                     /* 40: 0,  2   */
	BUZZZ_ENUM(FLOAT)                       /* 40:   1,  3 */

	/* group  8 INSTR_MISCELLANEOUS */
	BUZZZ_ENUM(JUMP)                        /* 42: 0,  2   */
	BUZZZ_ENUM(MULDIV)                      /* 43:   1,  3 */
	BUZZZ_ENUM(PREFETCH)                    /* 52: 0,  2   */
	BUZZZ_ENUM(PREFETCH_NULL)               /* 52:   1,  3 */

	/* group  9 INSTR_LOAD_STORE */
	BUZZZ_ENUM(LOAD)                        /* 41: 0,  2   */
	BUZZZ_ENUM(STORE)                       /* 41:   1,  3 */
	BUZZZ_ENUM(LOAD_UNCACHE)                /* 46: 0,  2   */
	BUZZZ_ENUM(STORE_UNCACHE)               /* 46:   1,  3 */

	/* group  10 CYCLES_IDLE_FULL */
	BUZZZ_ENUM(ALU_CAND_POOL)               /* 13: 0,  2   */
	BUZZZ_ENUM(AGEN_CAND_POOL)              /* 13:   1,  3 */
	BUZZZ_ENUM(ALU_COMPL_BUF)               /* 14: 0,  2   */
	BUZZZ_ENUM(AGEN_COMPL_BUF)              /* 14:   1,  3 */

	/* group 11 CYCLES_IDLE_WAIT */
	BUZZZ_ENUM(ALU_NO_INSTR)                /* 16: 0,  2   */
	BUZZZ_ENUM(AGEN_NO_INSTR)               /* 16:   1,  3 */
	BUZZZ_ENUM(ALU_NO_OPER)                 /* 17: 0,  2   */
	BUZZZ_ENUM(GEN_NO_OPER)                 /* 17:   1,  3 */

	/* group 12 : L2_CACHE */
	/* BUZZZ_ENUM(WBACK) */
	/* BUZZZ_ENUM(ACCESS) */
	/* BUZZZ_ENUM(MISSES) */
	/* BUZZZ_ENUM(MISS_CYCLES) */
};
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
static const char * _str_buzzz_pmon_group[BUZZZ_PMON_GROUP_MAXIMUM] =
{
	BUZZZ_ENUM(RESET)
	BUZZZ_ENUM(GENERAL)
	BUZZZ_ENUM(ICACHE)
	BUZZZ_ENUM(DCACHE)
	BUZZZ_ENUM(TLB)
	BUZZZ_ENUM(DATA)
	BUZZZ_ENUM(SPURIOUS)
	BUZZZ_ENUM(BRANCHES)
	BUZZZ_ENUM(MISCELLANEOUS)
};

static const char * _str_buzzz_pmon_event[BUZZZ_PMON_EVENT_MAXIMUM] =
{
	/* group  0: RESET */
	BUZZZ_ENUM(CTR0_SKIP)                   /* 0x00 */
	BUZZZ_ENUM(CTR1_SKIP)                   /* 0x00 */
	BUZZZ_ENUM(CTR2_SKIP)                   /* 0x00 */
	BUZZZ_ENUM(CTR3_SKIP)                   /* 0x00 */

	/* group  1: GENERAL */
	BUZZZ_ENUM(BRANCH_MISPRED)              /* 0x10 */
	BUZZZ_ENUM(CYCLES_ELAPSED)              /* 0x11 */
	BUZZZ_ENUM(EXCEPTIONS)                  /* 0x09 */
	BUZZZ_ENUM(SPEC_INSTRCNT)               /* 0x68 */

	/* group  2: ICACHE */
	BUZZZ_ENUM(INSRTUCTIONS)                /* 0x68 */
	BUZZZ_ENUM(IC_REFILL)                   /* 0x01 */
	BUZZZ_ENUM(CYCLES_IC_MISS)              /* 0x60 */
	BUZZZ_ENUM(CYCLES_NOISSUE)              /* 0x66 */

	/* group  3: DCACHE */
	BUZZZ_ENUM(DC_ACCESS)                   /* 0x04 */
	BUZZZ_ENUM(DC_REFILL)                   /* 0x03 */
	BUZZZ_ENUM(CYCLES_DC_MISS)              /* 0x61 */
	BUZZZ_ENUM(EVICTIONS)                   /* 0x65 */

	/* group  4: TLB */
	BUZZZ_ENUM(INSTR_REFILL)                /* 0x02 */
	BUZZZ_ENUM(DATA_REFILL)                 /* 0x05 */
	BUZZZ_ENUM(CYCLES_ITLB_MISS)            /* 0x82 */
	BUZZZ_ENUM(CYCLES_DTLB_MISS)            /* 0x83 */

	/* group  5: DATA */
	BUZZZ_ENUM(READ_ACCESS)                 /* 0x06 */
	BUZZZ_ENUM(WRITE_ACCESS)                /* 0x07 */
	BUZZZ_ENUM(CYCLES_WRITE)                /* 0x81 */
	BUZZZ_ENUM(CYCLES_DMB)                  /* 0x86 */

	/* group  6: SPURIOUS */
	BUZZZ_ENUM(INTERRUPTS)                  /* 0x93 */
	BUZZZ_ENUM(UNALIGNED)                   /* 0x0F */
	BUZZZ_ENUM(EXCEPTION_RTN)               /* 0x0A */
	BUZZZ_ENUM(CYCLES_TLB_MISS)             /* 0x62 */

	/* group  7: BRANCHES */
	BUZZZ_ENUM(SW_PC_CHANGE)                /* 0x0C */
	BUZZZ_ENUM(IMMED_BRANCHES)              /* 0x0D */
	BUZZZ_ENUM(PROCEDURE_RTN)               /* 0x0E */
	BUZZZ_ENUM(PRED_BRANCHES)               /* 0x12 */

	/* group  8: MISCELLANEOUS */
	BUZZZ_ENUM(STREX_PASSED)                /* 0x63 */
	BUZZZ_ENUM(STREX_FAILED)                /* 0x64 */
	BUZZZ_ENUM(DSB_INSTR)                   /* 0x91 */
	BUZZZ_ENUM(DMB_INSTR)                   /* 0x92 */
};
#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_pmon_group(uint32_t group)
{
	return (group >= BUZZZ_PMON_GROUP_MAXIMUM) ? _str_INV :
		_str_buzzz_pmon_group[group];
}
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_pmon_event(uint32_t event)
{
	return (event >= BUZZZ_PMON_EVENT_MAXIMUM) ? _str_INV :
		_str_buzzz_pmon_event[event];
}

static const char * _str_buzzz_kevt_group[BUZZZ_KEVT_GROUP_MAXIMUM] =
{
	BUZZZ_ENUM(RESET)
	BUZZZ_ENUM(GENERAL)
	BUZZZ_ENUM(ICACHE)
	BUZZZ_ENUM(DCACHE)
	BUZZZ_ENUM(TLB)
	BUZZZ_ENUM(BRANCH)
};

static const char * _str_buzzz_kevt_event[BUZZZ_KEVT_EVENT_MAXIMUM] =
{
	/* group  0: RESET */
	BUZZZ_ENUM(CTR0_NONE)                   /* 0x00 */
	BUZZZ_ENUM(CTR1_NONE)                   /* 0x00 */

	/* group  1: GENERAL */
	BUZZZ_ENUM(SPEC_INSTRCNT)               /* 0x68 */
	BUZZZ_ENUM(CYCLES_ELAPSED)              /* 0x11 */

	/* group  2: ICACHE */
	BUZZZ_ENUM(INSTRUCTIONS)                /* 0x68 */
	BUZZZ_ENUM(IC_REFILL)                   /* 0x01 */

	/* group  3: DCACHE */
	BUZZZ_ENUM(DC_ACCESS)                   /* 0x04 */
	BUZZZ_ENUM(DC_REFILL)                   /* 0x03 */

	/* group  4: TLB */
	BUZZZ_ENUM(INSTR_REFILL)                /* 0x02 */
	BUZZZ_ENUM(DATA_REFILL)                 /* 0x05 */

	/* group  5: MISCELLANEOUS */
	BUZZZ_ENUM(BRANCH_MISPRED)              /* 0x10 */
	BUZZZ_ENUM(STREX_FAILED)                /* 0x64 */
};

static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_kevt_group(uint32_t group)
{
	return (group >= BUZZZ_KEVT_GROUP_MAXIMUM) ? _str_INV :
	        _str_buzzz_kevt_group[group];
}
static BUZZZ_INLINE const char * BUZZZ_NOINSTR_FUNC
str_buzzz_kevt_event(uint32_t event)
{
	return (event >= BUZZZ_KEVT_EVENT_MAXIMUM) ? _str_INV :
	        _str_buzzz_kevt_event[event];
}


/*
 * -----------------------------------------------------------------------------
 * BUZZZ Logging Infrastructure
 * -----------------------------------------------------------------------------
 */

/* #define BUZZZ_PMON_LOGS 64-1  maximum number of buzzz_pmon_log() logs */

/* Performance Monitoring Tool private definitions */
typedef
struct buzzz_pmon_ctl
{
	uint8_t u8[BUZZZ_PMON_COUNTERS];
} buzzz_pmon_ctl_t;

typedef
struct buzzz_pmon_ctr
{
	uint32_t u32[4];
} buzzz_pmon_ctr_t;

typedef
struct buzzz_pmonst
{
	buzzz_pmon_ctr_t min, max, sum;
} buzzz_pmonst_t;

#if defined(BUZZZ_CONFIG_PMON_USR)
unsigned int buzzz_pmon_usr_g;

typedef
struct buzzz_pmon_usr
{
	unsigned int min, max, sum;
} buzzz_pmon_usr_t;

typedef
struct buzzz_pmonst_usr
{
	buzzz_pmon_usr_t run;
	buzzz_pmon_usr_t mon[BUZZZ_PMON_GROUPS];
} buzzz_pmonst_usr_t;
#endif	/*  BUZZZ_CONFIG_PMON_USR */

typedef /* Buzzz tool private configuration parameters and state */
struct buzzz_priv
{
	union
	{
		struct
		{
			uint32_t count;             /* count of logs */
			uint32_t limit;             /* limit functions logged @start */
			uint32_t indent;            /* indented entry exit printing */
			uint32_t config_exit;       /* function exit logging */

			uint32_t log_count;         /* used by proc filesystem */
			uint32_t log_index;         /* used by proc filesystem */
		} func;

		struct
		{
			uint32_t groupid;           /* current group */
			buzzz_pmon_ctl_t control;   /* current groups configuration */

			uint32_t sample;            /* iteration for this group */
			uint32_t next_log_id;       /* next log id */
			uint32_t last_log_id;       /* last pmon log */

			buzzz_pmon_ctr_t log[BUZZZ_PMON_LOGS + 1];
			buzzz_pmonst_t   run[BUZZZ_PMON_LOGS + 1];
			buzzz_pmonst_t   mon[BUZZZ_PMON_GROUPS][BUZZZ_PMON_LOGS + 1];

#if defined(BUZZZ_CONFIG_PMON_USR)
			buzzz_pmonst_usr_t usr;
#endif	/*  BUZZZ_CONFIG_PMON_USR */

			uint32_t config_skip;
			uint32_t config_samples;
		} pmon;

		struct
		{
			uint32_t count;             /* count of logs */
			uint32_t limit;             /* limit functions logged @start */
			uint32_t config_evt;        /* kevt logging perf event */

			uint32_t log_count;         /* used by proc filesystem */
			uint32_t log_index;         /* used by proc filesystem */
			bool     skip;
		} kevt;

	};
} buzzz_priv_t;


typedef /* Buzzz global structure */
struct buzzz
{
#if defined(CONFIG_SMP)
	spinlock_t      lock;
#endif  /*  CONFIG_SMP */
	buzzz_status_t  status;     /* current tool/user status */
	buzzz_tool_t    tool;       /* current tool/user of log buffer */
	uint8_t         panic;      /* auto dump on kernel panic */
	uint8_t         wrap;       /* log buffer wrapped */
	uint16_t        run;        /* tool/user incarnation number */

	void *          cur;        /* pointer to next log entry */
	void *          end;        /* pointer to end of log entry */
	void *          log;

	buzzz_priv_t    priv;       /* tool specific private data */

	struct timer_list timer;

	buzzz_mode_t    config_mode;    /* limited, continuous wrapover */
	uint32_t        config_limit;   /* configured limit */

	buzzz_fmt_t     klogs[BUZZZ_KLOG_MAXIMUM];

	char            page[4096];
} buzzz_t;

static buzzz_t buzzz_g =        /* Global Buzzz object, see __init_buzzz() */
{
#if defined(CONFIG_SMP)
	.lock   = __SPIN_LOCK_UNLOCKED(.lock),
#endif  /*  CONFIG_SMP */
	.tool   = BUZZZ_TOOL_UNDEF,
	.status = BUZZZ_STATUS_DISABLED,

	.wrap   = BUZZZ_FALSE,
	.run    = 0U,
	.cur    = (void *)NULL,
	.end    = (void *)NULL,
	.log    = (void*)NULL,

	.timer  = TIMER_INITIALIZER(NULL, 0, (int)&buzzz_g)
};

static DEFINE_PER_CPU(buzzz_status_t, kevt_status) = BUZZZ_STATUS_DISABLED;

#define BUZZZ_ASSERT_STATUS_DISABLED()                                         \
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {                             \
		printk(CLRwarn "WARN: %s tool already enabled" CLRnl, __FUNCTION__);   \
		return BUZZZ_ERROR;                                                    \
	}

static void BUZZZ_NOINSTR_FUNC /* Preamble to start tracing in a tool */
_buzzz_pre_start(void)
{
	buzzz_g.panic = BUZZZ_FALSE;
	buzzz_g.wrap  = BUZZZ_FALSE;
	buzzz_g.run  += 1U;
	buzzz_g.cur   = buzzz_g.log;
	buzzz_g.end   = (void*)((char*)buzzz_g.log
		+ (BUZZZ_LOG_BUFSIZE - BUZZZ_LOGENTRY_MAXSZ));
}

static void BUZZZ_NOINSTR_FUNC /* Postamble to start tracing in a tool */
_buzzz_post_start(void)
{
	buzzz_g.status = BUZZZ_STATUS_ENABLED;
}

static void BUZZZ_NOINSTR_FUNC /* Preamble to stop tracing in a tool */
_buzzz_pre_stop(void)
{
	buzzz_g.status = BUZZZ_STATUS_DISABLED;
}

static void BUZZZ_NOINSTR_FUNC /* Postamble to stop tracing in a tool */
_buzzz_post_stop(void)
{
}

typedef int (*buzzz_dump_log_fn_t)(char * page, void *log);

void BUZZZ_NOINSTR_FUNC /* Dump the kevt trace to console */
buzzz_log_dump(uint32_t limit, uint32_t count,
               uint32_t log_size, buzzz_dump_log_fn_t dump_log_fn)
{
	uint32_t total;
	void * log;

	if (buzzz_g.wrap == BUZZZ_TRUE)
		total = (BUZZZ_AVAIL_BUFSIZE / log_size);
	else
		total = count;

	BUZZZ_PRINT("limit<%u> bufsz<%u> max<%u> count<%u> wrap<%u> total<%u>"
		" log<%p> cur<%p> end<%p> log_size<%u>", limit, BUZZZ_AVAIL_BUFSIZE,
		(BUZZZ_AVAIL_BUFSIZE / log_size),
		count, buzzz_g.wrap, total,
		buzzz_g.log, buzzz_g.cur, buzzz_g.end, log_size);

	if (total > limit)
		total = limit;

	printk(CLRbold "BUZZZ_DUMP BGN total<%u>" CLRnl, total);

	if (total == 0U) {
		printk(CLRbold "BUZZZ_DUMP END" CLRnl);
		return;
	}

	if (limit != BUZZZ_INVALID) {   /* dump limited last few logs */
		uint32_t part1;

		part1 = ((uint32_t)buzzz_g.cur - (uint32_t)buzzz_g.log) / log_size;

		if (total > part1) {        /* some events prior to wrap */
			uint32_t part2;

			part2 = total - part1;  /* number of events prior to wrap */

			BUZZZ_PRINT("limit total<%u>: part2<%u> + part1<%u>",
			            total, part2, part1);

			log = (void*)((uint32_t)buzzz_g.end - (part2 * log_size));

			total -= part2;

			BUZZZ_PRINT("log<%p> part2<%u>", log, part2);
			while (part2--) {
				dump_log_fn(buzzz_g.page, log);
				printk("%s", buzzz_g.page);
				log = (void*)((uint32_t)log + log_size);
			}
		}

		log = (void*)((uint32_t)buzzz_g.cur - (total * log_size));

		BUZZZ_PRINT("log<%p> total<%u>", log, total);
		while (total--) {
			dump_log_fn(buzzz_g.page, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

	} else if (buzzz_g.wrap == BUZZZ_TRUE) {  /* all of ring buffer with wrap */

		uint32_t part1, part2;

		part1 = ((uint32_t)buzzz_g.cur - (uint32_t)buzzz_g.log) / log_size;
		part2 = ((uint32_t)buzzz_g.end - (uint32_t)buzzz_g.cur) / log_size;

		BUZZZ_PRINT("Wrap part2<%u> part1<%u>", part2, part1);

		log = (void*)buzzz_g.cur;
		BUZZZ_PRINT("log<%p> part2<%u>", log, part2);
		while (part2--) {   /* from cur to end : part2 */
			dump_log_fn(buzzz_g.page, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

		log = (void*)buzzz_g.log;
		BUZZZ_PRINT("log<%p> part1<%u>", log, part1);
		while (part1--) {   /* from log to cur : part1 */
			dump_log_fn(buzzz_g.page, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

	} else {                      /* everything in ring buffer, no wrap */

		log = (void*)buzzz_g.log; /* from log to cur */
		BUZZZ_PRINT("No Wrap log to cur: log<%p:%p> <%u>",
		            log, buzzz_g.cur, total);
		while (log < (void*)buzzz_g.cur) {
			dump_log_fn(buzzz_g.page, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

	}

	printk(CLRbold "BUZZZ_DUMP END" CLRnl);
}
/*
 * -----------------------------------------------------------------------------
 * Function Call Tracing subsystem
 * -----------------------------------------------------------------------------
 */

#define BUZZZ_FUNC_LOGISEVT     (1 << 1)
#define BUZZZ_FUNC_ADDRMASK     (~3U)
typedef
struct buzzz_func_log
{
	union {
		uint32_t    u32;
		void *      func;
		struct {
			uint32_t rsvd    :  1;
			uint32_t is_klog :  1;  /* BUZZZ_FUNC_LOGISEVT: 0:func, 1=evt */
			uint32_t args    : 14;  /* number of arguments logged */
			uint32_t id      : 16;
		} klog;

		struct {
			uint32_t is_ent  :  1;  /* entry or exit of a function */
			uint32_t is_klog :  1;  /* BUZZZ_FUNC_LOGISEVT: 0:func, 1=evt */
			uint32_t addr    : 30;  /* address of called function */
		} site;
	} arg0;

	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;

} buzzz_func_log_t;

static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
_buzzz_symbol(char *p, unsigned long address)
{
	int bytes = 0;
	unsigned long offset = 0LU;
	char * eos, symbol_buf[KSYM_NAME_LEN+1];

	sprint_symbol(symbol_buf, address);
	eos = strpbrk(symbol_buf, "+");

	if (eos == (char*)NULL)
		bytes += sprintf(p + bytes, "  %s" CLRnl, symbol_buf);
	else {
		*eos = '\0';
		sscanf(eos+1, "0x%lx", &offset);
		bytes += sprintf(p + bytes, "  %s" CLRnorm, symbol_buf);
		if (offset)
			bytes += sprintf(p + bytes, " +0x%lx", offset);
		bytes += sprintf(p + bytes, "\n");
	}
	return bytes;
}

static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
_buzzz_func_indent(char *p)
{
	int bytes = 0;
	if (buzzz_g.priv.func.config_exit) {
		uint32_t indent;
		for (indent = 0U; indent < buzzz_g.priv.func.indent; indent++)
			bytes += sprintf(p + bytes, BUZZZ_FUNC_INDENT_STRING);
	}
	return bytes;
}

static int BUZZZ_NOINSTR_FUNC
buzzz_func_dump_log(char * p, void * l)
{
	buzzz_func_log_t * log = (buzzz_func_log_t *)l;
	int bytes = 0;

	if (log->arg0.klog.is_klog) {   /* print using registered log formats */

		bytes += _buzzz_func_indent(p + bytes);   /* print indentation spaces */

		bytes += sprintf(p + bytes, "%s", CLRb);
		switch (log->arg0.klog.args) {
			case 0:
				bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id]);
				break;
			case 1:
				bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
					log->arg1);
				break;
			case 2:
				bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
					log->arg1, log->arg2);
				break;
			case 3:
				bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
					log->arg1, log->arg2, log->arg3);
				break;
		}
		bytes += sprintf(p + bytes, "%s", CLRnl);

	} else {                        /* print function call entry/exit */

		unsigned long address;      /* instruction address */

		if (log->arg0.site.is_ent)
			buzzz_g.priv.func.indent++;

		bytes += _buzzz_func_indent(p + bytes); /* print indentation spaces */

		if (log->arg0.site.is_ent)
			bytes += sprintf(p + bytes, "%s", CLRr "=>");
		else
			bytes += sprintf(p + bytes, "%s", CLRg "<=");

		if (!log->arg0.site.is_ent && (buzzz_g.priv.func.indent > 0))
			buzzz_g.priv.func.indent--;

		address = (unsigned long)(log->arg0.u32 & BUZZZ_FUNC_ADDRMASK);
		bytes += _buzzz_symbol(p + bytes, address);

	}

	return bytes;
}

#define _BUZZZ_FUNC_BGN(flags)                                                 \
	if (buzzz_g.tool != BUZZZ_TOOL_FUNC) return;                               \
	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) return;                        \
	BUZZZ_LOCK(flags);                                                         \
	if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED) {                           \
		if (buzzz_g.priv.func.limit == 0U) {                                   \
			_buzzz_pre_stop();                                                 \
			BUZZZ_UNLOCK(flags);                                               \
			return;                                                            \
		}                                                                      \
		buzzz_g.priv.func.limit--;                                             \
	}

#define _BUZZZ_FUNC_END(flags)                                                 \
	buzzz_g.cur = (void*)(((buzzz_func_log_t*)buzzz_g.cur) + 1);               \
	buzzz_g.priv.func.count++;                                                 \
	if (buzzz_g.cur >= buzzz_g.end) {                                          \
		buzzz_g.wrap = BUZZZ_TRUE;                                             \
		buzzz_g.cur = buzzz_g.log;                                             \
	}                                                                          \
	BUZZZ_UNLOCK(flags);


void BUZZZ_NOINSTR_FUNC /* -finstrument compiler stub on function entry */
__cyg_profile_func_enter(void * called, void * caller)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (uint32_t)called | 1U;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* -finstrument compiler stub on function exit */
__cyg_profile_func_exit(void * called, void * caller)
{
	unsigned long flags;
	if (buzzz_g.priv.func.config_exit == BUZZZ_DISABLE)
		return;

	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (uint32_t)called;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* embed prints with zero args in log */
buzzz_func_log0(uint32_t log_id)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (log_id << 16)
	                                             | BUZZZ_FUNC_LOGISEVT;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* embed prints with one u32 arg in log */
buzzz_func_log1(uint32_t log_id, uint32_t arg1)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (log_id << 16)
	                                             | BUZZZ_FUNC_LOGISEVT
	                                             | (1U << 2); /* 1 arg */
	((buzzz_func_log_t*)buzzz_g.cur)->arg1 = arg1;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* embed prints with two u32 args in log */
buzzz_func_log2(uint32_t log_id, uint32_t arg1, uint32_t arg2)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (log_id << 16)
	                                           | BUZZZ_FUNC_LOGISEVT
	                                           | (2U << 2); /* 2 args */
	((buzzz_func_log_t*)buzzz_g.cur)->arg1 = arg1;
	((buzzz_func_log_t*)buzzz_g.cur)->arg2 = arg2;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* embed prints with three u32 args in log */
buzzz_func_log3(uint32_t log_id, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (log_id << 16)
	                                           | BUZZZ_FUNC_LOGISEVT
	                                           | (3U << 2); /* 3 args */
	((buzzz_func_log_t*)buzzz_g.cur)->arg1 = arg1;
	((buzzz_func_log_t*)buzzz_g.cur)->arg2 = arg2;
	((buzzz_func_log_t*)buzzz_g.cur)->arg3 = arg3;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* Start function call entry (exit optional) tracing */
buzzz_func_start(void)
{
	if (buzzz_g.log == (void*)NULL) {
		printk(CLRerr "ERROR: Log not allocated" CLRnl);
		return;
	}

	_buzzz_pre_start();

	buzzz_g.priv.func.count = 0U;
	buzzz_g.priv.func.indent = 0U;
	buzzz_g.priv.func.limit = buzzz_g.config_limit;

	buzzz_g.priv.func.log_count = 0U;
	buzzz_g.priv.func.log_index = 0U;

	_buzzz_post_start();
}

void BUZZZ_NOINSTR_FUNC /* Stop function call entry (exit optional) tracing */
buzzz_func_stop(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		buzzz_g.status = BUZZZ_STATUS_ENABLED;

		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_func_stop);

		_buzzz_pre_stop();

		if (buzzz_g.wrap == BUZZZ_TRUE)
			buzzz_g.priv.func.log_count
				= (BUZZZ_AVAIL_BUFSIZE / sizeof(buzzz_func_log_t));
		else
			buzzz_g.priv.func.log_count = buzzz_g.priv.func.count;
		buzzz_g.priv.func.log_index = 0U;

		_buzzz_post_stop();
	}
}

void BUZZZ_NOINSTR_FUNC /* On kernel panic, to dump N function call history */
buzzz_func_panic(void)
{
	if (buzzz_g.tool != BUZZZ_TOOL_FUNC)
		return;

	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		buzzz_func_stop();

		if (buzzz_g.panic == BUZZZ_FALSE) {
			buzzz_g.panic = BUZZZ_TRUE;
			buzzz_func_dump(BUZZZ_FUNC_PANIC_LOGS);
		}
	}
}

void BUZZZ_NOINSTR_FUNC /* Dump the function call trace to console */
buzzz_func_dump(uint32_t limit)
{
	buzzz_log_dump(limit, buzzz_g.priv.func.count, sizeof(buzzz_func_log_t),
	               (buzzz_dump_log_fn_t)buzzz_func_dump_log);
}

void BUZZZ_NOINSTR_FUNC /* Dump the function call trace to console */
buzzz_func_show(void)
{
	printk("Count:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.func.count);
	printk("Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.func.limit);
	printk("+Exit:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.func.config_exit);
	printk("+Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.config_limit);
	printk("+Mode:\t\t" CLRb "%s" CLRnl, str_buzzz_mode(buzzz_g.config_mode));
	printk("\n");
}

int BUZZZ_NOINSTR_FUNC
buzzz_func_default(void)
{
#if defined(CONFIG_BUZZZ_KEVT)
	printk(CLRerr "ERROR: BUZZZ Kernel Event Tracing Enabled" CLRnl);
	printk(CLRerr "Disable CONFIG_BUZZZ_KEVT for FUNC Tracing" CLRnl);
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_KEVT */

	buzzz_g.priv.func.count = 0U;
	buzzz_g.priv.func.limit = BUZZZ_INVALID;
	buzzz_g.priv.func.indent = 0U;
	buzzz_g.priv.func.config_exit = BUZZZ_ENABLE;
	buzzz_g.priv.func.log_count = 0U;
	buzzz_g.priv.func.log_index = 0U;

	buzzz_g.config_limit = BUZZZ_INVALID;
	buzzz_g.config_mode = BUZZZ_MODE_WRAPOVER;

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC /* API to config function exit tracing */
buzzz_func_config(uint32_t config_exit)
{
	BUZZZ_ASSERT_STATUS_DISABLED();

	buzzz_g.priv.func.config_exit = config_exit;

	return BUZZZ_SUCCESS;
}

/* Initialization of Buzzz Function tool  during loading time */
static int BUZZZ_NOINSTR_FUNC
__init _init_buzzz_func(void)
{
	if ((BUZZZ_AVAIL_BUFSIZE % sizeof(buzzz_func_log_t)) != 0)
		return BUZZZ_ERROR;

	if (sizeof(buzzz_func_log_t) != 16)
		return BUZZZ_ERROR;

	return BUZZZ_SUCCESS;
}

/*
 * -----------------------------------------------------------------------------
 * Performance Monitoring subsystem
 * -----------------------------------------------------------------------------
 */

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
/*
 * Field Descriptions for PerfCtl0-3 Register
 * b31   : M    : Reads 1 if there is another PerfCtl register after this one.
 * b15   : PCTD : Disables counting when trace mode in PDTrace is enabled
 * b11:5 : Event: Determines which event to count.
 * b4    : IE   : Set to cause interrupt when counter "overflows".
 * b3    : U    : Counts events in User mode
 * b2    : S    : Counts events when in Supervisor mode
 * b1    : K    : Counts events when in Kernel mode
 * b0    : EXL  : Counts when in Exception mode
 */

#define BUZZZ_PMON_VAL(val, mode)   (((uint32_t)(val) << 5) | (mode))

/*
 * 4 Events are grouped, allowing a single log to include 4 similar events.
 * The values to be used in each of the 4 performance counters per group are
 * listed below. These values need to be applied to the corresponding
 * performance control register in bit locations b5..b11.
 */
static
buzzz_pmon_ctl_t buzzz_pmonctl_g[BUZZZ_PMON_GROUPS] =
{
	/*  ctl0  ctl1  ctl2  ctl3 */
	{{    0U,   0U,   0U,   0U }},      /* group  0 RESET            */

	{{   56U,   0U,  58U,   1U }},      /* group  1 GENERAL          */
	{{    6U,   6U,   7U,   7U }},      /* group  2 ICACHE           */
	{{   23U,  23U,  24U,  24U }},      /* group  3 DCACHE           */
	{{    4U,   4U,  25U,  25U }},      /* group  4 TLB              */
	{{   53U,  53U,  54U,  54U }},      /* group  5 CYCLES_COMPLETED */
	{{   20U,  20U,  21U,  21U }},      /* group  6 CYCLES_ISSUE_OOO */
	{{   39U,  39U,  40U,  40U }},      /* group  7 INSTR_GENERAL    */
	{{   42U,  43U,  52U,  52U }},      /* group  8 INSTR_MISC       */
	{{   41U,  41U,  46U,  46U }},      /* group  9 INSTR_LOAD_STORE */
	{{   13U,  13U,  14U,  14U }},      /* group 10 CYCLES_IDLE_FULL */
	{{   16U,  16U,  17U,  17U }}       /* group 11 CYCLES_IDLE_WAIT */
};
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)

#define BUZZZ_PMON_VAL(val, mode)   ((uint32_t)(val))

static
buzzz_pmon_ctl_t buzzz_pmonctl_g[BUZZZ_PMON_GROUPS] =
{
	/*  ctl0  ctl1  ctl2  ctl3 */
	{{    0U,   0U,   0U,   0U }},      /* group  0 RESET            */
	{{  0x10, 0x11, 0x09, 0x68 }},      /* group  1 GENERAL          */
	{{  0x68, 0x01, 0x60, 0x66 }},      /* group  2 ICACHE           */
	{{  0x04, 0x03, 0x61, 0x65 }},      /* group  3 DCACHE           */
	{{  0x02, 0x05, 0x82, 0x83 }},      /* group  4 TLB              */
	{{  0x06, 0x07, 0x81, 0x86 }},      /* group  5 DATA             */
	{{  0x93, 0x0F, 0x0A, 0x62 }},      /* group  6 SPURIOUS         */
	{{  0x0C, 0x0D, 0x0E, 0x12 }},      /* group  7 BRANCHES         */
	{{  0x63, 0x64, 0x91, 0x92 }}       /* group  8 MISCELLANEOUS    */
};

/* BUZZZ uses ARMv7 A9 counters #2,#3,#4,#5 as BUZZZ counters #0,#1,#2,#3 */
#define BUZZZ_PMON_ARM_A9_CTR(idx)      ((idx) + 2)
union cp15_c9_REG {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t ctr0     :  1; /* reserved for rest of the system            */
		uint32_t ctr1     :  1; /* reserved for rest of the system            */
		uint32_t ctr2     :  1; /* referred to as buzzz ctr 0                 */
		uint32_t ctr3     :  1; /* referred to as buzzz ctr 1                 */
		uint32_t ctr4     :  1; /* referred to as buzzz ctr 2                 */
		uint32_t ctr5     :  1; /* referred to as buzzz ctr 3                 */
		uint32_t ctr_none : 25; /* unsupported for A9                         */
		uint32_t cycle    :  1; /* cycle count register                       */
	};
};

union cp15_c9_c12_PMCR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t enable   :  1; /* E: enable all counters                     */
		uint32_t evt_reset:  1; /* P: event counter reset (not incld PMCCNTR) */
		uint32_t clk_reset:  1; /* C: clock counter reset                     */
		uint32_t clk_div  :  1; /* D: clock divider: 0=1cycle, 1=64cycle      */
		uint32_t export_en:  1; /* X: export enable                           */
		uint32_t prohibit :  1; /* DP: disable in prohibited regions          */
		uint32_t reserved :  5; /* UNK/SBZP reserved/unknown                  */
		uint32_t counters :  5; /* N: number of event counters                */
		uint32_t id_code  :  8; /* IDCODE: identification code                */
		uint32_t impl_code:  8; /* IMP: implementer code                      */
	};
};

union cp15_c9_c12_PMSELR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t ctr_select: 5; /* event counter selecter                     */
		uint32_t reserved  :25; /* reserved                                   */
	};
};

union cp15_c9_c13_PMXEVTYPER {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t evt_type :  8; /* event type to count                        */
		uint32_t reserved : 24; /* reserved                                   */
	};
};

union cp15_c9_c14_PMUSERENR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t enable   :  1; /* user mode enable                           */
		uint32_t reserved : 31; /* reserved                                   */
	};
};

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
_armv7_pmcr_RD(void)
{
	union cp15_c9_c12_PMCR pmcr;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(pmcr.u32));
	BUZZZ_PRINT("RD PMCR IMP%u ID%u N[%u] DP[%u] X[%u] D[%u] C[%u] P[%u] E[%u]",
		pmcr.impl_code, pmcr.id_code, pmcr.counters, pmcr.prohibit,
		pmcr.export_en, pmcr.clk_div, pmcr.clk_reset, pmcr.evt_reset,
		pmcr.enable);
	return pmcr.u32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_armv7_pmcr_WR(const uint32_t v32)
{
	union cp15_c9_c12_PMCR pmcr;
	pmcr.u32 = v32 & 0x3F;	/* write-able bits */
	BUZZZ_PRINT("WR PMCR: DP[%u] X[%u] D[%u] C[%u] P[%u] E[%u]",
		pmcr.prohibit, pmcr.export_en, pmcr.clk_div, pmcr.clk_reset,
		pmcr.evt_reset, pmcr.enable);
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(pmcr.u32));
}

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
_armv7_pmcntenset_test(void)
{
	/* Test whether PMU counters required by buzzz are in use */
	union cp15_c9_REG pmcntenset;

	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset.u32));
	return (pmcntenset.ctr2 || pmcntenset.ctr3 ||
		pmcntenset.ctr4 || pmcntenset.ctr5);
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_armv7_pmuserenr_enable(void)
{
	uint32_t u32 = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(u32));
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_armv7_pmcn_enable(void)
{
	union cp15_c9_c12_PMCR pmcr;
	union cp15_c9_REG pmcntenset;
	union cp15_c9_REG pmintenclr;

	/* Set Enable bit in PMCR */
	pmcr.u32 = _armv7_pmcr_RD();
	pmcr.enable = 1;
	_armv7_pmcr_WR(pmcr.u32);

	/* Disable overflow interrupts on 4 buzzz counters: A9 ctr2 to ctr5 */
	pmintenclr.u32 = 0U;
	pmintenclr.ctr2 = pmintenclr.ctr3 = pmintenclr.ctr4 = pmintenclr.ctr5 = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(pmintenclr.u32));
	BUZZZ_PRINT("Disable overflow interrupts PMINTENCLR[%08x]", pmintenclr.u32);

	/* Enable the 4 buzzz counters: A9 ctr2 to ctr5 */
	pmcntenset.u32 = 0U;
	pmcntenset.ctr2 = pmcntenset.ctr3 = pmcntenset.ctr4 = pmcntenset.ctr5 = 1;
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(pmcntenset.u32));
	BUZZZ_PRINT("Enable CTR2-5 PMCNTENSET[%08x]", pmcntenset.u32);
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_armv7_pmcn_disable(void)
{
	union cp15_c9_REG pmcntenclr;

	/* Disable the 4 buzzz counters: A9 ctr2 to ctr5 */
	pmcntenclr.u32 = 0U;
	pmcntenclr.ctr2 = pmcntenclr.ctr3 = pmcntenclr.ctr4 = pmcntenclr.ctr5 = 1;
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(pmcntenclr.u32));
	BUZZZ_PRINT("Disable CTR2-5 PMCNTENCLR[%08x]", pmcntenclr.u32);
}

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
_armv7_pmcn_read_buzzz_ctr(const uint32_t idx)
{
	uint32_t v32;
	union cp15_c9_c12_PMSELR pmselr;
	pmselr.u32 = BUZZZ_PMON_ARM_A9_CTR(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmselr.u32));
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(v32));
	return v32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_armv7_pmcn_config_buzzz_ctr(const uint32_t idx, const uint32_t evt_type)
{
	union cp15_c9_c13_PMXEVTYPER pmxevtyper;
	union cp15_c9_c12_PMSELR pmselr;
	pmselr.u32 = 0U;    /* Select the Counter using PMSELR */
	pmselr.ctr_select = BUZZZ_PMON_ARM_A9_CTR(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmselr.u32));

	pmxevtyper.u32 = 0U;
	pmxevtyper.evt_type = evt_type; /* Configure the event type */
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(pmxevtyper.u32));
	BUZZZ_PRINT("Config Ctr<%u> PMSELR[%08x] PMXEVTYPER[%08x]",
		idx, pmselr.u32, pmxevtyper.u32);
}

#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */


static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_enable(void)
{
#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	_armv7_pmcn_enable();
#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_disable(void)
{
#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	_armv7_pmcn_disable();
#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */
}

void buzzz_pmon_show(void);

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_log(uint32_t log_id)
{
	buzzz_pmon_ctr_t * log = &buzzz_g.priv.pmon.log[log_id];

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
	log->u32[0] = read_c0_perf(1);
	log->u32[1] = read_c0_perf(3);
	log->u32[2] = read_c0_perf(5);
	log->u32[3] = read_c0_perf(7);
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	log->u32[0] = _armv7_pmcn_read_buzzz_ctr(0);
	log->u32[1] = _armv7_pmcn_read_buzzz_ctr(1);
	log->u32[2] = _armv7_pmcn_read_buzzz_ctr(2);
	log->u32[3] = _armv7_pmcn_read_buzzz_ctr(3);
#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_iter_set_invalid(void)
{
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_pmon_usr_g = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
	buzzz_g.priv.pmon.log[0].u32[0] = BUZZZ_INVALID;
	buzzz_g.priv.pmon.next_log_id = 0U;
}

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
_buzzz_pmon_iter_is_invalid(void)
{
	return (buzzz_g.priv.pmon.log[0].u32[0] == BUZZZ_INVALID);
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_run(uint32_t last_log)
{
	uint32_t log, ctr;
	buzzz_pmon_ctr_t elapsed, prev, curr;

	for (log = 1U; log <= last_log; log++) {
		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++) {
			prev.u32[ctr] = buzzz_g.priv.pmon.log[log-1].u32[ctr];
			curr.u32[ctr] = buzzz_g.priv.pmon.log[log].u32[ctr];

			if (curr.u32[ctr] < prev.u32[ctr])  /* rollover */
				elapsed.u32[ctr] = curr.u32[ctr] + (~0U - prev.u32[ctr]);
			else
				elapsed.u32[ctr] = (curr.u32[ctr] - prev.u32[ctr]);

			/* update min, max and sum statistics */
			if (elapsed.u32[ctr] < buzzz_g.priv.pmon.run[log].min.u32[ctr])
				buzzz_g.priv.pmon.run[log].min.u32[ctr] = elapsed.u32[ctr];
			if (elapsed.u32[ctr] > buzzz_g.priv.pmon.run[log].max.u32[ctr])
				buzzz_g.priv.pmon.run[log].max.u32[ctr] = elapsed.u32[ctr];
			buzzz_g.priv.pmon.run[log].sum.u32[ctr] += elapsed.u32[ctr];
		}
	}

#if defined(BUZZZ_CONFIG_PMON_USR)
	if (buzzz_pmon_usr_g < buzzz_g.priv.pmon.usr.run.min)
		buzzz_g.priv.pmon.usr.run.min = buzzz_pmon_usr_g;
	if (buzzz_pmon_usr_g > buzzz_g.priv.pmon.usr.run.max)
		buzzz_g.priv.pmon.usr.run.max = buzzz_pmon_usr_g;
	buzzz_g.priv.pmon.usr.run.sum += buzzz_pmon_usr_g;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_tot(uint32_t last_log)
{
	uint32_t log, ctr;

	buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.groupid][0].sum.u32[0] = ~0U;

	for (log = 1U; log <= last_log; log++) {
		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++) {
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.groupid][log].min.u32[ctr]
				= buzzz_g.priv.pmon.run[log].min.u32[ctr];
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.groupid][log].max.u32[ctr]
				= buzzz_g.priv.pmon.run[log].max.u32[ctr];
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.groupid][log].sum.u32[ctr]
				= buzzz_g.priv.pmon.run[log].sum.u32[ctr];
		}
	}
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.groupid].min
		= buzzz_g.priv.pmon.usr.run.min;
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.groupid].max
		= buzzz_g.priv.pmon.usr.run.max;
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.groupid].sum
		= buzzz_g.priv.pmon.usr.run.sum;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_pmon_bind(uint32_t group)
{
	uint32_t mode;
	uint32_t log, ctr;
	buzzz_pmon_ctl_t * ctl;

	/* Initialize storage for statistics */
	for (log = 0U; log <= BUZZZ_PMON_LOGS; log++) {
		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++) {
			buzzz_g.priv.pmon.run[log].min.u32[ctr] = ~0U;
			buzzz_g.priv.pmon.run[log].max.u32[ctr] = 0U;
			buzzz_g.priv.pmon.run[log].sum.u32[ctr] = 0U;
		}
	}
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_g.priv.pmon.usr.run.min = ~0U;
	buzzz_g.priv.pmon.usr.run.max = 0U;
	buzzz_g.priv.pmon.usr.run.sum = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */

	/* Monitoring mode */
	mode = (group == BUZZZ_PMON_GROUP_RESET) ? 0 : BUZZZ_PMON_CTRL;

	/* Event values */
	ctl = &buzzz_pmonctl_g[group];

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
	write_c0_perf(0, BUZZZ_PMON_VAL(ctl->u8[0], mode));
	write_c0_perf(2, BUZZZ_PMON_VAL(ctl->u8[1], mode));
	write_c0_perf(4, BUZZZ_PMON_VAL(ctl->u8[2], mode));
	write_c0_perf(6, BUZZZ_PMON_VAL(ctl->u8[3], mode));
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++) {
		_armv7_pmcn_config_buzzz_ctr(ctr, ctl->u8[ctr]);
	}
#endif	/*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	BUZZZ_PRINT("Bind next group<%u>  <%u> <%u> <%u> <%u>", group,
		BUZZZ_PMON_VAL(ctl->u8[0], mode),
		BUZZZ_PMON_VAL(ctl->u8[1], mode),
		BUZZZ_PMON_VAL(ctl->u8[2], mode),
		BUZZZ_PMON_VAL(ctl->u8[3], mode));

	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.groupid = group;
}

/* Start a new sample */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_bgn(void)
{
	if (buzzz_g.status == BUZZZ_STATUS_ENABLED) {
#if defined(BUZZZ_CONFIG_PMON_USR)
		buzzz_pmon_usr_g = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
		_buzzz_pmon_log(0U);                        /* record baseline values */
		buzzz_g.priv.pmon.next_log_id = 1U;         /* for log sequence check */
	} else
		_buzzz_pmon_iter_set_invalid();
}

/* Invalidate this iteration */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_clr(void)
{
	_buzzz_pmon_iter_set_invalid();
}

/* Record performance counters at this event point */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_log(uint32_t log_id)
{
	if (buzzz_g.priv.pmon.next_log_id != log_id) {  /* check log sequence */
		_buzzz_pmon_iter_set_invalid();             /* tag sample as invalid */
		return;
	}

	_buzzz_pmon_log(log_id);                        /* record counter values */

	buzzz_g.priv.pmon.next_log_id = log_id + 1U;    /* for log sequence check */
}

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_end(uint32_t last_log_id)
{
	if (buzzz_g.status == BUZZZ_STATUS_DISABLED)
		return;

	if (last_log_id >= BUZZZ_PMON_LOGS) {
		printk(CLRerr "Too many log points<%u>, only %u permitted" CLRnl,
			last_log_id, BUZZZ_PMON_LOGS);
		buzzz_pmon_stop();
	}

	/* Event 0, counter#0 log is used to track an invalid sample */
	if (_buzzz_pmon_iter_is_invalid()) {
		BUZZZ_PRINT("invalid iteration");
		return;       /* invalid sample */
	}

	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) {
		_buzzz_pmon_iter_set_invalid();
		return;
	}

	buzzz_g.priv.pmon.last_log_id = last_log_id;
	buzzz_g.priv.pmon.sample++;

	BUZZZ_PRINT("group<%u> sample<%u> of <%u> last_log_id<%u>",
		buzzz_g.priv.pmon.groupid, buzzz_g.priv.pmon.sample,
		buzzz_g.priv.pmon.config_samples, last_log_id);

	if (buzzz_g.priv.pmon.groupid == 0) {  /* RESET or skip group ID */

		BUZZZ_PRINT("SKIP samples completed");

		if (buzzz_g.priv.pmon.sample >= buzzz_g.priv.pmon.config_samples) {

			if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
				buzzz_g.priv.pmon.groupid = buzzz_g.config_limit;
			else
				buzzz_g.priv.pmon.groupid++;

			_buzzz_pmon_bind(buzzz_g.priv.pmon.groupid);
		}
		return; /* exit without summing all samples for RESET group */
	}

	/* Accumulate into run, current iteration, and setup for next iteration */
	_buzzz_pmon_run(last_log_id);

	if (buzzz_g.priv.pmon.sample >= buzzz_g.priv.pmon.config_samples) {

		_buzzz_pmon_tot(last_log_id); /* record current groups sum total */

		/* Move to next group, or end if limited to single group */
		if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
			buzzz_g.priv.pmon.groupid = BUZZZ_PMON_GROUPS;
		else
			buzzz_g.priv.pmon.groupid++;

		if (buzzz_g.priv.pmon.groupid >= BUZZZ_PMON_GROUPS)
			buzzz_pmon_stop();
		else
			_buzzz_pmon_bind(buzzz_g.priv.pmon.groupid);
	}
}

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_dump(void)
{
	uint32_t group, log_id, ctr;
	buzzz_pmonst_t * log;
	buzzz_pmon_ctr_t tot;

	printk("\n\n+++++++++++++++\n+ PMON Report +\n+++++++++++++++\n\n");

	if (buzzz_g.status != BUZZZ_STATUS_DISABLED)
		printk(CLRwarn "PMon not disabled" CLRnl);

	if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
		group = buzzz_g.config_limit;
	else
		group = BUZZZ_PMON_GROUP_GENERAL;

	for (; group < BUZZZ_PMON_GROUP_MAXIMUM; group++) {

		/* Zero out the total cummulation */
		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
			tot.u32[ctr] = 0U;

		/* Print the table header */
		printk("\n\nGroup:\t" CLRb "%s\n" CLRnl, str_buzzz_pmon_group(group));

		if (group == BUZZZ_PMON_GROUP_GENERAL) {
			printk(CLRb "%15s %12s %12s %15s %12s    REGISTERED-EVENT" CLRnl,
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 1),
				"NANO_SECONDS",
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 3),
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 0),
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 2));
		} else {
			printk(CLRb "%15s %15s %15s %15s    REGISTERED-EVENT" CLRnl,
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 0),
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 1),
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 2),
				str_buzzz_pmon_event((group * BUZZZ_PMON_COUNTERS) + 3));
		}

		/* -------------- */
		/* SUM STATISTICS */
		/* -------------- */

		printk(CLRb "\n\nSamples %u Statistics",
			buzzz_g.priv.pmon.config_samples);

#if defined(BUZZZ_CONFIG_PMON_USR)
		printk(": UserCtr[%u]" CLRnl, buzzz_g.priv.pmon.usr.mon[group].sum);
#else
		printk(CLRnl);
#endif	/* !BUZZZ_CONFIG_PMON_USR */

		for (log_id = 1; log_id <= buzzz_g.priv.pmon.last_log_id; log_id++) {

			/* Print table row = per event entry */
			log = &buzzz_g.priv.pmon.mon[group][log_id];

			for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
				tot.u32[ctr] += log->sum.u32[ctr];

			if (group == BUZZZ_PMON_GROUP_GENERAL) {
				printk("%15u %12u %12u %15u %12u    %s\n",
					log->sum.u32[1],
					((log->sum.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
					log->sum.u32[3], log->sum.u32[0], log->sum.u32[2],
					buzzz_g.klogs[log_id]);
			} else {
				printk("%15u %15u %15u %15u    %s\n",
					log->sum.u32[0], log->sum.u32[1],
					log->sum.u32[2], log->sum.u32[3],
					buzzz_g.klogs[log_id]);
			}
		}

		/*
		 * Print table summary total:
		 * Total nanosecs is computed from total cycles/BUZZZ_CYCLES_PER_USEC.
		 * The total nanosecs will not include any fractions of each event's
		 * nanosec computed from cycles, and will be off by the fractions.
		 */
		if (group == BUZZZ_PMON_GROUP_GENERAL) {
			printk("\n%15u %12u %12u %15u %12u    " CLRb "%s" CLRnl,
				tot.u32[1], ((tot.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
				tot.u32[3], tot.u32[0], tot.u32[2], "Total");
		} else {
			printk("\n%15u %15u %15u %15u    " CLRb "%s" CLRnl,
				tot.u32[0], tot.u32[1], tot.u32[2], tot.u32[3], "Total");
		}

		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
				tot.u32[ctr] = 0U;

		/* -------------- */
		/* MIN STATISTICS */
		/* -------------- */

		printk(CLRb "\nMIN statistics");
#if defined(BUZZZ_CONFIG_PMON_USR)
		printk(": UserCtr[%u]" CLRnl, buzzz_g.priv.pmon.usr.mon[group].min);
#else
		printk(CLRnl);
#endif	/* !BUZZZ_CONFIG_PMON_USR */

		for (log_id = 1; log_id <= buzzz_g.priv.pmon.last_log_id; log_id++) {
			log = &buzzz_g.priv.pmon.mon[group][log_id];

			if (group == BUZZZ_PMON_GROUP_GENERAL) {
				printk("%15u %12u %12u %15u %12u    %s\n",
					log->min.u32[1],
					((log->min.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
					log->min.u32[3], log->min.u32[0], log->min.u32[2],
					buzzz_g.klogs[log_id]);
			} else {
				printk("%15u %15u %15u %15u    %s\n",
					log->min.u32[0], log->min.u32[1],
					log->min.u32[2], log->min.u32[3],
					buzzz_g.klogs[log_id]);
			}

			for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
				tot.u32[ctr] += log->min.u32[ctr];
		}

		if (group == BUZZZ_PMON_GROUP_GENERAL) {
			printk("\n%15u %12u %12u %15u %12u    " CLRb "%s" CLRnl,
				tot.u32[1], ((tot.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
				tot.u32[3], tot.u32[0], tot.u32[2], "Total");
		} else {
			printk("\n%15u %15u %15u %15u    " CLRb "%s" CLRnl,
				tot.u32[0], tot.u32[1], tot.u32[2], tot.u32[3], "Total");
		}

		for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
				tot.u32[ctr] = 0U;

		/* -------------- */
		/* MAX STATISTICS */
		/* -------------- */

		printk(CLRb "\nMAX statistics");
#if defined(BUZZZ_CONFIG_PMON_USR)
		printk(": UserCtr[%u]" CLRnl, buzzz_g.priv.pmon.usr.mon[group].max);
#else
		printk(CLRnl);
#endif	/* !BUZZZ_CONFIG_PMON_USR */

		for (log_id = 1; log_id <= buzzz_g.priv.pmon.last_log_id; log_id++) {
			log = &buzzz_g.priv.pmon.mon[group][log_id];

			if (group == BUZZZ_PMON_GROUP_GENERAL) {
				printk("%15u %12u %12u %15u %12u    %s\n",
					log->max.u32[1],
					((log->max.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
					log->max.u32[3], log->max.u32[0], log->max.u32[2],
					buzzz_g.klogs[log_id]);
			} else {
				printk("%15u %15u %15u %15u    %s\n",
					log->max.u32[0], log->max.u32[1],
					log->max.u32[2], log->max.u32[3],
					buzzz_g.klogs[log_id]);
			}

			for (ctr = 0U; ctr < BUZZZ_PMON_COUNTERS; ctr++)
				tot.u32[ctr] += log->max.u32[ctr];
		}

		if (group == BUZZZ_PMON_GROUP_GENERAL) {
			printk("\n%15u %12u %12u %15u %12u    " CLRb "%s" CLRnl,
				tot.u32[1], ((tot.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
				tot.u32[3], tot.u32[0], tot.u32[2], "Total");
		} else {
			printk("\n%15u %15u %15u %15u    " CLRb "%s" CLRnl,
				tot.u32[0], tot.u32[1], tot.u32[2], tot.u32[3], "Total");
		}

		if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
			break;
	}
}

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_start(void)
{
	_buzzz_pre_start();

	memset(buzzz_g.priv.pmon.log, 0, sizeof(buzzz_g.priv.pmon.log));
	memset(buzzz_g.priv.pmon.run, 0, sizeof(buzzz_g.priv.pmon.run));
	memset(buzzz_g.priv.pmon.mon, 0, sizeof(buzzz_g.priv.pmon.mon));

	_buzzz_pmon_bind(BUZZZ_PMON_GROUP_RESET);
	_buzzz_pmon_iter_set_invalid();

	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.next_log_id = 0U;
	buzzz_g.priv.pmon.last_log_id = 0U;

	_buzzz_pmon_enable();

	_buzzz_post_start();

	BUZZZ_PRINT("buzzz_pmon_start");
}

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_stop(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		_buzzz_pre_stop();

		_buzzz_pmon_bind(BUZZZ_PMON_GROUP_RESET);

		_buzzz_pmon_disable();

		buzzz_pmon_dump();

		_buzzz_post_stop();
	}
}

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_show(void)
{
	printk("Group:\t\t" CLRb "%s %d" CLRnl,
		str_buzzz_pmon_group(buzzz_g.priv.pmon.groupid), buzzz_g.priv.pmon.groupid);
	printk("Control:\t" CLRb "%u %u %u %u" CLRnl,
		buzzz_g.priv.pmon.control.u8[0], buzzz_g.priv.pmon.control.u8[1],
		buzzz_g.priv.pmon.control.u8[2], buzzz_g.priv.pmon.control.u8[3]);
	printk("Sample:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.sample);
	printk("NextId:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.next_log_id);
	printk("LastId:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.last_log_id);
	printk("+Skip:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.config_skip);
	printk("+Sample:\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.config_samples);
	printk("+Group:\t\t" CLRb "%s" CLRnl,
		str_buzzz_pmon_group(buzzz_g.config_limit));
	printk("+Mode:\t\t" CLRb "%s" CLRnl, str_buzzz_mode(buzzz_g.config_mode));
	printk("\n");

	printk("Groups\n\t1: General\n\t2: I-Cache\n\t3: D-Cache\n\t4: TLB\n");
	printk("\t5: Data\n\t6: Spurious\n\t7: Branches\n\t8: Miscellaneous\n\n");
}

int BUZZZ_NOINSTR_FUNC
buzzz_pmon_default(void)
{
	uint32_t evt;

#if defined(CONFIG_BUZZZ_FUNC)
	printk(CLRerr "ERROR: BUZZZ Function Call Tracing Enabled" CLRnl);
	printk(CLRerr "Disable CONFIG_BUZZZ_FUNC for PMON tool" CLRnl);
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_FUNC */

#if defined(CONFIG_BUZZZ_KEVT)
	printk(CLRerr "ERROR: BUZZZ Kernel Event Tracing Enabled" CLRnl);
	printk(CLRerr "Disable CONFIG_BUZZZ_KEVT for PMON tool" CLRnl);
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_KEVT */

#if defined(CONFIG_ARM)
	/* Ensure PMU is not being used by another subsystem */
	if (_armv7_pmcntenset_test() != 0U) {
		printk(CLRerr "PMU counters are in use" CLRnl);
		return BUZZZ_ERROR;
	}
#endif	/*  CONFIG_ARM */

	buzzz_g.priv.pmon.groupid = BUZZZ_PMON_GROUP_RESET;
	for (evt = 0; evt < BUZZZ_PMON_COUNTERS; evt++)
		buzzz_g.priv.pmon.control.u8[evt] =
			buzzz_pmonctl_g[BUZZZ_PMON_GROUP_RESET].u8[evt];
	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.next_log_id = 0U;
	buzzz_g.priv.pmon.last_log_id = 0U;
	buzzz_g.priv.pmon.config_skip = BUZZZ_PMON_SAMPLESZ;
	buzzz_g.priv.pmon.config_samples = BUZZZ_PMON_SAMPLESZ;

	buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
	buzzz_g.config_limit = BUZZZ_PMON_GROUP_GENERAL;

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_pmon_config(uint32_t config_samples, uint32_t config_skip)
{
	BUZZZ_ASSERT_STATUS_DISABLED();

	buzzz_g.priv.pmon.config_samples = config_samples;
	buzzz_g.priv.pmon.config_skip = config_skip;

	return BUZZZ_SUCCESS;
}

/* Initialization of Buzzz PMon tool during loading time */
static int BUZZZ_NOINSTR_FUNC
__init _init_buzzz_pmon(void)
{
	return BUZZZ_SUCCESS;
}

/*
 * -----------------------------------------------------------------------------
 * Kernel Event Logging subsystem
 * -----------------------------------------------------------------------------
 */
typedef
struct buzzz_kevt_log
{
	uint32_t ctr[BUZZZ_KEVT_COUNTERS];
	union {
		uint32_t    u32;
		struct {
			uint32_t core    :  1;
			uint32_t rsvd    :  7;
			uint32_t args    :  8;
			uint32_t id      : 16;
		} klog;
	} arg0;
	uint32_t arg1;
	uint32_t arg2, arg3, arg4, arg5;
} buzzz_kevt_log_t;

typedef
struct buzzz_kevt_ctl
{
	uint8_t u8[BUZZZ_KEVT_COUNTERS];
} buzzz_kevt_ctl_t;

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
static
buzzz_kevt_ctl_t buzzz_kevtctl_g[BUZZZ_KEVT_GROUPS] =
{
	/*  ctl0  ctl1 */
	{{    0U,   0U }},                  /* group  0 RESET            */
	{{  0x68, 0x11 }},                  /* group  1 GENERAL          */
	{{  0x68, 0x01 }},                  /* group  2 ICACHE           */
	{{  0x04, 0x03 }},                  /* group  3 DCACHE           */
	{{  0x02, 0x05 }},                  /* group  4 TLB              */
	{{  0x10, 0x64 }}                   /* group  5 MISCELLANEOUS    */
};
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

/* embed prints with one args in log */
#define _BUZZZ_KEVT_BGN(flags, cur)                                            \
	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) return;                        \
	BUZZZ_LOCK(flags);                                                         \
	cur = (buzzz_kevt_log_t*)buzzz_g.cur;                                      \
	cur->arg0.klog.core = raw_smp_processor_id();                              \
	cur->arg0.klog.id = log_id;

#define _BUZZZ_KEVT_END(flags)                                                 \
	buzzz_g.cur = (void*)(((buzzz_kevt_log_t*)buzzz_g.cur) + 1);               \
	buzzz_g.priv.kevt.count++;                                                 \
	if (buzzz_g.cur >= buzzz_g.end) {                                          \
		buzzz_g.wrap = BUZZZ_TRUE;                                             \
		buzzz_g.cur = buzzz_g.log;                                             \
	}                                                                          \
	BUZZZ_UNLOCK(flags);

void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log0(uint32_t log_id)
{
	unsigned long flags;
	buzzz_kevt_log_t * cur;

	_BUZZZ_KEVT_BGN(flags, cur);

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	cur->ctr[0] = _armv7_pmcn_read_buzzz_ctr(0);
	cur->ctr[1] = _armv7_pmcn_read_buzzz_ctr(1);
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	cur->arg0.klog.args = 0;

	_BUZZZ_KEVT_END(flags);
}

void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log1(uint32_t log_id, uint32_t arg1)
{
	unsigned long flags;
	buzzz_kevt_log_t * cur;

	_BUZZZ_KEVT_BGN(flags, cur);

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	cur->ctr[0] = _armv7_pmcn_read_buzzz_ctr(0);
	cur->ctr[1] = _armv7_pmcn_read_buzzz_ctr(1);
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	cur->arg0.klog.args = 1;
	cur->arg1 = arg1;

	_BUZZZ_KEVT_END(flags);
}

void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log2(uint32_t log_id, uint32_t arg1, uint32_t arg2)
{
	unsigned long flags;
	buzzz_kevt_log_t * cur;

	_BUZZZ_KEVT_BGN(flags, cur);

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	cur->ctr[0] = _armv7_pmcn_read_buzzz_ctr(0);
	cur->ctr[1] = _armv7_pmcn_read_buzzz_ctr(1);
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	cur->arg0.klog.args = 2;
	cur->arg1 = arg1;
	cur->arg2 = arg2;

	_BUZZZ_KEVT_END(flags);
}

void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log3(uint32_t log_id, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
	unsigned long flags;
	buzzz_kevt_log_t * cur;

	_BUZZZ_KEVT_BGN(flags, cur);

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	cur->ctr[0] = _armv7_pmcn_read_buzzz_ctr(0);
	cur->ctr[1] = _armv7_pmcn_read_buzzz_ctr(1);
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	cur->arg0.klog.args = 3;
	cur->arg1 = arg1;
	cur->arg2 = arg2;
	cur->arg3 = arg3;

	_BUZZZ_KEVT_END(flags);
}

void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log4(uint32_t log_id, uint32_t arg1, uint32_t arg2,
                uint32_t arg3, uint32_t arg4)
{
	unsigned long flags;
	buzzz_kevt_log_t * cur;

	_BUZZZ_KEVT_BGN(flags, cur);

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	cur->ctr[0] = _armv7_pmcn_read_buzzz_ctr(0);
	cur->ctr[1] = _armv7_pmcn_read_buzzz_ctr(1);
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	cur->arg0.klog.args = 4;
	cur->arg1 = arg1;
	cur->arg2 = arg2;
	cur->arg3 = arg3;
	cur->arg4 = arg4;

	_BUZZZ_KEVT_END(flags);
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_enable(void * none)
{
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	_armv7_pmcn_enable();
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_disable(void * none)
{
#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	_armv7_pmcn_disable();
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_bind(void * info)
{
	uint32_t mode, ctr;
	buzzz_kevt_ctl_t * ctl;
	buzzz_status_t * status;
	uint32_t group = *((uint32_t*)info);

	mode = (group == BUZZZ_PMON_GROUP_RESET) ? 0 : BUZZZ_PMON_CTRL;
	ctl = &buzzz_kevtctl_g[group];

#if defined(CONFIG_ARM) && defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	for (ctr = 0U; ctr < BUZZZ_KEVT_COUNTERS; ctr++) {
		_armv7_pmcn_config_buzzz_ctr(ctr, ctl->u8[ctr]);
	}
#endif  /*  CONFIG_ARM && BUZZZ_CONFIG_CPU_ARMV7_A9 */

	status = &per_cpu(kevt_status, raw_smp_processor_id());
	if (group == BUZZZ_KEVT_GROUP_RESET)
		*status = BUZZZ_STATUS_DISABLED;
	else
		*status = BUZZZ_STATUS_ENABLED;

	BUZZZ_PRINT("Bind group<%u>  <%u> <%u>", group,
		BUZZZ_PMON_VAL(ctl->u8[0], mode),
		BUZZZ_PMON_VAL(ctl->u8[1], mode));
}

void BUZZZ_NOINSTR_FUNC /* Start kevt tracing */
buzzz_kevt_start(void)
{
	buzzz_status_t * status;
	int cpu;
	_buzzz_pre_start();

	on_each_cpu(_buzzz_kevt_bind, &buzzz_g.priv.kevt.config_evt, 1);

	for_each_online_cpu(cpu) {
		status = &per_cpu(kevt_status, cpu);
		if (*status != BUZZZ_STATUS_ENABLED)
			printk(CLRerr "buzzz_kevt_start failed to start on cpu<%d>" CLRnl,
				cpu);
	}

	on_each_cpu(_buzzz_kevt_enable, NULL, 1);

	_buzzz_post_start();

	BUZZZ_PRINT("buzzz_kevt_start");
}

void BUZZZ_NOINSTR_FUNC /* Stop kevt tracing */
buzzz_kevt_stop(void)
{
	buzzz_kevt_group_t group = BUZZZ_KEVT_GROUP_RESET;

	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {

		buzzz_kevt_log0(BUZZZ_KEVT_ID_BUZZZ_TMR);

		_buzzz_pre_stop();

		on_each_cpu(_buzzz_kevt_bind, &group, 1);
		on_each_cpu(_buzzz_kevt_disable, NULL, 1);

		_buzzz_post_stop();
	}
}

int BUZZZ_NOINSTR_FUNC
buzzz_kevt_parse_log(char * p, int bytes, buzzz_kevt_log_t * log)
{
	switch (log->arg0.klog.id) {
		case BUZZZ_KEVT_ID_IRQ_BAD:
			bytes += sprintf(p + bytes, CLRerr "IRQ_BAD %u", log->arg1);
			break;
		case BUZZZ_KEVT_ID_IRQ_ACK_BAD:
			bytes += sprintf(p + bytes, CLRerr "IRQ_ACK_BAD %u", log->arg1);
			break;
		case BUZZZ_KEVT_ID_IRQ_MISROUTED:
			bytes += sprintf(p + bytes, CLRerr "IRQ_MISROUTED %u", log->arg1);
			break;
		case BUZZZ_KEVT_ID_IRQ_RESEND:
			bytes += sprintf(p + bytes, CLRerr "IRQ_RESEND %u", log->arg1);
			break;
		case BUZZZ_KEVT_ID_IRQ_CHECK:
			bytes += sprintf(p + bytes, CLRerr "IRQ_CHECK %u", log->arg1);
			break;
		case BUZZZ_KEVT_ID_IRQ_ENTRY:
			bytes += sprintf(p + bytes, CLRr " >> IRQ %03u   ", log->arg1);
			bytes += _buzzz_symbol(p + bytes, log->arg2);
			return bytes;
		case BUZZZ_KEVT_ID_IRQ_EXIT:
			bytes += sprintf(p + bytes, CLRr " << IRQ %03u   ", log->arg1);
			bytes += _buzzz_symbol(p + bytes, log->arg2);
			return bytes;
		case BUZZZ_KEVT_ID_SIRQ_ENTRY:
			bytes += sprintf(p + bytes, CLRm ">>> SOFTIRQ ");
			bytes += _buzzz_symbol(p + bytes, log->arg1);
			return bytes;
		case BUZZZ_KEVT_ID_SIRQ_EXIT:
			bytes += sprintf(p + bytes, CLRm "<<< SOFTIRQ ");
			bytes += _buzzz_symbol(p + bytes, log->arg1);
			return bytes;
		case BUZZZ_KEVT_ID_WORKQ_ENTRY:
			bytes += sprintf(p + bytes, CLRb ">>>>> WORKQ ");
			bytes += _buzzz_symbol(p + bytes, log->arg1);
			return bytes;
		case BUZZZ_KEVT_ID_WORKQ_EXIT:
			bytes += sprintf(p + bytes, CLRb "<<<<< WORKQ ");
			bytes += _buzzz_symbol(p + bytes, log->arg1);
			return bytes;
		case BUZZZ_KEVT_ID_SCHEDULE:
		{
			struct task_struct * ptsk, *ntsk;
			ptsk = (struct task_struct *)log->arg1;
			ntsk = (struct task_struct *)log->arg2;
			bytes += sprintf(p + bytes, CLRc
				"TASK_SWITCH from[%s %u:%u:%u] to[%s %u:%u:%u]",
				ptsk->comm, ptsk->pid, ptsk->normal_prio, ptsk->prio,
				ntsk->comm, ntsk->pid, ntsk->normal_prio, ntsk->prio);
			break;
		}
		case BUZZZ_KEVT_ID_SCHED_TICK:
			bytes += sprintf(p + bytes, CLRb "\tscheduler tick jiffies<%u>",
			                 log->arg1);
			break;
		case BUZZZ_KEVT_ID_SCHED_HRTICK:
			bytes += sprintf(p + bytes, CLRb "sched hrtick");
			break;
		case BUZZZ_KEVT_ID_GTIMER_EVENT:
			bytes += sprintf(p + bytes, CLRb "\tgtimer ");
			bytes += _buzzz_symbol(p + bytes, log->arg1);
			return bytes;
		case BUZZZ_KEVT_ID_GTIMER_NEXT:
			bytes += sprintf(p + bytes, CLRb "\tgtimer next<%u>", log->arg1);
			break;
	}
	bytes += sprintf(p + bytes, "%s", CLRnl);
	return bytes;
}

int BUZZZ_NOINSTR_FUNC
buzzz_kevt_dump_log(char * p, buzzz_kevt_log_t * log)
{
	static uint32_t core_cnt[NR_CPUS][BUZZZ_KEVT_COUNTERS]; /* [core][cntr] */
	static uint32_t nsecs[NR_CPUS];

	int bytes = 0;
	uint32_t curr[BUZZZ_KEVT_COUNTERS], prev[BUZZZ_KEVT_COUNTERS];
	uint32_t delta[BUZZZ_KEVT_COUNTERS], ctr;
	delta[1] = 0U;	/* compiler warning */

	for (ctr = 0U; ctr < BUZZZ_KEVT_COUNTERS; ctr++) {
		prev[ctr] = core_cnt[log->arg0.klog.core][ctr];
		curr[ctr] = log->ctr[ctr];
		core_cnt[log->arg0.klog.core][ctr] = curr[ctr];

		if (curr[ctr] < prev[ctr])  /* rollover */
			delta[ctr] = curr[ctr] + (~0U - prev[ctr]);
		else
			delta[ctr] = (curr[ctr] - prev[ctr]);
	}

	/* HACK: skip first event that simply fills starting values into core_cnt */
	if (buzzz_g.priv.kevt.skip == true) {
		buzzz_g.priv.kevt.skip = false;
		nsecs[0] = nsecs[1] = 0U;
		return bytes;
	} else if ((delta[0] > 1000000000) || (delta[1] > 1000000000)) {
		bytes += sprintf(p + bytes, CLRerr "---skipping log bug? ---" CLRnl);
		return bytes;
	}

	nsecs[log->arg0.klog.core] += (delta[1] * 1000) / BUZZZ_CYCLES_PER_USEC;

	if (buzzz_g.priv.kevt.config_evt == BUZZZ_KEVT_GROUP_GENERAL) {
		uint32_t nanosecs = ((delta[1] * 1000) / BUZZZ_CYCLES_PER_USEC)
		                    - BUZZZ_KEVT_NANOSECS;

		bytes += sprintf(p + bytes, "%s%3u%s %8u %5u.%03u %6u.%03u\t",
			(log->arg0.klog.core == 0)? CLRyk : CLRck,
			log->arg0.klog.core, CLRnorm,
			delta[0], nanosecs / 1000, (nanosecs % 1000),
		    nsecs[log->arg0.klog.core] / 1000,
			nsecs[log->arg0.klog.core] % 1000);
	} else {
		bytes += sprintf(p + bytes, "%s%3u%s %8u %8u\t",
			(log->arg0.klog.core == 0)? CLRyk : CLRck,
			log->arg0.klog.core, CLRnorm,
			delta[0], delta[1]);
	}

	if (log->arg0.klog.id < BUZZZ_KEVT_ID_MAXIMUM)
		return buzzz_kevt_parse_log(p, bytes, log);

	bytes += sprintf(p + bytes, CLRg);

	switch (log->arg0.klog.args) {
		case 0:
			bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id]);
			break;
		case 1:
			bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
			                 log->arg1);
			break;
		case 2:
			bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
			                 log->arg1, log->arg2);
			break;
		case 3:
			bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
			                 log->arg1, log->arg2, log->arg3);
			break;
		case 4:
			bytes += sprintf(p + bytes, buzzz_g.klogs[log->arg0.klog.id],
			                 log->arg1, log->arg2, log->arg3, log->arg4);
			break;
		default:
			break;
	}

	bytes += sprintf(p + bytes, "%s", CLRnl);
	return bytes;
}

void BUZZZ_NOINSTR_FUNC	/* Dump the format line */
buzzz_kevt_dump_format(void)
{
	uint32_t group = buzzz_g.priv.kevt.config_evt;

	if (buzzz_g.priv.kevt.config_evt == BUZZZ_KEVT_GROUP_GENERAL) {
		printk("Format: CPU INSTR_CNT DELTA_MICROSECS* CUMM_MICROSECS* INFO\n");
		printk("*Overhead 45-55 nanosecs per row, subtracted %u\n",
		       BUZZZ_KEVT_NANOSECS);
	} else {
		printk("Format: CPU %s %s INFO\n",
		       str_buzzz_kevt_event((group * BUZZZ_KEVT_COUNTERS) + 0),
		       str_buzzz_kevt_event((group * BUZZZ_KEVT_COUNTERS) + 1));
	}
}

void BUZZZ_NOINSTR_FUNC	/* Dump the kevt trace to console */
buzzz_kevt_dump(uint32_t limit)
{
	buzzz_g.priv.kevt.skip = true;

	buzzz_kevt_dump_format();

	buzzz_log_dump(limit, buzzz_g.priv.kevt.count, sizeof(buzzz_kevt_log_t),
	               (buzzz_dump_log_fn_t)buzzz_kevt_dump_log);

	buzzz_kevt_dump_format();
}

void BUZZZ_NOINSTR_FUNC /* Dump the kevt trace to console */
buzzz_kevt_show(void)
{
	printk("Count:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.kevt.count);
	printk("Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.kevt.limit);
	printk("+Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.config_limit);
	printk("+Mode:\t\t" CLRb "%s" CLRnl, str_buzzz_mode(buzzz_g.config_mode));
	printk("Group:\t\t" CLRb "%s" CLRnl,
	       str_buzzz_kevt_group(buzzz_g.priv.kevt.config_evt));
	printk("\n");
}

int BUZZZ_NOINSTR_FUNC
buzzz_kevt_default(void)
{
#if defined(CONFIG_BUZZZ_FUNC)
	printk(CLRerr "ERROR: BUZZZ Function Call Tracing Enabled" CLRnl);
	printk(CLRerr "Disable CONFIG_BUZZZ_FUNC for KEVT tracing" CLRnl);
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_FUNC */

#if !defined(CONFIG_ARM)
	printk(CLRerr "BUZZZ::KEVT not ported to MIPS" CLRnl);
	return BUZZZ_ERROR;
#else  /*  CONFIG_ARM */
	if (_armv7_pmcntenset_test() != 0U) {
		printk(CLRerr "PMU counters are in use" CLRnl);
		return BUZZZ_ERROR;
	}
#endif  /* !CONFIG_ARM */

	buzzz_g.priv.kevt.count = 0U;
	buzzz_g.priv.kevt.limit = BUZZZ_INVALID;
	buzzz_g.priv.kevt.config_evt = BUZZZ_KEVT_GROUP_GENERAL;
	buzzz_g.priv.kevt.log_count = 0U;
	buzzz_g.priv.kevt.log_index = 0U;

	buzzz_g.config_mode = BUZZZ_MODE_WRAPOVER;
	buzzz_g.config_limit = BUZZZ_INVALID;

	return BUZZZ_SUCCESS;
}


int BUZZZ_NOINSTR_FUNC /* API to config kevt performance counter group */
buzzz_kevt_config(uint32_t config_evt)
{
	BUZZZ_ASSERT_STATUS_DISABLED();

	if ((config_evt == BUZZZ_KEVT_GROUP_RESET) ||
	    (config_evt >= BUZZZ_KEVT_GROUP_MAXIMUM)) {
		printk(CLRwarn "Invalid event group<%u>" CLRnl, config_evt);
		return BUZZZ_ERROR;
	}

	buzzz_g.priv.kevt.config_evt = config_evt;

	printk("buzzz_kevt_config %s\n",
	       str_buzzz_kevt_group(buzzz_g.priv.kevt.config_evt));

	return BUZZZ_SUCCESS;
}

/* Initialization of Buzzz Kevt tool during loading time */
static int BUZZZ_NOINSTR_FUNC
__init _init_buzzz_kevt(void)
{
	if ((BUZZZ_AVAIL_BUFSIZE % sizeof(buzzz_kevt_log_t)) != 0)
		return BUZZZ_ERROR;

	if (sizeof(buzzz_kevt_log_t) != 32)
		return BUZZZ_ERROR;

	return BUZZZ_SUCCESS;
}

#if defined(BUZZZ_CONFIG_UNITTEST)
static void BUZZZ_NOINSTR_FUNC
buzzz_func_unittest(void)
{
	/* register events in _init function */
	buzzz_klog_reg(100, "Event 100 with no arguments");
	buzzz_klog_reg(101, "Event 101 with argument <%u>");
	buzzz_klog_reg(102, "Event 102 with argument <%u> <%u>");
	buzzz_klog_reg(103, "Event 103 with argument <%u> <%u> <%u>");

	/* invoke logging embedded in code */
	buzzz_func_log0(100);
	buzzz_func_log1(101, 1);
	buzzz_func_log2(102, 1, 2);
	buzzz_func_log3(103, 1, 2, 3);
	buzzz_stop(0U);
}

static void BUZZZ_NOINSTR_FUNC
buzzz_pmon_unittest(void)
{
	int i;
	buzzz_klog_reg(1,  "udelay  1sec");
	buzzz_klog_reg(2,  "udelay  2secs");
	buzzz_klog_reg(3,  "udelay  3secs");
	buzzz_klog_reg(4,  "udelay  4secs");
	buzzz_klog_reg(5,  "udelay  5secs");
	buzzz_klog_reg(6,  "udelay  6secs");
	buzzz_klog_reg(7,  "udelay  7secs");
	buzzz_klog_reg(8,  "udelay  8secs");
	buzzz_klog_reg(9,  "udelay  9secs");
	buzzz_klog_reg(10, "udelay 10secs");

	buzzz_klog_reg(11, "Invoke PMON Log");
	buzzz_klog_reg(12, "Invoke PMON Log");
	buzzz_klog_reg(13, "Invoke PMON Log");
	buzzz_klog_reg(14, "Invoke PMON Log");
	buzzz_klog_reg(15, "Invoke PMON Log");
	buzzz_klog_reg(16, "Invoke PMON Log");
	buzzz_klog_reg(17, "Invoke PMON Log");
	buzzz_klog_reg(18, "Invoke PMON Log");
	buzzz_klog_reg(19, "Invoke PMON Log");
	buzzz_klog_reg(20, "Invoke PMON Log");

	for (i = 0; (i < buzzz_g.priv.pmon.config_samples * 16); i++) {

		buzzz_pmon_bgn();

		udelay(1);  buzzz_pmon_log(1);
		udelay(2);  buzzz_pmon_log(2);
		udelay(3);  buzzz_pmon_log(3);
		udelay(4);  buzzz_pmon_log(4);
		udelay(5);  buzzz_pmon_log(5);
		udelay(6);  buzzz_pmon_log(6);
		udelay(7);  buzzz_pmon_log(7);
		udelay(8);  buzzz_pmon_log(8);
		udelay(9);  buzzz_pmon_log(9);
		udelay(10); buzzz_pmon_log(10);

		/* Measure cost of buzzz_pmon_log : do nothing between calls */
		buzzz_pmon_log(11);
		buzzz_pmon_log(12);
		buzzz_pmon_log(13);
		buzzz_pmon_log(14);
		buzzz_pmon_log(15);
		buzzz_pmon_log(16);
		buzzz_pmon_log(17);
		buzzz_pmon_log(18);
		buzzz_pmon_log(19);
		buzzz_pmon_log(20);

		buzzz_pmon_end(20);
	}
}

static void BUZZZ_NOINSTR_FUNC
buzzz_kevt_unittest(void)
{
	/* register events in _init function */
	buzzz_klog_reg(900, "Event argument");
	buzzz_klog_reg(901, "Event argument<%u>");
	buzzz_klog_reg(902, "Event argument<%u %u>");
	buzzz_klog_reg(903, "Event argument<%u %u %u>");
	buzzz_klog_reg(904, "Event argument<%u %u %u %u>");

	/* invoke logging embedded in code */
	buzzz_kevt_log0(900);
	buzzz_kevt_log0(900);
	buzzz_kevt_log0(900);
	buzzz_kevt_log0(900);
	buzzz_kevt_log0(900);
	buzzz_kevt_log1(901, 1);
	buzzz_kevt_log1(901, 1);
	buzzz_kevt_log1(901, 1);
	buzzz_kevt_log1(901, 1);
	buzzz_kevt_log1(901, 1);
	buzzz_kevt_log2(902, 1, 22);
	buzzz_kevt_log2(902, 1, 22);
	buzzz_kevt_log2(902, 1, 22);
	buzzz_kevt_log2(902, 1, 22);
	buzzz_kevt_log2(902, 1, 22);
	buzzz_kevt_log3(903, 1, 22, 333);
	buzzz_kevt_log3(903, 1, 22, 333);
	buzzz_kevt_log3(903, 1, 22, 333);
	buzzz_kevt_log3(903, 1, 22, 333);
	buzzz_kevt_log3(903, 1, 22, 333);
	buzzz_kevt_log4(904, 1, 22, 333, 4444);
	buzzz_kevt_log4(904, 1, 22, 333, 4444);
	buzzz_kevt_log4(904, 1, 22, 333, 4444);
	buzzz_kevt_log4(904, 1, 22, 333, 4444);
	buzzz_kevt_log4(904, 1, 22, 333, 4444);
}
#endif  /*  BUZZZ_CONFIG_UNITTEST */


#if defined(CONFIG_PROC_FS)
/*
 * -----------------------------------------------------------------------------
 * BUZZZ Proc Filesystem interface
 * -----------------------------------------------------------------------------
 */
static int BUZZZ_NOINSTR_FUNC /* Invoked on single-open, presently */
buzzz_proc_sys_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, CLRbold "%s" BUZZZ_VER_FMTS CLRnl,
		BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));
	seq_printf(seq, "Status:\t\t" CLRb "%s" CLRnl,
		str_buzzz_status(buzzz_g.status));
	seq_printf(seq, "Tool:\t\t" CLRb "%s" CLRnl,
		str_buzzz_tool(buzzz_g.tool));
	seq_printf(seq, "Wrap:\t\t" CLRb "%u" CLRnl, buzzz_g.wrap);
	seq_printf(seq, "Run:\t\t" CLRb "%u" CLRnl, buzzz_g.run);
	seq_printf(seq, "Buf.log:\t" CLRb "0x%p" CLRnl, buzzz_g.log);
	seq_printf(seq, "Buf.cur:\t" CLRb "0x%p" CLRnl, buzzz_g.cur);
	seq_printf(seq, "Buf.end:\t" CLRb "0x%p" CLRnl, buzzz_g.end);

	return BUZZZ_SUCCESS;
}

static int BUZZZ_NOINSTR_FUNC
buzzz_proc_log_show(char * page, char **start, off_t off, int count,
	int * eof, void * data)
{
	int bytes = 0;

	*start = page;  /* prepare start of buffer for printing */

	if (buzzz_g.priv.func.log_index > buzzz_g.priv.func.log_count) {
		buzzz_g.priv.func.log_index = 0U;   /* stop proc fs, return 0B */
	} else if (buzzz_g.priv.func.log_index == buzzz_g.priv.func.log_count) {
		bytes += sprintf(page + bytes, CLRbold "BUZZZ_DUMP END" CLRnl);
		buzzz_g.priv.func.log_index++;      /* stop proc fs on next call */
	} else {                                /* return a record */
		buzzz_func_log_t *log;
		uint32_t index = buzzz_g.priv.func.log_index;

		if (buzzz_g.wrap == BUZZZ_TRUE) {
			buzzz_func_log_t *bgn = (buzzz_func_log_t*)buzzz_g.log;
			buzzz_func_log_t *cur = (buzzz_func_log_t*)buzzz_g.cur;
			buzzz_func_log_t *end = (buzzz_func_log_t*)buzzz_g.end;

			if ((cur + index) >= end)	/* wrap: start from begining */
				log = bgn + (index - (end - cur));
			else
				log = cur + index;
		} else {
			log = (buzzz_func_log_t*)buzzz_g.log + index;
		}

		if (index == 0U) {  /* Log the header */
			buzzz_g.priv.func.indent = 0U;

			bytes += sprintf(page + bytes,
				CLRbold "BUZZZ_DUMP BGN total<%u>" CLRnl,
				buzzz_g.priv.func.log_count);

			if (buzzz_g.priv.func.log_count == 0U) {
				bytes += sprintf(page + bytes, CLRbold "BUZZZ_DUMP END" CLRnl);
				goto done;
			}
		}

		bytes += buzzz_func_dump_log(page + bytes, log);

		buzzz_g.priv.func.log_index++;
	}

	/* do not place any code here */

done:
	*eof = 1;       /* end of entry */
	return bytes;   /* 0B implies end of proc fs */
}

static int BUZZZ_NOINSTR_FUNC /* Proc file system open handler */
buzzz_proc_sys_open(struct inode *inode, struct file *file)
{
	return single_open(file, buzzz_proc_sys_show, NULL);
}

static const struct file_operations buzzz_proc_sys_fops =
{
	.open       =   buzzz_proc_sys_open,
	.read       =   seq_read,
	.llseek     =   seq_lseek,
	.release    =   single_release
};

#endif  /*  CONFIG_PROC_FS */


/*
 * -----------------------------------------------------------------------------
 * BUZZZ kernel space command line handling
 * -----------------------------------------------------------------------------
 */

int BUZZZ_NOINSTR_FUNC
buzzz_kcall(uint32_t arg)
{
	switch (arg) {
#if defined(BUZZZ_CONFIG_UNITTEST)
		case 0:
			printk(" 1. func unit test\n 2. pmon unit test\n"); break;
		case 1: buzzz_func_unittest(); break;
		case 2: buzzz_pmon_unittest(); break;
		case 3: buzzz_kevt_unittest(); break;
#endif /*  BUZZZ_CONFIG_UNITTEST */

		default:
			break;
	}

	return BUZZZ_SUCCESS;
}

#if defined(CONFIG_MIPS)
#include <bcmutils.h>	/* bcm_chipname */
#include <siutils.h>	/* typedef struct si_pub si_t */
#include <hndcpu.h>		/* si_cpu_clock */
#include <asm/cpu-features.h>
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih
#endif	/*  CONFIG_MIPS */

void BUZZZ_NOINSTR_FUNC
buzzz_kernel_info(void)
{
#if defined(CONFIG_MIPS)
	unsigned int hz;
	char cn[8];
	struct cpuinfo_mips *c = &current_cpu_data;
	bcm_chipname(sih->chip, cn, 8);
	hz = si_cpu_clock(sih);
	printk("CPU: BCM%s rev %d at %d MHz\n", cn, sih->chiprev,
		(hz + 500000) / 1000000);
	printk("I-Cache %dkB, %d-way, linesize %d bytes.\n",
		c->icache.waysize * c->icache.ways, c->icache.ways, c->icache.linesz);
	printk("D-Cache %dkB, %d-way, linesize %d bytes.\n\n",
		c->dcache.waysize * c->dcache.ways, c->dcache.ways, c->dcache.linesz);
#endif	/*  CONFIG_MIPS */

	printk("%s", linux_banner);
}

int BUZZZ_NOINSTR_FUNC /* Display the runtime status */
buzzz_show(void)
{
	buzzz_kernel_info();

	printk(CLRbold "%s" BUZZZ_VER_FMTS CLRnl,
		BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));

	printk("Status:\t\t" CLRb "%s" CLRnl, str_buzzz_status(buzzz_g.status));
	printk("Wrap:\t\t" CLRb "%u" CLRnl, buzzz_g.wrap);
	printk("Run:\t\t" CLRb "%u" CLRnl, buzzz_g.run);
	printk("Buf.log:\t" CLRb "0x%p" CLRnl, buzzz_g.log);
	printk("Buf.cur:\t" CLRb "0x%p" CLRnl, buzzz_g.cur);
	printk("Buf.end:\t" CLRb "0x%p" CLRnl, buzzz_g.end);

	printk("\nTool:\t\t" CLRb "%s" CLRnl, str_buzzz_tool(buzzz_g.tool));

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_show(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_show(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_show(); break;
		default:
			break;
	}

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_start_now(buzzz_t * buzzz_p)
{
	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_start(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_start(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_start(); break;
		default:
			printk(CLRwarn "Unsupported start for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_p->timer.function = NULL;

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_start(uint32_t after)
{
	if (after == 0U) {
		return buzzz_start_now(&buzzz_g);
	}

	if (buzzz_g.timer.function != NULL) {
		printk(CLRwarn "Timer already in use, overwriting" CLRnl);
		del_timer(&buzzz_g.timer);
	}
	setup_timer(&buzzz_g.timer,
	            (timer_fn_t)buzzz_start_now, (unsigned long)&buzzz_g);
	buzzz_g.timer.expires = jiffies + after;
	add_timer(&buzzz_g.timer);
	printk("BUZZZ deferred start after %u\n", after);

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_stop_now(buzzz_t * buzzz_p)
{
	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_stop(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_stop(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_stop(); break;
		default:
			printk(CLRwarn "Unsupported stop for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}
	buzzz_p->timer.function = NULL;

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_stop(uint32_t after)
{
	if (after == 0U) {
		return buzzz_stop_now(&buzzz_g);
	}
	if (buzzz_g.timer.function != NULL) {
		printk(CLRwarn "Timer already in use, overwriting" CLRnl);
		del_timer(&buzzz_g.timer);
	}
	setup_timer(&buzzz_g.timer,
	            (timer_fn_t)buzzz_stop_now, (unsigned long)&buzzz_g);
	buzzz_g.timer.expires = jiffies + after;
	add_timer(&buzzz_g.timer);
	printk("BUZZZ deferred stop %u\n", after);

	return BUZZZ_SUCCESS;
}


int BUZZZ_NOINSTR_FUNC
buzzz_pause(void)
{
	if (buzzz_g.status == BUZZZ_STATUS_ENABLED) {
		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_pause);

		buzzz_g.status = BUZZZ_STATUS_PAUSED;
	}

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_play(void)
{
	if (buzzz_g.status == BUZZZ_STATUS_PAUSED) {
		buzzz_g.status = BUZZZ_STATUS_ENABLED;

		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_play);
	}

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC
buzzz_audit(void)
{
	printk(CLRwarn "Unsupported audit capability" CLRnl);
	return BUZZZ_ERROR;
}

int BUZZZ_NOINSTR_FUNC
buzzz_dump(uint32_t items)
{
	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_dump(items); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_dump(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_dump(items); break;
		default:
			printk(CLRwarn "Unsupported dump for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC /* Configure the tool that will use the logging system */
buzzz_config_tool(buzzz_tool_t tool)
{
	if (tool > BUZZZ_TOOL_MAXIMUM) {
		printk(CLRerr "ERROR: Invalid tool %u" CLRnl, tool);
		return BUZZZ_ERROR;
	}

	BUZZZ_ASSERT_STATUS_DISABLED();

	buzzz_g.tool = tool;

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: return buzzz_func_default();
		case BUZZZ_TOOL_PMON: return buzzz_pmon_default();
		case BUZZZ_TOOL_KEVT: return buzzz_kevt_default();
		default:
			printk(CLRerr "Unsupported mode for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC /* Configure the mode of operation of the tool */
buzzz_config_mode(buzzz_mode_t mode)
{
	if ((mode == BUZZZ_MODE_UNDEF) || (mode >= BUZZZ_MODE_MAXIMUM)) {
		printk(CLRerr "ERROR: Invalid mode %u" CLRnl, mode);
		return BUZZZ_ERROR;
	}

	BUZZZ_ASSERT_STATUS_DISABLED();

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC:
		case BUZZZ_TOOL_KEVT:
			if (mode == BUZZZ_MODE_LIMITED)
				buzzz_g.config_limit = BUZZZ_FUNC_LIMIT_LOGS;
			break;
		case BUZZZ_TOOL_PMON:
			if (mode == BUZZZ_MODE_WRAPOVER)
				buzzz_g.config_limit = BUZZZ_INVALID;
			else if (mode == BUZZZ_MODE_LIMITED)
				buzzz_g.config_limit = BUZZZ_PMON_GROUP_GENERAL;
			else {
				printk(CLRwarn "Unsupported mode %s for %s" CLRnl,
					str_buzzz_mode(mode), str_buzzz_tool(buzzz_g.tool));
				return BUZZZ_ERROR;
			}
			break;
		default:
			printk(CLRerr "Unsupported mode for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_g.config_mode = mode;

	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC /* Configure a limit parameter in the tool */
buzzz_config_limit(uint32_t limit)
{
	BUZZZ_ASSERT_STATUS_DISABLED();

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC:   /* limit number events logged from start */
		case BUZZZ_TOOL_KEVT:
			buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
			break;
		case BUZZZ_TOOL_PMON:   /* limit the pmon group */
			buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
			if (limit > BUZZZ_PMON_GROUPS) {
				printk(CLRerr "Invalid limit. max<%u>" CLRnl,
					BUZZZ_PMON_GROUPS);
				return BUZZZ_ERROR;
			}
			break;
		default:
			printk(CLRerr "Unsupported limit for tool %s" CLRnl,
				str_buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_g.config_limit = limit;

	return BUZZZ_SUCCESS;
}

void BUZZZ_NOINSTR_FUNC
buzzz_klog_reg(uint32_t klog_id, char * klog_fmt)
{
	if (klog_id < BUZZZ_KLOG_MAXIMUM)
		strncpy(buzzz_g.klogs[klog_id], klog_fmt, BUZZZ_KLOG_FMT_LENGTH-1);
	else
		printk(CLRwarn "WARN: Too many events id<%u>" CLRnl, klog_id);
}

/*
 * -----------------------------------------------------------------------------
 * BUZZZ Character Driver Ioctl handlers
 * -----------------------------------------------------------------------------
 */
static int BUZZZ_NOINSTR_FUNC /* pre ioctl handling in character driver */
buzzz_open(struct inode *inodep, struct file *filep)
{
	/* int minor = MINOR(inodep->i_rdev) & 0xf; */
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
static long BUZZZ_NOINSTR_FUNC
buzzz_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else	/* < linux-2.6.36 */
static int BUZZZ_NOINSTR_FUNC /* ioctl handler in character driver */
buzzz_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
#endif  /* < linux-2.6.36 */
{
	int ret = BUZZZ_ERROR;

	BUZZZ_PRINT("cmd<%s>", str_buzzz_ioctl(cmd));

	switch (cmd) {
		case BUZZZ_IOCTL_KCALL:
			BUZZZ_PRINT("invoke buzzz_kcall %lu", arg);
			return buzzz_kcall(arg);

		case BUZZZ_IOCTL_CONFIG_TOOL:
			BUZZZ_PRINT("invoke buzzz_config_tool %s", str_buzzz_tool(arg));
			if ((buzzz_tool_t)arg < BUZZZ_TOOL_MAXIMUM)
				return buzzz_config_tool((buzzz_tool_t)arg);
			else
				return BUZZZ_ERROR;

		case BUZZZ_IOCTL_CONFIG_MODE:
			BUZZZ_PRINT("invoke buzzz_config_mode %s", str_buzzz_mode(arg));
			if ((buzzz_mode_t)arg < BUZZZ_MODE_MAXIMUM)
				return buzzz_config_mode((buzzz_mode_t)arg);
			else
				return BUZZZ_ERROR;

		case BUZZZ_IOCTL_CONFIG_LIMIT:
			BUZZZ_PRINT("invoke buzzz_config_limit %lu", arg);
			return buzzz_config_limit(arg);

		case BUZZZ_IOCTL_CONFIG_FUNC:
			BUZZZ_PRINT("invoke buzzz_func_config %lu", arg);
			return buzzz_func_config(arg);

		case BUZZZ_IOCTL_CONFIG_PMON:
			BUZZZ_PRINT("invoke buzzz_pmon_config %lu %lu",
				(arg & 0xFFFF), (arg >> 16));
			return buzzz_pmon_config((arg & 0xFFFF), (arg >> 16));

		case BUZZZ_IOCTL_CONFIG_KEVT:
			BUZZZ_PRINT("invoke buzzz_kevt_config %lu", arg);
			return buzzz_kevt_config(arg);

		case BUZZZ_IOCTL_SHOW:
			BUZZZ_PRINT("invoke buzzz_show");
			return buzzz_show();

		case BUZZZ_IOCTL_START:
			BUZZZ_PRINT("invoke buzzz_start %lu", arg);
			return buzzz_start(arg);

		case BUZZZ_IOCTL_STOP:
			BUZZZ_PRINT("invoke buzzz_stop %lu", arg);
			return buzzz_stop(arg);


		case BUZZZ_IOCTL_PAUSE:
			BUZZZ_PRINT("invoke buzzz_pause");
			return buzzz_pause();

		case BUZZZ_IOCTL_PLAY:
			BUZZZ_PRINT("invoke buzzz_play");
			return buzzz_play();

		case BUZZZ_IOCTL_AUDIT:
			BUZZZ_PRINT("invoke buzzz_audit");
			return buzzz_audit();

		case BUZZZ_IOCTL_DUMP:
			BUZZZ_PRINT("invoke buzzz_dump %lu", arg);
			return buzzz_dump(arg);

		default:
			return -EINVAL;
	}

	return ret;
}

static int BUZZZ_NOINSTR_FUNC /* post ioct handling in character driver */
buzzz_release(struct inode *inodep, struct file *filep)
{
	return 0;
}

static const struct file_operations buzzz_fops =
{
	.open           =   buzzz_open,
	.release        =   buzzz_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	.unlocked_ioctl =   buzzz_ioctl,
	.compat_ioctl   =   buzzz_ioctl
#else	/* < linux-2.6.36 */
	.ioctl          =   buzzz_ioctl
#endif  /* < linux-2.6.36 */
};

/*
 * -----------------------------------------------------------------------------
 * BUZZZ (linked into kernel) initialization (late level)
 * Should BUZZZ be a dynamically loaded module ....?
 * -----------------------------------------------------------------------------
 */
static struct miscdevice buzzz_dev =
{
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = BUZZZ_NAME,
	.fops   = &buzzz_fops
};

/* Initialization of Buzzz during loading time */
static int BUZZZ_NOINSTR_FUNC
__init __init_buzzz(void)
{
	int err, i;
	char event_str[64];

#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *ent_sys, *ent_log;
#endif  /*  CONFIG_PROC_FS */

	/* Create a 'miscelaneous' character driver and register with sysfs */
	if ((err = misc_register(&buzzz_dev)) != 0) {
		printk(CLRerr "ERROR[%d] Register device %s" BUZZZ_VER_FMTS CLRnl, err,
			BUZZZ_DEV_PATH, BUZZZ_VER_FMT(BUZZZ_DEV_VERSION));
		return err;
	} else {
		printk(CLRb "Registered device %s" BUZZZ_VER_FMTS " <%d,%d>" CLRnl,
			BUZZZ_DEV_PATH, BUZZZ_VER_FMT(BUZZZ_DEV_VERSION),
			MISC_MAJOR, buzzz_dev.minor);
	}

	/* Allocate Buzzz buffer */
	buzzz_g.log = (void *)kmalloc(BUZZZ_LOG_BUFSIZE, GFP_KERNEL);
	if (buzzz_g.log == (void*)NULL) {
		printk(CLRerr "ERROR: Log allocation %s" BUZZZ_VER_FMTS CLRnl,
			BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));

		goto fail_dev_dereg;
	} else {
		memset(buzzz_g.log, 0, BUZZZ_LOG_BUFSIZE);
	}
	buzzz_g.cur = buzzz_g.log;
	buzzz_g.end = (void*)((char*)buzzz_g.log - BUZZZ_LOGENTRY_MAXSZ);

#if defined(CONFIG_PROC_FS)
	/* Construct a Proc filesystem entry for Buzzz */

	proc_mkdir(BUZZZ_NAME, NULL);

	ent_sys = create_proc_entry(BUZZZ_NAME "/sys", 0, NULL);
	if (ent_sys) {
		ent_sys->proc_fops = &buzzz_proc_sys_fops;
	} else {
		printk(CLRerr "ERROR: BUZZZ sys proc register %s" BUZZZ_VER_FMTS CLRnl,
			BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));
		goto fail_free_log;
	}

	ent_log = create_proc_read_entry(BUZZZ_NAME "/log", 0, NULL,
		buzzz_proc_log_show, (void*)&buzzz_g);
	if (ent_log == (struct proc_dir_entry*)NULL) {
		printk(CLRerr "ERROR: BUZZZ log proc register %s" BUZZZ_VER_FMTS CLRnl,
			BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));
		goto fail_free_log;
	}
#endif  /*  CONFIG_PROC_FS */

	if (_init_buzzz_func() == BUZZZ_ERROR) {
		printk(CLRerr "ERROR: Initialize Func Tool" CLRnl);
		goto fail_free_log;
	}

	if (_init_buzzz_pmon() == BUZZZ_ERROR) {
		printk(CLRerr "ERROR: Initialize PMon Tool" CLRnl);
		goto fail_free_log;
	}

	if (_init_buzzz_kevt() == BUZZZ_ERROR) {
		printk(CLRerr "ERROR: Initialize KEvt Tool" CLRnl);
		goto fail_free_log;
	}

	for (i = 0; i < BUZZZ_KLOG_MAXIMUM; i++) {
		sprintf(event_str, "%sUNREGISTERED EVENT<%u>%s", CLRm, i, CLRnorm);
		buzzz_klog_reg(i, event_str);
	}

	buzzz_g.status = BUZZZ_STATUS_DISABLED;

	return BUZZZ_SUCCESS;   /* Successful initialization of Buzzz */

#if defined(CONFIG_PROC_FS)
fail_free_log:
#endif  /*  CONFIG_PROC_FS */
	kfree(buzzz_g.log);

fail_dev_dereg:
	misc_deregister(&buzzz_dev);

	return BUZZZ_ERROR;     /* Failed initialization of Buzzz */
}

late_initcall(__init_buzzz);    /* init level 7 */

EXPORT_SYMBOL(buzzz_start);
EXPORT_SYMBOL(buzzz_stop);
EXPORT_SYMBOL(buzzz_pause);
EXPORT_SYMBOL(buzzz_play);
EXPORT_SYMBOL(buzzz_audit);
EXPORT_SYMBOL(buzzz_dump);
EXPORT_SYMBOL(buzzz_config_tool);
EXPORT_SYMBOL(buzzz_config_mode);
EXPORT_SYMBOL(buzzz_config_limit);
EXPORT_SYMBOL(buzzz_klog_reg);
EXPORT_SYMBOL(buzzz_kcall);

EXPORT_SYMBOL(__cyg_profile_func_enter);
EXPORT_SYMBOL(__cyg_profile_func_exit);

EXPORT_SYMBOL(buzzz_func_log0);
EXPORT_SYMBOL(buzzz_func_log1);
EXPORT_SYMBOL(buzzz_func_log2);
EXPORT_SYMBOL(buzzz_func_log3);
EXPORT_SYMBOL(buzzz_func_start);
EXPORT_SYMBOL(buzzz_func_stop);
EXPORT_SYMBOL(buzzz_func_panic);
EXPORT_SYMBOL(buzzz_func_dump);
EXPORT_SYMBOL(buzzz_func_config);

EXPORT_SYMBOL(buzzz_pmon_bgn);
EXPORT_SYMBOL(buzzz_pmon_clr);
EXPORT_SYMBOL(buzzz_pmon_log);
EXPORT_SYMBOL(buzzz_pmon_end);
EXPORT_SYMBOL(buzzz_pmon_start);
EXPORT_SYMBOL(buzzz_pmon_stop);
EXPORT_SYMBOL(buzzz_pmon_config);
#if defined(BUZZZ_CONFIG_PMON_USR)
EXPORT_SYMBOL(buzzz_pmon_usr_g);
#endif	/*  BUZZZ_CONFIG_PMON_USR */

EXPORT_SYMBOL(buzzz_kevt_log0);
EXPORT_SYMBOL(buzzz_kevt_log1);
EXPORT_SYMBOL(buzzz_kevt_log2);
EXPORT_SYMBOL(buzzz_kevt_log3);
EXPORT_SYMBOL(buzzz_kevt_log4);
EXPORT_SYMBOL(buzzz_kevt_start);
EXPORT_SYMBOL(buzzz_kevt_stop);
EXPORT_SYMBOL(buzzz_kevt_dump);
EXPORT_SYMBOL(buzzz_kevt_config);
