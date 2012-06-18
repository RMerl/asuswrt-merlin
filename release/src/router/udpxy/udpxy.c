/* @(#) udpxy server: main module
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

#include "osdef.h"  /* os-specific definitions */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <sys/time.h>

/* #include <sys/select.h> */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>
#include <sys/uio.h>

#include "mtrace.h"
#include "rparse.h"
#include "util.h"
#include "prbuf.h"
#include "ifaddr.h"

#include "udpxy.h"
#include "ctx.h"
#include "mkpg.h"
#include "rtp.h"
#include "uopt.h"
#include "dpkt.h"
#include "netop.h"

/* external globals */

extern const char   CMD_UDP[];
extern const char   CMD_STATUS[];
extern const char   CMD_RESTART[];
extern const char   CMD_RTP[];

extern const size_t CMD_UDP_LEN;
extern const size_t CMD_STATUS_LEN;
extern const size_t CMD_RESTART_LEN;
extern const size_t CMD_RTP_LEN;

extern const char   IPv4_ALL[];

extern const char  UDPXY_COPYRIGHT_NOTICE[];
extern const char  UDPXY_CONTACT[];
extern const char  COMPILE_MODE[];
extern const char  VERSION[];
extern const int   BUILDNUM;

extern FILE*  g_flog;
extern volatile sig_atomic_t g_quit;

/* globals
 */

struct udpxy_opt    g_uopt;

/* misc
 */

static volatile sig_atomic_t g_childexit = 0;


/*********************************************************/

/* handler for signals requestin application exit
 */
static void
handle_quitsigs(int signo)
{
    g_quit = (sig_atomic_t)1;
    (void) &signo;

    TRACE( (void)tmfprintf( g_flog,
                "*** Caught SIGNAL %d in process=[%d] ***\n",
                signo, getpid()) );
    return;
}


/* return 1 if the application must gracefully quit
 */
static sig_atomic_t
must_quit() { return g_quit; }


/* handle SIGCHLD
 */
static void
handle_sigchld(int signo)
{
    (void) &signo;
    g_childexit = (sig_atomic_t)1;

    TRACE( (void)tmfprintf( g_flog, "*** Caught SIGCHLD in process=[%d] ***\n",
                getpid()) );
    return;
}


static int get_childexit()      { return g_childexit; }


/* clear SIGCHLD flag and adjust context if needed
 */
static void
wait_children( struct server_ctx* ctx, int options )
{
    int status;
    pid_t pid;

    assert( ctx );
    g_childexit = 0;

    while( 0 < (pid = waitpid( -1, &status, options )) ) {
        TRACE( (void)tmfprintf( g_flog, "Client [%d] has exited.\n", pid) );
        delete_client( ctx, pid );
    }

    if( (-1 == pid) && ( ECHILD != errno ) ) {
        mperror(g_flog, errno, "%s: waitpid", __func__);
    }

    return;
}


/* wait for all children to quit
 */
static void
wait_all( struct server_ctx* ctx ) { wait_children( ctx, 0 ); }


/* wait for the children who already terminated
 */
static void
wait_terminated( struct server_ctx* ctx ) { wait_children( ctx, WNOHANG ); }


/* read HTTP request from sockfd, parse it into command
 * and its parameters (for instance, command='udp' and
 * parameters being '192.168.0.1:5002')
 */
static int
read_command( int sockfd, char* cmd, size_t clen,
              char* param, size_t plen )
{
#define DBUF_SZ 2048  /* max size for raw data with HTTP request */
#define RBUF_SZ 512   /* max size for url-derived request */
    static char httpbuf[ DBUF_SZ ],
                request[ RBUF_SZ ];
    ssize_t hlen;
    size_t  rlen;
    int rc = 0;

    assert( (sockfd > 0) && cmd && clen && param && plen );

    TRACE( (void)tmfprintf( g_flog,  "Reading command from socket [%d]\n",
                            sockfd ) );
    hlen = recv( sockfd, httpbuf, sizeof(httpbuf), 0 );
    if( 0>hlen ) {
        mperror(g_flog, errno, "%s - recv", __func__);
        return errno;
    }

    /* deep DEBUG - re-enable if needed */
    TRACE( (void)tmfprintf( g_flog, "HTTP buffer [%ld bytes] received\n", (long)hlen ) );
    /* TRACE( (void) save_buffer( httpbuf, hlen, "/tmp/httpbuf.dat" ) ); */

    rlen = sizeof(request);
    rc = get_request( httpbuf, (size_t)hlen, request, &rlen );
    if( -1 == rc ) return rc;

    TRACE( (void)tmfprintf( g_flog, "Request=[%s], length=[%lu]\n",
                request, (u_long)rlen ) );

    rc = parse_param( request, rlen, cmd, clen, param, plen );
    if( 0 == rc ) {
        TRACE( (void)tmfprintf( g_flog, "Command [%s] with params [%s]"
                    " read from socket=[%d]\n", cmd, param, sockfd) );
    }

    return rc;
}


/* terminate the client process
 */
static int
terminate( pid_t pid )
{
    TRACE( (void)tmfprintf( g_flog, "Forcing client process [%d] to QUIT\n",
                            pid) );

    if( pid <= 0 ) return 0;

    if( 0 != kill( pid, SIGQUIT ) ) {
        if( ESRCH != errno ) {
            mperror(g_flog, errno, "%s - kill", __func__);
            return ERR_INTERNAL;
        }
        /* ESRCH could mean client has quit already;
            * if so we should wait for it */

        TRACE( (void)tmfprintf( g_flog, "Process [%d] is not running.\n",
                pid ) );
    }

    return 0;
}


