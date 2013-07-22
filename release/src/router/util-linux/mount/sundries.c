/*
 * Support functions.  Exported functions are prototyped in sundries.h.
 *
 * added fcntl locking by Kjetil T. (kjetilho@math.uio.no) - aeb, 950927
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <mntent.h>		/* for MNTTYPE_SWAP */

#include "canonicalize.h"

#include "sundries.h"
#include "xmalloc.h"
#include "nls.h"

int mount_quiet;
int verbose;
int nocanonicalize;
char *progname;

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

/* reallocates its first arg - typical use: s = xstrconcat3(s,t,u); */
char *
xstrconcat3 (char *s, const char *t, const char *u) {
     size_t len = 0;

     len = (s ? strlen(s) : 0) + (t ? strlen(t) : 0) + (u ? strlen(u) : 0);

     if (!len)
	     return NULL;
     if (!s) {
	     s = xmalloc(len + 1);
	     *s = '\0';
     }
     else
	     s = xrealloc(s, len + 1);
     if (t)
	     strcat(s, t);
     if (u)
	     strcat(s, u);
     return s;
}

/* frees its first arg - typical use: s = xstrconcat4(s,t,u,v); */
char *
xstrconcat4 (char *s, const char *t, const char *u, const char *v) {
     size_t len = 0;

     len = (s ? strlen(s) : 0) + (t ? strlen(t) : 0) +
		(u ? strlen(u) : 0) + (v ? strlen(v) : 0);

     if (!len)
	     return NULL;
     if (!s) {
	     s = xmalloc(len + 1);
	     *s = '\0';
     }
     else
	     s = xrealloc(s, len + 1);
     if (t)
	     strcat(s, t);
     if (u)
	     strcat(s, u);
     if (v)
	     strcat(s, v);
     return s;


}

/* Call this with SIG_BLOCK to block and SIG_UNBLOCK to unblock.  */
void
block_signals (int how) {
     sigset_t sigs;

     sigfillset (&sigs);
     sigdelset(&sigs, SIGTRAP);
     sigdelset(&sigs, SIGSEGV);
     sigprocmask (how, &sigs, (sigset_t *) 0);
}


/* Non-fatal error.  Print message and return.  */
/* (print the message in a single printf, in an attempt
    to avoid mixing output of several threads) */
void
error (const char *fmt, ...) {
     va_list args;

     if (mount_quiet)
	  return;
     va_start (args, fmt);
     vfprintf (stderr, fmt, args);
     va_end (args);
     fputc('\n', stderr);
}

/* Fatal error.  Print message and exit.  */
void __attribute__ ((noreturn)) die(int err, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(err);
}

/* True if fstypes match.  Null *TYPES means match anything,
   except that swap types always return false. */
/* Accept nonfs,proc,devpts and nonfs,noproc,nodevpts
   with the same meaning. */
int
matching_type (const char *type, const char *types) {
     int no;			/* negated types list */
     int len;
     const char *p;

     if (streq (type, MNTTYPE_SWAP))
	  return 0;
     if (types == NULL)
	  return 1;

     no = 0;
     if (!strncmp(types, "no", 2)) {
	  no = 1;
	  types += 2;
     }

     /* Does type occur in types, separated by commas? */
     len = strlen(type);
     p = types;
     while(1) {
	     if (!strncmp(p, "no", 2) && !strncmp(p+2, type, len) &&
		 (p[len+2] == 0 || p[len+2] == ','))
		     return 0;
	     if (strncmp(p, type, len) == 0 &&
		 (p[len] == 0 || p[len] == ','))
		     return !no;
	     p = strchr(p,',');
	     if (!p)
		     break;
	     p++;
     }
     return no;
}

/* Returns 1 if needle found or noneedle not found in haystack
 * Otherwise returns 0
 */
static int
check_option(const char *haystack, const char *needle) {
     const char *p, *r;
     int len, needle_len, this_len;
     int no;

     no = 0;
     if (!strncmp(needle, "no", 2)) {
	  no = 1;
	  needle += 2;
     }
     needle_len = strlen(needle);
     len = strlen(haystack);

     for (p = haystack; p < haystack+len; p++) {
	  r = strchr(p, ',');
	  if (r) {
	       this_len = r-p;
	  } else {
	       this_len = strlen(p);
	  }
	  if (this_len != needle_len) {
	       p += this_len;
	       continue;
	  }
	  if (strncmp(p, needle, this_len) == 0)
	       return !no; /* foo or nofoo was found */
	  p += this_len;
     }

     return no;  /* foo or nofoo was not found */
}


/* Returns 1 if each of the test_opts options agrees with the entire
 * list of options.
 * Returns 0 if any noopt is found in test_opts and opt is found in options.
 * Returns 0 if any opt is found in test_opts but is not found in options.
 * Unlike fs type matching, nonetdev,user and nonetdev,nouser have
 * DIFFERENT meanings; each option is matched explicitly as specified.
 */
int
matching_opts (const char *options, const char *test_opts) {
     const char *p, *r;
     char *q;
     int len, this_len;

     if (test_opts == NULL)
	  return 1;
     if (options == NULL)
	  options = "";

     len = strlen(test_opts);
     q = alloca(len+1);
     if (q == NULL)
          die (EX_SYSERR, _("not enough memory"));
     
     for (p = test_opts; p < test_opts+len; p++) {
	  r = strchr(p, ',');
	  if (r) {
	       this_len = r-p;
	  } else {
	       this_len = strlen(p);
	  }
	  if (!this_len) continue; /* if two ',' appear in a row */
	  strncpy(q, p, this_len);
	  q[this_len] = '\0';
	  if (!check_option(options, q))
	       return 0; /* any match failure means failure */
	  p += this_len;
     }

     /* no match failures in list means success */
     return 1;
}

int
is_pseudo_fs(const char *type)
{
	if (type == NULL || *type == '/')
		return 0;
	if (streq(type, "none") ||
	    streq(type, "proc") ||
	    streq(type, "tmpfs") ||
	    streq(type, "sysfs") ||
	    streq(type, "usbfs") ||
	    streq(type, "cgroup") ||
	    streq(type, "cpuset") ||
	    streq(type, "rpc_pipefs") ||
	    streq(type, "devpts") ||
	    streq(type, "securityfs") ||
	    streq(type, "debugfs"))
		return 1;
	return 0;
}

/* Make a canonical pathname from PATH.  Returns a freshly malloced string.
   It is up the *caller* to ensure that the PATH is sensible.  i.e.
   canonicalize ("/dev/fd0/.") returns "/dev/fd0" even though ``/dev/fd0/.''
   is not a legal pathname for ``/dev/fd0''.  Anything we cannot parse
   we return unmodified.   */
char *
canonicalize_spec (const char *path)
{
	char *res;

	if (!path)
		return NULL;
	if (nocanonicalize || is_pseudo_fs(path))
		return xstrdup(path);

	res = canonicalize_path(path);
	if (!res)
		die(EX_SYSERR, _("not enough memory"));
	return res;
}

char *canonicalize (const char *path)
{
	char *res;

	if (!path)
		return NULL;
	else if (nocanonicalize)
		return xstrdup(path);

	res = canonicalize_path(path);
	if (!res)
		die(EX_SYSERR, _("not enough memory"));
	return res;
}

