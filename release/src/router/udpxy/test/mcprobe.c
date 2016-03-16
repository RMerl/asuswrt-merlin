/* @(#) multicast-traffic probing utility
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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>

#ifdef _DEBUG
    #define TRACE( expr ) expr
#else
    #define TRACE( expr ) ((void)0)
#endif

#define IPv4_STRLEN     20
#define PORT_STRLEN     6

#define ETHERNET_MTU 1500
#define RC_TIMEOUT   -100

static volatile sig_atomic_t g_quit = 0;

/* return 1 if the application must gracefully quit
 */
static sig_atomic_t
must_quit() { return g_quit; }

static FILE* g_flog = NULL;

/* handler for signals requestin application exit
 */
static void
handle_quitsigs(int signo)
{
    g_quit = (sig_atomic_t)1;
    (void) &signo;

    TRACE( (void)fprintf( g_flog,
                "*** Caught SIGNAL %d in process=[%d] ***\n",
                signo, getpid()) );
    return;
}


static int
setup_mcast_socket( const char* mifcstr,
                    const char* ipstr,
                    int         sockbuflen )
{
    int sockfd = -1, rc = 0;
    char ipaddrp[ IPv4_STRLEN + PORT_STRLEN ] = { 0 },
         *p = NULL;
    struct sockaddr_in sa, msa;
    struct in_addr mifaddr;
    struct ip_mreq mreq;
    int buflen = sockbuflen;
    int port = 0;
    int ON = 1;
    socklen_t len = sizeof(msa);
    int ifc_idx = -1;
    struct group_req greq;

    assert( mifcstr && ipstr );

    TRACE( (void)fprintf( g_flog, "Setting up channel=[%s] on ifc=[%s]\n",
                ipstr, mifcstr ) );

    memset( &sa, 0, sizeof(sa) );
    memset( &msa, 0, sizeof(msa) );
    sa.sin_family = AF_INET;

    /* parameter parsing */


    (void) strncpy( ipaddrp, ipstr, sizeof(ipaddrp) - 1 );
    if( NULL == (p = strtok( ipaddrp, ":" ) ))
        return -2;

    if( 0 == inet_aton( p, &sa.sin_addr ) )
        return -3;

    if( NULL == (p = strtok( NULL, ":" )) )
        return -4;

    port = atoi(p);
    if( port < 1 ) {
        (void) fprintf( g_flog, "Invalid port value=[%d]\n",
                (int)port );
        return -4;
    }

    sa.sin_port = htons( (short)port );
    if( 0 == inet_aton( mifcstr, &mifaddr ) ) {
        ifc_idx = (int) if_nametoindex(mifcstr);
        if (0 == ifc_idx)
            return -5;
        else {
            fprintf(g_flog, "Interface [%s] matched to index [%d]\n",
                mifcstr, ifc_idx);
        }
    }

    /* socket setup */
    do {
        sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( -1 == sockfd ) {
            perror( "socket" );
            break;
        }

        if( buflen != 0 ) {
            rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVBUF,
                             &buflen, sizeof(buflen) );
            if( 0 != rc ) {
                perror( "setsockopt SO_RCVBUF" );
                break;
            }
        }

        ON = 1;
        rc = setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR,
                         &ON, sizeof(ON) );
        if( 0 != rc ) {
            perror( "setsockopt SO_REUSEADDR" );
            break;
        }

        if( 0 != bind( sockfd, (struct sockaddr*)&sa, sizeof(sa)) ) {
            perror( "bind" );
            break;
        }

        if (ifc_idx <= 0) {
            (void) fprintf(g_flog, "Using protocol-specific multicast API\n");
            (void) memset( &mreq, 0, sizeof(mreq) );
            (void) memcpy( &mreq.imr_multiaddr, &(sa.sin_addr),
                    sizeof(struct in_addr) );

            (void) memcpy( &mreq.imr_interface, &mifaddr,
                    sizeof(struct in_addr) );

            rc = setsockopt( sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    &mreq, sizeof(mreq) );
            if( 0 != rc ) {
                perror( "setsockopt IP_ADD_MEMBERSHIP" );
                break;
            }
        }
        else {
            (void) fprintf(g_flog, "Using GENERIC multicast API\n");
            greq.gr_interface = (unsigned)ifc_idx;
            (void) memcpy(&greq.gr_group, &sa, sizeof(sa));

            rc = setsockopt(sockfd, IPPROTO_IP, MCAST_JOIN_GROUP,
                    &greq, sizeof(greq));
            if (rc) {
                perror("setsockopt MCAST_JOIN_GROUP");
                break;
            }
        }

    } while(0);


    if( 0 != rc ) {
        if( sockfd > 0 ) {
            (void) close( sockfd );
        }
        return -1;
    }

    (void) memset( &msa, 0, sizeof(msa) );
    rc = getsockname( sockfd, (struct sockaddr*)&msa, &len );
    if( 0 != rc ) perror( "getsockname(msa)" );

    TRACE( (void)fprintf( g_flog, "Connected to channel=[%s:%d] on ifc=[%s]\n",
                inet_ntoa( msa.sin_addr ), ntohs( msa.sin_port ), mifcstr ) );
    return sockfd;
}


