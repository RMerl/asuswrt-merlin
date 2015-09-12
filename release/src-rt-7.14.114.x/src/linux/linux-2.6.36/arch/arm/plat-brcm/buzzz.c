/*
 * Broadcom BCM47xx Buzzz based Kernel Profiling and Debugging
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
 *      - Performance counter monitoring of code segments,
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
 * CAUTION: PMON and KEVT tool implementation uses 4 PMU counters. The BUZZZ
 * debug print (BUZZZ_PRINT) and the buzzz_dump functions are not parameterized
 * to BUZZZ_PMU_COUNTERS.
 *
 * On ARM, it was noted, that the Linux 10 millisec Tick (HZ=100) interrupt does
 * not occur every 10 millisec. This was verified using the PMU cyclecount as
 * well as event id 0x11. The same was noted on both UP and SMP modes.
 *
 * -----------------------------------------------------------------------------
 */

#include <typedefs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
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

/*
 * Maximum length of a single log entry cannot exceed 64 bytes.
 * Implementation uses a 32Byte per log entry.
 */
#define BUZZZ_LOGENTRY_MAXSZ    (64)

/* Length of the buffer available for logging */
#define BUZZZ_AVAIL_BUFSIZE     (BUZZZ_LOG_BUFSIZE - BUZZZ_LOGENTRY_MAXSZ)

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
#define BUZZZ_PRINT(_fmt, _args...) \
	printk(CLRg "BUZZZ %s: " _fmt CLRnl, __FUNCTION__, ##_args)
#else   /* !BUZZZ_CONFIG_SYS_KDBG */
#define BUZZZ_PRINT(_fmt, _args...)    BUZZZ_NULL_STMT
#endif  /* !BUZZZ_CONFIG_SYS_KDBG */

#define BUZZZ_PRERR(_fmt, _args...) \
	printk(CLRerr "BUZZZ " BUZZZ_VER_FMTS " %s ERROR: " _fmt CLRnl, \
		BUZZZ_VER_FMT(BUZZZ_SYS_VERSION), __FUNCTION__, ##_args)


/*
 * Macro to construct a formatted log printout by incrementally converting
 * parts of a log into a 4K buffer page.
 */
/* Page for formatted printing each log entry */
#define BUZZZ_PAGE_SIZE         (4096)

#define BUZZZ_SNPRINTF(_fmt, _args...) \
	bytes += snprintf(p + bytes, (BUZZZ_PAGE_SIZE - bytes), _fmt, ##_args)

/*
 * +---------------------------------------------------------------------------+
 *  Section: Various assert on input parameters.
 * +---------------------------------------------------------------------------+
 */
/* __buzzz_assert_xyz() template */
#define BUZZZ_ASSERT(NAME, name) \
static BUZZZ_INLINE bool BUZZZ_NOINSTR_FUNC \
__buzzz_assert_##name(enum buzzz_##name name) \
{ \
	if ((name == BUZZZ_##NAME##_UNDEF) || (name >= BUZZZ_##NAME##_MAXIMUM)) { \
		BUZZZ_PRERR(#name "<%d> is out of scope", (int)name); \
		return BUZZZ_FALSE; \
	} \
	return BUZZZ_TRUE; \
}
BUZZZ_ASSERT(TOOL, tool)            /* __buzzz_assert_tool()      */
BUZZZ_ASSERT(MODE, mode)            /* __buzzz_assert_mode()      */
BUZZZ_ASSERT(PMU_GROUP, pmu_group)  /* __buzzz_assert_pmu_group() */
BUZZZ_ASSERT(KEVT_ID, kevt_id)      /* __buzzz_assert_kevt_id()   */
#define BUZZZ_ASSERT_TOOL(tool) \
	do { if (!__buzzz_assert_tool(tool)) return BUZZZ_FAILURE; } while (0)
#define BUZZZ_ASSERT_MODE(mode) \
	do { if (!__buzzz_assert_mode(mode)) return BUZZZ_FAILURE; } while (0)
#define BUZZZ_ASSERT_PMU_GROUP(group) \
	do { if (!__buzzz_assert_pmu_group(group)) return BUZZZ_FAILURE; } while (0)
#define BUZZZ_ASSERT_KEVT_ID(kevt_id) \
	do { if (!__buzzz_assert_kevt_id(kevt_id)) return BUZZZ_FAILURE; } while (0)


#if defined(CONFIG_SMP)
#define BUZZZ_LOCK(flags)       spin_lock_irqsave(&buzzz_g.lock, flags)
#define BUZZZ_UNLOCK(flags)     spin_unlock_irqrestore(&buzzz_g.lock, flags)
#else   /*  CONFIG_SMP */
#define BUZZZ_LOCK(flags)       local_irq_save(flags)
#define BUZZZ_UNLOCK(flags)     local_irq_restore(flags)
#endif  /* !CONFIG_SMP */


/*
 * +---------------------------------------------------------------------------+
 *  Section: Enum to string conversion for human readable printout
 * +---------------------------------------------------------------------------+
 */
static const char *_buzzz_INV = "INVALID";

#define BUZZZ_STR_FUNC(NAME, name) \
static BUZZZ_INLINE const char *BUZZZ_NOINSTR_FUNC \
__buzzz_##name(uint32_t name) \
{ \
	return (name >= BUZZZ_##NAME##_MAXIMUM) ? _buzzz_INV : \
		_buzzz_##name[name]; \
}

/*
 * To facilitate enum to human-readable string conversion for pretty print
 * Cut an paste the enum list
 */
#undef  BUZZZ_DESC
#define BUZZZ_DESC(val)         #val,

static const char *_buzzz_tool[BUZZZ_TOOL_MAXIMUM] =
{
	BUZZZ_DESC(UNDEF)
	BUZZZ_DESC(FUNC)        /* Function call tracing */
	BUZZZ_DESC(PMON)        /* Algorithm performance monitoring */
	BUZZZ_DESC(KEVT)        /* Kernel space event tracing */
};
BUZZZ_STR_FUNC(TOOL, tool)  /* __buzzz_tool() */

static const char *_buzzz_status[BUZZZ_STATUS_MAXIMUM] =
{
	BUZZZ_DESC(DISABLED)
	BUZZZ_DESC(ENABLED)
	BUZZZ_DESC(PAUSED)
};
BUZZZ_STR_FUNC(STATUS, status)  /* __buzzz_status() */

static const char *_buzzz_mode[BUZZZ_MODE_MAXIMUM] =
{
	BUZZZ_DESC(UNDEF)
	BUZZZ_DESC(WRAPOVER)
	BUZZZ_DESC(LIMITED)
	BUZZZ_DESC(TRANSMIT)
};
BUZZZ_STR_FUNC(MODE, mode)      /* __buzzz_mode() */

static const char *_buzzz_ioctl[BUZZZ_IOCTL_MAXIMUM] =
{
	BUZZZ_DESC(UNDEF)

	BUZZZ_DESC(CONFIG_TOOL)
	BUZZZ_DESC(CONFIG_MODE)
	BUZZZ_DESC(CONFIG_LIMIT)

	BUZZZ_DESC(CONFIG_FUNC)
	BUZZZ_DESC(CONFIG_PMON)
	BUZZZ_DESC(CONFIG_KEVT)

	BUZZZ_DESC(SHOW)
	BUZZZ_DESC(START)
	BUZZZ_DESC(STOP)
	BUZZZ_DESC(PAUSE)
	BUZZZ_DESC(PLAY)
	BUZZZ_DESC(AUDIT)
	BUZZZ_DESC(DUMP)
	BUZZZ_DESC(KCALL)
};
BUZZZ_STR_FUNC(IOCTL, ioctl)      /* __buzzz_ioctl() */


#if defined(CONFIG_MIPS)
#if defined(BUZZZ_CONFIG_CPU_MIPS_74K)
static const char *_buzzz_pmu_group[BUZZZ_PMU_GROUP_MAXIMUM] =
{
	BUZZZ_DESC(UNDEF)
	BUZZZ_DESC(GENERAL)
	BUZZZ_DESC(ICACHE)
	BUZZZ_DESC(DCACHE)
	BUZZZ_DESC(TLB)
	BUZZZ_DESC(COMPLETED)
	BUZZZ_DESC(ISSUE_OOO)
	BUZZZ_DESC(INSTR_GENERAL)
	BUZZZ_DESC(INSTR_MISCELLANEOUS)
	BUZZZ_DESC(INSTR_LOAD_STORE)
	BUZZZ_DESC(CYCLES_IDLE_FULL)
	BUZZZ_DESC(CYCLES_IDLE_WAIT)
	/* BUZZZ_DESC(L2_CACHE) */
};
BUZZZ_STR_FUNC(PMU_GROUP, pmu_group)    /* __buzzz_pmu_group() */


#undef BUZZZ_DEFN
#define BUZZZ_DEFN(VER, EVTID, EVTNAME) { EVTID, #EVTNAME }
static const buzzz_pmu_event_desc_t
buzzz_pmu_event_g[BUZZZ_PMU_GROUPS][BUZZZ_PMU_COUNTERS] =
{
	{   /* group 00: UNDEF */
		BUZZZ_DEFN(74KE, 127, NONE),
		BUZZZ_DEFN(74KE, 127, NONE),
		BUZZZ_DEFN(74KE, 127, NONE),
		BUZZZ_DEFN(74KE, 127, NONE)
	},

	{   /* group 01: GENERAL [CYCLES_ELAPSED in 3rd counter!] */
		BUZZZ_DEFN(74KE,  56, COMPL0_MISPRED), /* 0   2   */
		BUZZZ_DEFN(74KE,  53, LOAD_MISS),      /*   1   3 */
		BUZZZ_DEFN(74KE,   1, INSTR_COMPL),    /* 0 1 2 3 */
		BUZZZ_DEFN(74KE,   0, CYCLES_ELAPSED)  /* 0 1 2 3 */
	},

	{	/* group 02: ICACHE */
		BUZZZ_DEFN(74KE,   6, IC_ACCESS),      /* 0   2   */
		BUZZZ_DEFN(74KE,   6, IC_MISS),        /*   1   3 */
		BUZZZ_DEFN(74KE,   7, CYCLES_IC_MISS), /* 0   2   */
		BUZZZ_DEFN(74KE,   8, CYCLES_I_UCACC)  /* 0   2   */
	},

	{	/* group 03: DCACHE */
		BUZZZ_DEFN(74KE,  23, LD_CACHEABLE),   /* 0   2   */
		BUZZZ_DEFN(74KE,  23, DCACHE_ACCESS),  /*   1   3 */
		BUZZZ_DEFN(74KE,  24, DCACHE_WBACK),   /* 0   2   */
		BUZZZ_DEFN(74KE,  24, DCACHE_MISS)     /*   1   3 */
	},

	{	/* group 04: TLB */
		BUZZZ_DEFN(74KE,   4, ITLB_ACCESS),    /* 0   2   */
		BUZZZ_DEFN(74KE,   4, ITLB_MISS),      /*   1   3 */
		BUZZZ_DEFN(74KE,  25, JTLB_DACCESS),   /* 0   2   */
		BUZZZ_DEFN(74KE,  25, JTLB_XL_FAIL)    /*   1   3 */
	},

	{	/* group 05: COMPLETED */
		BUZZZ_DEFN(74KE,  53, COMPL0_INSTR),   /* 0   2   */
		BUZZZ_DEFN(74KE,   1, INSTR_COMPL),    /* 0 1 2 3 */
		BUZZZ_DEFN(74KE,  54, COMPL1_INSTR),   /* 0   2   */
		BUZZZ_DEFN(74KE,  54, COMPL2_INSTR)    /*   1   3 */
	},

	{	/* group 06: ISSUE_OOO */
		BUZZZ_DEFN(74KE,  20, ISS1_INSTR),     /* 0   2   */
		BUZZZ_DEFN(74KE,  20, ISS2_INSTR),     /*   1   3 */
		BUZZZ_DEFN(74KE,  21, OOO_ALU),        /* 0   2   */
		BUZZZ_DEFN(74KE,  21, OOO_AGEN)        /*   1   3 */
	},

	{	/* group 07: INSTR_GENERAL */
		BUZZZ_DEFN(74KE,  39, CONDITIONAL),    /* 0   2   */
		BUZZZ_DEFN(74KE,  39, MISPREDICTED),   /*   1   3 */
		BUZZZ_DEFN(74KE,  40, INTEGER),        /* 0   2   */
		BUZZZ_DEFN(74KE,  40, FLOAT)           /*   1   3 */
	},

	{	/* group 08: INSTR_MISCELLANEOUS */
		BUZZZ_DEFN(74KE,  42, JUMP),           /* 0   2   */
		BUZZZ_DEFN(74KE,  43, MULDIV),         /*   1   3 */
		BUZZZ_DEFN(74KE,  52, PREFETCH),       /* 0   2   */
		BUZZZ_DEFN(74KE,  52, PREFETCH_NULL)   /*   1   3 */
	},

	{	/* group 09: INSTR_LOAD_STORE */
		BUZZZ_DEFN(74KE,  41, LOAD),           /* 0   2   */
		BUZZZ_DEFN(74KE,  41, STORE),          /*   1   3 */
		BUZZZ_DEFN(74KE,  46, LOAD_UNCACHE),   /* 0   2   */
		BUZZZ_DEFN(74KE,  46, STORE_UNCACHE)   /*   1   3 */
	},

	{	/* group 10: CYCLES_IDLE_FULL */
		BUZZZ_DEFN(74KE,  13, ALU_CAND_POOL),  /* 0   2   */
		BUZZZ_DEFN(74KE,  13, AGEN_CAND_POOL), /*   1   3 */
		BUZZZ_DEFN(74KE,  14, ALU_COMPL_BUF),  /* 0   2   */
		BUZZZ_DEFN(74KE,  14, AGEN_COMPL_BUF)  /*   1   3 */
	},

	{	/* group 11: CYCLES_IDLE_WAIT */
		BUZZZ_DEFN(74KE,  16, ALU_NO_ISSUE),   /* 0   2   */
		BUZZZ_DEFN(74KE,  16, AGEN_NO_ISSUE)   /*   1   3 */
		BUZZZ_DEFN(74KE,  17, ALU_NO_OPER),    /* 0   2   */
		BUZZZ_DEFN(74KE,  17, AGEN_NO_OPER)    /*   1   3 */
	}
};
static BUZZZ_INLINE const char *BUZZZ_NOINSTR_FUNC
__buzzz_pmu_event(uint32_t group, uint32_t counter)
{
	if ((group >= BUZZZ_PMU_GROUPS) || (counter >= BUZZZ_PMU_COUNTERS))
		return _buzzz_INV;
	return buzzz_pmu_event_g[group][counter].name;
}

#endif	/*  BUZZZ_CONFIG_CPU_MIPS_74K */
#endif	/*  CONFIG_MIPS */


#if defined(CONFIG_ARM)
static const char *_buzzz_pmu_group[BUZZZ_PMU_GROUP_MAXIMUM] =
{
	BUZZZ_DESC(UNDEF)
	BUZZZ_DESC(GENERAL)
	BUZZZ_DESC(ICACHE)
	BUZZZ_DESC(DCACHE)
	BUZZZ_DESC(MEMORY)
	BUZZZ_DESC(BUS)
	BUZZZ_DESC(BRANCH)
	BUZZZ_DESC(EXCEPTION)
	BUZZZ_DESC(SPURIOUS)
	BUZZZ_DESC(PREF_COH)
	BUZZZ_DESC(L2CACHE)
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	BUZZZ_DESC(INSTRTYPE)
	BUZZZ_DESC(BARRIER)
	BUZZZ_DESC(ISSUE)
	BUZZZ_DESC(STALLS)
	BUZZZ_DESC(TLBSTALLS)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
};
BUZZZ_STR_FUNC(PMU_GROUP, pmu_group)    /* __buzzz_pmu_group() */

/*
 * CAUTION: [BCM4709] : broken ARMv7 counters on CortexA9 rev0
 * 0x13, MEM_ACCESS
 * 0x08, INST_RETIRED      (use 0x68)
 * 0x1b, INST_SPEC         (use 0x68)
 * 0x0e, BR_RETURN_RETIRED (use 0x6e)
 */
#undef BUZZZ_DEFN
#define BUZZZ_DEFN(VER, EVTID, EVTNAME) { EVTID, #EVTNAME }
static const buzzz_pmu_event_desc_t
buzzz_pmu_event_g[BUZZZ_PMU_GROUPS][BUZZZ_PMU_COUNTERS] =
{
	{	/* group 00: UNDEF */
		BUZZZ_DEFN(V7, 0x00, NONE),
		BUZZZ_DEFN(V7, 0x00, NONE),
		BUZZZ_DEFN(V7, 0x00, NONE),
		BUZZZ_DEFN(V7, 0x00, NONE)
	},

	{	/* group 01: GENERAL [CPU_CYCLE in 3rd counter!] */
		BUZZZ_DEFN(V7, 0x10, BR_MIS_PRED),
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x04, L1D_CACHE),
#else
		BUZZZ_DEFN(V7, 0x13, MEM_ACCESS),
#endif
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x68, INSTR_RENAME_STAGE),
#else
		BUZZZ_DEFN(V7, 0x08, INST_RETIRED),
#endif
		BUZZZ_DEFN(V7, 0x11, CPU_CYCLE)
	},

	{	/* group 02: ICACHE */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x68, INSTR_RENAME_STAGE),
#else
		BUZZZ_DEFN(V7, 0x08, INST_RETIRED),
#endif
		BUZZZ_DEFN(V7, 0x14, L1I_CACHE),
		BUZZZ_DEFN(V7, 0x01, L1I_CACHE_REFILL),
		BUZZZ_DEFN(V7, 0x02, L1I_TLB_REFILL)
	},

	{	/* group 03: DCACHE */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x15, L1D_CACHE_WB),
#else
		BUZZZ_DEFN(V7, 0x13, MEM_ACCESS),
#endif
		BUZZZ_DEFN(V7, 0x04, L1D_CACHE),
		BUZZZ_DEFN(V7, 0x03, L1D_CACHE_REFILL),
		BUZZZ_DEFN(V7, 0x05, L1D_TLB_REFILL)
	},

	{	/* group 04: MEMORY */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x04, L1D_CACHE),
#else
		BUZZZ_DEFN(V7, 0x13, MEM_ACCESS),
#endif
		BUZZZ_DEFN(V7, 0x15, L1D_CACHE_WB),
		BUZZZ_DEFN(V7, 0x06, LD_RETIRED),
		BUZZZ_DEFN(V7, 0x07, ST_RETIRED)
	},

	{	/* group 05: BUS */
		BUZZZ_DEFN(V7, 0x19, BUS_ACCESS),
		BUZZZ_DEFN(V7, 0x1d, BUS_CYCLE),
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A7)
		BUZZZ_DEFN(A7, 0x60, BUS_RD_ACCESS),
		BUZZZ_DEFN(A7, 0x61, BUS_WR_ACCESS)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A7 */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(A9, 0x69, DATA_LINEFILL),
		BUZZZ_DEFN(A9, 0x65, DATA_EVICTION)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
	},

	{	/* group 06: BRANCH */
		BUZZZ_DEFN(V7, 0x10, BR_MIS_PRED),
		BUZZZ_DEFN(V7, 0x12, BR_PRED),
		BUZZZ_DEFN(V7, 0x0d, BR_IMMED_RETIRED),
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(A9, 0x6e, PRED_BR_RETURN)
#else
		BUZZZ_DEFN(V7, 0x0e, BR_RETURN_RETIRED)
#endif
	},

	{	/* group 06: EXCEPTION */
		BUZZZ_DEFN(V7, 0x09, EXC_TAKEN),
		BUZZZ_DEFN(V7, 0x0a, EXC_RETURN),
		BUZZZ_DEFN(V7, 0x0b, CID_WRITE_RETIRED),
		BUZZZ_DEFN(V7, 0x0c, PC_WRITE_RETIRED)
	},

	{	/* group 07: SPURIOUS */
		BUZZZ_DEFN(V7, 0x0f, UNALIGN_LDST_RETIRED),
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A7)
		BUZZZ_DEFN(A7, 0x86, IRQ_EXC_TAKEN),
		BUZZZ_DEFN(A7, 0xc9, WR_STALL),
		BUZZZ_DEFN(A7, 0xca, DATA_SNOOP)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A7 */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(A9, 0x93, EXT_INTERRUPTS),
		BUZZZ_DEFN(A9, 0x81, WR_STALL),
		BUZZZ_DEFN(A9, 0x80, PLD_STALL)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
	},

	{	/* group 08: PREF_COH */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A7)
		BUZZZ_DEFN(A7, 0xc0, EXT_MEM_REQUEST),
		BUZZZ_DEFN(A7, 0xc1, NC_EXT_MEM_REQUEST),
		BUZZZ_DEFN(A7, 0xc2, PREF_LINEFILL),
		BUZZZ_DEFN(A7, 0xc3, PREF_LINEFILL_DROP)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A7 */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(A9, 0x51, COH_LINEFILL_HIT),
		BUZZZ_DEFN(A9, 0x50, COH_LINEFILL_MISS),
		BUZZZ_DEFN(A9, 0x6a, PREF_LINEFILL),
		BUZZZ_DEFN(A9, 0x6b, PREF_CACHE_HIT)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
	},

	{	/* group 09: L2CACHE */
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
		BUZZZ_DEFN(V7, 0x04, L1D_CACHE),
#else
		BUZZZ_DEFN(V7, 0x13, MEM_ACCESS),
#endif
		BUZZZ_DEFN(V7, 0x16, L2D_CACHE),
		BUZZZ_DEFN(V7, 0x17, L2D_CACHE_REFILL),
		BUZZZ_DEFN(V7, 0x18, L2D_CACHE_WB)
	},

