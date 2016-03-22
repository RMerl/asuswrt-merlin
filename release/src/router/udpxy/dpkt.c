/* @(#) implementation of packet-io functions for udpxy
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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "udpxy.h"
#include "dpkt.h"
#include "rtp.h"
#include "util.h"
#include "mtrace.h"

extern FILE* g_flog;

static const size_t TS_SEG_LEN = 188;

/* data-stream format type */
enum {
    UPXDT_UNKNOWN = 0,     /* no assumptions */
    UPXDT_TS,          /* MPEG-TS */
    UPXDT_RTP_TS,      /* RTP over MPEG-TS */
    UPXDT_UDS,         /* UDS file format */
    UPXDT_RAW          /* read AS-IS */
};
static const char* upxfmt_NAME[] = {
    "UNKNOWN",
    "MPEG-TS",
    "RTP-TS",
    "UDPXY-UDS",
    "RAW"
};
static const int UPXDT_LEN = sizeof(upxfmt_NAME) / sizeof(upxfmt_NAME[0]);


const char*
fmt2str( upxfmt_t fmt )
{
    int ifmt = fmt;

    (void) UPXDT_LEN;
    assert( (ifmt >= 0 ) && (ifmt < UPXDT_LEN) );
    return upxfmt_NAME[ ifmt ];
}


/* check for MPEG TS signature, complain if not found
 */
static int
ts_sigcheck( const int c, off_t offset, ssize_t len,
             FILE* log, const char* func )

{
    assert( len );
    (void)(len); /* NOP to avoid warnings */

    if( c == MPEG_TS_SIG ) return 0;

    if( log && func ) {
        (void) offset; /* get rid of a warning if TRACE is disabled */
        TRACE( (void)tmfprintf( log, "%s: TS signature mismatch TS=[0x%02X], found=[0x%02X]; "
            "offset [0x%X(%u)] of packet len=[%lu]\n",
            func, MPEG_TS_SIG, c, offset, offset, (u_long)len ) );
    }

    return -1;
}


/* determine type of stream in memory
 */
upxfmt_t
get_mstream_type( const char* data, size_t len, FILE* log )
{
    int sig = 0;
    size_t hdrlen = 0;
    ssize_t n = -1;

    assert( data && len );

    if( len < (RTP_HDR_SIZE + 1) ) {
        (void) tmfprintf( log, "%s: read [%ld] bytes,"
                " not enough for RTP header\n", __func__,
                (long)n );
        return UPXDT_UNKNOWN;
    }

    /* if the 1st byte has MPEG-TS signature - skip further checks */
    sig = data[0] & 0xFF;
    if( 0 == ts_sigcheck( sig, 0, 1, NULL /*log*/, __func__ ) )
        return UPXDT_TS;

    /* if not RTP - quit */
    if( 0 == RTP_verify( data, RTP_HDR_SIZE, log ) )
        return UPXDT_UNKNOWN;

    /* check the first byte after RTP header - should be
     * TS signature to be RTP over TS */

    if( (0 != RTP_hdrlen( data, len, &hdrlen, log )) ||
        (len < hdrlen) ) {
        return UPXDT_UNKNOWN;
    }

    sig = data[ hdrlen ];
    if( 0 != ts_sigcheck( sig, 0, 1, log, __func__ ) )
        return UPXDT_UNKNOWN;

    return UPXDT_RTP_TS;
}



/* determine type of stream saved in file
 */
