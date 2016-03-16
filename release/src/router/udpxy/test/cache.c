/* @(#) implementation of data-stream cache for udpxy
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
#include <sys/types.h>

#include "mtrace.h"
#include "osdef.h"
#include "util.h"
#include "dpkt.h"
#include "cache.h"

static int ltrace_on = 1;

#define LTRACE( expr )      if( ltrace_on ) TRACE( (expr) )

struct dscache {
    struct chunk    *head, *tail;       /* cached data chunks           */
    struct chunk    *read_p, *write_p;  /* read/write-from chunk        */

    size_t          chunklen;           /* length of an individual chunk */
    size_t          max_totlen;         /* max combined size of chunks   */
    size_t          totlen;             /* combined size of chunks       */

    struct chunk    *free_head,
                    *free_tail;         /* free (released) chunks       */
    struct dstream_ctx
                    tpl_spc;            /* template of a stream context */
};


/* clone dstream context
 */
static int
clone_spc( const struct dstream_ctx* src, struct dstream_ctx* dst )
{
    assert( src && dst );

    dst->stype = src->stype;
    dst->flags = src->flags;
    dst->mtu   = src->mtu;


    dst->pkt = calloc( src->max_pkt, sizeof(dst->pkt[0]) );
    if( NULL == dst->pkt ) {
        (void) tmfprintf( g_flog, "%s: calloc(%ld, %ld)",
                (long)src->max_pkt, (long)sizeof(dst->pkt[0]) );
        return -1;
    }

    dst->max_pkt = src->max_pkt;
    dst->pkt_count = 0;

    return 0;
}


/* allocate from free-chunk list
 */
static struct chunk*
freelist_alloc( struct dscache* cache, size_t chunklen )
{
    struct chunk *chnk, *p;

    assert( cache && (chunklen > 0) );

    for( chnk = cache->free_head, p = NULL; chnk; ) {
        if( chnk->length >= chunklen ) {
            break;
        }
        else {
            p = chnk;
            chnk = chnk->next;
        }
    }

    if( !chnk ) return NULL;

    /* remove from the list */
    chnk->used_length = (size_t)0;

    if( cache->free_tail == chnk )
        cache->free_tail = p;

    if( cache->free_head == chnk )
        cache->free_head = chnk->next;

    if( p )
        p->next = chnk->next;

    /* reset the context */
    clone_spc( &cache->tpl_spc, &chnk->spc );

    chnk->next = NULL;

    LTRACE( (void)tmfprintf( g_flog,
                "Freelist-allocated chunk(%p) of len=[%ld]\n",
                (const void*)chnk, (long)chnk->length ) );
    return chnk;
}


/* allocate chunk on the heap
 */
static struct chunk*
heap_alloc( struct dscache* cache, size_t chunklen )
{
    struct chunk *chnk = NULL;
    int rc = 0;

    assert( cache && (chunklen > 0) );

    /* allocate on the heap */
    if( (cache->totlen + chunklen) > cache->max_totlen ) {
        (void) tmfprintf( g_flog, "%s: cannot add [%ld] bytes to cache, "
                    "current size=[%ld], max size=[%ld]\n",
                    __func__, (long)chunklen, (long)cache->totlen,
                    (long)cache->max_totlen );
        return NULL;
    }

    rc = -1;
    do {
        chnk = malloc( sizeof(struct chunk) );
        if( NULL == chnk ) {
            (void) tmfprintf( g_flog,
                    "%s: not enough memory for chunk structure size=[%ld]\n",
                    __func__, (long)sizeof(struct chunk) );
            break;
        }

        if( NULL == (chnk->data = malloc( chunklen )) ) {
            (void) tmfprintf( g_flog, "%s: not enough memory for chunk "
                    "data of size=[%ld]\n", __func__, chunklen );
            break;
        }

        chnk->length = chunklen;
        chnk->used_length = (size_t)0;

        if( 0 != clone_spc( &cache->tpl_spc, &chnk->spc ) )
            break;

        rc = 0;
    } while(0);

    /* error: cleanup */
    if( 0 != rc ) {
        if( !chnk ) return NULL;

        if( chnk->data ) free( chnk->data );
        free( chnk );

        return NULL;
    }

    cache->totlen += chnk->length;
    LTRACE( (void)tmfprintf( g_flog,
                "Heap-allocated chunk=[%p] of len=[%ld]\n",
                (const void*)chnk, (long)chnk->length ) );

    return chnk;
}


/* add buffer (chunk) of specific size to cache
 */
static struct chunk*
dscache_alloc( struct dscache* cache, size_t chunklen )
{
    struct chunk *chnk = freelist_alloc( cache, chunklen );
    if( !chnk )
        chnk = heap_alloc( cache, chunklen );

    if( !chnk ) {
        LTRACE( (void)tmfprintf( g_flog,
                    "%s: cannot allocate chunk", __func__ ) );
        return NULL;
    }

    chnk->next = NULL;

    if( NULL == cache->head )
        cache->head = chnk;

    if( NULL != cache->tail )
        cache->tail->next = chnk;

    cache->tail = chnk;

    return chnk;
}


/* initialize the cache
 */
