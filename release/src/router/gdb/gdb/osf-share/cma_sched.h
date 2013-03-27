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
 *	Header file for priority scheduling
 */


#ifndef CMA_SCHED
#define CMA_SCHED

/*
 *  INCLUDE FILES
 */

/*
 * CONSTANTS AND MACROS
 */

/*
 * Scaling factor for integer priority calculations
 */
#define cma__c_prio_scale   8

#if _CMA_VENDOR_ == _CMA__APOLLO
/*
 * FIX-ME: Apollo cc 6.8 blows contant folded "<<" and ">>"
 */
# define cma__scale_up(exp)  ((exp) * 256)
# define cma__scale_dn(exp)  ((exp) / 256)
#else
# define cma__scale_up(exp)  ((exp) << cma__c_prio_scale)
# define cma__scale_dn(exp)  ((exp) >> cma__c_prio_scale)
#endif


/*
 * Min. num. of ticks between self-adjustments for priority adjusting policies.
 */
#define cma__c_prio_interval	10


/*
 * Number of queues in each class of queues
 */
#define cma__c_prio_n_id    1	    /* Very-low-priority class threads */
#define cma__c_prio_n_bg    8	    /* Background class threads */
#define cma__c_prio_n_0	    1	    /* Very low priority throughput quartile */
#define cma__c_prio_n_1	    2	    /* Low priority throughput quartile */
#define cma__c_prio_n_2	    3	    /* Medium priority throughput quartile */
#define cma__c_prio_n_3	    4	    /* High priority throughput quartile */
#define cma__c_prio_n_rt    1	    /* Real Time priority queues */

/*
 * Number of queues to skip (offset) to get to the queues in this section of LA
 */
#define cma__c_prio_o_id 0
#define cma__c_prio_o_bg cma__c_prio_o_id + cma__c_prio_n_id
#define cma__c_prio_o_0  cma__c_prio_o_bg + cma__c_prio_n_bg
#define cma__c_prio_o_1  cma__c_prio_o_0  + cma__c_prio_n_0
#define cma__c_prio_o_2  cma__c_prio_o_1  + cma__c_prio_n_1
#define cma__c_prio_o_3  cma__c_prio_o_2  + cma__c_prio_n_2
#define cma__c_prio_o_rt cma__c_prio_o_3  + cma__c_prio_n_3

/*
 * Ada_low:  These threads are queued in the background queues, thus there
 * must be enough queues to allow one queue for each Ada priority below the
 * Ada default.
 */  
#define cma__c_prio_o_al cma__c_prio_o_bg

/*
 * Total number of ready queues, for declaration purposes
 */
#define cma__c_prio_n_tot  \
	cma__c_prio_n_id + cma__c_prio_n_bg + cma__c_prio_n_rt \
	+ cma__c_prio_n_0 + cma__c_prio_n_1 + cma__c_prio_n_2 + cma__c_prio_n_3

/*
 * Formulae for determining a thread's priority.  Variable priorities (such
 * as foreground and background) are scaled values.
 */
#define cma__sched_priority(tcb)	\
    ((tcb)->sched.class == cma__c_class_fore  ? cma__sched_prio_fore (tcb)  \
    :((tcb)->sched.class == cma__c_class_back ? cma__sched_prio_back (tcb)  \
    :((tcb)->sched.class == cma__c_class_rt   ? cma__sched_prio_rt (tcb)    \
    :((tcb)->sched.class == cma__c_class_idle ? cma__sched_prio_idle (tcb)  \
    :(cma__bugcheck ("cma__sched_priority: unrecognized class"), 0) ))))

#define cma__sched_prio_fore(tcb)	cma__sched_prio_fore_var (tcb)
#define cma__sched_prio_back(tcb)	((tcb)->sched.fixed_prio	\
	? cma__sched_prio_back_fix (tcb) : cma__sched_prio_back_var (tcb) )
#define cma__sched_prio_rt(tcb)		((tcb)->sched.priority)
#define cma__sched_prio_idle(tcb)	((tcb)->sched.priority)

#define cma__sched_prio_back_fix(tcb)	\
	(cma__g_prio_bg_min + (cma__g_prio_bg_max - cma__g_prio_bg_min) \
	* ((tcb)->sched.priority + cma__c_prio_o_al - cma__c_prio_o_bg) \
	/ cma__c_prio_n_bg)

/*
 * FIX-ME: Enable after modeling (if we like it)
 */
#if 1
# define cma__sched_prio_fore_var(tcb)  \
	((cma__g_prio_fg_max + cma__g_prio_fg_min)/2)
# define cma__sched_prio_back_var(tcb)  \
	((cma__g_prio_bg_max + cma__g_prio_bg_min)/2)
#else
# define cma__sched_prio_back_var(tcb)  cma__sched_prio_fore_var (tcb)

# if 1
/*
 * Re-scale, since the division removes the scale factor.
 * Scale and multiply before dividing to avoid loss of precision.
 */
#  define cma__sched_prio_fore_var(tcb)  \
	((cma__g_vp_count * cma__scale_up((tcb)->sched.tot_time)) \
	/ (tcb)->sched.cpu_time)
# else
/*
 * Re-scale, since the division removes the scale factor.
 * Scale and multiply before dividing to avoid loss of precision.
 * Left shift the numerator to multiply by two.
 */
