/* 
   Unix SMB/CIFS implementation.
   replacement routines for broken systems
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jelmer Vernooij 2005-2008
   Copyright (C) Matthieu Patou  2010

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"

#include "system/filesys.h"
#include "system/time.h"
#include "system/passwd.h"
#include "system/syslog.h"
#include "system/locale.h"
#include "system/wait.h"

#ifdef _WIN32
#define mkdir(d,m) _mkdir(d)
#endif

void replace_dummy(void);
void replace_dummy(void) {}

#ifndef HAVE_FTRUNCATE
 /*******************************************************************
ftruncate for operating systems that don't have it
********************************************************************/
int rep_ftruncate(int f, off_t l)
{
#ifdef HAVE_CHSIZE
      return chsize(f,l);
#elif defined(F_FREESP)
      struct  flock   fl;

      fl.l_whence = 0;
      fl.l_len = 0;
      fl.l_start = l;
      fl.l_type = F_WRLCK;
      return fcntl(f, F_FREESP, &fl);
#else
#error "you must have a ftruncate function"
#endif
}
#endif /* HAVE_FTRUNCATE */


#ifndef HAVE_STRLCPY
/* like strncpy but does not 0 fill the buffer and always null 
   terminates. bufsize is the size of the destination buffer */
size_t rep_strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (bufsize <= 0) return 0;
	if (len >= bufsize) len = bufsize-1;
	memcpy(d, s, len);
	d[len] = 0;
	return ret;
}
#endif

#ifndef HAVE_STRLCAT
/* like strncat but does not 0 fill the buffer and always null 
   terminates. bufsize is the length of the buffer, which should
   be one more than the maximum resulting string length */
size_t rep_strlcat(char *d, const char *s, size_t bufsize)
{
	size_t len1 = strlen(d);
	size_t len2 = strlen(s);
	size_t ret = len1 + len2;

	if (len1+len2 >= bufsize) {
		if (bufsize < (len1+1)) {
			return ret;
		}
		len2 = bufsize - (len1+1);
	}
	if (len2 > 0) {
		memcpy(d+len1, s, len2);
		d[len1+len2] = 0;
	}
	return ret;
}
#endif

#ifndef HAVE_MKTIME
/*******************************************************************
a mktime() replacement for those who don't have it - contributed by 
C.A. Lademann <cal@zls.com>
Corrections by richard.kettlewell@kewill.com
********************************************************************/

#define  MINUTE  60
#define  HOUR    60*MINUTE
#define  DAY             24*HOUR
#define  YEAR    365*DAY
time_t rep_mktime(struct tm *t)
{
  struct tm       *u;
  time_t  epoch = 0;
  int n;
  int             mon [] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  y, m, i;

  if(t->tm_year < 70)
    return((time_t)-1);

  n = t->tm_year + 1900 - 1;
  epoch = (t->tm_year - 70) * YEAR + 
    ((n / 4 - n / 100 + n / 400) - (1969 / 4 - 1969 / 100 + 1969 / 400)) * DAY;

  y = t->tm_year + 1900;
  m = 0;

  for(i = 0; i < t->tm_mon; i++) {
    epoch += mon [m] * DAY;
    if(m == 1 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))
      epoch += DAY;
    
    if(++m > 11) {
      m = 0;
      y++;
    }
  }

  epoch += (t->tm_mday - 1) * DAY;
  epoch += t->tm_hour * HOUR + t->tm_min * MINUTE + t->tm_sec;
  
  if((u = localtime(&epoch)) != NULL) {
    t->tm_sec = u->tm_sec;
    t->tm_min = u->tm_min;
    t->tm_hour = u->tm_hour;
    t->tm_mday = u->tm_mday;
    t->tm_mon = u->tm_mon;
    t->tm_year = u->tm_year;
    t->tm_wday = u->tm_wday;
    t->tm_yday = u->tm_yday;
    t->tm_isdst = u->tm_isdst;
  }

  return(epoch);
}
#endif /* !HAVE_MKTIME */


