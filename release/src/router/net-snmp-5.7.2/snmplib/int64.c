/**
 * @file int64.c
 *
 * @brief Functions for 64-bit integer computations.
 *
 * 21-jan-1998: David Perkins <dperkins@dsperkins.com>
 */

#include <net-snmp/net-snmp-config.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/int64.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/snmp_logging.h>

#include <net-snmp/net-snmp-features.h>

/**
 * Divide an unsigned 64-bit integer by 10.
 *
 * @param[in]  u64   Number to be divided.
 * @param[out] pu64Q Quotient.
 * @param[out] puR   Remainder.
 */
void
divBy10(U64 u64, U64 * pu64Q, unsigned int *puR)
{
    unsigned long   ulT;
    unsigned long   ulQ;
    unsigned long   ulR;

    /*
     * top 16 bits 
     */
    ulT = (u64.high >> 16) & 0x0ffff;
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->high = ulQ << 16;

    /*
     * next 16 
     */
    ulT = (u64.high & 0x0ffff);
    ulT += (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->high = pu64Q->high | ulQ;

    /*
     * next 16 
     */
    ulT = ((u64.low >> 16) & 0x0ffff) + (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->low = ulQ << 16;

    /*
     * final 16 
     */
    ulT = (u64.low & 0x0ffff);
    ulT += (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->low = pu64Q->low | ulQ;

    *puR = (unsigned int) (ulR);
}

/**
 * Multiply an unsigned 64-bit integer by 10.
 *
 * @param[in]  u64   Number to be multiplied.
 * @param[out] pu64P Product.
 */
void
multBy10(U64 u64, U64 * pu64P)
{
    unsigned long   ulT;
    unsigned long   ulP;
    unsigned long   ulK;

    /*
     * lower 16 bits 
     */
    ulT = u64.low & 0x0ffff;
    ulP = ulT * 10;
    ulK = ulP >> 16;
    pu64P->low = ulP & 0x0ffff;

    /*
     * next 16 
     */
    ulT = (u64.low >> 16) & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->low = (ulP & 0x0ffff) << 16 | pu64P->low;

    /*
     * next 16 bits 
     */
    ulT = u64.high & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->high = ulP & 0x0ffff;

    /*
     * final 16 
     */
    ulT = (u64.high >> 16) & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->high = (ulP & 0x0ffff) << 16 | pu64P->high;
}

/**
 * Add an unsigned 16-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] pu64 Number to be incremented.
 * @param[in]     u16  Amount to add.
 *
 */
void
incrByU16(U64 * pu64, unsigned int u16)
{
    incrByU32(pu64, u16);
}

/**
 * Add an unsigned 32-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] pu64 Number to be incremented.
 * @param[in]     u32  Amount to add.
 *
 */
void
incrByU32(U64 * pu64, unsigned int u32)
{
    uint32_t tmp;

    tmp = pu64->low;
    pu64->low = (uint32_t)(tmp + u32);
    if (pu64->low < tmp)
        pu64->high = (uint32_t)(pu64->high + 1);
}

/**
 * Subtract two 64-bit numbers.
 *
 * @param[in] pu64one Number to start from.
 * @param[in] pu64two Amount to subtract.
 * @param[out] pu64out pu64one - pu64two.
 */
void
u64Subtract(const U64 * pu64one, const U64 * pu64two, U64 * pu64out)
{
    int carry;

    carry = pu64one->low < pu64two->low;
    pu64out->low = (uint32_t)(pu64one->low - pu64two->low);
    pu64out->high = (uint32_t)(pu64one->high - pu64two->high - carry);
}

/**
 * Add two 64-bit numbers.
 *
 * @param[in] pu64one Amount to add.
 * @param[in,out] pu64out pu64out += pu64one.
 */
void
u64Incr(U64 * pu64out, const U64 * pu64one)
{
    pu64out->high = (uint32_t)(pu64out->high + pu64one->high);
    incrByU32(pu64out, pu64one->low);
}

/**
 * Add the difference of two 64-bit numbers to a 64-bit counter.
 *
 * @param[in] pu64one
 * @param[in] pu64two
 * @param[out] pu64out pu64out += (pu64one - pu64two)
 */
void
u64UpdateCounter(U64 * pu64out, const U64 * pu64one, const U64 * pu64two)
{
    U64 tmp;

    u64Subtract(pu64one, pu64two, &tmp);
    u64Incr(pu64out, &tmp);
}

netsnmp_feature_child_of(u64copy, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_U64COPY
/**
 * Copy a 64-bit number.
 *
 * @param[in] pu64two Number to be copied.
 * @param[out] pu64one Where to store the copy - *pu64one = *pu64two.
 */
void
u64Copy(U64 * pu64one, const U64 * pu64two)
{
    *pu64one = *pu64two;
}
#endif /* NETSNMP_FEATURE_REMOVE_U64COPY */

/**
 * Set an unsigned 64-bit number to zero.
 *
 * @param[in] pu64 Number to be zeroed.
 */
void
zeroU64(U64 * pu64)
{
    pu64->low = 0;
    pu64->high = 0;
}

/**
 * Check if an unsigned 64-bit number is zero.
 *
 * @param[in] pu64 Number to be checked.
 */
int
isZeroU64(const U64 * pu64)
{
    return pu64->low == 0 && pu64->high == 0;
}

/**
 * check the old and new values of a counter64 for 32bit wrapping
 *
 * @param adjust : set to 1 to auto-increment new_val->high
 *                 if a 32bit wrap is detected.
 *
 * @param old_val
 * @param new_val
 *
 * @note
 * The old and new values must be be from within a time period
 * which would only allow the 32bit portion of the counter to
 * wrap once. i.e. if the 32bit portion of the counter could
 * wrap every 60 seconds, the old and new values should be compared
 * at least every 59 seconds (though I'd recommend at least every
 * 50 seconds to allow for timer inaccuracies).
 *
 * @retval 64 : 64bit wrap
 * @retval 32 : 32bit wrap
 * @retval  0 : did not wrap
 * @retval -1 : bad parameter
 * @retval -2 : unexpected high value (changed by more than 1)
 */
int
netsnmp_c64_check_for_32bit_wrap(struct counter64 *old_val,
                                 struct counter64 *new_val,
                                 int adjust)
{
    if( (NULL == old_val) || (NULL == new_val) )
        return -1;

    DEBUGMSGTL(("9:c64:check_wrap", "check wrap 0x%0lx.0x%0lx 0x%0lx.0x%0lx\n",
                old_val->high, old_val->low, new_val->high, new_val->low));
    
    /*
     * check for wraps
     */
    if ((new_val->low >= old_val->low) &&
        (new_val->high == old_val->high)) {
        DEBUGMSGTL(("9:c64:check_wrap", "no wrap\n"));
        return 0;
    }

    /*
     * low wrapped. did high change?
     */
    if (new_val->high == old_val->high) {
        DEBUGMSGTL(("c64:check_wrap", "32 bit wrap\n"));
        if (adjust)
            new_val->high = (uint32_t)(new_val->high + 1);
        return 32;
    }
    else if (new_val->high == (uint32_t)(old_val->high + 1)) {
        DEBUGMSGTL(("c64:check_wrap", "64 bit wrap\n"));
        return 64;
    }

    return -2;
}

/**
 * update a 64 bit value with the difference between two (possibly) 32 bit vals
 *
 * @param prev_val       : the 64 bit current counter
 * @param old_prev_val   : the (possibly 32 bit) previous value
 * @param new_val        : the (possible 32bit) new value
 * @param need_wrap_check: pointer to integer indicating if wrap check is needed
 *                         flag may be cleared if 64 bit counter is detected
 *
 * @note
 * The old_prev_val and new_val values must be be from within a time
 * period which would only allow the 32bit portion of the counter to
 * wrap once. i.e. if the 32bit portion of the counter could
 * wrap every 60 seconds, the old and new values should be compared
 * at least every 59 seconds (though I'd recommend at least every
 * 50 seconds to allow for timer inaccuracies).
 *
 * Suggested use:
 *
 *   static needwrapcheck = 1;
 *   static counter64 current, prev_val, new_val;
 *
 *   your_functions_to_update_new_value(&new_val);
 *   if (0 == needwrapcheck)
 *      memcpy(current, new_val, sizeof(new_val));
 *   else {
 *      netsnmp_c64_check32_and_update(&current,&new,&prev,&needwrapcheck);
 *      memcpy(prev_val, new_val, sizeof(new_val));
 *   }
 *
 *
 * @retval  0 : success
 * @retval -1 : error checking for 32 bit wrap
 * @retval -2 : look like we have 64 bit values, but sums aren't consistent
 */
int
netsnmp_c64_check32_and_update(struct counter64 *prev_val, struct counter64 *new_val,
                               struct counter64 *old_prev_val, int *need_wrap_check)
{
    int rc;

    /*
     * counters are 32bit or unknown (which we'll treat as 32bit).
     * update the prev values with the difference between the
     * new stats and the prev old_stats:
     *    prev->stats += (new->stats - prev->old_stats)
     */
    if ((NULL == need_wrap_check) || (0 != *need_wrap_check)) {
        rc = netsnmp_c64_check_for_32bit_wrap(old_prev_val,new_val, 1);
        if (rc < 0) {
            DEBUGMSGTL(("c64","32 bit check failed\n"));
            return -1;
        }
    }
    else
        rc = 0;
    
    /*
     * update previous values
     */
    (void) u64UpdateCounter(prev_val, new_val, old_prev_val);

    /*
     * if wrap check was 32 bit, undo adjust, now that prev is updated
     */
    if (32 == rc) {
        /*
         * check wrap incremented high, so reset it. (Because having
         * high set for a 32 bit counter will confuse us in the next update).
         */
        netsnmp_assert(1 == new_val->high);
        new_val->high = 0;
    }
    else if (64 == rc) {
        /*
         * if we really have 64 bit counters, the summing we've been
         * doing for prev values should be equal to the new values.
         */
        if ((prev_val->low != new_val->low) ||
            (prev_val->high != new_val->high)) {
            DEBUGMSGTL(("c64", "looks like a 64bit wrap, but prev!=new\n"));
            return -2;
        }
        else if (NULL != need_wrap_check)
            *need_wrap_check = 0;
    }
    
    return 0;
}

/** Convert an unsigned 64-bit number to ASCII. */
void
printU64(char *buf, /* char [I64CHARSZ+1]; */
         const U64 * pu64)
{
    U64             u64a;
    U64             u64b;

    char            aRes[I64CHARSZ + 1];
    unsigned int    u;
    int             j;

    u64a = *pu64;
    aRes[I64CHARSZ] = 0;
    for (j = 0; j < I64CHARSZ; j++) {
        divBy10(u64a, &u64b, &u);
        aRes[(I64CHARSZ - 1) - j] = (char) ('0' + u);
        u64a = u64b;
        if (isZeroU64(&u64a))
            break;
    }
    strcpy(buf, &aRes[(I64CHARSZ - 1) - j]);
}

/** Convert a signed 64-bit number to ASCII. */
void
printI64(char *buf, /* char [I64CHARSZ+1]; */
         const U64 * pu64)
{
    U64             u64a;

    if (pu64->high & 0x80000000) {
        u64a.high = (uint32_t) ~pu64->high;
        u64a.low = (uint32_t) ~pu64->low;
        incrByU32(&u64a, 1);    /* bit invert and incr by 1 to print 2s complement */
        buf[0] = '-';
        printU64(buf + 1, &u64a);
    } else {
        printU64(buf, pu64);
    }
}

/** Convert a signed 64-bit integer from ASCII to U64. */
int
read64(U64 * i64, const char *str)
{
    U64             i64p;
    unsigned int    u;
    int             sign = 0;
    int             ok = 0;

    zeroU64(i64);
    if (*str == '-') {
        sign = 1;
        str++;
    }

    while (*str && isdigit((unsigned char)(*str))) {
        ok = 1;
        u = *str - '0';
        multBy10(*i64, &i64p);
        *i64 = i64p;
        incrByU16(i64, u);
        str++;
    }
    if (sign) {
        i64->high = (uint32_t) ~i64->high;
        i64->low = (uint32_t) ~i64->low;
        incrByU16(i64, 1);
    }
    return ok;
}
