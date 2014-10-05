/*  Copyright (C) 2002     Manuel Novoa III
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  The GNU C Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with the GNU C Library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA.
 */

/*  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!
 *
 *  Besides uClibc, I'm using this code in my libc for elks, which is
 *  a 16-bit environment with a fairly limited compiler.  It would make
 *  things much easier for me if this file isn't modified unnecessarily.
 *  In particular, please put any new or replacement functions somewhere
 *  else, and modify the makefile to use your version instead.
 *  Thanks.  Manuel
 *
 *  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION! */

#if !defined(_CTYPE_H) && !defined(_WCTYPE_H)
#error Always include <{w}ctype.h> rather than <bits/uClibc_ctype.h>
#endif

#ifndef _BITS_CTYPE_H
#define _BITS_CTYPE_H

#ifdef __UCLIBC_GEN_LOCALE

/* Taking advantage of the C99 mutual-exclusion guarantees for the various
 * (w)ctype classes, including the descriptions of printing and control
 * (w)chars, we can place each in one of the following mutually-exlusive
 * subsets.  Since there are less than 16, we can store the data for
 * each (w)chars in a nibble. In contrast, glibc uses an unsigned int
 * per (w)char, with one bit flag for each is* type.  While this allows
 * a simple '&' operation to determine the type vs. a range test and a
 * little special handling for the "blank" and "xdigit" types in my
 * approach, it also uses 8 times the space for the tables on the typical
 * 32-bit archs we supported.*/
enum {
	__CTYPE_unclassified = 0,
	__CTYPE_alpha_nonupper_nonlower,
	__CTYPE_alpha_lower,
	__CTYPE_alpha_upper_lower,
	__CTYPE_alpha_upper,
	__CTYPE_digit,
	__CTYPE_punct,
	__CTYPE_graph,
	__CTYPE_print_space_nonblank,
	__CTYPE_print_space_blank,
	__CTYPE_space_nonblank_noncntrl,
	__CTYPE_space_blank_noncntrl,
	__CTYPE_cntrl_space_nonblank,
	__CTYPE_cntrl_space_blank,
	__CTYPE_cntrl_nonspace
};

/* Some macros that test for various (w)ctype classes when passed one of the
 * designator values enumerated above. */
#define __CTYPE_isalnum(D)		((unsigned int)(D-1) <= (__CTYPE_digit-1))
#define __CTYPE_isalpha(D)		((unsigned int)(D-1) <= (__CTYPE_alpha_upper-1))
#define __CTYPE_isblank(D) \
	((((unsigned int)(D - __CTYPE_print_space_nonblank)) <= 5) && (D & 1))
#define __CTYPE_iscntrl(D)		(((unsigned int)(D - __CTYPE_cntrl_space_nonblank)) <= 2)
#define __CTYPE_isdigit(D)		(D == __CTYPE_digit)
#define __CTYPE_isgraph(D)		((unsigned int)(D-1) <= (__CTYPE_graph-1))
#define __CTYPE_islower(D)		(((unsigned int)(D - __CTYPE_alpha_lower)) <= 1)
#define __CTYPE_isprint(D)		((unsigned int)(D-1) <= (__CTYPE_print_space_blank-1))
#define __CTYPE_ispunct(D)		(D == __CTYPE_punct)
#define __CTYPE_isspace(D)		(((unsigned int)(D - __CTYPE_print_space_nonblank)) <= 5)
#define __CTYPE_isupper(D)		(((unsigned int)(D - __CTYPE_alpha_upper_lower)) <= 1)
/*  #define __CTYPE_isxdigit(D) -- isxdigit is untestable this way. 
 *  But that's ok as isxdigit() (and isdigit() too) are locale-invariant. */

#else  /* __UCLIBC_GEN_LOCALE *****************************************/

/* Define some ctype macros valid for the C/POSIX locale. */

/* ASCII ords of \t, \f, \n, \r, and \v are 9, 12, 10, 13, 11 respectively. */
#define __C_isspace(c) \
	((sizeof(c) == sizeof(char)) \
	 ? ((((c) == ' ') || (((unsigned char)((c) - 9)) <= (13 - 9)))) \
	 : ((((c) == ' ') || (((unsigned int)((c) - 9)) <= (13 - 9)))))
#define __C_isblank(c) (((c) == ' ') || ((c) == '\t'))
#define __C_isdigit(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned char)((c) - '0')) < 10) \
	 : (((unsigned int)((c) - '0')) < 10))
#define __C_isxdigit(c) \
	(__C_isdigit(c) \
	 || ((sizeof(c) == sizeof(char)) \
		 ? (((unsigned char)((((c)) | 0x20) - 'a')) < 6) \
		 : (((unsigned int)((((c)) | 0x20) - 'a')) < 6)))
#define __C_iscntrl(c) \
	((sizeof(c) == sizeof(char)) \
	 ? ((((unsigned char)(c)) < 0x20) || ((c) == 0x7f)) \
	 : ((((unsigned int)(c)) < 0x20) || ((c) == 0x7f)))
#define __C_isalpha(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned char)(((c) | 0x20) - 'a')) < 26) \
	 : (((unsigned int)(((c) | 0x20) - 'a')) < 26))
#define __C_isalnum(c) (__C_isalpha(c) || __C_isdigit(c))
#define __C_isprint(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned char)((c) - 0x20)) <= (0x7e - 0x20)) \
	 : (((unsigned int)((c) - 0x20)) <= (0x7e - 0x20)))
#define __C_islower(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned char)((c) - 'a')) < 26) \
	 : (((unsigned int)((c) - 'a')) < 26))
#define __C_isupper(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned char)((c) - 'A')) < 26) \
	 : (((unsigned int)((c) - 'A')) < 26))
