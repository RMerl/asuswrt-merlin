/* Copyright (C) 1991,92,93,95,96,97,98,99,2001,02
   	Free Software Foundation, Inc.
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

/*
 *	ISO C99 Standard 7.4: Character handling	<ctype.h>
 */

#ifndef	_CTYPE_H
#define	_CTYPE_H	1

#include <features.h>
#include <bits/types.h>

#ifdef __UCLIBC_HAS_CTYPE_TABLES__

__BEGIN_DECLS

#ifndef _ISbit
/* These are all the characteristics of characters.
   If there get to be more than 16 distinct characteristics,
   __ctype_mask_t will need to be adjusted. */

# define _ISbit(bit)	(1 << (bit))

enum
{
  _ISupper = _ISbit (0),	/* UPPERCASE.  */
  _ISlower = _ISbit (1),	/* lowercase.  */
  _ISalpha = _ISbit (2),	/* Alphabetic.  */
  _ISdigit = _ISbit (3),	/* Numeric.  */
  _ISxdigit = _ISbit (4),	/* Hexadecimal numeric.  */
  _ISspace = _ISbit (5),	/* Whitespace.  */
  _ISprint = _ISbit (6),	/* Printing.  */
  _ISgraph = _ISbit (7),	/* Graphical.  */
  _ISblank = _ISbit (8),	/* Blank (usually SPC and TAB).  */
  _IScntrl = _ISbit (9),	/* Control character.  */
  _ISpunct = _ISbit (10),	/* Punctuation.  */
  _ISalnum = _ISbit (11)	/* Alphanumeric.  */
};
#else
#error _ISbit already defined!
#endif /* ! _ISbit  */

#include <bits/uClibc_touplow.h>

#ifdef __UCLIBC_HAS_CTYPE_SIGNED__
# define __UCLIBC_CTYPE_IN_TO_DOMAIN(c) (((unsigned int)((c) + 128)) < 384)

#else  /* __UCLIBC_HAS_CTYPE_SIGNED__ */
# define __UCLIBC_CTYPE_IN_TO_DOMAIN(c) (((unsigned int)(c)) < 256)

#endif /* __UCLIBC_HAS_CTYPE_SIGNED__ */

/* In the thread-specific locale model (see `uselocale' in <locale.h>)
   we cannot use global variables for these as was done in the past.
   Instead, the following accessor functions return the address of
   each variable, which is local to the current thread if multithreaded.

   These point into arrays of 384, so they can be indexed by any `unsigned
   char' value [0,255]; by EOF (-1); or by any `signed char' value
   [-128,-1).  ISO C requires that the ctype functions work for `unsigned
   char' values and for EOF; we also support negative `signed char' values
   for broken old programs.  The case conversion arrays are of `int's
   rather than `unsigned char's because tolower (EOF) must be EOF, which
   doesn't fit into an `unsigned char'.  But today more important is that
   the arrays are also used for multi-byte character sets.  */

/* uClibc differences:
 *
 * When __UCLIBC_HAS_CTYPE_SIGNED is defined,
 *
 *    The upper and lower mapping arrays are type int16_t, so that
 *    they may store all char values plus EOF.  The glibc reasoning
 *    given above for these being type int is questionable, as the
 *    ctype mapping functions map from the set of (unsigned) char
 *    and EOF back into the set.  They have no awareness of multi-byte
 *    or wide characters.
 *
 * Otherwise,
 *
 *    The ctype array is defined for -1..255.
 *    The upper and lower mapping arrays are defined for 0..255.
 *    The upper and lower mapping arrays are type unsigned char.
 */

/* Pointers to the default C-locale data. */
extern const __ctype_mask_t *__C_ctype_b;
extern const __ctype_touplow_t *__C_ctype_toupper;
extern const __ctype_touplow_t *__C_ctype_tolower;

#ifdef __UCLIBC_HAS_XLOCALE__

