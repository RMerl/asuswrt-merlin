/* @(#) udpxrec utility: main module
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
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef __USE_LARGEFILE64
    #define __USE_LARGEFILE64
#endif

#ifndef __USE_FILE_OFFSET64
    #define __USE_FILE_OFFSET64
#endif

#include <fcntl.h>

#include "osdef.h"  /* os-specific definitions */
#include "udpxy.h"
#include "util.h"
#include "uopt.h"
#include "ctx.h"
#include "dpkt.h"
#include "mtrace.h"
#include "netop.h"
#include "ifaddr.h"


/* external globals */

extern FILE*  g_flog;
extern volatile sig_atomic_t g_quit;
extern const char g_udpxrec_app[];

static volatile sig_atomic_t g_alarm = 0;

/* globals
 */

struct udpxrec_opt g_recopt;
static char g_app_info[ 80 ] = {0};

/* handler for signals requestin application exit
 */
static void
handle_quitsigs(int signo)
{
    g_quit = 1;
    if( SIGALRM == signo ) g_alarm = 1;

    TRACE( (void)tmfprintf( g_flog,
                "*** Caught SIGNAL [%d] in process=[%d] ***\n",
                signo, getpid()) );
    return;
}


/* return 1 if the application must gracefully quit
 */
static sig_atomic_t
must_quit() { return g_quit; }


static void
usage( const char* app, FILE* fp )
{
    extern const char  UDPXY_COPYRIGHT_NOTICE[];
    extern const char  UDPXY_CONTACT[];

    (void) fprintf(fp, "%s\n", g_app_info);
    (void) fprintf(fp, "usage: %s [-v] [-b begin_time] [-e end_time] "
            "[-M maxfilesize] [-p pidfile] [-B bufsizeK] [-n nice_incr] "
            "[-m mcast_ifc_addr] [-l logfile] "
            "-c src_addr:port dstfile\n",
            app );

    (void) fprintf(fp,
            "\t-v : enable verbose output [default = disabled]\n"
            "\t-b : begin recording at [+]dd:hh24:mi.ss\n"
            "\t-e : stop recording at [+]dd:hh24:mi.ss\n"
            "\t-M : maximum size of destination file\n"
            "\t-p : pidfile for this process [MUST be specified if daemon]\n"
            "\t-B : buffer size for inbound (multicast) data [default = %ld bytes]\n"
            "\t-R : maximum messages to store in buffer (-1 = all) "
                    "[default = -1]\n",
            (long)DEFAULT_CACHE_LEN );
    (void)fprintf(fp,
            "\t-T : do NOT run as a daemon [default = daemon if root]\n"
            "\t-n : nice value increment [default = %d]\n"
            "\t-m : name or address of multicast interface to read from\n"
            "\t-c : multicast channel to record - ipv4addr:port\n"
            "\t-l : write output into the logfile\n"
            "\t-u : seconds to wait before updating on how long till recording starts\n",
            g_recopt.nice_incr );

    (void) fprintf( fp, "Examples:\n"
            "  %s -b 15:45.00 -e +2:00.00 -M 1.5Gb -n 2 -B 64K -c 224.0.11.31:5050 "
            " /opt/video/tv5.mpg \n"
            "\tbegin recording multicast channel 224.0.11.31:5050 at 15:45 today,\n"
            "\tfinish recording in two hours or if destination file size >= 1.5 Gb;\n"
            "\tset socket buffer to 64Kb; increment nice value by 2;\n"
            "\twrite captured video to /opt/video/tv5.mpg\n",
            g_udpxrec_app );
    (void) fprintf( fp, "\n  %s\n", UDPXY_COPYRIGHT_NOTICE );
    (void) fprintf( fp, "  %s\n\n", UDPXY_CONTACT );
    return;
}


/* update wait status
 *
 */
static void
update_waitstat( FILE* log, time_t end_time )
{
    /* TODO: make sure to distinguish if log is a terminal
     * terminal gets the progress bar, etc. */

    double tdiff = difftime( end_time, time(NULL) );
    (void) tmfprintf( log, "%.0f\tseconds till recording begins\n",
            tdiff );
}


/* wait till the given time, update wait status
 * every update_sec seconds
 *
 */
