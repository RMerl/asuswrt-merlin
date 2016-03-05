/* @(#) implementation of network operations for udpxy
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include "udpxy.h"
#include "netop.h"
#include "util.h"
#include "mtrace.h"
#include "osdef.h"


extern FILE* g_flog;    /* application log */


/* set up (server) listening sockfd
 */
int
setup_listener( const char* ipaddr, int port, int* sockfd, int bklog )
{
#define LOWMARK 10 /* do not accept input of less than X octets */
    int rc, lsock, wmark = LOWMARK;
    struct sockaddr_in servaddr;
    const int ON = 1;

    extern const char IPv4_ALL[];

    assert( (port > 0) && sockfd && ipaddr );
    (void)IPv4_ALL;
    TRACE( (void)tmfprintf( g_flog, "Setting up listener for [%s:%d]\n",
                ipaddr[0] ? ipaddr : IPv4_ALL, port) );

    rc = ERR_INTERNAL;
    do {
        lsock = socket( AF_INET, SOCK_STREAM, 0 );
        if( -1 == lsock ) break;

        (void) memset( &servaddr, 0, sizeof(servaddr) );
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons( (short)port );

        if( '\0' != ipaddr[0] ) {
            if( 1 != inet_aton(ipaddr, &servaddr.sin_addr) ) {
                TRACE( (void)tmfprintf( g_flog, "Invalid server IP: [%s]\n",
                        ipaddr) );

                rc = ERR_PARAM;
                break;
            }
        }
        else {
            servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        }

        rc = setsockopt( lsock, SOL_SOCKET, SO_REUSEADDR,
                         &ON, sizeof(ON) );
        if( 0 != rc ) {
            mperror(g_flog, errno, "%s: setsockopt SO_REUSEADDR",
                    __func__);
            break;
        }

        #define NONBLOCK 1
        rc = set_nblock (lsock, NONBLOCK);
        if (0 != rc) break;

        TRACE( (void)tmfprintf (g_flog, "Setting low watermark for "
            "server socket [%d] to [%d]\n", lsock, wmark) );
        rc = setsockopt (lsock, SOL_SOCKET, SO_RCVLOWAT,
                (char*)&wmark, sizeof(wmark));
        if (rc) {
            mperror (g_flog, errno, "%s: setsockopt SO_RCVLOWAT",
                __func__);
            break;
        }

        rc = bind( lsock, (struct sockaddr*)&servaddr, sizeof(servaddr) );
        if( 0 != rc ) break;

        rc = listen (lsock, (bklog > 0 ? bklog : 1));
        if( 0 != rc ) break;

        rc = 0;
    } while(0);

    if( 0 != rc ) {
        if(errno)
            mperror(g_flog, errno, "%s: socket/bind/listen error",
                    __func__);
        if( lsock ) {
            (void) close( lsock );
        }
    }
    else {
        *sockfd = lsock;
        TRACE( (void)tmfprintf( g_flog, "Created server socket=[%d], backlog=[%d]\n",
                lsock, bklog) );
    }

    return rc;
}


/* add or drop membership in a multicast group
 */
int
set_multicast( int msockfd, const struct in_addr* mifaddr,
               int opname )
{
    struct sockaddr_in addr;
    a_socklen_t len = sizeof(addr);
    struct ip_mreq mreq;

    int rc = 0;
    const char *opstr =
        ((IP_DROP_MEMBERSHIP == opname) ? "DROP" :
         ((IP_ADD_MEMBERSHIP == opname) ? "ADD" : ""));
    assert( opstr[0] );

    assert( (msockfd > 0) && mifaddr );

    (void) memset( &mreq, 0, sizeof(mreq) );
    (void) memcpy( &mreq.imr_interface, mifaddr,
                sizeof(struct in_addr) );

    (void) memset( &addr, 0, sizeof(addr) );
    rc = getsockname( msockfd, (struct sockaddr*)&addr, &len );
    if( 0 != rc ) {
        mperror( g_flog, errno, "%s: getsockname", __func__ );
        return -1;
    }

    (void) memcpy( &mreq.imr_multiaddr, &addr.sin_addr,
                sizeof(struct in_addr) );

    rc = setsockopt( msockfd, IPPROTO_IP, opname,
                    &mreq, sizeof(mreq) );
    if( 0 != rc ) {
        mperror( g_flog, errno, "%s: setsockopt MCAST option: %s",
                    __func__, opstr );
        return rc;
    }

    TRACE( (void)tmfprintf( g_flog, "multicast-group [%s]\n",
                            opstr ) );
    return rc;
}


/* set up the socket to receive multicast data
 *
 */
