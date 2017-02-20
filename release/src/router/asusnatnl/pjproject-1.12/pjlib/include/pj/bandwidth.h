/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2(b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: bandwidth.h 12509 2011-06-19 18:34:10Z jordan $
 */

#ifndef TR_BANDWIDTH_H
#define TR_BANDWIDTH_H

//#include <assert.h>
#include <pj/types.h>

PJ_BEGIN_DECL

/* Needed for MS compiler */
#if defined(_MSC_VER) && !defined(PJ_WIN32_UWP)
#  define inline _inline
#endif

/*
 * Misc macros
 */

/** Defines the NULL pointer */
#ifndef NULL
#define NULL ((void *)0)
#endif

/** Get number of elements in an array */
#undef ARRAY_SIZE
#define ARRAY_SIZE(a) ((sizeof(a))/(sizeof((a)[0])))

/** Align a value to the boundary of mask */
#define ALIGN_MASK(x, mask)    (((x)+(mask))&~(mask))


/** Get the minimal value */
#undef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))

/** Get the maximal value */
#undef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))

/** Get the minimal value */
#undef min
#define min(x,y) MIN(x, y)

/** Get the maximal value */
#undef max
#define max(x,y) MAX(x, y)

/**
 * @addtogroup networked_io Networked IO
 * @{
 */

/* these are PRIVATE IMPLEMENTATION details that should not be touched.
 * it's included in the header for inlining and composition. */
enum {
    HISTORY_MSEC = 2000u,
    INTERVAL_MSEC = HISTORY_MSEC,
    GRANULARITY_MSEC = 200,
    HISTORY_SIZE = ( INTERVAL_MSEC / GRANULARITY_MSEC ),
    BANDWIDTH_MAGIC_NUMBER = 43143,
    BANDWIDTH_PERIOD_MSEC = 500
};

/* these are PRIVATE IMPLEMENTATION details that should not be touched.
 * it's included in the header for inlining and composition. */
struct bratecontrol {
    int newest;
    struct {
        pj_uint64_t date, size;
    } transfers[HISTORY_SIZE];
    pj_uint64_t cache_time;
    pj_uint32_t cache_val;
};

/* these are PRIVATE IMPLEMENTATION details that should not be touched.
 * it's included in the header for inlining and composition. */
typedef struct pj_bandwidth {
    pj_uint32_t isLimited;
    pj_uint32_t bytesLeft;
    pj_uint32_t desiredSpeed_Bps;
    pj_uint64_t pulse;
    struct bratecontrol raw;
} pj_band_t;

/******
*******
******/

/** @brief test to see if the pointer refers to a live bandwidth object */
static inline pj_bool_t 
pj_isBandwidth(const pj_band_t *b)
{
    return( b!= NULL );
}

static inline pj_bool_t 
pj_bandwidthSetDesiredSpeed_Bps(pj_band_t *band,
                                                      pj_uint32_t desiredSpeed )
{
    pj_uint32_t * value = &band->desiredSpeed_Bps;
    const pj_bool_t didChange = desiredSpeed != *value;
    *value = desiredSpeed;
//fprintf(stderr, "desiredSpeed: %d\n", desiredSpeed);
    return didChange;
}

static inline double
pj_bandwidthGetDesiredSpeed_Bps(const pj_band_t  *band)
{
    return band->desiredSpeed_Bps;
}

static inline pj_bool_t 
pj_bandwidthSetLimited(pj_band_t  *band, pj_uint32_t isLimited)
{
    pj_uint32_t * value = &band->isLimited;
    const pj_bool_t didChange = isLimited != *value;
    *value = isLimited;

//fprintf(stderr, "isLimited %d\n", isLimited);
    return didChange;
}

static inline pj_uint32_t 
pj_bandwidthIsLimited( const pj_band_t  * band)
{
    return band->isLimited;
}

/**
 * @brief allocate the next period_msec's worth of bandwidth for the peer-ios to consume
 */
void pj_bandwidthAllocate(pj_band_t *band,
                          pj_uint32_t period_msec);

/**
 * @brief clamps byteCount down to a number that this bandwidth will allow to be consumed
 */
pj_uint32_t  pj_bandwidthClamp(pj_band_t *band,
                               pj_uint32_t byteCount);

/******
*******
******/

/** @brief Get the raw total of bytes read or sent by this bandwidth subtree. */
pj_uint32_t pj_bandwidthGetRawSpeed_Bps(const pj_band_t *band,
                                        const pj_uint64_t now);

/** @brief Get the number of piece data bytes read or sent by this bandwidth subtree. */
pj_uint32_t pj_bandwidthGetPieceSpeed_Bps(const pj_band_t *band,
                                          const pj_uint64_t now);

/**
 * @brief Notify the bandwidth object that some of its allocated bandwidth has been consumed.
 * This is is usually invoked by the peer-io after a read or write.
 */
void pj_bandwidthUsed(pj_band_t *band,
                      pj_size_t byteCount);

PJ_END_DECL

#endif
