/* 
 * (c) Copyright 1990-1996 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990-1996 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990-1996 DIGITAL EQUIPMENT CORPORATION
 * (c) Copyright 1991, 1992 Siemens-Nixdorf Information Systems
 * To anyone who acknowledges that this file is provided "AS IS" without
 * any express or implied warranty: permission to use, copy, modify, and
 * distribute this file for any purpose is hereby granted without fee,
 * provided that the above copyright notices and this notice appears in
 * all source code copies, and that none of the names listed above be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  None of these organizations
 * makes any representations about the suitability of this software for
 * any purpose.
 */
/*
 *	TCB-related type definitions.
 */

#ifndef CMA_TCB_DEFS
#define CMA_TCB_DEFS

/*
 *  INCLUDE FILES
 */
# if !_CMA_THREAD_SYNC_IO_
#  include <cma_thread_io.h>
# endif
#include <cma.h>
#include <cma_debug_client.h>
#include <cma_attr.h>
#include <cma_defs.h>
#include <cma_handle.h>
#include <cma_queue.h>
#if _CMA_OS_ == _CMA__UNIX
# if defined(SNI_DCOSX)
#   include <sys/ucontext.h>
# endif
# include <signal.h>
#endif
#include <cma_sched.h>

/*
 * CONSTANTS AND MACROS
 */

#if _CMA_PLATFORM_ == _CMA__IBMR2_UNIX
# define cma__c_ibmr2_ctx_stack_size	2048
# define cma__c_ibmr2_ctx_stack_top	(cma__c_ibmr2_ctx_stack_size - 1)
#endif

/*
 * TYPEDEFS
 */

#ifndef __STDC__
# if _CMA_HARDWARE_ != _CMA__HPPA
struct CMA__T_SEMAPHORE;
# endif
struct CMA__T_INT_CV;
struct CMA__T_INT_MUTEX;
struct CMA__T_INT_TCB;
#endif

typedef cma_t_address		*cma__t_context_list;

typedef struct CMA__T_TCB_PAD {
    /* 
     * Adjust to align the tcb prolog at byte 32.
     * 12 bytes are required as object header is currently 
     * 20 bytes long.
     */
    cma_t_integer	pad1;		/* pad bytes */
    cma_t_integer	pad2;		/* pad bytes */
    cma_t_integer	pad3;		/* pad bytes */
    } cma__t_tcb_pad;

#if (_CMA_OS_ == _CMA__UNIX) && !_CMA_THREAD_SYNC_IO_
typedef struct CMA__T_TCB_SELECT {
    cma__t_queue	queue;
#if (_CMA_UNIX_TYPE !=  _CMA__SVR4)
	cma__t_file_mask    *rfds;
	cma__t_file_mask    *wfds;
	cma__t_file_mask    *efds;
#else
	cma__t_poll_info 	poll_info;
#endif /* (_CMA_UNIX_TYPE !=  _CMA__SVR4) */
    cma_t_integer	nfound;
    } cma__t_tcb_select;
#endif

typedef struct CMA__T_TCB_TIME {
    cma__t_queue	queue;		/* must be first entry! */
    cma_t_integer	mode;
    struct CMA__T_SEMAPHORE *semaphore;	/* used for timed semaphores */
    cma_t_date_time	wakeup_time;
    cma_t_integer	quanta_remaining;
    } cma__t_tcb_time;

typedef enum CMA__T_DEBEVT {
	cma__c_debevt_activating = 1,	/* First transition to running */
	cma__c_debevt_running = 2,	/* Any transition to running */
	cma__c_debevt_preempting = 3,	/* Preemted (replaced) another thread */
	cma__c_debevt_blocking = 4,	/* Any transition to blocked */
	cma__c_debevt_terminating = 5,	/* Final state transition */
	cma__c_debevt_term_alert = 6,	/* Terminated due to alert/cancel */
	cma__c_debevt_term_exc = 7,	/* Terminated due to exception */
	cma__c_debevt_exc_handled = 8	/* Exception is being handled */
	} cma__t_debevt;

