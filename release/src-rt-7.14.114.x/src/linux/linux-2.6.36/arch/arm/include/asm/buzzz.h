/*
 *
 * Broadcom Buzzz based Kernel Profiling and Debugging
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
 */

/*
 * -----------------------------------------------------------------------------
 * Filename     : buzzz.h
 * Description  : Exported interface for buzzz.
 *                This header file is shared by kernel driver and user mode CLI.
 *                All kernel mode code must be placed in ifdef __KERNEL__.
 *
 * Kernel Config:
 *      Config BUZZZ: Enable Buzzz and performance monitoring build
 *      Config BUZZZ_FUNC: Enables Function call tracing build
 *      Config BUZZZ_KEVT: Enable kerbel event tracing
 *  When Function call tracing or kernel event tracing is enabled, performance
 *  monitoring may not be used.
 *
 * Pre-build requirement:
 *  Configure the BUZZZ_CYCLES_PER_USEC processor clock speed.
 *  Configure the Event tracing level: BUZZZ_KEVT_LVL and BUZZZ_KEVT_DPL
 *
 * -----------------------------------------------------------------------------
 */
#ifndef _BUZZZ_H_INCLUDED_
#define _BUZZZ_H_INCLUDED_

/*
 * BUZZZ Subsystem Configuration
 *
 * - BUZZZ_CONFIG_UNITTEST: Enables kcall based unittest of buzzz subsystems.
 * - BUZZZ_CONFIG_SYS_KDBG: Enables debugging of buzzz kernel subsystems.
 * - BUZZZ_CONFIG_SYS_UDBG: Enables debugging of buzzz user cli subsystem.
 * - BUZZZ_CONFIG_COLOR_EN: Enables color printing to console.
 *
 * - BUZZZ_CONFIG_PMON_USR: Enables logging of a user counter in PMON
 *
 * To disable, comment out the corresponding BUZZZ_CONFIG_XYZ defines.
 */


/*
 * +---------------------------------------------------------------------------+
 *  Section: Compile time configuration
 * +---------------------------------------------------------------------------+
 */
#define BUZZZ_CONFIG_UNITTEST
/* #define BUZZZ_CONFIG_SYS_KDBG */
/* #define BUZZZ_CONFIG_SYS_UDBG */
#define BUZZZ_CONFIG_COLOR_EN
#define BUZZZ_CONFIG_PMON_USR

/* BUZZZ: Setup processor speed defined in terms of cycles per microsecond. */
#define BUZZZ_CYCLES_PER_USEC   (800)

/* BUZZZ: Maximum log buffer size */
#define BUZZZ_LOG_BUFSIZE       (1024 * 1024)

/* BUZZZ_FUNC: Number of last logs to be dumped on a kernel panic */
#define BUZZZ_FUNC_LIMIT_LOGS   (512)
#define BUZZZ_FUNC_PANIC_LOGS   (512)

/* BUZZZ_FUNC: Function Entry/Exit pretty print indentation */
#define BUZZZ_FUNC_INDENT_STR   "  "

/* BUZZZ_PMON: Maximum number of events in each loop */
#define BUZZZ_PMON_LOGS         (63)    /* Only 63 of 1024 used by PMON */

/* BUZZZ_PMON: Number of samples used in averaging */
#define BUZZZ_PMON_SAMPLESZ     (10U)

/* BUZZZ_KEVT: Number of last logs to be dumped */
#define BUZZZ_KEVT_LIMIT_LOGS   (1024)

/* BUZZZ_KEVT: Maximum kernel log events that may be registered */
#define BUZZZ_KLOG_MAXIMUM      (1024)

/* BUZZZ_KEVT: Maximum size of a kernel log format string */
#define BUZZZ_KLOG_FMT_LENGTH   (128)

/* BUZZZ_KEVT: Linux Kernel and Driver Datapath Tracing Levels */
#define BUZZZ_KEVT_LVL          (3)         /* Kernel tracing level */
#define BUZZZ_KEVT_DPL          (5)         /* Datapath tracing level */


/* Enable only one CPU family: MIPS or ARM and Cortex-A processor. */
#if defined(CONFIG_MIPS)
#define BUZZZ_CONFIG_CPU_MIPS_74K           /* MIPS74k */
#endif  /*  CONFIG_MIPS */


#if defined(CONFIG_ARM)
/* PMU Counters verified for below revs
 * 4709 Cortex-A9 is rev 0
 * 53573 Cortex-A7 is rev 5
 */
#if (__LINUX_ARM_ARCH__ == 7) || defined(CONFIG_CPU_V7) /* ARMv7 */
#if (CONFIG_BUZZZ_PMU_ARMV7_CA == 7)
#define BUZZZ_CONFIG_CPU_ARMV7_A7           /* ARMv7 Cortex A7  CPU */
#elif (CONFIG_BUZZZ_PMU_ARMV7_CA == 9)
#define BUZZZ_CONFIG_CPU_ARMV7_A9           /* ARMv7 Cortex A9  CPU */
#elif (CONFIG_BUZZZ_PMU_ARMV7_CA == 15)
#define BUZZZ_CONFIG_CPU_ARMV7_A15          /* ARMv7 Cortex A15 CPU */
#else   /*  CONFIG_BUZZZ_KEVT_ARMV7_CA != 7, 9, 15 */
#error "ARMv7 : Unknown Cortex-A processor type"
#endif  /*  CONFIG_BUZZZ_KEVT_ARMV7_CA != 7, 9, 15 */
#endif  /*  (__LINUX_ARM_ARCH__ == 7) || CONFIG_CPU_V7 */
#endif  /*  CONFIG_ARM */

/*
 * +---------------------------------------------------------------------------+
 *  Section: BUZZZ defines
 * +---------------------------------------------------------------------------+
 */
/* BUZZZ System versioning */
#define BUZZZ_NAME              "buzzz"

#define BUZZZ_VERSION(a, b, c)  (((a) << 16) + ((b) << 8) + ((c) << 0))
#define BUZZZ_VER_A(v)          (((v) >> 16) & 0xff)
#define BUZZZ_VER_B(v)          (((v) >>  8) & 0xff)
#define BUZZZ_VER_C(v)          (((v) >>  0) & 0xff)