#ifndef HAVE_INITGROUPS
/****************************************************************************
 some systems don't have an initgroups call 
****************************************************************************/
int rep_initgroups(char *name, gid_t id)
{
#ifndef HAVE_SETGROUPS
	/* yikes! no SETGROUPS or INITGROUPS? how can this work? */
	errno = ENOSYS;
	return -1;
#else /* HAVE_SETGROUPS */

#include <grp.h>

	gid_t *grouplst = NULL;
	int max_gr = NGROUPS_MAX;
	int ret;
	int    i,j;
	struct group *g;
	char   *gr;
	
	if((grouplst = malloc(sizeof(gid_t) * max_gr)) == NULL) {
		errno = ENOMEM;
		return -1;
	}

	grouplst[0] = id;
	i = 1;
	while (i < max_gr && ((g = (struct group *)getgrent()) != (struct group *)NULL)) {
		if (g->gr_gid == id)
			continue;
		j = 0;
		gr = g->gr_mem[0];
		while (gr && (*gr != (char)NULL)) {
			if (strcmp(name,gr) == 0) {
				grouplst[i] = g->gr_gid;
				i++;
				gr = (char *)NULL;
				break;
			}
			gr = g->gr_mem[++j];
		}
	}
	endgrent();
	ret = setgroups(i, grouplst);
	free(grouplst);
	return ret;
#endif /* HAVE_SETGROUPS */
}
#endif /* HAVE_INITGROUPS */


#if (defined(SecureWare) && defined(SCO))
/* This is needed due to needing the nap() function but we don't want
   to include the Xenix libraries since that will break other things...
   BTW: system call # 0x0c28 is the same as calling nap() */
long nap(long milliseconds) {
	 return syscall(0x0c28, milliseconds);
 }
#endif


#ifndef HAVE_MEMMOVE
/*******************************************************************
safely copies memory, ensuring no overlap problems.
this is only used if the machine does not have its own memmove().
this is not the fastest algorithm in town, but it will do for our
needs.
********************************************************************/
void *rep_memmove(void *dest,const void *src,int size)
{
	unsigned long d,s;
	int i;
	if (dest==src || !size) return(dest);

	d = (unsigned long)dest;
	s = (unsigned long)src;

	if ((d >= (s+size)) || (s >= (d+size))) {
		/* no overlap */
		memcpy(dest,src,size);
		return(dest);
	}

	if (d < s) {
		/* we can forward copy */
		if (s-d >= sizeof(int) && 
		    !(s%sizeof(int)) && 
		    !(d%sizeof(int)) && 
		    !(size%sizeof(int))) {
			/* do it all as words */
			int *idest = (int *)dest;
			int *isrc = (int *)src;
			size /= sizeof(int);
			for (i=0;i<size;i++) idest[i] = isrc[i];
		} else {
			/* simplest */
			char *cdest = (char *)dest;
			char *csrc = (char *)src;
			for (i=0;i<size;i++) cdest[i] = csrc[i];
		}
	} else {
		/* must backward copy */
		if (d-s >= sizeof(int) && 
		    !(s%sizeof(int)) && 
		    !(d%sizeof(int)) && 
		    !(size%sizeof(int))) {
			/* do it all as words */
			int *idest = (int *)dest;
			int *isrc = (int *)src;
			size /= sizeof(int);
			for (i=size-1;i>=0;i--) idest[i] = isrc[i];
		} else {
			/* simplest */
			char *cdest = (char *)dest;
			char *csrc = (char *)src;
			for (i=size-1;i>=0;i--) cdest[i] = csrc[i];
		}      
	}
	return(dest);
}
#endif /* HAVE_MEMMOVE */

#ifndef HAVE_STRDUP
/****************************************************************************
duplicate a string
****************************************************************************/
char *rep_strdup(const char *s)
{
	size_t len;
	char *ret;

	if (!s) return(NULL);

	len = strlen(s)+1;
	ret = (char *)malloc(len);
	if (!ret) return(NULL);
	memcpy(ret,s,len);
	return(ret);
}
#endif /* HAVE_STRDUP */

