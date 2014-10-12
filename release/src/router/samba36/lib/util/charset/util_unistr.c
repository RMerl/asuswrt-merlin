/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Simo Sorce 2001
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/locale.h"

/**
 Case insensitive string compararison
**/
_PUBLIC_ int strcasecmp_m(const char *s1, const char *s2)
{
	codepoint_t c1=0, c2=0;
	size_t size1, size2;
	struct smb_iconv_convenience *iconv_convenience = get_iconv_convenience();

	/* handle null ptr comparisons to simplify the use in qsort */
	if (s1 == s2) return 0;
	if (s1 == NULL) return -1;
	if (s2 == NULL) return 1;

	while (*s1 && *s2) {
		c1 = next_codepoint_convenience(iconv_convenience, s1, &size1);
		c2 = next_codepoint_convenience(iconv_convenience, s2, &size2);

		s1 += size1;
		s2 += size2;

		if (c1 == c2) {
			continue;
		}

		if (c1 == INVALID_CODEPOINT ||
		    c2 == INVALID_CODEPOINT) {
			/* what else can we do?? */
			return strcasecmp(s1, s2);
		}

		if (toupper_m(c1) != toupper_m(c2)) {
			return c1 - c2;
		}
	}

	return *s1 - *s2;
}

/**
 Case insensitive string compararison, length limited
**/
_PUBLIC_ int strncasecmp_m(const char *s1, const char *s2, size_t n)
{
	codepoint_t c1=0, c2=0;
	size_t size1, size2;
	struct smb_iconv_convenience *iconv_convenience = get_iconv_convenience();

	/* handle null ptr comparisons to simplify the use in qsort */
	if (s1 == s2) return 0;
	if (s1 == NULL) return -1;
	if (s2 == NULL) return 1;

	while (*s1 && *s2 && n) {
		n--;

		c1 = next_codepoint_convenience(iconv_convenience, s1, &size1);
		c2 = next_codepoint_convenience(iconv_convenience, s2, &size2);

		s1 += size1;
		s2 += size2;

		if (c1 == c2) {
			continue;
		}

		if (c1 == INVALID_CODEPOINT ||
		    c2 == INVALID_CODEPOINT) {
			/* what else can we do?? */
			return strcasecmp(s1, s2);
		}

		if (toupper_m(c1) != toupper_m(c2)) {
			return c1 - c2;
		}
	}

	if (n == 0) {
		return 0;
	}

	return *s1 - *s2;
}

/**
 * Compare 2 strings.
 *
 * @note The comparison is case-insensitive.
 **/
_PUBLIC_ bool strequal_m(const char *s1, const char *s2)
{
	return strcasecmp_m(s1,s2) == 0;
}

/**
 Compare 2 strings (case sensitive).
**/
_PUBLIC_ bool strcsequal_m(const char *s1,const char *s2)
{
	if (s1 == s2)
		return true;
	if (!s1 || !s2)
		return false;
	
	return strcmp(s1,s2) == 0;
}


/**
 String replace.
 NOTE: oldc and newc must be 7 bit characters
**/
_PUBLIC_ void string_replace_m(char *s, char oldc, char newc)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	while (s && *s) {
		size_t size;
		codepoint_t c = next_codepoint_convenience(ic, s, &size);
		if (c == oldc) {
			*s = newc;
		}
		s += size;
	}
}

/**
 Paranoid strcpy into a buffer of given length (includes terminating
 zero. Strips out all but 'a-Z0-9' and the character in other_safe_chars
 and replaces with '_'. Deliberately does *NOT* check for multibyte
 characters. Don't change it !
**/

_PUBLIC_ char *alpha_strcpy(char *dest, const char *src, const char *other_safe_chars, size_t maxlength)
{
	size_t len, i;

	if (maxlength == 0) {
		/* can't fit any bytes at all! */
		return NULL;
	}

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in alpha_strcpy\n"));
		return NULL;
	}

	if (!src) {
		*dest = 0;
		return dest;
	}  

	len = strlen(src);
	if (len >= maxlength)
		len = maxlength - 1;

	if (!other_safe_chars)
		other_safe_chars = "";

	for(i = 0; i < len; i++) {
		int val = (src[i] & 0xff);
		if (isupper(val) || islower(val) || isdigit(val) || strchr_m(other_safe_chars, val))
			dest[i] = src[i];
		else
			dest[i] = '_';
	}

	dest[i] = '\0';

	return dest;
}