#define BUZZZ_VER_FMTS          " ver[%u.%u.%u]"
#define BUZZZ_VER_FMT(v)        BUZZZ_VER_A(v), BUZZZ_VER_B(v), BUZZZ_VER_C(v)

#define BUZZZ_SYS_VERSION       (BUZZZ_VERSION(01, 00, 00))
#define BUZZZ_TOOL_HDRSIZE      (4 * 1024)

/* Character device */
#define BUZZZ_CLI_VERSION       (BUZZZ_VERSION(02, 00, 00))
#define BUZZZ_DEV_VERSION       (BUZZZ_VERSION(02, 00, 00))
#define BUZZZ_DEV_PATH          "/dev/" BUZZZ_NAME

/* Color coded formatted buzzz log dumps */
#if defined(BUZZZ_CONFIG_COLOR_EN)
#define BUZZZCLR(clr_code)      clr_code
#else   /* !BUZZZ_CONFIG_COLOR_EN */
#define BUZZZCLR(clr_code)      ""
#endif  /* !BUZZZ_CONFIG_COLOR_EN */

/* Miscellaneous defines */
#define BUZZZ_ERROR             (-1)
#define BUZZZ_SUCCESS           (0)
#define BUZZZ_FAILURE           BUZZZ_ERROR

#define BUZZZ_DISABLE           (0)
#define BUZZZ_ENABLE            (1)

#define BUZZZ_FALSE             (0)
#define BUZZZ_TRUE              (1)

#define BUZZZ_INVALID           (~0U)
#define BUZZZ_NULL_STMT         do { /* Nothing */ } while(0)

#define BUZZZ_INLINE        inline  __attribute__ ((always_inline))
#define BUZZZ_NOINSTR_FUNC  __attribute__ ((no_instrument_function))

#define BUZZZ_RET_IP        (uint32_t)__builtin_return_address(0)
#define BUZZZ_CUR_IP        {{ __label__ __buzzz; __buzzz: (uint32_t)&&_buzzz;}}

static BUZZZ_INLINE int BUZZZ_NULL_FUNC(void) { return BUZZZ_SUCCESS; }


/*
 * +---------------------------------------------------------------------------+
 *  Section: Shared by user space buzzz_cli.c and kernel space buzzz.c
 * +---------------------------------------------------------------------------+
 */
#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(val)         BUZZZ_TOOL_ ## val,
typedef /* Only a single user/tool may be enabled at a time */
enum buzzz_tool {
	BUZZZ_DEFN(UNDEF)
	BUZZZ_DEFN(FUNC)        /* Function call tracing */
	BUZZZ_DEFN(PMON)        /* Algorithm performance monitoring */
	BUZZZ_DEFN(KEVT)        /* Kernel space event tracing */
	BUZZZ_DEFN(MAXIMUM)
} buzzz_tool_t;


#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(val)         BUZZZ_STATUS_ ## val,
typedef /* Runtime state of the tool with respect to logging */
enum buzzz_status {
	BUZZZ_DEFN(DISABLED)
	BUZZZ_DEFN(ENABLED)
	BUZZZ_DEFN(PAUSED)
	BUZZZ_DEFN(MAXIMUM)
} buzzz_status_t;


#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(val)         BUZZZ_MODE_ ## val,
typedef /* Auto flushing or bounding the logging */
enum buzzz_mode {
	BUZZZ_DEFN(UNDEF)
	BUZZZ_DEFN(WRAPOVER)    /* wrap around log buffer */
	BUZZZ_DEFN(LIMITED)     /* limited logging */
	BUZZZ_DEFN(TRANSMIT)    /* auto tranmission of log upon buffer fill */
	BUZZZ_DEFN(MAXIMUM)
} buzzz_mode_t;


/*
 * +---------------------------------------------------------------------------+
 *  Character device and ioctl values.
 * +---------------------------------------------------------------------------+
 */
#include <linux/major.h>    /* MISC_MAJOR */
#include <linux/ioctl.h>

#define BUZZZ_IOCTL_BASE            'Z'

#define BUZZZ_IOCTL_UNDEF           _IOR(BUZZZ_IOCTL_BASE, 0, uint32_t)
#define BUZZZ_IOCTL_CONFIG_TOOL     _IOR(BUZZZ_IOCTL_BASE, 1, uint32_t)
#define BUZZZ_IOCTL_CONFIG_MODE     _IOR(BUZZZ_IOCTL_BASE, 2, uint32_t)
#define BUZZZ_IOCTL_CONFIG_LIMIT    _IOR(BUZZZ_IOCTL_BASE, 3, uint32_t)
#define BUZZZ_IOCTL_CONFIG_FUNC     _IOR(BUZZZ_IOCTL_BASE, 4, uint32_t)
#define BUZZZ_IOCTL_CONFIG_PMON     _IOR(BUZZZ_IOCTL_BASE, 5, uint32_t)
#define BUZZZ_IOCTL_CONFIG_KEVT     _IOR(BUZZZ_IOCTL_BASE, 6, uint32_t)
#define BUZZZ_IOCTL_SHOW            _IOR(BUZZZ_IOCTL_BASE, 7, uint32_t)
#define BUZZZ_IOCTL_START           _IOR(BUZZZ_IOCTL_BASE, 8, uint32_t)
#define BUZZZ_IOCTL_STOP            _IOR(BUZZZ_IOCTL_BASE, 9, uint32_t)
#define BUZZZ_IOCTL_PAUSE           _IOR(BUZZZ_IOCTL_BASE, 10, uint32_t)
#define BUZZZ_IOCTL_PLAY            _IOR(BUZZZ_IOCTL_BASE, 11, uint32_t)
#define BUZZZ_IOCTL_AUDIT           _IOR(BUZZZ_IOCTL_BASE, 12, uint32_t)
#define BUZZZ_IOCTL_DUMP            _IOR(BUZZZ_IOCTL_BASE, 13, uint32_t)
#define BUZZZ_IOCTL_KCALL           _IOR(BUZZZ_IOCTL_BASE, 14, uint32_t)
#define BUZZZ_IOCTL_MAXIMUM         15