#ifndef HAVE_SETLINEBUF
void rep_setlinebuf(FILE *stream)
{
	setvbuf(stream, (char *)NULL, _IOLBF, 0);
}
#endif /* HAVE_SETLINEBUF */

#ifndef HAVE_VSYSLOG
#ifdef HAVE_SYSLOG
void rep_vsyslog (int facility_priority, const char *format, va_list arglist)
{
	char *msg = NULL;
	vasprintf(&msg, format, arglist);
	if (!msg)
		return;
	syslog(facility_priority, "%s", msg);
	free(msg);
}
#endif /* HAVE_SYSLOG */
#endif /* HAVE_VSYSLOG */

#ifndef HAVE_STRNLEN
/**
 Some platforms don't have strnlen
**/
 size_t rep_strnlen(const char *s, size_t max)
{
        size_t len;
  
        for (len = 0; len < max; len++) {
                if (s[len] == '\0') {
                        break;
                }
        }
        return len;  
}
#endif
  
#ifndef HAVE_STRNDUP
/**
 Some platforms don't have strndup.
**/
char *rep_strndup(const char *s, size_t n)
{
	char *ret;
	
	n = strnlen(s, n);
	ret = malloc(n+1);
	if (!ret)
		return NULL;
	memcpy(ret, s, n);
	ret[n] = 0;

	return ret;
}
#endif

#if !defined(HAVE_WAITPID) && defined(HAVE_WAIT4)
int rep_waitpid(pid_t pid,int *status,int options)
{
  return wait4(pid, status, options, NULL);
}
#endif

#ifndef HAVE_SETEUID
int rep_seteuid(uid_t euid)
{
#ifdef HAVE_SETRESUID
	return setresuid(-1, euid, -1);
#else
	errno = ENOSYS;
	return -1;
#endif
}
#endif

#ifndef HAVE_SETEGID
int rep_setegid(gid_t egid)
{
#ifdef HAVE_SETRESGID
	return setresgid(-1, egid, -1);
#else
	errno = ENOSYS;
	return -1;
#endif
}
#endif

/*******************************************************************
os/2 also doesn't have chroot
********************************************************************/
#ifndef HAVE_CHROOT
int rep_chroot(const char *dname)
{
	errno = ENOSYS;
	return -1;
}
#endif

/*****************************************************************
 Possibly replace mkstemp if it is broken.
*****************************************************************/  

#ifndef HAVE_SECURE_MKSTEMP
int rep_mkstemp(char *template)
{
	/* have a reasonable go at emulating it. Hope that
	   the system mktemp() isn't completely hopeless */
	char *p = mktemp(template);
	if (!p)
		return -1;
	return open(p, O_CREAT|O_EXCL|O_RDWR, 0600);
}
#endif

#ifndef HAVE_MKDTEMP
char *rep_mkdtemp(char *template)
{
	char *dname;
	
	if ((dname = mktemp(template))) {
		if (mkdir(dname, 0700) >= 0) {
			return dname;
		}
	}

	return NULL;
}
#endif

/*****************************************************************
 Watch out: this is not thread safe.
*****************************************************************/

#ifndef HAVE_PREAD
ssize_t rep_pread(int __fd, void *__buf, size_t __nbytes, off_t __offset)
{
	if (lseek(__fd, __offset, SEEK_SET) != __offset) {
		return -1;
	}
	return read(__fd, __buf, __nbytes);
}
#endif

/*****************************************************************
 Watch out: this is not thread safe.
*****************************************************************/

#ifndef HAVE_PWRITE
ssize_t rep_pwrite(int __fd, const void *__buf, size_t __nbytes, off_t __offset)
{
	if (lseek(__fd, __offset, SEEK_SET) != __offset) {
		return -1;
	}
	return write(__fd, __buf, __nbytes);
}
#endif

#ifndef HAVE_STRCASESTR
char *rep_strcasestr(const char *haystack, const char *needle)
{
	const char *s;
	size_t nlen = strlen(needle);
	for (s=haystack;*s;s++) {
		if (toupper(*needle) == toupper(*s) &&
		    strncasecmp(s, needle, nlen) == 0) {
			return (char *)((uintptr_t)s);
		}
	}
	return NULL;
}
#endif

