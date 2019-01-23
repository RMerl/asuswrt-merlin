/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Simo Sorce      2001-2002
   Copyright (C) Martin Pool     2003
   Copyright (C) James Peach	 2006
   Copyright (C) Jeremy Allison  1992-2007

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

const char toupper_ascii_fast_table[128] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

/**
 * Case insensitive string compararison.
 *
 * iconv does not directly give us a way to compare strings in
 * arbitrary unix character sets -- all we can is convert and then
 * compare.  This is expensive.
 *
 * As an optimization, we do a first pass that considers only the
 * prefix of the strings that is entirely 7-bit.  Within this, we
 * check whether they have the same value.
 *
 * Hopefully this will often give the answer without needing to copy.
 * In particular it should speed comparisons to literal ascii strings
 * or comparisons of strings that are "obviously" different.
 *
 * If we find a non-ascii character we fall back to converting via
 * iconv.
 *
 * This should never be slower than convering the whole thing, and
 * often faster.
 *
 * A different optimization would be to compare for bitwise equality
 * in the binary encoding.  (It would be possible thought hairy to do
 * both simultaneously.)  But in that case if they turn out to be
 * different, we'd need to restart the whole thing.
 *
 * Even better is to implement strcasecmp for each encoding and use a
 * function pointer.
 **/
int StrCaseCmp(const char *s, const char *t)
{

	const char *ps, *pt;
	size_t size;
	smb_ucs2_t *buffer_s, *buffer_t;
	int ret;

	for (ps = s, pt = t; ; ps++, pt++) {
		char us, ut;

		if (!*ps && !*pt)
			return 0; /* both ended */
 		else if (!*ps)
			return -1; /* s is a prefix */
		else if (!*pt)
			return +1; /* t is a prefix */
		else if ((*ps & 0x80) || (*pt & 0x80))
			/* not ascii anymore, do it the hard way
			 * from here on in */
			break;

		us = toupper_ascii_fast(*ps);
		ut = toupper_ascii_fast(*pt);
		if (us == ut)
			continue;
		else if (us < ut)
			return -1;
		else if (us > ut)
			return +1;
	}

	if (!push_ucs2_talloc(talloc_tos(), &buffer_s, ps, &size)) {
		return strcmp(ps, pt);
		/* Not quite the right answer, but finding the right one
		   under this failure case is expensive, and it's pretty
		   close */
	}

	if (!push_ucs2_talloc(talloc_tos(), &buffer_t, pt, &size)) {
		TALLOC_FREE(buffer_s);
		return strcmp(ps, pt);
		/* Not quite the right answer, but finding the right one
		   under this failure case is expensive, and it's pretty
		   close */
	}

	ret = strcasecmp_w(buffer_s, buffer_t);
	TALLOC_FREE(buffer_s);
	TALLOC_FREE(buffer_t);
	return ret;
}


/**
 Case insensitive string compararison, length limited.
**/
int StrnCaseCmp(const char *s, const char *t, size_t len)
{
	size_t n = 0;
	const char *ps, *pt;
	size_t size;
	smb_ucs2_t *buffer_s, *buffer_t;
	int ret;

	for (ps = s, pt = t; n < len ; ps++, pt++, n++) {
		char us, ut;

		if (!*ps && !*pt)
			return 0; /* both ended */
 		else if (!*ps)
			return -1; /* s is a prefix */
		else if (!*pt)
			return +1; /* t is a prefix */
		else if ((*ps & 0x80) || (*pt & 0x80))
			/* not ascii anymore, do it the
			 * hard way from here on in */
			break;

		us = toupper_ascii_fast(*ps);
		ut = toupper_ascii_fast(*pt);
		if (us == ut)
			continue;
		else if (us < ut)
			return -1;
		else if (us > ut)
			return +1;
	}

	if (n == len) {
		return 0;
	}

	if (!push_ucs2_talloc(talloc_tos(), &buffer_s, ps, &size)) {
		return strncmp(ps, pt, len-n);
		/* Not quite the right answer, but finding the right one
		   under this failure case is expensive,
		   and it's pretty close */
	}

	if (!push_ucs2_talloc(talloc_tos(), &buffer_t, pt, &size)) {
		TALLOC_FREE(buffer_s);
		return strncmp(ps, pt, len-n);
		/* Not quite the right answer, but finding the right one
		   under this failure case is expensive,
		   and it's pretty close */
	}

	ret = strncasecmp_w(buffer_s, buffer_t, len-n);
	TALLOC_FREE(buffer_s);
	TALLOC_FREE(buffer_t);
	return ret;
}

/**
 * Compare 2 strings.
 *
 * @note The comparison is case-insensitive.
 **/
bool strequal(const char *s1, const char *s2)
{
	if (s1 == s2)
		return(true);
	if (!s1 || !s2)
		return(false);

	return(StrCaseCmp(s1,s2)==0);
}

/**
 * Compare 2 strings up to and including the nth char.
 *
 * @note The comparison is case-insensitive.
 **/
bool strnequal(const char *s1,const char *s2,size_t n)
{
	if (s1 == s2)
		return(true);
	if (!s1 || !s2 || !n)
		return(false);

	return(StrnCaseCmp(s1,s2,n)==0);
}

/**
 Compare 2 strings (case sensitive).
**/

bool strcsequal(const char *s1,const char *s2)
{
	if (s1 == s2)
		return(true);
	if (!s1 || !s2)
		return(false);

	return(strcmp(s1,s2)==0);
}

/**
Do a case-insensitive, whitespace-ignoring string compare.
**/

int strwicmp(const char *psz1, const char *psz2)
{
	/* if BOTH strings are NULL, return TRUE, if ONE is NULL return */
	/* appropriate value. */
	if (psz1 == psz2)
		return (0);
	else if (psz1 == NULL)
		return (-1);
	else if (psz2 == NULL)
		return (1);

	/* sync the strings on first non-whitespace */
	while (1) {
		while (isspace((int)*psz1))
			psz1++;
		while (isspace((int)*psz2))
			psz2++;
		if (toupper_m(*psz1) != toupper_m(*psz2) ||
				*psz1 == '\0' || *psz2 == '\0')
			break;
		psz1++;
		psz2++;
	}
	return (*psz1 - *psz2);
}

/**
 Convert a string to "normal" form.
**/

void strnorm(char *s, int case_default)
{
	if (case_default == CASE_UPPER)
		strupper_m(s);
	else
		strlower_m(s);
}

/**
 Check if a string is in "normal" case.
**/

bool strisnormal(const char *s, int case_default)
{
	if (case_default == CASE_UPPER)
		return(!strhaslower(s));

	return(!strhasupper(s));
}


/**
 String replace.
 NOTE: oldc and newc must be 7 bit characters
**/
void string_replace( char *s, char oldc, char newc )
{
	char *p;

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */

	for (p = s; *p; p++) {
		if (*p & 0x80) /* mb string - slow path. */
			break;
		if (*p == oldc) {
			*p = newc;
		}
	}

	if (!*p)
		return;

	/* Slow (mb) path. */
#ifdef BROKEN_UNICODE_COMPOSE_CHARACTERS
	/* With compose characters we must restart from the beginning. JRA. */
	p = s;
#endif

	while (*p) {
		size_t c_size;
		next_codepoint(p, &c_size);

		if (c_size == 1) {
			if (*p == oldc) {
				*p = newc;
			}
		}
		p += c_size;
	}
}