extern __const __ctype_mask_t **__ctype_b_loc (void)
     __attribute__ ((__const));
extern __const __ctype_touplow_t **__ctype_tolower_loc (void)
     __attribute__ ((__const));
extern __const __ctype_touplow_t **__ctype_toupper_loc (void)
     __attribute__ ((__const));

#define __UCLIBC_CTYPE_B	(*__ctype_b_loc())
#define __UCLIBC_CTYPE_TOLOWER	(*__ctype_tolower_loc())
#define __UCLIBC_CTYPE_TOUPPER	(*__ctype_toupper_loc())

#else  /* __UCLIBC_HAS_XLOCALE__ */

/* Pointers to the current global locale data in use. */
extern const __ctype_mask_t *__ctype_b;
extern const __ctype_touplow_t *__ctype_toupper;
extern const __ctype_touplow_t *__ctype_tolower;

#define __UCLIBC_CTYPE_B	(__ctype_b)
#define __UCLIBC_CTYPE_TOLOWER	(__ctype_tolower)
#define __UCLIBC_CTYPE_TOUPPER	(__ctype_toupper)

#endif /* __UCLIBC_HAS_XLOCALE__ */

#define __isctype(c, type) \
  ((__UCLIBC_CTYPE_B)[(int) (c)] & (__ctype_mask_t) type)

#define	__isascii(c)	(((c) & ~0x7f) == 0)	/* If C is a 7 bit value.  */
#define	__toascii(c)	((c) & 0x7f)		/* Mask off high bits.  */

#if defined _LIBC && (defined IS_IN_libc || defined NOT_IN_libc)
/* isdigit() is really locale-invariant, so provide some small fast macros.
 * These are uClibc-specific. */
#define __isdigit_char(C)    (((unsigned char)((C) - '0')) <= 9)
#define __isdigit_int(C)     (((unsigned int)((C) - '0')) <= 9)
#endif

#define	__exctype(name)	extern int name (int) __THROW

__BEGIN_NAMESPACE_STD

/* The following names are all functions:
     int isCHARACTERISTIC(int c);
   which return nonzero iff C has CHARACTERISTIC.
   For the meaning of the characteristic names, see the `enum' above.  */
__exctype (isalnum);
__exctype (isalpha);
__exctype (iscntrl);
__exctype (isdigit);
__exctype (islower);
__exctype (isgraph);
__exctype (isprint);
__exctype (ispunct);
__exctype (isspace);
__exctype (isupper);
__exctype (isxdigit);


/* Return the lowercase version of C.  */
extern int tolower (int __c) __THROW;

/* Return the uppercase version of C.  */
extern int toupper (int __c) __THROW;

__END_NAMESPACE_STD


/* ISO C99 introduced one new function.  */
#ifdef	__USE_ISOC99
__BEGIN_NAMESPACE_C99

__exctype (isblank);

__END_NAMESPACE_C99
#endif

#ifdef __USE_GNU
/* Test C for a set of character classes according to MASK.  */
extern int isctype (int __c, int __mask) __THROW;
#endif

#if defined __USE_SVID || defined __USE_MISC || defined __USE_XOPEN

/* Return nonzero iff C is in the ASCII set
   (i.e., is no more than 7 bits wide).  */
extern int isascii (int __c) __THROW;

/* Return the part of C that is in the ASCII set
   (i.e., the low-order 7 bits of C).  */
extern int toascii (int __c) __THROW;

/* These are the same as `toupper' and `tolower' except that they do not
   check the argument for being in the range of a `char'.  */
__exctype (_toupper);
__exctype (_tolower);
#endif /* Use SVID or use misc.  */

