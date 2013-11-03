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
#include <arpa/inet.h>

#include <atalk/unicode.h>

#include "mac_roman.h"
#include "generic_mb.h"

static size_t   mac_roman_pull(void *,char **, size_t *, char **, size_t *);
static size_t   mac_roman_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_mac_roman =
{
	"MAC_ROMAN",
	0,
	mac_roman_pull,
	mac_roman_push,
	CHARSET_CLIENT | CHARSET_MULTIBYTE | CHARSET_PRECOMPOSED,
	NULL,
	NULL, NULL
};

/* ------------------------ */
static int
char_ucs2_to_mac_roman ( unsigned char *r, ucs2_t wc)
{
	unsigned char c = 0;
  	if (wc < 0x0080) {
		*r = wc;
		return 1;
	}
	else if (wc >= 0x00a0 && wc < 0x0100)
		c = mac_roman_page00[wc-0x00a0];
  	else if (wc >= 0x0130 && wc < 0x0198)
		c = mac_roman_page01[wc-0x0130];
	else if (wc >= 0x02c0 && wc < 0x02e0)
		c = mac_roman_page02[wc-0x02c0];
	else if (wc == 0x03c0)
		c = 0xb9;
	else if (wc >= 0x2010 && wc < 0x2048)
		c = mac_roman_page20[wc-0x2010];
	else if (wc >= 0x2120 && wc < 0x2128)
		c = mac_roman_page21[wc-0x2120];
	else if (wc >= 0x2200 && wc < 0x2268)
		c = mac_roman_page22[wc-0x2200];
	else if (wc == 0x25ca)
		c = 0xd7;
	else if (wc >= 0xfb00 && wc < 0xfb08)
		c = mac_roman_pagefb[wc-0xfb00];
	else if (wc == 0xf8ff)
		c = 0xf0;

	if (c != 0) {
		*r = c;
		return 1;
	}
  	return 0;
}

static size_t mac_roman_push( void *cd, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
	/* No special handling required */
	return (size_t) mb_generic_push( char_ucs2_to_mac_roman, cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

/* ------------------------ */
static int
char_mac_roman_to_ucs2 (ucs2_t *pwc, const unsigned char *s)
{
	unsigned char c = *s;
  	if (c < 0x80) {
    		*pwc = (ucs2_t) c;
    		return 1;
  	}
  	else {
		unsigned short wc = mac_roman_2uni[c-0x80];
		*pwc = (ucs2_t) wc;
		return 1;
  	}
	return 0;
}

static size_t mac_roman_pull ( void *cd, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
	return (size_t) mb_generic_pull( char_mac_roman_to_ucs2, cd, inbuf, inbytesleft, outbuf, outbytesleft);
}