/*
 * +---------------------------------------------------------------------------+
 *  Section: MIPS32 Processors (74Ke)
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_MIPS)
#if defined(BUZZZ_CONFIG_CPU_MIPS_74K)

/*
 * MIPS 74Ke Performance Counter Event ids and counter
 * Ref: MIPS32 74K Processor Core Family Software user's Manual, Revision 01.02
 */
#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(VER, EVTVAL, EVTNAME) MIPS ## VER ## _ ## EVTNAME = (EVTVAL),
typedef
enum buzzz_pmu_event
{
	BUZZZ_DEFN(74KE,   0, CYCLES_ELAPSED) /* 0 1 2 3: Cycles elapsed */
	BUZZZ_DEFN(74KE,   1, INSTR_COMPL)    /* 0 1 2 3: Instr gratuated */
	BUZZZ_DEFN(74KE,   2, JR_PRED)        /* 0   2  : JR target predicted */
	BUZZZ_DEFN(74KE,   2, JR_MISPRED)     /*   1   3: JR target mispredicted */
	BUZZZ_DEFN(74KE,   4, ITLB_ACCESS)    /* 0   2  : ITLB accesses */
	BUZZZ_DEFN(74KE,   4, ITLB_MISS)      /*   1   3: ITLB misses */
	BUZZZ_DEFN(74KE,   6, IC_ACCESS)      /* 0   2  : I-Ccahe access */
	BUZZZ_DEFN(74KE,   6, IC_MISS)        /*   1   3: I-Ccahe miss */
	BUZZZ_DEFN(74KE,   7, CYCLES_IC_MISS) /* 0   2  : Cycles I-Cache miss */
	BUZZZ_DEFN(74KE,   8, CYCLES_I_UCACC) /* 0   2  : Cycles I uncache access */
	BUZZZ_DEFN(74KE,   9, IUC_FULL)       /* 0   2  : I-fetch IUC full */
	BUZZZ_DEFN(74KE,  13, ALU_CAND_POOL)  /* 0   2  : ALU cand pool full */
	BUZZZ_DEFN(74KE,  13, AGEN_CAND_POOL) /*   1   3: AGEN cand pool full */
	BUZZZ_DEFN(74KE,  14, ALU_COMPL_BUF)  /* 0   2  : ALU Compl Buff deplete */
	BUZZZ_DEFN(74KE,  14, AGEN_COMPL_BUF) /*   1   3: AGEN Compl Buff deplete */
	BUZZZ_DEFN(74KE,  16, ALU_NO_ISSUE)   /* 0   2  : No ALU pipe issue */
	BUZZZ_DEFN(74KE,  16, AGEN_NO_ISSUE)  /*   1   3: No AGEN pipe issue */
	BUZZZ_DEFN(74KE,  17, ALU_NO_OPER)    /* 0   2  : No ALU pipe operands */
	BUZZZ_DEFN(74KE,  17, AGEN_NO_OPER)   /*   1   3: No AGEN pipe operands */
	BUZZZ_DEFN(74KE,  20, ISS1_INSTR)     /* 0   2  : 1 instr issued */
	BUZZZ_DEFN(74KE,  20, ISS2_INSTR)     /*   1   3: 2 instr issued */
	BUZZZ_DEFN(74KE,  21, OOO_ALU)        /* 0   2  : ALU Out of order issue */
	BUZZZ_DEFN(74KE,  21, OOO_AGEN)       /*   1   3: AGEN Out of order issue */
	BUZZZ_DEFN(74KE,  23, LD_CACHEABLE)   /* 0   2  : Cacheable loads */
	BUZZZ_DEFN(74KE,  23, DCACHE_ACCESS)  /*   1   3: D-Ccache access */
	BUZZZ_DEFN(74KE,  24, DCACHE_WBACK)   /* 0   2  : D-Ccahe writeback */
	BUZZZ_DEFN(74KE,  24, DCACHE_MISS)    /*   1   3: D-Cache misses */
	BUZZZ_DEFN(74KE,  25, JTLB_DACCESS)   /* 0   2  : JTLB d-side access */
	BUZZZ_DEFN(74KE,  25, JTLB_XL_FAIL)   /*   1   3: JTLB d-side xlate fail */
	BUZZZ_DEFN(74KE,  28, L2_WBACK)       /* 0   2  : L2 cache writebacks */
	BUZZZ_DEFN(74KE,  28, L2_ACCESS)      /*   1   3: L2 cache access */
	BUZZZ_DEFN(74KE,  29, L2_MISS)        /* 0   2  : L2 cache misses */
	BUZZZ_DEFN(74KE,  29, L2_MISS_CYCLES) /*   1   3: L2 cache miss cycles */
	BUZZZ_DEFN(74KE,  30, FSB_LOW)        /* 0   2  : Fill Store Buffer < 1/2 */
	BUZZZ_DEFN(74KE,  30, FSB_HI)         /*   1   3: Fill Store Buffer > 1/2 */
	BUZZZ_DEFN(74KE,  31, LDQ_LOW)        /* 0   2  : Load Data Queue < 1/2 */
	BUZZZ_DEFN(74KE,  31, LDQ_HI)         /*   1   3: Load Data Queue > 1/2 */
	BUZZZ_DEFN(74KE,  32, WBB_LOW)        /* 0   2  : Write back buffer < 1/2 */
	BUZZZ_DEFN(74KE,  32, WBB_HI)         /*   1   3: Write back buffer > 1/2 */
	BUZZZ_DEFN(74KE,  38, BR_LIKELY)      /* 0   2  : Likely branches */
	BUZZZ_DEFN(74KE,  38, MIS_BR_LIKELY)  /*   1   3: Mispred branch likely */
	BUZZZ_DEFN(74KE,  39, CONDITIONAL)    /* 0   2  : Conditional branches */
	BUZZZ_DEFN(74KE,  39, MISPREDICTED)   /*   1   3: Mispred cond branches */
	BUZZZ_DEFN(74KE,  40, INTEGER)        /* 0   2  : Integer instructions */
	BUZZZ_DEFN(74KE,  40, FLOAT)          /*   1   3: FloatingPt instr */
	BUZZZ_DEFN(74KE,  41, LOAD)           /* 0   2  : Load instructions */
	BUZZZ_DEFN(74KE,  41, STORE)          /*   1   3: Store instructions */
	BUZZZ_DEFN(74KE,  42, JUMP)           /* 0   2  : Jump instructions */
	BUZZZ_DEFN(74KE,  43, MULDIV)         /*   1   3: int mul/div instr */
	BUZZZ_DEFN(74KE,  46, LOAD_UNCACHE)   /* 0   2  : Load uncached */
	BUZZZ_DEFN(74KE,  46, STORE_UNCACHE)  /*   1   3: Store uncached */
	BUZZZ_DEFN(74KE,  52, PREFETCH)       /* 0   2  : Prefetch instr */
	BUZZZ_DEFN(74KE,  52, PREFETCH_NULL)  /*   1   3: Prefetch found in cache */
	BUZZZ_DEFN(74KE,  53, COMPL0_INSTR)   /* 0   2  : Cycles no instr */
	BUZZZ_DEFN(74KE,  53, LOAD_MISS)      /*   1   3: Load miss grad */
	BUZZZ_DEFN(74KE,  54, COMPL1_INSTR)   /* 0   2  : Cycle completed 0 instr */
	BUZZZ_DEFN(74KE,  54, COMPL2_INSTR)   /*   1   3: Cycle complete 1 instr */
	BUZZZ_DEFN(74KE,  56, COMPL0_MISPRED) /* 0   2  : Compl0 due to mispred */
	BUZZZ_DEFN(74KE,  58, EXCEPTIONS)     /* 0   2  : Exceptions taken */
	BUZZZ_DEFN(74KE, 127, NONE)
} buzzz_pmu_event_t;

