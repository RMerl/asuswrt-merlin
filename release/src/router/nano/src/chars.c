/**************************************************************************
 *   chars.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014 Free Software Foundation, Inc.                *
 *   Copyright (C) 2016 Benno Schulenberg                                 *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <string.h>
#include <ctype.h>

#ifdef ENABLE_UTF8
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif

static bool use_utf8 = FALSE;
	/* Whether we've enabled UTF-8 support. */

/* Enable UTF-8 support. */
void utf8_init(void)
{
    use_utf8 = TRUE;
}

/* Is UTF-8 support enabled? */
bool using_utf8(void)
{
    return use_utf8;
}
#endif /* ENABLE_UTF8 */

/* Concatenate two allocated strings, and free the second. */
char *addstrings(char* str1, size_t len1, char* str2, size_t len2)
{
    str1 = charealloc(str1, len1 + len2 + 1);
    str1[len1] = '\0';

    strncat(&str1[len1], str2, len2);
    free(str2);

    return str1;
}

#ifndef HAVE_ISBLANK
/* This function is equivalent to isblank(). */
bool nisblank(int c)
{
    return isspace(c) && (c == '\t' || !is_cntrl_char(c));
}
#endif

#if !defined(HAVE_ISWBLANK) && defined(ENABLE_UTF8)
/* This function is equivalent to iswblank(). */
bool niswblank(wchar_t wc)
{
    return iswspace(wc) && (wc == '\t' || !is_cntrl_wchar(wc));
}
#endif

/* Return TRUE if the value of c is in byte range, and FALSE otherwise. */
bool is_byte(int c)
{
    return ((unsigned int)c == (unsigned char)c);
}

void mbtowc_reset(void)
{
    IGNORE_CALL_RESULT(mbtowc(NULL, NULL, 0));
}

void wctomb_reset(void)
{
    IGNORE_CALL_RESULT(wctomb(NULL, 0));
}

/* This function is equivalent to isalpha() for multibyte characters. */
bool is_alpha_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    return 0;
	}

	return iswalpha(wc);
    } else
#endif
	return isalpha((unsigned char)*c);
}

/* This function is equivalent to isalnum() for multibyte characters. */
bool is_alnum_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    return 0;
	}

	return iswalnum(wc);
    } else
#endif
	return isalnum((unsigned char)*c);
}

/* This function is equivalent to isblank() for multibyte characters. */
bool is_blank_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    return 0;
	}

	return iswblank(wc);
    } else
#endif
	return isblank((unsigned char)*c);
}

/* This function is equivalent to iscntrl(), except in that it only
 * handles non-high-bit control characters. */
bool is_ascii_cntrl_char(int c)
{
    return (0 <= c && c < 32);
}

/* This function is equivalent to iscntrl(), except in that it also
 * handles high-bit control characters. */
bool is_cntrl_char(int c)
{
    return ((c & 0x60) == 0 || c == 127);
}

/* This function is equivalent to iscntrl() for multibyte characters,
 * except in that it also handles multibyte control characters with
 * their high bits set. */
bool is_cntrl_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	return ((c[0] & 0xE0) == 0 || c[0] == 127 ||
		((signed char)c[0] == -62 && (signed char)c[1] < -96));
    } else
#endif
	return is_cntrl_char((unsigned char)*c);
}

/* This function is equivalent to ispunct() for multibyte characters. */
bool is_punct_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    return 0;
	}

	return iswpunct(wc);
    } else
#endif
	return ispunct((unsigned char)*c);
}

/* Return TRUE when the given multibyte character c is a word-forming
 * character (that is: alphanumeric, or specified in wordchars, or
 * punctuation when allow_punct is TRUE), and FALSE otherwise. */
bool is_word_mbchar(const char *c, bool allow_punct)
{
    assert(c != NULL);

    if (*c == '\0')
	return FALSE;

    if (is_alnum_mbchar(c))
	return TRUE;

    if (word_chars != NULL && *word_chars != '\0') {
	bool wordforming;
	char *symbol = charalloc(MB_CUR_MAX + 1);
	int symlen = parse_mbchar(c, symbol, NULL);

	symbol[symlen] = '\0';
	wordforming = (strstr(word_chars, symbol) != NULL);
	free(symbol);

	return wordforming;
    }

    return (allow_punct && is_punct_mbchar(c));
}

