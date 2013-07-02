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

#ifndef _BITS_UCLIBC_CTYPE_H
#define _BITS_UCLIBC_CTYPE_H


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
	 ? (((unsigned char)((c) - 0x21)) <= (0x7e - 0x21)) \
	 : (((unsigned int)((c) - 0x21)) <= (0x7e - 0x21)))

#define __C_tolower(c) (__C_isupper(c) ? ((c) | 0x20) : (c))
#define __C_toupper(c) (__C_islower(c) ? ((c) ^ 0x20) : (c))

/**********************************************************************/
__BEGIN_DECLS


/* Now some non-ansi/iso c99 macros. */

#ifdef __UCLIBC_SUSV4_LEGACY__
#define __isascii(c) (((c) & ~0x7f) == 0)
#define __toascii(c) ((c) & 0x7f)
/* Works correctly *only* on lowercase letters! */
#define _toupper(c) ((c) ^ 0x20)
/* Works correctly *only* on letters (of any case) and numbers */
#define _tolower(c) ((c) | 0x20)
#endif

__END_DECLS

/**********************************************************************/
#ifdef __GNUC__

# define __body_C_macro(f,args)  __C_ ## f args

# define __body(f,c) \
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

# define __isspace(c)   __body(isspace,c)
# define __isblank(c)   __body(isblank,c)
# define __isdigit(c)   __body(isdigit,c)
# define __isxdigit(c)  __body(isxdigit,c)
# define __iscntrl(c)   __body(iscntrl,c)
# define __isalpha(c)   __body(isalpha,c)
# define __isalnum(c)   __body(isalnum,c)
# define __isprint(c)   __body(isprint,c)
# define __islower(c)   __body(islower,c)
# define __isupper(c)   __body(isupper,c)
# define __ispunct(c)   __body(ispunct,c)
# define __isgraph(c)   __body(isgraph,c)

/*locale-aware ctype.h has no __tolower, why stub locale
 *tries to have it? remove after 0.9.31
 *# define __tolower(c) __body(tolower,c)
 *# define __toupper(c) __body(toupper,c)
 */

/* Do not combine in one #if - unifdef tool is not that clever */
# ifndef __cplusplus

#  define isspace(c)    __isspace(c)
#  define isblank(c)    __isblank(c)
#  define isdigit(c)    __isdigit(c)
#  define isxdigit(c)   __isxdigit(c)
#  define iscntrl(c)    __iscntrl(c)
#  define isalpha(c)    __isalpha(c)
#  define isalnum(c)    __isalnum(c)
#  define isprint(c)    __isprint(c)
#  define islower(c)    __islower(c)
#  define isupper(c)    __isupper(c)
#  define ispunct(c)    __ispunct(c)
#  define isgraph(c)    __isgraph(c)

#  define tolower(c)    __body(tolower,c)
#  define toupper(c)    __body(toupper,c)

# endif

#else  /* !_GNUC__ */

# ifndef __cplusplus

/* These macros should be safe from side effects!
 * (not all __C_xxx macros are) */
#  define isdigit(c)    __C_isdigit(c)
#  define isalpha(c)    __C_isalpha(c)
#  define isprint(c)    __C_isprint(c)
#  define islower(c)    __C_islower(c)
#  define isupper(c)    __C_isupper(c)
#  define isgraph(c)    __C_isgraph(c)

# endif

#endif /* __GNUC__ */
/**********************************************************************/


#endif /* _BITS_UCLIBC_CTYPE_H */