/**
 * Calculate the number of units (8 or 16-bit, depending on the
 * destination charset), that would be needed to convert the input
 * string which is expected to be in in src_charset encoding to the
 * destination charset (which should be a unicode charset).
 */
_PUBLIC_ size_t strlen_m_ext(const char *s, charset_t src_charset, charset_t dst_charset)
{
	size_t count = 0;
	struct smb_iconv_convenience *ic = get_iconv_convenience();

	if (!s) {
		return 0;
	}

	while (*s && !(((uint8_t)*s) & 0x80)) {
		s++;
		count++;
	}

	if (!*s) {
		return count;
	}

	while (*s) {
		size_t c_size;
		codepoint_t c = next_codepoint_convenience_ext(ic, s, src_charset, &c_size);
		s += c_size;

		switch (dst_charset) {
		case CH_UTF16LE:
		case CH_UTF16BE:
		case CH_UTF16MUNGED:
			if (c < 0x10000) {
				count += 1;
			} else {
				count += 2;
			}
			break;
		case CH_UTF8:
			/*
			 * this only checks ranges, and does not
			 * check for invalid codepoints
			 */
			if (c < 0x80) {
				count += 1;
			} else if (c < 0x800) {
				count += 2;
			} else if (c < 0x1000) {
				count += 3;
			} else {
				count += 4;
			}
			break;
		default:
			/*
			 * non-unicode encoding:
			 * assume that each codepoint fits into
			 * one unit in the destination encoding.
			 */
			count += 1;
		}
	}

	return count;
}

_PUBLIC_ size_t strlen_m_ext_term(const char *s, const charset_t src_charset,
				  const charset_t dst_charset)
{
	if (!s) {
		return 0;
	}
	return strlen_m_ext(s, src_charset, dst_charset) + 1;
}

/**
 * Calculate the number of 16-bit units that would be needed to convert
 * the input string which is expected to be in CH_UNIX encoding to UTF16.
 *
 * This will be the same as the number of bytes in a string for single
 * byte strings, but will be different for multibyte.
 */
_PUBLIC_ size_t strlen_m(const char *s)
{
	return strlen_m_ext(s, CH_UNIX, CH_UTF16LE);
}

/**
   Work out the number of multibyte chars in a string, including the NULL
   terminator.
**/
_PUBLIC_ size_t strlen_m_term(const char *s)
{
	if (!s) {
		return 0;
	}

	return strlen_m(s) + 1;
}

/*
 * Weird helper routine for the winreg pipe: If nothing is around, return 0,
 * if a string is there, include the terminator.
 */

_PUBLIC_ size_t strlen_m_term_null(const char *s)
{
	size_t len;
	if (!s) {
		return 0;
	}
	len = strlen_m(s);
	if (len == 0) {
		return 0;
	}

	return len+1;
}

/**
 Strchr and strrchr_m are a bit complex on general multi-byte strings. 
**/
_PUBLIC_ char *strchr_m(const char *s, char c)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	if (s == NULL) {
		return NULL;
	}
	/* characters below 0x3F are guaranteed to not appear in
	   non-initial position in multi-byte charsets */
	if ((c & 0xC0) == 0) {
		return strchr(s, c);
	}

	while (*s) {
		size_t size;
		codepoint_t c2 = next_codepoint_convenience(ic, s, &size);
		if (c2 == c) {
			return discard_const_p(char, s);
		}
		s += size;
	}

	return NULL;
}

/**
 * Multibyte-character version of strrchr
 */
_PUBLIC_ char *strrchr_m(const char *s, char c)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	char *ret = NULL;

	if (s == NULL) {
		return NULL;
	}

	/* characters below 0x3F are guaranteed to not appear in
	   non-initial position in multi-byte charsets */
	if ((c & 0xC0) == 0) {
		return strrchr(s, c);
	}

	while (*s) {
		size_t size;
		codepoint_t c2 = next_codepoint_convenience(ic, s, &size);
		if (c2 == c) {
			ret = discard_const_p(char, s);
		}
		s += size;
	}

	return ret;
}