static void
close_mcast_socket( int msockfd, const char* mifcstr )
{
    struct ip_mreq mreq;
    struct sockaddr_in addr;
    struct in_addr  mifaddr;
    socklen_t len = sizeof( addr );
    int rc = 0, ifc_idx = -1;
    char ipaddr[ IPv4_STRLEN ] = {0};
    struct group_req greq;

    assert( (msockfd > 0) && mifcstr );

    if( msockfd <= 0 ) return;

    if( 0 == inet_aton( mifcstr, &mifaddr ) ) {
        ifc_idx = (int) if_nametoindex(mifcstr);
        if (0 == ifc_idx)
            fprintf( g_flog, "%s: address/ifc resolution error for [%s]\n",
                    __func__, mifcstr );
        return;
    }

    (void) memset( &mreq, 0, sizeof(mreq) );
    (void) memcpy( &mreq.imr_interface, &mifaddr,
                sizeof(struct in_addr) );

    (void) memset( &addr, 0, sizeof(addr) );
    rc = getsockname( msockfd, (struct sockaddr*)&addr, &len );
    if( 0 != rc ) {
        perror( "getsockname" );
    }
    else {
        strncpy( ipaddr, inet_ntoa( addr.sin_addr ), sizeof(ipaddr) - 1 );
        (void) memcpy( &mreq.imr_multiaddr, &addr.sin_addr,
                    sizeof(struct in_addr) );
    }

    if (ifc_idx <= 0) {
        rc = setsockopt( msockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                    &mreq, sizeof(mreq) );
        if( 0 != rc ) {
            perror( "setsockopt IP_DROP_MEMBERSHIP" );
        }
    }
    else {
        greq.gr_interface = (unsigned)ifc_idx;
        (void) memcpy(&greq.gr_group, &addr, sizeof(addr));

        rc = setsockopt(msockfd, IPPROTO_IP, MCAST_LEAVE_GROUP,
                &greq, sizeof(greq));
        if (rc) {
            perror("setsockopt MCAST_LEAVE_GROUP");
        }
    }

    rc = close( msockfd );
    if( 0 != rc ) {
        perror( "close (msockfd)" );
    }

    TRACE( (void)fprintf( g_flog, "Disconnected from channel=[%s:%d]\n",
                ipaddr, (int)ntohs(addr.sin_port)) );
    return;
}