/*  terminate all clients
 */
static void
terminate_all_clients( struct server_ctx* ctx )
{
    size_t i;
    pid_t pid;

    for( i = 0; i < ctx->clmax; ++i ) {
        pid = ctx->cl[i].pid;
        if( pid > 0 ) (void) terminate( pid );
    }

    return;
}


/* send HTTP response to socket
 */
static int
send_http_response( int sockfd, int code, const char* reason )
{
    char    msg[ 128 ];
    ssize_t nsent;
    a_socklen_t msglen;

    assert( (sockfd > 0) && code && reason );

    msglen = snprintf( msg, sizeof(msg) - 1, "HTTP/1.1 %d %s.\nContent-Type:application/octet-stream.\n\n",
              code, reason );
    if( msglen <= 0 ) return ERR_INTERNAL;

    nsent = send( sockfd, msg, msglen, 0 );
    if( -1 == nsent ) {
        mperror(g_flog, errno, "%s - send", __func__);
        return ERR_INTERNAL;
    }

    TRACE( (void)tmfprintf( g_flog, "Sent HTTP response code=[%d], "
                "reason=[%s] to socket=[%d]\n",
                code, reason, sockfd) );
    return 0;
}


/* renew multicast subscription if g_uopt.mcast_refresh seconds
 * have passed since the last renewal
 */
static void
check_mcast_refresh( int msockfd, time_t* last_tm,
                     const struct in_addr* mifaddr )
{
    time_t now = 0;

    if( NULL != g_uopt.srcfile ) /* reading from file */
        return;

    assert( (msockfd > 0) && last_tm && mifaddr );
    now = time(NULL);

    if( difftime( now, *last_tm ) >= (double)g_uopt.mcast_refresh ) {
        (void) renew_multicast( msockfd, mifaddr );
        *last_tm = now;
    }

    return;
}


/* analyze return value of I/O routines (read_data/write_data)
 * and the pause-time marker to determine if we are in a
 * PAUSE state
 */
static int
pause_detect( int ntrans, time_t* p_pause  )
{
    time_t now = 0;
    const double MAX_PAUSE_SEC = 5.0;

    assert( p_pause );

    /* timeshift: detect PAUSE by would-block error */
    if (IO_BLK == ntrans) {
        now = time(NULL);

        if (*p_pause) {
            if( difftime(now, *p_pause) > MAX_PAUSE_SEC ) {
                TRACE( (void)tmfprintf( g_flog,
                    "PAUSE timed out after [%.0f] seconds\n",
                    MAX_PAUSE_SEC ) );
                return -1;
            }
        }
        else {
            *p_pause = now;
            TRACE( (void)tmfprintf( g_flog, "PAUSE started\n" ) );
        }
    }
    else {
        if (*p_pause) {
            *p_pause = 0;
            TRACE( (void)tmfprintf( g_flog, "PAUSE ended\n" ) );
        }
    }

    return 0;
}


/* calculate values for:
 *  1. number of messages to fit into data buffer
 *  2. recommended (minimal) size of socket buffer
 *     (to read into the data buffer)
 */
static int
calc_buf_settings( ssize_t* bufmsgs, size_t* sock_buflen )
{
    ssize_t nmsgs = -1, max_buf_used = -1, env_snd_buflen = -1;
    size_t buflen = 0;

    /* how many messages should we process? */
    nmsgs = (g_uopt.rbuf_msgs > 0) ? g_uopt.rbuf_msgs :
             (int)g_uopt.rbuf_len / ETHERNET_MTU;

    /* how many bytes could be written at once
        * to the send socket */
    max_buf_used = (g_uopt.rbuf_msgs > 0)
        ? (ssize_t)(nmsgs * ETHERNET_MTU) : g_uopt.rbuf_len;
    if (max_buf_used > g_uopt.rbuf_len) {
        max_buf_used = g_uopt.rbuf_len;
    }

    assert( max_buf_used >= 0 );

    env_snd_buflen = get_sizeval( "UDPXY_SOCKBUF_LEN", 0);
    buflen = (env_snd_buflen > 0) ? (size_t)env_snd_buflen : (size_t)max_buf_used;

    if (buflen < (size_t) MIN_SOCKBUF_LEN) {
        buflen = (size_t) MIN_SOCKBUF_LEN;
    }

    /* cannot go below the size of effective usage */
    if( buflen < (size_t)max_buf_used ) {
        buflen = (size_t)max_buf_used;
    }

    if (bufmsgs) *bufmsgs = nmsgs;
    if (sock_buflen) *sock_buflen = buflen;

    TRACE( (void)tmfprintf( g_flog,
                "min socket buffer = [%ld], "
                "max space to use = [%ld], "
                "Rmsgs = [%ld]\n",
                (long)buflen, (long)max_buf_used, (long)nmsgs ) );

    return 0;
}


/* make send-socket (dsockfd) buffer size no less
 * than that of read-socket (ssockfd)
 */