upxfmt_t
get_fstream_type( int fd, FILE* log )
{
    ssize_t n = 0;
    off_t offset = 0, where = 0;
    upxfmt_t dtype = UPXDT_UNKNOWN;
    char* data = NULL;

    /* read in enough data to contain extended header
     * and beginning of payload segment */
    size_t len = TS_SEG_LEN + RTP_XTHDRLEN;

    assert( (fd > 0) && log );

    if( NULL == (data = malloc( len )) ) {
        mperror( log, errno, "%s: malloc", __func__ );
        return UPXDT_UNKNOWN;
    }

    do {
        /* check if it is a MPEG TS stream
         */
        n = read( fd, data, len );
        if( 0 != sizecheck( "Not enough space for stream data",
                    len, n, log, __func__ ) ) break;
        offset += n;

        dtype = get_mstream_type( data, len, log );
        if( UPXDT_UNKNOWN == dtype ) {
            TRACE( (void)tmfprintf( log, "%s: file type is not recognized\n",
                        __func__ ) );
            dtype = UPXDT_UNKNOWN;
            break;
        }
    } while(0);

    if( NULL != data ) free( data );

    if( n <= 0 ) {
        mperror( log, errno, "%s", __func__ );
        return UPXDT_UNKNOWN;
    }

    where = lseek( fd, (-1) * offset, SEEK_CUR );
    if( -1 == where ) {
        mperror( log, errno, "%s: lseek", __func__ );
        return UPXDT_UNKNOWN;
    }

    /*
    TRACE( (void)tmfprintf( log, "%s: stream type = [%d]=[%s]\n",
                __func__, (int)dtype, fmt2str(dtype) ) );
    */

    return dtype;
}


/* read a sequence of MPEG TS packets (to fit into the given buffer)
 */
static ssize_t
read_ts_file( int fd, char* data, const size_t len, FILE* log )
{
    const size_t pkt_len = ((len - 1) / TS_SEG_LEN) * TS_SEG_LEN;
    off_t k = 0;
    u_int bad_frg = 0;
    ssize_t n = -1;

    assert( (fd > 0) && data && len && log);

    assert( !buf_overrun( data, len, 0, pkt_len, log ) );
    n = read_buf( fd, data, pkt_len, log );
    if( n <= 0 ) return n;

    if( 0 != sizecheck( "Bad TS packet stream",
                pkt_len, n, log, __func__ ) )
        return -1;

    /* make sure we've read TS records, not random data
     */
    for( k = 0; k < (off_t)pkt_len; k += TS_SEG_LEN ) {
        if( -1 == ts_sigcheck( data[k], k, (u_long)pkt_len,
                    log, __func__ ) ) {
            ++bad_frg;
        }
    }

    return (bad_frg ? -1 : n);
}


/* read an RTP packet
 */
