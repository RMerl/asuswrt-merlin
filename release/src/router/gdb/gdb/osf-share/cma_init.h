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
 *	Header file for CMA initialization
 */

#ifndef CMA_INIT
#define CMA_INIT

/*
 *  INCLUDE FILES
 */
#include <dce/cma_host.h>
#include <cma_errors.h>

/*
 * CONSTANTS AND MACROS
 */

#define cma__c_env_maxattr	0
#define cma__c_env_minattr	1
#define cma__c_env_maxcond	2
#define	cma__c_env_mincond	3
#define cma__c_env_maxmutex	4
#define cma__c_env_minmutex	5
#define cma__c_env_maxthread	6
#define cma__c_env_minthread	7
#define cma__c_env_maxcluster	8
#define cma__c_env_mincluster	9
#define cma__c_env_maxvp	10
#define cma__c_env_multiplex	11
#define cma__c_env_trace	12
#define cma__c_env_trace_file	13

#define cma__c_env_count	13


/*
 * cma__int_init
 *
 * Initialize the main body of CMA exactly once.
 *
 * We raise an exception if, for some odd reason, there are already threads
 * in the environment (e.g. kernel threads), and one of them is trying to
 * initialize CMA before the  first thread got all the way through the actual
 * initialization. This code maintains the invariants: "after successfully
 * calling CMA_INIT, you can call any CMA function", and  "CMA is actually
 * initialized at most once".
 */
/*#ifndef _HP_LIBC_R */

#if  defined _HP_LIBC_R  ||(defined(SNI_SVR4) && !defined(CMA_INIT_NEEDED))
# define cma__int_init()
#else
# define cma__int_init() { \
    if (!cma__tac_isset(&cma__g_init_started)) { \
	if (!cma__test_and_set (&cma__g_init_started)) { \
	    cma__init_static (); \
	    cma__test_and_set (&cma__g_init_done); \
	    } \
	else if (!cma__tac_isset (&cma__g_init_done)) { \
	    cma__error (cma_s_inialrpro); \
    }}}
#endif

/*
 * TYPEDEFS
 */

typedef enum CMA__T_ENV_TYPE {
    cma__c_env_type_int,
    cma__c_env_type_file
    } cma__t_env_type;

typedef struct CMA__T_ENV {
    char		*name;		/* Name of environment variable */
    cma__t_env_type	type;		/* Type of variable */
    cma_t_integer	value;		/* Numeric value of the variable */
    } cma__t_env;

/*
 *  GLOBAL DATA
 */

extern cma__t_env		cma__g_env[cma__c_env_count];
extern cma__t_atomic_bit	cma__g_init_started;
extern cma__t_atomic_bit	cma__g_init_done;
extern char			*cma__g_version;

/*
 * INTERNAL INTERFACES
 */

extern void
cma__init_static (void);	/* Initialize static data */

#if _CMA_OS_ != _CMA__VMS
extern void cma__init_atfork (void);
#endif

#endif
