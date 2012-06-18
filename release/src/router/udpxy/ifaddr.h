/* @(#) definition of packet-io functions for udpxy
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

#ifndef IFADDR_UDPXY_0102081930
#define IFADDR_UDPXY_0102081930

#include <sys/types.h>

#ifdef __cplusplus
 extern "C" {
#endif

struct sockaddr;

/* retrieve IPv4 address of the given network interface
 *
 * @param ifname    name of the network interface
 * @param addr      pointer to socket address structure
 * @param addrlen   socket structure size
 *
 * @return 0 if success, -1 otherwise
 */
int
if2addr( const char* ifname,
         struct sockaddr *addr, size_t addrlen );


/* convert input parameter into an IPv4-address string
 *
 * @param s     input text string
 * @param buf   buffer for the destination string
 * @param len   size of the string buffer
 *
 * @return 0 if success, -1 otherwise
 */
int
get_ipv4_address( const char* s, char* buf, size_t len );


/* split input string into IP address and port
 *
 * @param s     input text string
 * @param addr  IP address (string) destination buffer
 * @param len   buffer length
 * @param port  address of port variable
 *
 * @return 0 in success, !=0 otherwise
 */
int
get_addrport( const char* s, char* addr, size_t len, int* port );


#ifdef __cplusplus
}
#endif

#endif /* IFADDR_UDPXY_0102081930 */

/* __EOF__ */