static ssize_t
read_rtp_file( int fd, char* data, const size_t len, FILE* log )
{
    ssize_t nrd = -1, offset = 0;
    size_t hdrlen = 0, rdlen = 0;
    u_int frg = 0;
    int rtp_end = 0, rc = 0;
    off_t where = 0;

    assert( (fd > 0) && data && len && log);

    assert( !buf_overrun( data, len, 0, RTP_HDR_SIZE, log ) );
    nrd = read_buf( fd, data, RTP_HDR_SIZE, log );
    if( nrd <= 0 ) return nrd;
    offset += nrd;

    if( -1 == sizecheck( "Bad RTP header", RTP_HDR_SIZE, nrd,
                log, __func__ ) )
        return -1;

    if( 0 == RTP_verify( data, nrd, log ) )
        return -1;

    if( -1 == (rc = RTP_hdrlen( data, nrd, &hdrlen, log )) )
        return -1;

    /* if there is an extended header, read it in */
    if( ENOMEM == rc ) {
        assert( !buf_overrun( data, len, offset,
                    RTP_XTHDRLEN - RTP_HDR_SIZE, log ) );
        nrd = read_buf( fd, data + offset,
                RTP_XTHDRLEN - RTP_HDR_SIZE, log );
        if( (nrd <= 0) ||
            (-1 == sizecheck("Bad RTP x-header",
                             RTP_XTHDRLEN - RTP_HDR_SIZE, nrd,
                             log, __func__ )) ) {
            return -1;
        }
        if( 0 == nrd ) return nrd;

        offset += nrd;
        rc = RTP_hdrlen( data, offset, &hdrlen, log );
        if( 0 != rc ) {
            TRACE( (void)tmfprintf( log, "%s: bad RTP header - quitting\n",
                        __func__ ) );
            return -1;
        }

        /*
        TRACE( (void)tmfprintf( log, "%s: RTP x-header length=[%lu]\n",
                    __func__, (u_long)hdrlen ) );
        */

        if( (size_t)offset > hdrlen ) {
            /* read more than needed: step back */

            where = lseek( fd, (-1)*((size_t)offset - hdrlen), SEEK_CUR );
            if( -1 == where ) {
                mperror( log, errno, "%s: lseek", __func__ );
                return -1;
            }

            offset -= ((size_t)offset - hdrlen);
            assert( (size_t)offset == hdrlen );

            /*
            TRACE( (void)tmfprintf( log, "%s: back to fpos=[0x%X], "
                        "offset=[%ld]\n", __func__, (u_int)where, (long)offset ) );
            */
        }
        else if( hdrlen > (size_t)offset ) {
            /* read remainder of the header in */

            assert( !buf_overrun( data, len, offset,
                        (hdrlen - (size_t)offset), log ) );
            nrd = read_buf( fd, data + offset,
                    (hdrlen - (size_t)offset), log );
            if( nrd <= 0 ||
               (-1 == sizecheck("Bad RTP x-header tail",
                             (hdrlen - (size_t)offset), nrd,
                             log, __func__ )) ) {
                    return -1;
            }
            if( 0 == nrd ) return nrd;

            offset += nrd;
            assert( (size_t)offset == hdrlen );
        }
    } /* read extended header */


    /* read TS records until there is another RTP header or EOF */
    for( frg = 0; (ssize_t)len > offset; ++frg ) {

        rdlen = ( (len - offset) < TS_SEG_LEN
                  ? (len - offset)
                  : TS_SEG_LEN );

       /*
       TRACE( (void)tmfprintf( log, "%s: reading [%lu] more bytes\n",
               __func__, (u_long)rdlen ) );
       */

        assert( !buf_overrun( data, len, offset, rdlen, log ) );
        nrd = read_buf( fd, data + offset, rdlen, log );
        if( nrd <= 0 ) break;

        /* if it's an RTP header, roll back and return
         */
        rtp_end = RTP_verify( data + offset, nrd, log );
        if( 1 == rtp_end ) {
            if( -1 == lseek( fd, (-1) * nrd, SEEK_CUR ) ) {
                mperror( log, errno, "%s: lseek", __func__ );
                return -1;
            }

            break;
        }

        /* check if it is a TS packet and it's of the right size
         */
        if( (-1 == ts_sigcheck( data[offset], offset, (u_long)TS_SEG_LEN,
                    log, __func__ )) ||
            (-1 == sizecheck( "Bad TS segment size", TS_SEG_LEN, nrd,
                    log, __func__ )) ) {
            TRACE( hex_dump( "Data in question", data, offset + nrd, log ) );
            return -1;
        }

        offset += nrd;
    } /* for */


    /* If it is not EOF and no RTP header for the next message is found,
     * it is either our buffer is too small (to fit the whole message)
     * or the stream is invalid
     */
    if( !rtp_end && (0 != nrd) ) {
        (void)tmfprintf( log, "%s: no RTP end after reading [%ld] bytes\n",
                __func__, (long)offset );
        return -1;
    }

    return (nrd < 0) ? nrd : offset;
}


/* read record of one of the supported types from file
 */
ssize_t
read_frecord( int fd, char* data, const size_t len,
             upxfmt_t* stream_type, FILE* log )
{
    upxfmt_t stype;
    /* off_t where = -1, endmark = -1; */
    ssize_t nrd = -1;

    assert( fd > 0 );
    assert( data && len );
    assert( stream_type && log );

    stype = *stream_type;

    /*
    where = lseek( fd, 0, SEEK_CUR );
    TRACE( (void)tmfprintf( log, "%s: BEGIN reading at pos=[0x%X:%u]\n",
                __func__, (u_int)where, (u_int)where ) );
    */

    if( UPXDT_UNKNOWN == *stream_type ) {
        stype = get_fstream_type( fd, log );

        if( UPXDT_UNKNOWN == stype ) {
            (void)tmfprintf( log, "%s: Unsupported type\n", __func__ );
            return -1;
        }

        *stream_type = stype;
    } /* UPXDT_UNKNOWN */

    if( UPXDT_TS == stype ) {
        nrd = read_ts_file( fd, data, len, log );
    }
    else if( UPXDT_RTP_TS == stype ) {
        nrd = read_rtp_file( fd, data, len, log );
    }
    else {
        (void)tmfprintf( log, "%s: unknown stream type [%d]\n",
                __func__, stype );
        return -1;
    }

    /*
    if( nrd >= 0 ) {
        endmark = lseek( fd, 0, SEEK_CUR );

        TRACE( (void)tmfprintf( log, "%s: END reading [%ld] bytes at pos=[0x%X:%u]\n",
                __func__, (long)nrd, (u_int)endmark, (u_int)endmark ) );

        TRACE( sizecheck( "WARNING: Read file discrepancy",
                    where + nrd, endmark,
                    log, __func__ ) );
    }
    */

    return nrd;
}


