/**
 * unistr.c - Unicode string handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "attrib.h"
#include "endians.h"
#include "types.h"
#include "unistr.h"
#include "debug.h"
#include "logging.h"

/*
 * IMPORTANT
 * =========
 *
 * All these routines assume that the Unicode characters are in little endian
 * encoding inside the strings!!!
 */

/*
 * This is used by the name collation functions to quickly determine what
 * characters are (in)valid.
 */
#if 0
static const u8 legal_ansi_char_array[0x40] = {
	0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x17, 0x07, 0x18, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x18, 0x16, 0x16, 0x17, 0x07, 0x00,

	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x04, 0x16, 0x18, 0x16, 0x18, 0x18,
};
#endif

/**
 * ntfs_names_are_equal - compare two Unicode names for equality
 * @s1:			name to compare to @s2
 * @s1_len:		length in Unicode characters of @s1
 * @s2:			name to compare to @s1
 * @s2_len:		length in Unicode characters of @s2
 * @ic:			ignore case bool
 * @upcase:		upcase table (only if @ic == IGNORE_CASE)
 * @upcase_size:	length in Unicode characters of @upcase (if present)
 *
 * Compare the names @s1 and @s2 and return TRUE (1) if the names are
 * identical, or FALSE (0) if they are not identical. If @ic is IGNORE_CASE,
 * the @upcase table is used to perform a case insensitive comparison.
 */
BOOL ntfs_names_are_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len,
		const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_size)
{
	if (s1_len != s2_len)
		return FALSE;
	if (!s1_len)
		return TRUE;
	if (ic == CASE_SENSITIVE)
		return ntfs_ucsncmp(s1, s2, s1_len) ? FALSE: TRUE;
	return ntfs_ucsncasecmp(s1, s2, s1_len, upcase, upcase_size) ? FALSE:
								       TRUE;
}

/**
 * ntfs_names_collate - collate two Unicode names
 * @name1:	first Unicode name to compare
 * @name1_len:	length of first Unicode name to compare
 * @name2:	second Unicode name to compare
 * @name2_len:	length of second Unicode name to compare
 * @err_val:	if @name1 contains an invalid character return this value
 * @ic:		either CASE_SENSITIVE or IGNORE_CASE
 * @upcase:	upcase table (ignored if @ic is CASE_SENSITIVE)
 * @upcase_len:	upcase table size (ignored if @ic is CASE_SENSITIVE)
 *
 * ntfs_names_collate() collates two Unicode names and returns:
 *
 *  -1 if the first name collates before the second one,
 *   0 if the names match,
 *   1 if the second name collates before the first one, or
 * @err_val if an invalid character is found in @name1 during the comparison.
 *
 * The following characters are considered invalid: '"', '*', '<', '>' and '?'.
 */
int ntfs_names_collate(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len,
		const int err_val __attribute__((unused)),
		const IGNORE_CASE_BOOL ic, const ntfschar *upcase,
		const u32 upcase_len)
{
	u32 cnt;
	u16 c1, c2;

#ifdef DEBUG
	if (!name1 || !name2 || (ic && (!upcase || !upcase_len))) {
		ntfs_log_debug("ntfs_names_collate received NULL pointer!\n");
		exit(1);
	}
#endif
	for (cnt = 0; cnt < min(name1_len, name2_len); ++cnt) {
		c1 = le16_to_cpu(*name1);
		name1++;
		c2 = le16_to_cpu(*name2);
		name2++;
		if (ic) {
			if (c1 < upcase_len)
				c1 = le16_to_cpu(upcase[c1]);
			if (c2 < upcase_len)
				c2 = le16_to_cpu(upcase[c2]);
		}
#if 0
		if (c1 < 64 && legal_ansi_char_array[c1] & 8)
			return err_val;
#endif
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
	}
	if (name1_len < name2_len)
		return -1;
	if (name1_len == name2_len)
		return 0;
	/* name1_len > name2_len */
#if 0
	c1 = le16_to_cpu(*name1);
	if (c1 < 64 && legal_ansi_char_array[c1] & 8)
		return err_val;
#endif
	return 1;
}

