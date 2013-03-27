/*
 * dstore.c - Linux global storage for /proc-based lsof
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dstore.c,v 1.3 2008/04/15 13:32:26 abe Exp $";
#endif


#include "lsof.h"

int HasNFS = 0;				/* NFS mount point status:
					 *     1 == there is an NFS mount point,
					 *          but its device number is
					 *          unknown
					 *     2 == there is an NFS mount point
					 *          and its device number is
					 *          known
					 */
int OffType = 0;			/* offset type:
					 *     0 == unknown
					 *     1 == lstat's st_size
					 *     2 == from /proc/<PID>/fdinfo */

/*
 * Pff_tab[] - table for printing file flags
 */

struct pff_tab Pff_tab[] = {
	{ (long)O_WRONLY,	FF_WRITE	},
	{ (long)O_RDWR,		FF_RDWR		},
	{ (long)O_CREAT,	FF_CREAT	},
	{ (long)O_EXCL,		FF_EXCL		},
	{ (long)O_NOCTTY,	FF_NOCTTY	},
	{ (long)O_TRUNC,	FF_TRUNC	},
	{ (long)O_APPEND,	FF_APPEND	},
	{ (long)O_NDELAY,	FF_NDELAY	},
	{ (long)O_SYNC,		FF_SYNC		},
	{ (long)O_ASYNC,	FF_ASYNC	},

#if	defined(O_DIRECT)
	{ (long)O_DIRECT,	FF_DIRECT	},
#endif	/* defined(O_DIRECT) */

#if	defined(O_DIRECTORY)
	{ (long)O_DIRECTORY,	FF_DIRECTORY	},
#endif	/* defined(O_DIRECTORY) */

#if	defined(O_NOFOLLOW)
	{ (long)O_NOFOLLOW,	FF_NOFOLNK	},
#endif	/* defined(O_NOFOLLOW) */

#if	defined(O_NOATIME)
	{ (long)O_NOATIME,	FF_NOATM	},
#endif	/* defined(O_NOATIME) */

#if	defined(O_DSYNC)
	{ (long)O_DSYNC,	FF_DSYNC	},
#endif	/* defined(O_DSYNC) */

#if	defined(O_RSYNC)
	{ (long)O_RSYNC,	FF_RSYNC	},
#endif	/* defined(O_RSYNC) */

#if	defined(O_LARGEFILE)
	{ (long)O_LARGEFILE,	FF_LARGEFILE	},
#endif	/* defined(O_LARGEFILE) */
	{ (long)0,		NULL		}
};


/*
 * Pof_tab[] - table for print process open file flags
 */

struct pff_tab Pof_tab[] = {
	{ (long)0,		NULL		}
};
