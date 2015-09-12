/*
 * Performance counters software interface.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmperf.h 241182 2011-02-17 21:50:03Z $
 */
/* essai */
#ifndef _BCMPERF_H_
#define _BCMPERF_H_
/* get cache hits and misses */
#if defined(BCMMIPS) && defined(BCMPERFSTATS)
#include <hndmips.h>
#define BCMPERF_ENABLE_INSTRCOUNT() hndmips_perf_instrcount_enable()
#define BCMPERF_ENABLE_ICACHE_MISS() hndmips_perf_icache_miss_enable()
#define BCMPERF_ENABLE_ICACHE_HIT() hndmips_perf_icache_hit_enable()
#define	BCMPERF_GETICACHE_MISS(x)	((x) = hndmips_perf_read_cache_miss())
#define	BCMPERF_GETICACHE_HIT(x)	((x) = hndmips_perf_read_cache_hit())
#define	BCMPERF_GETINSTRCOUNT(x)	((x) = hndmips_perf_read_instrcount())
#else
#define BCMPERF_ENABLE_INSTRCOUNT()
#define BCMPERF_ENABLE_ICACHE_MISS()
#define BCMPERF_ENABLE_ICACHE_HIT()
#define	BCMPERF_GETICACHE_MISS(x)	((x) = 0)
#define	BCMPERF_GETICACHE_HIT(x)	((x) = 0)
#define	BCMPERF_GETINSTRCOUNT(x)	((x) = 0)
#endif /* defined(mips) */
#endif /* _BCMPERF_H_ */