/**
 * ntfs_ucsncmp - compare two little endian Unicode strings
 * @s1:		first string
 * @s2:		second string
 * @n:		maximum unicode characters to compare
 *
 * Compare the first @n characters of the Unicode strings @s1 and @s2,
 * The strings in little endian format and appropriate le16_to_cpu()
 * conversion is performed on non-little endian machines.
 *
 * The function returns an integer less than, equal to, or greater than zero
 * if @s1 (or the first @n Unicode characters thereof) is found, respectively,
 * to be less than, to match, or be greater than @s2.
 */
int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n)
{
	u16 c1, c2;
	size_t i;

#ifdef DEBUG
	if (!s1 || !s2) {
		ntfs_log_debug("ntfs_wcsncmp() received NULL pointer!\n");
		exit(1);
	}
#endif
	for (i = 0; i < n; ++i) {
		c1 = le16_to_cpu(s1[i]);
		c2 = le16_to_cpu(s2[i]);
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
		if (!c1)
			break;
	}
	return 0;
}

/**
 * ntfs_ucsncasecmp - compare two little endian Unicode strings, ignoring case
 * @s1:			first string
 * @s2:			second string
 * @n:			maximum unicode characters to compare
 * @upcase:		upcase table
 * @upcase_size:	upcase table size in Unicode characters
 *
 * Compare the first @n characters of the Unicode strings @s1 and @s2,
 * ignoring case. The strings in little endian format and appropriate
 * le16_to_cpu() conversion is performed on non-little endian machines.
 *
 * Each character is uppercased using the @upcase table before the comparison.
 *
 * The function returns an integer less than, equal to, or greater than zero
 * if @s1 (or the first @n Unicode characters thereof) is found, respectively,
 * to be less than, to match, or be greater than @s2.
 */
int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2, size_t n,
		const ntfschar *upcase, const u32 upcase_size)
{
	u16 c1, c2;
	size_t i;

#ifdef DEBUG
	if (!s1 || !s2 || !upcase) {
		ntfs_log_debug("ntfs_wcsncasecmp() received NULL pointer!\n");
		exit(1);
	}
#endif
	for (i = 0; i < n; ++i) {
		if ((c1 = le16_to_cpu(s1[i])) < upcase_size)
			c1 = le16_to_cpu(upcase[c1]);
		if ((c2 = le16_to_cpu(s2[i])) < upcase_size)
			c2 = le16_to_cpu(upcase[c2]);
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
		if (!c1)
			break;
	}
	return 0;
}

/**
 * ntfs_ucsnlen - determine the length of a little endian Unicode string
 * @s:		pointer to Unicode string
 * @maxlen:	maximum length of string @s
 *
 * Return the number of Unicode characters in the little endian Unicode
 * string @s up to a maximum of maxlen Unicode characters, not including
 * the terminating (ntfschar)'\0'. If there is no (ntfschar)'\0' between @s
 * and @s + @maxlen, @maxlen is returned.
 *
 * This function never looks beyond @s + @maxlen.
 */
u32 ntfs_ucsnlen(const ntfschar *s, u32 maxlen)
{
	u32 i;

	for (i = 0; i < maxlen; i++) {
		if (!le16_to_cpu(s[i]))
			break;
	}
	return i;
}

/**
 * ntfs_ucsndup - duplicate little endian Unicode string
 * @s:		pointer to Unicode string
 * @maxlen:	maximum length of string @s
 *
 * Return a pointer to a new little endian Unicode string which is a duplicate
 * of the string s.  Memory for the new string is obtained with malloc(3), and
 * can be freed with free(3).
 *
 * A maximum of @maxlen Unicode characters are copied and a terminating
 * (ntfschar)'\0' little endian Unicode character is added.
 *
 * This function never looks beyond @s + @maxlen.
 *
 * Return a pointer to the new little endian Unicode string on success and NULL
 * on failure with errno set to the error code.
 */