/* Return the visible representation of control character c. */
char control_rep(const signed char c)
{
    assert(is_cntrl_char(c));

    /* An embedded newline is an encoded null. */
    if (c == '\n')
	return '@';
    else if (c == DEL_CODE)
	return '?';
    else if (c == -97)
	return '=';
    else if (c < 0)
	return c + 224;
    else
	return c + 64;
}

/* Return the visible representation of multibyte control character c. */
char control_mbrep(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	if ((unsigned char)c[0] < 128)
	    return control_rep(c[0]);
	else
	    return control_rep(c[1]);
    } else
#endif
	return control_rep(*c);
}

/* Assess how many bytes the given (multibyte) character occupies.  Return -1
 * if the byte sequence is invalid, and return the number of bytes minus 8
 * when it encodes an invalid codepoint.  Also, in the second parameter,
 * return the number of columns that the character occupies. */
int length_of_char(const char *c, int *width)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;
	int charlen = mbtowc(&wc, c, MB_CUR_MAX);

	/* If the sequence is invalid... */
	if (charlen < 0) {
	    mbtowc_reset();
	    return -1;
	}

	/* If the codepoint is invalid... */
	if (!is_valid_unicode(wc))
	    return charlen - 8;
	else {
	    *width = wcwidth(wc);
	    /* If the codepoint is unassigned, assume a width of one. */
	    if (*width < 0)
		*width = 1;
	    return charlen;
	}
    } else
#endif
	return 1;
}

/* This function is equivalent to wcwidth() for multibyte characters. */
int mbwidth(const char *c)
{
    assert(c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc;
	int width;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    return 1;
	}

	width = wcwidth(wc);

	if (width == -1)
	    return 1;

	return width;
    } else
#endif
	return 1;
}

/* Return the maximum width in bytes of a multibyte character. */
int mb_cur_max(void)
{
    return
#ifdef ENABLE_UTF8
	use_utf8 ? MB_CUR_MAX :
#endif
	1;
}

/* Convert the Unicode value in chr to a multibyte character with the
 * same wide character value as chr, if possible.  If the conversion
 * succeeds, return the (dynamically allocated) multibyte character and
 * its length.  Otherwise, return an undefined (dynamically allocated)
 * multibyte character and a length of zero. */
char *make_mbchar(long chr, int *chr_mb_len)
{
    char *chr_mb;

    assert(chr_mb_len != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	chr_mb = charalloc(MB_CUR_MAX);
	*chr_mb_len = wctomb(chr_mb, (wchar_t)chr);

	/* Reject invalid Unicode characters. */
	if (*chr_mb_len < 0 || !is_valid_unicode((wchar_t)chr)) {
	    wctomb_reset();
	    *chr_mb_len = 0;
	}
    } else
#endif
    {
	*chr_mb_len = 1;
	chr_mb = mallocstrncpy(NULL, (char *)&chr, 1);
    }

    return chr_mb;
}

/* Parse a multibyte character from buf.  Return the number of bytes
 * used.  If chr isn't NULL, store the multibyte character in it.  If
 * col isn't NULL, store the new display width in it.  If *buf is '\t',
 * we expect col to have the current display width. */
int parse_mbchar(const char *buf, char *chr, size_t *col)
{
    int buf_mb_len;

    assert(buf != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	/* Get the number of bytes in the multibyte character. */
	buf_mb_len = mblen(buf, MB_CUR_MAX);

	/* When the multibyte sequence is invalid, only take the first byte. */
	if (buf_mb_len < 0) {
	    IGNORE_CALL_RESULT(mblen(NULL, 0));
	    buf_mb_len = 1;
	} else if (buf_mb_len == 0)
	    buf_mb_len++;

	/* When requested, store the multibyte character in chr. */
	if (chr != NULL) {
	    int i;

	    for (i = 0; i < buf_mb_len; i++)
		chr[i] = buf[i];
	}

	/* When requested, store the width of the wide character in col. */
	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (*buf == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, it's two columns wide: one
	     * column for the "^", and one for the visible character. */
	    else if (is_cntrl_mbchar(buf)) {
		*col += 2;
	    /* If we have a normal character, get its width in columns
	     * normally. */
	    } else
		*col += mbwidth(buf);
	}
    } else
#endif
    {
	/* A byte character is one byte long. */
	buf_mb_len = 1;

	/* When requested, store the byte character in chr. */
	if (chr != NULL)
	    *chr = *buf;

	/* When requested, store the width of the wide character in col. */
	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (*buf == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, it's two columns wide: one
	     * column for the "^", and one for the visible character. */
	    else if (is_cntrl_char((unsigned char)*buf))
		*col += 2;
	    /* If we have a normal character, it's one column wide. */
	    else
		(*col)++;
	}
    }

    return buf_mb_len;
}

