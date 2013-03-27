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
 *	This module is the interface between CMA services and 
 * 	the platform-specific error reporting mechanism.
 */

#ifndef CMA_ERRORS
#define CMA_ERRORS

/*
 *  INCLUDE FILES
 */

/*
 * CONSTANTS AND MACROS
 */

/*
 * TYPEDEFS
 */

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

/*
 * The cma__bugcheck function will print information to stderr (or sys$error
 * on VMS), and more extensive information to the file cma_dump.log in the
 * current working directory.
 */
extern void cma__bugcheck  (char *,...);

extern void cma__error  (int);

extern void cma__unimplemented (void);

#endif