/**
  return True if any (multi-byte) character is lower case
*/
_PUBLIC_ bool strhaslower(const char *string)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	while (*string) {
		size_t c_size;
		codepoint_t s;
		codepoint_t t;

		s = next_codepoint_convenience(ic, string, &c_size);
		string += c_size;

		t = toupper_m(s);

		if (s != t) {
			return true; /* that means it has lower case chars */
		}
	}

	return false;
} 

/**
  return True if any (multi-byte) character is upper case
*/
_PUBLIC_ bool strhasupper(const char *string)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	while (*string) {
		size_t c_size;
		codepoint_t s;
		codepoint_t t;

		s = next_codepoint_convenience(ic, string, &c_size);
		string += c_size;

		t = tolower_m(s);

		if (s != t) {
			return true; /* that means it has upper case chars */
		}
	}

	return false;
} 

/**
 Convert a string to lower case, allocated with talloc
**/
_PUBLIC_ char *strlower_talloc(TALLOC_CTX *ctx, const char *src)
{
	size_t size=0;
	char *dest;
	struct smb_iconv_convenience *iconv_convenience = get_iconv_convenience();

	if(src == NULL) {
		return NULL;
	}

	/* this takes advantage of the fact that upper/lower can't
	   change the length of a character by more than 1 byte */
	dest = talloc_array(ctx, char, 2*(strlen(src))+1);
	if (dest == NULL) {
		return NULL;
	}

	while (*src) {
		size_t c_size;
		codepoint_t c = next_codepoint_convenience(iconv_convenience, src, &c_size);
		src += c_size;

		c = tolower_m(c);

		c_size = push_codepoint_convenience(iconv_convenience, dest+size, c);
		if (c_size == -1) {
			talloc_free(dest);
			return NULL;
		}
		size += c_size;
	}

	dest[size] = 0;

	/* trim it so talloc_append_string() works */
	dest = talloc_realloc(ctx, dest, char, size+1);

	talloc_set_name_const(dest, dest);

	return dest;
}

/**
 Convert a string to UPPER case, allocated with talloc
 source length limited to n bytes
**/
_PUBLIC_ char *strupper_talloc_n(TALLOC_CTX *ctx, const char *src, size_t n)
{
	size_t size=0;
	char *dest;
	struct smb_iconv_convenience *iconv_convenience = get_iconv_convenience();
	
	if (!src) {
		return NULL;
	}

	/* this takes advantage of the fact that upper/lower can't
	   change the length of a character by more than 1 byte */
	dest = talloc_array(ctx, char, 2*(n+1));
	if (dest == NULL) {
		return NULL;
	}

	while (n-- && *src) {
		size_t c_size;
		codepoint_t c = next_codepoint_convenience(iconv_convenience, src, &c_size);
		src += c_size;

		c = toupper_m(c);

		c_size = push_codepoint_convenience(iconv_convenience, dest+size, c);
		if (c_size == -1) {
			talloc_free(dest);
			return NULL;
		}
		size += c_size;
	}

	dest[size] = 0;

	/* trim it so talloc_append_string() works */
	dest = talloc_realloc(ctx, dest, char, size+1);

	talloc_set_name_const(dest, dest);

	return dest;
}

/**
 Convert a string to UPPER case, allocated with talloc
**/
_PUBLIC_ char *strupper_talloc(TALLOC_CTX *ctx, const char *src)
{
	return strupper_talloc_n(ctx, src, src?strlen(src):0);
}

/**
 talloc_strdup() a unix string to upper case.
**/
_PUBLIC_ char *talloc_strdup_upper(TALLOC_CTX *ctx, const char *src)
{
	return strupper_talloc(ctx, src);
}

/**
 Convert a string to lower case.
**/
_PUBLIC_ void strlower_m(char *s)
{
	char *d;
	struct smb_iconv_convenience *iconv_convenience;

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */
	while (*s && !(((uint8_t)*s) & 0x80)) {
		*s = tolower((uint8_t)*s);
		s++;
	}

	if (!*s)
		return;

	iconv_convenience = get_iconv_convenience();

	d = s;

	while (*s) {
		size_t c_size, c_size2;
		codepoint_t c = next_codepoint_convenience(iconv_convenience, s, &c_size);
		c_size2 = push_codepoint_convenience(iconv_convenience, d, tolower_m(c));
		if (c_size2 > c_size) {
			DEBUG(0,("FATAL: codepoint 0x%x (0x%x) expanded from %d to %d bytes in strlower_m\n",
				 c, tolower_m(c), (int)c_size, (int)c_size2));
			smb_panic("codepoint expansion in strlower_m\n");
		}
		s += c_size;
		d += c_size2;
	}
	*d = 0;
}

