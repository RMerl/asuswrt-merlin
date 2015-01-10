#ifndef UTIL_LINUX_EXITCODES_H
#define UTIL_LINUX_EXITCODES_H

/* Exit codes used by mkfs-type programs */
#define MKFS_OK		0	/* No errors */
#define MKFS_ERROR	8	/* Operational error */
#define MKFS_USAGE	16	/* Usage or syntax error */

/* Exit codes used by fsck-type programs */
#define FSCK_OK			0	/* No errors */
#define FSCK_NONDESTRUCT	1	/* File system errors corrected */
#define FSCK_REBOOT		2	/* System should be rebooted */
#define FSCK_UNCORRECTED	4	/* File system errors left uncorrected */
#define FSCK_ERROR		8	/* Operational error */
#define FSCK_USAGE		16	/* Usage or syntax error */
#define FSCK_LIBRARY		128	/* Shared library error */

/* exit status */
#define MOUNT_EX_SUCCESS	0	/* No errors */
#define MOUNT_EX_USAGE		1	/* incorrect invocation or permission */
#define MOUNT_EX_SYSERR		2	/* out of memory, cannot fork, ... */
#define MOUNT_EX_SOFTWARE	4	/* internal mount bug or wrong version */
#define MOUNT_EX_USER		8	/* user interrupt */
#define MOUNT_EX_FILEIO		16	/* problems writing, locking, ... mtab/fstab */
#define MOUNT_EX_FAIL		32	/* mount failure */
#define MOUNT_EX_SOMEOK		64	/* some mount succeeded */

#endif	/* UTIL_LINUX_EXITCODES_H */