/* Return the index in buf of the beginning of the multibyte character
 * before the one at pos. */
size_t move_mbleft(const char *buf, size_t pos)
{
    size_t before, char_len = 0;

    assert(buf != NULL && pos <= strlen(buf));

    /* There is no library function to move backward one multibyte
     * character.  So we just start groping for one at the farthest
     * possible point. */
    if (mb_cur_max() > pos)
	before = 0;
    else
	before = pos - mb_cur_max();

    while (before < pos) {
	char_len = parse_mbchar(buf + before, NULL, NULL);
	before += char_len;
    }

    return before - char_len;
}

/* Return the index in buf of the beginning of the multibyte character
 * after the one at pos. */
size_t move_mbright(const char *buf, size_t pos)
{
    return pos + parse_mbchar(buf + pos, NULL, NULL);
}

#ifndef HAVE_STRCASECMP
/* This function is equivalent to strcasecmp(). */
int nstrcasecmp(const char *s1, const char *s2)
{
    return strncasecmp(s1, s2, HIGHEST_POSITIVE);
}
#endif

/* This function is equivalent to strcasecmp() for multibyte strings. */
int mbstrcasecmp(const char *s1, const char *s2)
{
    return mbstrncasecmp(s1, s2, HIGHEST_POSITIVE);
}

#ifndef HAVE_STRNCASECMP
/* This function is equivalent to strncasecmp(). */
int nstrncasecmp(const char *s1, const char *s2, size_t n)
{
    if (s1 == s2)
	return 0;

    assert(s1 != NULL && s2 != NULL);

    for (; *s1 != '\0' && *s2 != '\0' && n > 0; s1++, s2++, n--) {
	if (tolower(*s1) != tolower(*s2))
	    break;
    }

    return (n > 0) ? tolower(*s1) - tolower(*s2) : 0;
}
#endif

/* This function is equivalent to strncasecmp() for multibyte strings. */
int mbstrncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef ENABLE_UTF8
    if (use_utf8) {
	wchar_t wc1, wc2;

	assert(s1 != NULL && s2 != NULL);

	while (*s1 != '\0' && *s2 != '\0' && n > 0) {
	    bool bad1 = FALSE, bad2 = FALSE;

	    if (mbtowc(&wc1, s1, MB_CUR_MAX) < 0) {
		mbtowc_reset();
		bad1 = TRUE;
	    }

	    if (mbtowc(&wc2, s2, MB_CUR_MAX) < 0) {
		mbtowc_reset();
		bad2 = TRUE;
	    }

	    if (bad1 || bad2) {
		if (*s1 != *s2)
		    return (unsigned char)*s1 - (unsigned char)*s2;

		if (bad1 != bad2)
		    return (bad1 ? 1 : -1);
	    } else {
		int difference = towlower(wc1) - towlower(wc2);

		if (difference != 0)
		    return difference;
	    }

	    s1 += move_mbright(s1, 0);
	    s2 += move_mbright(s2, 0);
	    n--;
	}

	return (n > 0) ? ((unsigned char)*s1 - (unsigned char)*s2) : 0;
    } else
#endif
	return strncasecmp(s1, s2, n);
}

#ifndef HAVE_STRCASESTR
/* This function is equivalent to strcasestr(). */
char *nstrcasestr(const char *haystack, const char *needle)
{
    size_t needle_len;

    assert(haystack != NULL && needle != NULL);

    if (*needle == '\0')
	return (char *)haystack;

    needle_len = strlen(needle);

    while (*haystack != '\0') {
	if (strncasecmp(haystack, needle, needle_len) == 0)
	    return (char *)haystack;

	haystack++;
    }

    return NULL;
}
#endif

/* This function is equivalent to strcasestr() for multibyte strings. */
char *mbstrcasestr(const char *haystack, const char *needle)
{
#ifdef ENABLE_UTF8
    if (use_utf8) {
	size_t needle_len;

	assert(haystack != NULL && needle != NULL);

	if (*needle == '\0')
	    return (char *)haystack;

	needle_len = mbstrlen(needle);

	while (*haystack != '\0') {
	    if (mbstrncasecmp(haystack, needle, needle_len) == 0)
		return (char *)haystack;

	    haystack += move_mbright(haystack, 0);
	}

	return NULL;
    } else
#endif
	return (char *) strcasestr(haystack, needle);
}