/* write data as a UDS record
 */
static ssize_t
write_uds_record( int fd, const char* data, size_t len, FILE* log )
{
    assert( (fd > 0) && data && len );
    (void)(data && len && fd);
    (void)tmfprintf( log, "%s: UDS conversion not yet implemented\n",
            __func__ );
    return -1;
}


/* write RTP record into TS stream
 */
static ssize_t
write_rtp2ts( int fd, const char* data, size_t len, FILE* log )
{
    void* buf = (void*)data;
    size_t pldlen = len;
    const int NO_VERIFY = 0;
    int rc = 0;

    assert( (fd > 0) && data && len && log );

    rc = RTP_process( &buf, &pldlen, NO_VERIFY, log );
    if( -1 == rc ) return -1;

    assert( !buf_overrun( buf, len, 0, pldlen, log ) );
    return write_buf( fd, buf, pldlen, log );
}


/* write record after converting it from source into destination
 * format
 */
ssize_t
write_frecord( int fd, const char* data, size_t len,
              upxfmt_t sfmt, upxfmt_t dfmt, FILE* log )
{
    ssize_t nwr = -1;
    int fmt_ok = 0;
    const char *str_from = NULL, *str_to = NULL;

    if( UPXDT_UDS == dfmt ) {
        fmt_ok = 1;
        nwr = write_uds_record( fd, data, len, log );
    }
    else if( UPXDT_TS == dfmt ) {
        if( UPXDT_RTP_TS == sfmt ) {
            fmt_ok = 1;
            nwr = write_rtp2ts( fd, data, len, log );
        }
    }

    if( !fmt_ok ) {
        str_from = fmt2str(sfmt);
        str_to   = fmt2str(dfmt);
        (void)tmfprintf( log, "Conversion from [%s] into [%s] is not supported\n",
                str_from, str_to );
        return -1;
    }

    return nwr;
}


/* reset packet-buffer registry in stream spec
 */
void
reset_pkt_registry( struct dstream_ctx* ds )
{
    assert( ds );

    ds->flags &= ~F_DROP_PACKET;
    ds->pkt_count = 0;
}


/* release resources allocated for stream spec
 */
void
free_dstream_ctx( struct dstream_ctx* ds )
{
    assert( ds );

    if( NULL != ds->pkt ) {
        free( ds->pkt );
        ds->pkt = NULL;

        ds->pkt_count = ds->max_pkt = 0;
    }
}



/* register received packet into registry (for scattered output)
 */
static int
register_packet( struct dstream_ctx* spc, char* buf, size_t len )
{
    struct iovec* new_pkt = NULL;
    static const int DO_VERIFY = 1;

    void* new_buf = buf;
    size_t new_len = len;

    assert( spc->max_pkt > 0 );

    /* enlarge packet registry if needed */
    if( spc->pkt_count >= spc->max_pkt ) {
        spc->max_pkt <<= 1;
        spc->pkt = realloc( spc->pkt, spc->max_pkt * sizeof(spc->pkt[0]) );
        if( NULL == spc->pkt ) {
            mperror( g_flog, errno, "%s: realloc", __func__ );
            return -1;
        }

        TRACE( (void)tmfprintf( g_flog, "RTP packet registry "
                "expanded to [%lu] records\n", (u_long)spc->max_pkt ) );
    }

    /* put packet info into registry */

    new_pkt = &(spc->pkt[ spc->pkt_count ]);

    /*
    TRACE( (void)tmfprintf( stderr, "IN: packet [%lu]: buf=[%p], len=[%lu]\n",
                (u_long)spc->pkt_count, (void*)buf, (u_long)len ) );
    */

    if( 0 != RTP_process( &new_buf, &new_len, DO_VERIFY, g_flog ) ) {
        TRACE( (void)tmfputs("register packet: dropping\n", g_flog) );
        spc->flags |= F_DROP_PACKET;

        return 0;
    }

    new_pkt->iov_base = new_buf;
    new_pkt->iov_len = new_len;

    /*
    TRACE( (void)tmfprintf( stderr, "OUT: packet [%lu]: buf=[%p], len=[%lu]\n",
                (u_long)spc->pkt_count, new_pkt->iov_base,
                (u_long)new_pkt->iov_len ) );
    */

    spc->pkt_count++;
    return 0;
}


