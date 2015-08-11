/* $Id: pool_perf.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"

#if INCLUDE_POOL_PERF_TEST

#include <pjlib.h>
#include <pj/compat/malloc.h>

#if !PJ_HAS_HIGH_RES_TIMER
# error Need high resolution timer for this test.
#endif

#define THIS_FILE   "test"

#define LOOP	    10
#define COUNT	    1024
static unsigned	    sizes[COUNT];
static char	   *p[COUNT];
#define MIN_SIZE    4
#define MAX_SIZE    512
static unsigned total_size;


static int pool_test_pool()
{
    int i;
    pj_pool_t *pool = pj_pool_create(mem, NULL, total_size + 4*COUNT, 0, NULL);
    if (!pool)
	return -1;

    for (i=0; i<COUNT; ++i) {
	char *p;
	if ( (p=(char*)pj_pool_alloc(pool, sizes[i])) == NULL) {
	    PJ_LOG(3,(THIS_FILE,"   error: pool failed to allocate %d bytes",
		      sizes[i]));
	    pj_pool_release(pool);
	    return -1;
	}
	*p = '\0';
    }

    pj_pool_release(pool);
    return 0;
}

/* Symbian doesn't have malloc()/free(), so we use new/delete instead */
//#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
#if 0
static int pool_test_malloc_free()
{
    int i; /* must be signed */

    for (i=0; i<COUNT; ++i) {
		p[i] = new char[sizes[i]];
		if (!p[i]) {
			PJ_LOG(3,(THIS_FILE,"   error: malloc failed to allocate %d bytes",
					  sizes[i]));
			--i;
			while (i >= 0) {
				delete [] p[i];
				--i;
			}
			return -1;
		}
		*p[i] = '\0';
    }

    for (i=0; i<COUNT; ++i) {
    	delete [] p[i];
    }

    return 0;
}

#else	/* PJ_SYMBIAN */

static int pool_test_malloc_free()
{
    int i; /* must be signed */

    for (i=0; i<COUNT; ++i) {
	p[i] = (char*)malloc(sizes[i]);
	if (!p[i]) {
	    PJ_LOG(3,(THIS_FILE,"   error: malloc failed to allocate %d bytes",
		      sizes[i]));
	    --i;
	    while (i >= 0)
		free(p[i]), --i;
	    return -1;
	}
	*p[i] = '\0';
    }

    for (i=0; i<COUNT; ++i) {
	free(p[i]);
    }

    return 0;
}

#endif /* PJ_SYMBIAN */

int pool_perf_test()
{
    unsigned i;
    pj_uint32_t pool_time=0, malloc_time=0, pool_time2=0;
    pj_timestamp start, end;
    pj_uint32_t best, worst;

    /* Initialize size of chunks to allocate in for the test. */
    for (i=0; i<COUNT; ++i) {
	unsigned aligned_size;
	sizes[i] = MIN_SIZE + (pj_rand() % MAX_SIZE);
	aligned_size = sizes[i];
	if (aligned_size & (PJ_POOL_ALIGNMENT-1))
	    aligned_size = ((aligned_size + PJ_POOL_ALIGNMENT - 1)) & ~(PJ_POOL_ALIGNMENT - 1);
	total_size += aligned_size;
    }

    /* Add some more for pool admin area */
    total_size += 512;

    PJ_LOG(3, (THIS_FILE, "Benchmarking pool.."));

    /* Warmup */
    pool_test_pool();
    pool_test_malloc_free();

    for (i=0; i<LOOP; ++i) {
	pj_get_timestamp(&start);
	if (pool_test_pool()) {
	    return 1;
	}
	pj_get_timestamp(&end);
	pool_time += (end.u32.lo - start.u32.lo);

	pj_get_timestamp(&start);
	if (pool_test_malloc_free()) {
	    return 2;
	}
	pj_get_timestamp(&end);
	malloc_time += (end.u32.lo - start.u32.lo);

	pj_get_timestamp(&start);
	if (pool_test_pool()) {
	    return 4;
	}
	pj_get_timestamp(&end);
	pool_time2 += (end.u32.lo - start.u32.lo);
    }

    PJ_LOG(4,(THIS_FILE,"..LOOP count:                        %u",LOOP));
    PJ_LOG(4,(THIS_FILE,"..number of alloc/dealloc per loop:  %u",COUNT));
    PJ_LOG(4,(THIS_FILE,"..pool allocation/deallocation time: %u",pool_time));
    PJ_LOG(4,(THIS_FILE,"..malloc/free time:                  %u",malloc_time));
    PJ_LOG(4,(THIS_FILE,"..pool again, second invocation:     %u",pool_time2));

    if (pool_time2==0) pool_time2=1;
    if (pool_time < pool_time2)
	best = pool_time, worst = pool_time2;
    else
	best = pool_time2, worst = pool_time;
    
    /* avoid division by zero */
    if (best==0) best=1;
    if (worst==0) worst=1;

    PJ_LOG(3, (THIS_FILE, "..pool speedup over malloc best=%dx, worst=%dx", 
			  (int)(malloc_time/best),
			  (int)(malloc_time/worst)));
    return 0;
}


#endif	/* INCLUDE_POOL_PERF_TEST */

