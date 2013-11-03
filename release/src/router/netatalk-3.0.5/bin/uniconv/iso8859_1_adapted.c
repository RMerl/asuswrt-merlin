/* 
   iso-8859-1.adapted conversion routines
   Copyright (C) Bjoern Fernhomberg 2002,2003
   
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
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>

#include "../../libatalk/unicode/byteorder.h"

static size_t   iso8859_adapted_pull(void *,char **, size_t *, char **, size_t *);
static size_t   iso8859_adapted_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_iso8859_adapted =
{
	"ISO-8859-ADAPTED",
	0,
	iso8859_adapted_pull,
	iso8859_adapted_push,
	CHARSET_MULTIBYTE | CHARSET_PRECOMPOSED,
	NULL,
	NULL, NULL
};


/* ------------------------ 
 * from unicode to iso8859_adapted code page
*/

static size_t iso8859_adapted_push( void *cd _U_, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
    int len = 0;

    while (*inbytesleft >= 2 && *outbytesleft >= 1) {
	ucs2_t inptr = SVAL((*inbuf),0);
        if ( inptr == 0x2122) {
                SSVAL((*outbuf),0,0xAA);
        }
        else if ( inptr == 0x0192) {
                SSVAL((*outbuf),0,0xC5);
        }
        else if ( inptr == 0x2014) {
                SSVAL((*outbuf),0,0xD1);
        }
        else if ( inptr == 0x201C) {
                SSVAL((*outbuf),0,0xD2);
        }
        else if ( inptr == 0x201D) {
                SSVAL((*outbuf),0,0xD3);
        }
        else if ( inptr == 0x2018) {
                SSVAL((*outbuf),0,0xD4);
        }
        else if ( inptr == 0x2019) {
                SSVAL((*outbuf),0,0xD5);
        }
        else if ( inptr == 0x25CA) {
                SSVAL((*outbuf),0,0xD7);
        }
	else if ( inptr > 0x0100) {
		errno = EILSEQ;
		return -1;
        }
	else {
		SSVAL((*outbuf), 0, inptr);
	}
        (*inbuf)        +=2;
        (*outbuf)       +=1;
        (*inbytesleft) -=2;
        (*outbytesleft)-=1;
        len++;
    }

    if (*inbytesleft > 0) {
        errno = E2BIG;
        return -1;
    }

    return len;
}

/* ------------------------ */
static size_t iso8859_adapted_pull ( void *cd _U_, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
    unsigned char  *inptr;
    size_t         len = 0;

    while (*inbytesleft >= 1 && *outbytesleft >= 2) {
        inptr = (unsigned char *) *inbuf;

	if ( *inptr == 0xAA) {
		SSVAL((*outbuf),0,0x2122);
	}
	else if ( *inptr == 0xC5) {
		SSVAL((*outbuf),0,0x0192);
	}
	else if ( *inptr == 0xD1) {
		SSVAL((*outbuf),0,0x2014);
	}
	else if ( *inptr == 0xD2) {
		SSVAL((*outbuf),0,0x201C);
	}
	else if ( *inptr == 0xD3) {
		SSVAL((*outbuf),0,0x201D);
	}
	else if ( *inptr == 0xD4) {
		SSVAL((*outbuf),0,0x2018);
	}
	else if ( *inptr == 0xD5) {
		SSVAL((*outbuf),0,0x2019);
	}
	else if ( *inptr == 0xD7) {
		SSVAL((*outbuf),0,0x25CA);
	}
	else {
		SSVAL((*outbuf),0,(*inptr));
	}
        (*outbuf)      +=2;
	(*outbytesleft)-=2;
        (*inbuf)        +=1;
	(*inbytesleft) -=1;
	len++;
    }

    if (*inbytesleft > 0) {
        errno = E2BIG;
        return (size_t) -1;
    }
    return len;
}

