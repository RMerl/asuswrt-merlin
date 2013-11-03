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
#include <stdlib.h> /* for size_t */
#include <arpa/inet.h>

#include <atalk/unicode.h>

#include "mac_cyrillic.h"
#include "generic_mb.h"

static size_t   mac_cyrillic_pull(void *,char **, size_t *, char **, size_t *);
static size_t   mac_cyrillic_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_mac_cyrillic =
{
	"MAC_CYRILLIC",
	7,
	mac_cyrillic_pull,
	mac_cyrillic_push,
	CHARSET_CLIENT | CHARSET_MULTIBYTE,
	NULL,
	NULL, NULL
};


/* ------------------------ */

static size_t mac_cyrillic_push( void *cd, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
      return (size_t) mb_generic_push( char_ucs2_to_mac_cyrillic, cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

/* ------------------------ */

static size_t mac_cyrillic_pull ( void *cd, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
      return (size_t) mb_generic_pull( char_mac_cyrillic_to_ucs2, cd, inbuf, inbytesleft, outbuf, outbytesleft);
}