/* This code is needed for the optimized mapping functions.  */
#define __tobody(c, f, a, args) \
  (__extension__							      \
   ({ int __res;							      \
      if (sizeof (c) > 1)						      \
	{								      \
	  if (__builtin_constant_p (c))					      \
	    {								      \
	      int __c = (c);						      \
	      __res = __UCLIBC_CTYPE_IN_TO_DOMAIN(__c) ? (a)[__c] : __c;      \
	    }								      \
	  else								      \
	    __res = f args;						      \
	}								      \
      else								      \
	__res = (a)[(int) (c)];						      \
      __res; }))

#if !defined __NO_CTYPE && !defined __cplusplus
# define isalnum(c)	__isctype((c), _ISalnum)
# define isalpha(c)	__isctype((c), _ISalpha)
# define iscntrl(c)	__isctype((c), _IScntrl)
# define isdigit(c)	__isctype((c), _ISdigit)
# define islower(c)	__isctype((c), _ISlower)
# define isgraph(c)	__isctype((c), _ISgraph)
# define isprint(c)	__isctype((c), _ISprint)
# define ispunct(c)	__isctype((c), _ISpunct)
# define isspace(c)	__isctype((c), _ISspace)
# define isupper(c)	__isctype((c), _ISupper)
# define isxdigit(c)	__isctype((c), _ISxdigit)

# ifdef __USE_ISOC99
#  define isblank(c)	__isctype((c), _ISblank)
# endif

# ifdef __USE_EXTERN_INLINES
extern __inline int
__NTH (tolower (int __c))
{
  return __UCLIBC_CTYPE_IN_TO_DOMAIN(__c) ? (__UCLIBC_CTYPE_TOLOWER)[__c] : __c;
}

extern __inline int
__NTH (toupper (int __c))
{
  return __UCLIBC_CTYPE_IN_TO_DOMAIN(__c) ? (__UCLIBC_CTYPE_TOUPPER)[__c] : __c;
}
# endif

# if __GNUC__ >= 2 && defined __OPTIMIZE__ && !defined __cplusplus
#  define tolower(c)	__tobody (c, tolower, __UCLIBC_CTYPE_TOLOWER, (c))
#  define toupper(c)	__tobody (c, toupper, __UCLIBC_CTYPE_TOUPPER, (c))
# endif /* Optimizing gcc */

# if defined __USE_SVID || defined __USE_MISC || defined __USE_XOPEN
#  define isascii(c)	__isascii (c)
#  define toascii(c)	__toascii (c)

#  define _tolower(c)	((int) (__UCLIBC_CTYPE_TOLOWER)[(int) (c)])
#  define _toupper(c)	((int) (__UCLIBC_CTYPE_TOUPPER)[(int) (c)])
# endif

#endif /* Not __NO_CTYPE.  */


#if defined(__USE_GNU) && defined(__UCLIBC_HAS_XLOCALE__)
/* The concept of one static locale per category is not very well
   thought out.  Many applications will need to process its data using
   information from several different locales.  Another application is
   the implementation of the internationalization handling in the
   upcoming ISO C++ standard library.  To support this another set of
   the functions using locale data exist which have an additional
   argument.

   Attention: all these functions are *not* standardized in any form.
   This is a proof-of-concept implementation.  */

/* Structure for reentrant locale using functions.  This is an
   (almost) opaque type for the user level programs.  */
# include <xlocale.h>

/* These definitions are similar to the ones above but all functions
   take as an argument a handle for the locale which shall be used.  */
#  define __isctype_l(c, type, locale) \
  ((locale)->__ctype_b[(int) (c)] & (__ctype_mask_t) type)

# define __exctype_l(name) 						      \
  extern int name (int, __locale_t) __THROW

/* The following names are all functions:
     int isCHARACTERISTIC(int c, locale_t *locale);
   which return nonzero iff C has CHARACTERISTIC.
   For the meaning of the characteristic names, see the `enum' above.  */
__exctype_l (isalnum_l);
__exctype_l (isalpha_l);
__exctype_l (iscntrl_l);
__exctype_l (isdigit_l);
__exctype_l (islower_l);
__exctype_l (isgraph_l);
__exctype_l (isprint_l);
__exctype_l (ispunct_l);
__exctype_l (isspace_l);
__exctype_l (isupper_l);
__exctype_l (isxdigit_l);

