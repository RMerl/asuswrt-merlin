/* vi: set sw=4 ts=4: */
/*
 * fsck.h
 */

#define FSCK_ATTR(x) __attribute__(x)

#define EXIT_OK          0
#define EXIT_NONDESTRUCT 1
#define EXIT_DESTRUCT    2
#define EXIT_UNCORRECTED 4
#define EXIT_ERROR       8
#define EXIT_USAGE       16
#define FSCK_CANCELED    32     /* Aborted with a signal or ^C */

extern char *e2fs_set_sbin_path(void);