/*
 * MIPS74K performance events are arranged to have all 4 performance counters
 * counting a unique event, constituting similar events to form a group.
 * See enum buzzz_pmon_event_t for the grouping of events into 12 groups.
 */
#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(group)       BUZZZ_PMU_GROUP_ ## group,
typedef
enum buzzz_pmu_group
{
	BUZZZ_DEFN(UNDEF)
	BUZZZ_DEFN(GENERAL)
	BUZZZ_DEFN(ICACHE)
	BUZZZ_DEFN(DCACHE)
	BUZZZ_DEFN(TLB)
	BUZZZ_DEFN(COMPLETED)
	BUZZZ_DEFN(ISSUE_OOO)
	BUZZZ_DEFN(INSTR_GENERAL)
	BUZZZ_DEFN(INSTR_MISCELLANEOUS)
	BUZZZ_DEFN(INSTR_LOAD_STORE)
	BUZZZ_DEFN(CYCLES_IDLE_FULL)
	BUZZZ_DEFN(CYCLES_IDLE_WAIT)
	/* BUZZZ_DEFN(L2_CACHE) */
	BUZZZ_DEFN(MAXIMUM)
} buzzz_pmu_group_t;
#define BUZZZ_PMU_COUNTERS      (4)
#define BUZZZ_PMU_GROUPS        (BUZZZ_PMU_GROUP_MAXIMUM)

#endif  /*  BUZZZ_CONFIG_CPU_MIPS_74K */
#endif  /*  CONFIG_MIPS */


/*
 * +---------------------------------------------------------------------------+
 *  Section: ARMv7 Cortex A series
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_ARM)

#if !defined(CONFIG_CPU_CP15)
/* warn: CP15 support not enabled in linux ... */
#endif

/*
 * Event ref. value definitions from the ARMv7 Architecture Reference Manual
 * Definition of terms and events in Section C12.8 ARM DDI 0406C.b ID072512
 *
 * NOTATION:
 *
 * ARMV7 prefixed event are ARMv7 Cortex-A processor implementation independent
 *
 * NOTE: Instruction count event 0x08 as well as Instruction speculatively
 * executed count 0x1b is broken on Cortex-A9. Use Instruction from renaming
 * stage 0x68 as the nearest best approximation of instruction count.
 *
 * ARMA7 prefixed event are Cortex-A7 implementation specific:
 * Definition of terms and events in Cortex-A7 TRM, Chapter 11, DDI 0464F r0p5
 *
 * ARMA9 prefixed event are Cortex-A9 implementation specific
 * Definition of terms and events in Cortex-A9 TRM, Chapter 11, DDI 0388H r4p0
 *
 * Please check each Cortex-A# TRM for more event definitions.
 */
