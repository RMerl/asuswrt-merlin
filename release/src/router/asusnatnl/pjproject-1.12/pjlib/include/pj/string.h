/* $Id: string.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_STRING_H__
#define __PJ_STRING_H__

/**
 * @file string.h
 * @brief PJLIB String Operations.
 */

#include <pj/types.h>
#include <pj/compat/string.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJ_PSTR String Operations
 * @ingroup PJ_DS
 * @{
 * This module provides string manipulation API.
 *
 * \section pj_pstr_not_null_sec PJLIB String is NOT Null Terminated!
 *
 * That is the first information that developers need to know. Instead
 * of using normal C string, strings in PJLIB are represented as
 * pj_str_t structure below:
 *
 * <pre>
 *   typedef struct pj_str_t
 *   {
 *       char      *ptr;
 *       pj_size_t  slen;
 *   } pj_str_t;
 * </pre>
 *
 * There are some advantages of using this approach:
 *  - the string can point to arbitrary location in memory even
 *    if the string in that location is not null terminated. This is
 *    most usefull for text parsing, where the parsed text can just
 *    point to the original text in the input. If we use C string,
 *    then we will have to copy the text portion from the input
 *    to a string variable.
 *  - because the length of the string is known, string copy operation
 *    can be made more efficient.
 *
 * Most of APIs in PJLIB that expect or return string will represent
 * the string as pj_str_t instead of normal C string.
 *
 * \section pj_pstr_examples_sec Examples
 *
 * For some examples, please see:
 *  - @ref page_pjlib_string_test
 */

/**
 * Create string initializer from a normal C string.
 *
 * @param str	Null terminated string to be stored.
 *
 * @return	pj_str_t.
 */
PJ_IDECL(pj_str_t) pj_str(char *str);

/**
 * Create constant string from normal C string.
 *
 * @param str	The string to be initialized.
 * @param s	Null terminated string.
 *
 * @return	pj_str_t.
 */
PJ_INLINE(const pj_str_t*) pj_cstr(pj_str_t *str, const char *s)
{
    str->ptr = (char*)s;
    str->slen = s ? (pj_ssize_t)strlen(s) : 0;
    return str;
}

/**
 * Set the pointer and length to the specified value.
 *
 * @param str	    the string.
 * @param ptr	    pointer to set.
 * @param length    length to set.
 *
 * @return the string.
 */
PJ_INLINE(pj_str_t*) pj_strset( pj_str_t *str, char *ptr, pj_size_t length)
{
    str->ptr = ptr;
    str->slen = (pj_ssize_t)length;
    return str;
}

/**
 * Set the pointer and length of the string to the source string, which
 * must be NULL terminated.
 *
 * @param str	    the string.
 * @param src	    pointer to set.
 *
 * @return the string.
 */
PJ_INLINE(pj_str_t*) pj_strset2( pj_str_t *str, char *src)
{
    str->ptr = src;
    str->slen = src ? (pj_ssize_t)strlen(src) : 0;
    return str;
}

/**
 * Set the pointer and the length of the string.
 *
 * @param str	    The target string.
 * @param begin	    The start of the string.
 * @param end	    The end of the string.
 *
 * @return the target string.
 */
PJ_INLINE(pj_str_t*) pj_strset3( pj_str_t *str, char *begin, char *end )
{
    str->ptr = begin;
    str->slen = (pj_ssize_t)(end-begin);
    return str;
}

/**
 * Assign string.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strassign( pj_str_t *dst, pj_str_t *src );

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strcpy(pj_str_t *dst, const pj_str_t *src);

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strcpy2(pj_str_t *dst, const char *src);

/**
 * Copy source string to destination up to the specified max length.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 * @param max	    Maximum characters to copy.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strncpy(pj_str_t *dst, const pj_str_t *src, 
			       pj_ssize_t max);

/**
 * Copy source string to destination up to the specified max length,
 * and NULL terminate the destination. If source string length is
 * greater than or equal to max, then max-1 will be copied.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 * @param max	    Maximum characters to copy.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strncpy_with_null(pj_str_t *dst, const pj_str_t *src,
					 pj_ssize_t max);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup(pj_pool_t *pool,
			      pj_str_t *dst,
			      const pj_str_t *src);

/**
 * Duplicate string and NULL terminate the destination string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return	    The string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup_with_null(pj_pool_t *pool,
					pj_str_t *dst,
					const pj_str_t *src);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup2(pj_pool_t *pool,
			       pj_str_t *dst,
			       const char *src);

/**
 * Duplicate string and NULL terminate the destination string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return	    The string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup2_with_null(pj_pool_t *pool,
					 pj_str_t *dst,
					 const char *src);


/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t) pj_strdup3(pj_pool_t *pool, const char *src);

/**
 * Return the length of the string.
 *
 * @param str	    The string.
 *
 * @return the length of the string.
 */
PJ_INLINE(pj_size_t) pj_strlen( const pj_str_t *str )
{
    return str->slen;
}

/**
 * Return the pointer to the string data.
 *
 * @param str	    The string.
 *
 * @return the pointer to the string buffer.
 */
PJ_INLINE(const char*) pj_strbuf( const pj_str_t *str )
{
    return str->ptr;
}

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strcmp( const pj_str_t *str1, const pj_str_t *str2);

/**
 * Compare strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strcmp2( const pj_str_t *str1, const char *str2 );

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strncmp( const pj_str_t *str1, const pj_str_t *str2, 
			  pj_size_t len);

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strncmp2( const pj_str_t *str1, const char *str2, 
			   pj_size_t len);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is equal to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_stricmp(const pj_str_t *str1, const pj_str_t *str2);

/**
 * Perform lowercase comparison to the strings which consists of only
 * alnum characters. More over, it will only return non-zero if both
 * strings are not equal, not the usual negative or positive value.
 *
 * If non-alnum inputs are given, then the function may mistakenly 
 * treat two strings as equal.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The length to compare.
 *
 * @return 
 *      - 0	    if str1 is equal to str2
 *      - (-1)	    if not equal.
 */
#if defined(PJ_HAS_STRICMP_ALNUM) && PJ_HAS_STRICMP_ALNUM!=0
PJ_IDECL(int) strnicmp_alnum(const char *str1, const char *str2,
			     int len);
#else
#define strnicmp_alnum	pj_ansi_strnicmp
#endif

/**
 * Perform lowercase comparison to the strings which consists of only
 * alnum characters. More over, it will only return non-zero if both
 * strings are not equal, not the usual negative or positive value.
 *
 * If non-alnum inputs are given, then the function may mistakenly 
 * treat two strings as equal.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *      - 0	    if str1 is equal to str2
 *      - (-1)	    if not equal.
 */
#if defined(PJ_HAS_STRICMP_ALNUM) && PJ_HAS_STRICMP_ALNUM!=0
PJ_IDECL(int) pj_stricmp_alnum(const pj_str_t *str1, const pj_str_t *str2);
#else
#define pj_stricmp_alnum    pj_stricmp
#endif

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_stricmp2( const pj_str_t *str1, const char *str2);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strnicmp( const pj_str_t *str1, const pj_str_t *str2, 
			   pj_size_t len);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strnicmp2( const pj_str_t *str1, const char *str2, 
			    pj_size_t len);

/**
 * Concatenate strings.
 *
 * @param dst	    The destination string.
 * @param src	    The source string.
 */
PJ_IDECL(void) pj_strcat(pj_str_t *dst, const pj_str_t *src);


/**
 * Concatenate strings.
 *
 * @param dst	    The destination string.
 * @param src	    The source string.
 */
PJ_IDECL(void) pj_strcat2(pj_str_t *dst, const char *src);


/**
 * Finds a character in a string.
 *
 * @param str	    The string.
 * @param chr	    The character to find.
 *
 * @return the pointer to first character found, or NULL.
 */
PJ_INLINE(char*) pj_strchr( const pj_str_t *str, int chr)
{
    return (char*) memchr((char*)str->ptr, chr, str->slen);
}

/**
 * Find the occurence of a substring substr in string str.
 *
 * @param str	    The string to search.
 * @param substr    The string to search fo.
 *
 * @return the pointer to the position of substr in str, or NULL. Note
 *         that if str is not NULL terminated, the returned pointer
 *         is pointing to non-NULL terminated string.
 */
PJ_DECL(char*) pj_strstr(const pj_str_t *str, const pj_str_t *substr);

/**
 * Performs substring lookup like pj_strstr() but ignores the case of
 * both strings.
 *
 * @param str	    The string to search.
 * @param substr    The string to search fo.
 *
 * @return the pointer to the position of substr in str, or NULL. Note
 *         that if str is not NULL terminated, the returned pointer
 *         is pointing to non-NULL terminated string.
 */
PJ_DECL(char*) pj_stristr(const pj_str_t *str, const pj_str_t *substr);

/**
 * Remove (trim) leading whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_DECL(pj_str_t*) pj_strltrim( pj_str_t *str );

/**
 * Remove (trim) the trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_DECL(pj_str_t*) pj_strrtrim( pj_str_t *str );

/**
 * Remove (trim) leading and trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_IDECL(pj_str_t*) pj_strtrim( pj_str_t *str );

/**
 * Initialize the buffer with some random string. Note that the 
 * generated string is not NULL terminated.
 *
 * @param str	    the string to store the result.
 * @param length    the length of the random string to generate.
 *
 * @return the string.
 */
PJ_DECL(char*) pj_create_random_string(char *str, pj_size_t length);

/**
 * Convert string to unsigned integer. The conversion will stop as
 * soon as non-digit character is found or all the characters have
 * been processed.
 *
 * @param str	the string.
 *
 * @return the unsigned integer.
 */
PJ_DECL(unsigned long) pj_strtoul(const pj_str_t *str);

/**
 * Convert strings to an unsigned long-integer value.
 * This function stops reading the string input either when the number
 * of characters has exceeded the length of the input or it has read 
 * the first character it cannot recognize as part of a number, that is
 * a character greater than or equal to base. 
 *
 * @param str	    The input string.
 * @param endptr    Optional pointer to receive the remainder/unparsed
 *		    portion of the input.
 * @param base	    Number base to use.
 *
 * @return the unsigned integer number.
 */
PJ_DECL(unsigned long) pj_strtoul2(const pj_str_t *str, pj_str_t *endptr,
				   unsigned base);

/**
 * Utility to convert unsigned integer to string. Note that the
 * string will be NULL terminated.
 *
 * @param val	    the unsigned integer value.
 * @param buf	    the buffer
 *
 * @return the number of characters written
 */
PJ_DECL(int) pj_utoa(unsigned long val, char *buf);

/**
 * Convert unsigned integer to string with minimum digits. Note that the
 * string will be NULL terminated.
 *
 * @param val	    The unsigned integer value.
 * @param buf	    The buffer.
 * @param min_dig   Minimum digits to be printed, or zero to specify no
 *		    minimum digit.
 * @param pad	    The padding character to be put in front of the string
 *		    when the digits is less than minimum.
 *
 * @return the number of characters written.
 */
PJ_DECL(int) pj_utoa_pad( unsigned long val, char *buf, int min_dig, int pad);


/**
 * Fill the memory location with zero.
 *
 * @param dst	    The destination buffer.
 * @param size	    The number of bytes.
 */
PJ_INLINE(void) pj_bzero(void *dst, pj_size_t size)
{
#if defined(PJ_HAS_BZERO) && PJ_HAS_BZERO!=0
    bzero(dst, size);
#else
    memset(dst, 0, size);
#endif
}


/**
 * Fill the memory location with value.
 *
 * @param dst	    The destination buffer.
 * @param c	    Character to set.
 * @param size	    The number of characters.
 *
 * @return the value of dst.
 */
PJ_INLINE(void*) pj_memset(void *dst, int c, pj_size_t size)
{
    return memset(dst, c, size);
}

/**
 * Copy buffer.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
PJ_INLINE(void*) pj_memcpy(void *dst, const void *src, pj_size_t size)
{
    return memcpy(dst, src, size);
}

/**
 * Move memory.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
PJ_INLINE(void*) pj_memmove(void *dst, const void *src, pj_size_t size)
{
    return memmove(dst, src, size);
}

/**
 * Compare buffers.
 *
 * @param buf1	    The first buffer.
 * @param buf2	    The second buffer.
 * @param size	    The size to compare.
 *
 * @return negative, zero, or positive value.
 */
PJ_INLINE(int) pj_memcmp(const void *buf1, const void *buf2, pj_size_t size)
{
    return memcmp(buf1, buf2, size);
}

/**
 * Find character in the buffer.
 *
 * @param buf	    The buffer.
 * @param c	    The character to find.
 * @param size	    The size to check.
 *
 * @return the pointer to location where the character is found, or NULL if
 *         not found.
 */
PJ_INLINE(void*) pj_memchr(const void *buf, int c, pj_size_t size)
{
    return (void*)memchr((void*)buf, c, size);
}


/**
 * @}
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  include <pj/string_i.h>
#endif

PJ_END_DECL

#endif	/* __PJ_STRING_H__ */

