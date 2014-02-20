/*
 * cvfs.c -- ckkv() function for lsof library
 */


/*
 * Copyright 1998 Purdue Research Foundation, West Lafayette, Indiana
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


#include "../machine.h"

#if	defined(USE_LIB_CKKV)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1998 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: ckkv.c,v 1.3 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"
#include <sys/utsname.h>


/*
 * ckkv() - check kernel version
 */

void
ckkv(d, er, ev, ea)
	char *d;			/* dialect */
	char *er;			/* expected revision; NULL, no test */
	char *ev;			/* expected version; NULL, no test */
	char *ea;			/* expected architecture; NULL, no
					 * test */
{

# if	defined(HASKERNIDCK) 
	struct utsname u;

	if (Fwarn)
	    return;
/*
 * Read the system information via uname(2).
 */
	if (uname(&u) < 0) {
	    (void) fprintf(stderr, "%s: uname error: %s\n",
		Pn, strerror(errno));
	    Exit(1);
	}
	if (er && strcmp(er, u.release)) {
	    (void) fprintf(stderr,
		"%s: WARNING: compiled for %s release %s; this is %s.\n",
		Pn, d, er, u.release);
	}
	if (ev && strcmp(ev, u.version)) {
	    (void) fprintf(stderr,
		"%s: WARNING: compiled for %s version %s; this is %s.\n",
		Pn, d, ev, u.version);
	}
	if (ea && strcmp(ea, u.machine)) {
	    (void) fprintf(stderr,
		"%s: WARNING: compiled for %s architecture %s; this is %s.\n",
		Pn, d, ea, u.machine);
	}
# endif	/* defined(HASKERNIDCK) */

}
#else	/* !defined(USE_LIB_CKKV) */
char ckkv_d1[] = "d"; char *ckkv_d2 = ckkv_d1;
#endif	/* defined(USE_LIB_CKKV) */