ntfschar *ntfs_ucsndup(const ntfschar *s, u32 maxlen)
{
	ntfschar *dst;
	u32 len;

	len = ntfs_ucsnlen(s, maxlen);
	dst = ntfs_malloc((len + 1) * sizeof(ntfschar));
	if (dst) {
		memcpy(dst, s, len * sizeof(ntfschar));
		dst[len] = 0;
	}
	return dst;
}

/**
 * ntfs_name_upcase - Map an Unicode name to its uppercase equivalent
 * @name:
 * @name_len:
 * @upcase:
 * @upcase_len:
 *
 * Description...
 *
 * Returns:
 */
void ntfs_name_upcase(ntfschar *name, u32 name_len, const ntfschar *upcase,
		const u32 upcase_len)
{
	u32 i;
	u16 u;

	for (i = 0; i < name_len; i++)
		if ((u = le16_to_cpu(name[i])) < upcase_len)
			name[i] = upcase[u];
}

/**
 * ntfs_file_value_upcase - Convert a filename to upper case
 * @file_name_attr:
 * @upcase:
 * @upcase_len:
 *
 * Description...
 *
 * Returns:
 */
void ntfs_file_value_upcase(FILE_NAME_ATTR *file_name_attr,
		const ntfschar *upcase, const u32 upcase_len)
{
	ntfs_name_upcase((ntfschar*)&file_name_attr->file_name,
			file_name_attr->file_name_length, upcase, upcase_len);
}

/**
 * ntfs_file_values_compare - Which of two filenames should be listed first
 * @file_name_attr1:
 * @file_name_attr2:
 * @err_val:
 * @ic:
 * @upcase:
 * @upcase_len:
 *
 * Description...
 *
 * Returns:
 */
int ntfs_file_values_compare(const FILE_NAME_ATTR *file_name_attr1,
		const FILE_NAME_ATTR *file_name_attr2,
		const int err_val, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len)
{
	return ntfs_names_collate((ntfschar*)&file_name_attr1->file_name,
			file_name_attr1->file_name_length,
			(ntfschar*)&file_name_attr2->file_name,
			file_name_attr2->file_name_length,
			err_val, ic, upcase, upcase_len);
}

/**
 * ntfs_ucstombs - convert a little endian Unicode string to a multibyte string
 * @ins:	input Unicode string buffer
 * @ins_len:	length of input string in Unicode characters
 * @outs:	on return contains the (allocated) output multibyte string
 * @outs_len:	length of output buffer in bytes
 *
 * Convert the input little endian, 2-byte Unicode string @ins, of length
 * @ins_len into the multibyte string format dictated by the current locale.
 *
 * If *@outs is NULL, the function allocates the string and the caller is
 * responsible for calling free(*@outs); when finished with it.
 *
 * On success the function returns the number of bytes written to the output
 * string *@outs (>= 0), not counting the terminating NULL byte. If the output
 * string buffer was allocated, *@outs is set to it.
 *
 * On error, -1 is returned, and errno is set to the error code. The following
 * error codes can be expected:
 *	EINVAL		Invalid arguments (e.g. @ins or @outs is NULL).
 *	EILSEQ		The input string cannot be represented as a multibyte
 *			sequence according to the current locale.
 *	ENAMETOOLONG	Destination buffer is too small for input string.
 *	ENOMEM		Not enough memory to allocate destination buffer.
 */