#define cma__c_debevt__first ((cma_t_integer)cma__c_debevt_activating)
#define cma__c_debevt__last  ((cma_t_integer)cma__c_debevt_exc_handled)
#define cma__c_debevt__dim (cma__c_debevt__last + 1)

/* 
 * Type defining thread substate, which is used by the debugger.
 * If the state is blocked, substate indicates WHY the thread is blocked.
 */
typedef enum CMA__T_SUBSTATE {
    cma__c_substa_normal = 0,
    cma__c_substa_mutex = 1,
    cma__c_substa_cv = 2,
    cma__c_substa_timed_cv = 3,
    cma__c_substa_term_alt = 4,
    cma__c_substa_term_exc = 5,
    cma__c_substa_delay =6,
    cma__c_substa_not_yet_run = 7
    } cma__t_substate;
#define cma__c_substa__first ((cma_t_integer)cma__c_substa_normal)
#define cma__c_substa__last  ((cma_t_integer)cma__c_substa_not_yet_run)
#define cma__c_substa__dim (cma__c_substa__last + 1)


/*
 * Per-thread state for the debugger
 */
typedef struct CMA__T_TCB_DEBUG {
    cma_t_boolean	on_hold;	/* Thread was put on hold by debugger */
    cma_t_boolean	activated;	/* Activation event was reported */
    cma_t_boolean	did_preempt;	/* Thread preempted prior one */
    cma_t_address	start_pc;	/* Start routine address */
    cma_t_address	object_addr;	/* Addr of thread object */
    cma__t_substate	substate;	/* Reason blocked, terminated, etc.*/
    cma_t_boolean	notify_debugger;/* Notify debugger thread is running */
    cma_t_address	SPARE2;		/* SPARE */
    cma_t_address	SPARE3;		/* SPARE */
    struct CMA__T_INT_TCB	
			*preempted_tcb;	/* TCB of thread that got preempted */
    cma_t_boolean	flags[cma__c_debevt__dim]; 	
					/* Events enabled for this thread */
    } cma__t_tcb_debug;

typedef struct CMA__T_TCB_SCHED {
    cma_t_integer	adj_time;	/* Abs. time in ticks of last prio adj */
    cma_t_integer	tot_time;	/* Weighted ave in ticks (scaled) */
    cma_t_integer	time_stamp;	/* Abs. time in ticks of last update */
    cma_t_integer	cpu_time;	/* Weighted average in ticks */
    cma_t_integer	cpu_ticks;	/* # of ticks while comp. (scaled) */
    cma_t_integer	q_num;		/* Number of last ready queue on */
    cma_t_priority	priority;	/* Thread priority */
    cma_t_sched_policy  policy;         /* Scheduling policy of thread */
    cma_t_boolean	rtb;		/* "Run 'Till Block" scheduling */
    cma_t_boolean	spp;		/* "Strict Priority Preemption" sched */
    cma_t_boolean	fixed_prio;	/* Fixed priority */
    cma__t_sched_class	class;		/* Scheduling class */
    struct CMA__T_VP	*processor;	/* Current processor (if running) */
    } cma__t_tcb_sched;

typedef struct CMA__T_INT_ALERT {
    cma_t_boolean	pending : 1;	/* alert_pending bit */
    cma_t_boolean	g_enable : 1;	/* general delivery state */
    cma_t_boolean	a_enable : 1;	/* asynchronous delivery state */
    cma_t_integer	spare : 29;	/* Pad to longword */
    cma_t_natural	count;		/* Alert scope nesting count */
    } cma__t_int_alert;

typedef enum CMA__T_STATE {
    cma__c_state_running = 0,		/* For consistency with initial TCB */
    cma__c_state_ready	= 1,
    cma__c_state_blocked = 2,
    cma__c_state_terminated = 3
    } cma__t_state;
#define cma__c_state__first ((cma_t_integer)cma__c_state_running)
#define cma__c_state__last  ((cma_t_integer)cma__c_state_terminated)
#define cma__c_state__dim (cma__c_state__last + 1)

typedef enum CMA__T_THKIND {
    cma__c_thkind_initial = 0,		/* Initial thread */
    cma__c_thkind_normal = 1,		/* Normal thread */
    cma__c_thkind_null	= 2		/* A null thread */
    } cma__t_thkind;