#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	{	/* group 10: INSTRTYPE */
		/* ARMv7 0x08 and 0x1b do not work on Cortex-A9, use 0x68 instead */
		BUZZZ_DEFN(V7, 0x68, INSTR_RENAME_STAGE), /* best approximation */
		BUZZZ_DEFN(A9, 0x72, INSTR_LDST),
		BUZZZ_DEFN(A9, 0x70, INSTR_MAIN_UNIT),
		BUZZZ_DEFN(A9, 0x71, INSTR_ALU_UNIT)
	},

	{	/* group 11: INSTRTYPE */
		/* ARMv7 0x08 and 0x1b do not work on Cortex-A9, use 0x68 instead */
		BUZZZ_DEFN(V7, 0x68, INSTR_RENAME_STAGE), /* best approximation */
		BUZZZ_DEFN(A9, 0x90, INSTR_ISB),
		BUZZZ_DEFN(A9, 0x91, INSTR_DSB),
		BUZZZ_DEFN(A9, 0x92, INSTR_DMB)
	},

	{	/* group 12: INSTRTYPE */
		/* ARMv7 0x08 and 0x1b do not work on Cortex-A9, use 0x68 instead */
		BUZZZ_DEFN(V7, 0x68, INSTR_RENAME_STAGE), /* best approximation */
		BUZZZ_DEFN(A9, 0x68, INSTR_RENAME_STAGE),
		BUZZZ_DEFN(A9, 0x66, ISSUE_NONE_CYCLE),
		BUZZZ_DEFN(A9, 0x67, ISSUE_EMPTY_CYCLE)
	},

	{	/* group 13: INSTRTYPE */
		BUZZZ_DEFN(A9, 0x60, ICACHE_STALL),
		BUZZZ_DEFN(A9, 0x61, DCACHE_STALL),
		BUZZZ_DEFN(A9, 0x62, TLB_STALL),
		BUZZZ_DEFN(A9, 0x86, DMB_STALL)
	},

	{	/* group 14: INSTRTYPE */
		BUZZZ_DEFN(A9, 0x82, IMTLB_STALL),
		BUZZZ_DEFN(A9, 0x83, DMTLB_STALL),
		BUZZZ_DEFN(A9, 0x84, IUTLB_STALL),
		BUZZZ_DEFN(A9, 0x85, DUTLB_STALL)
	}
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
};

static BUZZZ_INLINE const char *BUZZZ_NOINSTR_FUNC
__buzzz_pmu_event(uint32_t group, uint32_t counter)
{
	if ((group >= BUZZZ_PMU_GROUPS) || (counter >= BUZZZ_PMU_COUNTERS))
		return _buzzz_INV;
	return buzzz_pmu_event_g[group][counter].evtname;
}

#endif	/*  CONFIG_ARM */


/*
 * +---------------------------------------------------------------------------+
 *  Section: BUZZZ Logging Infrastructure
 * +---------------------------------------------------------------------------+
 */

/* Performance Monitoring Tool private definitions */
typedef /* counters enabled for PMON */
struct buzzz_pmon_ctr
{
	uint32_t u32[BUZZZ_PMU_COUNTERS];
} buzzz_pmon_ctr_t;

typedef /* statistics per counter */
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
	buzzz_pmon_usr_t mon[BUZZZ_PMU_GROUPS];
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
			uint32_t pmu_group;         /* current group */
			uint32_t sample;            /* iteration for this group */
			uint32_t next_log_id;       /* next log id */
			uint32_t last_log_id;       /* last pmon log */
			uint32_t evtid[BUZZZ_PMU_COUNTERS];

			buzzz_pmon_ctr_t log[BUZZZ_PMON_LOGS + 1];
			buzzz_pmonst_t   run[BUZZZ_PMON_LOGS + 1];
			buzzz_pmonst_t   mon[BUZZZ_PMU_GROUPS][BUZZZ_PMON_LOGS + 1];

#if defined(BUZZZ_CONFIG_PMON_USR)
			buzzz_pmonst_usr_t usr;
#endif	/*  BUZZZ_CONFIG_PMON_USR */

			uint32_t config_skip;       /* number of sample to skip at start */
			uint32_t config_samples;    /* number of sample per run */
		} pmon;

		struct
		{
			uint32_t pmu_group;         /* current group */
			uint32_t count;             /* count of logs */
			uint32_t limit;             /* limit logs displayed */

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

	void            *cur;       /* pointer to next log entry */
	void            *end;       /* pointer to end of log entry */
	void            *log;

	buzzz_priv_t    priv;       /* tool specific private data */

	struct timer_list timer;

	buzzz_mode_t    config_mode;    /* limited, continuous wrapover */
	uint32_t        config_limit;   /* configured limit */

	buzzz_fmt_t     klogs[BUZZZ_KLOG_MAXIMUM];

	char            page[BUZZZ_PAGE_SIZE];
} buzzz_t;

static buzzz_t buzzz_g =        /* Global Buzzz object, see __init_buzzz() */
{
#if defined(CONFIG_SMP)
	.lock   = __SPIN_LOCK_UNLOCKED(.lock),
#endif  /*  CONFIG_SMP */
	.status = BUZZZ_STATUS_DISABLED,
#if defined(CONFIG_BUZZZ_KEVT)
	.tool   = BUZZZ_TOOL_KEVT,
#elif defined(CONFIG_BUZZZ_PMON)
	.tool   = BUZZZ_TOOL_PMON,
#elif defined(CONFIG_BUZZZ_FUNC)
	.tool   = BUZZZ_TOOL_FUNC,
#else
	.tool   = BUZZZ_TOOL_UNDEF,
#endif
	.panic  = BUZZZ_FALSE,
	.wrap   = BUZZZ_FALSE,
	.run    = 0U,
	.cur    = (void *)NULL,
	.end    = (void *)NULL,
	.log    = (void*)NULL,
	.config_mode = BUZZZ_MODE_WRAPOVER,

	.timer  = TIMER_INITIALIZER(NULL, 0, (int)&buzzz_g)
};

static BUZZZ_INLINE bool BUZZZ_NOINSTR_FUNC
__buzzz_assert_status_is_disabled(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		BUZZZ_PRERR("tool %s already enabled", __buzzz_tool(buzzz_g.tool));
		return BUZZZ_FALSE;
	}
	return BUZZZ_TRUE;
}
#define BUZZZ_ASSERT_STATUS_IS_DISABLED() \
	do { if (!__buzzz_assert_status_is_disabled()) return BUZZZ_FAILURE; } while (0)


static DEFINE_PER_CPU(buzzz_status_t, kevt_status) = BUZZZ_STATUS_DISABLED;

/* Preamble to start tracing in a tool */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pre_start(void)
{
	buzzz_g.panic = BUZZZ_FALSE;
	buzzz_g.wrap  = BUZZZ_FALSE;
	buzzz_g.run  += 1U;
	buzzz_g.cur   = buzzz_g.log;
	buzzz_g.end   = (void*)((char*)buzzz_g.log
		+ (BUZZZ_LOG_BUFSIZE - BUZZZ_LOGENTRY_MAXSZ));
}

/* Postamble to start tracing in a tool */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_post_start(void)
{
	buzzz_g.status = BUZZZ_STATUS_ENABLED;
}

/* Preamble to stop tracing in a tool */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pre_stop(void)
{
	buzzz_g.status = BUZZZ_STATUS_DISABLED;
}

/* Postamble to stop tracing in a tool */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_post_stop(void)
{
}

typedef int (*buzzz_dump_log_fn_t)(char *page, int bytes, void *log);