int ntfs_ucstombs(const ntfschar *ins, const int ins_len, char **outs,
		int outs_len)
{
	char *mbs;
	wchar_t wc;
	int i, o, mbs_len;
	int cnt = 0;
#ifdef HAVE_MBSINIT
	mbstate_t mbstate;
#endif

	if (!ins || !outs) {
		errno = EINVAL;
		return -1;
	}
	mbs = *outs;
	mbs_len = outs_len;
	if (mbs && !mbs_len) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if (!mbs) {
		mbs_len = (ins_len + 1) * MB_CUR_MAX;
		mbs = (char*)ntfs_malloc(mbs_len);
		if (!mbs)
			return -1;
	}
#ifdef HAVE_MBSINIT
	memset(&mbstate, 0, sizeof(mbstate));
#else
	wctomb(NULL, 0);
#endif
	for (i = o = 0; i < ins_len; i++) {
		/* Reallocate memory if necessary or abort. */
		if ((int)(o + MB_CUR_MAX) > mbs_len) {
			char *tc;
			if (mbs == *outs) {
				errno = ENAMETOOLONG;
				return -1;
			}
			tc = (char*)ntfs_malloc((mbs_len + 64) & ~63);
			if (!tc)
				goto err_out;
			memcpy(tc, mbs, mbs_len);
			mbs_len = (mbs_len + 64) & ~63;
			free(mbs);
			mbs = tc;
		}
		/* Convert the LE Unicode character to a CPU wide character. */
		wc = (wchar_t)le16_to_cpu(ins[i]);
		if (!wc)
			break;
		/* Convert the CPU endian wide character to multibyte. */
#ifdef HAVE_MBSINIT
		cnt = wcrtomb(mbs + o, wc, &mbstate);
#else
		cnt = wctomb(mbs + o, wc);
#endif
		if (cnt == -1)
			goto err_out;
		if (cnt <= 0) {
			ntfs_log_debug("Eeek. cnt <= 0, cnt = %i\n", cnt);
			errno = EINVAL;
			goto err_out;
		}
		o += cnt;
	}
#ifdef HAVE_MBSINIT
	/* Make sure we are back in the initial state. */
	if (!mbsinit(&mbstate)) {
		ntfs_log_debug("Eeek. mbstate not in initial state!\n");
		errno = EILSEQ;
		goto err_out;
	}
#endif
	/* Now write the NULL character. */
	mbs[o] = 0;
	if (*outs != mbs)
		*outs = mbs;
	return o;
err_out:
	if (mbs != *outs)
		free(mbs);
	return -1;
}

/**
 * ntfs_mbstoucs - convert a multibyte string to a little endian Unicode string
 * @ins:	input multibyte string buffer
 * @outs:	on return contains the (allocated) output Unicode string
 * @outs_len:	length of output buffer in Unicode characters
 *
 * Convert the input multibyte string @ins, from the current locale into the
 * corresponding little endian, 2-byte Unicode string.
 *
 * If *@outs is NULL, the function allocates the string and the caller is
 * responsible for calling free(*@outs); when finished with it.
 *
 * On success the function returns the number of Unicode characters written to
 * the output string *@outs (>= 0), not counting the terminating Unicode NULL
 * character. If the output string buffer was allocated, *@outs is set to it.
 *
 * On error, -1 is returned, and errno is set to the error code. The following
 * error codes can be expected:
 *	EINVAL		Invalid arguments (e.g. @ins or @outs is NULL).
 *	EILSEQ		The input string cannot be represented as a Unicode
 *			string according to the current locale.
 *	ENAMETOOLONG	Destination buffer is too small for input string.
 *	ENOMEM		Not enough memory to allocate destination buffer.
 */