__exctype_l (isblank_l);


/* Return the lowercase version of C in locale L.  */
extern int __tolower_l (int __c, __locale_t __l) __THROW;
extern int tolower_l (int __c, __locale_t __l) __THROW;

/* Return the uppercase version of C.  */
extern int __toupper_l (int __c, __locale_t __l) __THROW;
extern int toupper_l (int __c, __locale_t __l) __THROW;

# if __GNUC__ >= 2 && defined __OPTIMIZE__ && !defined __cplusplus
#  define __tolower_l(c, locale) \
  __tobody (c, __tolower_l, (locale)->__ctype_tolower, (c, locale))
#  define __toupper_l(c, locale) \
  __tobody (c, __toupper_l, (locale)->__ctype_toupper, (c, locale))
#  define tolower_l(c, locale)	__tolower_l ((c), (locale))
#  define toupper_l(c, locale)	__toupper_l ((c), (locale))
# endif	/* Optimizing gcc */


# ifndef __NO_CTYPE
#  define __isalnum_l(c,l)	__isctype_l((c), _ISalnum, (l))
#  define __isalpha_l(c,l)	__isctype_l((c), _ISalpha, (l))
#  define __iscntrl_l(c,l)	__isctype_l((c), _IScntrl, (l))
#  define __isdigit_l(c,l)	__isctype_l((c), _ISdigit, (l))
#  define __islower_l(c,l)	__isctype_l((c), _ISlower, (l))
#  define __isgraph_l(c,l)	__isctype_l((c), _ISgraph, (l))
#  define __isprint_l(c,l)	__isctype_l((c), _ISprint, (l))
#  define __ispunct_l(c,l)	__isctype_l((c), _ISpunct, (l))
#  define __isspace_l(c,l)	__isctype_l((c), _ISspace, (l))
#  define __isupper_l(c,l)	__isctype_l((c), _ISupper, (l))
#  define __isxdigit_l(c,l)	__isctype_l((c), _ISxdigit, (l))

#  define __isblank_l(c,l)	__isctype_l((c), _ISblank, (l))

#  if defined __USE_SVID || defined __USE_MISC || defined __USE_XOPEN
#   define __isascii_l(c,l)	((l), __isascii (c))
#   define __toascii_l(c,l)	((l), __toascii (c))
#  endif

#  define isalnum_l(c,l)	__isalnum_l ((c), (l))
#  define isalpha_l(c,l)	__isalpha_l ((c), (l))
#  define iscntrl_l(c,l)	__iscntrl_l ((c), (l))
#  define isdigit_l(c,l)	__isdigit_l ((c), (l))
#  define islower_l(c,l)	__islower_l ((c), (l))
#  define isgraph_l(c,l)	__isgraph_l ((c), (l))
#  define isprint_l(c,l)	__isprint_l ((c), (l))
#  define ispunct_l(c,l)	__ispunct_l ((c), (l))
#  define isspace_l(c,l)	__isspace_l ((c), (l))
#  define isupper_l(c,l)	__isupper_l ((c), (l))
#  define isxdigit_l(c,l)	__isxdigit_l ((c), (l))

#  define isblank_l(c,l)	__isblank_l ((c), (l))

#  if defined __USE_SVID || defined __USE_MISC || defined __USE_XOPEN
#   define isascii_l(c,l)	__isascii_l ((c), (l))
#   define toascii_l(c,l)	__toascii_l ((c), (l))
#  endif

# endif /* Not __NO_CTYPE.  */

#endif /* Use GNU.  */

__END_DECLS

#else  /* __UCLIBC_HAS_CTYPE_TABLES__ */

#include <bits/uClibc_ctype.h>

#endif

#endif /* ctype.h  */