void BUZZZ_NOINSTR_FUNC /* Dump the kevt trace to console */
buzzz_log_dump(uint32_t limit, uint32_t count,
               uint32_t log_size, buzzz_dump_log_fn_t dump_log_fn)
{
	uint32_t total;
	void *log;

	if (buzzz_g.wrap == BUZZZ_TRUE)
		total = (BUZZZ_AVAIL_BUFSIZE / log_size);
	else
		total = count;

	BUZZZ_PRINT("limit<%u> bufsz<%u> max<%u> count<%u> wrap<%u> total<%u>"
		" log<%p> cur<%p> end<%p> log_size<%u>", limit, BUZZZ_AVAIL_BUFSIZE,
		(BUZZZ_AVAIL_BUFSIZE / log_size),
		count, buzzz_g.wrap, total,
		buzzz_g.log, buzzz_g.cur, buzzz_g.end, log_size);

	if (limit > buzzz_g.config_limit)
		limit = buzzz_g.config_limit;

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
			while (part2--) {   /* from cur to end : part2 */
				dump_log_fn(buzzz_g.page, 0, log);
				printk("%s", buzzz_g.page);
				log = (void*)((uint32_t)log + log_size);
			}
		}

		log = (void*)((uint32_t)buzzz_g.cur - (total * log_size));

		BUZZZ_PRINT("log<%p> total<%u>", log, total);
		while (total--) {   /* from log to cur : part1 */
			dump_log_fn(buzzz_g.page, 0, log);
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
			dump_log_fn(buzzz_g.page, 0, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

		log = (void*)buzzz_g.log;
		BUZZZ_PRINT("log<%p> part1<%u>", log, part1);
		while (part1--) {   /* from log to cur : part1 */
			dump_log_fn(buzzz_g.page, 0, log);
			printk("%s", buzzz_g.page);
			log = (void*)((uint32_t)log + log_size);
		}

	} else {                      /* everything in ring buffer, no wrap */

		log = (void*)buzzz_g.log; /* from log to cur */
		BUZZZ_PRINT("No Wrap log to cur: log<%p:%p> <%u>",
		            log, buzzz_g.cur, total);
		while (log < (void*)buzzz_g.cur) {
			dump_log_fn(buzzz_g.page, 0, log);
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
		uint32_t u32;
		void   *func;
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

	/* user arguments */
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;

} buzzz_func_log_t;

static int BUZZZ_NOINSTR_FUNC
_buzzz_symbol(char *p, unsigned long address)
{
	int bytes = 0;
	unsigned long offset = 0LU;
	char *eos, symbol_buf[KSYM_NAME_LEN+1];

	sprint_symbol(symbol_buf, address);
	eos = strpbrk(symbol_buf, "+");

	if (eos == (char*)NULL)
		BUZZZ_SNPRINTF("  %s" CLRnl, symbol_buf);
	else {
		*eos = '\0';
		sscanf(eos+1, "0x%lx", &offset);
		BUZZZ_SNPRINTF("  %s" CLRnorm, symbol_buf);
		if (offset)
			BUZZZ_SNPRINTF(" +0x%lx", offset);
		BUZZZ_SNPRINTF("\n");
	}
	return bytes;
}

static int BUZZZ_NOINSTR_FUNC
_buzzz_func_indent(char *p)
{
	int bytes = 0;
	if (buzzz_g.priv.func.config_exit) {
		uint32_t indent;
		for (indent = 0U; indent < buzzz_g.priv.func.indent; indent++)
			BUZZZ_SNPRINTF("%s", BUZZZ_FUNC_INDENT_STR);
	}
	return bytes;
}

static int BUZZZ_NOINSTR_FUNC
buzzz_func_dump_log(char *p, int bytes, void *l)
{
	buzzz_func_log_t *log = (buzzz_func_log_t *)l;

	if (log->arg0.klog.is_klog) {   /* print using registered log formats */

		char *fmt = buzzz_g.klogs[log->arg0.klog.id];

		bytes += _buzzz_func_indent(p + bytes);   /* print indentation spaces */

		BUZZZ_SNPRINTF("%s", CLRb);
		switch (log->arg0.klog.args) {
			case 0: BUZZZ_SNPRINTF(fmt); break;
			case 1: BUZZZ_SNPRINTF(fmt, log->arg1); break;
			case 2: BUZZZ_SNPRINTF(fmt, log->arg1, log->arg2); break;
			case 3: BUZZZ_SNPRINTF(fmt, log->arg1, log->arg2, log->arg3); break;
		}
		BUZZZ_SNPRINTF("%s", CLRnl);

	} else {                        /* print function call entry/exit */

		unsigned long address;      /* instruction address */

		if (log->arg0.site.is_ent)
			buzzz_g.priv.func.indent++;

		bytes += _buzzz_func_indent(p + bytes); /* print indentation spaces */

		if (log->arg0.site.is_ent)
			BUZZZ_SNPRINTF("%s", CLRr "=>");
		else
			BUZZZ_SNPRINTF("%s", CLRg "<=");

		if (!log->arg0.site.is_ent && (buzzz_g.priv.func.indent > 0))
			buzzz_g.priv.func.indent--;

		address = (unsigned long)(log->arg0.u32 & BUZZZ_FUNC_ADDRMASK);
		bytes += _buzzz_symbol(p + bytes, address);

	}

	return bytes;
}

#define _BUZZZ_FUNC_BGN(flags) \
	if (buzzz_g.tool != BUZZZ_TOOL_FUNC) return; \
	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) return; \
	BUZZZ_LOCK(flags); \
	if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED) { \
		if (buzzz_g.priv.func.limit == 0U) { \
			__buzzz_pre_stop(); \
			BUZZZ_UNLOCK(flags); \
			return; \
		} \
		buzzz_g.priv.func.limit--; \
	}

#define _BUZZZ_FUNC_END(flags) \
	buzzz_g.cur = (void*)(((buzzz_func_log_t*)buzzz_g.cur) + 1); \
	buzzz_g.priv.func.count++; \
	if (buzzz_g.cur >= buzzz_g.end) { \
		buzzz_g.wrap = BUZZZ_TRUE; \
		buzzz_g.cur = buzzz_g.log; \
	} \
	BUZZZ_UNLOCK(flags);


void BUZZZ_NOINSTR_FUNC /* -finstrument compiler stub on function entry */
__cyg_profile_func_enter(void *called, void *caller)
{
	unsigned long flags;
	_BUZZZ_FUNC_BGN(flags);
	((buzzz_func_log_t*)buzzz_g.cur)->arg0.u32 = (uint32_t)called | 1U;
	_BUZZZ_FUNC_END(flags);
}

void BUZZZ_NOINSTR_FUNC /* -finstrument compiler stub on function exit */
__cyg_profile_func_exit(void *called, void *caller)
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

	__buzzz_pre_start();

	buzzz_g.priv.func.count = 0U;
	buzzz_g.priv.func.indent = 0U;
	buzzz_g.priv.func.limit = buzzz_g.config_limit;

	buzzz_g.priv.func.log_count = 0U;
	buzzz_g.priv.func.log_index = 0U;

	__buzzz_post_start();
}

void BUZZZ_NOINSTR_FUNC /* Stop function call entry (exit optional) tracing */
buzzz_func_stop(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		buzzz_g.status = BUZZZ_STATUS_ENABLED;

		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_func_stop);

		__buzzz_pre_stop();

		if (buzzz_g.wrap == BUZZZ_TRUE)
			buzzz_g.priv.func.log_count
				= (BUZZZ_AVAIL_BUFSIZE / sizeof(buzzz_func_log_t));
		else
			buzzz_g.priv.func.log_count = buzzz_g.priv.func.count;
		buzzz_g.priv.func.log_index = 0U;

		__buzzz_post_stop();
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
	printk("+Mode:\t\t" CLRb "%s" CLRnl, __buzzz_mode(buzzz_g.config_mode));
	printk("\n");
}

static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_func_default(void)
{
#if defined(CONFIG_BUZZZ_KEVT)
	BUZZZ_PRERR("kernel event tracing enabled");
	BUZZZ_PRERR("disable CONFIG_BUZZZ_KEVT for FUNC tracing");
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_KEVT */

	buzzz_g.tool = BUZZZ_TOOL_FUNC;

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

static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_func_mode(buzzz_mode_t mode)
{
	if (mode == BUZZZ_MODE_LIMITED) {
		uint32_t limit_logs;
		/* Compute the last N entries to be dumped */
		limit_logs = (BUZZZ_LOG_BUFSIZE / sizeof(buzzz_func_log_t));
		buzzz_g.config_limit = (limit_logs < BUZZZ_FUNC_LIMIT_LOGS) ?
			limit_logs : BUZZZ_FUNC_LIMIT_LOGS;
	}

	return BUZZZ_SUCCESS;
}

static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_func_limit(uint32_t limit)
{
	buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
	buzzz_g.config_limit = limit;
	return BUZZZ_SUCCESS;
}

int BUZZZ_NOINSTR_FUNC /* API to config function exit tracing */
buzzz_func_config(uint32_t config_exit)
{
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	buzzz_g.priv.func.config_exit = config_exit;

	return BUZZZ_SUCCESS;
}

/*
 * Function: _init_buzzz_func
 * Initialization of Buzzz Function tool  during loading time
 */
static int BUZZZ_NOINSTR_FUNC
__init _init_buzzz_func(void)
{
	/* We may relax these two ... */
	if ((BUZZZ_AVAIL_BUFSIZE % sizeof(buzzz_func_log_t)) != 0) {
		BUZZZ_PRERR("BUZZZ_AVAIL_BUFSIZE<%d> ~ sizeof(buzzz_func_log_t)<%d>\n",
			(int)BUZZZ_AVAIL_BUFSIZE, (int)sizeof(buzzz_func_log_t));
		return BUZZZ_ERROR;
	}

	if (sizeof(buzzz_func_log_t) != 16) {
		BUZZZ_PRERR("sizeof(buzzz_func_log_t)<%d> != 16",
			(int)sizeof(buzzz_func_log_t));
		return BUZZZ_ERROR;
	}

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
#define BUZZZ_PMON_EXCP_MODE    (1U << 0)  /* Exception mode counting */
#define BUZZZ_PMON_KERN_MODE    (1U << 1)  /* Kernel mode counting */
#define BUZZZ_PMON_SPRV_MODE    (1U << 2)  /* Supervisor mode counting */
#define BUZZZ_PMON_USER_MODE    (1U << 3)  /* Supervisor mode counting */

#define BUZZZ_PMON_EKSU_MODE \
	(BUZZZ_PMON_EXCP_MODE | BUZZZ_PMON_KERN_MODE \
	| BUZZZ_PMON_SPRV_MODE | BUZZZ_PMON_USER_MODE)

#define BUZZZ_PMON_CTRL             BUZZZ_PMON_EKSU_MODE

#define BUZZZ_PMON_VAL(val, mode)   (((uint32_t)(val) << 5) | (mode))

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_prefetch(const void *addr)
{
	/* WRITE hint */
	__asm__ volatile("pref %0, (%1)" :: "i"(1), "r"(addr));
}

uint32 buzzz_cycles(void)
{
	read_c0_count();
}
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */

#if defined(CONFIG_ARM)

#define BUZZZ_PMON_VAL(val, mode)   ((uint32_t)(val))

#if defined(BUZZZ_CONFIG_CPU_ARMV7_A7)
/* BUZZZ uses ARMv7 A9 counters #0,#1,#2,#3 as BUZZZ counters #0,#1,#2,#3 */
#define BUZZZ_ARM_CTR_SEL(ctr_idx) (ctr_idx)
#define BUZZZ_ARM_CTR_BIT(ctr_idx) (1 << BUZZZ_ARM_CTR_SEL(ctr_idx))
#define BUZZZ_ARM_IDCODE           (0x07)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A7 */

#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
/* BUZZZ uses ARMv7 A9 counters #2,#3,#4,#5 as BUZZZ counters #0,#1,#2,#3 */
#define BUZZZ_ARM_CTR_SEL(ctr_idx) ((ctr_idx) + 2)
#define BUZZZ_ARM_CTR_BIT(ctr_idx) (1 << BUZZZ_ARM_CTR_SEL(ctr_idx))
#define BUZZZ_ARM_IDCODE           (0x09)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */

union cp15_c9_REG {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t ctr0   :  1; /* A7:Ctr#0  A9:rsvd  */
		uint32_t ctr1   :  1; /* A7:Ctr#1  A9:rsvd  */
		uint32_t ctr2   :  1; /* A7:Ctr#2  A9:Ctr#0 */
		uint32_t ctr3   :  1; /* A7:Ctr#3  A9:Ctr#1 */
		uint32_t ctr4   :  1; /* A7:none   A9:Ctr#2 */
		uint32_t ctr5   :  1; /* A7:none   A9:Ctr#3 */
		uint32_t ctr6   :  1;
		uint32_t ctr7   :  1;
		uint32_t none   : 23;
		uint32_t cycle  :  1; /* cycle count register */
	};
};

/*
 * +----------------------------------------------------------------------------
 * ARMv7 Performance Monitor Control Register
 *  MRC p15, 0, <Rd>, c9, c12, 0 ; Read PMCR
 *  MCR p15, 0, <Rd>, c9, c12, 0 ; Write PMCR
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_PMCR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t enable      : 1; /* E: enable all counters incld cctr  */
		uint32_t evt_reset   : 1; /* P: event counter reset             */
		uint32_t cctr_reset  : 1; /* C: cycle counter reset             */
		uint32_t clk_divider : 1; /* D: cycle count divider: 0= 1:1     */
		uint32_t export_en   : 1; /* X: export enable                   */
		uint32_t prohibit    : 1; /* DP: disable in prohibited regions  */
		uint32_t reserved    : 5; /* ReadAZ, Wr: ShouldBeZero/Preserved */
		uint32_t counters    : 5; /* N: number of event counters        */
		uint32_t id_code     : 8; /* IDCODE: identification code        */
		                          /* 0x07=Cortex-A7, 0x09=Cortex-A9     */
		uint32_t impl_code   : 8; /* IMP: implementer code 0x41=ARM     */
	};
};

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__armv7_PMCR_RD(void)
{
	union cp15_c9_c12_PMCR pmcr;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(pmcr.u32));
	if (pmcr.id_code != BUZZZ_ARM_IDCODE) {
		BUZZZ_PRERR("mismatch PMCR id_code 0x%02X != 0x%02X\n",
		            pmcr.id_code, BUZZZ_ARM_IDCODE);
	}
	BUZZZ_PRINT("RD PMCR IMP%u ID%u N[%u] DP[%u] X[%u] D[%u] C[%u] P[%u] E[%u]",
		pmcr.impl_code, pmcr.id_code, pmcr.counters, pmcr.prohibit,
		pmcr.export_en, pmcr.clk_divider, pmcr.cctr_reset, pmcr.evt_reset,
		pmcr.enable);
	return pmcr.u32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_PMCR_WR(const uint32_t v32)
{
	union cp15_c9_c12_PMCR pmcr;
	pmcr.u32 = v32;
	pmcr.reserved = 0;  /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(pmcr.u32));
	BUZZZ_PRINT("RD PMCR IMP%u ID%u N[%u] DP[%u] X[%u] D[%u] C[%u] P[%u] E[%u]",
		pmcr.impl_code, pmcr.id_code, pmcr.counters, pmcr.prohibit,
		pmcr.export_en, pmcr.clk_divider, pmcr.cctr_reset, pmcr.evt_reset,
		pmcr.enable);
}