static int
sync_dsockbuf_len( int ssockfd, int dsockfd )
{
    size_t curr_sendbuf_len = 0, curr_rcvbuf_len = 0;
    int rc = 0;

    if ( 0 != g_uopt.nosync_dbuf ) {
        TRACE( (void)tmfprintf( g_flog,
                "Must not adjust buffer size "
                "for send socket [%d]\n", dsockfd) );
        return 0;
    }

    assert( ssockfd && dsockfd );

    rc = get_sendbuf( dsockfd, &curr_sendbuf_len );
    if (0 != rc) return rc;

    rc = get_rcvbuf( ssockfd, &curr_rcvbuf_len );
    if (0 != rc) return rc;

    if ( curr_rcvbuf_len > curr_sendbuf_len ) {
        rc = set_sendbuf( dsockfd, curr_rcvbuf_len );
        if (0 != rc) return rc;
    }

    return rc;
}


/* relay traffic from source to destination socket
 *
 */
static int
relay_traffic( int ssockfd, int dsockfd, struct server_ctx* ctx,
               int dfilefd, const struct in_addr* mifaddr )
{
    volatile sig_atomic_t quit = 0;

    int rc = 0;
    ssize_t nmsgs = -1;
    ssize_t nrcv = 0, nsent = 0, nwr = 0,
            lrcv = 0, lsent = 0;
    char*  data = NULL;
    size_t data_len = g_uopt.rbuf_len;
    struct rdata_opt ropt;
    time_t pause_time = 0, rfr_tm = time(NULL);

    const int ALLOW_PAUSES = get_flagval( "UDPXY_ALLOW_PAUSES", 0 );

    /* permissible variation in data-packet size */
    static const ssize_t t_delta = 0x20;

    struct dstream_ctx ds;

    static const int SET_PID = 1;
    struct tps_data tps;

    assert( ctx && mifaddr );

    /* NOPs to eliminate warnings in lean version */
    lrcv = t_delta - t_delta + lsent;

    check_fragments( NULL, 0, 0, 0, 0, g_flog );

    /* INIT
     */

    rc = calc_buf_settings( &nmsgs, NULL );
    if (0 != rc) return -1;

    TRACE( (void)tmfprintf( g_flog, "Data buffer will hold up to "
                        "[%d] messages\n", nmsgs ) );

    rc = init_dstream_ctx( &ds, ctx->cmd, g_uopt.srcfile, nmsgs );
    if( 0 != rc ) return -1;

    (void) set_nice( g_uopt.nice_incr, g_flog );

    do {
        if( NULL == g_uopt.srcfile ) {
            rc = set_timeouts( ssockfd, dsockfd,
                               ctx->rcv_tmout, 0,
                               ctx->snd_tmout, 0 );
            if( 0 != rc ) break;
        }

        if( dsockfd > 0 ) {
            rc = sync_dsockbuf_len( ssockfd, dsockfd );
            if( 0 != rc ) break;

            rc = send_http_response( dsockfd, 200, "OK" );
            if( 0 != rc ) break;

            if ( ALLOW_PAUSES ) {
                /* timeshift: to detect PAUSE make destination
                * socket non-blocking
                */
                #define SET_NBLOCK      1
                rc = set_nblock( dsockfd, SET_NBLOCK );
                if( 0 != rc ) break;
                TRACE( (void)tmfprintf( g_flog,
                    "Socket [%d] set to non-blocking I/O\n", dsockfd ) );
            }
        }

        data = malloc(data_len);
        if( NULL == data ) {
            mperror( g_flog, errno, "%s: malloc", __func__ );
            break;
        }

        if( g_uopt.cl_tpstat )
            tpstat_init( &tps, SET_PID );
    } while(0);

    TRACE( (void)tmfprintf( g_flog, "Relaying traffic from socket[%d] "
            "to socket[%d], buffer size=[%d], Rmsgs=[%d], pauses=[%d]\n",
            ssockfd, dsockfd, data_len, g_uopt.rbuf_msgs, ALLOW_PAUSES) );

    /* RELAY LOOP
     */
    ropt.max_frgs = g_uopt.rbuf_msgs;
    ropt.buf_tmout = g_uopt.dhold_tmout;

    pause_time = 0;

    while( (0 == rc) && !(quit = must_quit()) ) {
        if( g_uopt.mcast_refresh > 0 ) {
            check_mcast_refresh( ssockfd, &rfr_tm, mifaddr );
        }

        nrcv = read_data( &ds, ssockfd, data, data_len, &ropt );
        if( -1 == nrcv ) break;

        TRACE( check_fragments( "received new", data_len,
                    lrcv, nrcv, t_delta, g_flog ) );
        lrcv = nrcv;

        if( dsockfd && (nrcv > 0) ) {
            nsent = write_data( &ds, data, nrcv, dsockfd );
            if( -1 == nsent ) break;

            if ( nsent < 0 ) {
                if ( !ALLOW_PAUSES ) break;
                if ( 0 != pause_detect( nsent, &pause_time ) ) break;
            }

            TRACE( check_fragments("sent", nrcv,
                        lsent, nsent, t_delta, g_flog) );
            lsent = nsent;
        }

        if( (dfilefd > 0) && (nrcv > 0) ) {
            nwr = write_data( &ds, data, nrcv, dfilefd );
            if( -1 == nwr )
                break;
            TRACE( check_fragments( "wrote to file",
                    nrcv, lsent, nwr, t_delta, g_flog ) );
            lsent = nwr;
        }

        if( ds.flags & F_SCATTERED ) reset_pkt_registry( &ds );

        if( uf_TRUE == g_uopt.cl_tpstat )
            tpstat_update( ctx, &tps, nsent );

    } /* end of RELAY LOOP */

    /* CLEANUP
     */
    TRACE( (void)tmfprintf( g_flog, "Exited relay loop: received=[%ld], "
        "sent=[%ld], quit=[%ld]\n", (long)nrcv, (long)nsent, (long)quit ) );

    free_dstream_ctx( &ds );
    if( NULL != data ) free( data );

    if( 0 != (quit = must_quit()) ) {
        TRACE( (void)tmfprintf( g_flog, "Child process=[%d] must quit\n",
                    getpid()) );
    }

    return rc;
}