/**
 *  Skip past some strings in a buffer - old version - no checks.
 *  **/

char *push_skip_string(char *buf)
{
	buf += strlen(buf) + 1;
	return(buf);
}

/**
 Skip past a string in a buffer. Buffer may not be
 null terminated. end_ptr points to the first byte after
 then end of the buffer.
**/

char *skip_string(const char *base, size_t len, char *buf)
{
	const char *end_ptr = base + len;

	if (end_ptr < base || !base || !buf || buf >= end_ptr) {
		return NULL;
	}

	/* Skip the string */
	while (*buf) {
		buf++;
		if (buf >= end_ptr) {
			return NULL;
		}
	}
	/* Skip the '\0' */
	buf++;
	return buf;
}

/**
 Count the number of characters in a string. Normally this will
 be the same as the number of bytes in a string for single byte strings,
 but will be different for multibyte.
**/

size_t str_charnum(const char *s)
{
	size_t ret, converted_size;
	smb_ucs2_t *tmpbuf2 = NULL;
	if (!push_ucs2_talloc(talloc_tos(), &tmpbuf2, s, &converted_size)) {
		return 0;
	}
	ret = strlen_w(tmpbuf2);
	TALLOC_FREE(tmpbuf2);
	return ret;
}

/**
 Count the number of characters in a string. Normally this will
 be the same as the number of bytes in a string for single byte strings,
 but will be different for multibyte.
**/

size_t str_ascii_charnum(const char *s)
{
	size_t ret, converted_size;
	char *tmpbuf2 = NULL;
	if (!push_ascii_talloc(talloc_tos(), &tmpbuf2, s, &converted_size)) {
		return 0;
	}
	ret = strlen(tmpbuf2);
	TALLOC_FREE(tmpbuf2);
	return ret;
}

bool trim_char(char *s,char cfront,char cback)
{
	bool ret = false;
	char *ep;
	char *fp = s;

	/* Ignore null or empty strings. */
	if (!s || (s[0] == '\0'))
		return false;

	if (cfront) {
		while (*fp && *fp == cfront)
			fp++;
		if (!*fp) {
			/* We ate the string. */
			s[0] = '\0';
			return true;
		}
		if (fp != s)
			ret = true;
	}

	ep = fp + strlen(fp) - 1;
	if (cback) {
		/* Attempt ascii only. Bail for mb strings. */
		while ((ep >= fp) && (*ep == cback)) {
			ret = true;
			if ((ep > fp) && (((unsigned char)ep[-1]) & 0x80)) {
				/* Could be mb... bail back to tim_string. */
				char fs[2], bs[2];
				if (cfront) {
					fs[0] = cfront;
					fs[1] = '\0';
				}
				bs[0] = cback;
				bs[1] = '\0';
				return trim_string(s, cfront ? fs : NULL, bs);
			} else {
				ep--;
			}
		}
		if (ep < fp) {
			/* We ate the string. */
			s[0] = '\0';
			return true;
		}
	}

	ep[1] = '\0';
	memmove(s, fp, ep-fp+2);
	return ret;
}

/**
 Does a string have any uppercase chars in it?
**/

bool strhasupper(const char *s)
{
	smb_ucs2_t *tmp, *p;
	bool ret;
	size_t converted_size;

	if (!push_ucs2_talloc(talloc_tos(), &tmp, s, &converted_size)) {
		return false;
	}

	for(p = tmp; *p != 0; p++) {
		if(isupper_w(*p)) {
			break;
		}
	}

	ret = (*p != 0);
	TALLOC_FREE(tmp);
	return ret;
}

/**
 Does a string have any lowercase chars in it?
**/

bool strhaslower(const char *s)
{
	smb_ucs2_t *tmp, *p;
	bool ret;
	size_t converted_size;

	if (!push_ucs2_talloc(talloc_tos(), &tmp, s, &converted_size)) {
		return false;
	}

	for(p = tmp; *p != 0; p++) {
		if(islower_w(*p)) {
			break;
		}
	}

	ret = (*p != 0);
	TALLOC_FREE(tmp);
	return ret;
}

/**
 Safe string copy into a known length string. maxlength does not
 include the terminating zero.
**/

char *safe_strcpy_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		size_t maxlength)
{
	size_t len;

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in safe_strcpy, "
			"called from [%s][%d]\n", fn, line));
		return NULL;
	}

#ifdef DEVELOPER
	clobber_region(fn,line,dest, maxlength+1);
#endif

	if (!src) {
		*dest = 0;
		return dest;
	}

	len = strnlen(src, maxlength+1);

	if (len > maxlength) {
		DEBUG(0,("ERROR: string overflow by "
			"%lu (%lu - %lu) in safe_strcpy [%.50s]\n",
			 (unsigned long)(len-maxlength), (unsigned long)len,
			 (unsigned long)maxlength, src));
		len = maxlength;
	}

	memmove(dest, src, len);
	dest[len] = 0;
	return dest;
}

/**
 Safe string cat into a string. maxlength does not
 include the terminating zero.
**/
char *safe_strcat_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		size_t maxlength)
{
	size_t src_len, dest_len;

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in safe_strcat, "
			"called from [%s][%d]\n", fn, line));
		return NULL;
	}

	if (!src)
		return dest;

	src_len = strnlen(src, maxlength + 1);
	dest_len = strnlen(dest, maxlength + 1);

#ifdef DEVELOPER
	clobber_region(fn, line, dest + dest_len, maxlength + 1 - dest_len);
#endif

	if (src_len + dest_len > maxlength) {
		DEBUG(0,("ERROR: string overflow by %d "
			"in safe_strcat [%.50s]\n",
			 (int)(src_len + dest_len - maxlength), src));
		if (maxlength > dest_len) {
			memcpy(&dest[dest_len], src, maxlength - dest_len);
		}
		dest[maxlength] = 0;
		return NULL;
	}

	memcpy(&dest[dest_len], src, src_len);
	dest[dest_len + src_len] = 0;
	return dest;
}

/**
 Paranoid strcpy into a buffer of given length (includes terminating
 zero. Strips out all but 'a-Z0-9' and the character in other_safe_chars
 and replaces with '_'. Deliberately does *NOT* check for multibyte
 characters. Treats src as an array of bytes, not as a multibyte
 string. Any byte >0x7f is automatically converted to '_'.
 other_safe_chars must also contain an ascii string (bytes<0x7f).
**/

char *alpha_strcpy_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		const char *other_safe_chars,
		size_t maxlength)
{
	size_t len, i;

#ifdef DEVELOPER
	clobber_region(fn, line, dest, maxlength);
#endif

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in alpha_strcpy, "
			"called from [%s][%d]\n", fn, line));
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
		if (val > 0x7f) {
			dest[i] = '_';
			continue;
		}
		if (isupper(val) || islower(val) ||
				isdigit(val) || strchr(other_safe_chars, val))
			dest[i] = src[i];
		else
			dest[i] = '_';
	}

	dest[i] = '\0';

	return dest;
}