/*
 * +----------------------------------------------------------------------------
 * ARMv7 Performance Monitor event counter SELection Register
 *  MRC p15, 0, <Rd>, c9, c12, 5 ; Read PMSELR
 *  MCR p15, 0, <Rd>, c9, c12, 5 ; Write PMSELR
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_PMSELR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t ctr_select: 8; /* event counter selecter */
		uint32_t reserved  :24; /* reserved */
	};
};

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__armv7_PMSELR_RD(void)
{
	union cp15_c9_c12_PMSELR pmselr;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 5" : "=r"(pmselr.u32));
	return pmselr.u32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_PMSELR_WR(const uint32_t v32)
{
	union cp15_c9_c12_PMSELR pmselr;
	pmselr.u32 = v32;
	pmselr.reserved = 0;  /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmselr.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARMv7 Performance Monitor EVent TYPE selection Register
 *  MRC p15, 0, <Rd>, c9, c13, 1 ; Read PMXEVTYPER
 *  MCR p15, 0, <Rd>, c9, c13, 1 ; Write PMXEVTYPER
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c13_PMXEVTYPER {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t event_id :  8; /* event type to count */
		uint32_t reserved : 24; /* reserved */
	};
};

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__armv7_PMXEVTYPER_RD(void)
{
	union cp15_c9_c13_PMXEVTYPER pmxevtyper;
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 1" : "=r"(pmxevtyper.u32));
	return pmxevtyper.u32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_PMXEVTYPER_WR(const uint32_t v32)
{
	union cp15_c9_c13_PMXEVTYPER pmxevtyper;
	pmxevtyper.u32 = v32;
	pmxevtyper.reserved = 0;  /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(pmxevtyper.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARMv7 Performance Monitor User Enable Register
 *  MRC p15, 0, <Rd>, c9, c14, 0 ; Read PMUSERENR
 *  MCR p15, 0, <Rd>, c9, c14, 0 ; Write PMUSERENR
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c14_PMUSERENR {
	uint32_t u32;
	struct {    /* Little Endian */
		uint32_t enable   :  1; /* user mode enable */
		uint32_t reserved : 31; /* reserved */
	};
};

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__armv7_PMUSERENR_RD(void)
{
	union cp15_c9_c14_PMUSERENR pmuserenr;
	__asm__ volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuserenr.u32));
	return pmuserenr.u32;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_PMUSERENR_WR(const uint32_t v32)
{
	union cp15_c9_c14_PMUSERENR pmuserenr;
	pmuserenr.u32 = v32;
	pmuserenr.reserved = 0;  /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(pmuserenr.u32));
}


static BUZZZ_INLINE bool BUZZZ_NOINSTR_FUNC
__armv7_PMCNTENSET_test(void)
{
	int ctr_idx;
	/* Test whether PMU counters required by buzzz are in use */
	union cp15_c9_REG pmcntenset;

	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset.u32));
	for (ctr_idx = 0; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
		if (pmcntenset.u32 & BUZZZ_ARM_CTR_BIT(ctr_idx))
			return TRUE;
	return FALSE;
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_PMUSERENR_enable(void)
{
	uint32_t u32 = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(u32));
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __armv7_pmu_enable
 *  Enable the ARMv7 Performance Monitoring Unit
 *  - Set enable bit in PMCR
 *  - Disable overflow interrupts
 *  - Enable 4 counters reserved for BUZZZ
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_pmu_enable(void)
{
	int ctr_idx;
	union cp15_c9_c12_PMCR pmcr;
	union cp15_c9_REG pmcntenset;
	union cp15_c9_REG pmcntenclr;
	union cp15_c9_REG pmintenclr;

	/* Set Enable bit in PMCR */
	pmcr.u32 = __armv7_PMCR_RD();
	pmcr.enable = 1;
	__armv7_PMCR_WR(pmcr.u32);

	asm volatile("mrc p15, 0, %0, c9, c14, 2" : "=r"(pmintenclr.u32));
	BUZZZ_PRINT("pmintenclr = 0x%08x", (int)pmintenclr.u32);

	asm volatile("mrc p15, 0, %0, c9, c12, 2" : "=r"(pmcntenclr.u32));
	BUZZZ_PRINT("pmcntenclr = 0x%08x", (int)pmcntenclr.u32);

	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset.u32));
	BUZZZ_PRINT("pmcntenset = 0x%08x", (int)pmcntenset.u32);

	/* Disable overflow interrupts on 4 buzzz counters: A9 ctr2 to ctr5 */
	pmintenclr.u32 = 0U;
	for (ctr_idx = 0; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++) {
		pmintenclr.u32 += BUZZZ_ARM_CTR_BIT(ctr_idx);
	}
	pmintenclr.cycle = 1; /* disable overflow on cycle count register, too */
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(pmintenclr.u32));
	BUZZZ_PRINT("disable overflow interrupts PMINTENCLR[%08x]", pmintenclr.u32);

	/*
	 * Enable the 4 buzzz counters:
	 *     Cortex-A7 ctr0 to ctr3
	 *     Cortex-A9 ctr2 to ctr5
	 * Also enable cycle counter 0b31
	 */
	pmcntenset.u32 = 0U;
	for (ctr_idx = 0; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++) {
		pmcntenset.u32 += BUZZZ_ARM_CTR_BIT(ctr_idx);
	}
	pmcntenset.cycle = 1; /* enable cycle count also */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(pmcntenset.u32));
	BUZZZ_PRINT("enable CTR PMCNTENSET[%08x]", pmcntenset.u32);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __armv7_pmu_disable
 *  Disable the counters used by BUZZZ in the ARMv7 Performance Monitoring Unit
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_pmu_disable(void)
{
	int ctr_idx;
	union cp15_c9_REG pmcntenclr;

	/*
	 * Do not disable the PMCR::enable bit as this would also disable the
	 * PMCCNTR cycle count register.
	 */

	/*
	 * Disable the 4 buzzz counters:
	 *     Cortex-A7 ctr0 to ctr3
	 *     Cortex-A9 ctr2 to ctr5
	 * Let the cycle count register, continue to be enabled.
	 */
	pmcntenclr.u32 = 0U;
	for (ctr_idx = 0; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++) {
		pmcntenclr.u32 += BUZZZ_ARM_CTR_BIT(ctr_idx);
	}
	/* Since pmcntenclr.cycle = 0, a WR of 0b31=0 will be ignored. */
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(pmcntenclr.u32));
	BUZZZ_PRINT("Disable CTR PMCNTENCLR[%08x]", pmcntenclr.u32);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __armv7_pmu_read_buzzz_ctr
 *  Program the counter selector and read the pmxevtctr0
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__armv7_pmu_read_buzzz_ctr(const uint32_t ctr_idx)
{
	uint32_t v32;
	union cp15_c9_c12_PMSELR pmselr;

	/* Select the counter */
	pmselr.u32 = BUZZZ_ARM_CTR_SEL(ctr_idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmselr.u32));

	/* Now read the pmxevtctr0 */
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(v32));
	return v32;
}

#define BUZZZ_READ_COUNTER(ctr_idx)  __armv7_pmu_read_buzzz_ctr(ctr_idx)

/*
 * +---------------------------------------------------------------------------+
 *  Function: __armv7_pmu_config_buzzz_ctr
 *  Configure the eventidx to be counted by one of the PMU BUZZZ counters
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armv7_pmu_config_buzzz_ctr(const uint32_t ctr_idx, const uint32_t event_id)
{
	union cp15_c9_c13_PMXEVTYPER pmxevtyper;
	union cp15_c9_c12_PMSELR pmselr;

	/* Select the Counter using PMSELR */
	pmselr.u32 = 0U;
	pmselr.ctr_select = BUZZZ_ARM_CTR_SEL(ctr_idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmselr.u32));

	/* Configure the event type */
	pmxevtyper.u32 = 0U;
	pmxevtyper.event_id = event_id;
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(pmxevtyper.u32));

	BUZZZ_PRINT("Config Ctr<%u> PMSELR[%08x] PMXEVTYPER[%08x]",
		ctr_idx, pmselr.u32, pmxevtyper.u32);
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_prefetch(const void *ptr)
{
#if (__LINUX_ARM_ARCH__ >= 7) && defined(CONFIG_SMP)
	__asm__ volatile(
		".arch_extension    mp\n" /* multiprocessor extention has PLDW */
		"pldw\t%a0" :: "p"(ptr)); /* prefetch with intent to write */
#else
	 __asm__ volatile("pld\t%a0" :: "p"(ptr));
#endif
}

uint32 buzzz_cycles(void)
{
	uint32 cycles;
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
	return cycles;
}

#endif	/*  CONFIG_ARM */


