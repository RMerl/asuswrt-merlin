/* @(#) unit test of udpxy data-stream cache */

#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "cache.h"
#include "dpkt.h"
#include "udpxy.h"

extern FILE* g_flog;

extern const char CMD_UDP[];

#define BUFSZ       (64 * 1024)
#define NUM_MSGS    (BUFSZ / ETHERNET_MTU)
#define CACHE_MAXSZ (BUFSZ * 10)

static struct dscache* cache = NULL;
static struct dstream_ctx spc;


static int
test_init()
{
    int rc = 0;

    (void) fprintf( g_flog, "\n%s: TEST STARTED\n", __func__ );

    rc = init_dstream_ctx( &spc, CMD_UDP, NULL, NUM_MSGS );
    if( 0 != rc )
        return -1;

    cache = dscache_init( CACHE_MAXSZ, &spc, BUFSZ, 3 );
    if( NULL == cache )
        return -1;

    dscache_free( cache );
    free_dstream_ctx( &spc );

    (void) fprintf( g_flog, "\n%s: TEST FINISHED\n", __func__ );
    return 0;
}


static void
dump_chunk( FILE* out, const struct chunk* chnk,
            const char* tag )
{
    assert( out && chnk );

    (void) fprintf( out, "%s(%p)=[l:%ld,u:%ld,n:%p]\n",
            (tag ? tag : "chunk"), (const void*)chnk,
            (long)chnk->length,
            (long)chnk->used_length, (const void*)chnk->next );
}



static int
test_read_into()
{
    int rc = 0, numiter = 10;
    ssize_t i;
    struct chunk *rchnk = NULL, *wchnk = NULL;

    (void) fprintf( g_flog, "\n%s: TEST STARTED\n", __func__ );

    rc = init_dstream_ctx( &spc, CMD_UDP, NULL, NUM_MSGS );
    if( 0 != rc )
        return -1;

    cache = dscache_init( CACHE_MAXSZ, &spc, BUFSZ, 3 );
    if( NULL == cache )
        return -1;

    do {
        (void) fprintf( g_flog, "%d: -------- BEGIN\n",
                numiter );

        rchnk = dscache_rget( cache, &rc );
        if( !rchnk ) break;

        dump_chunk( g_flog, rchnk, "R" );

        for( i = 0; i < (rchnk->length - 64); ++i )
            rchnk->data[i] = 'U';

        (void) fprintf( g_flog, "%d: data updated [%ld]\n",
                numiter, (long)i );

        rc = dscache_rset( cache, i );
        if( 0 != rc ) break;

        wchnk = dscache_wget( cache, &rc );
        if( !wchnk ) break;

        (void) fprintf( g_flog, "%d: grabbed W-chunk(%p)\n",
                numiter, (const void*)wchnk );
        dump_chunk( g_flog, wchnk, "W" );

        for( i = 0; (i < wchnk->used_length) && !rc; ++i ) {
            if( 'U' != wchnk->data[i] )
                rc = 1;
        }

        (void) fprintf( g_flog, "%d: W-chunk(%p) read\n",
                numiter, (const void*)wchnk );

        (void) fprintf( g_flog, "%d: -------- END\n", numiter );
    } while( --numiter > 0 );

    if( rc ) {
        (void) fprintf( g_flog, "Error = [%d]\n", rc );
    }

    dscache_free( cache );
    free_dstream_ctx( &spc );

    (void) fprintf( g_flog, "\n%s: TEST FINISHED\n", __func__ );
    return rc;
}


int
main( int argc, char* const argv[] )
{
    int rc = 0, tlevel = 0;
    const char* trace_str = getenv( "UDPXY_TRACE_CACHE" );


    (void)(&argc && &argv);

    g_flog = stderr;
    if( trace_str ) {
        tlevel = atoi( trace_str );
        if( !tlevel && '0' != trace_str[0] ) {
            (void) fprintf( g_flog, "Error specifying trace level [%s]\n",
                    trace_str );
            return 1;
        }

        set_cache_trace( tlevel );
    }

    do {
        if( 0 != (rc = test_init()) )
            break;
        if( 0 != (rc = test_read_into()) )
            break;

    } while(0);

    return rc;
}


/* __EOF__ */

