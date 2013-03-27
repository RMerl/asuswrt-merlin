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
 *	This file defines the internal interface to the core of CMA 
 * 	debugging services. (The client interface to debugging services
 *	is provided by cma_debug_client.h).
 */

#ifndef CMA_DEB_CORE
#define CMA_DEB_CORE

/*
 *  INCLUDE FILES
 */
#include <cma.h>
#include <cma_mutex.h>
#include <cma_queue.h>
#include <cma_tcb_defs.h>
#include <cma_util.h>

/*
 * CONSTANTS AND MACROS
 */


/*
 * TYPEDEFS
 */

/*FIX-ME* Need to use sizes that are platform specific */
typedef long int cma___t_debug_ctx[17];

/*
 * Type defing the format of known object lists
 */
typedef struct CMA__T_KNOWN_OBJECT {
    cma__t_queue	queue;		/* Queue header for known objects */
    cma__t_int_mutex	*mutex;		/* Mutex to control access to queue */
    } cma__t_known_object;


/*
 * Type defining the registration for one debug client (e.g. Ada)
 */
typedef struct CMA__T_DEB_REGISTRY {
    cma_t_address	entry;		/* Client's debug entry point */
    cma_t_key		key;		/* Client's context key */
    cma_t_integer	fac;		/* Client's debug facility number */
    cma_t_boolean 	has_prolog;	/* Client's TCBs have std prolog */
    } cma__t_deb_registry;

#define cma__c_deb_max_clients	10

/* 
 * Type defining the global debugging state for all threads.
 */
typedef struct CMA__T_DEBUG_STATE {
    /* 
     * The following flag is set if changes were made while in the
     * debugger that may make the ready lists inconsistent.  For 
     * example, if a thread priority is changed in the debugger, the
     * thread is not moved between queues.  Making things consistent
     * is deferred to when the dispatcher is next invoked -- which we
     * try to make very soon.
     */
    cma_t_boolean 	is_inconsistency;   /* Ready lists are inconsistent */


    cma_t_boolean 	events_enabled;	    /* Set if _any_ event is enabled */
    cma_t_boolean	flags[cma__c_debevt__dim]; 	
					    /* Which events are enabled */
    cma__t_int_tcb	*next_to_run;	    /* TCB of thread to run next */ 

    cma__t_int_mutex	*mutex;		    /* Mutex for registering clients */
    cma_t_integer	client_count;	    /* Count of debug clients */
    cma__t_deb_registry	clients[cma__c_deb_max_clients+1];
					    /* Array of current debug clients */
    } cma__t_debug_state;


/* 
 * Routine that will symbolize and address and print it.
 */
typedef void	(*cma__t_print_symbol) (cma_t_address);


/*
 *  GLOBAL DATA
 */

/* 
 * Variable holding the global debugging state 
 *
 * (This is primarily written by the debugger interface and read
 * by the thread dispatcher).
 */
extern cma__t_debug_state	cma__g_debug_state;

/* 
 * Known object queues
 */
extern cma__t_known_object	cma__g_known_atts;
extern cma__t_known_object	cma__g_known_cvs;
extern cma__t_known_object	cma__g_known_mutexes;
extern cma__t_known_object	cma__g_known_threads;

/*
 * INTERNAL INTERFACES
 */

/* Get information while in debugger context */
extern void cma__deb_get 
           (cma__t_int_tcb *,cma_t_debug_get,cma_t_address,cma_t_integer,cma_t_integer);

/* Set information while in debugger context */
extern void cma__deb_set (cma__t_int_tcb *,cma_t_debug_set,cma_t_address,cma_t_integer);

extern void cma__init_debug (void);

extern void cma__reinit_debug (cma_t_integer);

extern void cma__deb_anytcb_to_tcb (cma_t_tcb_header *,cma__t_int_tcb **);

extern void cma__deb_fac_to_client (cma_t_integer,cma_t_key *);

extern void cma__deb_get_client_info (cma_t_key,cma_t_address *,cma_t_boolean *);

extern void cma__deb_get_context (cma__t_int_tcb *,cma_t_key,cma_t_address *);

extern cma__t_int_tcb *cma__deb_get_self_tcb (void);

extern void cma__deb_get_time_slice (cma_t_interval *);

extern cma__t_int_tcb *cma__deb_next_tcb 
            (cma__t_int_tcb *,cma_t_integer *,cma_t_integer *,cma_t_boolean *);

extern cma_t_boolean cma__deb_set_alert (cma__t_int_tcb *);

extern void cma__deb_set_next_thread (cma__t_int_tcb *);
	
extern void cma__deb_set_force_dispatch (cma_t_address );

extern void cma__deb_set_time_slice (cma_t_interval);

extern void cma__deb_show_thread 
     (cma__t_int_tcb *,cma_t_boolean,cma_t_boolean,cma___t_debug_ctx,cma__t_eol_routine,
                cma__t_eol_routine,cma__t_print_symbol);

extern void
cma__deb_show_stats (cma__t_int_tcb *,cma_t_boolean,cma_t_boolean,cma__t_eol_routine,cma__t_eol_routine,cma__t_print_symbol);
#endif
