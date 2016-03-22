/* @(#) client/server context implementation
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
#include <sys/stat.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "ctx.h"
#include "udpxy.h"
#include "util.h"
#include "mtrace.h"

extern FILE* g_flog;
extern const char IPv4_ALL[];

/* initialize server context data
 */
int
init_server_ctx( struct server_ctx* ctx,
                 const size_t max,
                 const char* laddr, uint16_t lport,
                 const char* mifc_addr )
{
    int flags = -1;

    assert( lport && mifc_addr && ctx && max );

    ctx->lsockfd = 0;
    (void) strncpy( ctx->listen_addr, (laddr ? laddr : IPv4_ALL),
                    IPADDR_STR_SIZE );
    ctx->listen_addr[ IPADDR_STR_SIZE - 1 ] = '\0';

    ctx->listen_port = lport;

    (void) strncpy( ctx->mcast_ifc_addr, mifc_addr, IPADDR_STR_SIZE );
    ctx->mcast_ifc_addr[ IPADDR_STR_SIZE - 1 ] = '\0';

    ctx->cl = calloc(max, sizeof(struct client_ctx));
    if( NULL == ctx->cl ) {
        mperror( g_flog, errno, "%s: client_t - calloc", __func__ );
        return ERR_INTERNAL;
    }

    (void) memset( ctx->cl, 0, max * sizeof(struct client_ctx) );
    ctx->clfree = ctx->clmax = max;

    (void) memset( &ctx->rq, 0, sizeof(ctx->rq) );

    if( 0 != pipe(ctx->cpipe) ) {
        mperror( g_flog, errno, "%s: pipe", __func__ );
        return ERR_INTERNAL;
    }

    /* make reading end of pipe non-blocking (we don't want to
     * block on pipe read on the server side)
     */
    if( -1 == (flags = fcntl( ctx->cpipe[0], F_GETFL )) ||
        -1 == fcntl( ctx->cpipe[0], F_SETFL, flags | O_NONBLOCK ) ) {
        mperror( g_flog, errno, "%s: fcntl", __func__ );
        return ERR_INTERNAL;
    }

    return 0;
}


void
free_server_ctx( struct server_ctx* ctx )
{
    int i = -1;

    assert(ctx);
    free( ctx->cl );

    for( i = 0; i < 2; ++i ) {
        if( ctx->cpipe[i] < 0 ) continue;

        if( 0 != close( ctx->cpipe[i] ) ) {
            mperror( g_flog, errno,
                    "%s: close(pipe)", __func__ );
        }
    }

    (void) memset( ctx, 0, sizeof(struct server_ctx) );
    return;
}


/* find index of the first client with the given pid
 */
int
find_client( const struct server_ctx* ctx, pid_t pid )
{
    int i = -1;

    assert( ctx && (pid >= 0) );

    for( i = 0; (size_t)i < ctx->clmax; ++i ) {
        if( ctx->cl[ i ].pid == pid )
            return i;
    }

    return -1;
}

/* populate connection's source info in client context
 */
static int
get_src_info( struct client_ctx* cl, int sockfd )
{
    struct stat st;
    struct sockaddr_in addr;
    a_socklen_t len = 0;
    int rc = 0;

    assert( cl );
    if( -1 == (rc = fstat( sockfd, &st )) ) {
        mperror( g_flog, errno, "%s: fstat", __func__ );
        return ERR_INTERNAL;
    }

    if( S_ISREG( st.st_mode ) ) {
        (void) strncpy( cl->src_addr, "File", sizeof(cl->src_addr) );
        cl->src_addr[ sizeof(cl->src_addr) - 1 ] = '\0';
        cl->src_port = 0;
    }
    else if( S_ISSOCK( st.st_mode ) ) {
        len = sizeof(addr);
        rc = getpeername( sockfd, (struct sockaddr*)&addr, &len );
        if( 0 == rc ) {
            (void)strncpy( cl->src_addr, inet_ntoa(addr.sin_addr),
                    sizeof(cl->src_addr) - 1 );
            cl->src_addr[ sizeof(cl->src_addr) - 1 ] = '\0';

            cl->src_port = ntohs(addr.sin_port);
        }
        else {
            mperror( g_flog, errno, "%s: getpeername", __func__ );
            return ERR_INTERNAL;
        }
    } /* S_ISSOCK */

    return rc;
}


/* add client to server context
 */
int
add_client( struct server_ctx* ctx,
            pid_t cpid, const char* maddr, uint16_t mport,
            int sockfd )
{
    struct client_ctx* client = NULL;
    int index = -1;
    int rc = 0;

    assert( ctx && maddr && mport );

    index = find_client( ctx, 0 );
    if( -1 == index )
        return ERR_INTERNAL;

    client = &(ctx->cl[ index ]);
    client->pid = cpid;

    (void) strncpy( client->mcast_addr, maddr, IPADDR_STR_SIZE );
    client->mcast_addr[ IPADDR_STR_SIZE - 1 ] = '\0';

    client->mcast_port = mport;

    if (ctx->rq.tail[0])
        (void) strcpy( client->tail, ctx->rq.tail );

    rc = get_src_info( client, sockfd );
    if( 0 != rc ) {
        client->pid = 0;
        return ERR_INTERNAL;
    }

    /* init sender id: 0 indicates no stats */
    client->tstat.sender_id = 0;

    ctx->clfree--;

    if( g_flog ) {
        (void)tmfprintf( g_flog, "Added client: pid=[%d], maddr=[%s], mport=[%d], "
                "saddr=[%s], sport=[%d]\n",
                (int)cpid, maddr, (int)mport, client->src_addr, client->src_port );
    }
    return 0;
}


