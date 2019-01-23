/* Copyright (C) 1991-1992, 1995-1998, 2000-2001, 2004-2007, 2009-2017 Free
   Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _GLOB_H
#define _GLOB_H 1

#ifndef __GLOB_GNULIB
# include <sys/cdefs.h>
#endif

/* GCC 2.95 and later have "__restrict"; C99 compilers have
   "restrict", and "configure" may have defined "restrict".
   Other compilers use __restrict, __restrict__, and _Restrict, and
   'configure' might #define 'restrict' to those words, so pick a
   different name.  */
#ifndef _Restrict_
# if 199901L <= __STDC_VERSION__
#  define _Restrict_ restrict
# elif 2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__)
#  define _Restrict_ __restrict
# else
#  define _Restrict_
# endif
#endif

__BEGIN_DECLS

/* We need 'size_t' for the following definitions.  */
#ifndef __size_t
# if defined __GNUC__ && __GNUC__ >= 2
typedef __SIZE_TYPE__ __size_t;
#  ifdef __USE_XOPEN
typedef __SIZE_TYPE__ size_t;
#  endif
# else
#  include <stddef.h>
#  ifndef __size_t
#   define __size_t size_t
#  endif
# endif
#else
/* The GNU CC stddef.h version defines __size_t as empty.  We need a real
   definition.  */
# undef __size_t
# define __size_t size_t
#endif

/* Bits set in the FLAGS argument to 'glob'.  */
#define GLOB_ERR        (1 << 0)/* Return on read errors.  */
#define GLOB_MARK       (1 << 1)/* Append a slash to each name.  */
#define GLOB_NOSORT     (1 << 2)/* Don't sort the names.  */
#define GLOB_DOOFFS     (1 << 3)/* Insert PGLOB->gl_offs NULLs.  */
#define GLOB_NOCHECK    (1 << 4)/* If nothing matches, return the pattern.  */
#define GLOB_APPEND     (1 << 5)/* Append to results of a previous call.  */
#define GLOB_NOESCAPE   (1 << 6)/* Backslashes don't quote metacharacters.  */
#define GLOB_PERIOD     (1 << 7)/* Leading '.' can be matched by metachars.  */

#if !defined __USE_POSIX2 || defined __USE_BSD || defined __USE_GNU
# define GLOB_MAGCHAR    (1 << 8)/* Set in gl_flags if any metachars seen.  */
# define GLOB_ALTDIRFUNC (1 << 9)/* Use gl_opendir et al functions.  */
# define GLOB_BRACE      (1 << 10)/* Expand "{a,b}" to "a" "b".  */
# define GLOB_NOMAGIC    (1 << 11)/* If no magic chars, return the pattern.  */
# define GLOB_TILDE      (1 << 12)/* Expand ~user and ~ to home directories. */
# define GLOB_ONLYDIR    (1 << 13)/* Match only directories.  */
# define GLOB_TILDE_CHECK (1 << 14)/* Like GLOB_TILDE but return an error
                                      if the user name is not available.  */
# define __GLOB_FLAGS   (GLOB_ERR|GLOB_MARK|GLOB_NOSORT|GLOB_DOOFFS| \
                         GLOB_NOESCAPE|GLOB_NOCHECK|GLOB_APPEND|     \
                         GLOB_PERIOD|GLOB_ALTDIRFUNC|GLOB_BRACE|     \
                         GLOB_NOMAGIC|GLOB_TILDE|GLOB_ONLYDIR|GLOB_TILDE_CHECK)
#else
# define __GLOB_FLAGS   (GLOB_ERR|GLOB_MARK|GLOB_NOSORT|GLOB_DOOFFS| \
                         GLOB_NOESCAPE|GLOB_NOCHECK|GLOB_APPEND|     \
                         GLOB_PERIOD)
#endif

/* Error returns from 'glob'.  */
#define GLOB_NOSPACE    1       /* Ran out of memory.  */
#define GLOB_ABORTED    2       /* Read error.  */
#define GLOB_NOMATCH    3       /* No matches found.  */
#define GLOB_NOSYS      4       /* Not implemented.  */
#ifdef __USE_GNU
/* Previous versions of this file defined GLOB_ABEND instead of
   GLOB_ABORTED.  Provide a compatibility definition here.  */
# define GLOB_ABEND GLOB_ABORTED
#endif

