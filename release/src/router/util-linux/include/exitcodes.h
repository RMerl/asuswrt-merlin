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

#endif	/* UTIL_LINUX_EXITCODES_H */
