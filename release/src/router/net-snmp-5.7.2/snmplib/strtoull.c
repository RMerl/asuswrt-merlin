/*
 * An implementation of strtoull() for compilers that do not have this
 * function, e.g. MSVC.
 * See also http://www.opengroup.org/onlinepubs/000095399/functions/strtoul.html
 * for more information about strtoull().
 */


/*
 * For MSVC, disable the warning "unary minus operator applied to unsigned
 * type, result still unsigned"
 */
#ifdef _MSC_VER
#pragma warning (disable: 4146)
#endif


#define __STDC_CONSTANT_MACROS  /* Enable UINT64_C in <stdint.h> */
#define __STDC_FORMAT_MACROS    /* Enable PRIu64 in <inttypes.h> */

#include <net-snmp/net-snmp-config.h>

#include <errno.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/system.h>

/*
 * UINT64_C: C99 macro for the suffix for uint64_t constants. 
 */
#ifndef UINT64_C
#ifdef _MSC_VER
#define UINT64_C(c) c##ui64
#else
#define UINT64_C(c) c##ULL
#endif
#endif

/*
 * According to the C99 standard, the constant ULLONG_MAX must be defined in
 * <limits.h>. Define it here for pre-C99 compilers.
 */
#ifndef ULLONG_MAX
#define ULLONG_MAX UINT64_C(0xffffffffffffffff)
#endif

uint64_t
strtoull(const char *nptr, char **endptr, int base)
{
    uint64_t        result = 0;
    const char     *p;
    const char     *first_nonspace;
    const char     *digits_start;
    int             sign = 1;
    int             out_of_range = 0;

    if (base != 0 && (base < 2 || base > 36))
        goto invalid_input;

    p = nptr;

    /*
     * Process the initial, possibly empty, sequence of white-space characters.
     */
    while (isspace((unsigned char) (*p)))
        p++;

    first_nonspace = p;

    /*
     * Determine sign.
     */
    if (*p == '+')
        p++;
    else if (*p == '-') {
        p++;
        sign = -1;
    }

    if (base == 0) {
        /*
         * Determine base.
         */
        if (*p == '0') {
            if ((p[1] == 'x' || p[1] == 'X')) {
                if (isxdigit((unsigned char)(p[2]))) {
                    base = 16;
                    p += 2;
                } else {
                    /*
                     * Special case: treat the string "0x" without any further
                     * hex digits as a decimal number.
                     */
                    base = 10;
                }
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        /*
         * For base 16, skip the optional "0x" / "0X" prefix.
         */
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')
            && isxdigit((unsigned char)(p[2]))) {
            p += 2;
        }
    }

    digits_start = p;

    for (; *p; p++) {
        int             digit;
        digit = ('0' <= *p && *p <= '9') ? *p - '0'
            : ('a' <= *p && *p <= 'z') ? (*p - 'a' + 10)
            : ('A' <= *p && *p <= 'Z') ? (*p - 'A' + 10) : 36;
        if (digit < base) {
            if (! out_of_range) {
                if (result > ULLONG_MAX / base
                    || result * base > ULLONG_MAX - digit) {
                    out_of_range = 1;
                }
                result = result * base + digit;
            }
        } else
            break;
    }

    if (p > first_nonspace && p == digits_start)
        goto invalid_input;

    if (p == first_nonspace)
        p = nptr;

    if (endptr)
        *endptr = (char *) p;

    if (out_of_range) {
        errno = ERANGE;
        return ULLONG_MAX;
    }

    return sign > 0 ? result : -result;

  invalid_input:
    errno = EINVAL;
    if (endptr)
        *endptr = (char *) nptr;
    return 0;
}
