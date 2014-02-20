/*
 * dutil.c - AIX utility functions whose compilation conflicts with the
 *	     general header file tree defined by lsof.h and dlsof.h -- e.g.,
 *	     the conflict between <time.h> and <sys/time.h> for the time(2)
 *	     and localtime(3) functions
 *
 * V. Abell
 * Purdue University
 */


/*
 * Copyright 2008 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


#ifndef lint
static char copyright[] =
"@(#) Copyright 2008 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: util.c,v 1.1 2008/04/01 11:56:53 abe Exp $";
#endif

#if	defined(HAS_STRFTIME)
#include <time.h>
#endif	/* defined(HAS_STRFTIME) */


/*
 * util_strftime() -- utility function to call strftime(3) without header
 *		      file distractions
 */

int
util_strftime(fmtr, fmtl, fmt)
	char *fmtr;			/* format output receiver */
	int fmtl;			/* sizeof(*fmtr) */
	char *fmt;			/* format */
{

#if	defined(HAS_STRFTIME)
	struct tm *lt;
	time_t tm;

	tm = time((time_t *)NULL);
	lt = localtime(&tm);
	return(strftime(fmtr, fmtl, fmt, lt));
#else	/* !defined(HAS_STRFTIME) */
	return(0);
#endif	/* defined(HAS_STRFTIME) */

}
