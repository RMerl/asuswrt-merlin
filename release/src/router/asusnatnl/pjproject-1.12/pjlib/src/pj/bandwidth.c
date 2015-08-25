/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2(b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: bandwidth.c 13361 2012-07-01 02:17:35Z jordan $
 * 
 * modifed by Cheni 2012-11-06 : to adapt to libre 
 * modifed by Andrew 2013-03-18 : adapt to pjsip
 */

#include <assert.h>
#include <limits.h>
#include <string.h> /* memset() */
#include <stdio.h>

#include <pj/timer.h>
#include <pj/bandwidth.h>
#include <pj/os.h>
pj_uint64_t time_msec( void )
{
    //struct timeval tv;
    //gettimeofday( &tv, NULL );
    //return (pj_uint64_t) tv.tv_sec * 1000 + ( tv.tv_usec / 1000 );

    pj_time_val tv;
    pj_gettimeofday(&tv);

    return(pj_uint64_t) tv.sec * 1000 + tv.msec;
}

static pj_uint32_t
getSpeed_Bps( const struct bratecontrol * r, pj_uint32_t interval_msec, pj_uint64_t now )
{
    if ( !now )
        now = time_msec();

    if ( now != r->cache_time ) {
        int i = r->newest;
        pj_uint64_t bytes = 0;
        const pj_uint64_t cutoff = now - interval_msec;
        struct bratecontrol * rvolatile = (struct bratecontrol*) r;

        for ( ;; ) {
            if ( r->transfers[i].date <= cutoff )
                break;

            bytes += r->transfers[i].size;

            if ( --i == -1 ) i = HISTORY_SIZE - 1; /* circular history */
            if ( i == r->newest ) break; /* we've come all the way around */
        }

        rvolatile->cache_val = (pj_uint32_t)(( bytes * 1000u ) / interval_msec);
        rvolatile->cache_time = now;
    }

    return r->cache_val;
}

static void
bytesUsed( const pj_uint64_t now, struct bratecontrol * r, size_t size )
{
    if ( r->transfers[r->newest].date + GRANULARITY_MSEC >= now )
        r->transfers[r->newest].size += size;
    else {
        if ( ++r->newest == HISTORY_SIZE ) r->newest = 0;
        r->transfers[r->newest].date = now;
        r->transfers[r->newest].size = size;
    }

    /* invalidate cache_val*/
    r->cache_time = 0;
}

static void
allocateBandwidth( pj_band_t  * b,
                   pj_uint32_t    period_msec)
{
    assert( pj_isBandwidth( b ) );

    /* set the available bandwidth */
    if ( b->isLimited ) {
        const pj_uint64_t nextPulseSpeed = b->desiredSpeed_Bps;
        b->bytesLeft = (pj_uint32_t)( nextPulseSpeed * period_msec ) / 1000u;
    }

}

void
pj_bandwidthAllocate( pj_band_t  * b,
                      pj_uint32_t    period_msec )
{
    /* allocateBandwidth() is a helper function with two purposes:
     * 1. allocate bandwidth to b and its subtree
     * 2. accumulate an array of all the peerIos from b and its subtree. */
    allocateBandwidth( b, period_msec);
}

static pj_uint32_t
bandwidthClamp( pj_band_t  * b,
                pj_uint64_t              now,
                pj_uint32_t          byteCount )
{
    assert( pj_isBandwidth( b ) );

    if ( b ) {
        if ( b->isLimited ) {
            if ( now == 0 )
                now = time_msec();

            // not put in a timer but in regular polling process 
            if ( now - b->pulse > BANDWIDTH_PERIOD_MSEC) {
                b->pulse = now;
                allocateBandwidth( b, BANDWIDTH_PERIOD_MSEC);
            }

            byteCount = byteCount==0 ? b->bytesLeft : MIN( byteCount, b->bytesLeft );

            /* if we're getting close to exceeding the speed limit,
             * clamp down harder on the bytes available */
            if ( byteCount > 0 ) {
                double current;
                double desired;
                double r;

                current = pj_bandwidthGetRawSpeed_Bps( b, now);
                desired = pj_bandwidthGetDesiredSpeed_Bps( b);
                r = desired >= 1 ? current / desired : 0;

                if ( r > 1.0 ) byteCount = 0;
                else if ( r > 0.9 ) byteCount *= 0.8;
                else if ( r > 0.8 ) byteCount *= 0.9;
            }
        }
    }

    return byteCount;
}

pj_uint32_t
pj_bandwidthClamp( pj_band_t  * b,
                   pj_uint32_t          byteCount )
{
    return bandwidthClamp( b, 0, byteCount );
}


pj_uint32_t
pj_bandwidthGetRawSpeed_Bps( const pj_band_t * b, const pj_uint64_t now )
{
    assert( pj_isBandwidth( b ) );

    return getSpeed_Bps( &b->raw, HISTORY_MSEC, now );
}

void
pj_bandwidthUsed( pj_band_t  * b,
                  size_t          byteCount)
{
    pj_band_t * band;
    pj_uint64_t now = time_msec();

    assert( pj_isBandwidth( b ) );

    band = b;

    if ( band->isLimited) {
        band->bytesLeft -= MIN( band->bytesLeft, byteCount );
    }
    bytesUsed( now, &band->raw, byteCount );
}
