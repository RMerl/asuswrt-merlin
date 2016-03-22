/* @(#) HTML-page templates/generation for udpxy status page
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

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>

#include "ctx.h"
#include "prbuf.h"
#include "udpxy.h"
#include "util.h"
#include "mtrace.h"

#include "statpg.h"
#include "mkpg.h"

extern FILE* g_flog;


/* generate throughput statistic string
 */
static const char*
tpstat_str( const struct tput_stat* ts, char* str, size_t len )
{
    assert( ts && str && len );

    if( (0 != ts->sender_id) && (ts->nsec > 0.0) ) {
        (void) snprintf( str, len, "%.2f Kb/sec",
                (ts->nbytes / 1024.0) / ts->nsec  );
    }
    else {
        (void) snprintf( str, len, "N/A" );
    }

    return str;
}


/* generate individual clients' status records
 */
static int
mk_client_entries( const struct server_ctx* ctx,
                   char* buf, size_t* len )
{
    prbuf_t pb = NULL;
    int n = -1;
    size_t i = 0, total = 0;
    ssize_t max_cli_mem = 0, max_cli_tail = 0;
    char tpinfo[ 32 ];

    assert( ctx && buf && len );

    do {
        n = prbuf_open( &pb, buf, *len );
        if( -1 == n ) break;

        /* table header */
        n = prbuf_printf( pb, ACLIENT_TABLE[0] );
        if( -1 == n ) break;
        else
            total += n;

        for( i = 0, n = -1; n && (i < ctx->clmax); ++i ) {
            struct client_ctx* client = &(ctx->cl[i]);
            if( client->pid <= 0 ) continue;

            n = prbuf_printf( pb, ACLIENT_REC_FMT[ i % 2 ],
                    ctx->cl[i].pid,
                    client->src_addr, client->src_port,
                    client->mcast_addr, client->mcast_port,
                    client->tail ? client->tail : "",
                    tpstat_str( &(client->tstat), tpinfo,
                                sizeof(tpinfo) ) );
            if( n <= 0 ) break;

            if (n > max_cli_mem)
                max_cli_mem = n;
            if (client->tail && (ssize_t)strlen(client->tail) > max_cli_tail)
                max_cli_tail = strlen(client->tail);

            total += n;
        } /* loop */

        /* table footer */
        n = prbuf_printf( pb, ACLIENT_TABLE[1] );
        if( -1 == n ) break;
        else
            total += n;

    } while(0);

    if( n > 0 ) {
        *len = total;
    }
    else {
        (void)tmfprintf( g_flog,
                "Error preparing status (client) HTML: "
                "insufficient memory: %ld bytes allowed, "
                "%ld clients, max memory per client: %ld bytes, "
                "max client tail: %ld bytes\n",
                (long)*len, (long)ctx->clmax, (long)max_cli_mem,
                (long)max_cli_tail);
    }

    (void) prbuf_close( pb );
    return (n > 0 ? 0 : -1);
}



/* generate service's (HTML) status page
 */
int
mk_status_page( const struct server_ctx* ctx,
                char* buf, size_t* len,
                int options )
{
    int n = -1;
    prbuf_t pb = NULL;
    time_t tm_now = time(NULL);
    const char* str_now = Zasctime(localtime(&tm_now));

    size_t num_clients = 0, i = 0;
    char* client_text = NULL;
    const char* page_format = NULL;
    const unsigned delay_msec = 3000;

    size_t text_size = 0, HEADER_SIZE = 0;
    static size_t MIN_PER_CLI = 80 + (3 * (IPADDR_STR_SIZE + 5) );

    extern const char  UDPXY_COPYRIGHT_NOTICE[];
    extern const char  COMPILE_MODE[];
    extern const char  VERSION[];
    extern const int   BUILDNUM;

    assert( ctx && buf && len );

    do {
        n = prbuf_open( &pb, buf, *len );
        if( -1 == n ) break;

        if( options & MSO_HTTP_HEADER ) {
            n = prbuf_printf( pb, "%s", HTML_PAGE_HEADER );
            if( n <= 0 ) break;
        }

        if( options & MSO_RESTART ) {
            n = prbuf_printf( pb, REDIRECT_SCRIPT_FMT, delay_msec );
            if( n <= 0 ) break;
        }

        for( i = 0, n = -1; n && (i < LEN_STAT_PAGE_BASE); ++i ) {
            n = prbuf_printf( pb, "%s", STAT_PAGE_BASE[i] );
        }
        if( n <= 0 ) break;

        for(text_size = 0, num_clients = 0, i = 0; i < ctx->clmax; ++i ) {
            if( ctx->cl[i].pid > 0 ) {
                ++num_clients;
                text_size += MIN_PER_CLI + strlen(ctx->cl[i].tail);
            }
        }

        if( ! (options & MSO_SKIP_CLIENTS) ) {
            if( num_clients > (size_t)0 ) {
                HEADER_SIZE =  strlen(ACLIENT_TABLE[0]);
                HEADER_SIZE += strlen(ACLIENT_TABLE[1]);

                text_size += HEADER_SIZE;
                if (text_size > *len) {
                    (void) tmfprintf(g_flog, "Client portion of memory [%ld bytes] > "
                        "[%ld bytes] of the page buffer.\n",
                        (long)text_size, (long)*len);
                    n = -1; break;
                }

                client_text = malloc( text_size );
                if( NULL == client_text ) {
                    mperror( g_flog, errno, "%s: calloc", __func__ );
                    n = -1;
                    break;
                }

                n = mk_client_entries( ctx, client_text, &text_size );
                if( -1 == n ) break;
            }
        } /* MSO_SKIP_CLIENTS */

        page_format = ( options & MSO_RESTART ) ? RST_PAGE_FMT1 : STAT_PAGE_FMT1;
        n = prbuf_printf( pb, page_format,
            getpid(), ctx->listen_addr, ctx->listen_port,
            ctx->mcast_ifc_addr,
            num_clients,
            (client_text ? client_text : ""),
            REQUEST_GUIDE,
            VERSION, BUILDNUM, COMPILE_MODE, str_now,
            UDPXY_COPYRIGHT_NOTICE );
        if( -1 == n ) break;

    } while(0);

    if( -1 == n ) {
        (void)tmfprintf( g_flog,
                "Error preparing status HTML: "
                "insufficient memory size=[%lu]\n", (u_long)(*len));
    }
    else {
        *len = prbuf_len( pb );
    }

    free( client_text );
    (void) prbuf_close( pb );

    return (n > 0 ? 0 : -1);
}


/* __EOF__ */

