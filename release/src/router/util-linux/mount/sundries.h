/*
 * sundries.h
 * Support function prototypes.  Functions are in sundries.c.
 */
#ifndef SUNDRIES_H
#define SUNDRIES_H

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>

/* global mount, umount, and losetup variables */
extern int mount_quiet;
extern int verbose;
extern int nocanonicalize;
extern char *progname;

#define streq(s, t)	(strcmp ((s), (t)) == 0)

void block_signals (int how);

void error (const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 1, 2)));
void die(int err, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));

int matching_type (const char *type, const char *types);
int matching_opts (const char *options, const char *test_opts);
void *xmalloc (size_t size);
char *xstrdup (const char *s);
char *xstrndup (const char *s, int n);
char *xstrconcat3 (char *, const char *, const char *);
char *xstrconcat4 (char *, const char *, const char *, const char *);

int is_pseudo_fs(const char *type);

char *canonicalize (const char *path);
char *canonicalize_spec (const char *path);

/* exit status - bits below are ORed */
#define EX_USAGE	1	/* incorrect invocation or permission */
#define EX_SYSERR	2	/* out of memory, cannot fork, ... */
#define EX_SOFTWARE	4	/* internal mount bug or wrong version */
#define EX_USER		8	/* user interrupt */
#define EX_FILEIO      16	/* problems writing, locking, ... mtab/fstab */
#define EX_FAIL	       32	/* mount failure */
#define EX_SOMEOK      64	/* some mount succeeded */

#endif /* SUNDRIES_H */

