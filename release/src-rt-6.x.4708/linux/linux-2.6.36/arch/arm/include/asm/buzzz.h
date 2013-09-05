/*
 *
 * Broadcom Buzzz based Kernel Profiling and Debugging
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 *      Config BUZZZ_KECT: Enable kerbel event tracing
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
#define BUZZZ_CONFIG_UNITTEST
/* #define BUZZZ_CONFIG_SYS_KDBG */
/* #define BUZZZ_CONFIG_SYS_UDBG */
#define BUZZZ_CONFIG_COLOR_EN
#define BUZZZ_CONFIG_PMON_USR

/* Enable only one CPU family */
#if defined(CONFIG_MIPS)
#define BUZZZ_CONFIG_CPU_MIPS_74K   /* MIPS74k */
#define BUZZZ_CYCLES_PER_USEC   (600)
#endif  /*  CONFIG_MIPS */

#if defined(CONFIG_ARM)
#define BUZZZ_CONFIG_CPU_ARMV7_A9   /* ARMv7 Cortex A9 CPU */
#define BUZZZ_CYCLES_PER_USEC   (800)
#define BUZZZ_KEVT_NANOSECS     (50) /* overhead subtracted per event */
#endif  /*  CONFIG_ARM */

/* Linux Kernel and Driver Datapath Tracing Levels */
#if defined(CONFIG_BUZZZ_KEVT)
#define BUZZZ_KEVT_LVL          3       /* Kernel tracing level */
#define BUZZZ_KEVT_DPL          5       /* Datapath tracing level */
#endif  /*  CONFIG_BUZZZ_KEVT */

/* -------------------------------------------------------------------------- */

#define BUZZZ_NAME              "buzzz"

#define BUZZZ_VERSION(a, b, c)  (((a) << 16) + ((b) << 8) + ((c) << 0))
#define BUZZZ_VER_A(v)          (((v) >> 16) & 0xff)
#define BUZZZ_VER_B(v)          (((v) >>  8) & 0xff)
#define BUZZZ_VER_C(v)          (((v) >>  0) & 0xff)

#define BUZZZ_VER_FMTS          " ver[%u.%u.%u]"
#define BUZZZ_VER_FMT(v)        BUZZZ_VER_A(v), BUZZZ_VER_B(v), BUZZZ_VER_C(v)

	/* BUZZZ System */
#define BUZZZ_SYS_VERSION       (BUZZZ_VERSION(01, 00, 00))
#define BUZZZ_TOOL_HDRSIZE      (4 * 1024)
#define BUZZZ_LOG_BUFSIZE       (128 * 4 * 1024) /* min 8K, multiple of 16 */

	/* Character device */
#define BUZZZ_CLI_VERSION       (BUZZZ_VERSION(01, 00, 00))
#define BUZZZ_DEV_VERSION       (BUZZZ_VERSION(01, 00, 00))
#define BUZZZ_DEV_PATH          "/dev/" BUZZZ_NAME


#if defined(BUZZZ_CONFIG_COLOR_EN)
#define BUZZZCLR(clr_code)      clr_code
#else   /* !BUZZZ_CONFIG_COLOR_EN */
#define BUZZZCLR(clr_code)      ""
#endif  /* !BUZZZ_CONFIG_COLOR_EN */

	/* BUZZZ defines */
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

/* Kernel log events that may be registered */
#define BUZZZ_KLOG_MAXIMUM     1024
#define BUZZZ_KLOG_FMT_LENGTH  (128)

#define BUZZZ_PMON_LOGS         (63)    /* Only 63 of 1024 used by PMON */

#define BUZZZ_PMON_EXCP_MODE    (1U << 0)  /* Exception mode counting */
#define BUZZZ_PMON_KERN_MODE    (1U << 1)  /* Kernel mode counting */
#define BUZZZ_PMON_SPRV_MODE    (1U << 2)  /* Supervisor mode counting */
#define BUZZZ_PMON_USER_MODE    (1U << 3)  /* Supervisor mode counting */

#define BUZZZ_PMON_EKSU_MODE                                                   \
	(BUZZZ_PMON_EXCP_MODE | BUZZZ_PMON_KERN_MODE                               \
	| BUZZZ_PMON_SPRV_MODE | BUZZZ_PMON_USER_MODE)

#define BUZZZ_PMON_CTRL         BUZZZ_PMON_EKSU_MODE

/*
 * Following section is shared between userspace command line control utility
 * and kernel space character device driver.
 */
