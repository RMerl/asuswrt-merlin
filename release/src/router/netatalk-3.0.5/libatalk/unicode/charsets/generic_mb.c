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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>

#include "generic_mb.h"
#include "../byteorder.h"


/* ------------------------ */

size_t mb_generic_push( int (*char_func)(unsigned char *, ucs2_t), void *cd _U_, char **inbuf, 
			size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
        int len = 0;
	unsigned char *tmpptr = (unsigned char *) *outbuf;
	ucs2_t inval;

        while (*inbytesleft >= 2 && *outbytesleft >= 1) {

		inval = SVAL((*inbuf),0);
		if ( (char_func)( tmpptr, inval)) {
			(*inbuf) += 2;
			tmpptr++;
			len++;
			(*inbytesleft)  -= 2;
			(*outbytesleft) -= 1;
		}
		else	
		{
			errno = EILSEQ;
			return (size_t) -1;	
		}
        }

        if (*inbytesleft > 0) {
                errno = E2BIG;
                return -1;
        }

        return len;
}

/* ------------------------ */

size_t mb_generic_pull ( int (*char_func)(ucs2_t *, const unsigned char *), void *cd _U_, 
			char **inbuf, size_t *inbytesleft,char **outbuf, size_t *outbytesleft)
{
	ucs2_t 		temp;
	unsigned char	*inptr;
        size_t  len = 0;

        while (*inbytesleft >= 1 && *outbytesleft >= 2) {

		inptr = (unsigned char *) *inbuf;
		if (char_func ( &temp, inptr)) {
			SSVAL((*outbuf), 0, temp);
			(*inbuf)        +=1;
			(*outbuf)       +=2;
			(*inbytesleft) -=1;
			(*outbytesleft)-=2;
			len++;
			
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