/**
 Convert a string to UPPER case.
**/
_PUBLIC_ void strupper_m(char *s)
{
	char *d;
	struct smb_iconv_convenience *iconv_convenience;

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */
	while (*s && !(((uint8_t)*s) & 0x80)) {
		*s = toupper((uint8_t)*s);
		s++;
	}

	if (!*s)
		return;

	iconv_convenience = get_iconv_convenience();

	d = s;

	while (*s) {
		size_t c_size, c_size2;
		codepoint_t c = next_codepoint_convenience(iconv_convenience, s, &c_size);
		c_size2 = push_codepoint_convenience(iconv_convenience, d, toupper_m(c));
		if (c_size2 > c_size) {
			DEBUG(0,("FATAL: codepoint 0x%x (0x%x) expanded from %d to %d bytes in strupper_m\n",
				 c, toupper_m(c), (int)c_size, (int)c_size2));
			smb_panic("codepoint expansion in strupper_m\n");
		}
		s += c_size;
		d += c_size2;
	}
	*d = 0;
}


/**
 Find the number of 'c' chars in a string
**/
_PUBLIC_ size_t count_chars_m(const char *s, char c)
{
	struct smb_iconv_convenience *ic = get_iconv_convenience();
	size_t count = 0;

	while (*s) {
		size_t size;
		codepoint_t c2 = next_codepoint_convenience(ic, s, &size);
		if (c2 == c) count++;
		s += size;
	}

	return count;
}


/**
 * Copy a string from a char* unix src to a dos codepage string destination.
 *
 * @return the number of bytes occupied by the string in the destination.
 *
 * @param flags can include
 * <dl>
 * <dt>STR_TERMINATE</dt> <dd>means include the null termination</dd>
 * <dt>STR_UPPER</dt> <dd>means uppercase in the destination</dd>
 * </dl>
 *
 * @param dest_len the maximum length in bytes allowed in the
 * destination.  If @p dest_len is -1 then no maximum is used.
 **/
static ssize_t push_ascii(void *dest, const char *src, size_t dest_len, int flags)
{
	size_t src_len;
	ssize_t ret;

	if (flags & STR_UPPER) {
		char *tmpbuf = strupper_talloc(NULL, src);
		if (tmpbuf == NULL) {
			return -1;
		}
		ret = push_ascii(dest, tmpbuf, dest_len, flags & ~STR_UPPER);
		talloc_free(tmpbuf);
		return ret;
	}

	src_len = strlen(src);

	if (flags & (STR_TERMINATE | STR_TERMINATE_ASCII))
		src_len++;

	return convert_string(CH_UNIX, CH_DOS, src, src_len, dest, dest_len, false);
}

/**
 * Copy a string from a unix char* src to an ASCII destination,
 * allocating a buffer using talloc().
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 *         or -1 in case of error.
 **/
_PUBLIC_ bool push_ascii_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size)
{
	size_t src_len = strlen(src)+1;
	*dest = NULL;
	return convert_string_talloc(ctx, CH_UNIX, CH_DOS, src, src_len, (void **)dest, converted_size, false);
}


/**
 * Copy a string from a dos codepage source to a unix char* destination.
 *
 * The resulting string in "dest" is always null terminated.
 *
 * @param flags can have:
 * <dl>
 * <dt>STR_TERMINATE</dt>
 * <dd>STR_TERMINATE means the string in @p src
 * is null terminated, and src_len is ignored.</dd>
 * </dl>
 *
 * @param src_len is the length of the source area in bytes.
 * @returns the number of bytes occupied by the string in @p src.
 **/
static ssize_t pull_ascii(char *dest, const void *src, size_t dest_len, size_t src_len, int flags)
{
	size_t ret;

	if (flags & (STR_TERMINATE | STR_TERMINATE_ASCII)) {
		if (src_len == (size_t)-1) {
			src_len = strlen((const char *)src) + 1;
		} else {
			size_t len = strnlen((const char *)src, src_len);
			if (len < src_len)
				len++;
			src_len = len;
		}
	}

	ret = convert_string(CH_DOS, CH_UNIX, src, src_len, dest, dest_len, false);

	if (dest_len)
		dest[MIN(ret, dest_len-1)] = 0;

	return src_len;
}