/**
 Like strncpy but always null terminates. Make sure there is room!
 The variable n should always be one less than the available size.
**/
char *StrnCpy_fn(const char *fn, int line,char *dest,const char *src,size_t n)
{
	char *d = dest;

#ifdef DEVELOPER
	clobber_region(fn, line, dest, n+1);
#endif

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in StrnCpy, "
			"called from [%s][%d]\n", fn, line));
		return(NULL);
	}

	if (!src) {
		*dest = 0;
		return(dest);
	}

	while (n-- && (*d = *src)) {
		d++;
		src++;
	}

	*d = 0;
	return(dest);
}

#if 0
/**
 Like strncpy but copies up to the character marker.  always null terminates.
 returns a pointer to the character marker in the source string (src).
**/

static char *strncpyn(char *dest, const char *src, size_t n, char c)
{
	char *p;
	size_t str_len;

#ifdef DEVELOPER
	clobber_region(dest, n+1);
#endif
	p = strchr_m(src, c);
	if (p == NULL) {
		DEBUG(5, ("strncpyn: separator character (%c) not found\n", c));
		return NULL;
	}

	str_len = PTR_DIFF(p, src);
	strncpy(dest, src, MIN(n, str_len));
	dest[str_len] = '\0';

	return p;
}
#endif

/**
 Check if a string is part of a list.
**/

bool in_list(const char *s, const char *list, bool casesensitive)
{
	char *tok = NULL;
	bool ret = false;
	TALLOC_CTX *frame;

	if (!list) {
		return false;
	}

	frame = talloc_stackframe();
	while (next_token_talloc(frame, &list, &tok,LIST_SEP)) {
		if (casesensitive) {
			if (strcmp(tok,s) == 0) {
				ret = true;
				break;
			}
		} else {
			if (StrCaseCmp(tok,s) == 0) {
				ret = true;
				break;
			}
		}
	}
	TALLOC_FREE(frame);
	return ret;
}

/* this is used to prevent lots of mallocs of size 1 */
static const char null_string[] = "";

/**
 Set a string value, allocing the space for the string
**/

static bool string_init(char **dest,const char *src)
{
	size_t l;

	if (!src)
		src = "";

	l = strlen(src);

	if (l == 0) {
		*dest = CONST_DISCARD(char*, null_string);
	} else {
		(*dest) = SMB_STRDUP(src);
		if ((*dest) == NULL) {
			DEBUG(0,("Out of memory in string_init\n"));
			return false;
		}
	}
	return(true);
}

/**
 Free a string value.
**/

void string_free(char **s)
{
	if (!s || !(*s))
		return;
	if (*s == null_string)
		*s = NULL;
	SAFE_FREE(*s);
}

/**
 Set a string value, deallocating any existing space, and allocing the space
 for the string
**/

bool string_set(char **dest,const char *src)
{
	string_free(dest);
	return(string_init(dest,src));
}

/**
 Substitute a string for a pattern in another string. Make sure there is
 enough room!

 This routine looks for pattern in s and replaces it with
 insert. It may do multiple replacements or just one.

 Any of " ; ' $ or ` in the insert string are replaced with _
 if len==0 then the string cannot be extended. This is different from the old
 use of len==0 which was for no length checks to be done.
**/

