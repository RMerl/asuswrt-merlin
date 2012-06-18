/* @(#) implementation of RTP-protocol parsing functions for udpxy
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
#include <assert.h>
#include <sys/types.h>
#include <errno.h>

#include "rtp.h"
#include "mtrace.h"
#include "util.h"


/* check if the buffer is an RTP packet
 */
int
RTP_check( const char* buf, const size_t len, int* is_rtp, FILE* log )
{
    int rtp_version = 0;
    int rtp_payload_type = 0;

    assert( buf && len && is_rtp && log );

    if( len < RTP_MIN_SIZE ) {
        (void)tmfprintf( log, "RTP_check: buffer size [%lu] is "
                    "less than minimum [%lu]\n" , (u_long)len, (u_long)RTP_MIN_SIZE );
        return -1;
    }

    /* initial assumption: is NOT RTP */
    *is_rtp = 0;

    if( MPEG_TS_SIG == buf[0] ) {
        TRACE( (void)tmfputs( "MPEG-TS stream detected\n", log ) );
        return 0;
    }

    rtp_version = ( buf[0] & 0xC0 ) >> 6;
    if( RTP_VER2 != rtp_version ) {
        TRACE( (void)tmfprintf( log, "RTP_check: wrong RTP version [%d], "
                    "must be [%d]\n", (int)rtp_version, (int)RTP_VER2) );
        return 0;
    }

    if( len < RTP_HDR_SIZE ) {
        TRACE( (void)tmfprintf( log, "RTP_check: header size "
                    "is too small [%lu]\n", (u_long)len ) );
        return 0;
    }

    *is_rtp = 1;

    rtp_payload_type = buf[1] & 0x7F;
    switch( rtp_payload_type ) {
        case P_MPGA:
            TRACE( (void)tmfprintf( log, "RTP_check: [%d] MPEG audio stream\n",
                        (int)rtp_payload_type ) );
            break;
        case P_MPGV:
            TRACE( (void)tmfprintf( log, "RTP_check: [%d] MPEG video stream\n",
                        (int)rtp_payload_type ) );
            break;
        case P_MPGTS:
            TRACE( (void)tmfprintf( log, "RTP_check: [%d] MPEG TS stream\n",
                        (int)rtp_payload_type ) );
            break;

        default:
            (void) tmfprintf( log, "RTP_check: unsupported RTP "
                    "payload type [%d]\n", (int)rtp_payload_type );
            return -1;
    }

    return 0;
}


/* return 1 if buffer contains an RTP packet, 0 otherwise
 */
int
RTP_verify( const char* buf, const size_t len, FILE* log )
{
    int rtp_version = 0;
    int rtp_payload_type = 0;

    if( (len < RTP_MIN_SIZE) || (len < RTP_HDR_SIZE) ) {
        (void)tmfprintf( log, "RTP_verify: inappropriate size=[%lu] "
                "of RTP packet\n", (u_long)len );
        return 0;
    }

    rtp_version = ( buf[0] & 0xC0 ) >> 6;
    rtp_payload_type = buf[1] & 0x7F;

    /*
    TRACE( (void)tmfprintf( log, "RTP version [%d] at [%p]\n", rtp_version,
                (const void*)buf ) );
    */

    if( RTP_VER2 != rtp_version ) {
        /*
        TRACE( (void)tmfprintf( log, "RTP_verify: wrong RTP version [%d], "
                    "must be [%d]\n", (int)rtp_version, (int)RTP_VER2) );
        */
        return 0;
    }

    switch( rtp_payload_type ) {
        case P_MPGA:
        case P_MPGV:
        case P_MPGTS:
            break;
        default:
            (void) tmfprintf( log, "RTP_verify: unsupported RTP "
                    "payload type [%d]\n", (int)rtp_payload_type );
            return 0;
    }

    return 1;
}


/* calculate length of an RTP header
 */
int
RTP_hdrlen( const char* buf, const size_t len, size_t* hdrlen, FILE* log )
{
    int rtp_CSRC = -1, rtp_ext = -1;
    int rtp_payload = -1;
    ssize_t front_skip = 0,
            ext_len = 0;

    assert( buf && (len >= RTP_HDR_SIZE) && hdrlen && log );

    rtp_payload = buf[1] & 0x7F;
    rtp_CSRC    = buf[0] & 0x0F;
    rtp_ext     = buf[0] & 0x10;

    /* profile-based skip: adopted from vlc 0.8.6 code */
    if( (P_MPGA == rtp_payload) || (P_MPGV == rtp_payload) )
        front_skip = 4;
    else if( P_MPGTS != rtp_payload ) {
        (void)tmfprintf( log, "RTP_process: "
                "Unsupported payload type [%d]\n", rtp_payload );
        return -1;
    }

    if( rtp_ext ) {
        /*
        TRACE( (void)tmfprintf( log, "%s: RTP x-header detected, CSRC=[%d]\n",
                    __func__, rtp_CSRC) );
        */

        if( len < RTP_XTHDRLEN ) {
            /*
            TRACE( (void)tmfprintf( log, "%s: RTP x-header requires "
                    "[%lu] bytes, only [%lu] provided\n", __func__,
                    (u_long)(XTLEN_OFFSET + 1), (u_long)len ) );
            */
            return ENOMEM;
        }
    }

    if( rtp_ext ) {
        ext_len = XTSIZE +
            sizeof(int32_t) * ((buf[ XTLEN_OFFSET ] << 8) + buf[ XTLEN_OFFSET + 1 ]);
    }

    front_skip += RTP_HDR_SIZE + (CSRC_SIZE * rtp_CSRC) + ext_len;

    *hdrlen = front_skip;
    return 0;
}


/* process RTP package to retrieve the payload
 */
int
RTP_process( void** pbuf, size_t* len, int verify, FILE* log )
{
    int rtp_padding = -1;
    size_t front_skip = 0, back_skip = 0, pad_len = 0;

    char* buf = NULL;
    size_t pkt_len = 0;

    assert( pbuf && len && log );
    buf = *pbuf;
    pkt_len = *len;

    if( verify && !RTP_verify( buf, pkt_len, log ) )
        return -1;

    if( 0 != RTP_hdrlen( buf, pkt_len, &front_skip, log ) )
        return -1;

    rtp_padding = buf[0] & 0x20;
    if( rtp_padding ) {
        pad_len = buf[ pkt_len - 1 ];
    }

    back_skip += pad_len;

    if( verify && (pkt_len < (front_skip + back_skip)) ) {
        (void) tmfprintf( log, "RTP_process: invalid header "
                "(skip [%lu] exceeds packet length [%lu])\n",
                (u_long)(front_skip + back_skip), (u_long)pkt_len );
        return -1;
    }

    /* adjust buffer/length to skip heading and padding */
    /*
    TRACE( (void)tmfprintf( log, "In: RTP buf=[%p] of [%lu] bytes, "
                "fskip=[%ld], bskip=[%lu]\n",
                (void*)buf, (u_long)pkt_len,
                (u_long)front_skip, (u_long)back_skip ) );
    */

    buf += front_skip;
    pkt_len -= (front_skip + back_skip);

    /*
    TRACE( (void)tmfprintf( log, "Out RTP buf=[%p] of [%lu] bytes\n",
                (void*)buf, (u_long)pkt_len ) );
    */
    *pbuf = buf;
    *len  = pkt_len;

    return 0;
}


/* __EOF__ */