/**
 * Copy a string from a char* src to a unicode destination.
 *
 * @returns the number of bytes occupied by the string in the destination.
 *
 * @param flags can have:
 *
 * <dl>
 * <dt>STR_TERMINATE <dd>means include the null termination.
 * <dt>STR_UPPER     <dd>means uppercase in the destination.
 * <dt>STR_NOALIGN   <dd>means don't do alignment.
 * </dl>
 *
 * @param dest_len is the maximum length allowed in the
 * destination. If dest_len is -1 then no maxiumum is used.
 **/
static ssize_t push_ucs2(void *dest, const char *src, size_t dest_len, int flags)
{
	size_t len=0;
	size_t src_len = strlen(src);
	size_t ret;

	if (flags & STR_UPPER) {
		char *tmpbuf = strupper_talloc(NULL, src);
		if (tmpbuf == NULL) {
			return -1;
		}
		ret = push_ucs2(dest, tmpbuf, dest_len, flags & ~STR_UPPER);
		talloc_free(tmpbuf);
		return ret;
	}

	if (flags & STR_TERMINATE)
		src_len++;

	if (ucs2_align(NULL, dest, flags)) {
		*(char *)dest = 0;
		dest = (void *)((char *)dest + 1);
		if (dest_len) dest_len--;
		len++;
	}

	/* ucs2 is always a multiple of 2 bytes */
	dest_len &= ~1;

	ret = convert_string(CH_UNIX, CH_UTF16, src, src_len, dest, dest_len, false);
	if (ret == (size_t)-1) {
		return 0;
	}

	len += ret;

	return len;
}


/**
 * Copy a string from a unix char* src to a UCS2 destination,
 * allocating a buffer using talloc().
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 *         or -1 in case of error.
 **/
_PUBLIC_ bool push_ucs2_talloc(TALLOC_CTX *ctx, smb_ucs2_t **dest, const char *src, size_t *converted_size)
{
	size_t src_len = strlen(src)+1;
	*dest = NULL;
	return convert_string_talloc(ctx, CH_UNIX, CH_UTF16, src, src_len, (void **)dest, converted_size, false);
}


/**
 * Copy a string from a unix char* src to a UTF-8 destination, allocating a buffer using talloc
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 **/

_PUBLIC_ bool push_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size)
{
	size_t src_len = strlen(src)+1;
	*dest = NULL;
	return convert_string_talloc(ctx, CH_UNIX, CH_UTF8, src, src_len, (void **)dest, converted_size, false);
}

/**
 Copy a string from a ucs2 source to a unix char* destination.
 Flags can have:
  STR_TERMINATE means the string in src is null terminated.
  STR_NOALIGN   means don't try to align.
 if STR_TERMINATE is set then src_len is ignored if it is -1.
 src_len is the length of the source area in bytes
 Return the number of bytes occupied by the string in src.
 The resulting string in "dest" is always null terminated.
**/

static size_t pull_ucs2(char *dest, const void *src, size_t dest_len, size_t src_len, int flags)
{
	size_t ret;

	if (ucs2_align(NULL, src, flags)) {
		src = (const void *)((const char *)src + 1);
		if (src_len > 0)
			src_len--;
	}

	if (flags & STR_TERMINATE) {
		if (src_len == (size_t)-1) {
			src_len = utf16_len(src);
		} else {
			src_len = utf16_len_n(src, src_len);
		}
	}

	/* ucs2 is always a multiple of 2 bytes */
	if (src_len != (size_t)-1)
		src_len &= ~1;
	
	ret = convert_string(CH_UTF16, CH_UNIX, src, src_len, dest, dest_len, false);
	if (dest_len)
		dest[MIN(ret, dest_len-1)] = 0;

	return src_len;
}

/**
 * Copy a string from a ASCII src to a unix char * destination, allocating a buffer using talloc
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 **/

_PUBLIC_ bool pull_ascii_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size)
{
	size_t src_len = strlen(src)+1;
	*dest = NULL;
	return convert_string_talloc(ctx, CH_DOS, CH_UNIX, src, src_len, (void **)dest, converted_size, false);
}

/**
 * Copy a string from a UCS2 src to a unix char * destination, allocating a buffer using talloc
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 **/

