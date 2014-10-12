/* 
   Unix SMB/CIFS implementation.
   Character set conversion Extensions
   Copyright (C) Igor Vergeichik <iverg@mail.ru> 2001
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jelmer Vernooij 2007
   
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
#include "lib/util/charset/charset.h"
#include "system/locale.h"
#include "dynconfig.h"

#ifdef strcasecmp
#undef strcasecmp
#endif

/**
 * @file
 * @brief Unicode string manipulation
 */

/* these 2 tables define the unicode case handling.  They are loaded
   at startup either via mmap() or read() from the lib directory */
static void *upcase_table;
static void *lowcase_table;


/*******************************************************************
load the case handling tables

This is the function that should be called from library code.
********************************************************************/
void load_case_tables_library(void)
{
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("load_case_tables");
	if (!mem_ctx) {
		smb_panic("No memory for case_tables");
	}
	upcase_table = map_file(talloc_asprintf(mem_ctx, "%s/upcase.dat", get_dyn_CODEPAGEDIR()), 0x20000);
	lowcase_table = map_file(talloc_asprintf(mem_ctx, "%s/lowcase.dat", get_dyn_CODEPAGEDIR()), 0x20000);
	talloc_free(mem_ctx);
	if (upcase_table == NULL) {
		DEBUG(1, ("Failed to load upcase.dat, will use lame ASCII-only case sensitivity rules\n"));
		upcase_table = (void *)-1;
	}
	if (lowcase_table == NULL) {
		DEBUG(1, ("Failed to load lowcase.dat, will use lame ASCII-only case sensitivity rules\n"));
		lowcase_table = (void *)-1;
	}
}

/*******************************************************************
load the case handling tables

This MUST only be called from main() in application code, never from a
library.  We don't know if the calling program has already done
setlocale() to another value, and can't tell if they have.
********************************************************************/
void load_case_tables(void)
{
	/* This is a useful global hook where we can ensure that the
	 * locale is set from the environment.  This is needed so that
	 * we can use LOCALE as a codepage */
#ifdef HAVE_SETLOCALE
	setlocale(LC_ALL, "");
#endif
	load_case_tables_library();
}

/**
 Convert a codepoint_t to upper case.
**/
_PUBLIC_ codepoint_t toupper_m(codepoint_t val)
{
	if (val < 128) {
		return toupper(val);
	}
	if (upcase_table == NULL) {
		load_case_tables_library();
	}
	if (upcase_table == (void *)-1) {
		return val;
	}
	if (val & 0xFFFF0000) {
		return val;
	}
	return SVAL(upcase_table, val*2);
}

/**
 Convert a codepoint_t to lower case.
**/
_PUBLIC_ codepoint_t tolower_m(codepoint_t val)
{
	if (val < 128) {
		return tolower(val);
	}
	if (lowcase_table == NULL) {
		load_case_tables_library();
	}
	if (lowcase_table == (void *)-1) {
		return val;
	}
	if (val & 0xFFFF0000) {
		return val;
	}
	return SVAL(lowcase_table, val*2);
}

/**
 If we upper cased this character, would we get the same character?
**/
_PUBLIC_ bool islower_m(codepoint_t val)
{
	return (toupper_m(val) != val);
}

/**
 If we lower cased this character, would we get the same character?
**/
_PUBLIC_ bool isupper_m(codepoint_t val)
{
	return (tolower_m(val) != val);
}

/**
  compare two codepoints case insensitively
*/
_PUBLIC_ int codepoint_cmpi(codepoint_t c1, codepoint_t c2)
{
	if (c1 == c2 ||
	    toupper_m(c1) == toupper_m(c2)) {
		return 0;
	}
	return c1 - c2;
}


struct smb_iconv_convenience {
	TALLOC_CTX *child_ctx;
	const char *unix_charset;
	const char *dos_charset;
	const char *display_charset;
	bool native_iconv;
	smb_iconv_t conv_handles[NUM_CHARSETS][NUM_CHARSETS];
};

struct smb_iconv_convenience *global_iconv_convenience = NULL;

struct smb_iconv_convenience *get_iconv_convenience(void)
{
	if (global_iconv_convenience == NULL)
		global_iconv_convenience = smb_iconv_convenience_reinit(talloc_autofree_context(),
									"ASCII", "UTF-8", "ASCII", true, NULL);
	return global_iconv_convenience;
}

/**
 * Return the name of a charset to give to iconv().
 **/
const char *charset_name(struct smb_iconv_convenience *ic, charset_t ch)
{
	switch (ch) {
	case CH_UTF16: return "UTF-16LE";
	case CH_UNIX: return ic->unix_charset;
	case CH_DOS: return ic->dos_charset;
	case CH_DISPLAY: return ic->display_charset;
	case CH_UTF8: return "UTF8";
	case CH_UTF16BE: return "UTF-16BE";
	case CH_UTF16MUNGED: return "UTF16_MUNGED";
	default:
	return "ASCII";
	}
}

