/* @(#) client/server context data structures and interfaces
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


#ifndef UDPXY_CTX_H_0111081738
#define UDPXY_CTX_H_0111081738

#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#include "udpxy.h"
#include "dpkt.h"

#ifdef __cpluspplus
    extern "C" {
#endif


/* throughput statistics */
struct tput_stat {
    int32_t     sender_id;
    double      nbytes;     /* how many bytes transferred */
    double      nsec;       /* within how many seconds    */
};

/* context of a relay client */
struct client_ctx
{
    pid_t       pid;
    char        mcast_addr[ IPADDR_STR_SIZE ];
    uint16_t    mcast_port;
    char        src_addr[ IPADDR_STR_SIZE ];
    uint16_t    src_port;

    struct tput_stat
                tstat;

    char        tail[ MAX_TAIL_LEN + 1 ];
};


/* statistics on traffic relay & data gathering
 */
struct tps_data {
    pid_t  pid;         /* our PID - cached */
    time_t tm_from;     /* last time update sent (successfully) */
    double niter;       /* number of iterations since last try  */
    double nbytes;      /* bytes transferred since last update  */
};


/* server request components */
struct srv_request {
    char        cmd[ MAX_CMD_LEN + 1 ];
    char        param[ MAX_PARAM_LEN + 1 ];
    char        tail[ MAX_TAIL_LEN + 1 ];
};


/* context of the server */
struct server_ctx
{
    int         lsockfd;
    char        listen_addr[ IPADDR_STR_SIZE ];
    uint16_t    listen_port;
    char        mcast_ifc_addr[ IPADDR_STR_SIZE ];
    struct in_addr
                mcast_inaddr;

    struct srv_request rq;  /* (current) request to process */

    size_t      clfree,
                clmax;
    struct client_ctx*
                cl;
    u_short     rcv_tmout,  /* receive/send timeout */
                snd_tmout;

    int         cpipe[ 2 ]; /* client communications pipe */
};


/* initialize server context data
 */
int
init_server_ctx( struct server_ctx* ctx,
                 const size_t       max,
                 const char*        laddr,
                 uint16_t           lport,
                 const char*        mifc_addr );

/* release server context
 */
void
free_server_ctx( struct server_ctx* ctx );


/* find index of the first client with the given pid
 */
int
find_client( const struct server_ctx* ctx, pid_t pid );


/* add client to server context
 */
int
add_client( struct server_ctx* ctx,
            pid_t cpid, const char* maddr, uint16_t mport,
            int sockfd );


/* delete client from server context
 */
int
delete_client( struct server_ctx* ctx, pid_t cpid );


/* init traffic relay statistics
 */
void
tpstat_init( struct tps_data* d, int setpid );


/* send statistics update to server (if it's time)
 */
void
tpstat_update( struct server_ctx* ctx,
               struct tps_data* d, ssize_t nbytes );


/* read client statistics data and update the context
 */
int
tpstat_read( struct server_ctx* ctx );


#ifdef __cpluspplus
}
#endif

#endif /* UDPXY_CTX_H_0111081738 */

/* __EOF__ */

