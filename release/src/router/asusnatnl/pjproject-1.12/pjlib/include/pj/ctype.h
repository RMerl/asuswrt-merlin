/* $Id: ctype.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_CTYPE_H__
#define __PJ_CTYPE_H__

/**
 * @file ctype.h
 * @brief C type helper macros.
 */

#include <pj/types.h>
#include <pj/compat/ctype.h>

PJ_BEGIN_DECL

/**
 * @defgroup pj_ctype ctype - Character Type
 * @ingroup PJ_MISC
 * @{
 *
 * This module contains several inline functions/macros for testing or
 * manipulating character types. It is provided in PJLIB because PJLIB
 * must not depend to LIBC.
 */

/** 
 * Returns a non-zero value if either isalpha or isdigit is true for c.
 * @param c     The integer character to test.
 * @return      Non-zero value if either isalpha or isdigit is true for c.
 */
PJ_INLINE(int) pj_isalnum(unsigned char c) { return isalnum(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * alphabetic character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of an 
 *              alphabetic character.
 */
PJ_INLINE(int) pj_isalpha(unsigned char c) { return isalpha(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * ASCII character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an ASCII character.
 */
PJ_INLINE(int) pj_isascii(unsigned char c) { return c<128; }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a decimal-digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a decimal-digit character.
 */
PJ_INLINE(int) pj_isdigit(unsigned char c) { return isdigit(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a space character (0x09 - 0x0D or 0x20).
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a space character (0x09 - 0x0D or 0x20).
 */
PJ_INLINE(int) pj_isspace(unsigned char c) { return isspace(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a lowercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a lowercase character.
 */
PJ_INLINE(int) pj_islower(unsigned char c) { return islower(c); }


/** 
 * Returns a non-zero value if c is a particular representation of 
 * a uppercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a uppercase character.
 */
PJ_INLINE(int) pj_isupper(unsigned char c) { return isupper(c); }

/**
 * Returns a non-zero value if c is a either a space (' ') or horizontal
 * tab ('\\t') character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a either a space (' ') or horizontal
 *              tab ('\\t') character.
 */
PJ_INLINE(int) pj_isblank(unsigned char c) { return isblank(c); }

/**
 * Converts character to lowercase.
 * @param c     The integer character to convert.
 * @return      Lowercase character of c.
 */
PJ_INLINE(int) pj_tolower(unsigned char c) { return tolower(c); }

/**
 * Converts character to uppercase.
 * @param c     The integer character to convert.
 * @return      Uppercase character of c.
 */
PJ_INLINE(int) pj_toupper(unsigned char c) { return toupper(c); }

/**
 * Returns a non-zero value if c is a particular representation of 
 * an hexadecimal digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an hexadecimal digit character.
 */
PJ_INLINE(int) pj_isxdigit(unsigned char c){ return isxdigit(c); }

/**
 * Array of hex digits, in lowerspace.
 */
/*extern char pj_hex_digits[];*/
#define pj_hex_digits	"0123456789abcdef"

/**
 * Convert a value to hex representation.
 * @param value	    Integral value to convert.
 * @param p	    Buffer to hold the hex representation, which must be
 *		    at least two bytes length.
 */
PJ_INLINE(void) pj_val_to_hex_digit(unsigned value, char *p)
{
    *p++ = pj_hex_digits[ (value & 0xF0) >> 4 ];
    *p   = pj_hex_digits[ (value & 0x0F) ];
}

/**
 * Convert hex digit c to integral value.
 * @param c	The hex digit character.
 * @return	The integral value between 0 and 15.
 */
PJ_INLINE(unsigned) pj_hex_digit_to_val(unsigned char c)
{
    if (c <= '9')
	return (c-'0') & 0x0F;
    else if (c <= 'F')
	return  (c-'A'+10) & 0x0F;
    else
	return (c-'a'+10) & 0x0F;
}

/** @} */

PJ_END_DECL

#endif	/* __PJ_CTYPE_H__ */

