/* 
   Unix SMB/CIFS implementation.
   minimal iconv implementation
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jelmer Vernooij 2002,2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
   From samba 3.0 beta and GNU libiconv-1.8
   It's bad but most of the time we can't use libc iconv service:
   - it doesn't round trip for most encoding
   - it doesn't know about Apple extension
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>
#include <atalk/unicode.h>
#include "byteorder.h"

/* Given a trailing UTF-8 byte, get the contribution from it to
 * the Unicode scalar value for a particular bit shift amount
 */
#define GETUCVAL(utf8_trailbyte,shift)  ((unsigned int) (( utf8_trailbyte & 0x3F) << shift))

/* Given a unicode scalar, get a trail UTF-8 byte for a particular bit shift amount */
#define GETUTF8TRAILBYTE(uc,shift)      ((char)( 0x80 | ((uc >> shift) & 0x3F) ) )



static size_t   utf8_pull(void *,char **, size_t *, char **, size_t *);
static size_t   utf8_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_utf8 =
{
	"UTF8",
	0x08000103,
	utf8_pull,
	utf8_push,
	CHARSET_VOLUME | CHARSET_MULTIBYTE | CHARSET_PRECOMPOSED,
	NULL,
	NULL, NULL
};

struct charset_functions charset_utf8_mac =
{
	"UTF8-MAC",
	0x08000103,
	utf8_pull,
	utf8_push,
	CHARSET_VOLUME | CHARSET_CLIENT | CHARSET_MULTIBYTE | CHARSET_DECOMPOSED,
	NULL,
	NULL, NULL
};

/* ------------------- Convert from UTF-8 to UTF-16 -------------------*/
static size_t utf8_pull(void *cd _U_, char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	ucs2_t uc = 0;
	unsigned int codepoint;
	int len;

	while (*inbytesleft >= 1 && *outbytesleft >= 2) {
		unsigned char *c = (unsigned char *)*inbuf;
		len = 1;

		/* Arrange conditionals in the order of most frequent occurrence 
		 * for users of Latin-based chars */
		if ((c[0] & 0x80) == 0) {
			uc = c[0];
		} else if ((c[0] & 0xe0) == 0xc0) {
			if (*inbytesleft < 2) {
				LOG(log_debug, logtype_default, "short utf8 char");
				goto badseq;
			}
			uc = (ucs2_t) (((c[0] & 0x1f) << 6) | GETUCVAL(c[1],0)) ;
			len = 2;
		} else if ((c[0] & 0xf0) == 0xe0) {
			if (*inbytesleft < 3) {
				LOG(log_debug, logtype_default, "short utf8 char");
				goto badseq;
			}
			uc = (ucs2_t) (((c[0] & 0x0f) << 12) | GETUCVAL(c[1],6) | GETUCVAL(c[2],0)) ;
			len = 3;
		} else if ((c[0] & 0xf8) == 0xf0) {
			/* 4 bytes, which happens for surrogate pairs only */
			if (*inbytesleft < 4) {
				LOG(log_debug, logtype_default, "short utf8 char");
				goto badseq;
			}
			if (*outbytesleft < 4) {
				LOG(log_debug, logtype_default, "short ucs-2 write");
				errno = E2BIG;
				return -1;
			}
			codepoint = ((c[0] & 0x07) << 18) | GETUCVAL(c[1],12) |
				GETUCVAL(c[2],6) |  GETUCVAL(c[3],0);
			SSVAL(*outbuf,0,(((codepoint - 0x10000) >> 10) + 0xD800)); /* hi  */
			SSVAL(*outbuf,2,(0xDC00 + (codepoint & 0x03FF)));          /* low */
			len = 4;
			(*inbuf)  += 4;
			(*inbytesleft)  -= 4;
			(*outbytesleft) -= 4;
			(*outbuf) += 4;
			continue;
		}
		else {
			errno = EINVAL;
			return -1;
		}

		SSVAL(*outbuf,0,uc);
		(*inbuf)  += len;
		(*inbytesleft)  -= len;
		(*outbytesleft) -= 2;
		(*outbuf) += 2;
	}

	if (*inbytesleft > 0) {
		errno = E2BIG;
		return -1;
	}
	
	return 0;

badseq:
	errno = EINVAL;
	return -1;
}