static int
wait_till( time_t endtime, int update_sec )
{
    u_int sec2wait = 0;
    u_int unslept = 0;
    time_t now = time(NULL);
    sig_atomic_t quit = 0;

    TRACE( (void)tmfprintf( g_flog, "%s: waiting till time=[%ld], now=[%ld]\n",
                __func__, (long)endtime, (long)now ) );

    if( now >= endtime ) return 0;

    (void) tmfprintf( g_flog, "[%ld] seconds before recording begins\n",
                        (long)(endtime - now) );

    if( (update_sec <= 0) ||
        ((now + update_sec) > endtime) ) {
        unslept = sleep( endtime - now );
    }
    else {
        while( !(quit = must_quit()) && (now < endtime) ) {
            sec2wait = (now + update_sec) <= endtime
                        ? update_sec : (endtime - now);
            update_waitstat( g_flog, endtime );

            TRACE( (void)tmfprintf( g_flog, "Waiting for [%u] more seconds.\n",
                        sec2wait ) );

            unslept = sleep( sec2wait );
            now = time(NULL);
        }
    }

    TRACE( (void)tmfprintf( g_flog, "[%u] seconds unslept, quit=[%d]\n", unslept,
                (long)quit ) );
    return (unslept ? ERR_INTERNAL : 0);
}