#undef BUZZZ_DEFN
#define BUZZZ_DEFN(VER, EVTVAL, EVTNAME) ARM ## VER ## _ ## EVTNAME = (EVTVAL),
typedef
enum buzzz_pmu_event
{
	BUZZZ_DEFN(V7, 0x00, SW_INC)            /* Software increment */
	BUZZZ_DEFN(V7, 0x01, L1I_CACHE_REFILL)  /* Level1 I-Cache refill */
	BUZZZ_DEFN(V7, 0x02, L1I_TLB_REFILL)    /* Intruction fetch TLB refill */
	BUZZZ_DEFN(V7, 0x03, L1D_CACHE_REFILL)  /* Level1 D-Cache refill */
	BUZZZ_DEFN(V7, 0x04, L1D_CACHE)         /* L1 D-Cache access */
	BUZZZ_DEFN(V7, 0x05, L1D_TLB_REFILL)    /* L1 data TLB refill */
	BUZZZ_DEFN(V7, 0x06, LD_RETIRED)        /* Data load executed */
	BUZZZ_DEFN(V7, 0x07, ST_RETIRED)        /* Data store executed */
	BUZZZ_DEFN(V7, 0x08, INST_RETIRED)      /* Instruction executed (!CA9) */
	BUZZZ_DEFN(V7, 0x09, EXC_TAKEN)         /* Exception taken */
	BUZZZ_DEFN(V7, 0x0a, EXC_RETURN)        /* Exception return executed */
	BUZZZ_DEFN(V7, 0x0b, CID_WRITE_RETIRED) /* Context ID change executed */
	BUZZZ_DEFN(V7, 0x0c, PC_WRITE_RETIRED)  /* PC change executed */
	BUZZZ_DEFN(V7, 0x0d, BR_IMMED_RETIRED)  /* B/BL/BLX immed executed */
	BUZZZ_DEFN(V7, 0x0e, BR_RETURN_RETIRED) /* Procedure return: CA9 see 0x6E */
	BUZZZ_DEFN(V7, 0x0f, UNALIGN_LDST_RETIRED) /* Unaligned access executed */
	BUZZZ_DEFN(V7, 0x10, BR_MIS_PRED)       /* Branch mispredicted */
	BUZZZ_DEFN(V7, 0x11, CPU_CYCLE)         /* Cycle count */
	BUZZZ_DEFN(V7, 0x12, BR_PRED)           /* Predictable branches */
	BUZZZ_DEFN(V7, 0x13, MEM_ACCESS)        /* Data memory access: CA9 broken */
	BUZZZ_DEFN(V7, 0x14, L1I_CACHE)         /* Instruction cache access */
	BUZZZ_DEFN(V7, 0x15, L1D_CACHE_WB)      /* Data cache write-back:eviction */
	BUZZZ_DEFN(V7, 0x16, L2D_CACHE)         /* L2 data cache access */
	BUZZZ_DEFN(V7, 0x17, L2D_CACHE_REFILL)  /* L2 data cache refills */
	BUZZZ_DEFN(V7, 0x18, L2D_CACHE_WB)      /* L2 data cache write back */
	BUZZZ_DEFN(V7, 0x19, BUS_ACCESS)        /* Bus accesses */
	BUZZZ_DEFN(V7, 0x1a, MEMORY_ERROR)      /* Memory errors */
	BUZZZ_DEFN(V7, 0x1b, INST_SPEC)         /* Instr speculatively executed */
	BUZZZ_DEFN(V7, 0x1c, TTBR_WRITE_RETIRED) /* Write to TTBR */
	BUZZZ_DEFN(V7, 0x1d, BUS_CYCLE)         /* Bus cycle */

#if defined(BUZZZ_CONFIG_CPU_ARMV7_A7)
	BUZZZ_DEFN(A7, 0x60, BUS_RD_ACCESS)     /* CA7: Bus access read */
	BUZZZ_DEFN(A7, 0x61, BUS_WR_ACCESS)     /* CA7: Bus access write */
	BUZZZ_DEFN(A7, 0x86, IRQ_EXC_TAKEN)     /* CA7: IRQ exceptions taken */
	BUZZZ_DEFN(A7, 0x87, FIQ_EXC_TAKEN)     /* CA7: FIQ expceptions taken */
	BUZZZ_DEFN(A7, 0xc0, EXT_MEM_REQUEST)   /* CA7: External memory request */
	BUZZZ_DEFN(A7, 0xc1, NC_EXT_MEM_REQUEST) /* CA7: Noncacheable ext mem req */
	BUZZZ_DEFN(A7, 0xc2, PREF_LINEFILL)     /* CA7: Prefetch linefill */
	BUZZZ_DEFN(A7, 0xc3, PREF_LINEFILL_DROP) /* CA7: Prefetch linefill drops */
	BUZZZ_DEFN(A7, 0xc4, RD_ALLOC_ENTER)    /* CA7: Entering read alloc mode */
	BUZZZ_DEFN(A7, 0xc5, RD_ALLOC)          /* CA7: Read allocate mode */
	BUZZZ_DEFN(A7, 0xc9, WR_STALL)          /* CA7: WR store full stall  */
	BUZZZ_DEFN(A7, 0xca, DATA_SNOOP)        /* CA7: Data snooped other proc */
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A7 */

#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	BUZZZ_DEFN(A9, 0x50, COH_LINEFILL_MISS) /* CA9: Coherent linefill miss */
	BUZZZ_DEFN(A9, 0x51, COH_LINEFILL_HIT)  /* CA9: Coherent linefill hit */
	BUZZZ_DEFN(A9, 0x60, ICACHE_STALL)      /* CA9: ICache stall cycles */
	BUZZZ_DEFN(A9, 0x61, DCACHE_STALL)      /* CA9: DCache stall cycles */
	BUZZZ_DEFN(A9, 0x62, TLB_STALL)         /* CA9: TLB stall cycles */
	BUZZZ_DEFN(A9, 0x63, STREX_PASSED)      /* CA9: STREX Passed */
	BUZZZ_DEFN(A9, 0x64, STREX_FAILED)      /* CA9: STREX Failed */
	BUZZZ_DEFN(A9, 0x65, DATA_EVICTION)     /* CA9: Data eviction */
	BUZZZ_DEFN(A9, 0x66, ISSUE_NONE_CYCLE)  /* CA9: Issue no dispatch cycles */
	BUZZZ_DEFN(A9, 0x67, ISSUE_EMPTY_CYCLE) /* CA9: Issue stage empty cycles */
	BUZZZ_DEFN(A9, 0x68, INSTR_RENAME_STAGE) /* CA9 Instr out of rename stage */
	BUZZZ_DEFN(A9, 0x69, DATA_LINEFILL)     /* CA9: Number of data linefills */
	BUZZZ_DEFN(A9, 0x6a, PREF_LINEFILL)     /* CA9: Prefetch linefills */
	BUZZZ_DEFN(A9, 0x6b, PREF_CACHE_HIT)    /* CA9: Hits in prefetch cache */
	BUZZZ_DEFN(A9, 0x6e, PRED_BR_RETURN)    /* CA9: Predictable funct returns */
	BUZZZ_DEFN(A9, 0x70, INSTR_MAIN_UNIT)   /* CA9: Spec main unit(s) instr */
	BUZZZ_DEFN(A9, 0x71, INSTR_ALU_UNIT)    /* CA9: Spec ALU inst */
	BUZZZ_DEFN(A9, 0x72, INSTR_LDST)        /* CA9: Spec LD|ST inst */
	BUZZZ_DEFN(A9, 0x73, INSTR_FLOATPT)     /* CA9: Spec FloatPt inst */
	BUZZZ_DEFN(A9, 0x74, INSTR_NEON)        /* CA9: Spec NEON inst */
	BUZZZ_DEFN(A9, 0x80, PLD_STALL)         /* CA9: PLD slot full stalls */
	BUZZZ_DEFN(A9, 0x81, WR_STALL)          /* CA9: WR slot full stalls */
	BUZZZ_DEFN(A9, 0x82, IMTLB_STALL)       /* CA9: main ITLB miss stalls */
	BUZZZ_DEFN(A9, 0x83, DMTLB_STALL)       /* CA9: main DTLB miss stalls */
	BUZZZ_DEFN(A9, 0x84, IUTLB_STALL)       /* CA9: micro ITLB miss stalls */
	BUZZZ_DEFN(A9, 0x85, DUTLB_STALL)       /* CA9: micro DTLB miss stalls */
	BUZZZ_DEFN(A9, 0x86, DMB_STALL)         /* CA9: DMB stalls */
	BUZZZ_DEFN(A9, 0x90, INSTR_ISB)         /* CA9: ISB instr executed */
	BUZZZ_DEFN(A9, 0x91, INSTR_DSB)         /* CA9: DSB instr executed */
	BUZZZ_DEFN(A9, 0x92, INSTR_DMB)         /* CA9: DMB instr executed */
	BUZZZ_DEFN(A9, 0x93, EXT_INTERRUPTS)    /* CA9: External interrupts */
	BUZZZ_DEFN(A9, 0xa0, PLE_CLREQ_COMPL)   /* CA9: PLE cache line req done */
	BUZZZ_DEFN(A9, 0xa1, PLE_REQ_SKIPPED)   /* CA9: PLE requests skipped */
	BUZZZ_DEFN(A9, 0xa2, PLE_FIFO_FLUSH)    /* CA9: PLE FIFO flush */
	BUZZZ_DEFN(A9, 0xa3, PLE_REQ_COMPL)     /* CA9: PLE requests completed */
	BUZZZ_DEFN(A9, 0xa4, PLE_FIFO_OVFL)     /* CA9: PLE FIFO overflow */
	BUZZZ_DEFN(A9, 0xa5, PLE_REQ_PROG)      /* CA9: PLE requests programmed */
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
} buzzz_pmu_event;