/**
 re-initialize iconv conversion descriptors
**/
static int close_iconv_convenience(struct smb_iconv_convenience *data)
{
	unsigned c1, c2;
	for (c1=0;c1<NUM_CHARSETS;c1++) {
		for (c2=0;c2<NUM_CHARSETS;c2++) {
			if (data->conv_handles[c1][c2] != NULL) {
				if (data->conv_handles[c1][c2] != (smb_iconv_t)-1) {
					smb_iconv_close(data->conv_handles[c1][c2]);
				}
				data->conv_handles[c1][c2] = NULL;
			}
		}
	}

	return 0;
}

static const char *map_locale(const char *charset)
{
	if (strcmp(charset, "LOCALE") != 0) {
		return charset;
	}
#if defined(HAVE_NL_LANGINFO) && defined(CODESET)
	{
		const char *ln;
		smb_iconv_t handle;

		ln = nl_langinfo(CODESET);
		if (ln == NULL) {
			DEBUG(1,("Unable to determine charset for LOCALE - using ASCII\n"));
			return "ASCII";
		}
		/* Check whether the charset name is supported
		   by iconv */
		handle = smb_iconv_open(ln, "UCS-2LE");
		if (handle == (smb_iconv_t) -1) {
			DEBUG(5,("Locale charset '%s' unsupported, using ASCII instead\n", ln));
			return "ASCII";
		} else {
			DEBUG(5,("Substituting charset '%s' for LOCALE\n", ln));
			smb_iconv_close(handle);
		}
		return ln;
	}
#endif
	return "ASCII";
}

/*
  the old_ic is passed in here as the smb_iconv_convenience structure
  is used as a global pointer in some places (eg. python modules). We
  don't want to invalidate those global pointers, but we do want to
  update them with the right charset information when loadparm
  runs. To do that we need to re-use the structure pointer, but
  re-fill the elements in the structure with the updated values
 */
_PUBLIC_ struct smb_iconv_convenience *smb_iconv_convenience_reinit(TALLOC_CTX *mem_ctx,
								    const char *dos_charset,
								    const char *unix_charset,
								    const char *display_charset,
								    bool native_iconv,
								    struct smb_iconv_convenience *old_ic)
{
	struct smb_iconv_convenience *ret;

	display_charset = map_locale(display_charset);

	if (old_ic != NULL) {
		ret = old_ic;
		close_iconv_convenience(ret);
		talloc_free(ret->child_ctx);
		ZERO_STRUCTP(ret);
	} else {
		ret = talloc_zero(mem_ctx, struct smb_iconv_convenience);
	}
	if (ret == NULL) {
		return NULL;
	}

	/* we use a child context to allow us to free all ptrs without
	   freeing the structure itself */
	ret->child_ctx = talloc_new(ret);
	if (ret->child_ctx == NULL) {
		return NULL;
	}

	talloc_set_destructor(ret, close_iconv_convenience);

	ret->dos_charset = talloc_strdup(ret->child_ctx, dos_charset);
	ret->unix_charset = talloc_strdup(ret->child_ctx, unix_charset);
	ret->display_charset = talloc_strdup(ret->child_ctx, display_charset);
	ret->native_iconv = native_iconv;

	return ret;
}

/*
  on-demand initialisation of conversion handles
*/
smb_iconv_t get_conv_handle(struct smb_iconv_convenience *ic,
			    charset_t from, charset_t to)
{
	const char *n1, *n2;
	static bool initialised;

	if (initialised == false) {
		initialised = true;
	}

	if (ic->conv_handles[from][to]) {
		return ic->conv_handles[from][to];
	}

	n1 = charset_name(ic, from);
	n2 = charset_name(ic, to);

	ic->conv_handles[from][to] = smb_iconv_open_ex(ic, n2, n1,
						       ic->native_iconv);

	if (ic->conv_handles[from][to] == (smb_iconv_t)-1) {
		if ((from == CH_DOS || to == CH_DOS) &&
		    strcasecmp(charset_name(ic, CH_DOS), "ASCII") != 0) {
			DEBUG(0,("dos charset '%s' unavailable - using ASCII\n",
				 charset_name(ic, CH_DOS)));
			ic->dos_charset = "ASCII";

			n1 = charset_name(ic, from);
			n2 = charset_name(ic, to);

			ic->conv_handles[from][to] =
				smb_iconv_open_ex(ic, n2, n1, ic->native_iconv);
		}
	}

	return ic->conv_handles[from][to];
}

/**
 * Return the unicode codepoint for the next character in the input
 * string in the given src_charset.
 * The unicode codepoint (codepoint_t) is an unsinged 32 bit value.
 *
 * Also return the number of bytes consumed (which tells the caller
 * how many bytes to skip to get to the next src_charset-character).
 *
 * This is implemented (in the non-ascii-case) by first converting the
 * next character in the input string to UTF16_LE and then calculating
 * the unicode codepoint from that.
 *
 * Return INVALID_CODEPOINT if the next character cannot be converted.
 */
