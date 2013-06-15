/* 
   Unix SMB/CIFS implementation.

   macros to go along with the lib/replace/ portability layer code

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2006

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _LIBREPLACE_REPLACE_H
#define _LIBREPLACE_REPLACE_H

#ifndef NO_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "win32_replace.h"
#endif

#ifdef __COMPAR_FN_T
#define QSORT_CAST (__compar_fn_t)
#endif

#ifndef QSORT_CAST
#define QSORT_CAST (int (*)(const void *, const void *))
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
/* force off HAVE_INTTYPES_H so that roken doesn't try to include both,
   which causes a warning storm on irix */
#undef HAVE_INTTYPES_H
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

/**
  this is a warning hack. The idea is to use this everywhere that we
  get the "discarding const" warning from gcc. That doesn't actually
  fix the problem of course, but it means that when we do get to
  cleaning them up we can do it by searching the code for
  discard_const.

  It also means that other error types aren't as swamped by the noise
  of hundreds of const warnings, so we are more likely to notice when
  we get new errors.

  Please only add more uses of this macro when you find it
  _really_ hard to fix const warnings. Our aim is to eventually use
  this function in only a very few places.

  Also, please call this via the discard_const_p() macro interface, as that
  makes the return type safe.
*/
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))

/** Type-safe version of discard_const */
#define discard_const_p(type, ptr) ((type *)discard_const(ptr))

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(i) sys_errlist[i]
#endif

#ifndef HAVE_ERRNO_DECL
extern int errno;
#endif

#ifndef HAVE_STRDUP
#define strdup rep_strdup
char *rep_strdup(const char *s);
#endif

#ifndef HAVE_MEMMOVE
#define memmove rep_memmove
void *rep_memmove(void *dest,const void *src,int size);
#endif

#if !defined(HAVE_MKTIME) || !defined(HAVE_TIMEGM)
#include "system/time.h"
#endif

#ifndef HAVE_MKTIME
#define mktime rep_mktime
time_t rep_mktime(struct tm *t);
#endif

#ifndef HAVE_TIMEGM
struct tm;
#define timegm rep_timegm
time_t rep_timegm(struct tm *tm);
#endif

#ifndef HAVE_STRLCPY
#define strlcpy rep_strlcpy
size_t rep_strlcpy(char *d, const char *s, size_t bufsize);
#endif

#ifndef HAVE_STRLCAT
#define strlcat rep_strlcat
size_t rep_strlcat(char *d, const char *s, size_t bufsize);
#endif

#if (defined(BROKEN_STRNDUP) || !defined(HAVE_STRNDUP))
#undef HAVE_STRNDUP
#define strndup rep_strndup
char *rep_strndup(const char *s, size_t n);
#endif

#if (defined(BROKEN_STRNLEN) || !defined(HAVE_STRNLEN))
#undef HAVE_STRNLEN
#define strnlen rep_strnlen
size_t rep_strnlen(const char *s, size_t n);
#endif

#ifndef HAVE_SETENV
#define setenv rep_setenv
int rep_setenv(const char *name, const char *value, int overwrite);
#else
#ifndef HAVE_SETENV_DECL
int setenv(const char *name, const char *value, int overwrite);
#endif
#endif

#ifndef HAVE_UNSETENV
#define unsetenv rep_unsetenv
int rep_unsetenv(const char *name); 
#endif

#ifndef HAVE_SETEUID
#define seteuid rep_seteuid
int rep_seteuid(uid_t);
#endif

#ifndef HAVE_SETEGID
#define setegid rep_setegid
int rep_setegid(gid_t);
#endif

#ifndef HAVE_SETLINEBUF
#define setlinebuf rep_setlinebuf
void rep_setlinebuf(FILE *);
#endif

#ifndef HAVE_STRCASESTR
#define strcasestr rep_strcasestr
char *rep_strcasestr(const char *haystack, const char *needle);
#endif

#ifndef HAVE_STRTOK_R
#define strtok_r rep_strtok_r 
char *rep_strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#ifndef HAVE_STRTOLL
#define strtoll rep_strtoll
long long int rep_strtoll(const char *str, char **endptr, int base);
#endif

#ifndef HAVE_STRTOULL
#define strtoull rep_strtoull
unsigned long long int rep_strtoull(const char *str, char **endptr, int base);
#endif

#ifndef HAVE_FTRUNCATE
#define ftruncate rep_ftruncate
int rep_ftruncate(int,off_t);
#endif

#ifndef HAVE_INITGROUPS
#define initgroups rep_initgroups
int rep_initgroups(char *name, gid_t id);
#endif

#if !defined(HAVE_BZERO) && defined(HAVE_MEMSET)
#define bzero(a,b) memset((a),'\0',(b))
#endif

#ifndef HAVE_DLERROR
#define dlerror rep_dlerror
char *rep_dlerror(void);
#endif

#ifndef HAVE_DLOPEN
#define dlopen rep_dlopen
void *rep_dlopen(const char *name, int flags);
#endif

#ifndef HAVE_DLSYM
#define dlsym rep_dlsym
void *rep_dlsym(void *handle, const char *symbol);
#endif

#ifndef HAVE_DLCLOSE
#define dlclose rep_dlclose
int rep_dlclose(void *handle);
#endif

