/* @(#) interface to RTP-protocol parsing functions for udpxy
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

#ifndef RTPH_UDPXY_021308
#define RTPH_UDPXY_021308

#include <sys/types.h>

static const int    MPEG_TS_SIG = 0x47;
static const size_t RTP_MIN_SIZE = 4;
static const size_t RTP_HDR_SIZE = 12; /* RFC 3550 */
static const int    RTP_VER2 = 0x02;

/* offset to header extension and extension length,
    * as per RFC 3550 5.3.1 */
static const size_t XTLEN_OFFSET = 14;
static const size_t XTSIZE = 4;

/* minimum length to determine size of an extended RTP header
 */
#define RTP_XTHDRLEN (XTLEN_OFFSET + XTSIZE)

static const size_t CSRC_SIZE = 4;

/* MPEG payload-type constants - adopted from VLC 0.8.6 */
#define P_MPGA      0x0E     /* MPEG audio */
#define P_MPGV      0x20     /* MPEG video */
#define P_MPGTS     0x21     /* MPEG TS    */

#ifdef __cplusplus
extern "C" {
#endif


/* check if the buffer is an RTP packet
 *
 * @param buf       buffer to analyze
 * @param len       size of the buffer
 * @param is_rtp    variable to set to 1 if data is an RTP packet
 * @param log       log file
 *
 * @return 0 if there was no error, -1 otherwise
 */
int RTP_check( const char* buf, const size_t len, int* is_rtp, FILE* log );


/* process RTP package to retrieve the payload: set
 * pbuf to the start of the payload area; set len to
 * be equal payload's length
 *
 * @param pbuf      address of pointer to beginning of RTP packet
 * @param len       pointer to RTP packet's length
 * @param verify    verify that it is an RTP packet if != 0
 * @param log       log file
 *
 * @return 0 if there was no error, -1 otherwise;
 *         set pbuf to point to beginning of payload and len
 *         be payload size in bytes
 */
int RTP_process( void** pbuf, size_t* len, int verify, FILE* log );


/* verify if buffer contains an RTP packet, 0 otherwise
 *
 * @param buf       buffer to analyze
 * @param len       size of the buffer
 * @param log       error log
 *
 * @return 1 if buffer contains an RTP packet, 0 otherwise
 */
int RTP_verify( const char* buf, const size_t len, FILE* log );


/* calculate length of an RTP header
 *
 * @param buf       buffer to analyze
 * @param len       size of the buffer
 * @param hdrlen    pointer to header length variable
 * @param log       error log
 *
 * @return          0 if header length has been calculated,
 *                  ENOMEM if buffer is not big enough
 */
int RTP_hdrlen( const char* buf, const size_t len, size_t* hdrlen,
                    FILE* log );

#ifdef __cplusplus
}
#endif

#endif /* RTPH_UDPXY_021308 */


/* __EOF__ */

