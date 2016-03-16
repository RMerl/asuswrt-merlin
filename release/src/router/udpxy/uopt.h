/* @(#) option definitions and associated structures for udpxy
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

#ifndef UOPT_H_0215082300
#define UOPT_H_0215082300

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "udpxy.h"

static const int MIN_CLIENT_COUNT       = 1;
static const int MAX_CLIENT_COUNT       = 5000;
static const int DEFAULT_CLIENT_COUNT   = 3;

static const ssize_t MIN_MCACHE_LEN    = 4 * 1024;
static const ssize_t MAX_MCACHE_LEN    = 2048 * 1024;
static const ssize_t DEFAULT_CACHE_LEN = 2 * 1024;
static const u_short DEFAULT_MCAST_REFRESH = 0;

static const ssize_t MIN_SOCKBUF_LEN = (1024 * 64);


/* udpxy options
 */
struct udpxy_opt {
    flag_t  is_verbose;     /* verbose output on/off                */
    flag_t  cl_tpstat;      /* client reports throughput stats      */
    int     nice_incr;      /* value to increment nice by           */
    ssize_t rbuf_len;       /* size of read buffer                  */
    int     rbuf_msgs;      /* max msgs in read buffer (-1 = all)   */
    int     max_clients;    /* max clients to accept                */
    u_short mcast_refresh;  /* refresh rate (sec) for multicast
                               subscription */

    time_t  rcv_tmout;      /* receive (mcast) socket timeout           */
    time_t  dhold_tmout;    /* timeout to hold buffered data (milisec)  */

    time_t  sr_tmout,       /* server READ/RCV  timeout (sec)           */
            sw_tmout;       /* server WRITE/SND timeout (sec)           */
    long    ssel_tmout;     /* server select/poll timeout (sec)         */
    int     lq_backlog;     /* accept queue length for listening socket */
    int     rcv_lwmark;     /* receive low watermark on listening socket */

    flag_t  nosync_sbuf,    /* do not alter source-socket's buffer size */
            nosync_dbuf;    /* do not alter dest-socket's buffer size   */

    char*   srcfile;         /* file to read (video stream) from        */
    char*   dstfile;         /* file to save (video stream) to          */

    char    h200_ftr[2048];  /* text to add to HTTP 200 response        */
    flag_t  tcp_nodelay;     /* apply TCP_NODELAY option to
                                newly-accepted sockets                  */
    char    cnt_type[80];   /* custom HTTP 200 content type             */
};


#ifdef UDPXREC_MOD
/* udpxrec options
 */
struct udpxrec_opt {
    flag_t  is_verbose;     /* verbose output on/off                */
    int     nice_incr;      /* value to increment nice by           */

    time_t  bg_time;        /* time to start recording              */
    time_t  end_time;       /* time to end recording                */
    int64_t max_fsize;      /* max size of dest file (in bytes)     */
    ssize_t bufsize;        /* size of receiving socket buffer      */
    int     rbuf_msgs;      /* max number of messages to save
                               in buffer (-1 = as many as would fit)    */

                            /* address of the multicast interface       */
    char    mcast_addr[ IPADDR_STR_SIZE ];
    char    rec_channel[ IPADDR_STR_SIZE ];
    int     rec_port;
    int     waitupd_sec;    /* update every N seconds while waiting 
                               to start recording */

    time_t  rcv_tmout;      /* receive (mcast) socket timeout           */
    time_t  sr_tmout,       /* server READ/RCV  timeout (sec)           */
            sw_tmout;       /* server WRITE/SND timeout (sec)           */

    flag_t  nosync_sbuf,    /* do not alter source-socket's buffer size */
            nosync_dbuf;    /* do not alter dest-socket's buffer size   */


    char*   pidfile;        /* file to store app's PID                  */
    char*   dstfile;        /* file to save (video stream) to           */
};
#endif /* UDPXREC_MOD */

#ifdef __cplusplus
    extern "C" {
#endif

/* populate udpxy options with default/initial values
 */
int
init_uopt( struct udpxy_opt* uo );


/* release udpxy resources allocated for udpxy options
 */
void
free_uopt( struct udpxy_opt* uo );


#ifdef UDPXREC_MOD
/* populate udpxrec options with default/initial values
 */
int
init_recopt( struct udpxrec_opt* ro );


/* release resources allocated for udpxy options
 */
void
free_recopt( struct udpxrec_opt* ro );

/* print udpxrec options to stream
 */
void
fprint_recopt( FILE* stream, struct udpxrec_opt* ro );

#endif /* UDPXREC_MOD */

/* set verbose output on
 */
void
set_verbose( flag_t* verbose );


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UOPT_H_0215082300 */

/* __EOF__ */