struct dscache*
dscache_init( size_t maxlen, struct dstream_ctx* spc,
              size_t chunklen, unsigned nchunks )
{
    struct dscache* cache = NULL;
    unsigned i;

    assert( (maxlen > 0) && spc && chunklen );
    assert( spc->mtu && spc->max_pkt );

    cache = malloc( sizeof(struct dscache) );
    if( NULL == cache )
        return NULL;

    cache->head = cache->tail = NULL;
    cache->read_p = cache->write_p = NULL;
    cache->max_totlen = maxlen;
    cache->totlen = 0;
    cache->chunklen = chunklen;

    cache->free_head = cache->free_tail = NULL;
    cache->tpl_spc = *spc;

    for( i = 0; i < nchunks; ++i ) {
        if( NULL == dscache_alloc( cache, cache->chunklen ) ) {
            dscache_free( cache );
            return NULL;
        }
    }

    cache->read_p = cache->write_p = NULL;

    return cache;
}


/* get chunk at R-head, allocate new one if needed
 */
struct chunk*
dscache_rget( struct dscache* cache, int* error )
{
    struct chunk* chnk = NULL;

    assert( cache && error );

    if( NULL == cache->head ) {
        LTRACE( (void)tmfprintf( g_flog, "%s: cache is empty\n",
                __func__ ) );
        *error = ERR_CACHE_END;
        return NULL;
    }

    chnk = cache->read_p ? cache->read_p->next : cache->head;
    if( NULL == chnk ) {
        chnk = dscache_alloc( cache, cache->chunklen );
        if( !chnk ) {
            *error = ERR_CACHE_ALLOC;
            return NULL;
        }
    }

    /* check that next chunk is unused */
    assert( 0 == chnk->used_length );

    cache->read_p = chnk;
    LTRACE( (void)tmfprintf( g_flog, "Cache R-head moved to [%p]\n",
                (const void*)cache->read_p ) );

    return cache->read_p;
}


/* specify length of the used chunk's portion
 */
int
dscache_rset( struct dscache* cache, size_t usedlen )
{
    assert( cache );

    if( !cache->read_p ) {
        LTRACE( (void) tmfprintf( g_flog,
                    "%s: R-head is not positioned\n", __func__ ) );
        return ERR_CACHE_END;
    }

    cache->read_p->used_length = usedlen;

    return 0;
}



/* free up data chunk: move it from data- to free-chunk list */
static int
dscache_freechunk( struct dscache* cache, struct chunk* chnk )
{
    struct chunk *p = NULL;

    assert( chnk && cache->head );

    /* remove chunk from data-chunk list */
    if( cache->head == chnk ) {
        p = NULL;   /* no 'previous' */
        cache->head = chnk->next;
    }
    else {
        for( p = cache->head; p; ) {
            if( p->next == chnk )
                break;
        }
        if( !p )        /* not found */
            return -1;

        p->next = chnk->next;
    }

    /* adjust head positions */
    if( cache->read_p == chnk )
        cache->read_p = NULL;
    if( cache->write_p == chnk )
        cache->write_p = NULL;

    /* adjust the tail if we shortened it */
    if( cache->tail == chnk )
        cache->tail = p;

    chnk->next = NULL;
    free_dstream_ctx( &chnk->spc );

    /* add chunk to the free-chunk list */
    if( !cache->free_head )
        cache->free_head = chnk;

    if( cache->free_tail )
        cache->free_tail->next = chnk;

    cache->free_tail = chnk;

    LTRACE( (void)tmfprintf( g_flog, "Free-listed chunk(%p)\n",
                (const void*)chnk ) );
    return 0;
}


/* advance get a chunk at W-head
 */
struct chunk*
dscache_wget( struct dscache* cache, int* error )
{
    struct chunk *chnk = NULL, *old_chnk = NULL;

    assert( cache && error );

    chnk = cache->write_p;
    if( chnk ) {
        if( NULL == chnk->next ) {
            LTRACE( (void)tmfprintf( g_flog,
                    "%s: W-head cannot advance beyond end of cache\n",
                    __func__ ) );

            *error = ERR_CACHE_END;
            return NULL;
        }

        /* free up the chunk behind */
        old_chnk = chnk;
        chnk = chnk->next;

        if( 0 != dscache_freechunk( cache, old_chnk ) ) {
            LTRACE( (void)tmfprintf( g_flog, "%s: Cannot free chunk(%p)\n",
                        __func__, (const void*)old_chnk ) );
            *error = ERR_CACHE_INTRNL;
            return NULL;
        }
    }
    else {
        chnk = cache->head;
        LTRACE( (void)tmfprintf( g_flog, "%s: W-head moved to cache-head [%p]\n",
                    __func__, (const void*)chnk ) );
        if( NULL == chnk )
            *error = ERR_CACHE_END;
    }

    if( (size_t)0 == chnk->used_length ) {
        LTRACE( (void)tmfprintf( g_flog,
            "%s: W-head cannot move to unpopulated data\n", __func__ ) );
        *error = ERR_CACHE_BADDATA;
        return NULL;
    }

    cache->write_p = chnk;
    return chnk;
}


/* free all chunks in a list
 */
static void
free_chunklist( struct chunk* head )
{
    struct chunk *chnk, *p;

    for( chnk = head; chnk; ) {
        p = chnk->next;

        free( chnk->data );
        free_dstream_ctx( &chnk->spc );

        free( chnk );
        LTRACE( (void)tmfprintf( g_flog, "Heap-freed chunk(%p)\n",
                    (const void*)chnk ) );

        chnk = p;
    }

    return;
}


/* release all resources from cache
 */
void
dscache_free( struct dscache* cache )
{
    assert( cache );

    LTRACE( (void)tmfprintf( g_flog, "Purging chunk list\n" ) );
    free_chunklist( cache->head );

    LTRACE( (void)tmfprintf( g_flog, "Purging free list\n" ) );
    free_chunklist( cache->free_head );

    free( cache );
    LTRACE( (void)tmfprintf( g_flog, "Cache freed up\n" ) );


    return;
}

/* __EOF__ */

