/* @(#) parsing functions for udpxy
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include "rparse.h"
#include "mtrace.h"

/* parse and copy parameters of HTTP GET request into
 * request buffer
 *
 * @param src       buffer with raw source data
 * @param srclen    length of raw data
 * @param request   destination buffer for the request
 * @param rqlen     length of request buffer on entry
 *                  length of request on exit
 *
 * @return 0 if success: request buffer gets populated,
 *         non-zero if error
 */
int
get_request( const char* src, size_t srclen,
             char* request, size_t* rqlen )
{
    const char HEAD[] = "GET /";
    char* p = NULL;
    const char* EOD = src + srclen - 1;
    size_t n = 0;
    const char SPACE[] = " ";

    p = strstr( src, HEAD );
    if( NULL == p ) return 1;   /* no header */

    p += (sizeof(HEAD) - 1);
    if( p >= EOD)               /* no request */
        return 2;

    n = strcspn( p, " " );
    if( (SPACE[0] != p[n]) || ((p + n) > EOD) || (n >= *rqlen) ) /* overflow */
        return 3;

    (void) strncpy( request, p, n );
    request[ n ] = '\0';

    *rqlen = n;

    return 0;
}


/* parse (GET) request into command and parameters (options)
 * c-strings
 *
 * @param s         source c-string
 * @param cmd       buffer for the parsed command c-string
 * @param clen      length of command buffer
 * @param opt       buffer for the parsed options c-string
 * @param optlen    length of options buffer
 * @param tail      buffer for tail (whatever is beyond options)
 * @param tlen      length of tail buffer
 *
 * @return 0 if success: cmd and opt get get populated
 *         non-zero if an error ocurred
 */
int
parse_param( const char* s, size_t slen,
             char* cmd,     size_t clen,
             char* opt,     size_t optlen,
             char* tail,    size_t tlen)
{
    const char DLM = '/';
    size_t i, j, n = 0;

    assert( s && cmd && (clen > (size_t)0) && opt && (optlen > (size_t)0) );

    *cmd = *opt = '\0';
    if( (size_t)0 == slen ) return 0;   /* empty source */

    /* request ::= [DLM] cmd [DLM opt] */

    /* skip leading delimiter */
    i = ( DLM == s[0] ) ? 1 : 0;

    /* copy into cmd until next delimiter or EOD */
    for( j = 0; (i < slen) && (j < clen) && s[i] && (s[i] != DLM); ) {
        cmd[j++] = s[i++];
    }
    if( j >= clen ) return EOVERFLOW;
    cmd[ j ] = '\0';

    /* skip dividing delimiter */
    if( DLM == s[i] )
        ++i;

    /* over the edge yet? */
    if (i >= slen)
        return 0;

    /* look for '?' separating options from tail */
    n = strcspn(s + i, "?");
    if (n < optlen) {
        (void) strncpy( opt, s + i, n );
        opt[n] = '\0';
    }
    else
        return EOVERFLOW;

    i += n;
    if (i >= slen) return 0;

    if (tail && tlen > 0) {
        (void) strncpy(tail, s + i, tlen);
        tail[tlen - 1] = '\0';
    }

    return 0;
}



/* parse options of upd-relay command into IP address
 * and port
 *
 * @param opt       options string
 * @param addr      destination for address string
 * @param addrlen   length of address string buffer
 * @param port      port to populate
 *
 * @return 0 if success: inaddr and port get populated
 *         non-zero if error
 */
int
parse_udprelay( const char*  opt, size_t optlen,
                char* addr,       size_t addrlen,
                uint16_t* port )
{
    int rc = 1;
    size_t n;
    int pval;

    const char* SEP = ":%~+-^";
    const int MAX_PORT = 65535;

    #define MAX_OPTLEN 512
    char s[ MAX_OPTLEN ];

    assert( opt && addr && addrlen && port );

    (void) strncpy( s, opt, MAX_OPTLEN );
    s[ MAX_OPTLEN - 1 ] = '\0';
    do {
        n = strcspn( s, SEP );
        if( !n || n >= optlen ) break;
        s[n] = '\0';

        strncpy( addr, s, addrlen );
        addr[ addrlen - 1 ] ='\0';

        ++n;
        pval = atoi( s + n );
        if( (pval > 0) && (pval < MAX_PORT) ) {
            *port = (uint16_t)pval;
        }
        else {
            rc = 3;
            break;
        }

        rc = 0;
    }
    while(0);

    return rc;
}

/* __EOF__ */

