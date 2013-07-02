/* Copyright (C) 1991-1993,1995-1999,2000,2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* March 11, 2001         Manuel Novoa III
 *
 * Modified as appropriate for my new stdio lib.
 */

#ifndef	_PRINTF_H

#define	_PRINTF_H	1
#include <features.h>

__BEGIN_DECLS

#define	__need_FILE
#include <stdio.h>
#define	__need_size_t
#define __need_wchar_t
#include <stddef.h>

/* WARNING -- This is definitely nonportable... but it seems to work
 * with gcc, which is currently the only "supported" compiler.
 * The library code uses bitmasks for space-efficiency (you can't
 * set/test multiple bitfields in one operation).  Unfortunatly, we
 * need to support bitfields since that's what glibc made visible to users.
 * So, we take
 * advantage of how gcc lays out bitfields to create an appropriate
 * mapping.  Inside uclibc (i.e. if _LIBC is defined) we access the
 * bitfields using bitmasks in a single flag variable.
 *
 * WARNING -- This may very well fail if built with -fpack-struct!!!
 *
 * TODO -- Add a validation test.
 * TODO -- Add an option to build in a shim translation function if
 *         the bitfield<->bitmask mapping fails.
 */
#include <endian.h>

struct printf_info {
  int prec;                     /* Precision.  */
  int width;                    /* Width.  */
#ifdef __UCLIBC_HAS_WCHAR__
  wchar_t spec;                 /* Format letter.  */
#else
  int spec;
#endif


#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned int space:1;         /* Space flag.  */
  unsigned int showsign:1;      /* + flag.  */
  unsigned int extra:1;         /* For special use.  */
  unsigned int left:1;          /* - flag.  */
  unsigned int alt:1;           /* # flag.  */
  unsigned int group:1;         /* ' flag.  */
  unsigned int i18n:1;          /* I flag.  */
  unsigned int wide:1;          /* Nonzero for wide character streams.  */
  unsigned int is_char:1;       /* hh flag.  */
  unsigned int is_short:1;      /* h flag.  */
  unsigned int is_long:1;       /* l flag.  */
  unsigned int is_long_double:1;/* L flag.  */
  unsigned int __padding:20;    /* non-gnu: match _flags width of 32 bits */
#elif __BYTE_ORDER == __BIG_ENDIAN
  unsigned int __padding:20;    /* non-gnu: match _flags width of 32 bits */
  unsigned int is_long_double:1;/* L flag.  */
  unsigned int is_long:1;       /* l flag.  */
  unsigned int is_short:1;      /* h flag.  */
  unsigned int is_char:1;       /* hh flag.  */
  unsigned int wide:1;          /* Nonzero for wide character streams.  */
  unsigned int i18n:1;          /* I flag.  */
  unsigned int group:1;         /* ' flag.  */
  unsigned int alt:1;           /* # flag.  */
  unsigned int left:1;          /* - flag.  */
  unsigned int extra:1;         /* For special use.  */
  unsigned int showsign:1;      /* + flag.  */
  unsigned int space:1;         /* Space flag.  */
#else
#error unsupported byte order!
#endif


#ifdef __UCLIBC_HAS_WCHAR__
  wchar_t pad;                  /* Padding character.  */
#else
  int pad;
#endif
};


/* Type of a printf specifier-handler function.
   STREAM is the FILE on which to write output.
   INFO gives information about the format specification.
   ARGS is a vector of pointers to the argument data;
   the number of pointers will be the number returned
   by the associated arginfo function for the same INFO.

   The function should return the number of characters written,
   or -1 for errors.  */

#ifdef __UCLIBC_HAS_GLIBC_CUSTOM_PRINTF__
typedef int (*printf_function) (FILE *__stream,
			     __const struct printf_info *__info,
			     __const void *__const *__args);

/* Type of a printf specifier-arginfo function.
   INFO gives information about the format specification.
   N, ARGTYPES, and return value are as for parse_printf_format.  */

typedef int printf_arginfo_function (__const struct printf_info *__info,
				     size_t __n, int *__argtypes);


/* Register FUNC to be called to format SPEC specifiers; ARGINFO must be
   specified to determine how many arguments a SPEC conversion requires and
   what their types are.  */

extern int register_printf_function (int __spec, printf_function __func,
				     printf_arginfo_function __arginfo);
#endif


/* Parse FMT, and fill in N elements of ARGTYPES with the
   types needed for the conversions FMT specifies.  Returns
   the number of arguments required by FMT.

   The ARGINFO function registered with a user-defined format is passed a
   `struct printf_info' describing the format spec being parsed.  A width
   or precision of INT_MIN means a `*' was used to indicate that the
   width/precision will come from an arg.  The function should fill in the
   array it is passed with the types of the arguments it wants, and return
   the number of arguments it wants.  */

extern size_t parse_printf_format (__const char *__restrict __fmt, size_t __n,
				   int *__restrict __argtypes) __THROW;


/* Codes returned by `parse_printf_format' for basic types.

   These values cover all the standard format specifications.
   Users can add new values after PA_LAST for their own types.  */

/* WARNING -- The above is not entirely true, even for glibc.
 * As far as the library code is concerned, such args are treated
 * as 'your type' pointers if qualified by PA_FLAG_PTR.  If they
 * aren't qualified as pointers, I _think_ glibc just ignores them
 * and carries on.  I think it should be treated as an error. */

enum {                          /* C type: */
  PA_INT,                       /* int */
  PA_CHAR,                      /* int, cast to char */
  PA_WCHAR,                     /* wide char */
  PA_STRING,                    /* const char *, a '\0'-terminated string */
  PA_WSTRING,                   /* const wchar_t *, wide character string */
  PA_POINTER,                   /* void * */
  PA_FLOAT,                     /* float */
  PA_DOUBLE,                    /* double */
  __PA_NOARG,                   /* non-glibc -- signals non-arg width or prec */
  PA_LAST
};

/* Flag bits that can be set in a type returned by `parse_printf_format'.  */
/* WARNING -- These differ in value from what glibc uses. */
#define PA_FLAG_MASK		(0xff00)
#define __PA_FLAG_CHAR		(0x0100) /* non-gnu -- to deal with hh */
#define PA_FLAG_SHORT		(0x0200)
#define PA_FLAG_LONG		(0x0400)
#define PA_FLAG_LONG_LONG	(0x0800)
#define PA_FLAG_LONG_DOUBLE	PA_FLAG_LONG_LONG
#define PA_FLAG_PTR		(0x1000) /* TODO -- make dynamic??? */

#define __PA_INTMASK		(0x0f00) /* non-gnu -- all int flags */

#if 0
/* Function which can be registered as `printf'-handlers.  */

/* Print floating point value using using abbreviations for the orders
   of magnitude used for numbers ('k' for kilo, 'm' for mega etc).  If
   the format specifier is a uppercase character powers of 1000 are
   used.  Otherwise powers of 1024.  */
extern int printf_size (FILE *__restrict __fp,
			__const struct printf_info *__info,
			__const void *__const *__restrict __args) __THROW;

/* This is the appropriate argument information function for `printf_size'.  */
extern int printf_size_info (__const struct printf_info *__restrict
			     __info, size_t __n, int *__restrict __argtypes)
     __THROW;

#endif

__END_DECLS

#endif /* printf.h  */
