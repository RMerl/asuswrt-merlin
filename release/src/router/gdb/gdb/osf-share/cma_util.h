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
 *	Header file for CMA internal UTIL operations
 */

#ifndef	CMA_UTIL
#define CMA_UTIL

/*
 *  INCLUDE FILES
 */

#include <cma.h>
#include <cma_attr.h>
#include <cma_defs.h>

#if _CMA_OS_ == _CMA__VMS
# include <cma_rms.h>
#endif

#if _CMA_VENDOR_ == _CMA__SUN
# include <sys/time.h>
#else
# include <time.h>
#endif

#if _CMA_OS_ == _CMA__UNIX
# include <stdio.h>
#endif

/*
 * CONSTANTS AND MACROS
 */

#define cma__c_buffer_size  256		    /* Size of output buffer	    */

/*
 * TYPEDEFS
 */

/* 
 * Alternate eol routine
 */
typedef void (*cma__t_eol_routine) (char *);

#if _CMA_OS_ == _CMA__VMS
 typedef struct CMA__T_VMSFILE {
    struct RAB	rab;
    struct FAB	fab;
    } cma__t_vmsfile, 	*cma__t_file;
#elif  ( _CMA_UNIX_TYPE == _CMA__SVR4 )
 typedef int           cma__t_file;
#else
 typedef FILE		*cma__t_file;
#endif

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

extern void cma__abort  (void);

extern cma_t_integer cma__atol  (char *);

extern cma_t_integer cma__atoi  (char *);

extern char * cma__getenv  (char *,char *,int);

extern int cma__gettimespec  (struct timespec *);

extern cma__t_file cma__int_fopen  (char *,char *);

#ifndef NDEBUG
extern void cma__init_trace  (char *_env);
#endif

extern char * cma__memcpy  (char *,char *,cma_t_integer);
	
#ifndef cma__memset
extern char * cma__memset  (char *,cma_t_integer,cma_t_integer);
#endif

extern void cma__putformat  (char *,char *,...);

extern void cma__putstring  (char *,char *);

extern void cma__putint  (char *,cma_t_integer);

extern void cma__putint_5  (char *,cma_t_integer);

extern void cma__putint_10  (char *,cma_t_integer);

extern void cma__puthex  (char *,cma_t_integer);

extern void cma__puthex_8  (char *,cma_t_integer);

extern void cma__puteol  (char *);

extern void cma__set_eol_routine  (cma__t_eol_routine,cma__t_eol_routine *);

extern cma_t_integer cma__strlen  (char *);

extern int cma__strncmp  (char *,char *,cma_t_integer);

extern char *cma__gets (char *,char *);

#endif