/* process command to relay udp traffic
 *
 */
static int
udp_relay( int sockfd, const char* param, size_t plen,
           const struct in_addr* mifaddr,
           struct server_ctx* ctx )
{
    char                mcast_addr[ IPADDR_STR_SIZE ];
    struct sockaddr_in  addr;

    uint16_t    port;
    pid_t       new_pid;
    int         rc = 0, flags, PID_RESET = 1;
    int         msockfd = -1, sfilefd = -1,
                dfilefd = -1, srcfd = -1;
    char        dfile_name[ MAXPATHLEN ];
    size_t      rcvbuf_len = 0;

    assert( (sockfd > 0) && param && plen && ctx );

    TRACE( (void)tmfprintf( g_flog, "udp_relay : new_socket=[%d] param=[%s]\n",
                        sockfd, param) );
    do {
        rc = parse_udprelay( param, plen, mcast_addr, IPADDR_STR_SIZE, &port );
        if( 0 != rc ) {
            (void) tmfprintf( g_flog, "Error [%d] parsing parameters [%s]\n",
                            rc, param );
            break;
        }

        if( 1 != inet_aton(mcast_addr, &addr.sin_addr) ) {
            (void) tmfprintf( g_flog, "Invalid address: [%s]\n", mcast_addr );
            rc = ERR_INTERNAL;
            break;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons( (short)port );

    } while(0);

    if( 0 != rc ) {
        (void) send_http_response( sockfd, 500, "Service error" );
        return rc;
    }

    /* start the (new) process to relay traffic */

    if( 0 != (new_pid = fork()) ) {
        rc = add_client( ctx, new_pid, mcast_addr, port, sockfd );
        return rc; /* parent returns */
    }

    /* child process:
     */
    TRACE( (void)tmfprintf( g_flog, "Client process=[%d] started "
                "for socket=[%d]\n", getpid(), sockfd) );

    (void) get_pidstr( PID_RESET );

    (void)close( ctx->lsockfd );

    /* close the reading end of the comm. pipe */
    (void)close( ctx->cpipe[0] );
    ctx->cpipe[0] = -1;

    do {
        /* make write end of pipe non-blocking (we don't want to
        * block on pipe write while relaying traffic)
        */
        if( -1 == (flags = fcntl( ctx->cpipe[1], F_GETFL )) ||
            -1 == fcntl( ctx->cpipe[1], F_SETFL, flags | O_NONBLOCK ) ) {
            mperror( g_flog, errno, "%s: fcntl", __func__ );
            rc = -1;
            break;
        }

        if( NULL != g_uopt.dstfile ) {
            (void) snprintf( dfile_name, MAXPATHLEN - 1,
                    "%s.%d", g_uopt.dstfile, getpid() );
            dfilefd = creat( dfile_name, S_IRUSR | S_IWUSR | S_IRGRP );
            if( -1 == dfilefd ) {
                mperror( g_flog, errno, "%s: g_uopt.dstfile open", __func__ );
                rc = -1;
                break;
            }

            TRACE( (void)tmfprintf( g_flog,
                        "Dest file [%s] opened as fd=[%d]\n",
                        dfile_name, dfilefd ) );
        }
        else dfilefd = -1;

        if( NULL != g_uopt.srcfile ) {
            sfilefd = open( g_uopt.srcfile, O_RDONLY | O_NOCTTY );
            if( -1 == sfilefd ) {
                mperror( g_flog, errno, "%s: g_uopt.srcfile open", __func__ );
                rc = -1;
            }
            else {
                TRACE( (void) tmfprintf( g_flog, "Source file [%s] opened\n",
                            g_uopt.srcfile ) );
                srcfd = sfilefd;
            }
        }
        else {
            rc = calc_buf_settings( NULL, &rcvbuf_len );
            if (0 == rc ) {
                rc = setup_mcast_listener( &addr, mifaddr, &msockfd,
                    (g_uopt.nosync_sbuf ? 0 : rcvbuf_len) );
                srcfd = msockfd;
            }
        }
        if( 0 != rc ) break;

        rc = relay_traffic( srcfd, sockfd, ctx, dfilefd, mifaddr );
        if( 0 != rc ) break;

    } while(0);

    if( msockfd > 0 ) {
        close_mcast_listener( msockfd, mifaddr );
    }
    if( sfilefd > 0 ) {
       (void) close( sfilefd );
       TRACE( (void) tmfprintf( g_flog, "Source file [%s] closed\n",
                            g_uopt.srcfile ) );
    }
    if( dfilefd > 0 ) {
       (void) close( dfilefd );
       TRACE( (void) tmfprintf( g_flog, "Dest file [%s] closed\n",
                            dfile_name ) );
    }

    if( 0 != rc ) {
        (void) send_http_response( sockfd, 500, "Service error" );
    }

    (void) close( sockfd );
    free_server_ctx( ctx );

    closelog();

    TRACE( (void)tmfprintf( g_flog, "Child process=[%d] exits with rc=[%d]\n",
                getpid(), rc) );

    if( g_flog && (stderr != g_flog) ) {
        (void) fclose(g_flog);
    }

    free_uopt( &g_uopt );

    rc = ( 0 != rc ) ? ERR_INTERNAL : rc;
    exit(rc);   /* child exits */

    return rc;
}


/* send server status as HTTP response to the given socket
 */
static int
report_status( int sockfd, const struct server_ctx* ctx, int options )
{
    char *buf = NULL;
    int rc = 0;
    ssize_t n, nsent;
    size_t nlen = 0, bufsz, i;

    static size_t BYTES_HDR = 2048;
    static size_t BYTES_PER_CLI = 512;

    assert( (sockfd > 0) && ctx );

    for (bufsz=BYTES_HDR, i=0; i < ctx->clmax; ++i) {
        bufsz+=BYTES_PER_CLI;
    }
    buf = malloc(bufsz);
    if( !buf ) {
        mperror(g_flog, ENOMEM, "malloc for %ld bytes for HTTP buffer "
            "failed in %s", (long)bufsz, __func__ );
        return ERR_INTERNAL;
    }

    (void) memset( buf, 0, sizeof(bufsz) );

    nlen = bufsz;
    rc = mk_status_page( ctx, buf, &nlen, options | MSO_HTTP_HEADER );

    for( n = nsent = 0; (0 == rc) && (nsent < (ssize_t)nlen);  ) {
        errno = 0;
        n = send( sockfd, buf, (int)nlen, 0 );

        if( (-1 == n) && (EINTR != errno) ) {
            mperror(g_flog, errno, "%s: send", __func__);
            rc = ERR_INTERNAL;
            break;
        }

        nsent += n;
    }

    if( 0 != rc ) {
        TRACE( (void)tmfprintf( g_flog, "Error generating status report\n" ) );
    }
    else {
        /* DEBUG only
        TRACE( (void)tmfprintf( g_flog, "Saved status buffer to file\n" ) );
        TRACE( (void)save_buffer(buf, nlen, "/tmp/status-udpxy.html") );
        */
    }

    free(buf);
    return rc;
}


/* process command within a request
 */
static int
process_command( int new_sockfd, struct server_ctx* ctx,
                 const char* param, size_t plen )
{
    int rc = 0;
    const int STAT_OPTIONS = 0;
    const int RESTART_OPTIONS = MSO_SKIP_CLIENTS | MSO_RESTART;

    assert( (new_sockfd > 0) && ctx && param );

    if( 0 == strncmp( ctx->cmd, CMD_UDP, sizeof(ctx->cmd) ) ||
        0 == strncmp( ctx->cmd, CMD_RTP, sizeof(ctx->cmd) ) ) {
        if( ctx->clfree ) {
            rc = udp_relay( new_sockfd, param, plen,
                            &(ctx->mcast_inaddr), ctx );
        }
        else {
            send_http_response( new_sockfd, 401, "Bad request" );
            (void)tmfprintf( g_flog, "Client limit [%d] has been reached.\n",
                    ctx->clmax);
        }
    }
    else if( 0 == strncmp( ctx->cmd, CMD_STATUS, sizeof(ctx->cmd) ) ) {
        rc = report_status( new_sockfd, ctx, STAT_OPTIONS );
    }
    else if( 0 == strncmp( ctx->cmd, CMD_RESTART, sizeof(ctx->cmd) ) ) {
        (void) report_status( new_sockfd, ctx, RESTART_OPTIONS );

        terminate_all_clients( ctx );
        wait_all( ctx );
    }
    else {
        TRACE( (void)tmfprintf( g_flog, "Unrecognized command [%s]"
                    " - ignoring.\n", ctx->cmd) );
        send_http_response( new_sockfd, 200, "OK" );
    }

    return rc;

}


/* process client requests
 */
static int
server_loop( const char* ipaddr, int port,
             const char* mcast_addr )
{
    int                 rc, maxfd,
                        new_sockfd;
    struct sockaddr_in  cliaddr;
    struct in_addr      mcast_inaddr;
    char                param[ 128 ];
    struct server_ctx   srv;
    a_socklen_t         addrlen;
    fd_set              rset;

    assert( (port > 0) && mcast_addr && ipaddr );

    (void)tmfprintf( g_flog, "Starting server [%d]; capacity=[%u] clients\n",
                        getpid(), g_uopt.max_clients );

    if( 1 != inet_aton(mcast_addr, &mcast_inaddr) ) {
        mperror(g_flog, errno, "%s: inet_aton", __func__);
        return ERR_INTERNAL;
    }

    init_server_ctx( &srv, g_uopt.max_clients,
            (ipaddr[0] ? ipaddr : "0.0.0.0") , (uint16_t)port, mcast_addr );

    srv.rcv_tmout = (u_short)g_uopt.rcv_tmout;
    srv.snd_tmout = RLY_SOCK_TIMEOUT;
    srv.mcast_inaddr = mcast_inaddr;

    if( 0 != (rc = setup_listener( ipaddr, port, &srv.lsockfd )) )
        return rc;

    new_sockfd = -1;
    TRACE( (void)tmfprintf( g_flog, "Entering server loop\n") );

    while( !must_quit() ) {
        if( get_childexit() ) {
            wait_terminated( &srv );
        }

        FD_ZERO( &rset );
        FD_SET( srv.lsockfd, &rset );
        FD_SET( srv.cpipe[0], &rset );

        TRACE( (void)tmfprintf( g_flog, "Server is waiting for input: "
                    "socket=[%d], pipe=[%d]\n",
                    srv.lsockfd, srv.cpipe[0]) );

        maxfd = (srv.lsockfd > srv.cpipe[0] ) ? srv.lsockfd : srv.cpipe[0];
        rc = select( maxfd + 1, &rset, NULL, NULL, NULL );
        if( -1 == rc ) {
            if( EINTR == errno ) {
                if( must_quit() ) {
                    TRACE( (void)tmfputs( "Server must quit.\n", g_flog ) );
                    rc = 0;
                    break;
                }
                else
                    continue;
            }

            mperror( g_flog, errno, "%s: select", __func__ );
            break;
        }

        if( FD_ISSET(srv.cpipe[0], &rset) ) {
            if( 0 != tpstat_read( &srv ) )
                break;
            else
                continue;
        }

        addrlen = sizeof(cliaddr);
        do {
            new_sockfd = accept( srv.lsockfd, (struct sockaddr*)&cliaddr, &addrlen );
            if (-1 != new_sockfd) break;
            mperror( g_flog, errno,  "%s: accept", __func__ );

            /* loop for these two, terminate for others */
            if ((ECONNABORTED != errno) || (EINTR != errno)) break;
            if( get_childexit() ) {
                wait_terminated( &srv );
            }
        } while (!must_quit() && -1==new_sockfd);

        /* kill signal or fatal error */
        if ((-1==new_sockfd) || must_quit()) break;

        TRACE( (void)tmfprintf( g_flog, "Accepted socket=[%d]\n",
                    new_sockfd) );

        /* service client's request:
         *   should a non-critical error occur, bail out on the client
         *   but do not stop the server
         */
        do {
            rc = set_timeouts(new_sockfd, new_sockfd,
                    SRVSOCK_TIMEOUT, 0, SRVSOCK_TIMEOUT, 0);
            if( 0 != rc ) break;

            rc = read_command( new_sockfd, srv.cmd, sizeof(srv.cmd),
                    param, sizeof(param) );
            if( 0 != rc ) break;

            rc = process_command( new_sockfd, &srv, param, sizeof(param) );
        }
        while(0);

        (void) close( new_sockfd );
        TRACE( (void)tmfprintf( g_flog, "Closed accepted socket [%d]\n",
                    new_sockfd) );
        new_sockfd = -1;

    } /* server loop */


    TRACE( (void)tmfprintf( g_flog, "Exited server loop\n") );

    if( new_sockfd > 0 ) {
        (void) close( new_sockfd );
    }

    wait_terminated( &srv );
    terminate_all_clients( &srv );
    wait_all( &srv );

    (void) close( srv.lsockfd );

    free_server_ctx( &srv );

    (void)tmfprintf( g_flog, "Server [%d] exits rc=[%d]\n", getpid(), rc );
    return rc;
}


static void
usage( const char* app, FILE* fp )
{
    (void) fprintf(fp, "%s %s (build %d) %s\n", app, VERSION, BUILDNUM,
            COMPILE_MODE );
    (void) fprintf(fp, "usage: %s [-vTS] [-a listenaddr] -p port "
            "[-m mcast_ifc_addr] [-c clients] [-l logfile] "
            "[-B sizeK] [-n nice_incr]\n", app );
    (void) fprintf(fp,
            "\t-v : enable verbose output [default = disabled]\n"
            "\t-S : enable client statistics [default = disabled]\n"
            "\t-T : do NOT run as a daemon [default = daemon if root]\n"
            "\t-a : (IPv4) address/interface to listen on [default = %s]\n"
            "\t-p : port to listen on\n"
            "\t-m : (IPv4) address/interface of (multicast) "
                    "source [default = %s]\n"
            "\t-c : max clients to serve [default = %d, max = %d]\n",
            IPv4_ALL, IPv4_ALL, DEFAULT_CLIENT_COUNT, MAX_CLIENT_COUNT);
    (void)fprintf(fp,
            "\t-l : log output to file [default = stderr]\n"
            "\t-B : buffer size (65536, 32Kb, 1Mb) for inbound (multicast) data [default = %ld bytes]\n"
            "\t-R : maximum messages to store in buffer (-1 = all) "
                    "[default = %d]\n"
            "\t-H : maximum time (sec) to hold data in buffer "
                    "(-1 = unlimited) [default = %d]\n"
            "\t-n : nice value increment [default = %d]\n"
            "\t-M : periodically renew multicast subscription (skip if 0 sec) [default = %d sec]\n",
            (long)DEFAULT_CACHE_LEN, g_uopt.rbuf_msgs, DHOLD_TIMEOUT, g_uopt.nice_incr,
            (int)g_uopt.mcast_refresh );
    (void) fprintf( fp, "Examples:\n"
            "  %s -p 4022 \n"
            "\tlisten for HTTP requests on port 4022, all network interfaces\n"
            "  %s -a lan0 -p 4022 -m lan1\n"
            "\tlisten for HTTP requests on interface lan0, port 4022;\n"
            "\tsubscribe to multicast groups on interface lan1\n",
            app, app);
    (void) fprintf( fp, "\n  %s\n", UDPXY_COPYRIGHT_NOTICE );
    (void) fprintf( fp, "  %s\n\n", UDPXY_CONTACT );
    return;
}


#ifdef HAVE_UDPXREC
extern int udpxy_main( int argc, char* const argv[] );

int
udpxy_main( int argc, char* const argv[] )
#else
int
main( int argc, char* const argv[] )
#endif
{
    int rc, ch, port, custom_log, no_daemon;
    char ipaddr[IPADDR_STR_SIZE],
         mcast_addr[IPADDR_STR_SIZE];

    char pidfile[ MAXPATHLEN ];
    u_short MIN_MCAST_REFRESH = 0, MAX_MCAST_REFRESH = 0;
    char udpxy_finfo[ 80 ] = {0};

/* support for -r -w (file read/write) option is disabled by default;
 * those features are experimental and for dev debugging ONLY
 * */
#ifdef UDPXY_FILEIO
    static const char UDPXY_OPTMASK[] = "TvSa:l:p:m:c:B:n:R:r:w:H:M:";
#else
    static const char UDPXY_OPTMASK[] = "TvSa:l:p:m:c:B:n:R:H:M:";
#endif

    struct sigaction qact, iact, cact, oldact;

    extern const char g_udpxy_app[];

    rc = 0;
    ipaddr[0] = mcast_addr[0] = pidfile[0] = '\0';
    port = -1;
    custom_log = no_daemon = 0;

    init_uopt( &g_uopt );

    while( (0 == rc) && (-1 != (ch = getopt(argc, argv, UDPXY_OPTMASK))) ) {
        switch( ch ) {
            case 'v': set_verbose( &g_uopt.is_verbose );
                      break;
            case 'T': no_daemon = 1;
                      break;
            case 'S': g_uopt.cl_tpstat = uf_TRUE;
                      break;
            case 'a':
                      rc = get_ipv4_address( optarg, ipaddr, sizeof(ipaddr) );
                      if( 0 != rc ) {
                        (void) fprintf( stderr, "Invalid address: [%s]\n",
                                        optarg );
                          rc = ERR_PARAM;
                      }
                      break;

            case 'p':
                      port = atoi( optarg );
                      if( port <= 0 ) {
                        (void) fprintf( stderr, "Invalid port number: [%d]\n",
                                        port );
                        rc = ERR_PARAM;
                      }
                      break;

            case 'm':
                      rc = get_ipv4_address( optarg, mcast_addr,
                              sizeof(mcast_addr) );
                      if( 0 != rc ) {
                        (void) fprintf( stderr, "Invalid multicast address: "
                                "[%s]\n", optarg );
                          rc = ERR_PARAM;
                      }
                      break;

            case 'c':
                      g_uopt.max_clients = atoi( optarg );
                      if( (g_uopt.max_clients < MIN_CLIENT_COUNT) ||
                          (g_uopt.max_clients > MAX_CLIENT_COUNT) ) {
                        (void) fprintf( stderr,
                                "Client count should be between %d and %d\n",
                                MIN_CLIENT_COUNT, MAX_CLIENT_COUNT );
                        rc = ERR_PARAM;
                      }
                      break;

            case 'l':
                      g_flog = fopen( optarg, "a" );
                      if( NULL == g_flog ) {
                        rc = errno;
                        (void) fprintf( stderr, "Error opening logfile "
                                "[%s]: %s\n",
                                optarg, strerror(rc) );
                        rc = ERR_PARAM;
                        break;
                      }

                      Setlinebuf( g_flog );
                      custom_log = 1;
                      break;

            case 'B':
                      rc = a2size(optarg, &g_uopt.rbuf_len);
                      if( 0 != rc ) {
                            (void) fprintf( stderr, "Invalid buffer size: [%s]\n",
                                    optarg );
                            exit( ERR_PARAM );
                      }
                      else if( (g_uopt.rbuf_len < MIN_MCACHE_LEN) ||
                          (g_uopt.rbuf_len > MAX_MCACHE_LEN) ) {
                        fprintf(stderr, "Buffer size "
                                "must be within [%ld-%ld] bytes\n",
                                (long)MIN_MCACHE_LEN, (long)MAX_MCACHE_LEN );
                        rc = ERR_PARAM;
                      }
                      break;

            case 'n':
                      g_uopt.nice_incr = atoi( optarg );
                      if( 0 == g_uopt.nice_incr ) {
                        (void) fprintf( stderr,
                            "Invalid nice-value increment: [%s]\n", optarg );
                        rc = ERR_PARAM;
                        break;
                      }
                      break;

            case 'R':
                      g_uopt.rbuf_msgs = atoi( optarg );
                      if( (g_uopt.rbuf_msgs <= 0) && (-1 != g_uopt.rbuf_msgs) ) {
                        (void) fprintf( stderr,
                                "Invalid Rmsgs size: [%s]\n", optarg );
                        rc = ERR_PARAM;
                        break;
                      }
                      break;

            case 'H':
                      g_uopt.dhold_tmout = (time_t)atoi( optarg );
                      if( (0 == g_uopt.dhold_tmout) ||
                          ((g_uopt.dhold_tmout) < 0 && (-1 != g_uopt.dhold_tmout)) ) {
                        (void) fprintf( stderr, "Invalid value for max time "
                                "to hold buffered data: [%s]\n", optarg );
                        rc = ERR_PARAM;
                        break;
                      }
                      break;

    #ifdef UDPXY_FILEIO
            case 'r':
                      if( 0 != access(optarg, R_OK) ) {
                        perror("source file - access");
                        rc = ERR_PARAM;
                        break;
                      }

                      g_uopt.srcfile = strdup( optarg );
                      break;

            case 'w':
                      g_uopt.dstfile = strdup( optarg );
                      break;
    #endif /* UDPXY_FILEIO */
            case 'M':
                      g_uopt.mcast_refresh = (u_short)atoi( optarg );

                      MIN_MCAST_REFRESH = 30;
                      MAX_MCAST_REFRESH = 64000;
                      if( g_uopt.mcast_refresh &&
                         (g_uopt.mcast_refresh < MIN_MCAST_REFRESH ||
                          g_uopt.mcast_refresh > MAX_MCAST_REFRESH )) {
                            (void) fprintf( stderr, 
                                "Invalid multicast refresh period [%d] seconds, "
                                "min=[%d] sec, max=[%d] sec\n",
                                (int)g_uopt.mcast_refresh,
                                (int)MIN_MCAST_REFRESH, (int)MAX_MCAST_REFRESH );
                            rc = ERR_PARAM;
                            break;
                       }
                      break;

            case ':':
                      (void) fprintf( stderr,
                              "Option [-%c] requires an argument\n",
                                    optopt );
                      rc = ERR_PARAM;
                      break;
            case '?':
                      (void) fprintf( stderr,
                              "Unrecognized option: [-%c]\n", optopt );
                      rc = ERR_PARAM;
                      break;

            default:
                     usage( argv[0], stderr );
                     rc = ERR_PARAM;
                     break;
        }
    } /* while getopt */

    if( 0 != rc ) {
        free_uopt( &g_uopt );
        return rc;
    }

    openlog( g_udpxy_app, LOG_CONS | LOG_PID, LOG_LOCAL0 );

    do {
        if( (argc < 2) || (port <= 0) || (rc != 0) ) {
            usage( argv[0], stderr );
            rc = ERR_PARAM; break;
        }

        if( '\0' == mcast_addr[0] ) {
            (void) strncpy( mcast_addr, IPv4_ALL, sizeof(mcast_addr) - 1 );
        }

        if( !custom_log ) {
            /* in debug mode output goes to stderr, otherwise to /dev/null */
            g_flog = ((uf_TRUE == g_uopt.is_verbose)
                    ? stderr
                    : fopen( "/dev/null", "a" ));
            if( NULL == g_flog ) {
                perror("fopen");
                rc = ERR_INTERNAL; break;
            }
        }

        if( 0 == geteuid() ) {
            if( !no_daemon ) {
                if( stderr == g_flog ) {
                    (void) fprintf( stderr,
                        "Logfile must be specified to run "
                        "in verbose mode in background\n" );
                    rc = ERR_PARAM; break;
                }

                if( 0 != (rc = daemonize(0, g_flog)) ) {
                    rc = ERR_INTERNAL; break;
                }
            }

            rc = set_pidfile( g_udpxy_app, port, pidfile, sizeof(pidfile) );
            if( 0 != rc ) {
                mperror( g_flog, errno, "set_pidfile" );
                rc = ERR_INTERNAL; break;
            }

            if( 0 != (rc = make_pidfile( pidfile, getpid(), g_flog )) )
                break;
        }

        qact.sa_handler = handle_quitsigs;
        sigemptyset(&qact.sa_mask);
        qact.sa_flags = 0;

        if( (sigaction(SIGTERM, &qact, &oldact) < 0) ||
            (sigaction(SIGQUIT, &qact, &oldact) < 0) ||
            (sigaction(SIGINT,  &qact, &oldact) < 0)) {
            perror("sigaction-quit");
            rc = ERR_INTERNAL; break;
        }

        iact.sa_handler = SIG_IGN;
        sigemptyset(&iact.sa_mask);
        iact.sa_flags = 0;

        if( (sigaction(SIGPIPE, &iact, &oldact) < 0) ) {
            perror("sigaction-ignore");
            rc = ERR_INTERNAL; break;
        }

        cact.sa_handler = handle_sigchld;
        sigemptyset(&cact.sa_mask);
        cact.sa_flags = 0;

        if( sigaction(SIGCHLD, &cact, &oldact) < 0 ) {
            perror("sigaction-sigchld");
            rc = ERR_INTERNAL; break;
        }

        (void) snprintf( udpxy_finfo, sizeof(udpxy_finfo),
                "%s %s (build %d) %s", g_udpxy_app, VERSION, BUILDNUM,
            COMPILE_MODE );

        syslog( LOG_NOTICE, "%s is starting\n",udpxy_finfo );
        TRACE( printcmdln( g_flog, udpxy_finfo, argc, argv ) );

        rc = server_loop( ipaddr, port, mcast_addr );

        syslog( LOG_NOTICE, "%s is exiting with rc=[%d]\n",
                udpxy_finfo, rc);
        TRACE( tmfprintf( g_flog, "%s is exiting with rc=[%d]\n",
                    g_udpxy_app, rc ) );
        TRACE( printcmdln( g_flog, udpxy_finfo, argc, argv ) );
    } while(0);

    if( '\0' != pidfile[0] ) {
        if( -1 == unlink(pidfile) ) {
            mperror( g_flog, errno, "unlink [%s]", pidfile );
        }
    }

    if( g_flog && (stderr != g_flog) ) {
        (void) fclose(g_flog);
    }

    closelog();
    free_uopt( &g_uopt );

    return rc;
}

/* __EOF__ */

