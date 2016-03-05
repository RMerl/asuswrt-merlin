/* @(#) option-associated functions for udpxy
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

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "udpxy.h"
#include "uopt.h"
#include "util.h"


static int
read_http_footer (char* buf, size_t len)
{
    const char* ev = getenv ("UDPXY_HTTP200_FTR_LN");
    ssize_t n = 0;

    assert (buf && len);

    /* if 1-line footer is specified */
    if (ev) {
        (void) strncpy (buf, ev, len-1);
        buf[len-1] = '\0';
        return 0;
    }

    if (NULL != (ev = getenv ("UDPXY_HTTP200_FTR_FILE"))) {
        n = txtf_read (ev, buf, len, stderr);
    }

    return (n<0) ? 1 : 0;
}


inline static void
get_content_type(char *buf, size_t buflen)
{
    static const char DEFAULT_CONTENT_TYPE[] = "Content-Type:application/octet-stream";
    const char* ev = getenv("UDPXY_CONTENT_TYPE");

    if (ev && *ev)
        (void) snprintf(buf, buflen-1, "Content-Type:%s", ev);
    else
        strncpy(buf, DEFAULT_CONTENT_TYPE, buflen);

    buf[buflen - 1] = '\0';
}


/* populate options with default/initial values
 */
int
init_uopt( struct udpxy_opt* uo )
{
    int rc = 0;
    assert( uo );

    uo->is_verbose      = uf_FALSE;
    uo->cl_tpstat       = uf_FALSE;
    uo->nice_incr       = 0;
    uo->rbuf_len        = DEFAULT_CACHE_LEN;
    uo->rbuf_msgs       = 1;
    uo->max_clients     = DEFAULT_CLIENT_COUNT;
    uo->rcv_tmout       = get_timeval( "UDPXY_RCV_TMOUT",
                                RLY_SOCK_TIMEOUT );
    uo->dhold_tmout     = get_timeval( "UDPXY_DHOLD_TMOUT",
                                DHOLD_TIMEOUT );
    uo->sr_tmout        = get_timeval ( "UDPXY_SREAD_TMOUT", SRVSOCK_TIMEOUT  );
    uo->sw_tmout        = get_timeval ( "UDPXY_SWRITE_TMOUT", SRVSOCK_TIMEOUT );
    uo->ssel_tmout      = get_timeval ( "UDPXY_SSEL_TMOUT", SSEL_TIMEOUT );
    uo->lq_backlog      = (int) get_sizeval ("UDPXY_LQ_BACKLOG", LQ_BACKLOG);
    uo->rcv_lwmark      = (int) get_sizeval ("UDPXY_RCV_LWMARK", RCV_LWMARK);

    uo->nosync_sbuf     = (u_short)get_flagval( "UDPXY_SSOCKBUF_NOSYNC", 0 );
    uo->nosync_dbuf     = (u_short)get_flagval( "UDPXY_DSOCKBUF_NOSYNC", 0 );

    uo->srcfile         = NULL;
    uo->dstfile         = NULL;
    uo->mcast_refresh   = DEFAULT_MCAST_REFRESH;

    if (-1 == read_http_footer (uo->h200_ftr, sizeof(uo->h200_ftr))) {
        rc = -1; /* modify rc only if there is an error */
    }

    get_content_type(uo->cnt_type, sizeof(uo->cnt_type));
    assert( uo->cnt_type[0] );

    uo->tcp_nodelay = (flag_t)get_flagval( "UDPXY_TCP_NODELAY", 1);
    return rc;
}


/* release resources allocated for udpxy options
 */
void
free_uopt( struct udpxy_opt* uo )
{
    assert( uo );

    if( uo->srcfile ) {
        free( uo->srcfile ); uo->srcfile = NULL;
    }

    if( uo->dstfile ) {
        free( uo->dstfile ); uo->dstfile = NULL;
    }
}


#ifdef UDPXREC_MOD

/* populate udpxrec options with default/initial values
 */
int
init_recopt( struct udpxrec_opt* ro )
{
    int rc = 0;

    assert( ro );

    ro->is_verbose      = uf_FALSE;
    ro->nice_incr       = 0;

    ro->bg_time = ro->end_time = 0;
    ro->max_fsize = 0;
    ro->bufsize = DEFAULT_CACHE_LEN;
    ro->rbuf_msgs = 1;

    ro->mcast_addr[0]   = '\0';

    ro->dstfile         = NULL;
    ro->pidfile         = NULL;
    ro->rec_channel[0]  = '\0';
    ro->rec_port = 0;
    ro->waitupd_sec = -1;

    ro->nosync_sbuf  =
        (flag_t)get_flagval( "UDPXY_SSOCKBUF_NOSYNC", 0 );
    ro->nosync_dbuf =
        (flag_t)get_flagval( "UDPXY_DSOCKBUF_NOSYNC", 0 );

    ro->rcv_tmout       = 0;

    return rc;
}


/* release resources allocated for udpxy options
 */
void
free_recopt( struct udpxrec_opt* ro )
{
    assert( ro );

    if( ro->dstfile ) {
        free( ro->dstfile ); ro->dstfile = NULL;
    }
    if( ro->pidfile ) {
        free( ro->pidfile ); ro->pidfile = NULL;
    }

    ro->rec_channel[0] = '\0';
}


/* print udpxrec options to stream
 */
void
fprint_recopt( FILE* stream, struct udpxrec_opt* ro )
{
    assert( stream && ro );

    if( ro->is_verbose ) {
        (void)fprintf( stream, "verbose=[ON] " );
    }
    if( ro->nice_incr ) {
        (void)fprintf( stream, "nice_incr=[%d] ", ro->nice_incr );
    }
    if( ro->bg_time > 0 ) {
        (void)fprintf( stream, "begin_time=[%s] ",
                Zasctime( localtime(&(ro->bg_time) )) );
    }
    if( ro->end_time > 0 ) {
        (void)fprintf( stream, "end_time=[%s] ",
                Zasctime( localtime(&(ro->end_time) )) );
    }
    if( ro->max_fsize > 0 ) {
        (void)fprintf( stream, "Max filesize=[%.0f] bytes ",
                (double)ro->max_fsize );
    }

    (void)fprintf( stream, "Buffer size=[%ld] bytes ",
            (long)ro->bufsize );

    if( ro->rbuf_msgs > 0 ) {
        (void)fprintf( stream, "Max messages=[%ld] ",
                (long)ro->rbuf_msgs );
    }
    if( ro->mcast_addr[0] ) {
        (void)fprintf( stream, "Multicast interface=[%s] ", ro->mcast_addr );
    }
    if( ro->rec_channel[0] ) {
        (void)fprintf( stream, "Channel=[%s:%d] ", ro->rec_channel, ro->rec_port );
    }
    if( ro->pidfile ) {
        (void)fprintf( stream, "Pidfile=[%s] ", ro->pidfile );
    }
    if( ro->dstfile ) {
        (void)fprintf( stream, "Destination file=[%s] ", ro->dstfile );
    }
    if( ro->waitupd_sec > 0 ) {
        (void)fprintf( stream, "Update-wait=[%d] sec ", ro->waitupd_sec );
    }

    (void) fputs( "\n", stream );
    return;
}

#endif /* UDPXREC_MOD */

/* set verbose output on
 */
void
set_verbose( flag_t* verbose )
{
    assert( verbose );
    *verbose = uf_TRUE;
}



/* __EOF__ */