/* --------------------- Convert from UTF-16 to UTF-8 -----------*/
static size_t utf8_push(void *cd _U_, char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	ucs2_t uc=0;
	ucs2_t hi, low;
	unsigned int codepoint;
	int olen, ilen;

	while (*inbytesleft >= 2 && *outbytesleft >= 1) {
		unsigned char *c = (unsigned char *)*outbuf;
		uc = SVAL((*inbuf),0);
		olen=1;
		ilen=2;

		/* Arrange conditionals in the order of most frequent occurrence for
		   users of Latin-based chars */
		if (uc < 0x80) {
			c[0] = uc;
		} else if (uc < 0x800) {
			if (*outbytesleft < 2) {
				LOG(log_debug, logtype_default, "short utf8 write");
				goto toobig;
			}
			c[1] = GETUTF8TRAILBYTE(uc, 0);
			c[0] = (char)(0xc0 | ((uc >> 6) & 0x1f));
			olen = 2;
		}
		else if ( uc >= 0x202a && uc <= 0x202e ) {
			/* ignore bidi hint characters */
			olen = 0;
		}
		/*
		 * A 2-byte uc value represents a stand-alone Unicode character if
		 *     0 <= uc < 0xd800 or 0xdfff < uc <= 0xffff.
		 * If  0xd800 <= uc <= 0xdfff, uc itself does not represent a Unicode character.
		 * Rather, it is just part of a surrogate pair.  A surrogate pair consists of 
		 * a high surrogate in the range [0xd800 ... 0xdbff] and a low surrogate in the
		 * range [0xdc00 ... 0xdfff].  Together the pair maps to a single Unicode character
		 * whose scalar value is 64K or larger.  It is this scalar value that is transformed
		 * to UTF-8, not the individual surrogates.
		 *
		 * See www.unicode.org/faq/utf_bom.html for more info.
		 */

		else if ( 0xd800 <= uc && uc <= 0xdfff) {
			/* surrogate - needs 4 bytes from input and 4 bytes for output to UTF-8 */
			if (*outbytesleft < 4) {
				LOG(log_debug, logtype_default, "short utf8 write");
				goto toobig;
			}
			if (*inbytesleft < 4) {
				errno = EINVAL;
				return -1;
			}
			hi =  SVAL((*inbuf),0);
			low = SVAL((*inbuf),2);
			if ( 0xd800 <= hi && hi <= 0xdbff && 0xdc00 <= low && low <= 0xdfff) {
				codepoint = ((hi - 0xd800) << 10) + (low - 0xdc00) + 0x10000;
				c[3] = GETUTF8TRAILBYTE(codepoint, 0);
				c[2] = GETUTF8TRAILBYTE(codepoint, 6);
				c[1] = GETUTF8TRAILBYTE(codepoint, 12);
				c[0] = (char)(0xf0 | ((codepoint >> 18) & 0x07));
				ilen = olen = 4;
			} else { /* invalid values for surrogate */
				errno = EINVAL;
				return -1;
			}
		} else {
			if (*outbytesleft < 3) {
				LOG(log_debug, logtype_default, "short utf8 write");
				goto toobig;
			}
			c[2] = GETUTF8TRAILBYTE(uc, 0);
			c[1] = GETUTF8TRAILBYTE(uc, 6);
			c[0] = (char)(0xe0 | ((uc >> 12) & 0x0f));
			olen = 3;
		}

		(*inbytesleft)  -= ilen;
		(*outbytesleft) -= olen;
		(*inbuf)  += ilen;
		(*outbuf) += olen;
	}

	if (*inbytesleft == 1) {
		errno = EINVAL;
		return -1;
	}

	if (*inbytesleft > 1) {
		errno = E2BIG;
		return -1;
	}
	
	return 0;

toobig:
	errno = E2BIG;
	return -1;
}