#ifndef HAVE_STRTOK_R
/* based on GLIBC version, copyright Free Software Foundation */
char *rep_strtok_r(char *s, const char *delim, char **save_ptr)
{
	char *token;

	if (s == NULL) s = *save_ptr;

	s += strspn(s, delim);
	if (*s == '\0') {
		*save_ptr = s;
		return NULL;
	}

	token = s;
	s = strpbrk(token, delim);
	if (s == NULL) {
		*save_ptr = token + strlen(token);
	} else {
		*s = '\0';
		*save_ptr = s + 1;
	}

	return token;
}
#endif


#ifndef HAVE_STRTOLL
long long int rep_strtoll(const char *str, char **endptr, int base)
{
#ifdef HAVE_STRTOQ
	return strtoq(str, endptr, base);
#elif defined(HAVE___STRTOLL) 
	return __strtoll(str, endptr, base);
#elif SIZEOF_LONG == SIZEOF_LONG_LONG
	return (long long int) strtol(str, endptr, base);
#else
# error "You need a strtoll function"
#endif
}
#else
#ifdef HAVE_BSD_STRTOLL
#ifdef HAVE_STRTOQ
long long int rep_strtoll(const char *str, char **endptr, int base)
{
	long long int nb = strtoq(str, endptr, base);
	/* In linux EINVAL is only returned if base is not ok */
	if (errno == EINVAL) {
		if (base == 0 || (base >1 && base <37)) {
			/* Base was ok so it's because we were not
			 * able to make the convertion.
			 * Let's reset errno.
			 */
			errno = 0;
		}
	}
	return nb;
}
#else
#error "You need the strtoq function"
#endif /* HAVE_STRTOQ */
#endif /* HAVE_BSD_STRTOLL */
#endif /* HAVE_STRTOLL */


#ifndef HAVE_STRTOULL
unsigned long long int rep_strtoull(const char *str, char **endptr, int base)
{
#ifdef HAVE_STRTOUQ
	return strtouq(str, endptr, base);
#elif defined(HAVE___STRTOULL) 
	return __strtoull(str, endptr, base);
#elif SIZEOF_LONG == SIZEOF_LONG_LONG
	return (unsigned long long int) strtoul(str, endptr, base);
#else
# error "You need a strtoull function"
#endif
}
#else
#ifdef HAVE_BSD_STRTOLL
#ifdef HAVE_STRTOUQ
unsigned long long int rep_strtoull(const char *str, char **endptr, int base)
{
	unsigned long long int nb = strtouq(str, endptr, base);
	/* In linux EINVAL is only returned if base is not ok */
	if (errno == EINVAL) {
		if (base == 0 || (base >1 && base <37)) {
			/* Base was ok so it's because we were not
			 * able to make the convertion.
			 * Let's reset errno.
			 */
			errno = 0;
		}
	}
	return nb;
}
#else
#error "You need the strtouq function"
#endif /* HAVE_STRTOUQ */
#endif /* HAVE_BSD_STRTOLL */
#endif /* HAVE_STRTOULL */

#ifndef HAVE_SETENV
int rep_setenv(const char *name, const char *value, int overwrite) 
{
	char *p;
	size_t l1, l2;
	int ret;

	if (!overwrite && getenv(name)) {
		return 0;
	}

	l1 = strlen(name);
	l2 = strlen(value);

	p = malloc(l1+l2+2);
	if (p == NULL) {
		return -1;
	}
	memcpy(p, name, l1);
	p[l1] = '=';
	memcpy(p+l1+1, value, l2);
	p[l1+l2+1] = 0;

	ret = putenv(p);
	if (ret != 0) {
		free(p);
	}

	return ret;
}
#endif

#ifndef HAVE_UNSETENV
int rep_unsetenv(const char *name)
{
	extern char **environ;
	size_t len = strlen(name);
	size_t i, count;

	if (environ == NULL || getenv(name) == NULL) {
		return 0;
	}

	for (i=0;environ[i];i++) /* noop */ ;

	count=i;
	
	for (i=0;i<count;) {
		if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
			/* note: we do _not_ free the old variable here. It is unsafe to 
			   do so, as the pointer may not have come from malloc */
			memmove(&environ[i], &environ[i+1], (count-i)*sizeof(char *));
			count--;
		} else {
			i++;
		}
	}

	return 0;
}
#endif