/* read data from source, determine underlying protocol
 * (if not already known); and process the packet
 * if needed (for RTP - register packet)
 *
 * return the number of octets read from the source
 * into the buffer
 */
static ssize_t
read_packet( struct dstream_ctx* spc, int fd, char* buf, size_t len )
{
    ssize_t n = -1;
    size_t chunk_len = len;

    assert( spc && buf && len );
    assert( fd > 0 );

    /* if *RAW* data specified - read AS IS
     * and exit */
    if( UPXDT_RAW == spc->stype ) {
        return read_buf( fd, buf, len, g_flog );
    }

    /* if it is (or *could* be) RTP, read only MTU bytes
     */
    if( (spc->stype == UPXDT_RTP_TS) || (spc->flags & F_CHECK_FMT) )
        chunk_len = (len > spc->mtu) ? spc->mtu : len;

    if( spc->flags & F_FILE_INPUT ) {
        assert( !buf_overrun( buf, len, 0, chunk_len, g_flog ) );
        n = read_frecord( fd, buf, chunk_len, &(spc->stype), g_flog );
        if( n <= 0 ) return n;
    }
    else {
        assert( !buf_overrun(buf, len, 0, chunk_len, g_flog) );
        n = read_buf( fd, buf, chunk_len, g_flog );
        if( n <= 0 ) return n;
    }

    if( spc->flags & F_CHECK_FMT ) {
        spc->stype = get_mstream_type( buf, n, g_flog );
        switch (spc->stype) {
            case UPXDT_RTP_TS:
                /* scattered: exclude RTP headers */
                spc->flags |= F_SCATTERED; break;
            case UPXDT_TS:
                spc->flags &= ~F_SCATTERED; break;
            default:
                spc->stype = UPXDT_RAW;
                TRACE( (void)tmfputs( "Unrecognized stream type\n", g_flog ) );
                break;
        } /* switch */

        TRACE( (void)tmfprintf( g_flog, "Established stream as [%s]\n",
               fmt2str( spc->stype ) ) );

        spc->flags &= ~F_CHECK_FMT;
    }

    if( spc->flags & F_SCATTERED )
        if( -1 == register_packet( spc, buf, n ) ) return -1;

    return n;
}


/* read data from source of specified type (UDP socket or otherwise);
 * read as many fragments as specified (max_frgs) into the buffer
 */
