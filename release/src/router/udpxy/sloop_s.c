/* @(#) abstracted server loop routine */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <netinet/in.h>

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

#include "osdef.h"  /* os-specific definitions */
#include "udpxy.h"

#include "mtrace.h"
#include "util.h"

#include "ctx.h"
#include "uopt.h"
#include "netop.h"

extern FILE*                 g_flog;
extern struct udpxy_opt      g_uopt;

extern sig_atomic_t must_quit();
extern void         wait_terminated  (struct server_ctx* ctx);
extern void         tmout_requests   (tmfd_t* asock, size_t *alen);
extern void         process_requests (tmfd_t* asock, size_t *alen,
                                      fd_set* rset, struct server_ctx* srv);
extern void         accept_requests  (int sockfd, tmfd_t* asock, size_t* alen);
extern void         terminate_all_clients (struct server_ctx* ctx);
extern void         wait_all (struct server_ctx* ctx);

static const char SLOOP_TAG[] = "select(2)";

/* global server context */
struct server_ctx  g_srv;


/*********************************************************/

/* process client requests */
int
srv_loop( const char* ipaddr, int port,
             const char* mcast_addr )
{
    int                 rc, maxfd, err, nrdy, i;
    struct in_addr      mcast_inaddr;
    fd_set              rset;
    struct timeval      tmout, idle_tmout, *ptmout = NULL;
    tmfd_t              *asock = NULL;
    size_t              n = 0, nasock = 0, max_nasock = LQ_BACKLOG;
    sigset_t            oset, bset;

    static const long IDLE_TMOUT_SEC = 30;

    assert( (port > 0) && mcast_addr && ipaddr );

    (void)tmfprintf( g_flog, "Server is starting up, max clients = [%u]\n",
                        g_uopt.max_clients );
    asock = calloc (max_nasock, sizeof(*asock));
    if (!asock) {
        mperror (g_flog, ENOMEM, "%s: calloc", __func__);
        return ERR_INTERNAL;
    }

    if( 1 != inet_aton(mcast_addr, &mcast_inaddr) ) {
        mperror(g_flog, errno, "%s: inet_aton", __func__);
        return ERR_INTERNAL;
    }

    init_server_ctx( &g_srv, g_uopt.max_clients,
            (ipaddr[0] ? ipaddr : "0.0.0.0") , (uint16_t)port, mcast_addr );

    g_srv.rcv_tmout = (u_short)g_uopt.rcv_tmout;
    g_srv.snd_tmout = RLY_SOCK_TIMEOUT;
    g_srv.mcast_inaddr = mcast_inaddr;

    /* NB: server socket is non-blocking! */
    if( 0 != (rc = setup_listener( ipaddr, port, &g_srv.lsockfd,
            g_uopt.lq_backlog )) ) {
        return rc;
    }

    sigemptyset (&bset);
    sigaddset (&bset, SIGINT);
    sigaddset (&bset, SIGQUIT);
    sigaddset (&bset, SIGCHLD);
    sigaddset (&bset, SIGTERM);

    (void) sigprocmask (SIG_BLOCK, &bset, &oset);

    TRACE( (void)tmfprintf( g_flog, "Entering server loop [%s]\n",
        SLOOP_TAG) );
    while (1) {
        FD_ZERO( &rset );
        FD_SET( g_srv.lsockfd, &rset );
        FD_SET( g_srv.cpipe[0], &rset );

        maxfd = (g_srv.lsockfd > g_srv.cpipe[0] ) ? g_srv.lsockfd : g_srv.cpipe[0];
        for (i = 0; (size_t)i < nasock; ++i) {
            assert (asock[i].fd >= 0);
            FD_SET (asock[i].fd, &rset);
            if (asock[i].fd > maxfd) maxfd = asock[i].fd;
        }

        /* if there are accepted sockets - apply specified time-out
         */
        tmout.tv_sec = g_uopt.ssel_tmout;
        tmout.tv_usec = 0;

        idle_tmout.tv_sec = IDLE_TMOUT_SEC;
        idle_tmout.tv_usec = 0;

        /* enforce *idle* select(2) timeout to alleviate signal contention */
        ptmout = ((nasock > 0) && (g_uopt.ssel_tmout > 0)) ? &tmout : &idle_tmout;

        TRACE( (void)tmfprintf( g_flog, "Waiting for input from [%ld] fd's, "
            "%s timeout\n", (long)(2 + nasock), (ptmout ? "with" : "NO")));

        if (ptmout && ptmout->tv_sec) {
            TRACE( (void)tmfprintf (g_flog, "select() timeout set to "
            "[%ld] seconds\n", ptmout->tv_sec) );
        }

        (void) sigprocmask (SIG_UNBLOCK, &bset, NULL);
        if( must_quit() ) {
            TRACE( (void)tmfputs( "Must quit now\n", g_flog ) );
            rc = 0; break;
        }

        nrdy = select (maxfd + 1, &rset, NULL, NULL, ptmout);
        err = errno;
        (void) sigprocmask (SIG_BLOCK, &bset, NULL);

        if( must_quit() ) {
            TRACE( (void)tmfputs( "Must quit now\n", g_flog ) );
            rc = 0; break;
        }
        wait_terminated( &g_srv );

        if( nrdy < 0 ) {
            if (EINTR == err) {
                TRACE( (void)tmfputs ("INTERRUPTED, yet "
                        "will continue.\n", g_flog)  );
                rc = 0; continue;
            }

            mperror( g_flog, err, "%s: select", __func__ );
            break;
        }

        TRACE( (void)tmfprintf (g_flog, "Got %ld requests\n", (long)nrdy) );
        if (0 == nrdy) {    /* time-out */
            tmout_requests (asock, &nasock);
            rc = 0; continue;
        }

        if( FD_ISSET(g_srv.cpipe[0], &rset) ) {
            (void) tpstat_read( &g_srv );
            if (--nrdy <= 0) continue;
        }

        if ((0 < nasock) &&
                 (0 < (nrdy - (FD_ISSET(g_srv.lsockfd, &rset) ? 1 : 0)))) {
            process_requests (asock, &nasock, &rset, &g_srv);
            /* n now contains # (yet) unprocessed accepted sockets */
        }

        if (FD_ISSET(g_srv.lsockfd, &rset)) {
            if (nasock >= max_nasock) {
                (void) tmfprintf (g_flog, "Cannot accept sockets beyond "
                    "the limit [%ld/%ld], skipping\n",
                    (long)nasock, (long)max_nasock);
            }
            else {
                n = max_nasock - nasock; /* append asock */
                accept_requests (g_srv.lsockfd, &(asock[nasock]), &n);
                nasock += n;
            }
        }
    } /* server loop */

    TRACE( (void)tmfprintf( g_flog, "Exited server loop [%s]\n", SLOOP_TAG) );

    for (i = 0; (size_t)i < nasock; ++i) {
        if (asock[i].fd > 0) (void) close (asock[i].fd);
    }
    free (asock);

    /* receive additional (blocked signals) */
    (void) sigprocmask (SIG_SETMASK, &oset, NULL);
    wait_terminated( &g_srv );
    terminate_all_clients( &g_srv );
    wait_all( &g_srv );

    if (0 != close( g_srv.lsockfd )) {
        mperror (g_flog, errno, "server socket close");
    }

    free_server_ctx( &g_srv );

    (void)tmfprintf( g_flog, "Server exits with rc=[%d]\n", rc );
    return rc;
}


/* __EOF__ */