/* delete client from server context
 */
int
delete_client( struct server_ctx* ctx, pid_t cpid )
{
    struct client_ctx* client = NULL;
    int index = -1;

    assert( ctx && (cpid > 0) );

    index = find_client( ctx, cpid );
    if( -1 == index ) {
        return ERR_INTERNAL;
    }

    client = &(ctx->cl[ index ]);
    client->pid = 0;
    client->tail[0] = '\0';

    ctx->clfree++;

    if( g_flog ) {
        TRACE( (void)tmfprintf( g_flog, "Deleted client: pid=[%d]\n", cpid) );
    }

    return 0;
}


/* init traffic relay statistics
 */
void
tpstat_init( struct tps_data* d, int setpid )
{
    assert( d );

    if( setpid )
        d->pid = getpid();

    d->tm_from = time(NULL);
    d->niter = 0;
    d->nbytes = 0;

    return;
}


/* send statistics update to server (if it's time)
 */
void
tpstat_update( struct server_ctx* ctx,
               struct tps_data* d, ssize_t nbytes )
{
    static const double MAX_NOUPDATE_ITER = 1000.0;
    static const double MAX_SEC = 10.0;

    struct tput_stat ts;
    int writefd = -1, rc = 0;
    const int DO_NOT_SET_PID = 0;
    ssize_t nwr = -1;
    double nsec;

    assert( ctx && d );

    d->nbytes += nbytes;

    nsec = difftime( time(NULL), d->tm_from );
    d->niter++;
    if( !((d->niter >= MAX_NOUPDATE_ITER) || (nsec >= MAX_SEC)) )
        return;

    /* attempt to send update to server */
    d->niter = 0;

    ts.sender_id = d->pid;
    ts.nbytes = d->nbytes;
    ts.nsec = nsec;

    writefd = ctx->cpipe[1];
    do {
        nwr = write( writefd, &ts, sizeof(ts) );
        if( nwr <= 0 ) {
            if( (EINTR != errno) && (EAGAIN != errno) ) {
                mperror( g_flog, errno, "%s: write", __func__ );
                rc = -1;
                break;
            }
            /* if it's an interrupt or pipe full - ignore */
            TRACE( (void)tmfprintf( g_flog, "%s - write error [%s] - ignored\n",
                            __func__, strerror(errno)) );
            break;
        }
        if( sizeof(ts) != (size_t)nwr ) {
            (void)tmfprintf( g_flog, "%s - wrote [%d] bytes to pipe, "
                    "expected [%u]\n",
                    __func__, nwr, (int)sizeof(ts) );
            rc = -1;
            break;
        }

        /*
        TRACE( (void)tmfprintf( g_flog,
                "Sent TSTAT={ sender=[%ld], bytes=[%f], seconds=[%f] }\n",
                (long)ts.sender_id, ts.nbytes, ts.nsec) );
        */
    } while(0);


    if( 0 == rc ) {
        tpstat_init( d, DO_NOT_SET_PID );
    }

    return;
}


/* read client statistics data and update the context
 */
int
tpstat_read( struct server_ctx* ctx )
{
    int cindex = -1;
    int readfd = -1;
    ssize_t nread = 0;
    struct tput_stat ts;
    struct client_ctx* client = NULL;

    assert( ctx );

    readfd = ctx->cpipe[0];
    assert( readfd > 0 );

    (void)memset( &ts, 0, sizeof(ts) );

    nread = read( readfd, &ts, sizeof(ts) );
    if( nread <= 0 ) {
        if( (EINTR != errno) && (EAGAIN != errno) ) {
            mperror( g_flog, errno, "%s: read", __func__ );
            return ERR_INTERNAL;
        }
        /* if it's an interrupt or no data available - ignore */
        TRACE( (void)tmfprintf( g_flog, "%s - read error [%s] - ingored\n",
                        __func__, strerror(errno)) );
        return 0;
    }
    if( sizeof(ts) != (size_t)nread ) {
        (void)tmfprintf( g_flog, "%s - read [%d] bytes from pipe, expected [%u]\n",
                __func__, nread, (int)sizeof(ts) );
        return ERR_INTERNAL;
    }

    TRACE( (void)tmfprintf( g_flog,
            "Received TSTAT={ sender=[%ld], bytes=[%f], seconds=[%f] }\n",
            (long)ts.sender_id, ts.nbytes, ts.nsec) );

    cindex = find_client( ctx, (pid_t)ts.sender_id );
    if( -1 == cindex ) {
        (void)tmfprintf( g_flog, "%s - cannot find client [%ld]\n",
                __func__, (long)ts.sender_id );
        /* ignore invalid client id's */
        return 0;
    }

    client = &(ctx->cl[ cindex ]);
    client->tstat = ts;

    TRACE( (void)tmfprintf( g_flog, "Updated context for pid=[%d]; "
                "[%.1f] Kb/sec\n", client->pid, (ts.nbytes / 1024) / ts.nsec  ) );
    return 0;
}



/* __EOF__ */