int
setup_mcast_listener( struct sockaddr_in*   sa,
                      const struct in_addr* mifaddr,
                      int*                  mcastfd,
                      int                   sockbuflen )
{
    int sockfd, rc;
    int ON = 1;
    int buflen = sockbuflen;
    size_t rcvbuf_len = 0;

    assert( sa && mifaddr && mcastfd && (sockbuflen >= 0) );

    TRACE( (void)tmfprintf( g_flog, "Setting up multicast listener\n") );
    rc = ERR_INTERNAL;
    do {
        sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( -1 == sockfd ) {
            mperror(g_flog, errno, "%s: socket", __func__);
            break;
        }

        if (buflen != 0) {
            rc = get_rcvbuf( sockfd, &rcvbuf_len );
            if (0 != rc) break;

            if ((size_t)buflen > rcvbuf_len) {
                rc = set_rcvbuf( sockfd, buflen );
                if (0 != rc) break;
            }
        }
        else {
            TRACE( (void)tmfprintf( g_flog, "Must not adjust buffer size "
                                            "for mcast socket [%d]\n", sockfd ) );
        }

        rc = setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR,
                         &ON, sizeof(ON) );
        if( 0 != rc ) {
            mperror(g_flog, errno, "%s: setsockopt SO_REUSEADDR",
                    __func__);
            break;
        }

#ifdef SO_REUSEPORT
        /*  On some systems (such as FreeBSD) SO_REUSEADDR
            just isn't enough to subscribe to N same channels for different clients.
        */
        rc = setsockopt( sockfd, SOL_SOCKET, SO_REUSEPORT,
                         &ON, sizeof(ON) );
        if( 0 != rc ) {
            mperror(g_flog, errno, "%s: setsockopt SO_REUSEPORT",
                    __func__);
            break;
        }
#endif /* SO_REUSEPORT */

        rc = bind( sockfd, (struct sockaddr*)sa, sizeof(*sa) );
        if( 0 != rc ) {
            mperror(g_flog, errno, "%s: bind", __func__);
            break;
        }

        rc = set_multicast( sockfd, mifaddr, IP_ADD_MEMBERSHIP );
        if( 0 != rc )
            break;
    } while(0);

    if( 0 == rc ) {
        *mcastfd = sockfd;
        TRACE( (void)tmfprintf( g_flog, "Mcast listener socket=[%d] set up\n",
                                sockfd) );
    }
    else {
        (void)close(sockfd);
    }

    return rc;
}


/* unsubscribe from multicast and close the reader socket
 */
void
close_mcast_listener( int msockfd, const struct in_addr* mifaddr )
{
    assert( mifaddr );

    if( msockfd <= 0 ) return;

    (void) set_multicast( msockfd, mifaddr, IP_DROP_MEMBERSHIP );
    (void) close( msockfd );

    TRACE( (void)tmfprintf( g_flog, "Mcast listener socket=[%d] closed\n",
                msockfd) );
    return;
}


/* drop from and add into a multicast group
 */
int
renew_multicast( int msockfd, const struct in_addr* mifaddr )
{
    int rc = 0;

    rc = set_multicast( msockfd, mifaddr, IP_DROP_MEMBERSHIP );
    if( 0 != rc ) return rc;

    rc = set_multicast( msockfd, mifaddr, IP_ADD_MEMBERSHIP );
    return rc;
}


/* set send/receive timeouts on socket(s)
 */
int
set_timeouts( int rsock, int ssock,
              u_short rcv_tmout_sec, u_short rcv_tmout_usec,
              u_short snd_tmout_sec, u_short snd_tmout_usec )
{
    struct timeval rtv, stv;
    int rc = 0;

    if( rsock ) {
        rtv.tv_sec = rcv_tmout_sec;
        rtv.tv_usec = rcv_tmout_usec;
        rc = setsockopt( rsock, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv) );
        if( -1 == rc ) {
            mperror(g_flog, errno, "%s: setsockopt - SO_RCVTIMEO",
                    __func__);
            return ERR_INTERNAL;
        }
        TRACE( (void)tmfprintf (g_flog, "socket %d: RCV timeout set to %ld sec, %ld usec\n",
                rsock, rtv.tv_sec, rtv.tv_usec) );
    }

    if( ssock ) {
        stv.tv_sec = snd_tmout_sec;
        stv.tv_usec = snd_tmout_usec;
        rc = setsockopt( ssock, SOL_SOCKET, SO_SNDTIMEO, &stv, sizeof(stv) );
        if( -1 == rc ) {
            mperror(g_flog, errno, "%s: setsockopt - SO_SNDTIMEO", __func__);
            return ERR_INTERNAL;
        }
        TRACE( (void)tmfprintf (g_flog, "socket %d: SEND timeout set to %ld sec, %ld usec\n",
                rsock, rtv.tv_sec, rtv.tv_usec) );
    }

    return rc;
}



/* set socket's send/receive buffer size
 */