#ifndef HAVE_SOCKETPAIR
#define socketpair rep_socketpair
int rep_socketpair(int d, int type, int protocol, int sv[2]);
#endif

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1 )
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

#ifndef HAVE_VASPRINTF
#define vasprintf rep_vasprintf
int rep_vasprintf(char **ptr, const char *format, va_list ap) PRINTF_ATTRIBUTE(2,0);
#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
#define snprintf rep_snprintf
int rep_snprintf(char *,size_t ,const char *, ...) PRINTF_ATTRIBUTE(3,4);
#endif

#if !defined(HAVE_VSNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
#define vsnprintf rep_vsnprintf
int rep_vsnprintf(char *,size_t ,const char *, va_list ap) PRINTF_ATTRIBUTE(3,0);
#endif

#ifndef HAVE_ASPRINTF
#define asprintf rep_asprintf
int rep_asprintf(char **,const char *, ...) PRINTF_ATTRIBUTE(2,3);
#endif

#ifndef HAVE_VSYSLOG
#ifdef HAVE_SYSLOG
#define vsyslog rep_vsyslog
void rep_vsyslog (int facility_priority, const char *format, va_list arglist) PRINTF_ATTRIBUTE(2,0);
#endif
#endif

/* we used to use these fns, but now we have good replacements
   for snprintf and vsnprintf */
#define slprintf snprintf


#ifndef HAVE_VA_COPY
#undef va_copy
#ifdef HAVE___VA_COPY
#define va_copy(dest, src) __va_copy(dest, src)
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

#ifndef HAVE_VOLATILE
#define volatile
#endif

#ifndef HAVE_COMPARISON_FN_T
typedef int (*comparison_fn_t)(const void *, const void *);
#endif

#ifdef REPLACE_STRPTIME
#define strptime rep_strptime
struct tm;
char *rep_strptime(const char *buf, const char *format, struct tm *tm);
#endif

/* Load header file for dynamic linking stuff */
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

#ifndef HAVE_SECURE_MKSTEMP
#define mkstemp(path) rep_mkstemp(path)
int rep_mkstemp(char *temp);
#endif

#ifndef HAVE_MKDTEMP
#define mkdtemp rep_mkdtemp
char *rep_mkdtemp(char *template);
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* The extra casts work around common compiler bugs.  */
#define _TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
/* The outer cast is needed to work around a bug in Cray C 5.0.3.0.
   It is necessary at least when t == time_t.  */
#define _TYPE_MINIMUM(t) ((t) (_TYPE_SIGNED (t) \
  			      ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) : (t) 0))
#define _TYPE_MAXIMUM(t) ((t) (~ (t) 0 - _TYPE_MINIMUM (t)))

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#ifndef UINT32_MAX
#define UINT32_MAX (4294967295U)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)-1)
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef INT32_MAX
#define INT32_MAX _TYPE_MAXIMUM(int32_t)
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#if !defined(HAVE_BOOL)
#ifdef HAVE__Bool
#define bool _Bool
#else
typedef int bool;
#endif
#endif

/*
 * to prevent <rpcsvc/yp_prot.h> from doing a redefine of 'bool'
 *
 * IRIX, HPUX, MacOS 10 and Solaris need BOOL_DEFINED
 * Tru64 needs _BOOL_EXISTS
 * AIX needs _BOOL,_TRUE,_FALSE
 */
#ifndef BOOL_DEFINED
#define BOOL_DEFINED
#endif
#ifndef _BOOL_EXISTS
#define _BOOL_EXISTS
#endif
#ifndef _BOOL
#define _BOOL
#endif

#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined
#endif

#ifndef true
#define true (1)
#endif
#ifndef false
#define false (0)
#endif

#ifndef _TRUE
#define _TRUE true
#endif
#ifndef _FALSE
#define _FALSE false
#endif

#ifndef HAVE_FUNCTION_MACRO
#ifdef HAVE_func_MACRO
#define __FUNCTION__ __func__
#else
#define __FUNCTION__ ("")
#endif
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef __STRING
#define __STRING(x)    #x
#endif

#ifndef _STRINGSTRING
#define __STRINGSTRING(x) __STRING(x)
#endif

#ifndef __LINESTR__
#define __LINESTR__ __STRINGSTRING(__LINE__)
#endif

#ifndef __location__
#define __location__ __FILE__ ":" __LINESTR__
#endif

/** 
 * zero a structure 
 */
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))

/** 
 * zero a structure given a pointer to the structure 
 */
#define ZERO_STRUCTP(x) do { if ((x) != NULL) memset((char *)(x), 0, sizeof(*(x))); } while(0)

/** 
 * zero a structure given a pointer to the structure - no zero check 
 */
#define ZERO_STRUCTPN(x) memset((char *)(x), 0, sizeof(*(x)))

/* zero an array - note that sizeof(array) must work - ie. it must not be a
   pointer */
#define ZERO_ARRAY(x) memset((char *)(x), 0, sizeof(x))

/**
 * work out how many elements there are in a static array 
 */
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

/** 
 * pointer difference macro 
 */
#define PTR_DIFF(p1,p2) ((ptrdiff_t)(((const char *)(p1)) - (const char *)(p2)))

#if MMAP_BLACKLIST
#undef HAVE_MMAP
#endif

#endif /* _LIBREPLACE_REPLACE_H */