/*
 * ARMv7 performance events are arranged to have all 4 performance counters
 * counting a unique event, constituting similar events to form a group.
 * See enum buzzz_pmon_event_t for the grouping of events into 8 groups.
 */
#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(group)       BUZZZ_PMU_GROUP_ ## group,
typedef
enum buzzz_pmu_group
{
	BUZZZ_DEFN(UNDEF)
	BUZZZ_DEFN(GENERAL)
	BUZZZ_DEFN(ICACHE)
	BUZZZ_DEFN(DCACHE)
	BUZZZ_DEFN(MEMORY)
	BUZZZ_DEFN(BUS)
	BUZZZ_DEFN(BRANCH)
	BUZZZ_DEFN(EXCEPTION)
	BUZZZ_DEFN(SPURIOUS)
	BUZZZ_DEFN(PREF_COH)
	BUZZZ_DEFN(L2CACHE)
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
	BUZZZ_DEFN(INSTRTYPE)
	BUZZZ_DEFN(BARRIER)
	BUZZZ_DEFN(ISSUE)
	BUZZZ_DEFN(STALLS)
	BUZZZ_DEFN(TLBSTALLS)
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
	BUZZZ_DEFN(MAXIMUM)
} buzzz_pmu_group_t;
#define BUZZZ_PMU_COUNTERS      (4)
#define BUZZZ_PMU_GROUPS        (BUZZZ_PMU_GROUP_MAXIMUM)

#endif  /*  CONFIG_ARM */


/*
 * +---------------------------------------------------------------------------+
 *  Section: Linux Kernel baseline instrumentation.
 *  If any new baseline kernel events are added, pls update __buzzz_kevt_default
 * +---------------------------------------------------------------------------+
 */
#undef  BUZZZ_DEFN
#define BUZZZ_DEFN(event)       BUZZZ_KEVT_ID_ ## event,
typedef
enum buzzz_kevt_id
{
	BUZZZ_DEFN(UNDEF)           /* Undefined                                  */
	BUZZZ_DEFN(IRQ_BAD)         /* 1: irq                                     */
	BUZZZ_DEFN(IRQ_ACK_BAD)     /* 1: irq                                     */
	BUZZZ_DEFN(IRQ_MISROUTED)   /* 1: irq                                     */
	BUZZZ_DEFN(IRQ_RESEND)      /* 1: irq                                     */
	BUZZZ_DEFN(IRQ_CHECK)       /* 1: irq                                     */
	BUZZZ_DEFN(IRQ_ENTRY)       /* 2: irq, handler                            */
	BUZZZ_DEFN(IRQ_EXIT)        /* 2: irq, handler                            */
	BUZZZ_DEFN(SIRQ_ENTRY)      /* 1: action                                  */
	BUZZZ_DEFN(SIRQ_EXIT)       /* 1: action                                  */
	BUZZZ_DEFN(WORKQ_ENTRY)     /* 1: function                                */
	BUZZZ_DEFN(WORKQ_EXIT)      /* 1: function                                */
	BUZZZ_DEFN(SCHEDULE)        /* 4: prev->[pid,prio], next->[pid,prio]      */
	BUZZZ_DEFN(SCHED_TICK)      /* 2: jiffies cycles                          */
	BUZZZ_DEFN(SCHED_HRTICK)    /* 0:                                         */
	BUZZZ_DEFN(GTIMER_EVENT)    /* 1: event_handler                           */
	BUZZZ_DEFN(GTIMER_NEXT)     /* 1: next                                    */
	BUZZZ_DEFN(BUZZZ_TMR)       /* 0:                                         */

	BUZZZ_DEFN(LINUX_LAST)

	BUZZZ_KEVT_ID_MAXIMUM = BUZZZ_KLOG_MAXIMUM
} buzzz_kevt_id_t;


/*
 * +---------------------------------------------------------------------------+
 *  Section: Kernel space APIs
 * +---------------------------------------------------------------------------+
 */
#if defined(CONFIG_BUZZZ)

#ifndef __ASSEMBLY__