#define cma__c_thkind__first ((cma_t_integer)cma__c_thkind_initial)
#define cma__c_thkind__last  ((cma_t_integer)cma__c_thkind_null)
#define cma__c_thkind__dim (cma__c_thkind__last + 1)

typedef enum CMA__T_SYSCALL_STATE {
	cma__c_syscall_ok = 1,                      /* syscall was not interrupted */
	cma__c_syscall_intintrpt = 1,       /* syscall was interrupted by VTALRM */
	cma__c_syscall_extintrpt = 2        /* syscall was interrupted by external signal */
	} cma__t_syscall_state;


typedef struct CMA__T_INT_TCB {
    /* 
     * Fixed part of TCB.
     *   Modifications to the following three fields must be coordinated.  
     *   The object header must always be first, and the prolog must always 
     *   remain at the same offset (32) for all time. Thus the object header
     *   must never grow beyond a maximum of 32 bytes.
     */
    cma__t_object	header;		/* Common object header */
    cma__t_tcb_pad	pad1;		/* Pad required to align prolog */
    cma_t_tcb_prolog	prolog;		/* Standard prolog for tasks, threads */

    /* 
     * Floating part of TCB (fields here on are free to be moved and resized).
     */
    cma__t_queue	threads;	/* List of all known threads */
    cma__t_int_attr	*attributes;	/* Backpointer to attr obj */
    cma__t_state	state;		/* Current state of thread */
    cma__t_thkind	kind;		/* Which kind of thread */
    struct CMA__T_INT_MUTEX 
			*mutex;		/* Mutex to control TCB access */
    struct CMA__T_INT_CV 
			*term_cv;	/* CV for join */
    struct CMA__T_INT_MUTEX 
			*tswait_mutex;	/* Mutex for thread-synchronous waits */
    struct CMA__T_INT_CV 
			*tswait_cv;	/* CV for thread-synchronous waits */
    cma_t_start_routine	start_code;	/* Address of start routine */
    cma_t_address	start_arg;	/* Argument to pass to start_code */
    cma__t_queue	stack;		/* Queue header for stack descr. */
    cma_t_natural	context_count;	/* Size of context array */
    cma__t_context_list	contexts;	/* Context value array pointer */
    cma_t_exit_status	exit_status;	/* Exit status of thread */
    cma_t_address	return_value;	/* Thread's return value */
    cma__t_async_ctx	async_ctx;	/* Asynchronous context switch info */
    cma__t_static_ctx	static_ctx;	/* Static context switch information */
    cma_t_integer	event_status;	/* Status of semaphore operation */
    cma__t_tcb_time	timer;		/* Time info for dispatcher */
    cma__t_tcb_sched	sched;		/* Scheduler info */
    cma__t_tcb_debug	debug;		/* Debugger info */
#if _CMA_OS_ == _CMA__UNIX
# if !_CMA_THREAD_SYNC_IO_
    cma__t_tcb_select	select;		/* Select info for timed selects */
# endif
    struct sigaction	sigaction_data[NSIG];
#endif
    cma_t_natural       syscall_state; /* set if one of the cma wrapped syscalls was interrupted. */
    cma_t_boolean	detached;	/* Set if already detached */
    cma_t_boolean	terminated;	/* Set if terminated */
    cma_t_integer	joiners;	/* Count of joiners, for zombie frees */
    cma__t_int_alert	alert;		/* Current alert state info */
    struct CMA__T_INT_CV 
			*wait_cv;	/* CV thread is currently waiting on */
    struct CMA__T_INT_MUTEX 
			*wait_mutex;	/* Mutex thread is waiting on */
    struct EXC_CONTEXT_T
			*exc_stack;	/* Top of exception stack */
#if _CMA_PLATFORM_ == _CMA__IBMR2_UNIX
    char		ctx_stack[cma__c_ibmr2_ctx_stack_size];
#endif
    cma_t_integer	thd_errno;	/* Per-thread errno value */
#if _CMA_OS_ == _CMA__VMS
    cma_t_integer	thd_vmserrno;	/* Per-thread VMS errno value */
#endif
    } cma__t_int_tcb;

#endif