#  define cma__sched_prio_fore_var(tcb)  \
    (((cma__g_vp_count * cma__scale_up((tcb)->sched.tot_time)  \
    * (tcb)->sched.priority * cma__g_init_frac_sum) << 1)  \
    / ((tcb)->sched.cpu_time * (tcb)->sched.priority * cma__g_init_frac_sum  \
	+ (tcb)->sched.tot_time))
# endif
#endif

/*
 * Update weighted-averaged, scaled tick counters
 */
#define cma__sched_update_time(ave, new) \
    (ave) = (ave) - ((cma__scale_dn((ave)) - (new)) << (cma__c_prio_scale - 4))

#define cma__sched_parameterize(tcb, policy) { \
    switch (policy) { \
	case cma_c_sched_fifo : { \
	    (tcb)->sched.rtb =		cma_c_true; \
	    (tcb)->sched.spp =		cma_c_true; \
	    (tcb)->sched.fixed_prio =	cma_c_true; \
	    (tcb)->sched.class =	cma__c_class_rt; \
	    break; \
	    } \
	case cma_c_sched_rr : { \
	    (tcb)->sched.rtb =		cma_c_false; \
	    (tcb)->sched.spp =		cma_c_true; \
	    (tcb)->sched.fixed_prio =	cma_c_true; \
	    (tcb)->sched.class =	cma__c_class_rt; \
	    break; \
	    } \
	case cma_c_sched_throughput : { \
	    (tcb)->sched.rtb =		cma_c_false; \
	    (tcb)->sched.spp =		cma_c_false; \
	    (tcb)->sched.fixed_prio =	cma_c_false; \
	    (tcb)->sched.class =	cma__c_class_fore; \
	    break; \
	    } \
	case cma_c_sched_background : { \
	    (tcb)->sched.rtb =		cma_c_false; \
	    (tcb)->sched.spp =		cma_c_false; \
	    (tcb)->sched.fixed_prio =	cma_c_false; \
	    (tcb)->sched.class =	cma__c_class_back; \
	    break; \
	    } \
	case cma_c_sched_ada_low : { \
	    (tcb)->sched.rtb =		cma_c_false; \
	    (tcb)->sched.spp =		cma_c_true; \
	    (tcb)->sched.fixed_prio =	cma_c_true; \
	    (tcb)->sched.class =	cma__c_class_back; \
	    break; \
	    } \
	case cma_c_sched_idle : { \
	    (tcb)->sched.rtb =		cma_c_false; \
	    (tcb)->sched.spp =		cma_c_false; \
	    (tcb)->sched.fixed_prio =	cma_c_false; \
	    (tcb)->sched.class =	cma__c_class_idle; \
	    break; \
	    } \
	default : { \
	    cma__bugcheck ("cma__sched_parameterize: bad scheduling Policy"); \
	    break; \
	    } \
	} \
    }

/*
 * TYPEDEFS
 */

/*
 * Scheduling classes
 */
typedef enum CMA__T_SCHED_CLASS {
    cma__c_class_rt,
    cma__c_class_fore,
    cma__c_class_back,
    cma__c_class_idle
    } cma__t_sched_class;

/*
 *  GLOBAL DATA
 */

/*
 * Minimuma and maximum prioirities, for foreground and background threads,
 * as of the last time the scheduler ran.  (Scaled once.)
 */
extern cma_t_integer	cma__g_prio_fg_min;
extern cma_t_integer	cma__g_prio_fg_max;
extern cma_t_integer	cma__g_prio_bg_min;
extern cma_t_integer	cma__g_prio_bg_max;

/*
 * The "m" values are the slopes of the four sections of linear approximation.
 *
 * cma__g_prio_m_I = 4*N(I)/cma__g_prio_range	    (Scaled once.)
 */
extern cma_t_integer	cma__g_prio_m_0,
		    	cma__g_prio_m_1,
		    	cma__g_prio_m_2,
		    	cma__g_prio_m_3;

/* 
 * The "b" values are the intercepts of the four sections of linear approx.
 *  (Not scaled.)
 *
 * cma__g_prio_b_I = -N(I)*(I*prio_max + (4-I)*prio_min)/prio_range + prio_o_I
 */
extern cma_t_integer	cma__g_prio_b_0,
		    	cma__g_prio_b_1,
		    	cma__g_prio_b_2,
		    	cma__g_prio_b_3;

/* 
 * The "p" values are the end points of the four sections of linear approx.
 *
 * cma__g_prio_p_I = cma__g_prio_fg_min + (I/4)*cma__g_prio_range
 *
 * [cma__g_prio_p_0 is not defined since it is not used (also, it is the same
 *  as cma__g_prio_fg_min).]	    (Scaled once.)
 */
extern cma_t_integer	cma__g_prio_p_1,
		    	cma__g_prio_p_2,
		    	cma__g_prio_p_3;

/*
 * Points to the next queue for the dispatcher to check for ready threads.
 */
extern cma_t_integer	cma__g_next_ready_queue;

/*
 * Points to the queues of virtual processors (for preempt victim search)
 */
extern cma__t_queue	cma__g_run_vps;
extern cma__t_queue	cma__g_susp_vps;
extern cma_t_integer	cma__g_vp_count;

/*
 * INTERNAL INTERFACES
 */

#endif
