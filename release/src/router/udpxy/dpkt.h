/* @(#) definition of packet-io functions for udpxy
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

#ifndef UDPXY_DPKTH_02182008
#define UDPXY_DPKTH_02182008

#include <stdio.h>
#include "udpxy.h"

typedef int upxfmt_t;


/*  data stream context
 */
struct dstream_ctx {
    upxfmt_t        stype;
    int32_t         flags;
    size_t          mtu;

    struct iovec*   pkt;
    int32_t         max_pkt,
                    pkt_count;
};

/* data-stream context flags
 */
static const int32_t F_DROP_PACKET = (1 << 1);
static const int32_t F_CHECK_FMT   = (1 << 2);
static const int32_t F_SCATTERED   = (1 << 3);
static const int32_t F_FILE_INPUT  = (1 << 4);


#ifdef __cplusplus
extern "C" {
#endif

/* return human-readable name of file/stream format
*/
const char* fmt2str( upxfmt_t fmt );


/* determine type of stream saved in file
 */
upxfmt_t
get_fstream_type( int fd, FILE* log );

/* determine type of stream in memory
 */
upxfmt_t
get_mstream_type( const char* data, size_t len, FILE* log );


/* read record of one of the supported types from file
 */
ssize_t
read_frecord( int fd, char* data, const size_t len,
             upxfmt_t* stream_type, FILE* log );


/* write record after converting it from source into destination
 * format
 */
ssize_t
write_frecord( int fd, const char* data, size_t len,
              upxfmt_t sfmt, upxfmt_t dfmt, FILE* log );


/* reset packet-buffer registry in stream context
 */
void
reset_pkt_registry( struct dstream_ctx* ds );


/* release resources allocated for stream context
 */
void
free_dstream_ctx( struct dstream_ctx* ds );


/* initialize incoming-stream context:
 *      set data type (if possible) and flags
 */
int
init_dstream_ctx( struct dstream_ctx* ds, const char* cmd, const char* fname,
                  ssize_t nmsgs );


/* read data from source of specified type (UDP socket or otherwise);
 * read as many fragments as specified (max_frgs) into the buffer
 */
struct rdata_opt {
    int     max_frgs;   /* max fragments to read in */
    time_t  buf_tmout;  /* max time (sec) to hold data in buffer */
};

ssize_t
read_data( struct dstream_ctx* spc, int fd, char* data,
           const ssize_t data_len, const struct rdata_opt* opt );


/* write data to destination(s)
 */
ssize_t
write_data( const struct dstream_ctx* spc,
            const char* data,
            const ssize_t len,
            int fd );

#ifdef __cplusplus
}
#endif

#endif /* UDPXY_DPKTH_02182008 */

/* __EOF__ */