#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(val)         BUZZZ_TOOL_ ## val,
typedef /* Only a single user/tool of the logging system is permitted */
enum buzzz_tool {
	BUZZZ_ENUM(UNDEF)
	BUZZZ_ENUM(FUNC)        /* Function call tracing */
	BUZZZ_ENUM(PMON)        /* Algorithm performance monitoring */
	BUZZZ_ENUM(KEVT)        /* Kernel space event tracing */
	BUZZZ_ENUM(MAXIMUM)
} buzzz_tool_t;

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(val)         BUZZZ_STATUS_ ## val,
typedef /* Runtime state of the tool with respect to logging */
enum buzzz_status {
	BUZZZ_ENUM(DISABLED)
	BUZZZ_ENUM(ENABLED)
	BUZZZ_ENUM(PAUSED)
	BUZZZ_ENUM(MAXIMUM)
} buzzz_status_t;

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(val)         BUZZZ_MODE_ ## val,
typedef /* Auto flushing or bounding the logging */
enum buzzz_mode {
	BUZZZ_ENUM(UNDEF)
	BUZZZ_ENUM(WRAPOVER)    /* wrap around log buffer */
	BUZZZ_ENUM(LIMITED)     /* limited logging */
	BUZZZ_ENUM(TRANSMIT)    /* auto tranmission of log upon buffer fill */
	BUZZZ_ENUM(MAXIMUM)
} buzzz_mode_t;

#include <linux/major.h>    /* MISC_MAJOR */
#include <linux/ioctl.h>

#define BUZZZ_IOCTL_BASE            'Z'

#define BUZZZ_IOCTL_KCALL           _IOR(BUZZZ_IOCTL_BASE, 0, uint32_t)
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
#define BUZZZ_IOCTL_MAXIMUM         14


#if defined(CONFIG_MIPS)
#if defined(BUZZZ_CONFIG_CPU_MIPS_74K)
/*
 * MIPS74K performance events are arranged to have all 4 performance counters
 * counting a unique event, constituting similar events to form a group.
 * See enum buzzz_pmon_event_t for the grouping of events into 12 groups.
 */

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(group)       BUZZZ_PMON_GROUP_ ## group,
typedef
enum buzzz_pmon_group
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
	BUZZZ_ENUM(MAXIMUM)
} buzzz_pmon_group_t;

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(event)       BUZZZ_PMON_EVENT_ ## event,
typedef /* MIPS 74K Performance counters supported by BUZZZ PMon tool */
enum buzzz_pmon_event
{                                           /* evt: cnter  */
	/* group  0: RESET */
	BUZZZ_ENUM(CTR0_SKIP)                   /* 127 */
	BUZZZ_ENUM(CTR1_SKIP)                   /* 127 */
	BUZZZ_ENUM(CTR2_SKIP)                   /* 127 */
	BUZZZ_ENUM(CTR3_SKIP)                   /* 127 */

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
	BUZZZ_ENUM(LSP_DC_MISS)                 /* 24:   1,  3 */

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

	BUZZZ_ENUM(MAXIMUM)

} buzzz_pmon_event_t;

#define BUZZZ_PMON_COUNTERS     (4)
#define BUZZZ_PMON_GROUPS       (BUZZZ_PMON_EVENT_MAXIMUM / BUZZZ_PMON_COUNTERS)

#endif  /*  BUZZZ_CONFIG_CPU_MIPS_74K */
#endif  /*  CONFIG_MIPS */


#if defined(CONFIG_ARM)
#if defined(BUZZZ_CONFIG_CPU_ARMV7_A9)
/*
 * ARMv7 A9 performance events are arranged to have all 4 performance counters
 * counting a unique event, constituting similar events to form a group.
 * See enum buzzz_pmon_event_t for the grouping of events into 8 groups.
 */

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(group)       BUZZZ_PMON_GROUP_ ## group,
typedef
enum buzzz_pmon_group
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
	BUZZZ_ENUM(MAXIMUM)
} buzzz_pmon_group_t;

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(event)       BUZZZ_PMON_EVENT_ ## event,
typedef /* ARMv7 Cortex A9 Performance counters supported by BUZZZ PMon tool */
enum buzzz_pmon_event
{                                           /* evt: cnter  */
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
	BUZZZ_ENUM(INSTRUCTIONS)                /* 0x68 */
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

	BUZZZ_ENUM(MAXIMUM)

} buzzz_pmon_event_t;

#define BUZZZ_PMON_COUNTERS     (4)
#define BUZZZ_PMON_GROUPS       (BUZZZ_PMON_EVENT_MAXIMUM / BUZZZ_PMON_COUNTERS)

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(group)       BUZZZ_KEVT_GROUP_ ## group,
typedef
enum buzzz_kevt_group
{
	BUZZZ_ENUM(RESET)
	BUZZZ_ENUM(GENERAL)
	BUZZZ_ENUM(ICACHE)
	BUZZZ_ENUM(DCACHE)
	BUZZZ_ENUM(TLB)
	BUZZZ_ENUM(BRANCH)
	BUZZZ_ENUM(MAXIMUM)
} buzzz_kevt_group_t;

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(event)       BUZZZ_KEVT_EVENT_ ## event,
typedef /* ARMv7 Cortex A9 Performance counters supported by BUZZZ KEvt tool */
enum buzzz_kevt_event
{                                           /* evt: cnter  */
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

	BUZZZ_ENUM(MAXIMUM)
} buzzz_kevt_event_t;