#if defined(__cplusplus)
extern "C" {
#endif  /*  __cplusplus */

#if defined(__KERNEL__)
#include <linux/types.h>            /* ISO C99 7.18 Integer types */
#include <linux/version.h>
#else   /* !__KERNEL__ */
#include <stdint.h>                 /* ISO C99 7.18 Integer types */
#endif  /* !__KERNEL__ */

#if defined(__KERNEL__)

typedef void (*buzzz_timer_fn_t)(unsigned long);
typedef char buzzz_fmt_t[BUZZZ_KLOG_FMT_LENGTH]; /* Format string type */

typedef struct buzzz_pmu_event_desc {
	int evtid; /* PMU event selection value */
	char * evtname; /* PMU event name */
} buzzz_pmu_event_desc_t;

/*
 * BUZZZ tool control APIs exposed for kernel space invocation
 */
int buzzz_show(void);
int buzzz_start(uint32_t after_jiffies);
int buzzz_stop(uint32_t after_jiffies);
int buzzz_pause(void);
int buzzz_play(void);
int buzzz_audit(void);
int buzzz_dump(uint32_t items);
int buzzz_kcall(uint32_t args);

int buzzz_config_tool(buzzz_tool_t tool);
int buzzz_config_mode(buzzz_mode_t mode);
int buzzz_config_limit(uint32_t limit);

int buzzz_klog_reg(uint32_t klog_id, char * klog_fmt);

uint32_t buzzz_cycles(void);

/*
 * BUZZZ_FUNC: Function call tracing kernel space APIs
 */
void __cyg_profile_func_enter(void * called, void * caller);
void __cyg_profile_func_exit(void * called, void * caller);
void buzzz_func_log0(uint32_t event);
void buzzz_func_log1(uint32_t event, uint32_t u1);
void buzzz_func_log2(uint32_t event, uint32_t u1, uint32_t u2);
void buzzz_func_log3(uint32_t event, uint32_t u1, uint32_t u2, uint32_t u3);
void buzzz_func_start(void);
void buzzz_func_stop(void);
void buzzz_func_panic(void);
void buzzz_func_dump(uint32_t limit);
int  buzzz_func_config(uint32_t config_exit);

#define BUZZZ_FUNC_LOG(func)   \
	__cyg_profile_func_enter((void*)func, __builtin_return_address(0))

/*
 * BUZZZ_PMON: Performance Monitoring kernel space APIs
 */
void buzzz_pmon_bgn(void);
void buzzz_pmon_clr(void);
void buzzz_pmon_log(uint32_t event);
void buzzz_pmon_end(uint32_t event);    /* last event id logged */
void buzzz_pmon_start(void);
void buzzz_pmon_stop(void);
int  buzzz_pmon_config(uint32_t config_samples, uint32_t config_skip);

#if defined(BUZZZ_CONFIG_PMON_USR)
/*
 * Counter will be logged on buzzz_pmon_end(), may be used to record an
 * application state, such as txmpduperampdu.
 */
extern unsigned int buzzz_pmon_usr_g;
#endif  /*  BUZZZ_CONFIG_PMON_USR */

/*
 * BUZZZ_KEVT: Kernel Event logging kernel space APIs
 */
void buzzz_kevt_log0(uint32_t event);
void buzzz_kevt_log1(uint32_t event, uint32_t u1);
void buzzz_kevt_log2(uint32_t event, uint32_t u1, uint32_t u2);
void buzzz_kevt_log3(uint32_t event, uint32_t u1, uint32_t u2, uint32_t u3);
void buzzz_kevt_start(void);
void buzzz_kevt_stop(void);
void buzzz_kevt_dump(uint32_t limit);
int  buzzz_kevt_config(uint32_t config_exit);

/*
 * +---------------------------------------------------------------------------+
 *  Section: BUZZZ_KEVT MACROS for embedding instrumentation in source code.
 * +---------------------------------------------------------------------------+
 */
#if defined(BUZZZ_KEVT_DPL) && (BUZZZ_KEVT_DPL >= 1)
#define BUZZZ_DPL1(ID, N, ARG...)   buzzz_kevt_log ##N(BUZZZ_KEVT__ ##ID, ##ARG)
#else
#define BUZZZ_DPL1(ID, N, ARG...)   BUZZZ_NULL_STMT
#endif  /* ! BUZZZ_KEVT_DPL >= 1 */
#if defined(BUZZZ_KEVT_DPL) && (BUZZZ_KEVT_DPL >= 2)
#define BUZZZ_DPL2(ID, N, ARG...)   buzzz_kevt_log ##N(BUZZZ_KEVT__ ##ID, ##ARG)
#else
#define BUZZZ_DPL2(ID, N, ARG...)   BUZZZ_NULL_STMT
#endif  /* ! BUZZZ_KEVT_DPL >= 2 */
#if defined(BUZZZ_KEVT_DPL) && (BUZZZ_KEVT_DPL >= 3)
#define BUZZZ_DPL3(ID, N, ARG...)   buzzz_kevt_log ##N(BUZZZ_KEVT__ ##ID, ##ARG)
#else
#define BUZZZ_DPL3(ID, N, ARG...)   BUZZZ_NULL_STMT
#endif  /* ! BUZZZ_KEVT_DPL >= 3 */
#if defined(BUZZZ_KEVT_DPL) && (BUZZZ_KEVT_DPL >= 4)
#define BUZZZ_DPL4(ID, N, ARG...)   buzzz_kevt_log ##N(BUZZZ_KEVT__ ##ID, ##ARG)
#else
#define BUZZZ_DPL4(ID, N, ARG...)   BUZZZ_NULL_STMT
#endif  /* ! BUZZZ_KEVT_DPL >= 4 */
#if defined(BUZZZ_KEVT_DPL) && (BUZZZ_KEVT_DPL >= 5)
#define BUZZZ_DPL5(ID, N, ARG...)   buzzz_kevt_log ##N(BUZZZ_KEVT__ ##ID, ##ARG)
#else
#define BUZZZ_DPL5(ID, N, ARG...)   BUZZZ_NULL_STMT
#endif  /* ! BUZZZ_KEVT_DPL >= 5 */

#endif  /*  __KERNEL__ */

#if defined(__cplusplus)
}
#endif  /*  __cplusplus */

#else   /* !__ASSEMBLY__ */

/*
 * +---------------------------------------------------------------------------+
 *  Section: Kernel mode embedded assembly APIs
 * +---------------------------------------------------------------------------+
 */

#endif  /* !__ASSEMBLY__ */


#else   /* !CONFIG_BUZZZ */

