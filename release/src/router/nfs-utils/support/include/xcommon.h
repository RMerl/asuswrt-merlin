/*
 * xcommon.h -- Support function prototypes. Functions are in xcommon.c.
 *
 * 2006-06-06 Amit Gud <agud@redhat.com>
 * - Moved code snippets  from mount/sundries.h of util-linux
 *   and merged code from support/nfs/xmalloc.c by Olaf Kirch <okir@monad.swb.de> here.
 */

#ifndef _XMALLOC_H
#define _MALLOC_H

#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define streq(s, t)	(strcmp ((s), (t)) == 0)

/* Functions in sundries.c that are used in mount.c and umount.c  */ 
char *canonicalize (const char *path);
void nfs_error (const char *fmt, ...);
void *xmalloc (size_t size);
void *xrealloc(void *p, size_t size);
void xfree(void *);
char *xstrdup (const char *s);
char *xstrndup (const char *s, int n);
char *xstrconcat2 (const char *, const char *);
char *xstrconcat3 (const char *, const char *, const char *);
char *xstrconcat4 (const char *, const char *, const char *, const char *);
void die (int errcode, const char *fmt, ...);

extern void die(int err, const char *fmt, ...);
extern void (*at_die)(void);

/* exit status - bits below are ORed */
#define EX_SUCCESS	0	/* no failure occurred */
#define EX_USAGE	1	/* incorrect invocation or permission */
#define EX_SYSERR	2	/* out of memory, cannot fork, ... */
#define EX_SOFTWARE	4	/* internal mount bug or wrong version */
#define EX_USER		8	/* user interrupt */
#define EX_FILEIO      16	/* problems writing, locking, ... mtab/fstab */
#define EX_FAIL	       32	/* mount failure */
#define EX_SOMEOK      64	/* some mount succeeded */
#define EX_BG         256       /* retry in background (internal only) */

#endif /* XMALLOC_H */