#define __C_ispunct(c) \
	((!__C_isalnum(c)) \
	 && ((sizeof(c) == sizeof(char)) \
		 ? (((unsigned char)((c) - 0x21)) <= (0x7e - 0x21)) \
		 : (((unsigned int)((c) - 0x21)) <= (0x7e - 0x21))))
#define __C_isgraph(c) \
	((sizeof(c) == sizeof(char)) \
	 ? (((unsigned int)((c) - 0x21)) <= (0x7e - 0x21)) \
	 : (((unsigned int)((c) - 0x21)) <= (0x7e - 0x21)))

#define __C_tolower(c) (__C_isupper(c) ? ((c) | 0x20) : (c))
#define __C_toupper(c) (__C_islower(c) ? ((c) ^ 0x20) : (c))

/**********************************************************************/
__BEGIN_DECLS

extern int isalnum(int c) __THROW;
extern int isalpha(int c) __THROW;
#ifdef __USE_ISOC99
extern int isblank(int c) __THROW;
#endif
extern int iscntrl(int c) __THROW;
extern int isdigit(int c) __THROW;
extern int isgraph(int c) __THROW;
extern int islower(int c) __THROW;
extern int isprint(int c) __THROW;
extern int ispunct(int c) __THROW;
extern int isspace(int c) __THROW;
extern int isupper(int c) __THROW;
extern int isxdigit(int c) __THROW;

extern int tolower(int c) __THROW;
extern int toupper(int c) __THROW;

#if defined __USE_SVID || defined __USE_MISC || defined __USE_XOPEN
extern int isascii(int c) __THROW;
extern int toascii(int c) __THROW;
#endif

#if defined _LIBC && (defined NOT_IN_libc || defined IS_IN_libc)
/* isdigit() is really locale-invariant, so provide some small fast macros.
 * These are uClibc-specific. */
#define __isdigit_char(C)    (((unsigned char)((C) - '0')) <= 9)
#define __isdigit_int(C)     (((unsigned int)((C) - '0')) <= 9)
#endif

/* Next, some ctype macros which are valid for all supported locales. */
/* WARNING: isspace and isblank need to be reverified if more 8-bit codesets
 * are added!!!  But isdigit and isxdigit are always valid. */

/* #define __isspace(c)	__C_isspace(c) */
/* #define __isblank(c)	__C_isblank(c) */

/* #define __isdigit(c)	__C_isdigit(c) */
/* #define __isxdigit(c)	__C_isxdigit(c) */

/* Now some non-ansi/iso c99 macros. */

#define __isascii(c) (((c) & ~0x7f) == 0)
#define __toascii(c) ((c) & 0x7f)
#define _toupper(c) ((c) ^ 0x20)
#define _tolower(c) ((c) | 0x20)

__END_DECLS

/**********************************************************************/
#ifdef __GNUC__

#define __isbody_C_macro(f,args)  __C_ ## f args

#define __isbody(f,c) \
	(__extension__ ({ \
		int __res; \
		if (sizeof(c) > sizeof(char)) { \
			int __c = (c); \
			__res = __isbody_C_macro(f,(__c)); \
		} else { \
			unsigned char __c = (c); \
			__res = __isbody_C_macro(f,(__c)); \
		} \
		__res; \
	}))

#define __body_C_macro(f,args)  __C_ ## f args

#define __body(f,c) \
	(__extension__ ({ \
		int __res; \
		if (sizeof(c) > sizeof(char)) { \
			int __c = (c); \
			__res = __body_C_macro(f,(__c)); \
		} else { \
			unsigned char __c = (c); \
			__res = __body_C_macro(f,(__c)); \
		} \
		__res; \
	}))

#define __isspace(c)		__body(isspace,c)
#define __isblank(c)		__body(isblank,c)
#define __isdigit(c)		__body(isdigit,c)
#define __isxdigit(c)		__body(isxdigit,c)
#define __iscntrl(c)		__body(iscntrl,c)
#define __isalpha(c)		__body(isalpha,c)
#define __isalnum(c)		__body(isalnum,c)
#define __isprint(c)		__body(isprint,c)
#define __islower(c)		__body(islower,c)
#define __isupper(c)		__body(isupper,c)
#define __ispunct(c)		__body(ispunct,c)
#define __isgraph(c)		__body(isgraph,c)

#define __tolower(c)		__body(tolower,c)
#define __toupper(c)		__body(toupper,c)

#if !defined __NO_CTYPE && !defined __cplusplus

#define isspace(c)			__isspace(c)
#define isblank(c)			__isblank(c)
#define isdigit(c)			__isdigit(c)
#define isxdigit(c)			__isxdigit(c)
#define iscntrl(c)			__iscntrl(c)
#define isalpha(c)			__isalpha(c)
#define isalnum(c)			__isalnum(c)
#define isprint(c)			__isprint(c)
#define islower(c)			__islower(c)
#define isupper(c)			__isupper(c)
#define ispunct(c)			__ispunct(c)
#define isgraph(c)			__isgraph(c)

#define tolower(c)			__tolower(c)
#define toupper(c)			__toupper(c)


#endif

#else  /* _GNUC__   ***************************************************/

#if !defined __NO_CTYPE && !defined __cplusplus

/* These macros should be safe from side effects. */

#define isdigit(c)			__C_isdigit(c)
#define isalpha(c)			__C_isalpha(c)
#define isprint(c)			__C_isprint(c)
#define islower(c)			__C_islower(c)
#define isupper(c)			__C_isupper(c)
#define isgraph(c)			__C_isgraph(c)

#endif

#endif /* __GNUC__ */
/**********************************************************************/

#endif  /* __UCLIBC_GEN_LOCALE */

#endif /* _BITS_CTYPE_H */