static int
calc_buf_settings( ssize_t* bufmsgs, size_t* sock_buflen )
{
    ssize_t nmsgs = -1, max_buf_used = -1, env_snd_buflen = -1;
    size_t buflen = 0;

    /* how many messages should we process? */
    nmsgs = (g_recopt.rbuf_msgs > 0) ? g_recopt.rbuf_msgs :
             (int)g_recopt.bufsize / ETHERNET_MTU;

    /* how many bytes could be written at once
        * to the send socket */
    max_buf_used = (g_recopt.rbuf_msgs > 0)
        ? (ssize_t)(nmsgs * ETHERNET_MTU) : g_recopt.bufsize;
    if (max_buf_used > g_recopt.bufsize) {
        max_buf_used = g_recopt.bufsize;
    }

    assert( max_buf_used >= 0 );

    env_snd_buflen = get_sizeval( "UDPXREC_SOCKBUF_LEN", 0);
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



/* subscribe to the (configured) multicast channel
 */
static int
subscribe( int* sockfd, struct in_addr* mcast_inaddr )
{
    struct sockaddr_in sa;
    const char* ipaddr = g_recopt.rec_channel;
    size_t rcvbuf_len = 0;
    int rc = 0;

    assert( sockfd && mcast_inaddr );

    if( 1 != inet_aton( ipaddr, &sa.sin_addr ) ) {
        mperror( g_flog, errno,
                "%s: Invalid subscription [%s:%d]: inet_aton",
                __func__, ipaddr, g_recopt.rec_port );
        return -1;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons( (uint16_t)g_recopt.rec_port );

    if( 1 != inet_aton( g_recopt.mcast_addr, mcast_inaddr ) ) {
        mperror( g_flog, errno,
                "%s: Invalid multicast interface: [%s]: inet_aton",
                __func__, g_recopt.mcast_addr );
        return -1;
    }

    rc = calc_buf_settings( NULL, &rcvbuf_len );
    if (0 != rc) return rc;

    return setup_mcast_listener( &sa, mcast_inaddr,
            sockfd, (g_recopt.nosync_sbuf ? 0 : rcvbuf_len) );
}


/* record network stream as per spec in opt
 */
static int
record()
{
    int rsock = -1, destfd = -1, rc = 0, wtime_sec = 0;
    struct in_addr raddr;
    struct timeval rtv;
    struct dstream_ctx ds;
    ssize_t nmsgs = 0;
    ssize_t nrcv = -1, lrcv = -1, t_delta = 0;
    uint64_t n_total = 0;
    ssize_t nwr = -1, lwr = -1;
    sig_atomic_t quit = 0;
    struct rdata_opt ropt;
    int oflags = 0;

    char* data = NULL;

    static const u_short RSOCK_TIMEOUT  = 5;
    extern const char CMD_UDP[];

    /* NOPs to eliminate warnings in lean version */
    (void)&t_delta; (void)&lrcv;
    t_delta = lrcv = lwr = 0; quit=0;

    check_fragments( NULL, 0, 0, 0, 0, g_flog );

    /* init */
    do {
        data = malloc( g_recopt.bufsize );
        if( NULL == data ) {
            mperror(g_flog, errno, "%s: cannot allocate [%ld] bytes",
                    __func__, (long)g_recopt.bufsize );
            rc = ERR_INTERNAL;
            break;
        }

        rc = subscribe( &rsock, &raddr );
        if( 0 != rc ) break;

        rtv.tv_sec = RSOCK_TIMEOUT;
        rtv.tv_usec = 0;

        rc = setsockopt( rsock, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv) );
        if( -1 == rc ) {
            mperror(g_flog, errno, "%s: setsockopt - SO_RCVTIMEO",
                    __func__);
            rc = ERR_INTERNAL;
            break;
        }

        oflags = O_CREAT | O_TRUNC | O_WRONLY |
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        # if defined(O_LARGEFILE)
            /* O_LARGEFILE is not defined under FreeBSD ??-7.1 */
            oflags |= O_LARGEFILE;
        # endif
        destfd = open( g_recopt.dstfile, oflags,
                (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
        if( -1 == destfd ) {
            mperror( g_flog, errno, "%s: cannot create destination file [%s]",
                     __func__, g_recopt.dstfile );
            rc = ERR_INTERNAL;
            break;
        }

        rc = calc_buf_settings( &nmsgs, NULL );
        if (0 != rc) return -1;

        if( nmsgs < (ssize_t)1 ) {
            (void) tmfprintf( g_flog, "Buffer for inbound data is too small [%ld] bytes; "
                    "the minimum size is [%ld] bytes\n",
                    (long)g_recopt.bufsize, (long)ETHERNET_MTU );
            rc = ERR_PARAM;
            break;
        }

        TRACE( (void)tmfprintf( g_flog, "Inbound buffer set to "
                        "[%d] messages\n", nmsgs ) );

        rc = init_dstream_ctx( &ds, CMD_UDP, NULL, nmsgs );
        if( 0 != rc ) return -1;

        (void) set_nice( g_recopt.nice_incr, g_flog );

        /* set up alarm to break main loop */
        if( 0 != g_recopt.end_time ) {
            wtime_sec = (int)difftime( g_recopt.end_time, time(NULL) );
            assert( wtime_sec >= 0 );

            (void) alarm( wtime_sec );

            (void)tmfprintf( g_flog, "Recording will end in [%d] seconds\n",
                    wtime_sec );
        }
    } while(0);

    /* record loop */
    ropt.max_frgs = g_recopt.rbuf_msgs;
    ropt.buf_tmout = -1;

    for( n_total = 0; (0 == rc) && !(quit = must_quit()); ) {
        nrcv = read_data( &ds, rsock, data, g_recopt.bufsize, &ropt );
        if( -1 == nrcv ) { rc = ERR_INTERNAL; break; }

        if( 0 == n_total ) {
            (void) tmfprintf( g_flog, "Recording to file=[%s] started.\n",
                    g_recopt.dstfile );
        }

        TRACE( check_fragments( "received new", g_recopt.bufsize,
                    lrcv, nrcv, t_delta, g_flog ) );
        lrcv = nrcv;

        if( nrcv > 0 ) {
            if( g_recopt.max_fsize &&
                ((int64_t)(n_total + nrcv) >= g_recopt.max_fsize) ) {
                break;
            }

            nwr = write_data( &ds, data, nrcv, destfd );
            if( -1 == nwr ) { rc = ERR_INTERNAL; break; }

            n_total += (size_t)nwr;
            /*
            TRACE( tmfprintf( g_flog, "Wrote [%ld] to file, total=[%ld]\n",
                        (long)nwr, (long)n_total ) );
            */

            TRACE( check_fragments( "wrote to file",
                    nrcv, lwr, nwr, t_delta, g_flog ) );
            lwr = nwr;
        }

        if( ds.flags & F_SCATTERED ) reset_pkt_registry( &ds );

    } /* record loop */

    (void) tmfprintf( g_flog, "Recording to file=[%s] stopped at filesize=[%lu] bytes\n",
                      g_recopt.dstfile, (u_long)n_total );

    /* CLEANUP
     */
    (void) alarm(0);

    TRACE( (void)tmfprintf( g_flog, "Exited record loop: wrote [%lu] bytes to file [%s], "
                    "rc=[%d], alarm=[%ld], quit=[%ld]\n",
                    (u_long)n_total, g_recopt.dstfile, rc, g_alarm, (long)quit ) );

    free_dstream_ctx( &ds );
    if( data ) free( data );

    close_mcast_listener( rsock, &raddr );
    if( destfd >= 0 ) (void) close( destfd );

    if( quit )
        TRACE( (void)tmfprintf( g_flog, "%s process must quit\n",
                        g_udpxrec_app ) );

    return rc;
}


/* make sure the channel is valid: subscribe/close
 */
static int
verify_channel()
{
    struct in_addr mcast_inaddr;
    int sockfd = -1, rc = -1;
    char buf[16];
    ssize_t nrd = -1;
    struct timeval rtv;

    static const time_t MSOCK_TMOUT_SEC = 2;

    rc = subscribe( &sockfd, &mcast_inaddr );
    do {
        if( rc ) break;

        rtv.tv_sec = MSOCK_TMOUT_SEC;
        rtv.tv_usec = 0;
        rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv) );
        if( -1 == rc ) {
            mperror(g_flog, errno, "%s: setsockopt - SO_RCVTIMEO",
                __func__);
            rc = ERR_INTERNAL; break;
        }

        /* attempt to read from the socket to
         * make sure the channel is alive
         */
        nrd = read( sockfd, buf, sizeof(buf) );
        if( nrd <= 0 ) {
            rc = errno;
            mperror( g_flog, errno, "channel read" );
            if( EAGAIN == rc ) {
                (void) tmfprintf( g_flog,
                        "failed to read from [%s:%d]\n",
                        g_recopt.rec_channel, g_recopt.rec_port );
            }
            rc = ERR_INTERNAL; break;
        }

        TRACE( (void)tmfprintf( g_flog, "%s: read [%ld] bytes "
                    "from source channel\n", __func__, nrd ) );
    } while(0);

    if( sockfd >= 0 ) {
        close_mcast_listener( sockfd, &mcast_inaddr );
    }

    return rc;
}


/* set up signal handling
 */
static int
setup_signals()
{
    struct sigaction qact, oldact;

    qact.sa_handler = handle_quitsigs;
    sigemptyset(&qact.sa_mask);
    qact.sa_flags = 0;

    if( (sigaction(SIGTERM, &qact, &oldact) < 0) ||
        (sigaction(SIGQUIT, &qact, &oldact) < 0) ||
        (sigaction(SIGINT,  &qact, &oldact) < 0) ||
        (sigaction(SIGALRM, &qact, &oldact) < 0) ||
        (sigaction(SIGPIPE, &qact, &oldact) < 0)) {
        perror("sigaction-quit");

        return ERR_INTERNAL;
    }

    return 0;
}


extern int udpxrec_main( int argc, char* const argv[] );

/* main() for udpxrec module
 */
int udpxrec_main( int argc, char* const argv[] )
{
    int rc = 0, ch = 0, custom_log = 0, no_daemon = 0;
    static const char OPTMASK[] = "vb:e:M:p:B:n:m:l:c:R:u:T";
    time_t now = time(NULL);
    char now_buf[ 32 ] = {0}, sel_buf[ 32 ] = {0}, app_finfo[80] = {0};

    extern int optind, optopt;
    extern const char IPv4_ALL[];

    mk_app_info(g_udpxrec_app, g_app_info, sizeof(g_app_info) - 1);

    if( argc < 2 ) {
        usage( argv[0], stderr );
        return ERR_PARAM;
    }

    rc = init_recopt( &g_recopt );
    while( (0 == rc) && (-1 != (ch = getopt( argc, argv, OPTMASK ))) ) {
        switch(ch) {
            case 'T':   no_daemon = 1; break;
            case 'v':   set_verbose( &g_recopt.is_verbose ); break;
            case 'b':
                        if( (time_t)0 != g_recopt.end_time ) {
                            (void) fprintf( stderr, "Cannot specify start-recording "
                                    "time after end-recording time has been set\n" );
                        }

                        rc = a2time( optarg, &g_recopt.bg_time, time(NULL) );
                        if( 0 != rc ) {
                            (void) fprintf( stderr, "Invalid time: [%s]\n", optarg );
                            rc = ERR_PARAM;
                        }
                        else {
                            if( g_recopt.bg_time < now ) {
                                (void)strncpy( now_buf, Zasctime(localtime( &now )),
                                        sizeof(now_buf) );
                                (void)strncpy( sel_buf,
                                        Zasctime(localtime( &g_recopt.bg_time )),
                                        sizeof(sel_buf) );

                                (void) fprintf( stderr,
                                        "Selected %s time is in the past, "
                                        "now=[%s], selected=[%s]\n", "start",
                                        now_buf, sel_buf );
                                rc = ERR_PARAM;
                            }
                        }

                        break;
            case 'e':
                        if( (time_t)0 == g_recopt.bg_time ) {
                            g_recopt.bg_time = time(NULL);
                            (void)fprintf( stderr,
                                    "Start-recording time defaults to now [%s]\n",
                                    Zasctime( localtime( &g_recopt.bg_time ) ) );
                        }

                        rc = a2time( optarg, &g_recopt.end_time, g_recopt.bg_time );
                        if( 0 != rc ) {
                            (void) fprintf( stderr, "Invalid time: [%s]\n", optarg );
                            rc = ERR_PARAM;
                        }
                        else {
                            if( g_recopt.end_time < now ) {
                                (void)strncpy( now_buf, Zasctime(localtime( &now )),
                                        sizeof(now_buf) );
                                (void)strncpy( sel_buf,
                                        Zasctime(localtime( &g_recopt.end_time )),
                                        sizeof(sel_buf) );

                                (void) fprintf( stderr,
                                        "Selected %s time is in the past, "
                                        "now=[%s], selected=[%s]\n", "end",
                                        now_buf, sel_buf );
                                rc = ERR_PARAM;
                            }
                        }
                        break;

            case 'M':
                        rc = a2int64( optarg, &g_recopt.max_fsize );
                        if( 0 != rc ) {
                            (void) fprintf( stderr, "Invalid file size: [%s]\n",
                                    optarg );
                            rc = ERR_PARAM;
                        }
                        break;
            case 'p':
                        g_recopt.pidfile = strdup(optarg);
                        break;

            case 'B':
                        rc = a2size( optarg, &g_recopt.bufsize );
                        if( 0 != rc ) {
                            (void) fprintf( stderr, "Invalid buffer size: [%s]\n",
                                    optarg );
                            rc = ERR_PARAM;
                        }
                        else if( (g_recopt.bufsize < MIN_MCACHE_LEN) ||
                                 (g_recopt.bufsize > MAX_MCACHE_LEN)) {
                            (void) fprintf( stderr,
                                "Buffer size must be in [%ld-%ld] bytes range\n",
                                (long)MIN_MCACHE_LEN, (long)MAX_MCACHE_LEN );
                            rc = ERR_PARAM;
                        }

                        break;
            case 'n':
                      g_recopt.nice_incr = atoi( optarg );
                      if( 0 == g_recopt.nice_incr ) {
                        (void) fprintf( stderr,
                            "Invalid nice-value increment: [%s]\n", optarg );
                        rc = ERR_PARAM;
                      }
                      break;
            case 'm':
                      rc = get_ipv4_address( optarg, g_recopt.mcast_addr,
                              sizeof(g_recopt.mcast_addr) );
                      if( 0 != rc ) {
                        (void) fprintf( stderr, "Invalid multicast address: [%s]\n",
                                        optarg );
                          rc = ERR_PARAM;
                      }
                      break;
            case 'l':
                      g_flog = fopen( optarg, "a" );
                      if( NULL == g_flog ) {
                        rc = errno;
                        (void) fprintf( stderr, "Error opening logfile [%s]: %s\n",
                                optarg, strerror(rc) );
                        rc = ERR_PARAM; break;
                      }

                      Setlinebuf( g_flog );
                      custom_log = 1;
                      break;

            case 'c':
                      rc = get_addrport( optarg, g_recopt.rec_channel,
                                         sizeof( g_recopt.rec_channel ),
                                         &g_recopt.rec_port );
                      if( 0 != rc ) rc = ERR_PARAM;
                      break;

            case 'R':
                      g_recopt.rbuf_msgs = atoi( optarg );
                      if( (g_recopt.rbuf_msgs <= 0) && (-1 != g_recopt.rbuf_msgs) ) {
                        (void) fprintf( stderr,
                                "Invalid rcache size: [%s]\n", optarg );
                        rc = ERR_PARAM;
                      }
                      break;

            case 'u':
                      g_recopt.waitupd_sec = atoi(optarg);
                      if( g_recopt.waitupd_sec <= 0 ) {
                          (void) fprintf( stderr, "Invalid wait-update value [%s] "
                                  "(must be a number > 0)\n", optarg );
                          rc = ERR_PARAM;
                      }
                      break;

            case ':':
                      (void) fprintf( stderr, "Option [-%c] requires an argument\n",
                              optopt );
                      rc = ERR_PARAM; break;
            case '?':
                      (void) fprintf( stderr, "Unrecognized option: [-%c]\n", optopt );
                      rc = ERR_PARAM; break;
            default:
                      usage( argv[0], stderr );
                      rc = ERR_PARAM; break;

        } /* switch */
    } /* while getopt */

    if( 0 == rc ) {
        if( optind >= argc ) {
            (void) fputs( "Missing destination file parameter\n", stderr );
            rc = ERR_PARAM;
        }
        else {
            g_recopt.dstfile = strdup( argv[optind] );
        }

        if( !(g_recopt.max_fsize > 0 || g_recopt.end_time)  ) {
            (void) fputs( "Must specify either max file [-M] size "
                    "or end time [-e]\n", stderr );
            rc = ERR_PARAM;
        }

        if( !g_recopt.rec_channel[0] || !g_recopt.rec_port ) {
            (void) fputs( "Must specify multicast channel to record from\n",
                    stderr );
            rc = ERR_PARAM;
        }
    }

    if( rc ) {
        free_recopt( &g_recopt );
        return rc;
    }

    do {
        if( '\0' == g_recopt.mcast_addr[0] ) {
            (void) strncpy( g_recopt.mcast_addr, IPv4_ALL,
                    sizeof(g_recopt.mcast_addr) - 1 );
        }

        if( !custom_log ) {
            /* in debug mode output goes to stderr, otherwise to /dev/null */
            g_flog = ((uf_TRUE == g_recopt.is_verbose)
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

                if( NULL == g_recopt.pidfile ) {
                    (void) fprintf( stderr, "pidfile must be specified "
                            "to run as daemon\n" );
                    rc = ERR_PARAM; break;
                }

                if( 0 != (rc = daemonize(0, g_flog)) ) {
                    rc = ERR_INTERNAL; break;
                }
            }

        } /* 0 == geteuid() */

        if( NULL != g_recopt.pidfile ) {
            rc = make_pidfile( g_recopt.pidfile, getpid(), g_flog );
            if( 0 != rc ) break;
        }

        (void) set_nice( g_recopt.nice_incr, g_flog );

        if( 0 != (rc = setup_signals()) ) break;

        TRACE( fprint_recopt( g_flog, &g_recopt ) );

        TRACE( printcmdln( g_flog, g_app_info, argc, argv ) );

        if( g_recopt.bg_time ) {
            if( 0 != (rc = verify_channel()) || g_quit )
                break;

            rc = wait_till( g_recopt.bg_time, g_recopt.waitupd_sec );
            if( rc || g_quit ) break;
        }

        rc = record();

        if( NULL != g_recopt.pidfile ) {
            if( -1 == unlink(g_recopt.pidfile) ) {
                mperror( g_flog, errno, "unlink [%s]", g_recopt.pidfile );
            }
        }
    }
    while(0);

    if( g_flog ) {
        (void)tmfprintf( g_flog, "%s is exiting with rc=[%d]\n",
                app_finfo, rc );
    }

    if( g_flog && (stderr != g_flog) ) {
        (void) fclose(g_flog);
    }

    free_recopt( &g_recopt );

    return rc;
}


/* __EOF__ */