static int
set_sockbuf_size( int sockfd, int option, const size_t len,
        const char* bufname )
{
    size_t data_len = len;
    int rc = 0;

    assert( bufname );

    rc = setsockopt( sockfd, SOL_SOCKET, option,
                    &data_len, sizeof(data_len) );
    if( 0 != rc ) {
        mperror(g_flog, errno, "%s: setsockopt %s [%d]",
                __func__, bufname, option);
        return -1;
    }
    else {
        TRACE( (void)tmfprintf( g_flog,
            "%s buffer size set to [%u] bytes for socket [%d]\n",
            bufname, data_len, sockfd ) );
    }

    return rc;
}


/* set socket's send buffer value
 */
int
set_sendbuf( int sockfd, const size_t len )
{
    return set_sockbuf_size( sockfd, SO_SNDBUF, len, "send" );
}


/* set socket's send buffer value
 */
int
set_rcvbuf( int sockfd, const size_t len )
{
    return set_sockbuf_size( sockfd, SO_RCVBUF, len, "receive" );
}


static int
get_sockbuf_size( int sockfd, int option, size_t* const len,
                  const char* bufname )
{
    int rc = 0;
    size_t buflen = 0;
    socklen_t varsz = sizeof(buflen);

    assert( sockfd && len && bufname );

    rc = getsockopt( sockfd, SOL_SOCKET, option, &buflen, &varsz );
    if (0 != rc) {
        mperror( g_flog, errno, "%s: getsockopt (%s) [%d]", __func__,
                bufname, option);
        return -1;
    }
    else {
        TRACE( (void)tmfprintf( g_flog,
            "current %s buffer size is [%u] bytes for socket [%d]\n",
            bufname, buflen, sockfd ) );
        *len = buflen;
    }

    return rc;

}

/* get socket's send buffer size
 */
int
get_sendbuf( int sockfd, size_t* const len )
{
    return get_sockbuf_size( sockfd, SO_SNDBUF, len, "send" );
}


/* get socket's receive buffer size
 */
int
get_rcvbuf( int sockfd, size_t* const len )
{
    return get_sockbuf_size( sockfd, SO_RCVBUF, len, "receive" );
}



/* set/clear file/socket's mode as non-blocking
 */
int
set_nblock( int fd, int set )
{
    int flags = 0;

    flags = fcntl( fd, F_GETFL, 0 );
    if( flags < 0 ) {
        mperror( g_flog, errno, "%s: fcntl() getting flags on fd=[%d]",
                    __func__, fd );
        return -1;
    }

    if( set )
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if( fcntl( fd, F_SETFL, flags ) < 0 ) {
        mperror( g_flog, errno, "%s: fcntl() %s non-blocking mode "
                "on fd=[%d]", __func__, (set ? "setting" : "clearing"),
                fd );
        return -1;
    }

    return 0;
}


static int
sock_info (int peer, int sockfd, char* addr, size_t alen, int* port)
{
    int rc = 0;
    union {
        struct sockaddr sa;
        char data [sizeof(struct sockaddr_in6)];
    } gsa;
    socklen_t len;

    const void* dst = NULL;
    struct sockaddr_in6 *sa6 = NULL;
    struct sockaddr_in  *sa4 = NULL;

    len = sizeof (gsa.data);
    rc = peer ? getpeername (sockfd, (struct sockaddr*)gsa.data, &len):
                getsockname (sockfd, (struct sockaddr*)gsa.data, &len);
    if (0 != rc) {
        rc = errno;
        (void) fprintf (g_flog, "getsockname error [%d]: %s\n",
            rc, strerror (rc));
        return rc;
    }

    switch (gsa.sa.sa_family) {
        case AF_INET:
            sa4 = (struct sockaddr_in*)&gsa.sa;
            if (addr) {
                dst = inet_ntop (gsa.sa.sa_family, &(sa4->sin_addr),
                    addr, (socklen_t)alen);
            }
            if (port) *port = (int) ntohs (sa4->sin_port);
            break;
        case AF_INET6:
            sa6 = (struct sockaddr_in6*)&gsa.sa;
            if (addr) {
                dst = inet_ntop (gsa.sa.sa_family, &(sa6->sin6_addr),
                    addr, (socklen_t)alen);
            }
            if (port) *port = (int) ntohs (sa6->sin6_port);
            break;
        default:
            (void) tmfprintf (g_flog, "%s: unsupported socket family: %d\n",
                __func__, (int)gsa.sa.sa_family);
            return EAFNOSUPPORT;
    }
    if (addr && !dst) {
        rc = errno;
        (void) tmfprintf (g_flog, "%s: inet_pton error [%d]: %s\n",
            __func__, rc, strerror (rc));
        return rc;
    }

    return rc;
}

enum {e_host = 0, e_peer = 1};
int
get_sockinfo (int sockfd, char* addr, size_t alen, int* port)
{
    return sock_info (e_host, sockfd, addr, alen, port);
}

int
get_peerinfo (int sockfd, char* addr, size_t alen, int* port)
{
    return sock_info (e_peer, sockfd, addr, alen, port);
}


/* __EOF__ */

