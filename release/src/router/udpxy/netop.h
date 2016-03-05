/* @(#) interface to network operations for udpxy
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com) (pcherenkov@gmail.com)
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

#ifndef NETOP_UDPXY_0313082217_
#define NETOP_UDPXY_0313082217_

#include <sys/types.h>

struct in_addr;
struct sockaddr_in;

#ifdef __cplusplus
extern "C" {
#endif

/* set up (server) listening sockfd
 */
int
setup_listener( const char* ipaddr, int port, int* sockfd, int bklog );


/* set up the socket to receive multicast data
 *
 */
int
setup_mcast_listener( struct sockaddr_in*   sa,
                      const struct in_addr* mifaddr,
                      int*                  mcastfd,
                      int                   sockbuflen );


/* unsubscribe from multicast and close the reader socket
 *
 */
void
close_mcast_listener( int msockfd, const struct in_addr* mifaddr );


/* add or drop membership in a multicast group
 */
int
set_multicast( int msockfd, const struct in_addr* mifaddr,
               int opname );


/* drop from and add into a multicast group
 */
int
renew_multicast( int msockfd, const struct in_addr* mifaddr );


/* set send/receive timeouts on socket(s)
 */
int
set_timeouts( int rsock, int ssock,
        u_short rcv_tmout_sec, u_short rcv_tmout_usec,
        u_short snd_tmout_sec, u_short snd_tmout_usec );


/* set socket's send buffer value
 */
int
set_sendbuf( int sockfd, const size_t len );


/* set socket's send buffer value
 */
int
set_rcvbuf( int sockfd, const size_t len );


/* get socket's send buffer size
 */
int
get_sendbuf( int sockfd, size_t* const len );


/* get socket's send buffer size
 */
int
get_rcvbuf( int sockfd, size_t* const len );


/* set/clear file/socket's mode as non-blocking
 */
int
set_nblock( int fd, int set );

/* retrieve string address, int port of local/remote
   end of the socket
 */
int
get_sockinfo (int sockfd, char* addr, size_t alen, int* port);
int
get_peerinfo (int sockfd, char* addr, size_t alen, int* port);

#ifdef __cplusplus
}
#endif


#endif /* NETOP_UDPXY_0313082217_ */


/* __EOF__ */