void string_sub2(char *s,const char *pattern, const char *insert, size_t len,
		 bool remove_unsafe_characters, bool replace_once,
		 bool allow_trailing_dollar)
{
	char *p;
	ssize_t ls,lp,li, i;

	if (!insert || !pattern || !*pattern || !s)
		return;

	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);

	if (len == 0)
		len = ls + 1; /* len is number of *bytes* */

	while (lp <= ls && (p = strstr_m(s,pattern))) {
		if (ls + (li-lp) >= len) {
			DEBUG(0,("ERROR: string overflow by "
				"%d in string_sub(%.50s, %d)\n",
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		for (i=0;i<li;i++) {
			switch (insert[i]) {
			case '$':
				/* allow a trailing $
				 * (as in machine accounts) */
				if (allow_trailing_dollar && (i == li - 1 )) {
					p[i] = insert[i];
					break;
				}
			case '`':
			case '"':
			case '\'':
			case ';':
			case '%':
			case '\r':
			case '\n':
				if ( remove_unsafe_characters ) {
					p[i] = '_';
					/* yes this break should be here
					 * since we want to fall throw if
					 * not replacing unsafe chars */
					break;
				}
			default:
				p[i] = insert[i];
			}
		}
		s = p + li;
		ls += (li-lp);

		if (replace_once)
			break;
	}
}

void string_sub_once(char *s, const char *pattern,
		const char *insert, size_t len)
{
	string_sub2( s, pattern, insert, len, true, true, false );
}

void string_sub(char *s,const char *pattern, const char *insert, size_t len)
{
	string_sub2( s, pattern, insert, len, true, false, false );
}

void fstring_sub(char *s,const char *pattern,const char *insert)
{
	string_sub(s, pattern, insert, sizeof(fstring));
}

/**
 Similar to string_sub2, but it will accept only allocated strings
 and may realloc them so pay attention at what you pass on no
 pointers inside strings, no const may be passed
 as string.
**/

char *realloc_string_sub2(char *string,
			const char *pattern,
			const char *insert,
			bool remove_unsafe_characters,
			bool allow_trailing_dollar)
{
	char *p, *in;
	char *s;
	ssize_t ls,lp,li,ld, i;

	if (!insert || !pattern || !*pattern || !string || !*string)
		return NULL;

	s = string;

	in = SMB_STRDUP(insert);
	if (!in) {
		DEBUG(0, ("realloc_string_sub: out of memory!\n"));
		return NULL;
	}
	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);
	ld = li - lp;
	for (i=0;i<li;i++) {
		switch (in[i]) {
			case '$':
				/* allow a trailing $
				 * (as in machine accounts) */
				if (allow_trailing_dollar && (i == li - 1 )) {
					break;
				}
			case '`':
			case '"':
			case '\'':
			case ';':
			case '%':
			case '\r':
			case '\n':
				if ( remove_unsafe_characters ) {
					in[i] = '_';
					break;
				}
			default:
				/* ok */
				break;
		}
	}

	while ((p = strstr_m(s,pattern))) {
		if (ld > 0) {
			int offset = PTR_DIFF(s,string);
			string = (char *)SMB_REALLOC(string, ls + ld + 1);
			if (!string) {
				DEBUG(0, ("realloc_string_sub: "
					"out of memory!\n"));
				SAFE_FREE(in);
				return NULL;
			}
			p = string + offset + (p - s);
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		memcpy(p, in, li);
		s = p + li;
		ls += ld;
	}
	SAFE_FREE(in);
	return string;
}

char *realloc_string_sub(char *string,
			const char *pattern,
			const char *insert)
{
	return realloc_string_sub2(string, pattern, insert, true, false);
}

/*
 * Internal guts of talloc_string_sub and talloc_all_string_sub.
 * talloc version of string_sub2.
 */

char *talloc_string_sub2(TALLOC_CTX *mem_ctx, const char *src,
			const char *pattern,
			const char *insert,
			bool remove_unsafe_characters,
			bool replace_once,
			bool allow_trailing_dollar)
{
	char *p, *in;
	char *s;
	char *string;
	ssize_t ls,lp,li,ld, i;

	if (!insert || !pattern || !*pattern || !src) {
		return NULL;
	}

	string = talloc_strdup(mem_ctx, src);
	if (string == NULL) {
		DEBUG(0, ("talloc_string_sub2: "
			"talloc_strdup failed\n"));
		return NULL;
	}

	s = string;

	in = SMB_STRDUP(insert);
	if (!in) {
		DEBUG(0, ("talloc_string_sub2: ENOMEM\n"));
		return NULL;
	}
	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);
	ld = li - lp;

	for (i=0;i<li;i++) {
		switch (in[i]) {
			case '$':
				/* allow a trailing $
				 * (as in machine accounts) */
				if (allow_trailing_dollar && (i == li - 1 )) {
					break;
				}
			case '`':
			case '"':
			case '\'':
			case ';':
			case '%':
			case '\r':
			case '\n':
				if (remove_unsafe_characters) {
					in[i] = '_';
					break;
				}
			default:
				/* ok */
				break;
		}
	}

	while ((p = strstr_m(s,pattern))) {
		if (ld > 0) {
			int offset = PTR_DIFF(s,string);
			string = (char *)TALLOC_REALLOC(mem_ctx, string,
							ls + ld + 1);
			if (!string) {
				DEBUG(0, ("talloc_string_sub: out of "
					  "memory!\n"));
				SAFE_FREE(in);
				return NULL;
			}
			p = string + offset + (p - s);
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		memcpy(p, in, li);
		s = p + li;
		ls += ld;

		if (replace_once) {
			break;
		}
	}
	SAFE_FREE(in);
	return string;
}

/* Same as string_sub, but returns a talloc'ed string */

char *talloc_string_sub(TALLOC_CTX *mem_ctx,
			const char *src,
			const char *pattern,
			const char *insert)
{
	return talloc_string_sub2(mem_ctx, src, pattern, insert,
			true, false, false);
}

/**
 Similar to string_sub() but allows for any character to be substituted.
 Use with caution!
 if len==0 then the string cannot be extended. This is different from the old
 use of len==0 which was for no length checks to be done.
**/

void all_string_sub(char *s,const char *pattern,const char *insert, size_t len)
{
	char *p;
	ssize_t ls,lp,li;

	if (!insert || !pattern || !s)
		return;

	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);

	if (!*pattern)
		return;

	if (len == 0)
		len = ls + 1; /* len is number of *bytes* */

	while (lp <= ls && (p = strstr_m(s,pattern))) {
		if (ls + (li-lp) >= len) {
			DEBUG(0,("ERROR: string overflow by "
				"%d in all_string_sub(%.50s, %d)\n",
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		memcpy(p, insert, li);
		s = p + li;
		ls += (li-lp);
	}
}

char *talloc_all_string_sub(TALLOC_CTX *ctx,
				const char *src,
				const char *pattern,
				const char *insert)
{
	return talloc_string_sub2(ctx, src, pattern, insert,
			false, false, false);
}

/**
 Write an octal as a string.
**/

char *octal_string(int i)
{
	char *result;
	if (i == -1) {
		result = talloc_strdup(talloc_tos(), "-1");
	}
	else {
		result = talloc_asprintf(talloc_tos(), "0%o", i);
	}
	SMB_ASSERT(result != NULL);
	return result;
}


/**
 Truncate a string at a specified length.
**/

char *string_truncate(char *s, unsigned int length)
{
	if (s && strlen(s) > length)
		s[length] = 0;
	return s;
}

/**
 Strchr and strrchr_m are very hard to do on general multi-byte strings.
 We convert via ucs2 for now.
**/

char *strchr_m(const char *src, char c)
{
	smb_ucs2_t *ws = NULL;
	char *s2 = NULL;
	smb_ucs2_t *p;
	const char *s;
	char *ret;
	size_t converted_size;

	/* characters below 0x3F are guaranteed to not appear in
	   non-initial position in multi-byte charsets */
	if ((c & 0xC0) == 0) {
		return strchr(src, c);
	}

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */

	for (s = src; *s && !(((unsigned char)s[0]) & 0x80); s++) {
		if (*s == c)
			return (char *)s;
	}

	if (!*s)
		return NULL;

#ifdef BROKEN_UNICODE_COMPOSE_CHARACTERS
	/* With compose characters we must restart from the beginning. JRA. */
	s = src;
#endif

	if (!push_ucs2_talloc(talloc_tos(), &ws, s, &converted_size)) {
		/* Wrong answer, but what can we do... */
		return strchr(src, c);
	}
	p = strchr_w(ws, UCS2_CHAR(c));
	if (!p) {
		TALLOC_FREE(ws);
		return NULL;
	}
	*p = 0;
	if (!pull_ucs2_talloc(talloc_tos(), &s2, ws, &converted_size)) {
		SAFE_FREE(ws);
		/* Wrong answer, but what can we do... */
		return strchr(src, c);
	}
	ret = (char *)(s+strlen(s2));
	TALLOC_FREE(ws);
	TALLOC_FREE(s2);
	return ret;
}

char *strrchr_m(const char *s, char c)
{
	/* characters below 0x3F are guaranteed to not appear in
	   non-initial position in multi-byte charsets */
	if ((c & 0xC0) == 0) {
		return strrchr(s, c);
	}

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars). Also, in Samba
	   we only search for ascii characters in 'c' and that
	   in all mb character sets with a compound character
	   containing c, if 'c' is not a match at position
	   p, then p[-1] > 0x7f. JRA. */

	{
		size_t len = strlen(s);
		const char *cp = s;
		bool got_mb = false;

		if (len == 0)
			return NULL;
		cp += (len - 1);
		do {
			if (c == *cp) {
				/* Could be a match. Part of a multibyte ? */
			       	if ((cp > s) &&
					(((unsigned char)cp[-1]) & 0x80)) {
					/* Yep - go slow :-( */
					got_mb = true;
					break;
				}
				/* No - we have a match ! */
			       	return (char *)cp;
			}
		} while (cp-- != s);
		if (!got_mb)
			return NULL;
	}

	/* String contained a non-ascii char. Slow path. */
	{
		smb_ucs2_t *ws = NULL;
		char *s2 = NULL;
		smb_ucs2_t *p;
		char *ret;
		size_t converted_size;

		if (!push_ucs2_talloc(talloc_tos(), &ws, s, &converted_size)) {
			/* Wrong answer, but what can we do. */
			return strrchr(s, c);
		}
		p = strrchr_w(ws, UCS2_CHAR(c));
		if (!p) {
			TALLOC_FREE(ws);
			return NULL;
		}
		*p = 0;
		if (!pull_ucs2_talloc(talloc_tos(), &s2, ws, &converted_size)) {
			TALLOC_FREE(ws);
			/* Wrong answer, but what can we do. */
			return strrchr(s, c);
		}
		ret = (char *)(s+strlen(s2));
		TALLOC_FREE(ws);
		TALLOC_FREE(s2);
		return ret;
	}
}

/***********************************************************************
 Return the equivalent of doing strrchr 'n' times - always going
 backwards.
***********************************************************************/

char *strnrchr_m(const char *s, char c, unsigned int n)
{
	smb_ucs2_t *ws = NULL;
	char *s2 = NULL;
	smb_ucs2_t *p;
	char *ret;
	size_t converted_size;

	if (!push_ucs2_talloc(talloc_tos(), &ws, s, &converted_size)) {
		/* Too hard to try and get right. */
		return NULL;
	}
	p = strnrchr_w(ws, UCS2_CHAR(c), n);
	if (!p) {
		TALLOC_FREE(ws);
		return NULL;
	}
	*p = 0;
	if (!pull_ucs2_talloc(talloc_tos(), &s2, ws, &converted_size)) {
		TALLOC_FREE(ws);
		/* Too hard to try and get right. */
		return NULL;
	}
	ret = (char *)(s+strlen(s2));
	TALLOC_FREE(ws);
	TALLOC_FREE(s2);
	return ret;
}

/***********************************************************************
 strstr_m - We convert via ucs2 for now.
***********************************************************************/

char *strstr_m(const char *src, const char *findstr)
{
	smb_ucs2_t *p;
	smb_ucs2_t *src_w, *find_w;
	const char *s;
	char *s2;
	char *retp;

	size_t converted_size, findstr_len = 0;

	/* for correctness */
	if (!findstr[0]) {
		return (char*)src;
	}

	/* Samba does single character findstr calls a *lot*. */
	if (findstr[1] == '\0')
		return strchr_m(src, *findstr);

	/* We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */

	for (s = src; *s && !(((unsigned char)s[0]) & 0x80); s++) {
		if (*s == *findstr) {
			if (!findstr_len)
				findstr_len = strlen(findstr);

			if (strncmp(s, findstr, findstr_len) == 0) {
				return (char *)s;
			}
		}
	}

	if (!*s)
		return NULL;

#if 1 /* def BROKEN_UNICODE_COMPOSE_CHARACTERS */
	/* 'make check' fails unless we do this */

	/* With compose characters we must restart from the beginning. JRA. */
	s = src;
#endif

	if (!push_ucs2_talloc(talloc_tos(), &src_w, src, &converted_size)) {
		DEBUG(0,("strstr_m: src malloc fail\n"));
		return NULL;
	}

	if (!push_ucs2_talloc(talloc_tos(), &find_w, findstr, &converted_size)) {
		TALLOC_FREE(src_w);
		DEBUG(0,("strstr_m: find malloc fail\n"));
		return NULL;
	}

	p = strstr_w(src_w, find_w);

	if (!p) {
		TALLOC_FREE(src_w);
		TALLOC_FREE(find_w);
		return NULL;
	}

	*p = 0;
	if (!pull_ucs2_talloc(talloc_tos(), &s2, src_w, &converted_size)) {
		TALLOC_FREE(src_w);
		TALLOC_FREE(find_w);
		DEBUG(0,("strstr_m: dest malloc fail\n"));
		return NULL;
	}
	retp = (char *)(s+strlen(s2));
	TALLOC_FREE(src_w);
	TALLOC_FREE(find_w);
	TALLOC_FREE(s2);
	return retp;
}

/**
 Convert a string to lower case.
**/

void strlower_m(char *s)
{
	size_t len;
	int errno_save;

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */

	while (*s && !(((unsigned char)s[0]) & 0x80)) {
		*s = tolower_m((unsigned char)*s);
		s++;
	}

	if (!*s)
		return;

	/* I assume that lowercased string takes the same number of bytes
	 * as source string even in UTF-8 encoding. (VIV) */
	len = strlen(s) + 1;
	errno_save = errno;
	errno = 0;
	unix_strlower(s,len,s,len);
	/* Catch mb conversion errors that may not terminate. */
	if (errno)
		s[len-1] = '\0';
	errno = errno_save;
}

/**
 Convert a string to upper case.
**/

void strupper_m(char *s)
{
	size_t len;
	int errno_save;

	/* this is quite a common operation, so we want it to be
	   fast. We optimise for the ascii case, knowing that all our
	   supported multi-byte character sets are ascii-compatible
	   (ie. they match for the first 128 chars) */

	while (*s && !(((unsigned char)s[0]) & 0x80)) {
		*s = toupper_ascii_fast((unsigned char)*s);
		s++;
	}

	if (!*s)
		return;

	/* I assume that lowercased string takes the same number of bytes
	 * as source string even in multibyte encoding. (VIV) */
	len = strlen(s) + 1;
	errno_save = errno;
	errno = 0;
	unix_strupper(s,len,s,len);
	/* Catch mb conversion errors that may not terminate. */
	if (errno)
		s[len-1] = '\0';
	errno = errno_save;
}

/**
 * Calculate the number of units (8 or 16-bit, depending on the
 * destination charset), that would be needed to convert the input
 * string which is expected to be in in src_charset encoding to the
 * destination charset (which should be a unicode charset).
 */

size_t strlen_m_ext(const char *s, const charset_t src_charset,
		    const charset_t dst_charset)
{
	size_t count = 0;

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
		codepoint_t c = next_codepoint_ext(s, src_charset, &c_size);
		s += c_size;

		switch (dst_charset) {
		case CH_UTF16LE:
		case CH_UTF16BE:
		case CH_UTF16MUNGED:
			if (c < 0x10000) {
				/* Unicode char fits into 16 bits. */
				count += 1;
			} else {
				/* Double-width unicode char - 32 bits. */
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

size_t strlen_m_ext_term(const char *s, const charset_t src_charset,
			 const charset_t dst_charset)
{
	if (!s) {
		return 0;
	}
	return strlen_m_ext(s, src_charset, dst_charset) + 1;
}

/**
 * Calculate the number of 16-bit units that would bee needed to convert
 * the input string which is expected to be in CH_UNIX encoding to UTF16.
 *
 * This will be the same as the number of bytes in a string for single
 * byte strings, but will be different for multibyte.
 */

size_t strlen_m(const char *s)
{
	return strlen_m_ext(s, CH_UNIX, CH_UTF16LE);
}

/**
 Count the number of UCS2 characters in a string including the null
 terminator.
**/

size_t strlen_m_term(const char *s)
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

size_t strlen_m_term_null(const char *s)
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
 Just a typesafety wrapper for snprintf into a fstring.
**/

int fstr_sprintf(fstring s, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(s, FSTRING_LEN, fmt, ap);
	va_end(ap);
	return ret;
}

/**
 List of Strings manipulation functions
**/

#define S_LIST_ABS 16 /* List Allocation Block Size */

/******************************************************************************
 version of standard_sub_basic() for string lists; uses talloc_sub_basic()
 for the work
 *****************************************************************************/

bool str_list_sub_basic( char **list, const char *smb_name,
			 const char *domain_name )
{
	TALLOC_CTX *ctx = list;
	char *s, *tmpstr;

	while ( *list ) {
		s = *list;
		tmpstr = talloc_sub_basic(ctx, smb_name, domain_name, s);
		if ( !tmpstr ) {
			DEBUG(0,("str_list_sub_basic: "
				"alloc_sub_basic() return NULL!\n"));
			return false;
		}

		TALLOC_FREE(*list);
		*list = tmpstr;

		list++;
	}

	return true;
}

/******************************************************************************
 substitute a specific pattern in a string list
 *****************************************************************************/

bool str_list_substitute(char **list, const char *pattern, const char *insert)
{
	TALLOC_CTX *ctx = list;
	char *p, *s, *t;
	ssize_t ls, lp, li, ld, i, d;

	if (!list)
		return false;
	if (!pattern)
		return false;
	if (!insert)
		return false;

	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);
	ld = li -lp;

	while (*list) {
		s = *list;
		ls = (ssize_t)strlen(s);

		while ((p = strstr_m(s, pattern))) {
			t = *list;
			d = p -t;
			if (ld) {
				t = TALLOC_ARRAY(ctx, char, ls +ld +1);
				if (!t) {
					DEBUG(0,("str_list_substitute: "
						"Unable to allocate memory"));
					return false;
				}
				memcpy(t, *list, d);
				memcpy(t +d +li, p +lp, ls -d -lp +1);
				TALLOC_FREE(*list);
				*list = t;
				ls += ld;
				s = t +d +li;
			}

			for (i = 0; i < li; i++) {
				switch (insert[i]) {
					case '`':
					case '"':
					case '\'':
					case ';':
					case '$':
					case '%':
					case '\r':
					case '\n':
						t[d +i] = '_';
						break;
					default:
						t[d +i] = insert[i];
				}
			}
		}

		list++;
	}

	return true;
}


#define IPSTR_LIST_SEP	","
#define IPSTR_LIST_CHAR	','

/**
 * Add ip string representation to ipstr list. Used also
 * as part of @function ipstr_list_make
 *
 * @param ipstr_list pointer to string containing ip list;
 *        MUST BE already allocated and IS reallocated if necessary
 * @param ipstr_size pointer to current size of ipstr_list (might be changed
 *        as a result of reallocation)
 * @param ip IP address which is to be added to list
 * @return pointer to string appended with new ip and possibly
 *         reallocated to new length
 **/

static char *ipstr_list_add(char **ipstr_list, const struct ip_service *service)
{
	char *new_ipstr = NULL;
	char addr_buf[INET6_ADDRSTRLEN];
	int ret;

	/* arguments checking */
	if (!ipstr_list || !service) {
		return NULL;
	}

	print_sockaddr(addr_buf,
			sizeof(addr_buf),
			&service->ss);

	/* attempt to convert ip to a string and append colon separator to it */
	if (*ipstr_list) {
		if (service->ss.ss_family == AF_INET) {
			/* IPv4 */
			ret = asprintf(&new_ipstr, "%s%s%s:%d",	*ipstr_list,
				       IPSTR_LIST_SEP, addr_buf,
				       service->port);
		} else {
			/* IPv6 */
			ret = asprintf(&new_ipstr, "%s%s[%s]:%d", *ipstr_list,
				       IPSTR_LIST_SEP, addr_buf,
				       service->port);
		}
		SAFE_FREE(*ipstr_list);
	} else {
		if (service->ss.ss_family == AF_INET) {
			/* IPv4 */
			ret = asprintf(&new_ipstr, "%s:%d", addr_buf,
				       service->port);
		} else {
			/* IPv6 */
			ret = asprintf(&new_ipstr, "[%s]:%d", addr_buf,
				       service->port);
		}
	}
	if (ret == -1) {
		return NULL;
	}
	*ipstr_list = new_ipstr;
	return *ipstr_list;
}

/**
 * Allocate and initialise an ipstr list using ip adresses
 * passed as arguments.
 *
 * @param ipstr_list pointer to string meant to be allocated and set
 * @param ip_list array of ip addresses to place in the list
 * @param ip_count number of addresses stored in ip_list
 * @return pointer to allocated ip string
 **/

char *ipstr_list_make(char **ipstr_list,
			const struct ip_service *ip_list,
			int ip_count)
{
	int i;

	/* arguments checking */
	if (!ip_list || !ipstr_list) {
		return 0;
	}

	*ipstr_list = NULL;

	/* process ip addresses given as arguments */
	for (i = 0; i < ip_count; i++) {
		*ipstr_list = ipstr_list_add(ipstr_list, &ip_list[i]);
	}

	return (*ipstr_list);
}


/**
 * Parse given ip string list into array of ip addresses
 * (as ip_service structures)
 *    e.g. [IPv6]:port,192.168.1.100:389,192.168.1.78, ...
 *
 * @param ipstr ip string list to be parsed
 * @param ip_list pointer to array of ip addresses which is
 *        allocated by this function and must be freed by caller
 * @return number of successfully parsed addresses
 **/

int ipstr_list_parse(const char *ipstr_list, struct ip_service **ip_list)
{
	TALLOC_CTX *frame;
	char *token_str = NULL;
	size_t count;
	int i;

	if (!ipstr_list || !ip_list)
		return 0;

	count = count_chars(ipstr_list, IPSTR_LIST_CHAR) + 1;
	if ( (*ip_list = SMB_MALLOC_ARRAY(struct ip_service, count)) == NULL ) {
		DEBUG(0,("ipstr_list_parse: malloc failed for %lu entries\n",
					(unsigned long)count));
		return 0;
	}

	frame = talloc_stackframe();
	for ( i=0; next_token_talloc(frame, &ipstr_list, &token_str,
				IPSTR_LIST_SEP) && i<count; i++ ) {
		char *s = token_str;
		char *p = strrchr(token_str, ':');

		if (p) {
			*p = 0;
			(*ip_list)[i].port = atoi(p+1);
		}

		/* convert single token to ip address */
		if (token_str[0] == '[') {
			/* IPv6 address. */
			s++;
			p = strchr(token_str, ']');
			if (!p) {
				continue;
			}
			*p = '\0';
		}
		if (!interpret_string_addr(&(*ip_list)[i].ss,
					s,
					AI_NUMERICHOST)) {
			continue;
		}
	}
	TALLOC_FREE(frame);
	return count;
}

/**
 * Safely free ip string list
 *
 * @param ipstr_list ip string list to be freed
 **/

void ipstr_list_free(char* ipstr_list)
{
	SAFE_FREE(ipstr_list);
}

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Decode a base64 string into a DATA_BLOB - simple and slow algorithm
 **/
DATA_BLOB base64_decode_data_blob(const char *s)
{
	int bit_offset, byte_offset, idx, i, n;
	DATA_BLOB decoded = data_blob(s, strlen(s)+1);
	unsigned char *d = decoded.data;
	char *p;

	n=i=0;

	while (*s && (p=strchr_m(b64,*s))) {
		idx = (int)(p - b64);
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		d[byte_offset] &= ~((1<<(8-bit_offset))-1);
		if (bit_offset < 3) {
			d[byte_offset] |= (idx << (2-bit_offset));
			n = byte_offset+1;
		} else {
			d[byte_offset] |= (idx >> (bit_offset-2));
			d[byte_offset+1] = 0;
			d[byte_offset+1] |= (idx << (8-(bit_offset-2))) & 0xFF;
			n = byte_offset+2;
		}
		s++; i++;
	}

	if ((n > 0) && (*s == '=')) {
		n -= 1;
	}

	/* fix up length */
	decoded.length = n;
	return decoded;
}

/**
 * Decode a base64 string in-place - wrapper for the above
 **/
void base64_decode_inplace(char *s)
{
	DATA_BLOB decoded = base64_decode_data_blob(s);

	if ( decoded.length != 0 ) {
		memcpy(s, decoded.data, decoded.length);

		/* null terminate */
		s[decoded.length] = '\0';
	} else {
		*s = '\0';
	}

	data_blob_free(&decoded);
}

/**
 * Encode a base64 string into a talloc()ed string caller to free.
 *
 * From SQUID: adopted from http://ftp.sunet.se/pub2/gnu/vm/base64-encode.c
 * with adjustments
 **/

char *base64_encode_data_blob(TALLOC_CTX *mem_ctx, DATA_BLOB data)
{
	int bits = 0;
	int char_count = 0;
	size_t out_cnt, len, output_len;
	char *result;

        if (!data.length || !data.data)
		return NULL;

	out_cnt = 0;
	len = data.length;
	output_len = data.length * 2 + 4; /* Account for closing bytes. 4 is
					   * random but should be enough for
					   * the = and \0 */
	result = TALLOC_ARRAY(mem_ctx, char, output_len); /* get us plenty of space */
	SMB_ASSERT(result != NULL);

	while (len--) {
		int c = (unsigned char) *(data.data++);
		bits += c;
		char_count++;
		if (char_count == 3) {
			result[out_cnt++] = b64[bits >> 18];
			result[out_cnt++] = b64[(bits >> 12) & 0x3f];
			result[out_cnt++] = b64[(bits >> 6) & 0x3f];
			result[out_cnt++] = b64[bits & 0x3f];
			bits = 0;
			char_count = 0;
		} else {
			bits <<= 8;
		}
	}
	if (char_count != 0) {
		bits <<= 16 - (8 * char_count);
		result[out_cnt++] = b64[bits >> 18];
		result[out_cnt++] = b64[(bits >> 12) & 0x3f];
		if (char_count == 1) {
			result[out_cnt++] = '=';
			result[out_cnt++] = '=';
		} else {
			result[out_cnt++] = b64[(bits >> 6) & 0x3f];
			result[out_cnt++] = '=';
		}
	}
	result[out_cnt] = '\0';	/* terminate */
	return result;
}

/* read a SMB_BIG_UINT from a string */
uint64_t STR_TO_SMB_BIG_UINT(const char *nptr, const char **entptr)
{

	uint64_t val = (uint64_t)-1;
	const char *p = nptr;

	if (!p) {
		if (entptr) {
			*entptr = p;
		}
		return val;
	}

	while (*p && isspace(*p))
		p++;

	sscanf(p,"%"PRIu64,&val);
	if (entptr) {
		while (*p && isdigit(*p))
			p++;
		*entptr = p;
	}

	return val;
}

/* Convert a size specification to a count of bytes. We accept the following
 * suffixes:
 *	    bytes if there is no suffix
 *	kK  kibibytes
 *	mM  mebibytes
 *	gG  gibibytes
 *	tT  tibibytes
 *	pP  whatever the ISO name for petabytes is
 *
 *  Returns 0 if the string can't be converted.
 */
SMB_OFF_T conv_str_size(const char * str)
{
	SMB_OFF_T lval_orig;
        SMB_OFF_T lval;
	char * end;

        if (str == NULL || *str == '\0') {
                return 0;
        }

#ifdef HAVE_STRTOULL
	if (sizeof(SMB_OFF_T) == 8) {
		lval = strtoull(str, &end, 10 /* base */);
	} else {
		lval = strtoul(str, &end, 10 /* base */);
	}
#else
	lval = strtoul(str, &end, 10 /* base */);
#endif

        if (end == NULL || end == str) {
                return 0;
        }

        if (*end == '\0') {
		return lval;
	}

	lval_orig = lval;

	if (strwicmp(end, "K") == 0) {
		lval *= (SMB_OFF_T)1024;
	} else if (strwicmp(end, "M") == 0) {
		lval *= ((SMB_OFF_T)1024 * (SMB_OFF_T)1024);
	} else if (strwicmp(end, "G") == 0) {
		lval *= ((SMB_OFF_T)1024 * (SMB_OFF_T)1024 *
			 (SMB_OFF_T)1024);
	} else if (strwicmp(end, "T") == 0) {
		lval *= ((SMB_OFF_T)1024 * (SMB_OFF_T)1024 *
			 (SMB_OFF_T)1024 * (SMB_OFF_T)1024);
	} else if (strwicmp(end, "P") == 0) {
		lval *= ((SMB_OFF_T)1024 * (SMB_OFF_T)1024 *
			 (SMB_OFF_T)1024 * (SMB_OFF_T)1024 *
			 (SMB_OFF_T)1024);
	} else {
		return 0;
	}

	/*
	 * Primitive attempt to detect wrapping on platforms with
	 * 4-byte SMB_OFF_T. It's better to let the caller handle a
	 * failure than some random number.
	 */
	if (lval_orig <= lval) {
		return 0;
	}

	return lval;
}

void string_append(char **left, const char *right)
{
	int new_len = strlen(right) + 1;

	if (*left == NULL) {
		*left = (char *)SMB_MALLOC(new_len);
		if (*left == NULL) {
			return;
		}
		*left[0] = '\0';
	} else {
		new_len += strlen(*left);
		*left = (char *)SMB_REALLOC(*left, new_len);
	}

	if (*left == NULL) {
		return;
	}

	safe_strcat(*left, right, new_len-1);
}

bool add_string_to_array(TALLOC_CTX *mem_ctx,
			 const char *str, const char ***strings,
			 int *num)
{
	char *dup_str = talloc_strdup(mem_ctx, str);

	*strings = TALLOC_REALLOC_ARRAY(mem_ctx, *strings,
			const char *, (*num)+1);

	if ((*strings == NULL) || (dup_str == NULL)) {
		*num = 0;
		return false;
	}

	(*strings)[*num] = dup_str;
	*num += 1;
	return true;
}

/* Append an sprintf'ed string. Double buffer size on demand. Usable without
 * error checking in between. The indiation that something weird happened is
 * string==NULL */

void sprintf_append(TALLOC_CTX *mem_ctx, char **string, ssize_t *len,
		    size_t *bufsize, const char *fmt, ...)
{
	va_list ap;
	char *newstr;
	int ret;
	bool increased;

	/* len<0 is an internal marker that something failed */
	if (*len < 0)
		goto error;

	if (*string == NULL) {
		if (*bufsize == 0)
			*bufsize = 128;

		*string = TALLOC_ARRAY(mem_ctx, char, *bufsize);
		if (*string == NULL)
			goto error;
	}

	va_start(ap, fmt);
	ret = vasprintf(&newstr, fmt, ap);
	va_end(ap);

	if (ret < 0)
		goto error;

	increased = false;

	while ((*len)+ret >= *bufsize) {
		increased = true;
		*bufsize *= 2;
		if (*bufsize >= (1024*1024*256))
			goto error;
	}

	if (increased) {
		*string = TALLOC_REALLOC_ARRAY(mem_ctx, *string, char,
					       *bufsize);
		if (*string == NULL) {
			goto error;
		}
	}

	StrnCpy((*string)+(*len), newstr, ret);
	(*len) += ret;
	free(newstr);
	return;

 error:
	*len = -1;
	*string = NULL;
}

/*
 * asprintf into a string and strupper_m it after that.
 */

int asprintf_strupper_m(char **strp, const char *fmt, ...)
{
	va_list ap;
	char *result;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(&result, fmt, ap);
	va_end(ap);

	if (ret == -1)
		return -1;

	strupper_m(result);
	*strp = result;
	return ret;
}

char *talloc_asprintf_strupper_m(TALLOC_CTX *t, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = talloc_vasprintf(t, fmt, ap);
	va_end(ap);

	if (ret == NULL) {
		return NULL;
	}
	strupper_m(ret);
	return ret;
}

char *talloc_asprintf_strlower_m(TALLOC_CTX *t, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = talloc_vasprintf(t, fmt, ap);
	va_end(ap);

	if (ret == NULL) {
		return NULL;
	}
	strlower_m(ret);
	return ret;
}


/*
   Returns the substring from src between the first occurrence of
   the char "front" and the first occurence of the char "back".
   Mallocs the return string which must be freed.  Not for use
   with wide character strings.
*/
char *sstring_sub(const char *src, char front, char back)
{
	char *temp1, *temp2, *temp3;
	ptrdiff_t len;

	temp1 = strchr(src, front);
	if (temp1 == NULL) return NULL;
	temp2 = strchr(src, back);
	if (temp2 == NULL) return NULL;
	len = temp2 - temp1;
	if (len <= 0) return NULL;
	temp3 = (char*)SMB_MALLOC(len);
	if (temp3 == NULL) {
		DEBUG(1,("Malloc failure in sstring_sub\n"));
		return NULL;
	}
	memcpy(temp3, temp1+1, len-1);
	temp3[len-1] = '\0';
	return temp3;
}

/********************************************************************
 Check a string for any occurrences of a specified list of invalid
 characters.
********************************************************************/

bool validate_net_name( const char *name,
		const char *invalid_chars,
		int max_len)
{
	int i;

	if (!name) {
		return false;
	}

	for ( i=0; i<max_len && name[i]; i++ ) {
		/* fail if strchr_m() finds one of the invalid characters */
		if ( name[i] && strchr_m( invalid_chars, name[i] ) ) {
			return false;
		}
	}

	return true;
}


/*******************************************************************
 Add a shell escape character '\' to any character not in a known list
 of characters. UNIX charset format.
*******************************************************************/

#define INCLUDE_LIST "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_/ \t.,"
#define INSIDE_DQUOTE_LIST "$`\n\"\\"

char *escape_shell_string(const char *src)
{
	size_t srclen = strlen(src);
	char *ret = SMB_MALLOC_ARRAY(char, (srclen * 2) + 1);
	char *dest = ret;
	bool in_s_quote = false;
	bool in_d_quote = false;
	bool next_escaped = false;

	if (!ret) {
		return NULL;
	}

	while (*src) {
		size_t c_size;
		codepoint_t c = next_codepoint(src, &c_size);

		if (c == INVALID_CODEPOINT) {
			SAFE_FREE(ret);
			return NULL;
		}

		if (c_size > 1) {
			memcpy(dest, src, c_size);
			src += c_size;
			dest += c_size;
			next_escaped = false;
			continue;
		}

		/*
		 * Deal with backslash escaped state.
		 * This only lasts for one character.
		 */

		if (next_escaped) {
			*dest++ = *src++;
			next_escaped = false;
			continue;
		}

		/*
		 * Deal with single quote state. The
		 * only thing we care about is exiting
		 * this state.
		 */

		if (in_s_quote) {
			if (*src == '\'') {
				in_s_quote = false;
			}
			*dest++ = *src++;
			continue;
		}

		/*
		 * Deal with double quote state. The most
		 * complex state. We must cope with \, meaning
		 * possibly escape next char (depending what it
		 * is), ", meaning exit this state, and possibly
		 * add an \ escape to any unprotected character
		 * (listed in INSIDE_DQUOTE_LIST).
		 */

		if (in_d_quote) {
			if (*src == '\\') {
				/*
				 * Next character might be escaped.
				 * We have to peek. Inside double
				 * quotes only INSIDE_DQUOTE_LIST
				 * characters are escaped by a \.
				 */

				char nextchar;

				c = next_codepoint(&src[1], &c_size);
				if (c == INVALID_CODEPOINT) {
					SAFE_FREE(ret);
					return NULL;
				}
				if (c_size > 1) {
					/*
					 * Don't escape the next char.
					 * Just copy the \.
					 */
					*dest++ = *src++;
					continue;
				}

				nextchar = src[1];

				if (nextchar && strchr(INSIDE_DQUOTE_LIST,
							(int)nextchar)) {
					next_escaped = true;
				}
				*dest++ = *src++;
				continue;
			}

			if (*src == '\"') {
				/* Exit double quote state. */
				in_d_quote = false;
				*dest++ = *src++;
				continue;
			}

			/*
			 * We know the character isn't \ or ",
			 * so escape it if it's any of the other
			 * possible unprotected characters.
			 */

	       		if (strchr(INSIDE_DQUOTE_LIST, (int)*src)) {
				*dest++ = '\\';
			}
			*dest++ = *src++;
			continue;
		}

		/*
		 * From here to the end of the loop we're
		 * not in the single or double quote state.
		 */

		if (*src == '\\') {
			/* Next character must be escaped. */
			next_escaped = true;
			*dest++ = *src++;
			continue;
		}

		if (*src == '\'') {
			/* Go into single quote state. */
			in_s_quote = true;
			*dest++ = *src++;
			continue;
		}

		if (*src == '\"') {
			/* Go into double quote state. */
			in_d_quote = true;
			*dest++ = *src++;
			continue;
		}

		/* Check if we need to escape the character. */

	       	if (!strchr(INCLUDE_LIST, (int)*src)) {
			*dest++ = '\\';
		}
		*dest++ = *src++;
	}
	*dest++ = '\0';
	return ret;
}

/***************************************************
 str_list_make, v3 version. The v4 version does not
 look at quoted strings with embedded blanks, so
 do NOT merge this function please!
***************************************************/

#define S_LIST_ABS 16 /* List Allocation Block Size */

char **str_list_make_v3(TALLOC_CTX *mem_ctx, const char *string,
	const char *sep)
{
	char **list;
	const char *str;
	char *s, *tok;
	int num, lsize;

	if (!string || !*string)
		return NULL;

	list = TALLOC_ARRAY(mem_ctx, char *, S_LIST_ABS+1);
	if (list == NULL) {
		return NULL;
	}
	lsize = S_LIST_ABS;

	s = talloc_strdup(list, string);
	if (s == NULL) {
		DEBUG(0,("str_list_make: Unable to allocate memory"));
		TALLOC_FREE(list);
		return NULL;
	}
	if (!sep) sep = LIST_SEP;

	num = 0;
	str = s;

	while (next_token_talloc(list, &str, &tok, sep)) {

		if (num == lsize) {
			char **tmp;

			lsize += S_LIST_ABS;

			tmp = TALLOC_REALLOC_ARRAY(mem_ctx, list, char *,
						   lsize + 1);
			if (tmp == NULL) {
				DEBUG(0,("str_list_make: "
					"Unable to allocate memory"));
				TALLOC_FREE(list);
				return NULL;
			}

			list = tmp;

			memset (&list[num], 0,
				((sizeof(char**)) * (S_LIST_ABS +1)));
		}

		list[num] = tok;
		num += 1;
	}

	list[num] = NULL;

	TALLOC_FREE(s);
	return list;
}

char *sanitize_username(TALLOC_CTX *mem_ctx, const char *username)
{
	fstring tmp;

	alpha_strcpy(tmp, username, ". _-$", sizeof(tmp));
	return talloc_strdup(mem_ctx, tmp);
}