_PUBLIC_ codepoint_t next_codepoint_convenience_ext(
			struct smb_iconv_convenience *ic,
			const char *str, charset_t src_charset,
			size_t *bytes_consumed)
{
	/* it cannot occupy more than 4 bytes in UTF16 format */
	uint8_t buf[4];
	smb_iconv_t descriptor;
	size_t ilen_orig;
	size_t ilen;
	size_t olen;
	char *outbuf;

	if ((str[0] & 0x80) == 0) {
		*bytes_consumed = 1;
		return (codepoint_t)str[0];
	}

	/*
	 * we assume that no multi-byte character can take more than 5 bytes.
	 * This is OK as we only support codepoints up to 1M (U+100000)
	 */
	ilen_orig = strnlen(str, 5);
	ilen = ilen_orig;

	descriptor = get_conv_handle(ic, src_charset, CH_UTF16);
	if (descriptor == (smb_iconv_t)-1) {
		*bytes_consumed = 1;
		return INVALID_CODEPOINT;
	}

	/*
	 * this looks a little strange, but it is needed to cope with
	 * codepoints above 64k (U+1000) which are encoded as per RFC2781.
	 */
	olen = 2;
	outbuf = (char *)buf;
	smb_iconv(descriptor, &str, &ilen, &outbuf, &olen);
	if (olen == 2) {
		olen = 4;
		outbuf = (char *)buf;
		smb_iconv(descriptor,  &str, &ilen, &outbuf, &olen);
		if (olen == 4) {
			/* we didn't convert any bytes */
			*bytes_consumed = 1;
			return INVALID_CODEPOINT;
		}
		olen = 4 - olen;
	} else {
		olen = 2 - olen;
	}

	*bytes_consumed = ilen_orig - ilen;

	if (olen == 2) {
		return (codepoint_t)SVAL(buf, 0);
	}
	if (olen == 4) {
		/* decode a 4 byte UTF16 character manually */
		return (codepoint_t)0x10000 +
			(buf[2] | ((buf[3] & 0x3)<<8) |
			 (buf[0]<<10) | ((buf[1] & 0x3)<<18));
	}

	/* no other length is valid */
	return INVALID_CODEPOINT;
}

/*
  return the unicode codepoint for the next multi-byte CH_UNIX character
  in the string

  also return the number of bytes consumed (which tells the caller
  how many bytes to skip to get to the next CH_UNIX character)

  return INVALID_CODEPOINT if the next character cannot be converted
*/
_PUBLIC_ codepoint_t next_codepoint_convenience(struct smb_iconv_convenience *ic,
				    const char *str, size_t *size)
{
	return next_codepoint_convenience_ext(ic, str, CH_UNIX, size);
}

/*
  push a single codepoint into a CH_UNIX string the target string must
  be able to hold the full character, which is guaranteed if it is at
  least 5 bytes in size. The caller may pass less than 5 bytes if they
  are sure the character will fit (for example, you can assume that
  uppercase/lowercase of a character will not add more than 1 byte)

  return the number of bytes occupied by the CH_UNIX character, or
  -1 on failure
*/
_PUBLIC_ ssize_t push_codepoint_convenience(struct smb_iconv_convenience *ic,
				char *str, codepoint_t c)
{
	smb_iconv_t descriptor;
	uint8_t buf[4];
	size_t ilen, olen;
	const char *inbuf;

	if (c < 128) {
		*str = c;
		return 1;
	}

	descriptor = get_conv_handle(ic,
				     CH_UTF16, CH_UNIX);
	if (descriptor == (smb_iconv_t)-1) {
		return -1;
	}

	if (c < 0x10000) {
		ilen = 2;
		olen = 5;
		inbuf = (char *)buf;
		SSVAL(buf, 0, c);
		smb_iconv(descriptor, &inbuf, &ilen, &str, &olen);
		if (ilen != 0) {
			return -1;
		}
		return 5 - olen;
	}

	c -= 0x10000;

	buf[0] = (c>>10) & 0xFF;
	buf[1] = (c>>18) | 0xd8;
	buf[2] = c & 0xFF;
	buf[3] = ((c>>8) & 0x3) | 0xdc;

	ilen = 4;
	olen = 5;
	inbuf = (char *)buf;

	smb_iconv(descriptor, &inbuf, &ilen, &str, &olen);
	if (ilen != 0) {
		return -1;
	}
	return 5 - olen;
}

_PUBLIC_ codepoint_t next_codepoint_ext(const char *str, charset_t src_charset,
					size_t *size)
{
	return next_codepoint_convenience_ext(get_iconv_convenience(), str,
					      src_charset, size);
}

_PUBLIC_ codepoint_t next_codepoint(const char *str, size_t *size)
{
	return next_codepoint_convenience(get_iconv_convenience(), str, size);
}

_PUBLIC_ ssize_t push_codepoint(char *str, codepoint_t c)
{
	return push_codepoint_convenience(get_iconv_convenience(), str, c);
}
