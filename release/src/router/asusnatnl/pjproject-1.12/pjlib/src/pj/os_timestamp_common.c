/* $Id: os_timestamp_common.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/os.h>
#include <pj/compat/high_precision.h>

#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0

#define U32MAX  (0xFFFFFFFFUL)
#define NANOSEC (1000000000UL)
#define USEC    (1000000UL)
#define MSEC    (1000)

#define u64tohighprec(u64)	((pj_highprec_t)((pj_int64_t)(u64)))

static pj_highprec_t get_elapsed( const pj_timestamp *start,
                                  const pj_timestamp *stop )
{
#if defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
    return u64tohighprec(stop->u64 - start->u64);
#else
    pj_highprec_t elapsed_hi, elapsed_lo;

    elapsed_hi = stop->u32.hi - start->u32.hi;
    elapsed_lo = stop->u32.lo - start->u32.lo;

    /* elapsed_hi = elapsed_hi * U32MAX */
    pj_highprec_mul(elapsed_hi, U32MAX);

    return elapsed_hi + elapsed_lo;
#endif
}

static pj_highprec_t elapsed_msec( const pj_timestamp *start,
                                   const pj_timestamp *stop )
{
    pj_timestamp ts_freq;
    pj_highprec_t freq, elapsed;

    if (pj_get_timestamp_freq(&ts_freq) != PJ_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    pj_highprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * MSEC / freq */
    pj_highprec_mul(elapsed, MSEC);
    pj_highprec_div(elapsed, freq);

    return elapsed;
}

static pj_highprec_t elapsed_usec( const pj_timestamp *start,
                                   const pj_timestamp *stop )
{
    pj_timestamp ts_freq;
    pj_highprec_t freq, elapsed;

    if (pj_get_timestamp_freq(&ts_freq) != PJ_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    pj_highprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    pj_highprec_mul(elapsed, USEC);
    pj_highprec_div(elapsed, freq);

    return elapsed;
}

PJ_DEF(pj_uint32_t) pj_elapsed_nanosec( const pj_timestamp *start,
                                        const pj_timestamp *stop )
{
    pj_timestamp ts_freq;
    pj_highprec_t freq, elapsed;

    if (pj_get_timestamp_freq(&ts_freq) != PJ_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    pj_highprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    pj_highprec_mul(elapsed, NANOSEC);
    pj_highprec_div(elapsed, freq);

    return (pj_uint32_t)elapsed;
}

PJ_DEF(pj_uint32_t) pj_elapsed_usec( const pj_timestamp *start,
                                     const pj_timestamp *stop )
{
    return (pj_uint32_t)elapsed_usec(start, stop);
}

PJ_DEF(pj_uint32_t) pj_elapsed_msec( const pj_timestamp *start,
                                     const pj_timestamp *stop )
{
    return (pj_uint32_t)elapsed_msec(start, stop);
}

PJ_DEF(pj_uint64_t) pj_elapsed_msec64(const pj_timestamp *start,
                                      const pj_timestamp *stop )
{
    return (pj_uint64_t)elapsed_msec(start, stop);
}

PJ_DEF(pj_time_val) pj_elapsed_time( const pj_timestamp *start,
                                     const pj_timestamp *stop )
{
    pj_highprec_t elapsed = elapsed_msec(start, stop);
    pj_time_val tv_elapsed;

    if (PJ_HIGHPREC_VALUE_IS_ZERO(elapsed)) {
        tv_elapsed.sec = tv_elapsed.msec = 0;
        return tv_elapsed;
    } else {
        pj_highprec_t sec, msec;

        sec = elapsed;
        pj_highprec_div(sec, MSEC);
        tv_elapsed.sec = (long)sec;

        msec = elapsed;
        pj_highprec_mod(msec, MSEC);
        tv_elapsed.msec = (long)msec;

        return tv_elapsed;
    }
}

PJ_DEF(pj_uint32_t) pj_elapsed_cycle( const pj_timestamp *start,
                                      const pj_timestamp *stop )
{
    return stop->u32.lo - start->u32.lo;
}

PJ_DEF(pj_status_t) pj_gettickcount(pj_time_val *tv)
{
    pj_timestamp ts, start;
    pj_status_t status;

    if ((status = pj_get_timestamp(&ts)) != PJ_SUCCESS)
        return status;

    pj_set_timestamp32(&start, 0, 0);
    *tv = pj_elapsed_time(&start, &ts);

    return PJ_SUCCESS;
}

#endif  /* PJ_HAS_HIGH_RES_TIMER */

