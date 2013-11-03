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
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>

#include "../byteorder.h"
#include "mac_hebrew.h"

static size_t   mac_hebrew_pull(void *,char **, size_t *, char **, size_t *);
static size_t   mac_hebrew_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_mac_hebrew =
{
	"MAC_HEBREW",
	5,
	mac_hebrew_pull,
	mac_hebrew_push,
	CHARSET_CLIENT | CHARSET_MULTIBYTE,
	NULL,
	NULL, NULL
};


/* ------------------------ 
 * from unicode to mac hebrew code page
*/
static int
char_ucs2_to_mac_hebrew ( unsigned char *r, ucs2_t wc)
{
    unsigned char c = 0;
    if (wc < 0x0080) {
       *r = wc;
       return 1;
    }
    else if (wc >= 0x00a0 && wc < 0x0100)
        c = mac_hebrew_page00[wc-0x00a0];
    else if (wc >= 0x05b0 && wc < 0x05f0)
        c = mac_hebrew_page05[wc-0x05b0];
    else if (wc >= 0x2010 && wc < 0x2028)
        c = mac_hebrew_page20[wc-0x2010];
    else if (wc == 0x20aa)
        c = 0xa6;
    else if (wc >= 0xfb18 && wc < 0xfb50)
        c = mac_hebrew_pagefb[wc-0xfb18];
    if (c != 0) {
       *r = c;
       return 1;
    }
    return 0;
}

static size_t mac_hebrew_push( void *cd _U_, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
    unsigned char c = 0;
    int len = 0;
    unsigned char *tmpptr = (unsigned char *) *outbuf;

    while (*inbytesleft >= 2 && *outbytesleft >= 1) {
	ucs2_t inptr = SVAL((*inbuf),0);
	if (inptr == 0x05b8) {
	    (*inbuf) += 2;
	    (*inbytesleft)  -= 2;
	    if (*inbytesleft >= 2 && SVAL((*inbuf),0) == 0xf87f ) {
	        (*inbuf) += 2;
	        (*inbytesleft)  -= 2;
	        c = 0xde;
	    }
	    else {
	        c = 0xcb;
	    }
	    *tmpptr = c; 
	}
	else if (inptr == 0x05f2 && *inbytesleft >= 4 && SVAL((*inbuf),2) == 0x05b7) {
	    (*inbuf) += 4;
	    (*inbytesleft)  -= 4;
	    *tmpptr = 0x81;
	}
	else if (inptr == 0xf86a && *inbytesleft >= 6 && SVAL((*inbuf),2) == 0x05dc && SVAL((*inbuf),4) == 0x05b9) {
	    (*inbuf) += 6;
	    (*inbytesleft)  -= 6;
	    *tmpptr = 0xc0;
	}
	else if (char_ucs2_to_mac_hebrew ( tmpptr, inptr)) {
	    (*inbuf) += 2;
	    (*inbytesleft)  -= 2;
	}
	else {
	    errno = EILSEQ;
	    return (size_t) -1;
	}
	(*outbytesleft) -= 1;
	tmpptr++;
	len++;
    }

    if (*inbytesleft > 0) {
        errno = E2BIG;
        return -1;
    }

    return len;
}

/* ------------------------ */
static int
char_mac_hebrew_to_ucs2 (ucs2_t *pwc, const unsigned char *s)
{
	unsigned char c = *s;
  	if (c < 0x80) {
    		*pwc = (ucs2_t) c;
    		return 1;
  	}
  	else {
		unsigned short wc = mac_hebrew_2uni[c-0x80];
		if (wc != 0xfffd) {
		    *pwc = (ucs2_t) wc;
		    return 1;
		}
  	}
	return 0;
}

static size_t mac_hebrew_pull ( void *cd _U_, char **inbuf, size_t *inbytesleft,
                         char **outbuf, size_t *outbytesleft)
{
    ucs2_t         temp;
    unsigned char  *inptr;
    size_t         len = 0;

    while (*inbytesleft >= 1 && *outbytesleft >= 2) {
        inptr = (unsigned char *) *inbuf;
	if (char_mac_hebrew_to_ucs2 ( &temp, inptr)) {
	    if (temp == 1) {       /* 0x81 --> 0x05f2+0x05b7 */
	        if (*outbytesleft < 4) {
	            errno = E2BIG;
	            return (size_t) -1;	
	        }
		SSVAL((*outbuf),0,0x05f2);
		SSVAL((*outbuf),2,0x05b7);
	        (*outbuf)      +=4;
	        (*outbytesleft)-=4;
	        len += 2;
	    }
	    else if (temp == 2) { /* 0xc0 -> 0xf86a 0x05dc 0x05b9*/
	        if (*outbytesleft < 6) {
	            errno = E2BIG;
	            return (size_t) -1;	
	        }
		SSVAL((*outbuf),0,0xf86a);
		SSVAL((*outbuf),2,0x05dc);
		SSVAL((*outbuf),4,0x05b9);
	        (*outbuf)      +=6;
	        (*outbytesleft)-=6;
	        len += 3;
	    }
	    else if (temp == 3) { /* 0xde --> 0x05b8 0xf87f */
	        if (*outbytesleft < 4) {
	            errno = E2BIG;
	            return (size_t) -1;	
	        }
		SSVAL((*outbuf),0,0x05b8);
		SSVAL((*outbuf),2,0xf87f);
	        (*outbuf)      +=4;
	        (*outbytesleft)-=4;
	        len += 2;
	    }
	    else {
		SSVAL((*outbuf),0,temp);
	        (*outbuf)      +=2;
		(*outbytesleft)-=2;
		len++;
	    }
	    (*inbuf)        +=1;
	    (*inbytesleft) -=1;
	}
	else	
	{
	    errno = EILSEQ;
	    return (size_t) -1;	
	}
    }

    if (*inbytesleft > 0) {
        errno = E2BIG;
        return (size_t) -1;
    }
    return len;
}

