/*
 * xcommon.c - various functions put together to avoid basic error checking.
 *
 * added fcntl locking by Kjetil T. (kjetilho@math.uio.no) - aeb, 950927
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2006-06-06 Amit Gud <agud@redhat.com>
 * - Moved code snippets here from mount/sundries.c of util-linux
 *   and merged code from support/nfs/xmalloc.c by Olaf Kirch <okir@monad.swb.de> here.
 */

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcommon.h"
#include "nls.h"	/* _() */

void (*at_die)(void ) = NULL;

#if 0
char *
xstrndup (const char *s, int n) {
     char *t;

     if (s == NULL)
	  die (EX_SOFTWARE, _("bug in xstrndup call"));

     t = xmalloc(n+1);
     strncpy(t,s,n);
     t[n] = 0;

     return t;
}

char *
xstrconcat2 (const char *s, const char *t) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     res = xmalloc(strlen(s) + strlen(t) + 1);
     strcpy(res, s);
     strcat(res, t);
     return res;
}

/* frees its first arg - typical use: s = xstrconcat3(s,t,u); */
char *
xstrconcat3 (const char *s, const char *t, const char *u) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     if (!u) u = "";
     res = xmalloc(strlen(s) + strlen(t) + strlen(u) + 1);
     strcpy(res, s);
     strcat(res, t);
     strcat(res, u);
     free((void *) s);
     return res;
}

/* frees its first arg - typical use: s = xstrconcat4(s,t,u,v); */
char *
xstrconcat4 (const char *s, const char *t, const char *u, const char *v) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     if (!u) u = "";
     if (!v) v = "";
     res = xmalloc(strlen(s) + strlen(t) + strlen(u) + strlen(v) + 1);
     strcpy(res, s);
     strcat(res, t);
     strcat(res, u);
     strcat(res, v);
     free((void *) s);
     return res;
}

/* Non-fatal error.  Print message and return.  */
/* (print the message in a single printf, in an attempt
    to avoid mixing output of several threads) */
void
nfs_error (const char *fmt, ...) {
     va_list args;
     char *fmt2;

     fmt2 = xstrconcat2 (fmt, "\n");
     va_start (args, fmt);
     vfprintf (stderr, fmt2, args);
     va_end (args);
     free (fmt2);
}

/* Make a canonical pathname from PATH.  Returns a freshly malloced string.
   It is up the *caller* to ensure that the PATH is sensible.  i.e.
   canonicalize ("/dev/fd0/.") returns "/dev/fd0" even though ``/dev/fd0/.''
   is not a legal pathname for ``/dev/fd0''.  Anything we cannot parse
   we return unmodified.   */
char *canonicalize (const char *path) {
	char canonical[PATH_MAX+2];

	if (path == NULL)
		return NULL;

#if 1
	if (streq(path, "none") ||
	    streq(path, "proc") ||
	    streq(path, "devpts"))
		return xstrdup(path);
#endif
	if (realpath (path, canonical))
		return xstrdup(canonical);

	return xstrdup(path);
}
#endif

/* Fatal error.  Print message and exit.  */
void
die(int err, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	if (at_die)
		(*at_die)();

	exit(err);
}

static void
die_if_null(void *t) {
	if (t == NULL)
		die(EX_SYSERR, _("not enough memory"));
}

void *
xmalloc (size_t size) {
	void *t;

	if (size == 0)
		return NULL;

	t = malloc(size);
	die_if_null(t);

	return t;
}

void *
xrealloc (void *p, size_t size) {
	void *t;

	t = realloc(p, size);
	die_if_null(t);

	return t;
}

void
xfree(void *ptr)
{
	free(ptr);
}

char *
xstrdup (const char *s) {
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup(s);
	die_if_null(t);

	return t;
}