/*
 * +---------------------------------------------------------------------------+
 *  Section: NO-OP kernel space APIs when CONFIG_BUZZZ is not defined
 * +---------------------------------------------------------------------------+
 */

/* BUZZZ APIs */
#define buzzz_show()                        BUZZZ_NULL_FUNC
#define buzzz_start(a)                      BUZZZ_NULL_FUNC
#define buzzz_stop(a)                       BUZZZ_NULL_FUNC
#define buzzz_pause()                       BUZZZ_NULL_FUNC
#define buzzz_play()                        BUZZZ_NULL_FUNC
#define buzzz_audit()                       BUZZZ_NULL_FUNC
#define buzzz_dump(l)                       BUZZZ_NULL_FUNC
#define buzzz_config_tool(t)                BUZZZ_NULL_FUNC
#define buzzz_config_mode(m)                BUZZZ_NULL_FUNC
#define buzzz_config_limit(l)               BUZZZ_NULL_FUNC
#define buzzz_cycles()                      BUZZZ_NULL_FUNC

#define buzzz_klog_reg(l, f)                BUZZZ_NULL_FUNC

/* BUZZZ_FUNC: Function call tracing kernel APIs */
#define __cyg_profile_func_enter(ced, cer)  BUZZZ_NULL_STMT
#define __cyg_profile_func_exit(ced, cer)   BUZZZ_NULL_STMT
#define buzzz_func_log0(e)                  BUZZZ_NULL_STMT
#define buzzz_func_log1(e, a1)              BUZZZ_NULL_STMT
#define buzzz_func_log2(e, a1, a2)          BUZZZ_NULL_STMT
#define buzzz_func_log3(e, a1, a2, a3)      BUZZZ_NULL_STMT
#define buzzz_func_start()                  BUZZZ_NULL_STMT
#define buzzz_func_stop()                   BUZZZ_NULL_STMT
#define buzzz_func_panic()                  BUZZZ_NULL_STMT
#define buzzz_func_dump(l)                  BUZZZ_NULL_STMT
#define buzzz_func_config(e)                BUZZZ_NULL_FUNC

#define BUZZZ_FUNC_LOG(f)                   BUZZZ_NULL_STMT

/* BUZZZ_PMON: Performance monitoring kernel APIs */
#define buzzz_pmon_bgn()                    BUZZZ_NULL_STMT
#define buzzz_pmon_clr()                    BUZZZ_NULL_STMT
#define buzzz_pmon_log(e)                   BUZZZ_NULL_STMT
#define buzzz_pmon_end()                    BUZZZ_NULL_STMT
#define buzzz_pmon_start()                  BUZZZ_NULL_STMT
#define buzzz_pmon_stop()                   BUZZZ_NULL_STMT
#define buzzz_pmon_config(i, s)             BUZZZ_NULL_FUNC

/* BUZZZ_KEVT: Kernel event logging kernel space APIs */
#define buzzz_kevt_log0(e)                  BUZZZ_NULL_STMT
#define buzzz_kevt_log1(e, a1)              BUZZZ_NULL_STMT
#define buzzz_kevt_log2(e, a1, a2)          BUZZZ_NULL_STMT
#define buzzz_kevt_log3(e, a1, a2, a3)      BUZZZ_NULL_STMT
#define buzzz_kevt_start()                  BUZZZ_NULL_STMT
#define buzzz_kevt_stop()                   BUZZZ_NULL_STMT
#define buzzz_kevt_dump(l)                  BUZZZ_NULL_STMT
#define buzzz_kevt_config(g)                BUZZZ_NULL_FUNC
#define BUZZZ_DPL1(N, ID, ARG...)           BUZZZ_NULL_STMT
#define BUZZZ_DPL2(N, ID, ARG...)           BUZZZ_NULL_STMT
#define BUZZZ_DPL3(N, ID, ARG...)           BUZZZ_NULL_STMT
#define BUZZZ_DPL4(N, ID, ARG...)           BUZZZ_NULL_STMT
#define BUZZZ_DPL5(N, ID, ARG...)           BUZZZ_NULL_STMT

#endif  /* !CONFIG_BUZZZ */


/*
 * -----------------------------------------------------------------------------
 * Color coded terminal display
 * -----------------------------------------------------------------------------
 */
	/* White background */
#define CLRr        BUZZZCLR("\e[0;31m")        /* red              */
#define CLRg        BUZZZCLR("\e[0;32m")        /* green            */
#define CLRy        BUZZZCLR("\e[0;33m")        /* yellow           */
#define CLRb        BUZZZCLR("\e[0;34m")        /* blue             */
#define CLRm        BUZZZCLR("\e[0;35m")        /* magenta          */
#define CLRc        BUZZZCLR("\e[0;36m")        /* cyan             */

	/* blacK "inverted" background */
#define CLRrk       BUZZZCLR("\e[0;31;40m")     /* red     on blacK */
#define CLRgk       BUZZZCLR("\e[0;32;40m")     /* green   on blacK */
#define CLRyk       BUZZZCLR("\e[0;33;40m")     /* yellow  on blacK */
#define CLRmk       BUZZZCLR("\e[0;35;40m")     /* magenta on blacK */
#define CLRck       BUZZZCLR("\e[0;36;40m")     /* cyan    on blacK */
#define CLRwk       BUZZZCLR("\e[0;37;40m")     /* whilte  on blacK */

	/* Colored background */
#define CLRcb       BUZZZCLR("\e[0;36;44m")     /* cyan    on blue  */
#define CLRyr       BUZZZCLR("\e[0;33;41m")     /* yellow  on red   */
#define CLRym       BUZZZCLR("\e[0;33;45m")     /* yellow  on magen */

	/* Generic foreground colors */
#define CLRhigh     CLRm                        /* Highlight color  */
#define CLRbold     CLRcb                       /* Bold      color  */
#define CLRbold2    CLRym                       /* Bold2     color  */
#define CLRerr      CLRyr                       /* Error     color  */
#define CLRwarn     CLRym                       /* Warn      color  */
#define CLRnorm     BUZZZCLR("\e[0m")           /* Normal    color  */
#define CLRnl       CLRnorm "\n"                /* Normal + newline */

#endif  /* !_BUZZZ_H_INCLUDED_ */