_PUBLIC_ bool pull_ucs2_talloc(TALLOC_CTX *ctx, char **dest, const smb_ucs2_t *src, size_t *converted_size)
{
	size_t src_len = utf16_len(src);
	*dest = NULL;
	return convert_string_talloc(ctx, CH_UTF16, CH_UNIX, src, src_len, (void **)dest, converted_size, false);
}

/**
 * Copy a string from a UTF-8 src to a unix char * destination, allocating a buffer using talloc
 *
 * @param dest always set at least to NULL 
 *
 * @returns The number of bytes occupied by the string in the destination
 **/

_PUBLIC_ bool pull_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size)
{
	size_t src_len = strlen(src)+1;
	*dest = NULL;
	return convert_string_talloc(ctx, CH_UTF8, CH_UNIX, src, src_len, (void **)dest, converted_size, false);
}

/**
 Copy a string from a char* src to a unicode or ascii
 dos codepage destination choosing unicode or ascii based on the 
 flags in the SMB buffer starting at base_ptr.
 Return the number of bytes occupied by the string in the destination.
 flags can have:
  STR_TERMINATE means include the null termination.
  STR_UPPER     means uppercase in the destination.
  STR_ASCII     use ascii even with unicode packet.
  STR_NOALIGN   means don't do alignment.
 dest_len is the maximum length allowed in the destination. If dest_len
 is -1 then no maxiumum is used.
**/

_PUBLIC_ ssize_t push_string(void *dest, const char *src, size_t dest_len, int flags)
{
	if (flags & STR_ASCII) {
		return push_ascii(dest, src, dest_len, flags);
	} else if (flags & STR_UNICODE) {
		return push_ucs2(dest, src, dest_len, flags);
	} else {
		smb_panic("push_string requires either STR_ASCII or STR_UNICODE flag to be set");
		return -1;
	}
}


/**
 Copy a string from a unicode or ascii source (depending on
 the packet flags) to a char* destination.
 Flags can have:
  STR_TERMINATE means the string in src is null terminated.
  STR_UNICODE   means to force as unicode.
  STR_ASCII     use ascii even with unicode packet.
  STR_NOALIGN   means don't do alignment.
 if STR_TERMINATE is set then src_len is ignored is it is -1
 src_len is the length of the source area in bytes.
 Return the number of bytes occupied by the string in src.
 The resulting string in "dest" is always null terminated.
**/

_PUBLIC_ ssize_t pull_string(char *dest, const void *src, size_t dest_len, size_t src_len, int flags)
{
	if (flags & STR_ASCII) {
		return pull_ascii(dest, src, dest_len, src_len, flags);
	} else if (flags & STR_UNICODE) {
		return pull_ucs2(dest, src, dest_len, src_len, flags);
	} else {
		smb_panic("pull_string requires either STR_ASCII or STR_UNICODE flag to be set");
		return -1;
	}
}


/**
 * Convert string from one encoding to another, making error checking etc
 *
 * @param src pointer to source string (multibyte or singlebyte)
 * @param srclen length of the source string in bytes
 * @param dest pointer to destination string (multibyte or singlebyte)
 * @param destlen maximal length allowed for string
 * @returns the number of bytes occupied in the destination
 **/
_PUBLIC_ size_t convert_string(charset_t from, charset_t to,
				void const *src, size_t srclen, 
				void *dest, size_t destlen, 
				bool allow_badcharcnv)
{
	size_t ret;
	if (!convert_string_convenience(get_iconv_convenience(), from, to, 
									  src, srclen,
									  dest, destlen, &ret,
									  allow_badcharcnv))
		return -1;
	return ret;
}

/**
 * Convert between character sets, allocating a new buffer using talloc for the result.
 *
 * @param srclen length of source buffer.
 * @param dest always set at least to NULL
 * @param converted_size Size in bytes of the converted string
 * @note -1 is not accepted for srclen.
 *
 * @returns boolean indication whether the conversion succeeded
 **/

_PUBLIC_ bool convert_string_talloc(TALLOC_CTX *ctx, 
				       charset_t from, charset_t to, 
				       void const *src, size_t srclen, 
				       void *dest, size_t *converted_size, 
					   bool allow_badcharcnv)
{
	return convert_string_talloc_convenience(ctx, get_iconv_convenience(),
											 from, to, src, srclen, dest,
											 converted_size, 
											 allow_badcharcnv);
}

