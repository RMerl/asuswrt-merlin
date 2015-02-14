/*
 * Helpers for netfiler modules.  This file provides implementations for basic
 * functions such as strncasecmp(), etc.
 *
 * gcc will warn for defined but unused functions, so we only include the
 * functions requested.  The following macros are used:
 *   NF_NEED_STRNCASECMP        nf_strncasecmp()
 *   NF_NEED_STRTOU16           nf_strtou16()
 *   NF_NEED_STRTOU32           nf_strtou32()
 */
#ifndef _NETFILTER_HELPERS_H
#define _NETFILTER_HELPERS_H

/* Only include these functions for kernel code. */
#ifdef __KERNEL__

#include <linux/ctype.h>
#define iseol(c) ( (c) == '\r' || (c) == '\n' )

/*
 * The standard strncasecmp()
 */
#ifdef NF_NEED_STRNCASECMP
static int
nf_strncasecmp(const char* s1, const char* s2, u_int32_t len)
{
    if (s1 == NULL || s2 == NULL)
    {
        if (s1 == NULL && s2 == NULL)
        {
            return 0;
        }
        return (s1 == NULL) ? -1 : 1;
    }
    while (len > 0 && tolower(*s1) == tolower(*s2))
    {
        len--;
        s1++;
        s2++;
    }
    return ( (len == 0) ? 0 : (tolower(*s1) - tolower(*s2)) );
}
#endif /* NF_NEED_STRNCASECMP */

/*
 * Parse a string containing a 16-bit unsigned integer.
 * Returns the number of chars used, or zero if no number is found.
 */
#ifdef NF_NEED_STRTOU16
static int
nf_strtou16(const char* pbuf, u_int16_t* pval)
{
    int n = 0;

    *pval = 0;
    while (isdigit(pbuf[n]))
    {
        *pval = (*pval * 10) + (pbuf[n] - '0');
        n++;
    }

    return n;
}
#endif /* NF_NEED_STRTOU16 */

/*
 * Parse a string containing a 32-bit unsigned integer.
 * Returns the number of chars used, or zero if no number is found.
 */
#ifdef NF_NEED_STRTOU32
static int
nf_strtou32(const char* pbuf, u_int32_t* pval)
{
    int n = 0;

    *pval = 0;
    while (pbuf[n] >= '0' && pbuf[n] <= '9')
    {
        *pval = (*pval * 10) + (pbuf[n] - '0');
        n++;
    }

    return n;
}
#endif /* NF_NEED_STRTOU32 */

/*
 * Given a buffer and length, advance to the next line and mark the current
 * line.
 */
#ifdef NF_NEED_NEXTLINE
static int
nf_nextline(char* p, uint len, uint* poff, uint* plineoff, uint* plinelen)
{
    uint    off = *poff;
    uint    physlen = 0;

    if (off >= len)
    {
        return 0;
    }

    while (p[off] != '\n')
    {
        if (len-off <= 1)
        {
            return 0;
        }

        physlen++;
        off++;
    }

    /* if we saw a crlf, physlen needs adjusted */
    if (physlen > 0 && p[off] == '\n' && p[off-1] == '\r')
    {
        physlen--;
    }

    /* advance past the newline */
    off++;

    *plineoff = *poff;
    *plinelen = physlen;
    *poff = off;

    return 1;
}
#endif /* NF_NEED_NEXTLINE */

#endif /* __KERNEL__ */

#endif /* _NETFILTER_HELPERS_H */