static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_enable(void)
{
#if defined(CONFIG_ARM)
	__armv7_pmu_enable();
#endif	/*  CONFIG_ARM */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_disable(void)
{
#if defined(CONFIG_ARM)
	__armv7_pmu_disable();
#endif	/*  CONFIG_ARM */
}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_log(uint32_t log_id)
{
	buzzz_pmon_ctr_t *log = &buzzz_g.priv.pmon.log[log_id];

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
	log->u32[0] = read_c0_perf(1);
	log->u32[1] = read_c0_perf(3);
	log->u32[2] = read_c0_perf(5);
	log->u32[3] = read_c0_perf(7);
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */
#if defined(CONFIG_ARM)
	log->u32[0] = BUZZZ_READ_COUNTER(0);
	log->u32[1] = BUZZZ_READ_COUNTER(1);
	log->u32[2] = BUZZZ_READ_COUNTER(2);
	log->u32[3] = BUZZZ_READ_COUNTER(3);
#endif	/*  CONFIG_ARM */

}

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_iter_set_invalid(void) /* Tag an iteration as incomplete */
{
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_pmon_usr_g = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
	buzzz_g.priv.pmon.log[0].u32[0] = BUZZZ_INVALID;
	buzzz_g.priv.pmon.next_log_id = 0U;
}

static BUZZZ_INLINE uint32_t BUZZZ_NOINSTR_FUNC
__buzzz_pmon_iter_is_invalid(void) /* Query if last iteration was incomplete */
{
	return (buzzz_g.priv.pmon.log[0].u32[0] == BUZZZ_INVALID);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_run
 *  Accumulate all counters for one sample iteration.
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_run(uint32_t last_log)
{
	uint32_t log, ctr;
	buzzz_pmon_ctr_t elapsed, prev, curr;

	for (log = 1U; log <= last_log; log++) {
		for (ctr = 0U; ctr < BUZZZ_PMU_COUNTERS; ctr++) {
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

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_tot
 *  Summarize all logs for sample number of iterations for a group.
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_tot(uint32_t last_log)
{
	uint32_t log, ctr;

	buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.pmu_group][0].sum.u32[0] = ~0U;

	for (log = 1U; log <= last_log; log++) {
		for (ctr = 0U; ctr < BUZZZ_PMU_COUNTERS; ctr++) {
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.pmu_group][log].min.u32[ctr]
				= buzzz_g.priv.pmon.run[log].min.u32[ctr];
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.pmu_group][log].max.u32[ctr]
				= buzzz_g.priv.pmon.run[log].max.u32[ctr];
			buzzz_g.priv.pmon.mon[buzzz_g.priv.pmon.pmu_group][log].sum.u32[ctr]
				= buzzz_g.priv.pmon.run[log].sum.u32[ctr];
		}
	}
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.pmu_group].min
		= buzzz_g.priv.pmon.usr.run.min;
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.pmu_group].max
		= buzzz_g.priv.pmon.usr.run.max;
	buzzz_g.priv.pmon.usr.mon[buzzz_g.priv.pmon.pmu_group].sum
		= buzzz_g.priv.pmon.usr.run.sum;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_bind
 *  Initialize storage for statistics and configure the PMU for a new group
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_pmon_bind(uint32_t pmu_group)
{
	uint32_t m; /* PMU mode */
	uint32_t log, counter;
	buzzz_pmu_event_desc_t *desc;

	/* Initialize storage for statistics */
	for (log = 0U; log <= BUZZZ_PMON_LOGS; log++) {
		for (counter = 0U; counter < BUZZZ_PMU_COUNTERS; counter++) {
			buzzz_g.priv.pmon.run[log].min.u32[counter] = ~0U;
			buzzz_g.priv.pmon.run[log].max.u32[counter] = 0U;
			buzzz_g.priv.pmon.run[log].sum.u32[counter] = 0U;
		}
	}
#if defined(BUZZZ_CONFIG_PMON_USR)
	buzzz_g.priv.pmon.usr.run.min = ~0U;
	buzzz_g.priv.pmon.usr.run.max = 0U;
	buzzz_g.priv.pmon.usr.run.sum = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */

	/* PMU Event values */
	desc = (buzzz_pmu_event_desc_t*)&buzzz_pmu_event_g[pmu_group][0];

#if defined(CONFIG_MIPS) && defined(BUZZZ_CONFIG_CPU_MIPS_74K)
	m = (pmu_group == BUZZZ_PMU_GROUP_UNDEF) ? 0 : BUZZZ_PMON_CTRL;

	write_c0_perf(0, BUZZZ_PMON_VAL((desc + 0)->evtid, m));
	write_c0_perf(2, BUZZZ_PMON_VAL((desc + 1)->evtid, m));
	write_c0_perf(4, BUZZZ_PMON_VAL((desc + 2)->evtid, m));
	write_c0_perf(6, BUZZZ_PMON_VAL((desc + 3)->evtid, m));
#endif	/*  CONFIG_MIPS && BUZZZ_CONFIG_CPU_MIPS_74K */

#if defined(CONFIG_ARM)
	m = 0; /* dummy */
	for (counter = 0U; counter < BUZZZ_PMU_COUNTERS; counter++) {
		__armv7_pmu_config_buzzz_ctr(counter, (desc + counter)->evtid);
	}
#endif	/*  CONFIG_ARM */

	/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
	BUZZZ_PRINT("Bind group<%u:%s> <%x:%s> <%x:%s> <%x:%s> <%x:%s>",
		pmu_group, __buzzz_pmu_group(pmu_group),
		BUZZZ_PMON_VAL((desc + 0)->evtid, m), __buzzz_pmu_event(pmu_group, 0),
		BUZZZ_PMON_VAL((desc + 1)->evtid, m), __buzzz_pmu_event(pmu_group, 1),
		BUZZZ_PMON_VAL((desc + 2)->evtid, m), __buzzz_pmu_event(pmu_group, 2),
		BUZZZ_PMON_VAL((desc + 3)->evtid, m), __buzzz_pmu_event(pmu_group, 3));

	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.pmu_group = pmu_group;

}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_bgn
 *  User log indicating that a new iteration has started.
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_bgn(void)
{
	if (buzzz_g.status == BUZZZ_STATUS_ENABLED) {
#if defined(BUZZZ_CONFIG_PMON_USR)
		buzzz_pmon_usr_g = 0U;
#endif	/*  BUZZZ_CONFIG_PMON_USR */
		__buzzz_pmon_log(0U);                       /* record baseline values */
		buzzz_g.priv.pmon.next_log_id = 1U;         /* for log sequence check */
	} else
		__buzzz_pmon_iter_set_invalid();
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_clr
 *  User log indication that an interation is invalid
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_clr(void)
{
	__buzzz_pmon_iter_set_invalid();
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_log
 *  Record performance counters at this event point
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_log(uint32_t log_id)
{
	if (buzzz_g.priv.pmon.next_log_id != log_id) {  /* check log sequence */
		__buzzz_pmon_iter_set_invalid();            /* tag sample as invalid */
		return;
	}

	__buzzz_pmon_log(log_id);                       /* record counter values */

	buzzz_g.priv.pmon.next_log_id = log_id + 1U;    /* for log sequence check */
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_end
 *  User log entry identifying that one iteration has completed.
 * +---------------------------------------------------------------------------+
 */
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
	if (__buzzz_pmon_iter_is_invalid()) {
		BUZZZ_PRINT("invalid iteration");
		return;       /* invalid sample */
	}

	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) {
		__buzzz_pmon_iter_set_invalid();
		return;
	}

	buzzz_g.priv.pmon.last_log_id = last_log_id;
	buzzz_g.priv.pmon.sample++;

	BUZZZ_PRINT("group<%u> sample<%u> of <%u> last_log_id<%u>",
		buzzz_g.priv.pmon.pmu_group, buzzz_g.priv.pmon.sample,
		buzzz_g.priv.pmon.config_samples, last_log_id);

	/* UNDEF or skip group ID */
	if (buzzz_g.priv.pmon.pmu_group == BUZZZ_PMU_GROUP_UNDEF) {

		BUZZZ_PRINT("SKIP samples completed");

		if (buzzz_g.priv.pmon.sample >= buzzz_g.priv.pmon.config_samples) {

			/* If wrapover, start with first group, else use configured group */
			if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
				buzzz_g.priv.pmon.pmu_group = buzzz_g.config_limit;
			else
				buzzz_g.priv.pmon.pmu_group++;

			/* bind to configured group */
			__buzzz_pmon_bind(buzzz_g.priv.pmon.pmu_group);
		}
		return; /* exit without summing all samples for UNDEF group */
	}

	/* Accumulate into run, current iteration, and setup for next iteration */
	__buzzz_pmon_run(last_log_id);

	/* If sample number of iterations have completed switch to next pmu_group */
	if (buzzz_g.priv.pmon.sample >= buzzz_g.priv.pmon.config_samples) {

		__buzzz_pmon_tot(last_log_id); /* record current groups sum total */

		/* Move to next group, or end if limited to single group */
		if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
			buzzz_g.priv.pmon.pmu_group = BUZZZ_PMU_GROUPS;
		else
			buzzz_g.priv.pmon.pmu_group++;

		/* If last group, stop or else switch to next group */
		if (buzzz_g.priv.pmon.pmu_group >= BUZZZ_PMU_GROUPS)
			buzzz_pmon_stop();
		else
			__buzzz_pmon_bind(buzzz_g.priv.pmon.pmu_group);
	}
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_dump
 *  Dump PMON statistics
 *  CAUTION: printk in this function assumes BUZZZ_PMU_COUNTERS is 4
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_BUZZZ_PMON) && (BUZZZ_PMU_COUNTERS != 4)
#error "buzzz_pmon_dump: printk assumes BUZZZ_PMU_COUNTERS is 4"
#endif

void BUZZZ_NOINSTR_FUNC
buzzz_pmon_dump(void)
{
	uint32_t group, log_id, ctr_idx;
	buzzz_pmonst_t *log;
	buzzz_pmon_ctr_t tot;

	printk("\n\n+++++++++++++++\n+ PMON Report +\n+++++++++++++++\n\n");

	if (buzzz_g.status != BUZZZ_STATUS_DISABLED)
		printk(CLRwarn "PMon not disabled" CLRnl);

	if (buzzz_g.config_mode == BUZZZ_MODE_LIMITED)
		group = buzzz_g.config_limit;
	else
		group = BUZZZ_PMU_GROUP_GENERAL;

	for (; group < BUZZZ_PMU_GROUP_MAXIMUM; group++) {

		/* Zero out the total cummulation */
		for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
			tot.u32[ctr_idx] = 0U;

		/* Print the table header */
		printk("\n\nGroup:\t" CLRb "%s\n" CLRnl, __buzzz_pmu_group(group));

		/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
		if (group == BUZZZ_PMU_GROUP_GENERAL) {
			printk(CLRb "%15s %15s %15s %15s %15s    REGISTERED-EVENT" CLRnl,
				__buzzz_pmu_event(group, 0), __buzzz_pmu_event(group, 1),
				__buzzz_pmu_event(group, 2), __buzzz_pmu_event(group, 3),
				"NANO_SECONDS");
		} else {
			printk(CLRb "%15s %15s %15s %15s    REGISTERED-EVENT" CLRnl,
				__buzzz_pmu_event(group, 0), __buzzz_pmu_event(group, 1),
				__buzzz_pmu_event(group, 2), __buzzz_pmu_event(group, 3));
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

			for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
				tot.u32[ctr_idx] += log->sum.u32[ctr_idx];

			/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
			if (group == BUZZZ_PMU_GROUP_GENERAL) {
				printk("%15u %15u %15u %15u %15u    %s\n",
					log->sum.u32[0], log->sum.u32[1],
					log->sum.u32[2], log->sum.u32[3],
					((log->sum.u32[3] * 1000) / BUZZZ_CYCLES_PER_USEC),
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
		/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
		if (group == BUZZZ_PMU_GROUP_GENERAL) {
			printk("\n%15u %12u %12u %15u %12u    " CLRb "%s" CLRnl,
				tot.u32[1], ((tot.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
				tot.u32[3], tot.u32[0], tot.u32[2], "Total");
		} else {
			printk("\n%15u %15u %15u %15u    " CLRb "%s" CLRnl,
				tot.u32[0], tot.u32[1], tot.u32[2], tot.u32[3], "Total");
		}

		for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
				tot.u32[ctr_idx] = 0U;

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

			/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
			if (group == BUZZZ_PMU_GROUP_GENERAL) {
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

			for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
				tot.u32[ctr_idx] += log->min.u32[ctr_idx];
		}

		/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
		if (group == BUZZZ_PMU_GROUP_GENERAL) {
			printk("\n%15u %12u %12u %15u %12u    " CLRb "%s" CLRnl,
				tot.u32[1], ((tot.u32[1] * 1000) / BUZZZ_CYCLES_PER_USEC),
				tot.u32[3], tot.u32[0], tot.u32[2], "Total");
		} else {
			printk("\n%15u %15u %15u %15u    " CLRb "%s" CLRnl,
				tot.u32[0], tot.u32[1], tot.u32[2], tot.u32[3], "Total");
		}

		for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
				tot.u32[ctr_idx] = 0U;

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

			/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
			if (group == BUZZZ_PMU_GROUP_GENERAL) {
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

			for (ctr_idx = 0U; ctr_idx < BUZZZ_PMU_COUNTERS; ctr_idx++)
				tot.u32[ctr_idx] += log->max.u32[ctr_idx];
		}

		/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
		if (group == BUZZZ_PMU_GROUP_GENERAL) {
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

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_start
 *  Start PMON tool
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_start(void)
{
	__buzzz_pre_start();

	memset(buzzz_g.priv.pmon.log, 0, sizeof(buzzz_g.priv.pmon.log));
	memset(buzzz_g.priv.pmon.run, 0, sizeof(buzzz_g.priv.pmon.run));
	memset(buzzz_g.priv.pmon.mon, 0, sizeof(buzzz_g.priv.pmon.mon));

	__buzzz_pmon_bind(BUZZZ_PMU_GROUP_UNDEF);
	__buzzz_pmon_iter_set_invalid();

	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.next_log_id = 0U;
	buzzz_g.priv.pmon.last_log_id = 0U;

	__buzzz_pmon_enable();

	__buzzz_post_start();

	BUZZZ_PRINT("buzzz_pmon_start");
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_stop
 *  Stop PMON tool
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_stop(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {
		__buzzz_pre_stop();

		__buzzz_pmon_bind(BUZZZ_PMU_GROUP_UNDEF);

		__buzzz_pmon_disable();

		buzzz_pmon_dump();

		__buzzz_post_stop();
	}
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_show
 *  Dump PMON tool state
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_pmon_show(void)
{
	uint32_t pmu_group = buzzz_g.priv.pmon.pmu_group;
	printk("Group:\t\t" CLRb "%d %s" CLRnl, buzzz_g.priv.pmon.pmu_group,
		__buzzz_pmu_group(buzzz_g.priv.pmon.pmu_group));
	printk("Evtid:\t" CLRb "%u:%s %u:%s %u:%s %u:%s" CLRnl,
		buzzz_g.priv.pmon.evtid[0], __buzzz_pmu_event(pmu_group, 0),
		buzzz_g.priv.pmon.evtid[1], __buzzz_pmu_event(pmu_group, 1),
		buzzz_g.priv.pmon.evtid[2], __buzzz_pmu_event(pmu_group, 2),
		buzzz_g.priv.pmon.evtid[3], __buzzz_pmu_event(pmu_group, 3));
	printk("Sample:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.sample);
	printk("NextId:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.next_log_id);
	printk("LastId:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.last_log_id);
	printk("+Skip:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.config_skip);
	printk("+Sample:\t" CLRb "%u" CLRnl, buzzz_g.priv.pmon.config_samples);
	printk("+Group:\t\t" CLRb "%s" CLRnl,
		__buzzz_pmu_group(buzzz_g.config_limit));
	printk("+Mode:\t\t" CLRb "%s" CLRnl, __buzzz_mode(buzzz_g.config_mode));
	printk("\n");

	for (pmu_group = BUZZZ_PMU_GROUP_UNDEF + 1;
	     pmu_group < BUZZZ_PMU_GROUP_MAXIMUM; pmu_group++) {
		printk("\t%d : " CLRb "%s" CLRnl,
			pmu_group, __buzzz_pmu_group(pmu_group));
	}
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_default
 *  Reset to default PMON configuration on a PMON tool selection.
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_pmon_default(void)
{
	uint32_t counter;

#if defined(CONFIG_BUZZZ_FUNC)
	BUZZZ_PRERR("function call tracing enabled");
	BUZZZ_PRERR("disable CONFIG_BUZZZ_FUNC for PMON tool");
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_FUNC */

#if defined(CONFIG_BUZZZ_KEVT)
	BUZZZ_PRERR("ERROR: BUZZZ Kernel Event Tracing Enabled");
	BUZZZ_PRERR("disable CONFIG_BUZZZ_KEVT for PMON tool");
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_KEVT */

#if defined(CONFIG_ARM)
	/* Ensure PMU is not being used by another subsystem */
	if (__armv7_PMCNTENSET_test() != 0U) {
		BUZZZ_PRERR("PMU counters are in use");
		return BUZZZ_ERROR;
	}
#endif	/*  CONFIG_ARM */

	buzzz_g.tool = BUZZZ_TOOL_PMON;

	buzzz_g.priv.pmon.pmu_group = BUZZZ_PMU_GROUP_UNDEF;
	for (counter = 0; counter < BUZZZ_PMU_COUNTERS; counter++)
		buzzz_g.priv.pmon.evtid[counter] =
			buzzz_pmu_event_g[BUZZZ_PMU_GROUP_UNDEF][counter].evtid;
	buzzz_g.priv.pmon.sample = 0U;
	buzzz_g.priv.pmon.next_log_id = 0U;
	buzzz_g.priv.pmon.last_log_id = 0U;
	buzzz_g.priv.pmon.config_skip = BUZZZ_PMON_SAMPLESZ;
	buzzz_g.priv.pmon.config_samples = BUZZZ_PMON_SAMPLESZ;

	buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
	buzzz_g.config_limit = BUZZZ_PMU_GROUP_GENERAL;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_mode
 *  Configure the PMON operational mode:
 *    wrapover: all supported PMU groups are covered
 *    limited : a specific group is selected, default to GENERAL
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_pmon_mode(buzzz_mode_t mode)
{
	if (mode == BUZZZ_MODE_WRAPOVER)
		buzzz_g.config_limit = BUZZZ_INVALID;
	else if (mode == BUZZZ_MODE_LIMITED)
		buzzz_g.config_limit = BUZZZ_PMU_GROUP_GENERAL;
	else {
		BUZZZ_PRERR("Unsupported mode %s for %s",
			__buzzz_mode(mode), __buzzz_tool(buzzz_g.tool));
		return BUZZZ_ERROR;
	}

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_pmon_limit
 *  Limit the PMON ro a single group.
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_pmon_limit(uint32_t pmu_group)
{
	BUZZZ_PRINT("overriding mode<%s>", __buzzz_mode(buzzz_g.config_mode));
	buzzz_g.config_mode = BUZZZ_MODE_LIMITED;

	BUZZZ_ASSERT_PMU_GROUP(pmu_group);

	buzzz_g.config_limit = pmu_group; /* limit to selected group */

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pmon_config
 *  Configure the number of iterations to be skipped (at start) and how many
 *  iterations to be averaged over for each group.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_pmon_config(uint32_t config_samples, uint32_t config_skip)
{
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	buzzz_g.priv.pmon.config_samples = config_samples;
	buzzz_g.priv.pmon.config_skip = config_skip;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _init_buzzz_pmon
 *  Initialization of Buzzz PMON tool during loading time
 * +---------------------------------------------------------------------------+
 */
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
	uint32_t ctr[BUZZZ_PMU_COUNTERS];
	union {
		uint32_t    u32;
		struct {
			uint32_t core :  4; /* allow for 16 cpu cores */
			uint32_t args :  4;
			uint32_t rsvd :  8; /* future: preempt count */
			uint32_t id   : 16;
		} klog;
	} arg0;
	uint32_t arg1, arg2, arg3;
	/* Try to maintain this structure at 32Bytes, typical cacheline size */
} buzzz_kevt_log_t;


/*
 * +---------------------------------------------------------------------------+
 *  Section: KEVT tool logging APIs (0, 1, 2, 3 args)
 * +---------------------------------------------------------------------------+
 */

/*
 * To limit the number of logs from start, insert in _BUZZZ_KEVT_PREAMBLE
 *
 * if (buzzz_g.priv.kevt.count >= buzzz_g.priv.kevt.limit) {
 *     buzzz_g.status = BUZZZ_STATUS_DISABLED;
 * }
 *
 */
#define _BUZZZ_KEVT_PREAMBLE(flags, log_p, kevt_id, nargs) \
({ \
	__buzzz_prefetch(buzzz_g.cur); \
	if (buzzz_g.status != BUZZZ_STATUS_ENABLED) return; \
	BUZZZ_LOCK(flags); \
	(log_p) = (buzzz_kevt_log_t*)buzzz_g.cur; \
	(log_p)->arg0.klog.id = (kevt_id); \
	(log_p)->arg0.klog.core = raw_smp_processor_id(); \
	(log_p)->arg0.klog.args = nargs; \
	(log_p)->ctr[0] = BUZZZ_READ_COUNTER(0); \
	(log_p)->ctr[1] = BUZZZ_READ_COUNTER(1); \
	(log_p)->ctr[2] = BUZZZ_READ_COUNTER(2); \
	(log_p)->ctr[3] = BUZZZ_READ_COUNTER(3); \
})

#define _BUZZZ_KEVT_POSTAMBLE(flags) \
({ \
	buzzz_g.cur = (void*)(((buzzz_kevt_log_t*)buzzz_g.cur) + 1); \
	buzzz_g.priv.kevt.count++; \
	if (buzzz_g.cur >= buzzz_g.end) { \
		buzzz_g.wrap = BUZZZ_TRUE; \
		buzzz_g.cur = buzzz_g.log; \
	} \
	BUZZZ_UNLOCK(flags); \
})

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_log0
 *  Add a kernel event log with 0 arguments
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log0(uint32_t kevt_id)
{
	unsigned long flags;
	buzzz_kevt_log_t *log_p;

	_BUZZZ_KEVT_PREAMBLE(flags, log_p, kevt_id, 0);
	_BUZZZ_KEVT_POSTAMBLE(flags);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_log1
 *  Add a kernel event log with 1 arguments
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log1(uint32_t kevt_id, uint32_t arg1)
{
	unsigned long flags;
	buzzz_kevt_log_t *log_p;

	_BUZZZ_KEVT_PREAMBLE(flags, log_p, kevt_id, 1);
	log_p->arg1 = arg1;
	_BUZZZ_KEVT_POSTAMBLE(flags);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_log2
 *  Add a kernel event log with 2 arguments
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log2(uint32_t kevt_id, uint32_t arg1, uint32_t arg2)
{
	unsigned long flags;
	buzzz_kevt_log_t *log_p;

	_BUZZZ_KEVT_PREAMBLE(flags, log_p, kevt_id, 2);
	log_p->arg1 = arg1;
	log_p->arg2 = arg2;
	_BUZZZ_KEVT_POSTAMBLE(flags);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_log3
 *  Add a kernel event log with 3 arguments
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC
buzzz_kevt_log3(uint32_t kevt_id, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
	unsigned long flags;
	buzzz_kevt_log_t *log_p;

	_BUZZZ_KEVT_PREAMBLE(flags, log_p, kevt_id, 3);
	log_p->arg1 = arg1;
	log_p->arg2 = arg2;
	log_p->arg3 = arg3;
	_BUZZZ_KEVT_POSTAMBLE(flags);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_kevt_enable
 *  Invoked on each CPU to enable the PMU
 * +---------------------------------------------------------------------------+
 */
static void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_enable(void *none)
{
#if defined(CONFIG_ARM)
	__armv7_pmu_enable();
#endif  /*  CONFIG_ARM */
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_kevt_disable
 *  Invoked on each CPU to disable the PMU
 * +---------------------------------------------------------------------------+
 */
static void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_disable(void *none)
{
#if defined(CONFIG_ARM)
	__armv7_pmu_disable();
#endif  /*  CONFIG_ARM */
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_kevt_bind
 * +---------------------------------------------------------------------------+
 */
static void BUZZZ_NOINSTR_FUNC
_buzzz_kevt_bind(void *info)
{
	uint32_t counter;
	buzzz_pmu_event_desc_t *desc;
	buzzz_status_t *status;
	uint32_t pmu_group = *((uint32_t*)info);

	desc = (buzzz_pmu_event_desc_t *)&buzzz_pmu_event_g[pmu_group][0];

#if defined(CONFIG_ARM)
	for (counter = 0U; counter < BUZZZ_PMU_COUNTERS; counter++) {
		__armv7_pmu_config_buzzz_ctr(counter, (desc + counter)->evtid);
	}
#endif  /*  CONFIG_ARM */

	status = &per_cpu(kevt_status, raw_smp_processor_id());
	if (pmu_group == BUZZZ_PMU_GROUP_UNDEF)
		*status = BUZZZ_STATUS_DISABLED;
	else
		*status = BUZZZ_STATUS_ENABLED;

	/* CAUTION: assumes BUZZZ_PMU_COUNTERS is 4 */
	BUZZZ_PRINT("Bind group<%u:%s>  <%x:%s> <%x:%s> <%x:%s> <%x:%s>",
		pmu_group, __buzzz_pmu_group(pmu_group),
		BUZZZ_PMON_VAL((desc + 0)->evtid, m), __buzzz_pmu_event(pmu_group, 0),
		BUZZZ_PMON_VAL((desc + 1)->evtid, m), __buzzz_pmu_event(pmu_group, 1),
		BUZZZ_PMON_VAL((desc + 2)->evtid, m), __buzzz_pmu_event(pmu_group, 2),
		BUZZZ_PMON_VAL((desc + 3)->evtid, m), __buzzz_pmu_event(pmu_group, 3));
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_start
 *  Start KVT tool tracing by invoking _buzzz_kevt_enable in each CPU.
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC /* Start kevt tracing */
buzzz_kevt_start(void)
{
	buzzz_status_t *status;
	int cpu;
	__buzzz_pre_start();

	/* Select the pmu_group */
	on_each_cpu(_buzzz_kevt_bind, &buzzz_g.priv.kevt.pmu_group, 1);
	on_each_cpu(_buzzz_kevt_enable, NULL, 1);

	/* Other CPUs may not have yet completed _buzzz_kevt_bind ... */
	for_each_online_cpu(cpu) {
		status = &per_cpu(kevt_status, cpu);
		if (*status != BUZZZ_STATUS_ENABLED)
			BUZZZ_PRERR("on cpu<%d> kevt_status not yet BUZZZ_STATUS_ENABLED",
				cpu);
	}

	__buzzz_post_start();

	BUZZZ_PRINT("buzzz_kevt_start");
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_stop
 *  Stop KEVT tool tracing by invoking _buzzz_kevt_disable on each CPU.
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC /* Stop kevt tracing */
buzzz_kevt_stop(void)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED) {

		buzzz_pmu_group_t pmu_group;

		buzzz_kevt_log0(BUZZZ_KEVT_ID_BUZZZ_TMR);

		__buzzz_pre_stop();

		pmu_group = BUZZZ_PMU_GROUP_UNDEF;
		on_each_cpu(_buzzz_kevt_bind, &pmu_group, 1);
		on_each_cpu(_buzzz_kevt_disable, NULL, 1);

		__buzzz_post_stop();
	}
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_kevt_snprintf
 *  Perform the formatted print using the registered format strings.
 * +---------------------------------------------------------------------------+
 */
static int BUZZZ_NOINSTR_FUNC
_buzzz_kevt_snprintf(char *p, int bytes, buzzz_kevt_log_t *log)
{
	char *fmt = buzzz_g.klogs[log->arg0.klog.id];
	switch (log->arg0.klog.args) {
		case 0: BUZZZ_SNPRINTF(fmt); break;
		case 1: BUZZZ_SNPRINTF(fmt, log->arg1); break;
		case 2: BUZZZ_SNPRINTF(fmt, log->arg1, log->arg2); break;
		case 3: BUZZZ_SNPRINTF(fmt, log->arg1, log->arg2, log->arg3); break;
		default:
			BUZZZ_PRERR("invalid number of args<%d>", log->arg0.klog.args);
			break;
	}

	BUZZZ_SNPRINTF("%s", CLRnl);
	return bytes;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_parse_log
 *  Parsing of packed arguments or requiring special handling
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_kevt_parse_log(char *p, int bytes, buzzz_kevt_log_t *log)
{
	if (log->arg0.klog.id == BUZZZ_KEVT_ID_SCHEDULE) {
		struct task_struct *ptsk, *ntsk;
		ptsk = (struct task_struct *)log->arg1;
		ntsk = (struct task_struct *)log->arg2;
		BUZZZ_SNPRINTF(CLRc "TASK_SWITCH from[%s %u:%u:%u] to[%s %u:%u:%u]\n",
			ptsk->comm, ptsk->pid, ptsk->normal_prio, ptsk->prio,
			ntsk->comm, ntsk->pid, ntsk->normal_prio, ntsk->prio);
	} else {
		bytes += _buzzz_kevt_snprintf(p, bytes, log);
	}
	return bytes;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_dump_log
 *  Process a log entry, computing the delta event count, and print the kevt
 *  using the registered format string.
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_BUZZZ_KEVT) && (BUZZZ_PMU_COUNTERS != 4)
#error "buzzz_kevt_dump_log: snprintf assumes BUZZZ_PMU_COUNTERS is 4"
#endif

int BUZZZ_NOINSTR_FUNC
buzzz_kevt_dump_log(char *p, int bytes, buzzz_kevt_log_t *log)
{
	uint32_t cpu = log->arg0.klog.core;

	static uint32_t core_cnt[NR_CPUS][BUZZZ_PMU_COUNTERS]; /* [cpu][cntr] */
	static uint32_t nsecs[NR_CPUS];

	uint32_t curr[BUZZZ_PMU_COUNTERS], prev[BUZZZ_PMU_COUNTERS];
	uint32_t delta[BUZZZ_PMU_COUNTERS], ctr;

	if (cpu >= NR_CPUS) {
		BUZZZ_SNPRINTF(CLRerr "--- skipping log on CPU %u? ---" CLRnl, cpu);
		return bytes;
	}

	delta[3] = 0U;	/* compiler warning: used for cycle count */

	for (ctr = 0U; ctr < BUZZZ_PMU_COUNTERS; ctr++) {
		prev[ctr] = core_cnt[cpu][ctr];
		curr[ctr] = log->ctr[ctr];
		core_cnt[cpu][ctr] = curr[ctr];

		if (curr[ctr] < prev[ctr])  /* rollover */
			delta[ctr] = curr[ctr] + (~0U - prev[ctr]);
		else
			delta[ctr] = (curr[ctr] - prev[ctr]);

		if ((delta[ctr] > 1000000000) && (buzzz_g.priv.kevt.skip == false)) {
			BUZZZ_SNPRINTF(CLRerr "--- skipping log"
			               " (not sufficient preceeding event info ---" CLRnl);
			return bytes;
		}
	}

	/* HACK: skip first event that simply fills starting values into core_cnt */
	if (buzzz_g.priv.kevt.skip == true) {
		int cpu;
		buzzz_g.priv.kevt.skip = false;
		for (cpu = 0; cpu < NR_CPUS; cpu++)
			nsecs[cpu] = 0U;
		return bytes;
	}


	if (buzzz_g.priv.kevt.pmu_group == BUZZZ_PMU_GROUP_GENERAL) {
		uint32_t nanosecs = ((delta[3] * 1000) / BUZZZ_CYCLES_PER_USEC);
		nsecs[cpu] += nanosecs;

		BUZZZ_SNPRINTF("%s%3u%s %10u %10u %10u %10u %s%5u.%03u%s %s%6u.%03u%s\t",
			(cpu == 0)? CLRyk : CLRmk, cpu, CLRnorm,
			delta[0], delta[1], delta[2], delta[3],
			CLRgk, nanosecs / 1000, (nanosecs % 1000), CLRnorm,
			CLRck, nsecs[cpu] / 1000,
			nsecs[cpu] % 1000, CLRnorm);
	} else {
		BUZZZ_SNPRINTF("%s%3u%s %10u %10u %10u %10u\t",
			(cpu == 0)? CLRyk : CLRmk, cpu, CLRnorm,
			delta[0], delta[1], delta[2], delta[3]);
	}

	if (log->arg0.klog.id < BUZZZ_KEVT_ID_MAXIMUM) {
		bytes += buzzz_kevt_parse_log(p, bytes, log);
	} else {
		BUZZZ_SNPRINTF(CLRg);
		bytes += _buzzz_kevt_snprintf(p, bytes, log);
	}
	return bytes;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_dump_format
 *  Print the header line, listing counter event names
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC	/* Dump the header format line */
buzzz_kevt_dump_format(void)
{
	uint32_t pmu_group = buzzz_g.priv.kevt.pmu_group;

	printk("%s<CPU>%s <%s> <%s> <%s> <%s> %s\n",
	      CLRyk, CLRnorm,
	      __buzzz_pmu_event(pmu_group, 0), __buzzz_pmu_event(pmu_group, 1),
	      __buzzz_pmu_event(pmu_group, 2), __buzzz_pmu_event(pmu_group, 3),
	      (buzzz_g.priv.kevt.pmu_group == BUZZZ_PMU_GROUP_GENERAL) ?
	      CLRgk "<DELTA_MICROSECS>"  CLRnorm " "
	      CLRck "<CUMMUL_MICROSECS>" CLRnorm " <Event>" : "<Event>");
	printk(CLRbold "BUZZZ_CYCLES_PER_USEC <%d>" CLRnorm "\n\n",
	       BUZZZ_CYCLES_PER_USEC);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_dump
 *  Dump a kernel event log. Limit the number of entries dumped.
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC	/* Dump the kevt trace to console */
buzzz_kevt_dump(uint32_t limit)
{
	buzzz_g.priv.kevt.skip = true;

	buzzz_kevt_dump_format();
	buzzz_log_dump(limit, buzzz_g.priv.kevt.count, sizeof(buzzz_kevt_log_t),
	               (buzzz_dump_log_fn_t)buzzz_kevt_dump_log);
	buzzz_kevt_dump_format();
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_show
 *  Dump KEVT tool configuration
 * +---------------------------------------------------------------------------+
 */
void BUZZZ_NOINSTR_FUNC /* Dump the kevt trace to console */
buzzz_kevt_show(void)
{
	printk("Count:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.kevt.count);
	printk("Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.priv.kevt.limit);
	printk("+Limit:\t\t" CLRb "%u" CLRnl, buzzz_g.config_limit);
	printk("+Mode:\t\t" CLRb "%s" CLRnl, __buzzz_mode(buzzz_g.config_mode));
	printk("Group:\t\t" CLRb "%s" CLRnl,
	       __buzzz_pmu_group(buzzz_g.priv.kevt.pmu_group));
	printk("\n");
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_kevt_default
 *  Reset all configurations for KEVT tool
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE  int BUZZZ_NOINSTR_FUNC
__buzzz_kevt_default(void)
{
#if defined(CONFIG_BUZZZ_FUNC)
	BUZZZ_PRERR("function call tracing enabled");
	BUZZZ_PRERR("disable CONFIG_BUZZZ_FUNC for KEVT tracing");
	return BUZZZ_ERROR;
#endif  /*  CONFIG_BUZZZ_FUNC */

#if defined(CONFIG_MIPS)
	BUZZZ_PRERR("tool KEVT not ported to MIPS");
	return BUZZZ_ERROR;
#endif  /*  CONFIG_MIPS */

#if defined(CONFIG_ARM)
	if (__armv7_PMCNTENSET_test() != 0U) {
		BUZZZ_PRERR("PMU counters are in use");
		return BUZZZ_ERROR;
	}
#endif  /* !CONFIG_ARM */

	buzzz_g.tool = BUZZZ_TOOL_KEVT;

	buzzz_g.priv.kevt.count = 0U;
	buzzz_g.priv.kevt.limit = BUZZZ_INVALID;
	buzzz_g.priv.kevt.pmu_group = BUZZZ_PMU_GROUP_GENERAL;
	buzzz_g.priv.kevt.log_count = 0U;
	buzzz_g.priv.kevt.log_index = 0U;

	buzzz_g.config_mode = BUZZZ_MODE_WRAPOVER;
	buzzz_g.config_limit = BUZZZ_INVALID;

	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_BAD, CLRerr "IRQ_BAD %u");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_ACK_BAD, CLRerr "IRQ_ACK_BAD %u");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_MISROUTED, CLRerr "IRQ_MISROUTED %u");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_RESEND, CLRerr "IRQ_RESEND %u");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_CHECK, CLRerr "IRQ_CHECK %u");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_ENTRY, CLRr " >> IRQ %03u   ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_IRQ_EXIT, CLRr " << IRQ %03u   ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_SIRQ_ENTRY, CLRm ">>> SOFTIRQ ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_SIRQ_EXIT, CLRm "<<< SOFTIRQ ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_WORKQ_ENTRY, CLRb ">>>>> WORKQ ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_WORKQ_EXIT, CLRb "<<<<< WORKQ ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_SCHEDULE,
	               "TASK_SWITCH from[%s %u:%u:%u] to[%s %u:%u:%u]");
	buzzz_klog_reg(BUZZZ_KEVT_ID_SCHED_TICK, CLRb
	               "\tscheduler tick jiffies<%u> cycles<%u>");
	buzzz_klog_reg(BUZZZ_KEVT_ID_SCHED_HRTICK, CLRb "sched hrtick");
	buzzz_klog_reg(BUZZZ_KEVT_ID_GTIMER_EVENT, CLRb "\tgtimer ");
	buzzz_klog_reg(BUZZZ_KEVT_ID_GTIMER_NEXT, CLRb "\tgtimer next<%u>");
	buzzz_klog_reg(BUZZZ_KEVT_ID_BUZZZ_TMR, CLRb "\tbuzzz sys timer");
	BUZZZ_PRINT("using default configuration for KEVT");

	buzzz_kevt_show();

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_kevt_mode
 *  Configure the mode of operation for KEVT tool
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_kevt_mode(buzzz_mode_t mode)
{
	if (mode == BUZZZ_MODE_LIMITED) {
		uint32_t limit_logs;
		/* Setup default number of entries to be logged in limited mode */
		limit_logs = (BUZZZ_LOG_BUFSIZE / sizeof(buzzz_kevt_log_t));
		buzzz_g.config_limit = (limit_logs < BUZZZ_KEVT_LIMIT_LOGS) ?
			limit_logs : BUZZZ_KEVT_LIMIT_LOGS;
	}

	buzzz_g.config_mode = mode;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: __buzzz_kevt_limit
 *  Override the mode of operation to be "limited" (instead of wrapover), and
 *  restrict the number of entries to be logged.
 * +---------------------------------------------------------------------------+
 */
static BUZZZ_INLINE int BUZZZ_NOINSTR_FUNC
__buzzz_kevt_limit(uint32_t limit)
{
	BUZZZ_PRINT("overriding mode<%s>", __buzzz_mode(buzzz_g.config_mode));
	buzzz_g.config_mode = BUZZZ_MODE_LIMITED;
	buzzz_g.config_limit = limit;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kevt_config
 *  Configure the tool to use a PMU group.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC /* API to config kevt performance counter group */
buzzz_kevt_config(uint32_t pmu_group)
{
	BUZZZ_ASSERT_PMU_GROUP(pmu_group);
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	buzzz_g.priv.kevt.pmu_group = pmu_group;

	printk("buzzz_kevt_config %s\n",
	       __buzzz_pmu_group(buzzz_g.priv.kevt.pmu_group));

	return BUZZZ_SUCCESS;
}

/* Initialization of Buzzz Kevt tool during loading time */
static int BUZZZ_NOINSTR_FUNC
__init _init_buzzz_kevt(void)
{
	/* We may relax these two ... */
	if ((BUZZZ_AVAIL_BUFSIZE % sizeof(buzzz_kevt_log_t)) != 0)
		return BUZZZ_ERROR;

	if (sizeof(buzzz_kevt_log_t) != 32) /* one log per cacheline */
		return BUZZZ_ERROR;

	return BUZZZ_SUCCESS;
}

#if defined(BUZZZ_CONFIG_UNITTEST)
/*
 * +---------------------------------------------------------------------------+
 *  Section: Unittest each tool, invoked via buzzz_kcall framework
 * +---------------------------------------------------------------------------+
 */
static void BUZZZ_NOINSTR_FUNC
_buzzz_func_unittest(void)
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
_buzzz_pmon_unittest(void)
{
	int i;
	buzzz_klog_reg(1,  "udelay  1");
	buzzz_klog_reg(2,  "udelay  2");
	buzzz_klog_reg(3,  "udelay  3");
	buzzz_klog_reg(4,  "udelay  4");
	buzzz_klog_reg(5,  "udelay  5");
	buzzz_klog_reg(6,  "udelay  6");
	buzzz_klog_reg(7,  "udelay  7");
	buzzz_klog_reg(8,  "udelay  8");
	buzzz_klog_reg(9,  "udelay  9");
	buzzz_klog_reg(10, "udelay 10");

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
_buzzz_kevt_unittest(void)
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

	buzzz_stop(0);
}
#endif  /*  BUZZZ_CONFIG_UNITTEST */


#if defined(CONFIG_PROC_FS)
/*
 * +---------------------------------------------------------------------------+
 * Section: BUZZZ Proc Filesystem interface
 * +---------------------------------------------------------------------------+
 */

#if defined(CONFIG_BUZZZ_FUNC)
/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_procfs_log_show
 *  cat /proc/buzzz/log handler
 *  Note: this will display from start of the log, even if the tracing was
 *  stopped in-between, wherein the log would contain two parts, cur to end
 *  and log to cur.
 * +---------------------------------------------------------------------------+
 */
static int BUZZZ_NOINSTR_FUNC
_buzzz_procfs_log_show(char *p, char **start, off_t off, int count,
	int *eof, void *data)
{
	int bytes = 0;

	*start = p;  /* prepare start of buffer for printing */

	if (buzzz_g.tool != BUZZZ_TOOL_FUNC) {
		BUZZZ_PRERR("cat /proc/buzzz/func only supported for FUNC tool");
		goto done;
	}

	if (buzzz_g.priv.func.log_index > buzzz_g.priv.func.log_count) {
		buzzz_g.priv.func.log_index = 0U;   /* stop proc fs, return 0B */
	} else if (buzzz_g.priv.func.log_index == buzzz_g.priv.func.log_count) {
		BUZZZ_SNPRINTF(CLRbold "BUZZZ_DUMP END" CLRnl);
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

			BUZZZ_SNPRINTF(CLRbold "BUZZZ_DUMP BGN total<%u>" CLRnl,
				buzzz_g.priv.func.log_count);

			if (buzzz_g.priv.func.log_count == 0U) {
				BUZZZ_SNPRINTF(CLRbold "BUZZZ_DUMP END" CLRnl);
				goto done;
			}
		}

		bytes += buzzz_func_dump_log(p, bytes, log);

		buzzz_g.priv.func.log_index++;
	}

	/* do not place any code here */

done:
	*eof = 1;       /* end of entry */
	return bytes;   /* 0B implies end of proc fs */
}
#endif  /*  CONFIG_BUZZZ_FUNC */

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_procfs_sys_open
 *  Buzzz ProcFS open fops
 * +---------------------------------------------------------------------------+
 */
static int BUZZZ_NOINSTR_FUNC /* Proc file system open handler */
_buzzz_procfs_sys_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int BUZZZ_NOINSTR_FUNC /* cat /proc/buzzz/sys handler */
_buzzz_procfs_sys_read(struct file *file, char __user *buff, size_t size,
                     loff_t *off)
{
	size = 0;
	buzzz_show();
	return size;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: _buzzz_procfs_sys_write
 *  Buzzz ProcFS Write fops
 *
 *  Test Harness: BUZZZ ProcFS interface to trigger some test.
 *
 *  You may extend this function to parse, echo commands and parameters and
 *  invoke kernel space functions to trigger some activity, without having to
 *  derive a userspace to kernelspace IOCTL command and parameters.
 *
 *  E.g. to concoct a cpu eater that eats up 80% CPU in intervals of 10 millisec
 *	echo "c 80 10" > /proc/buzzz/sys
 *
 *  Alternatively you may use the buzzz_kcall(), invoked by buzzz -kcall command
 *  which takes a single parameter.
 *
 * +---------------------------------------------------------------------------+
 */
ssize_t BUZZZ_NOINSTR_FUNC /* echo "..." > /proc/buzzz/sys handler */
_buzzz_procfs_sys_write(struct file *file, const char __user *buff,
                        size_t size, loff_t *off)
{
	char commandline[128];
	char command;

	memset(commandline, 0, sizeof(commandline));
	if (copy_from_user(commandline, buff, size)) {
		BUZZZ_PRERR("copy_from_user error\n");
		return -EFAULT;
	}

	command = commandline[0];
	BUZZZ_PRERR("BUZZZ commandline: %s\n", commandline);

	switch (command) {
		case 'h': BUZZZ_PRINT("procfs sys help goes here\n"); break;
		default: BUZZZ_PRERR("invalid command[%c]\n", command); break;
	}

	return size;
}

static int BUZZZ_NOINSTR_FUNC
_buzzz_procfs_sys_release(struct inode *i, struct file *file)
{
	return 0;
}

/* BUZZZ ProcFS file ops */
static const struct file_operations buzzz_procfs_sys_fops =
{
	.open       =   _buzzz_procfs_sys_open,
	.read       =   _buzzz_procfs_sys_read,
	.write      =   _buzzz_procfs_sys_write,
	.release    =   _buzzz_procfs_sys_release
};

#endif  /*  CONFIG_PROC_FS */


/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_kcall
 *  Convenient kernel space function invocation with one argument using the
 *  buzzz_cli. May be used to develop a test harness.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_kcall(uint32_t arg)
{
	switch (arg) {
#if defined(BUZZZ_CONFIG_UNITTEST)
		case 0:
			printk("1. func unit test\n2. pmon unit test\n3. kevt unit test\n");
			break;
		case 1: _buzzz_func_unittest(); break;
		case 2: _buzzz_pmon_unittest(); break;
		case 3: _buzzz_kevt_unittest(); break;
#endif /*  BUZZZ_CONFIG_UNITTEST */

		default:
			break;
	}

	return BUZZZ_SUCCESS;
}


/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_cpu_kernel_info
 *  Helper function to dump some processor, chip and kernel info (HACK)
 *  Hack: Preferrable, to use /proc/cpuinfo, ...
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_MIPS)
#include <bcmutils.h>	/* bcm_chipname */
#include <siutils.h>	/* typedef struct si_pub si_t */
#include <hndcpu.h>		/* si_cpu_clock */
#include <asm/cpu-features.h>
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih
#endif	/*  CONFIG_MIPS */

#if defined(CONFIG_ARM)
#include <asm/procinfo.h>
#include <asm/cputype.h>
#endif  /*  CONFIG_ARM */

static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__buzzz_chip_kernel_info(void)
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

#if defined(CONFIG_ARM)
	unsigned int cpu_id = read_cpuid_id();
	printk("ARM Processor: implementor<0x%02x> architecture<%s> ",
		cpu_id >> 24, (cpu_architecture() == CPU_ARCH_ARMv7) ? "v7" : "UNK");
	if ((cpu_id & 0x0008f000) == 0x00007000) /* ARMv7 */
		printk("variant<0x%02X>", (cpu_id>> 16) & 127);
	printk("part<0x%03x> rev<%d>\n", (cpu_id >> 4) & 0xfff, cpu_id & 15);
#endif
	printk("BUZZZ_CYCLES_PER_USEC <%d>\n", BUZZZ_CYCLES_PER_USEC);
	printk("%s", linux_banner);
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_show
 *  Show the buzzz runtime state and tool configuration.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC /* Display the runtime status */
buzzz_show(void)
{
	printk(CLRbold);
	__buzzz_chip_kernel_info();
	printk("%s" BUZZZ_VER_FMTS CLRnl,
		BUZZZ_NAME, BUZZZ_VER_FMT(BUZZZ_SYS_VERSION));

	printk("Status:\t\t" CLRb "%s" CLRnl, __buzzz_status(buzzz_g.status));
	printk("Wrap:\t\t" CLRb "%u" CLRnl, buzzz_g.wrap);
	printk("Run:\t\t" CLRb "%u" CLRnl, buzzz_g.run);
	printk("Buf.log:\t" CLRb "0x%p" CLRnl, buzzz_g.log);
	printk("Buf.cur:\t" CLRb "0x%p" CLRnl, buzzz_g.cur);
	printk("Buf.end:\t" CLRb "0x%p" CLRnl, buzzz_g.end);

	printk("\nTool:\t\t" CLRb "%s" CLRnl, __buzzz_tool(buzzz_g.tool));

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_show(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_show(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_show(); break;
		default: break;
	}

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_start_now
 *  Stop tracing now, maybe invoked via a timer.
 *  NOTE: buzzz_g.timer is not SMP safe ...
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_start_now(buzzz_t *buzzz_p)
{
	BUZZZ_ASSERT_TOOL(buzzz_g.tool);

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_start(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_start(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_start(); break;
		default:
			BUZZZ_PRERR("unsupported start for tool %s",
				__buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_p->timer.function = NULL;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_start
 *  Start tracing control, after specified jiffies.
 *  NOTE: buzzz_g.timer is not SMP safe ...
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_start(uint32_t after_jiffies)
{
	if (buzzz_g.status != BUZZZ_STATUS_DISABLED)
		return BUZZZ_SUCCESS;

	BUZZZ_ASSERT_TOOL(buzzz_g.tool);

	if (after_jiffies == 0U) {
		return buzzz_start_now(&buzzz_g);
	}

	if (buzzz_g.timer.function != NULL) { /* not SMP safe */
		BUZZZ_PRERR("Timer already in use, overwriting");
		del_timer(&buzzz_g.timer);
	}
	setup_timer(&buzzz_g.timer,
	            (buzzz_timer_fn_t)buzzz_start_now, (unsigned long)&buzzz_g);
	buzzz_g.timer.expires = jiffies + after_jiffies;
	add_timer(&buzzz_g.timer);
	BUZZZ_PRINT("deferred start after %u jiffies\n", after_jiffies);

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_stop_now
 *  Stop the tool now, maybe invoked via a timer.
 *  NOTE: buzzz_g.timer is not SMP safe ...
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_stop_now(buzzz_t *buzzz_p)
{
	BUZZZ_ASSERT_TOOL(buzzz_g.tool);

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_stop(); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_stop(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_stop(); break;
		default:
			BUZZZ_PRERR("unsupported stop for tool %s",
				__buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}
	buzzz_p->timer.function = NULL;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_stop
 *  Stop tracing control, after specified jiffies.
 *  NOTE: buzzz_g.timer is not SMP safe ...
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_stop(uint32_t after_jiffies)
{
	if (buzzz_g.status == BUZZZ_STATUS_DISABLED)
		return BUZZZ_SUCCESS;

	BUZZZ_ASSERT_TOOL(buzzz_g.tool);

	if (after_jiffies == 0U) {
		return buzzz_stop_now(&buzzz_g);
	}
	if (buzzz_g.timer.function != NULL) {
		BUZZZ_PRERR("timer already in use, overwriting");
		del_timer(&buzzz_g.timer);
	}
	setup_timer(&buzzz_g.timer,
	            (buzzz_timer_fn_t)buzzz_stop_now, (unsigned long)&buzzz_g);
	buzzz_g.timer.expires = jiffies + after_jiffies;
	add_timer(&buzzz_g.timer);
	BUZZZ_PRINT("deferred stop after %u jiffies\n", after_jiffies);

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_pause
 *  Pause tracing control
 * +---------------------------------------------------------------------------+
 */

int BUZZZ_NOINSTR_FUNC
buzzz_pause(void)
{
	unsigned long flags;
	BUZZZ_LOCK(flags);

	if (buzzz_g.status == BUZZZ_STATUS_ENABLED) {
		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_pause);

		buzzz_g.status = BUZZZ_STATUS_PAUSED;
	}

	BUZZZ_UNLOCK(flags);
	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_play
 *  Continue tracing control, if the state was paused
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_play(void)
{
	unsigned long flags;
	BUZZZ_LOCK(flags);

	if (buzzz_g.status == BUZZZ_STATUS_PAUSED) {
		buzzz_g.status = BUZZZ_STATUS_ENABLED;

		BUZZZ_FUNC_LOG(BUZZZ_RET_IP);
		BUZZZ_FUNC_LOG(buzzz_play);
	}

	BUZZZ_UNLOCK(flags);
	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_audit
 *  Invoke an audit harness (not supported)
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_audit(void)
{
	BUZZZ_PRERR("Unsupported audit capability");
	return BUZZZ_ERROR;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_dump
 *  Dump the logged data from a tool
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_dump(uint32_t items)
{
	BUZZZ_ASSERT_TOOL(buzzz_g.tool);

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: buzzz_func_dump(items); break;
		case BUZZZ_TOOL_PMON: buzzz_pmon_dump(); break;
		case BUZZZ_TOOL_KEVT: buzzz_kevt_dump(items); break;
		default:
			BUZZZ_PRERR("Unsupported dump for tool %s",
				__buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_config_tool
 *
 *  BUZZZ supports multiple tools. Select the tool that will be using the global
 *  buzzz configuration state. Only one tool may run at a time.
 *
 *  Tools in BUZZZ: function call tracing, performance monitoring or kernel
 *  event logging.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC /* Configure the tool that will use the logging system */
buzzz_config_tool(buzzz_tool_t tool)
{
	BUZZZ_ASSERT_TOOL(tool);
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	switch (tool) {
		case BUZZZ_TOOL_FUNC: return __buzzz_func_default();
		case BUZZZ_TOOL_PMON: return __buzzz_pmon_default();
		case BUZZZ_TOOL_KEVT: return __buzzz_kevt_default();

		default:
			BUZZZ_PRERR("unsupported tool selection %s", __buzzz_tool(tool));
			return BUZZZ_ERROR;
	}

	buzzz_g.tool = tool;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_config_mode
 *  Configure the mode of operation of the tool, being wrapover or limited.
 *
 *  For function call tracing and Kernel event tracing, in a "limited" trace
 *  dump mode, the number of logs dumped is limited to the last N entries.
 *
 *  For PMON tool, if the mode is WRAPOVER, then on completion of one PMU group,
 *  the next PMU group will be selected. In the limited mode operation, the
 *  default general group will be selected. To select a different group in the
 *  limited mode of operation, invoke buzzz_config_limit().
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_config_mode(buzzz_mode_t mode)
{
	BUZZZ_ASSERT_MODE(mode);
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	switch (buzzz_g.tool) {
		/* Enable the wraparound or limited mode of operation */
		case BUZZZ_TOOL_FUNC: return __buzzz_func_mode(mode);
		case BUZZZ_TOOL_PMON: return __buzzz_pmon_mode(mode);
		case BUZZZ_TOOL_KEVT: return __buzzz_kevt_mode(mode);

		default:
			BUZZZ_PRERR("unsupported mode %s for tool %s",
				__buzzz_mode(mode), __buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_g.config_mode = mode;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_config_limit
 *
 *  For FUNC and KEVT tracing, limit the number of logs dumped.
 *  For PMON, select mode "limited" and select the PMU group to be used.
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC /* Configure a limit parameter in the tool */
buzzz_config_limit(uint32_t limit)
{
	BUZZZ_ASSERT_STATUS_IS_DISABLED();

	switch (buzzz_g.tool) {
		case BUZZZ_TOOL_FUNC: return __buzzz_func_limit(limit); /* num logs */
		case BUZZZ_TOOL_PMON: return __buzzz_pmon_limit(limit); /* PMU group */
		case BUZZZ_TOOL_KEVT: return __buzzz_kevt_limit(limit); /* num logs */

		default:
			BUZZZ_PRERR("unsupported limit %u for tool %s",
				limit, __buzzz_tool(buzzz_g.tool));
			return BUZZZ_ERROR;
	}

	buzzz_g.config_limit = limit;

	return BUZZZ_SUCCESS;
}

/*
 * +---------------------------------------------------------------------------+
 *  Function: buzzz_klog_reg
 *
 *  Register a format string to be used during pretty print of a logged eventid
 * +---------------------------------------------------------------------------+
 */
int BUZZZ_NOINSTR_FUNC
buzzz_klog_reg(uint32_t klog_id, char *klog_fmt)
{
	BUZZZ_ASSERT_KEVT_ID(klog_id);
	strncpy(buzzz_g.klogs[klog_id], klog_fmt, BUZZZ_KLOG_FMT_LENGTH - 1);
	return BUZZZ_SUCCESS;
}


/*
 * +---------------------------------------------------------------------------+
 *  Section: Miscellaneous character device (buzzz_ctl ioctl control commands)
 *
 *  File Ops Open, Ioctl and Release, for Buzzz character device.
 * +---------------------------------------------------------------------------+
 */
static int BUZZZ_NOINSTR_FUNC /* pre ioctl handling in character driver */
_buzzz_fops_open(struct inode *inodep, struct file *filep)
{
	/* int minor = MINOR(inodep->i_rdev) & 0xf; */
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
static long BUZZZ_NOINSTR_FUNC
_buzzz_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else	/* < linux-2.6.36 */
static int BUZZZ_NOINSTR_FUNC /* ioctl handler in character driver */
_buzzz_fops_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
#endif  /* < linux-2.6.36 */
{
	int ret = BUZZZ_ERROR;

	BUZZZ_PRINT("cmd[%s]", __buzzz_ioctl(cmd - BUZZZ_IOCTL_UNDEF));

	switch (cmd) {
		case BUZZZ_IOCTL_KCALL:
			BUZZZ_PRINT("invoke buzzz_kcall(param<%lu>)", arg);
			return buzzz_kcall(arg);

		case BUZZZ_IOCTL_CONFIG_TOOL:
			BUZZZ_PRINT("invoke buzzz_config_tool(%s)", __buzzz_tool(arg));
			return buzzz_config_tool((buzzz_tool_t)arg);

		case BUZZZ_IOCTL_CONFIG_MODE:
			BUZZZ_PRINT("invoke buzzz_config_mode(mode<%s>)",
				__buzzz_mode(arg));
			return buzzz_config_mode((buzzz_mode_t)arg);

		case BUZZZ_IOCTL_CONFIG_LIMIT:
			BUZZZ_PRINT("invoke buzzz_config_limit %lu", arg);
			return buzzz_config_limit(arg);

		case BUZZZ_IOCTL_CONFIG_FUNC:
			BUZZZ_PRINT("invoke buzzz_func_config(exit_enabled<%lu>", arg);
			return buzzz_func_config(arg);

		case BUZZZ_IOCTL_CONFIG_PMON:
			BUZZZ_PRINT("invoke buzzz_pmon_config(samples<%lu>, skip<%lu>)",
				(arg & 0xFFFF), (arg >> 16));
			return buzzz_pmon_config((arg & 0xFFFF), (arg >> 16));

		case BUZZZ_IOCTL_CONFIG_KEVT:
			BUZZZ_PRINT("invoke buzzz_kevt_config(event<%lu>)", arg);
			return buzzz_kevt_config(arg);

		case BUZZZ_IOCTL_SHOW:
			BUZZZ_PRINT("invoke buzzz_show");
			return buzzz_show();

		case BUZZZ_IOCTL_START:
			BUZZZ_PRINT("invoke buzzz_start(after_jiffies<%lu>)", arg);
			return buzzz_start(arg);

		case BUZZZ_IOCTL_STOP:
			BUZZZ_PRINT("invoke buzzz_stop(after_jiffies<%lu>)", arg);
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
			BUZZZ_PRINT("invoke buzzz_dump(%lu)", arg);
			return buzzz_dump(arg);

		default:
			BUZZZ_PRERR("invalid ioctl<%u>", cmd);
			return -EINVAL;
	}

	return ret;
}

static int BUZZZ_NOINSTR_FUNC
_buzzz_fops_release(struct inode *inodep, struct file *filep)
{
	return 0;
}

static const struct file_operations buzzz_fops =
{	/* file ops for buzzz character device (misc major) */
	.open           =   _buzzz_fops_open,
	.release        =   _buzzz_fops_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	.unlocked_ioctl =   _buzzz_fops_ioctl,
	.compat_ioctl   =   _buzzz_fops_ioctl
#else	/* < linux-2.6.36 */
	.ioctl          =   _buzzz_fops_ioctl
#endif  /* < linux-2.6.36 */
};

static struct miscdevice buzzz_dev =
{
	/* misc character device */
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = BUZZZ_NAME,
	.fops   = &buzzz_fops
};


/*
 * +---------------------------------------------------------------------------+
 *  Section: BUZZZ Initialization (built-into Linux kernel BSP)
 *
 *  Function: __init_buzzz
 *  - Registers a character device (misc major number) for handling control
 *    ioctl from a command line buzzz utility.
 *  - Allocates storage for the log buffer.
 *  - Create a ProcFS directory and 2 files: sys and log
 *  - Initializes each tools state.
 *
 *  BUZZZ is initialized at level 7 (late init call)
 * +---------------------------------------------------------------------------+
 */
static int BUZZZ_NOINSTR_FUNC
__init __init_buzzz(void)
{
	int err, i;
	char event_str[BUZZZ_KLOG_FMT_LENGTH];

#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *buzzz_dir = NULL;
	struct proc_dir_entry *buzzz_sys = NULL; /* system state */
#if defined(CONFIG_BUZZZ_FUNC)
	struct proc_dir_entry *buzzz_log = NULL; /* function call trace log */
#endif
#endif  /*  CONFIG_PROC_FS */

	/* Create a 'miscelaneous' character driver and register */
	if ((err = misc_register(&buzzz_dev)) != 0) {
		BUZZZ_PRERR("register misc device %s", BUZZZ_DEV_PATH);
		return err;
	}

	/* Allocate Buzzz log buffer */
	buzzz_g.log = (void *)kmalloc(BUZZZ_LOG_BUFSIZE, GFP_KERNEL);
	if (buzzz_g.log == (void*)NULL) {
		BUZZZ_PRERR("log buffer size<%d> allocation", BUZZZ_LOG_BUFSIZE);
		goto fail_dev_dereg;
	} else {
		memset(buzzz_g.log, 0, BUZZZ_LOG_BUFSIZE); /* not necessary */
	}
	buzzz_g.cur = buzzz_g.log;
	buzzz_g.end = (void*)((char*)buzzz_g.log - BUZZZ_LOGENTRY_MAXSZ);

#if defined(CONFIG_PROC_FS)
	/*
	 * Construct a ProcFS directory entry for Buzzz
	 */
	buzzz_dir = proc_mkdir(BUZZZ_NAME, NULL);
	if (buzzz_dir == NULL) {
		BUZZZ_PRERR("mkdir /proc/%s", BUZZZ_NAME);
		goto fail_free_log;
	}

	/* /proc/buzzz/sys :: _buzzz_procfs_sys_write() implements a test harness */
	buzzz_sys = proc_create_data("sys", 0644, buzzz_dir,
			&buzzz_procfs_sys_fops, NULL);
	if (!buzzz_sys) {
		BUZZZ_PRERR("create file entry /proc/%s/sys", BUZZZ_NAME);
		goto fail_free_log;
	}

#if defined(CONFIG_BUZZZ_FUNC)
	/* /proc/buzzz/log :: _buzzz_procfs_log_show will dump a log */
	buzzz_log = create_proc_read_entry(BUZZZ_NAME "/func", 0, NULL,
		_buzzz_procfs_log_show, (void*)&buzzz_g);
	if (buzzz_log == (struct proc_dir_entry*)NULL) {
		BUZZZ_PRERR("create file entry /proc/%s/func", BUZZZ_NAME);
		goto fail_free_log;
	}
#endif  /*  CONFIG_BUZZZ_FUNC */
#endif  /*  CONFIG_PROC_FS */

	/*
	 * Invoke per BUZZZ tool init
	 */
	if (_init_buzzz_func() == BUZZZ_ERROR) {
		BUZZZ_PRERR("initialize FUNC tool");
		goto fail_free_log;
	}

	if (_init_buzzz_pmon() == BUZZZ_ERROR) {
		BUZZZ_PRERR("initialize PMON tool");
		goto fail_free_log;
	}

	if (_init_buzzz_kevt() == BUZZZ_ERROR) {
		BUZZZ_PRERR("initialize KEVT tool");
		goto fail_free_log;
	}

	/*
	 * Initialize all format strings to undefined. User needs to register
	 * each event's format string using buzzz_klog_reg().
	 *
	 * See buzzz_rtr.h: buzzz_kevt_init();
	 */
	for (i = 0; i < BUZZZ_KLOG_MAXIMUM; i++) {
		snprintf(event_str, BUZZZ_KLOG_FMT_LENGTH - 1,
			"%sUNREGISTERED EVENT<%u>%s", CLRm, i, CLRnorm);
	}
	for (i = BUZZZ_KEVT_ID_UNDEF + 1; i < BUZZZ_KEVT_ID_LINUX_LAST; i++)
		buzzz_klog_reg(i, event_str); /* ignore failure return */

	buzzz_g.status = BUZZZ_STATUS_DISABLED;

	printk(CLRb "Registered device %s" BUZZZ_VER_FMTS " misc<%d,%d>" CLRnl,
		BUZZZ_DEV_PATH, BUZZZ_VER_FMT(BUZZZ_DEV_VERSION),
		MISC_MAJOR, buzzz_dev.minor);

	return BUZZZ_SUCCESS;   /* Successful initialization of Buzzz */

fail_free_log:

#if defined(CONFIG_PROC_FS)
#if defined(CONFIG_BUZZZ_FUNC)
	if (buzzz_log) {
		remove_proc_entry("/proc/" BUZZZ_NAME "/func", buzzz_log);
		buzzz_log = NULL;
	}
#endif  /*  CONFIG_BUZZZ_FUNC */
	if (buzzz_sys) {
		remove_proc_entry("/proc/" BUZZZ_NAME "/sys", buzzz_sys);
		buzzz_sys = NULL;
	}
	if (buzzz_dir) {
		remove_proc_entry(BUZZZ_NAME, buzzz_dir);
		buzzz_dir = NULL;
	}
#endif  /*  CONFIG_PROC_FS */

	if (buzzz_g.log) {
		kfree(buzzz_g.log);
		buzzz_g.log = NULL;
	}

fail_dev_dereg:
	misc_deregister(&buzzz_dev);

	return BUZZZ_ERROR;     /* Failed initialization of Buzzz */
}

late_initcall(__init_buzzz);    /* init level 7 */


/*
 * +---------------------------------------------------------------------------+
 *  Section: BUZZZ APIs Exported
 * +---------------------------------------------------------------------------+
 */
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
EXPORT_SYMBOL(buzzz_cycles);

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
EXPORT_SYMBOL(buzzz_kevt_start);
EXPORT_SYMBOL(buzzz_kevt_stop);
EXPORT_SYMBOL(buzzz_kevt_dump);
EXPORT_SYMBOL(buzzz_kevt_config);