/* This function is equivalent to strstr(), except in that it scans the
 * string in reverse, starting at rev_start. */
char *revstrstr(const char *haystack, const char *needle, const char
	*rev_start)
{
    size_t rev_start_len, needle_len;

    assert(haystack != NULL && needle != NULL && rev_start != NULL);

    if (*needle == '\0')
	return (char *)rev_start;

    needle_len = strlen(needle);

    if (strlen(haystack) < needle_len)
	return NULL;

    rev_start_len = strlen(rev_start);

    for (; rev_start >= haystack; rev_start--, rev_start_len++) {
	if (rev_start_len >= needle_len && strncmp(rev_start, needle,
		needle_len) == 0)
	    return (char *)rev_start;
    }

    return NULL;
}

/* This function is equivalent to strcasestr(), except in that it scans
 * the string in reverse, starting at rev_start. */
char *revstrcasestr(const char *haystack, const char *needle, const char
	*rev_start)
{
    size_t rev_start_len, needle_len;

    assert(haystack != NULL && needle != NULL && rev_start != NULL);

    if (*needle == '\0')
	return (char *)rev_start;

    needle_len = strlen(needle);

    if (strlen(haystack) < needle_len)
	return NULL;

    rev_start_len = strlen(rev_start);

    for (; rev_start >= haystack; rev_start--, rev_start_len++) {
	if (rev_start_len >= needle_len && strncasecmp(rev_start,
		needle, needle_len) == 0)
	    return (char *)rev_start;
    }

    return NULL;
}

/* This function is equivalent to strcasestr() for multibyte strings,
 * except in that it scans the string in reverse, starting at rev_start. */
char *mbrevstrcasestr(const char *haystack, const char *needle, const
	char *rev_start)
{
#ifdef ENABLE_UTF8
    if (use_utf8) {
	size_t rev_start_len, needle_len;

	assert(haystack != NULL && needle != NULL && rev_start != NULL);

	if (*needle == '\0')
	    return (char *)rev_start;

	needle_len = mbstrlen(needle);

	if (mbstrlen(haystack) < needle_len)
	    return NULL;

	rev_start_len = mbstrlen(rev_start);

	while (TRUE) {
	    if (rev_start_len >= needle_len &&
			mbstrncasecmp(rev_start, needle, needle_len) == 0)
		return (char *)rev_start;

	    /* If we've reached the head of the haystack, we found nothing. */
	    if (rev_start == haystack)
		return NULL;

	    rev_start = haystack + move_mbleft(haystack, rev_start - haystack);
	    rev_start_len++;
	}
    } else
#endif
	return revstrcasestr(haystack, needle, rev_start);
}

/* This function is equivalent to strlen() for multibyte strings. */
size_t mbstrlen(const char *s)
{
    return mbstrnlen(s, (size_t)-1);
}

#ifndef HAVE_STRNLEN
/* This function is equivalent to strnlen(). */
size_t nstrnlen(const char *s, size_t maxlen)
{
    size_t n = 0;

    assert(s != NULL);

    for (; *s != '\0' && maxlen > 0; s++, maxlen--, n++)
	;

    return n;
}
#endif

/* This function is equivalent to strnlen() for multibyte strings. */
size_t mbstrnlen(const char *s, size_t maxlen)
{
    assert(s != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	size_t n = 0;

	for (; *s != '\0' && maxlen > 0; s += move_mbright(s, 0),
		maxlen--, n++)
	    ;

	return n;
    } else
#endif
	return strnlen(s, maxlen);
}

#if !defined(NANO_TINY) || !defined(DISABLE_JUSTIFY)
/* This function is equivalent to strchr() for multibyte strings. */
char *mbstrchr(const char *s, const char *c)
{
    assert(s != NULL && c != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	bool bad_s_mb = FALSE, bad_c_mb = FALSE;
	char *s_mb = charalloc(MB_CUR_MAX);
	const char *q = s;
	wchar_t ws, wc;

	if (mbtowc(&wc, c, MB_CUR_MAX) < 0) {
	    mbtowc_reset();
	    wc = (unsigned char)*c;
	    bad_c_mb = TRUE;
	}

	while (*s != '\0') {
	    int s_mb_len = parse_mbchar(s, s_mb, NULL);

	    if (mbtowc(&ws, s_mb, s_mb_len) < 0) {
		mbtowc_reset();
		ws = (unsigned char)*s;
		bad_s_mb = TRUE;
	    }

	    if (bad_s_mb == bad_c_mb && ws == wc)
		break;

	    s += s_mb_len;
	    q += s_mb_len;
	}

	free(s_mb);

	if (*s == '\0')
	    q = NULL;

	return (char *)q;
    } else
#endif
	return (char *) strchr(s, *c);
}
#endif /* !NANO_TINY || !DISABLE_JUSTIFY */