#ifndef HAVE_UTIME
int rep_utime(const char *filename, const struct utimbuf *buf)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_UTIMES
int rep_utimes(const char *filename, const struct timeval tv[2])
{
	struct utimbuf u;

	u.actime = tv[0].tv_sec;
	if (tv[0].tv_usec > 500000) {
		u.actime += 1;
	}

	u.modtime = tv[1].tv_sec;
	if (tv[1].tv_usec > 500000) {
		u.modtime += 1;
	}

	return utime(filename, &u);
}
#endif

#ifndef HAVE_DUP2
int rep_dup2(int oldfd, int newfd) 
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_CHOWN
/**
chown isn't used much but OS/2 doesn't have it
**/
int rep_chown(const char *fname, uid_t uid, gid_t gid)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_LINK
int rep_link(const char *oldpath, const char *newpath)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_READLINK
int rep_readlink(const char *path, char *buf, size_t bufsiz)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_SYMLINK
int rep_symlink(const char *oldpath, const char *newpath)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_LCHOWN
int rep_lchown(const char *fname,uid_t uid,gid_t gid)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_REALPATH
char *rep_realpath(const char *path, char *resolved_path)
{
	/* As realpath is not a system call we can't return ENOSYS. */
	errno = EINVAL;
	return NULL;
}
#endif


#ifndef HAVE_MEMMEM
void *rep_memmem(const void *haystack, size_t haystacklen,
		 const void *needle, size_t needlelen)
{
	if (needlelen == 0) {
		return discard_const(haystack);
	}
	while (haystacklen >= needlelen) {
		char *p = (char *)memchr(haystack, *(const char *)needle,
					 haystacklen-(needlelen-1));
		if (!p) return NULL;
		if (memcmp(p, needle, needlelen) == 0) {
			return p;
		}
		haystack = p+1;
		haystacklen -= (p - (const char *)haystack) + 1;
	}
	return NULL;
}
#endif

#if !defined(HAVE_VDPRINTF) || !defined(HAVE_C99_VSNPRINTF)
int rep_vdprintf(int fd, const char *format, va_list ap)
{
	char *s = NULL;
	int ret;

	vasprintf(&s, format, ap);
	if (s == NULL) {
		errno = ENOMEM;
		return -1;
	}
	ret = write(fd, s, strlen(s));
	free(s);
	return ret;
}
#endif

#if !defined(HAVE_DPRINTF) || !defined(HAVE_C99_VSNPRINTF)
int rep_dprintf(int fd, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vdprintf(fd, format, ap);
	va_end(ap);

	return ret;
}
#endif

#ifndef HAVE_GET_CURRENT_DIR_NAME
char *rep_get_current_dir_name(void)
{
	char buf[PATH_MAX+1];
	char *p;
	p = getcwd(buf, sizeof(buf));
	if (p == NULL) {
		return NULL;
	}
	return strdup(p);
}
#endif

#if !defined(HAVE_STRERROR_R) || !defined(STRERROR_R_PROTO_COMPATIBLE)
int rep_strerror_r(int errnum, char *buf, size_t buflen)
{
	char *s = strerror(errnum);
	if (strlen(s)+1 > buflen) {
		errno = ERANGE;
		return -1;
	}
	strncpy(buf, s, buflen);
	return 0;
}
#endif

#ifndef HAVE_CLOCK_GETTIME
int rep_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	struct timeval tval;
	switch (clk_id) {
		case 0: /* CLOCK_REALTIME :*/
#ifdef HAVE_GETTIMEOFDAY_TZ
			gettimeofday(&tval,NULL);
#else
			gettimeofday(&tval);
#endif
			tp->tv_sec = tval.tv_sec;
			tp->tv_nsec = tval.tv_usec * 1000;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	return 0;
}
#endif
