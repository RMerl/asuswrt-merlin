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
#include "system/iconv.h"

/**
 * @file
 *
 * @brief Character-set conversion routines built on our iconv.
 * 
 * @note Samba's internal character set (at least in the 3.0 series)
 * is always the same as the one for the Unix filesystem.  It is
 * <b>not</b> necessarily UTF-8 and may be different on machines that
 * need i18n filenames to be compatible with Unix software.  It does
 * have to be a superset of ASCII.  All multibyte sequences must start
 * with a byte with the high bit set.
 *
 * @sa lib/iconv.c
 */

struct smb_iconv_convenience {
	const char *unix_charset;
	const char *dos_charset;
	bool native_iconv;
	smb_iconv_t conv_handles[NUM_CHARSETS][NUM_CHARSETS];
};


/**
 * Return the name of a charset to give to iconv().
 **/
static const char *charset_name(struct smb_iconv_convenience *ic, charset_t ch)
{
	switch (ch) {
	case CH_UTF16: return "UTF-16LE";
	case CH_UNIX: return ic->unix_charset;
	case CH_DOS: return ic->dos_charset;
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

_PUBLIC_ struct smb_iconv_convenience *smb_iconv_convenience_init(TALLOC_CTX *mem_ctx,
							 const char *dos_charset,
							 const char *unix_charset,
							 bool native_iconv)
{
	struct smb_iconv_convenience *ret = talloc_zero(mem_ctx, 
					struct smb_iconv_convenience);

	if (ret == NULL) {
		return NULL;
	}

	talloc_set_destructor(ret, close_iconv_convenience);

	ret->dos_charset = talloc_strdup(ret, dos_charset);
	ret->unix_charset = talloc_strdup(ret, unix_charset);
	ret->native_iconv = native_iconv;

	return ret;
}

/*
  on-demand initialisation of conversion handles
*/
static smb_iconv_t get_conv_handle(struct smb_iconv_convenience *ic,
				   charset_t from, charset_t to)
{
	const char *n1, *n2;
	static bool initialised;

	if (initialised == false) {
		initialised = true;
		
#ifdef LC_ALL
		/* we set back the locale to C to get ASCII-compatible
		   toupper/lower functions.  For now we do not need
		   any other POSIX localisations anyway. When we
		   should really need localized string functions one
		   day we need to write our own ascii_tolower etc.
		*/
		setlocale(LC_ALL, "C");
#endif
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
 * Convert string from one encoding to another, making error checking etc
 *
 * @param mem_ctx Memory context
 * @param cd Iconv handle
 * @param src pointer to source string (multibyte or singlebyte)
 * @param srclen length of the source string in bytes
 * @param dest pointer to destination string (multibyte or singlebyte)
 * @param destlen maximal length allowed for string
 * @returns the number of bytes occupied in the destination
 **/
_PUBLIC_ ssize_t iconv_talloc(TALLOC_CTX *ctx, 
				       smb_iconv_t cd,
				       void const *src, size_t srclen, 
				       void *dst)
{
	size_t i_len, o_len, destlen;
	void **dest = (void **)dst;
	size_t retval;
	const char *inbuf = (const char *)src;
	char *outbuf, *ob;

	*dest = NULL;

	/* it is _very_ rare that a conversion increases the size by
	   more than 3x */
	destlen = srclen;
	outbuf = NULL;
convert:
	destlen = 2 + (destlen*3);
	ob = talloc_realloc(ctx, outbuf, char, destlen);
	if (!ob) {
		DEBUG(0, ("iconv_talloc: realloc failed!\n"));
		talloc_free(outbuf);
		return (size_t)-1;
	} else {
		outbuf = ob;
	}

	/* we give iconv 2 less bytes to allow us to terminate at the
	   end */
	i_len = srclen;
	o_len = destlen-2;
	retval = smb_iconv(cd,
			   &inbuf, &i_len,
			   &outbuf, &o_len);
	if(retval == (size_t)-1) 		{
	    	const char *reason="unknown error";
		switch(errno) {
			case EINVAL:
				reason="Incomplete multibyte sequence";
				break;
			case E2BIG:
				goto convert;		
			case EILSEQ:
				reason="Illegal multibyte sequence";
				break;
		}
		DEBUG(0,("Conversion error: %s(%s)\n",reason,inbuf));
		talloc_free(ob);
		return (size_t)-1;
	}
	
	destlen = (destlen-2) - o_len;

	/* guarantee null termination in all charsets */
	SSVAL(ob, destlen, 0);

	*dest = ob;

	return destlen;

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
_PUBLIC_ bool convert_string_convenience(struct smb_iconv_convenience *ic,
				charset_t from, charset_t to,
				void const *src, size_t srclen, 
				void *dest, size_t destlen, size_t *converted_size,
				bool allow_badcharcnv)
{
	size_t i_len, o_len;
	size_t retval;
	const char* inbuf = (const char*)src;
	char* outbuf = (char*)dest;
	smb_iconv_t descriptor;

	if (allow_badcharcnv) {
		/* Not implemented yet */
		return false;
	}

	if (srclen == (size_t)-1)
		srclen = strlen(inbuf)+1;

	descriptor = get_conv_handle(ic, from, to);

	if (descriptor == (smb_iconv_t)-1 || descriptor == (smb_iconv_t)0) {
		/* conversion not supported, use as is */
		size_t len = MIN(srclen,destlen);
		memcpy(dest,src,len);
		*converted_size = len;
		return true;
	}

	i_len=srclen;
	o_len=destlen;
	retval = smb_iconv(descriptor,  &inbuf, &i_len, &outbuf, &o_len);
	if(retval==(size_t)-1) {
	    	const char *reason;
		switch(errno) {
			case EINVAL:
				reason="Incomplete multibyte sequence";
				return false;
			case E2BIG:
				reason="No more room"; 
				if (from == CH_UNIX) {
					DEBUG(0,("E2BIG: convert_string(%s,%s): srclen=%d destlen=%d - '%s'\n",
						 charset_name(ic, from), charset_name(ic, to),
						 (int)srclen, (int)destlen, 
						 (const char *)src));
				} else {
					DEBUG(0,("E2BIG: convert_string(%s,%s): srclen=%d destlen=%d\n",
						 charset_name(ic, from), charset_name(ic, to),
						 (int)srclen, (int)destlen));
				}
			       return false;
			case EILSEQ:
			       reason="Illegal multibyte sequence";
			       return false;
		}
		/* smb_panic(reason); */
	}
	if (converted_size != NULL)
		*converted_size = destlen-o_len;
	return true;
}
	
/**
 * Convert between character sets, allocating a new buffer using talloc for the result.
 *
 * @param srclen length of source buffer.
 * @param dest always set at least to NULL
 * @note -1 is not accepted for srclen.
 *
 * @returns Size in bytes of the converted string; or -1 in case of error.
 **/

_PUBLIC_ bool convert_string_talloc_convenience(TALLOC_CTX *ctx, 
				       struct smb_iconv_convenience *ic, 
				       charset_t from, charset_t to, 
				       void const *src, size_t srclen, 
				       void *dst, size_t *converted_size, 
					   bool allow_badcharcnv)
{
	void **dest = (void **)dst;
	smb_iconv_t descriptor;
	ssize_t ret;

	if (allow_badcharcnv)
		return false; /* Not implemented yet */

	*dest = NULL;

	if (src == NULL || srclen == (size_t)-1 || srclen == 0)
		return false;

	descriptor = get_conv_handle(ic, from, to);

	if (descriptor == (smb_iconv_t)-1 || descriptor == (smb_iconv_t)0) {
		/* conversion not supported, return -1*/
		DEBUG(3, ("convert_string_talloc: conversion from %s to %s not supported!\n",
			  charset_name(ic, from), 
			  charset_name(ic, to)));
		return false;
	}

	ret = iconv_talloc(ctx, descriptor, src, srclen, dest);
	if (ret == -1)
		return false;
	if (converted_size != NULL)
		*converted_size = ret;
	return true;
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
	/* it cannot occupy more than 4 bytes in UTF16 format */
	uint8_t buf[4];
	smb_iconv_t descriptor;
	size_t ilen_orig;
	size_t ilen;
	size_t olen;
	char *outbuf;

	if ((str[0] & 0x80) == 0) {
		*size = 1;
		return (codepoint_t)str[0];
	}

	/* we assume that no multi-byte character can take
	   more than 5 bytes. This is OK as we only
	   support codepoints up to 1M */
	ilen_orig = strnlen(str, 5);
	ilen = ilen_orig;

	descriptor = get_conv_handle(ic, CH_UNIX, CH_UTF16);
	if (descriptor == (smb_iconv_t)-1) {
		*size = 1;
		return INVALID_CODEPOINT;
	}

	/* this looks a little strange, but it is needed to cope
	   with codepoints above 64k */
	olen = 2;
	outbuf = (char *)buf;
	smb_iconv(descriptor, &str, &ilen, &outbuf, &olen);
	if (olen == 2) {
		olen = 4;
		outbuf = (char *)buf;
		smb_iconv(descriptor,  &str, &ilen, &outbuf, &olen);
		if (olen == 4) {
			/* we didn't convert any bytes */
			*size = 1;
			return INVALID_CODEPOINT;
		}
		olen = 4 - olen;
	} else {
		olen = 2 - olen;
	}

	*size = ilen_orig - ilen;

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


