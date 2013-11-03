/*
 * Copyright (c) 1996 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * NOTE: SunOS 4 and ultrix are pretty much the only reason why there
 * are checks for EINTR everywhere. 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"

#include <signal.h>

#if defined(ultrix) || defined(_IBMR2) || defined(NEED_GETUSERSHELL)
extern char *getusershell (void);
#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)
# include <stdio.h>
# include <stdarg.h>
# ifndef HAVE_SNPRINTF
int snprintf (char *str,size_t count,const char *fmt,...);
# endif

# ifndef HAVE_VSNPRINTF
int vsnprintf(char *str, size_t count, const char *fmt, va_list args);
# endif
#endif

/* OpenBSD */
#if defined(__OpenBSD__) && !defined(ENOTSUP)
#define ENOTSUP EOPNOTSUPP
#endif

#if !defined(HAVE_PSELECT) || defined(__OpenBSD__)
extern int pselect(int, fd_set * restrict, fd_set * restrict,
                   fd_set * restrict, const struct timespec * restrict,
                   const sigset_t * restrict);
#endif

#ifndef HAVE_FLOCK
extern int flock (int, int);
#endif

#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t n);
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy (char *, const char *, size_t);
#endif
 
#ifndef HAVE_STRLCAT
extern size_t strlcat (char *, const char *, size_t);
#endif

#endif
