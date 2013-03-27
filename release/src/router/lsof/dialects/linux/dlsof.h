/*
 * dlsof.h - Linux header file for /proc-based lsof
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


/*
 * $Id: dlsof.h,v 1.18 2008/10/21 16:17:21 abe Exp $
 */


#if	!defined(LINUX_LSOF_H)
#define	LINUX_LSOF_H	1

#include <dirent.h>
#define	DIRTYPE	dirent			/* for arg.c's enter_dir() */
#define	__USE_GNU			/* to get all O_* symbols in fcntl.h */
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <netinet/in.h>

#include <netinet/tcp.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
 
#if	defined(HASSELINUX)
#include <selinux/selinux.h>
#endif	/* defined(HASSELINUX) */

#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>


/*
 * This definition is needed for the common function prototype  definitions
 * in "proto.h", but isn't used in /proc-based lsof.
 */

typedef	unsigned long	KA_T;


/*
 * Local definitions
 */

#define	COMP_P		const void
#define DEVINCR		1024	/* device table malloc() increment */
#define	FSNAMEL		4
#define MALLOC_P	void
#define FREE_P		MALLOC_P
#define MALLOC_S	size_t
#define	MAXSYSCMDL	15	/* max system command name length
				 *   This value should be obtained from a
				 * header file #define, but no consistent one
				 * exists.  Some versions of the Linux kernel
				 * have a hard-coded "char comm[16]" command
				 * name member of the task structured
				 * definition in <linux/sched.h>, while others
				 * have a "char comm[TASK_COMM_LEN]" member
				 * with TASK_COMM_LEN #define'd to be 16.
				 *   Hence, a universal, local definition of
				 * 16 is #define'd here. */
#define	PROCFS		"/proc"
#define QSORT_P		void
#define	READLEN_T	size_t

/*
 * Definitions that indicate what values are present in a stat(2) or lstat(2)
 * buffer.
 */

#define	SB_DEV		0x01		/* st_dev */
#define	SB_INO		0x02		/* st_ino */
#define	SB_MODE		0x04		/* st_mode */
#define	SB_NLINK	0x08		/* st_nlink */
#define	SB_RDEV		0x10		/* st_rdev */
#define	SB_SIZE		0x20		/* st_size */
#define	SB_ALL		(SB_DEV | SB_INO | SB_MODE | SB_NLINK | SB_RDEV | \
			 SB_SIZE)	/* all values */

#define STRNCPY_L	size_t
#define	STRNML		32

# if	defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS==64
#define	SZOFFTYPE	unsigned long long
					/* size and offset internal storage
					 * type */
#define	SZOFFPSPEC	"ll"		/* SZOFFTYPE print specification
					 * modifier */
# endif	/* defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS==64 */

#define	XDR_PMAPLIST	(xdrproc_t)xdr_pmaplist
#define	XDR_VOID	(xdrproc_t)xdr_void


/*
 * Global storage definitions (including their structure definitions)
 */

struct mounts {
        char *dir;              	/* directory (mounted on) */
	char *fsname;           	/* file system
					 * (symbolic links unresolved) */
	char *fsnmres;           	/* file system
					 * (symbolic links resolved) */
        dev_t dev;              	/* directory st_dev */
	dev_t rdev;			/* directory st_rdev */
	INODETYPE inode;		/* directory st_ino */
	mode_t mode;			/* directory st_mode */
	int ds;				/* directory status -- i.e., SB_*
					 * values */
	mode_t fs_mode;			/* file system st_mode */
	int ty;				/* node type -- e.g., N_REGLR, N_NFS */
        struct mounts *next;    	/* forward link */
};

struct sfile {
	char *aname;			/* argument file name */
	char *name;			/* file name (after readlink()) */
	char *devnm;			/* device name (optional) */
	dev_t dev;			/* device */
	dev_t rdev;			/* raw device */
	mode_t mode;			/* S_IFMT mode bits from stat() */
	int type;			/* file type: 0 = file system
				 	 *	      1 = regular file */
	INODETYPE i;			/* inode number */
	int f;				/* file found flag */
	struct sfile *next;		/* forward link */
};

extern int HasNFS;
extern int OffType;

#endif	/* LINUX_LSOF_H	*/
