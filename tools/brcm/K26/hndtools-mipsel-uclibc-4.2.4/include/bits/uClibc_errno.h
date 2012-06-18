/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */
#ifndef _BITS_UCLIBC_ERRNO_H
#define _BITS_UCLIBC_ERRNO_H 1

#ifdef IS_IN_rtld
# undef errno
# define errno _dl_errno
extern int _dl_errno; /* attribute_hidden; */
#elif defined __UCLIBC_HAS_THREADS__
# include <tls.h>
# if defined USE___THREAD && USE___THREAD
#  undef errno
#  ifndef NOT_IN_libc
#   define errno __libc_errno
#  else
#   define errno errno
#  endif
extern __thread int errno attribute_tls_model_ie;
# endif /* USE___THREAD */
#endif /* IS_IN_rtld */

#define __set_errno(val) (errno = (val))

#ifndef __ASSEMBLER__
extern int *__errno_location (void) __THROW __attribute__ ((__const__))
# ifdef IS_IN_rtld
	attribute_hidden
# endif
;
# if defined __UCLIBC_HAS_THREADS__
#  include <tls.h>
#  if defined USE___THREAD && USE___THREAD
#  endif
# endif

#endif /* !__ASSEMBLER__ */

#endif