static int
renew_mcast_subscr( int msockfd, const char* mifcstr )
{
    struct ip_mreq mreq;
    struct sockaddr_in addr;
    struct in_addr  mifaddr;
    socklen_t len = sizeof( addr );
    int rc = 0;
    char ipaddr[ IPv4_STRLEN ] = {0};

    assert( (msockfd > 0) && mifcstr );

    if( 0 == inet_aton( mifcstr, &mifaddr ) ) {
        fprintf( g_flog, "%s: inet_aton error for [%s]\n",
                __func__, mifcstr );
        return -1;
    }

    (void) memset( &mreq, 0, sizeof(mreq) );
    (void) memcpy( &mreq.imr_interface, &mifaddr,
                sizeof(struct in_addr) );

    (void) memset( &addr, 0, sizeof(addr) );
    rc = getsockname( msockfd, (struct sockaddr*)&addr, &len );
    if( 0 != rc ) {
        perror( "getsockname" );
        return -1;
    }

    strncpy( ipaddr, inet_ntoa( addr.sin_addr ), sizeof(ipaddr) - 1 );
    (void) memcpy( &mreq.imr_multiaddr, &addr.sin_addr,
                sizeof(struct in_addr) );

    rc = setsockopt( msockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                    &mreq, sizeof(mreq) );
    if( 0 != rc ) {
        perror( "setsockopt IP_DROP_MEMBERSHIP" );
    }

    /* re-populate mreq in case it was reset by prior setsockopt */
    (void) memset( &mreq, 0, sizeof(mreq) );
    (void) memcpy( &mreq.imr_interface, &mifaddr,
                sizeof(struct in_addr) );
    (void) memcpy( &mreq.imr_multiaddr, &addr.sin_addr,
                sizeof(struct in_addr) );

    rc = setsockopt( msockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            &mreq, sizeof(mreq) );
    if( 0 != rc ) {
        perror( "setsockopt IP_ADD_MEMBERSHIP" );
        return -1;
    }

    TRACE( (void)fprintf( g_flog, "Multicast subscription renewed\n" ) );
    return 0;
}


static int
set_rtmout( int sockfd, short sec, short usec )
{
    struct timeval tv;
    int rc = 0;

    tv.tv_sec   = sec;
    tv.tv_usec  = usec;

    rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) );
    if( -1 == rc ) {
        perror( "setsockopt SO_RCVTIMEIO" );
    }

    return rc;
}


static int
tvalcmp( const struct timeval* tv1, const struct timeval* tv2 )
{
    assert( tv1 && tv2 );

    if( tv1->tv_sec > tv2->tv_sec ) return 1;
    if( tv1->tv_sec < tv2->tv_sec ) return -1;

    if( tv1->tv_usec > tv2->tv_usec ) return 1;
    if( tv1->tv_usec < tv2->tv_usec ) return -1;

    return 0;
}


