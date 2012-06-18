/* @(#) header of data-stream cache for udpxy
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

#ifndef UDPXY_CACHEH_06122008
#define UDPXY_CACHEH_06122008

#include <sys/types.h>

#include "dpkt.h"

extern FILE* g_flog;

struct chunk {
    char*           data;
    size_t          length;
    size_t          used_length;

    struct dstream_ctx
                    spc;


    struct chunk*   next;
};

struct dscache;

#define ERR_CACHE_END       20      /* reached end of data */
#define ERR_CACHE_ALLOC     21      /* cannot allocate memory */
#define ERR_CACHE_BADDATA   22      /* data at a cache's head is invalid */
#define ERR_CACHE_INTRNL    99      /* internal error */

#ifdef __cplusplus
extern "C" {
#endif

/* initialize the cache
 */
struct dscache*
dscache_init( size_t maxlen, struct dstream_ctx* spc,
              size_t chunklen, unsigned nchunks );


/* get chunk at R-head, allocate new one if needed
 */
struct chunk*
dscache_rget( struct dscache* cache, int* error );


/* specify length of the used chunk's portion
 */
int
dscache_rset( struct dscache* cache, size_t usedlen );


/* specify length of the used chunk's portion
 * and advance R-head
 */
int
dscache_rnext( struct dscache* cache, size_t usedlen );


/* get a chunk at W-head
 */
struct chunk*
dscache_wget( struct dscache* cache, int* error );


/* advance (W-head) to the next chunk,
 * release unneeded resources
 */
int
dscache_wnext( struct dscache* cache, int* error );


/* release all resources from cache
 */
void
dscache_free( struct dscache* cache );


#ifdef __cplusplus
}
#endif


#endif  /* UDPXY_CACHEH_06122008 */

/* __EOF__*/