int ntfs_mbstoucs(const char *ins, ntfschar **outs, int outs_len)
{
	ntfschar *ucs;
	const char *s;
	wchar_t wc;
	int i, o, cnt, ins_len, ucs_len, ins_size;
#ifdef HAVE_MBSINIT
	mbstate_t mbstate;
#endif

	if (!ins || !outs) {
		errno = EINVAL;
		return -1;
	}
	ucs = *outs;
	ucs_len = outs_len;
	if (ucs && !ucs_len) {
		errno = ENAMETOOLONG;
		return -1;
	}
	/* Determine the size of the multi-byte string in bytes. */
	ins_size = strlen(ins);
	/* Determine the length of the multi-byte string. */
	s = ins;
#if defined(HAVE_MBSINIT)
	memset(&mbstate, 0, sizeof(mbstate));
	ins_len = mbsrtowcs(NULL, (const char **)&s, 0, &mbstate);
#ifdef __CYGWIN32__
	if (!ins_len && *ins) {
		/* Older Cygwin had broken mbsrtowcs() implementation. */
		ins_len = strlen(ins);
	}
#endif
#elif !defined(DJGPP)
	ins_len = mbstowcs(NULL, s, 0);
#else
	/* Eeek!!! DJGPP has broken mbstowcs() implementation!!! */
	ins_len = strlen(ins);
#endif
	if (ins_len == -1)
		return ins_len;
#ifdef HAVE_MBSINIT
	if ((s != ins) || !mbsinit(&mbstate)) {
#else
	if (s != ins) {
#endif
		errno = EILSEQ;
		return -1;
	}
	/* Add the NULL terminator. */
	ins_len++;
	if (!ucs) {
		ucs_len = ins_len;
		ucs = (ntfschar*)ntfs_malloc(ucs_len * sizeof(ntfschar));
		if (!ucs)
			return -1;
	}
#ifdef HAVE_MBSINIT
	memset(&mbstate, 0, sizeof(mbstate));
#else
	mbtowc(NULL, NULL, 0);
#endif
	for (i = o = cnt = 0; i < ins_size; i += cnt, o++) {
		/* Reallocate memory if necessary or abort. */
		if (o >= ucs_len) {
			ntfschar *tc;
			if (ucs == *outs) {
				errno = ENAMETOOLONG;
				return -1;
			}
			/*
			 * We will never get here but hey, it's only a bit of
			 * extra code...
			 */
			ucs_len = (ucs_len * sizeof(ntfschar) + 64) & ~63;
			tc = (ntfschar*)realloc(ucs, ucs_len);
			if (!tc)
				goto err_out;
			ucs = tc;
			ucs_len /= sizeof(ntfschar);
		}
		/* Convert the multibyte character to a wide character. */
#ifdef HAVE_MBSINIT
		cnt = mbrtowc(&wc, ins + i, ins_size - i, &mbstate);
#else
		cnt = mbtowc(&wc, ins + i, ins_size - i);
#endif
		if (!cnt)
			break;
		if (cnt == -1)
			goto err_out;
		if (cnt < -1) {
			ntfs_log_trace("Eeek. cnt = %i\n", cnt);
			errno = EINVAL;
			goto err_out;
		}
		/* Make sure we are not overflowing the NTFS Unicode set. */
		if ((unsigned long)wc >= (unsigned long)(1 <<
				(8 * sizeof(ntfschar)))) {
			errno = EILSEQ;
			goto err_out;
		}
		/* Convert the CPU wide character to a LE Unicode character. */
		ucs[o] = cpu_to_le16(wc);
	}
#ifdef HAVE_MBSINIT
	/* Make sure we are back in the initial state. */
	if (!mbsinit(&mbstate)) {
		ntfs_log_trace("Eeek. mbstate not in initial state!\n");
		errno = EILSEQ;
		goto err_out;
	}
#endif
	/* Now write the NULL character. */
	ucs[o] = 0;
	if (*outs != ucs)
		*outs = ucs;
	return o;
err_out:
	if (ucs != *outs)
		free(ucs);
	return -1;
}

/**
 * ntfs_upcase_table_build - build the default upcase table for NTFS
 * @uc:		destination buffer where to store the built table
 * @uc_len:	size of destination buffer in bytes
 *
 * ntfs_upcase_table_build() builds the default upcase table for NTFS and
 * stores it in the caller supplied buffer @uc of size @uc_len.
 *
 * The generated $UpCase table is the one used by Windows Vista.
 *
 * Note, @uc_len must be at least 128kiB in size or bad things will happen!
 */
void ntfs_upcase_table_build(ntfschar *uc, u32 uc_len)
{
	/*
	 * "Start" is inclusive and "End" is exclusive, every value has the
	 * value of "Add" added to it.
	 */
	static int add[][3] = { /* Start, End, Add */
	{0x0061, 0x007b,   -32}, {0x00e0, 0x00f7,  -32}, {0x00f8, 0x00ff, -32}, 
	{0x0256, 0x0258,  -205}, {0x028a, 0x028c, -217}, {0x037b, 0x037e, 130}, 
	{0x03ac, 0x03ad,   -38}, {0x03ad, 0x03b0,  -37}, {0x03b1, 0x03c2, -32},
	{0x03c2, 0x03c3,   -31}, {0x03c3, 0x03cc,  -32}, {0x03cc, 0x03cd, -64},
	{0x03cd, 0x03cf,   -63}, {0x0430, 0x0450,  -32}, {0x0450, 0x0460, -80},
	{0x0561, 0x0587,   -48}, {0x1f00, 0x1f08,    8}, {0x1f10, 0x1f16,   8},
	{0x1f20, 0x1f28,     8}, {0x1f30, 0x1f38,    8}, {0x1f40, 0x1f46,   8},
	{0x1f51, 0x1f52,     8}, {0x1f53, 0x1f54,    8}, {0x1f55, 0x1f56,   8},
	{0x1f57, 0x1f58,     8}, {0x1f60, 0x1f68,    8}, {0x1f70, 0x1f72,  74},
	{0x1f72, 0x1f76,    86}, {0x1f76, 0x1f78,  100}, {0x1f78, 0x1f7a, 128},
	{0x1f7a, 0x1f7c,   112}, {0x1f7c, 0x1f7e,  126}, {0x1f80, 0x1f88,   8},
	{0x1f90, 0x1f98,     8}, {0x1fa0, 0x1fa8,    8}, {0x1fb0, 0x1fb2,   8},
	{0x1fb3, 0x1fb4,     9}, {0x1fcc, 0x1fcd,   -9}, {0x1fd0, 0x1fd2,   8},
	{0x1fe0, 0x1fe2,     8}, {0x1fe5, 0x1fe6,    7}, {0x1ffc, 0x1ffd,  -9},
	{0x2170, 0x2180,   -16}, {0x24d0, 0x24ea,  -26}, {0x2c30, 0x2c5f, -48},
	{0x2d00, 0x2d26, -7264}, {0xff41, 0xff5b,  -32}, {0}
	};
	/*
	 * "Start" is exclusive and "End" is inclusive, every second value is
	 * decremented by one.
	 */
	static int skip_dec[][2] = { /* Start, End */
	{0x0100, 0x012f}, {0x0132, 0x0137}, {0x0139, 0x0149}, {0x014a, 0x0178},
	{0x0179, 0x017e}, {0x01a0, 0x01a6}, {0x01b3, 0x01b7}, {0x01cd, 0x01dd},
	{0x01de, 0x01ef}, {0x01f4, 0x01f5}, {0x01f8, 0x01f9}, {0x01fa, 0x0220},
	{0x0222, 0x0234}, {0x023b, 0x023c}, {0x0241, 0x0242}, {0x0246, 0x024f},
	{0x03d8, 0x03ef}, {0x03f7, 0x03f8}, {0x03fa, 0x03fb}, {0x0460, 0x0481},
	{0x048a, 0x04bf}, {0x04c1, 0x04c4}, {0x04c5, 0x04c8}, {0x04c9, 0x04ce},
	{0x04ec, 0x04ed}, {0x04d0, 0x04eb}, {0x04ee, 0x04f5}, {0x04f6, 0x0513},
	{0x1e00, 0x1e95}, {0x1ea0, 0x1ef9}, {0x2183, 0x2184}, {0x2c60, 0x2c61},
	{0x2c67, 0x2c6c}, {0x2c75, 0x2c76}, {0x2c80, 0x2ce3}, {0}
	};
	/*
	 * Set the Unicode character at offset "Offset" to "Value".  Note,
	 * "Value" is host endian.
	 */
	static int set[][2] = { /* Offset, Value */
	{0x00ff, 0x0178}, {0x0180, 0x0243}, {0x0183, 0x0182}, {0x0185, 0x0184},
	{0x0188, 0x0187}, {0x018c, 0x018b}, {0x0192, 0x0191}, {0x0195, 0x01f6},
	{0x0199, 0x0198}, {0x019a, 0x023d}, {0x019e, 0x0220}, {0x01a8, 0x01a7},
	{0x01ad, 0x01ac}, {0x01b0, 0x01af}, {0x01b9, 0x01b8}, {0x01bd, 0x01bc},
	{0x01bf, 0x01f7}, {0x01c6, 0x01c4}, {0x01c9, 0x01c7}, {0x01cc, 0x01ca},
	{0x01dd, 0x018e}, {0x01f3, 0x01f1}, {0x023a, 0x2c65}, {0x023e, 0x2c66},
	{0x0253, 0x0181}, {0x0254, 0x0186}, {0x0259, 0x018f}, {0x025b, 0x0190},
	{0x0260, 0x0193}, {0x0263, 0x0194}, {0x0268, 0x0197}, {0x0269, 0x0196},
	{0x026b, 0x2c62}, {0x026f, 0x019c}, {0x0272, 0x019d}, {0x0275, 0x019f},
	{0x027d, 0x2c64}, {0x0280, 0x01a6}, {0x0283, 0x01a9}, {0x0288, 0x01ae},
	{0x0289, 0x0244}, {0x028c, 0x0245}, {0x0292, 0x01b7}, {0x03f2, 0x03f9},
	{0x04cf, 0x04c0}, {0x1d7d, 0x2c63}, {0x214e, 0x2132}, {0}
	};
	unsigned i, r;

	memset(uc, 0, uc_len);
	uc_len /= 2;
	/* Start with a one-to-one mapping, i.e. no upcasing happens at all. */
	for (i = 0; i < uc_len; i++)
		uc[i] = cpu_to_le16(i);
	/* Adjust specified runs by the specified amount. */
	for (r = 0; add[r][0]; r++)
		for (i = add[r][0]; i < add[r][1]; i++)
			uc[i] = cpu_to_le16(le16_to_cpu(uc[i]) + add[r][2]);
	/* Decrement every second value in specified runs. */
	for (r = 0; skip_dec[r][0]; r++)
		for (i = skip_dec[r][0]; i < skip_dec[r][1];
				i += 2)
			uc[i + 1] = cpu_to_le16(le16_to_cpu(uc[i + 1]) - 1);
	/* Set specified characters to specified values. */
	for (r = 0; set[r][0]; r++)
		uc[set[r][0]] = cpu_to_le16(set[r][1]);
}

/**
 * ntfs_str2ucs - convert a string to a valid NTFS file name
 * @s:		input string
 * @len:	length of output buffer in Unicode characters
 *
 * Convert the input @s string into the corresponding little endian,
 * 2-byte Unicode string. The length of the converted string is less
 * or equal to the maximum length allowed by the NTFS format (255).
 *
 * If @s is NULL then return AT_UNNAMED.
 *
 * On success the function returns the Unicode string in an allocated
 * buffer and the caller is responsible to free it when it's not needed
 * anymore.
 *
 * On error NULL is returned and errno is set to the error code.
 */
ntfschar *ntfs_str2ucs(const char *s, int *len)
{
	ntfschar *ucs = NULL;

	if (s && ((*len = ntfs_mbstoucs(s, &ucs, 0)) == -1)) {
		ntfs_log_perror("Couldn't convert '%s' to Unicode", s);
		return NULL;
	}
	if (*len > NTFS_MAX_NAME_LEN) {
		free(ucs);
		errno = ENAMETOOLONG;
		return NULL;
	}
	if (!ucs || !*len) {
		ucs  = AT_UNNAMED;
		*len = 0;
	}
	return ucs;
}

/**
 * ntfs_ucsfree - free memory allocated by ntfs_str2ucs()
 * @ucs:	input string to be freed
 *
 * Free memory at @ucs and which was allocated by ntfs_str2ucs.
 *
 * Return value: none.
 */
void ntfs_ucsfree(ntfschar *ucs)
{
	if (ucs && (ucs != AT_UNNAMED))
		free(ucs);
}