ssize_t
read_data( struct dstream_ctx* spc, int fd, char* data,
           const ssize_t data_len, const struct rdata_opt* opt )
{
    int m = 0;
    ssize_t n = 0, nrcv = -1;
    time_t start_tm = time(NULL), cur_tm = 0;
    time_t buftm_sec = 0;

    assert( spc && (data_len > 0) && opt );

    /* if max_frgs < 0, read as many packets as can fit in the buffer,
     * otherwise read no more than max_frgs packets
     */

    for( m = 0, n = 0; ((opt->max_frgs < 0) ? 1 : (m < opt->max_frgs)); ++m ) {
        nrcv = read_packet( spc, fd, data + n, data_len - n );
        if( nrcv <= 0 ) {
            if( EAGAIN == errno ) {
                (void)tmfprintf( g_flog,
                        "Receive on socket/file [%d] timed out\n",
                        fd);
            }

            if( 0 == nrcv ) (void)tmfprintf(g_flog, "%s - EOF\n",__func__);
            else {
                mperror(g_flog, errno, "%s: read/recv", __func__);
            }

            break;
        }

        if( spc->flags & F_DROP_PACKET ) {
            spc->flags &= ~F_DROP_PACKET;
            continue;
        }

        n += nrcv;
        if( n >= (data_len - nrcv) ) break;

        if( -1 != opt->buf_tmout ) {
            cur_tm = time(NULL);
            buftm_sec = cur_tm - start_tm;
            if( buftm_sec >= opt->buf_tmout ) {
                TRACE( (void)tmfprintf( g_flog, "%s: Buffer timed out "
                            "after [%ld] seconds\n", __func__,
                            (long)buftm_sec ) );
                break;
            }
            /*
            else {
                (void) tmfprintf( g_flog, "%s: Skip\n", __func__ );
            }
            */
        }
    } /* for */

    if( (nrcv > 0) && !n ) {
        TRACE( (void)tmfprintf( g_flog, "%s: no data to send "
                    "out of [%d] packets\n", __func__, m ) );
        return -1;
    }

    return (nrcv > 0) ? n : -1;
}


/* write data to destination(s)
 */
ssize_t
write_data( const struct dstream_ctx* spc,
            const char* data,
            const ssize_t len,
            int fd )
{
    ssize_t n = 0, error = IO_ERR;
    int32_t n_count = -1;

    assert( spc && data && len );
    if( fd <= 0 ) return 0;

    if( spc->flags & F_SCATTERED ) {
        n_count = spc->pkt_count;
        n = writev( fd, spc->pkt, n_count );
        if( n <= 0 ) {
            if( EAGAIN == errno ) {
                (void)tmfprintf( g_flog, "Write on fd=[%d] timed out\n", fd);
                error = IO_BLK;
            }
            mperror( g_flog, errno, "%s: writev", __func__ );
            return error;
        }
    }
    else {
        n = write_buf( fd, data, len, g_flog );
        if( n < 0 )
            error = n;
    }

    return (n > 0) ? n : error;
}


/* initialize incoming-stream context:
 *      set data type (if possible) and flags
 */
int
init_dstream_ctx( struct dstream_ctx* ds, const char* cmd, const char* fname,
                  ssize_t nmsgs )
{
    extern const char       CMD_UDP[];
    extern const size_t     CMD_UDP_LEN;
    extern const char       CMD_RTP[];
    extern const size_t     CMD_RTP_LEN;

    assert( ds && cmd && (nmsgs > 0) );

    ds->flags = 0;
    ds->pkt = NULL;
    ds->max_pkt = ds->pkt_count = 0;
    ds->mtu = ETHERNET_MTU;

    if( NULL != fname ) {
        ds->stype = UPXDT_UNKNOWN;
        ds->flags |= (F_CHECK_FMT | F_FILE_INPUT);
        TRACE( (void)tmfputs( "File stream, RTP check enabled\n", g_flog ) );
    }
    else if( 0 == strncmp( cmd, CMD_UDP, CMD_UDP_LEN ) ) {
        ds->stype = UPXDT_UNKNOWN;
        ds->flags |= F_CHECK_FMT;
        TRACE( (void)tmfputs( "UDP stream, RTP check enabled\n", g_flog ) );
    }
    else if( 0 == strncmp( cmd, CMD_RTP, CMD_RTP_LEN ) ) {
        ds->stype = UPXDT_RTP_TS;
        ds->flags |= F_SCATTERED;
        TRACE( (void)tmfputs( "RTP (over UDP) stream assumed,"
                    " no checks\n", g_flog ) );
    }
    else {
        TRACE( (void)tmfprintf( g_flog, "%s: "
                    "Irrelevant command [%s]\n", __func__, cmd) );
        return -1;
    }

    ds->pkt = calloc( nmsgs, sizeof(ds->pkt[0]) );
    if( NULL == ds->pkt ) {
        mperror( g_flog, errno, "%s: calloc", __func__ );
        return -1;
    }

    ds->max_pkt = nmsgs;
    return 0;
}



/* __EOF__ */