#ifndef NANO_TINY
/* This function is equivalent to strpbrk() for multibyte strings. */
char *mbstrpbrk(const char *s, const char *accept)
{
    assert(s != NULL && accept != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	for (; *s != '\0'; s += move_mbright(s, 0)) {
	    if (mbstrchr(accept, s) != NULL)
		return (char *)s;
	}

	return NULL;
    } else
#endif
	return (char *) strpbrk(s, accept);
}

/* This function is equivalent to strpbrk(), except in that it scans the
 * string in reverse, starting at rev_start. */
char *revstrpbrk(const char *s, const char *accept, const char
	*rev_start)
{
    assert(s != NULL && accept != NULL && rev_start != NULL);

    if (*rev_start == '\0') {
	if (rev_start == s)
	   return NULL;
	rev_start--;
    }

    for (; rev_start >= s; rev_start--) {
	if (strchr(accept, *rev_start) != NULL)
	    return (char *)rev_start;
    }

    return NULL;
}

/* This function is equivalent to strpbrk() for multibyte strings,
 * except in that it scans the string in reverse, starting at rev_start. */
char *mbrevstrpbrk(const char *s, const char *accept, const char
	*rev_start)
{
    assert(s != NULL && accept != NULL && rev_start != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	if (*rev_start == '\0') {
	    if (rev_start == s)
		return NULL;
	    rev_start = s + move_mbleft(s, rev_start - s);
	}

	while (TRUE) {
	    if (mbstrchr(accept, rev_start) != NULL)
		return (char *)rev_start;

	    /* If we've reached the head of the string, we found nothing. */
	    if (rev_start == s)
		return NULL;

	    rev_start = s + move_mbleft(s, rev_start - s);
	}
    } else
#endif
	return revstrpbrk(s, accept, rev_start);
}
#endif /* !NANO_TINY */

#if !defined(DISABLE_NANORC) && (!defined(NANO_TINY) || !defined(DISABLE_JUSTIFY))
/* Return TRUE if the string s contains one or more blank characters,
 * and FALSE otherwise. */
bool has_blank_chars(const char *s)
{
    assert(s != NULL);

    for (; *s != '\0'; s++) {
	if (isblank(*s))
	    return TRUE;
    }

    return FALSE;
}

/* Return TRUE if the multibyte string s contains one or more blank
 * multibyte characters, and FALSE otherwise. */
bool has_blank_mbchars(const char *s)
{
    assert(s != NULL);

#ifdef ENABLE_UTF8
    if (use_utf8) {
	bool retval = FALSE;
	char *chr_mb = charalloc(MB_CUR_MAX);

	for (; *s != '\0'; s += move_mbright(s, 0)) {
	    parse_mbchar(s, chr_mb, NULL);

	    if (is_blank_mbchar(chr_mb)) {
		retval = TRUE;
		break;
	    }
	}

	free(chr_mb);

	return retval;
    } else
#endif
	return has_blank_chars(s);
}
#endif /* !DISABLE_NANORC && (!NANO_TINY || !DISABLE_JUSTIFY) */

#ifdef ENABLE_UTF8
/* Return TRUE if wc is valid Unicode, and FALSE otherwise. */
bool is_valid_unicode(wchar_t wc)
{
    return ((0 <= wc && wc <= 0xD7FF) ||
		(0xE000 <= wc && wc <= 0xFDCF) ||
		(0xFDF0 <= wc && wc <= 0xFFFD) ||
		(0xFFFF < wc && wc <= 0x10FFFF && (wc & 0xFFFF) <= 0xFFFD));
}
#endif

#ifndef DISABLE_NANORC
/* Check if the string s is a valid multibyte string.  Return TRUE if it
 * is, and FALSE otherwise. */
bool is_valid_mbstring(const char *s)
{
    assert(s != NULL);

    return
#ifdef ENABLE_UTF8
	use_utf8 ? (mbstowcs(NULL, s, 0) != (size_t)-1) :
#endif
	TRUE;
}
#endif /* !DISABLE_NANORC */