/* Structure describing a globbing run.  */
#ifdef __USE_GNU
struct stat;
#endif
typedef struct
  {
    __size_t gl_pathc;          /* Count of paths matched by the pattern.  */
    char **gl_pathv;            /* List of matched pathnames.  */
    __size_t gl_offs;           /* Slots to reserve in 'gl_pathv'.  */
    int gl_flags;               /* Set to FLAGS, maybe | GLOB_MAGCHAR.  */

    /* If the GLOB_ALTDIRFUNC flag is set, the following functions
       are used instead of the normal file access functions.  */
    void (*gl_closedir) (void *);
#ifdef __USE_GNU
    struct dirent *(*gl_readdir) (void *);
#else
    void *(*gl_readdir) (void *);
#endif
    void *(*gl_opendir) (const char *);
#ifdef __USE_GNU
    int (*gl_lstat) (const char *_Restrict_, struct stat *_Restrict_);
    int (*gl_stat) (const char *_Restrict_, struct stat *_Restrict_);
#else
    int (*gl_lstat) (const char *_Restrict_, void *_Restrict_);
    int (*gl_stat) (const char *_Restrict_, void *_Restrict_);
#endif
  } glob_t;

#if defined __USE_LARGEFILE64 && !defined __GLOB_GNULIB
# ifdef __USE_GNU
struct stat64;
# endif
typedef struct
  {
    __size_t gl_pathc;
    char **gl_pathv;
    __size_t gl_offs;
    int gl_flags;

    /* If the GLOB_ALTDIRFUNC flag is set, the following functions
       are used instead of the normal file access functions.  */
    void (*gl_closedir) (void *);
# ifdef __USE_GNU
    struct dirent64 *(*gl_readdir) (void *);
# else
    void *(*gl_readdir) (void *);
# endif
    void *(*gl_opendir) (const char *);
# ifdef __USE_GNU
    int (*gl_lstat) (const char *_Restrict_, struct stat64 *_Restrict_);
    int (*gl_stat) (const char *_Restrict_, struct stat64 *_Restrict_);
# else
    int (*gl_lstat) (const char *_Restrict_, void *_Restrict_);
    int (*gl_stat) (const char *_Restrict_, void *_Restrict_);
# endif
  } glob64_t;
#endif

#if defined __USE_FILE_OFFSET64 && __GNUC__ < 2 && !defined __GLOB_GNULIB
# define glob glob64
# define globfree globfree64
#endif

/* Do glob searching for PATTERN, placing results in PGLOB.
   The bits defined above may be set in FLAGS.
   If a directory cannot be opened or read and ERRFUNC is not nil,
   it is called with the pathname that caused the error, and the
   'errno' value from the failing call; if it returns non-zero
   'glob' returns GLOB_ABEND; if it returns zero, the error is ignored.
   If memory cannot be allocated for PGLOB, GLOB_NOSPACE is returned.
   Otherwise, 'glob' returns zero.  */
#if !defined __USE_FILE_OFFSET64 || __GNUC__ < 2 || defined __GLOB_GNULIB
extern int glob (const char *_Restrict_ __pattern, int __flags,
                 int (*__errfunc) (const char *, int),
                 glob_t *_Restrict_ __pglob) __THROW _GL_ARG_NONNULL ((1, 4));

/* Free storage allocated in PGLOB by a previous 'glob' call.  */
extern void globfree (glob_t *__pglob) __THROW _GL_ARG_NONNULL ((1));
#else
extern int __REDIRECT_NTH (glob, (const char *_Restrict_ __pattern,
                                  int __flags,
                                  int (*__errfunc) (const char *, int),
                                  glob_t *_Restrict_ __pglob), glob64);

extern void __REDIRECT_NTH (globfree, (glob_t *__pglob), globfree64);
#endif

#if defined __USE_LARGEFILE64 && !defined __GLOB_GNULIB
extern int glob64 (const char *_Restrict_ __pattern, int __flags,
                   int (*__errfunc) (const char *, int),
                   glob64_t *_Restrict_ __pglob)
     __THROW _GL_ARG_NONNULL ((1, 4));

extern void globfree64 (glob64_t *__pglob) __THROW _GL_ARG_NONNULL ((1));
#endif


#ifdef __USE_GNU
/* Return nonzero if PATTERN contains any metacharacters.
   Metacharacters can be quoted with backslashes if QUOTE is nonzero.

   This function is not part of the interface specified by POSIX.2
   but several programs want to use it.  */
extern int glob_pattern_p (const char *__pattern, int __quote)
     __THROW _GL_ARG_NONNULL ((1));
#endif

__END_DECLS

#endif /* glob.h  */