#define BUZZZ_KEVT_COUNTERS     (2)
#define BUZZZ_KEVT_GROUPS       (BUZZZ_KEVT_EVENT_MAXIMUM / BUZZZ_KEVT_COUNTERS)

#undef  BUZZZ_ENUM
#define BUZZZ_ENUM(event)       BUZZZ_KEVT_ID_ ## event,
typedef
enum buzzz_kevt_id
{
	BUZZZ_ENUM(IRQ_BAD)         /* 1: irq                                     */
	BUZZZ_ENUM(IRQ_ACK_BAD)     /* 1: irq                                     */
	BUZZZ_ENUM(IRQ_MISROUTED)   /* 1: irq                                     */
	BUZZZ_ENUM(IRQ_RESEND)      /* 1: irq                                     */
	BUZZZ_ENUM(IRQ_CHECK)       /* 1: irq                                     */
	BUZZZ_ENUM(IRQ_ENTRY)       /* 2: irq, handler                            */
	BUZZZ_ENUM(IRQ_EXIT)        /* 2: irq, handler                            */
	BUZZZ_ENUM(SIRQ_ENTRY)      /* 1: action                                  */
	BUZZZ_ENUM(SIRQ_EXIT)       /* 1: action                                  */
	BUZZZ_ENUM(WORKQ_ENTRY)     /* 1: function                                */
	BUZZZ_ENUM(WORKQ_EXIT)      /* 1: function                                */
	BUZZZ_ENUM(SCHEDULE)        /* 4: prev->[pid,prio], next->[pid,prio]      */
	BUZZZ_ENUM(SCHED_TICK)      /* 1: jiffies                                 */
	BUZZZ_ENUM(SCHED_HRTICK)    /* 0:                                         */
	BUZZZ_ENUM(GTIMER_EVENT)    /* 1: event_handler                           */
	BUZZZ_ENUM(GTIMER_NEXT)     /* 1: next                                    */
	BUZZZ_ENUM(BUZZZ_TMR)       /* 0:                                         */

	BUZZZ_ENUM(MAXIMUM)
} buzzz_kevt_id_t;
#endif  /*  BUZZZ_CONFIG_CPU_ARMV7_A9 */
#endif  /*  CONFIG_ARM */

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

typedef char buzzz_fmt_t[BUZZZ_KLOG_FMT_LENGTH];

/*
 * APIs exposed for kernel space invocation
 */
int buzzz_show(void);
int buzzz_start(uint32_t u32);
int buzzz_stop(uint32_t u32);
int buzzz_pause(void);
int buzzz_play(void);
int buzzz_audit(void);
int buzzz_dump(uint32_t items);
int buzzz_kcall(uint32_t args);

int buzzz_config_tool(buzzz_tool_t tool);
int buzzz_config_mode(buzzz_mode_t mode);
int buzzz_config_limit(uint32_t limit);

void buzzz_klog_reg(uint32_t klog_id, char * klog_fmt);

/*
 * Function call tracing kernel APIs
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
 * Performance monitoring kernel APIs
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
 * Kernel Event logging APIs
 */
void buzzz_kevt_log0(uint32_t event);
void buzzz_kevt_log1(uint32_t event, uint32_t u1);
void buzzz_kevt_log2(uint32_t event, uint32_t u1, uint32_t u2);
void buzzz_kevt_log3(uint32_t event, uint32_t u1, uint32_t u2,
                                     uint32_t u3);
void buzzz_kevt_log4(uint32_t event, uint32_t u1, uint32_t u2,
                                     uint32_t u3, uint32_t u4);
void buzzz_kevt_start(void);
void buzzz_kevt_stop(void);
void buzzz_kevt_dump(uint32_t limit);
int  buzzz_kevt_config(uint32_t config_exit);

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
 * Kernel mode embedded assembly APIs
 */

#endif  /* !__ASSEMBLY__ */


#else   /* !CONFIG_BUZZZ */

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

#define buzzz_klog_reg(l, f)                BUZZZ_NULL_STMT

/* Function call tracing kernel APIs */
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

/* Performance monitoring kernel APIs */
#define buzzz_pmon_bgn()                    BUZZZ_NULL_STMT
#define buzzz_pmon_clr()                    BUZZZ_NULL_STMT
#define buzzz_pmon_log(e)                   BUZZZ_NULL_STMT
#define buzzz_pmon_end()                    BUZZZ_NULL_STMT
#define buzzz_pmon_start()                  BUZZZ_NULL_STMT
#define buzzz_pmon_stop()                   BUZZZ_NULL_STMT
#define buzzz_pmon_config(i, s)             BUZZZ_NULL_FUNC

#define buzzz_kevt_log0(e)                  BUZZZ_NULL_STMT
#define buzzz_kevt_log1(e, a1)              BUZZZ_NULL_STMT
#define buzzz_kevt_log2(e, a1, a2)          BUZZZ_NULL_STMT
#define buzzz_kevt_log3(e, a1, a2, a3)      BUZZZ_NULL_STMT
#define buzzz_kevt_log4(e, a1, a2, a3, a4)  BUZZZ_NULL_STMT
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