static int
read_msock( int msockfd )
{
    char buf[ ETHERNET_MTU ];
    struct timeval tvb, tva, dtv,
                   min_dtv, max_dtv;
    time_t ltm, tmnow;
    char str_tmnow[ 32 ];

    int rc = 0;
    ssize_t nread = -1, sum_read = 0, bytesec;
    size_t packets = (size_t)0, len;

    assert( msockfd > 0 );
    ltm = tmnow = time(NULL);

    TRACE( (void)fprintf( g_flog, "%s started\n", __func__ ) );

    timerclear( &min_dtv );
    timerclear( &max_dtv );

    for( packets = (size_t)0; !must_quit(); ++packets ) {
        (void) gettimeofday( &tva, NULL );
        nread = read( msockfd, buf, sizeof(buf) );

        (void) gettimeofday( &tvb, NULL );

        if( tvb.tv_usec >= tva.tv_usec ) {
            dtv.tv_usec = tvb.tv_usec - tva.tv_usec;
            dtv.tv_sec = tvb.tv_sec - tva.tv_sec;
        }
        else {
            dtv.tv_usec = tvb.tv_usec - tva.tv_usec + 1000000;
            dtv.tv_sec = tvb.tv_sec - tva.tv_sec - 1;
        }

        if( !timerisset( &max_dtv ) ) {
            min_dtv = max_dtv = dtv;
            /* TRACE( (void)fprintf( g_flog, "Reading ...\n" ) ); */
        }

        if( tvalcmp( &dtv, &min_dtv ) < 0 ) min_dtv = dtv;
        if( tvalcmp( &dtv, &max_dtv ) > 0 ) max_dtv = dtv;

        if( nread >= 0 )
            sum_read += nread;

#define REPORT_PERIOD_SEC 2
        tmnow = time(NULL);
        if( difftime( tmnow, ltm ) > (double)REPORT_PERIOD_SEC )
        {
            strncpy( str_tmnow, ctime(&tmnow), sizeof(str_tmnow) - 1 );
            str_tmnow[ sizeof(str_tmnow) - 1 ] = 0;
            len = strlen( str_tmnow );
            if( '\n' == str_tmnow[ len - 1 ] )
                str_tmnow[ len - 1 ] = 0;

            bytesec = sum_read / REPORT_PERIOD_SEC;

            (void) fprintf( g_flog, "%s - packets: %ld, bytes: %ld speed: %.2f K/sec "
                    "min: %lds.%ldms max: %lds.%ldms last: %lds.%ldms\n",
                    str_tmnow, (long)packets, (long)sum_read,
                    ((double)bytesec / 1024),
                    (long)min_dtv.tv_sec, (long)(min_dtv.tv_usec / 1000),
                    (long)max_dtv.tv_sec, (long)(max_dtv.tv_usec / 1000),
                    (long)dtv.tv_sec, (long)(dtv.tv_usec / 1000) );

            ltm = tmnow;
            packets = sum_read = (size_t)0;
            timerclear( &max_dtv );
            timerclear( &min_dtv );
        }

        if( nread <= 0 ) {
            if( EINTR == errno ) {
                (void) fprintf( g_flog, "\nINTERRUPT!\n" );
                break;
            }
            else if( EAGAIN == errno ) {
                (void) fprintf( g_flog, "\nTIME-OUT!\n" );
                rc = RC_TIMEOUT;
                break;
            }

            rc = -1;
            break;
        }

    } /* for */

    TRACE( (void)fprintf( g_flog, "%s finished\n", __func__ ) );
    return rc;
}


int
main( int argc, char* const argv[] )
{
    const char app[] = "mcprobe";
    int msockfd = 0, rc = 0, tries = -1;
    const char *mifaddr, *addrport;
    struct sigaction qact, oldact;

    if( argc < 3 ) {
        (void) fprintf( stderr,
                "Usage: %s mcast_ifc_addr channel_ip:port\n"
                "Example:\n"
                "\t mcprobe 192.168.1.1 224.0.2.26:1234\n\n"
                "(C) 2008-2013 by Pavel V. Cherenkov, licensed under GNU GPLv3\n\n",
                app );
        return 1;
    }

    g_flog = stdout;

    mifaddr = argv[1];
    addrport = argv[2];

    qact.sa_handler = handle_quitsigs;
    sigemptyset(&qact.sa_mask);
    qact.sa_flags = 0;

    if( (sigaction(SIGTERM, &qact, &oldact) < 0) ||
        (sigaction(SIGQUIT, &qact, &oldact) < 0) ||
        (sigaction(SIGINT,  &qact, &oldact) < 0) ||
        (sigaction(SIGPIPE, &qact, &oldact) < 0)) {
        perror("sigaction-quit");

        return 2;
    }


#define LEN_64K (64 * 1024)
    msockfd = setup_mcast_socket( mifaddr, addrport, LEN_64K );
    if( msockfd < 1 )
        return msockfd;

#define TMOUT_SEC 2
    rc = set_rtmout( msockfd, TMOUT_SEC, 0 );

#define RENEW_TRIES 3
    for( tries = RENEW_TRIES; (0 == rc) && (tries >= 0); ) {
        rc = read_msock( msockfd );

        if( 0 != rc ) {
            if( RC_TIMEOUT == rc && (tries > 0) ) {
                rc = renew_mcast_subscr( msockfd, mifaddr );
                if( 0 != rc ) break;
                --tries;
            }
        } /* 0 != rc */

        if( must_quit() )
            break;
    }

    close_mcast_socket( msockfd, mifaddr );
    return rc;
}


/* __EOF__ */

