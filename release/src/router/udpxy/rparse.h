/* @(#) header for parsing functions for udpxy
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

#ifndef RPARSE_H_121420071651_
#define RPARSE_H_121420071651_

#include <sys/types.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

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
             char* request, size_t* rqlen );


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
             char* tail,    size_t tlen);


/* parse options of upd-relay command into IP address
 * and port
 *
 * @param opt       options string
 * @param optlen    options string size (including zero-terminating byte)
 * @param addr      destination for address string
 * @param addrlen   length of address string buffer
 * @param port      port to populate
 *
 * @return 0 if success: inaddr and port get populated
 *         non-zero if error
 */
int
parse_udprelay( const char* opt, size_t optlen,
                char* addr, size_t addrlen,
                uint16_t*       port );


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

/* __EOF__ */

