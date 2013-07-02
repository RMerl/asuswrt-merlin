/*  Copyright (C) 2002, 2003     Manuel Novoa III
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

#ifndef _UCLIBC_LOCALE_H
#define _UCLIBC_LOCALE_H

/**********************************************************************/
/* uClibc compatibilty stuff */

#ifdef __UCLIBC_HAS_LOCALE__

# undef __LOCALE_C_ONLY

#else

# define __LOCALE_C_ONLY


#endif

/**********************************************************************/

#define __NL_ITEM_CATEGORY_SHIFT        8
#define __NL_ITEM_INDEX_MASK            0xff

/* TODO: Make sure these agree with the locale mmap file gererator! */

#define __LC_CTYPE      0
#define __LC_NUMERIC    1
#define __LC_MONETARY   2
#define __LC_TIME       3
#define __LC_COLLATE    4
#define __LC_MESSAGES   5
#define __LC_ALL        6

/**********************************************************************/
#ifndef __LOCALE_C_ONLY

enum {
	__ctype_encoding_7_bit,		/* C/POSIX */
	__ctype_encoding_utf8,		/* UTF-8 */
	__ctype_encoding_8_bit		/* for 8-bit codeset locales */
};

#define LOCALE_STRING_SIZE (2 * __LC_ALL + 2)

 /*
  * '#' + 2_per_category + '\0'
  *       {locale row # : 0 = C|POSIX} + 0x8001
  *       encoded in two chars as (((N+1) >> 8) | 0x80) and ((N+1) & 0xff)
  *       so decode is  ((((uint16_t)(*s & 0x7f)) << 8) + s[1]) - 1
  *
  *       Note: 0s are not used as they are nul-terminators for strings.
  *       Note: 0xff, 0xff is the encoding for a non-selected locale.
  *             (see setlocale() below).
  * In particular, C/POSIX locale is '#' + "\x80\x01"}*LC_ALL + nul.
  */

struct __uclibc_locale_struct;
typedef struct __uclibc_locale_struct *__locale_t;


#endif /* !defined(__LOCALE_C_ONLY) */

#endif /* _UCLIBC_LOCALE_H */
