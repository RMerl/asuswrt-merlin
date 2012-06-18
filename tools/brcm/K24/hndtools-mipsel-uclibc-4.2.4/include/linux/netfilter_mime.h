/*
 * MIME functions for netfilter modules.  This file provides implementations
 * for basic MIME parsing.  MIME headers are used in many protocols, such as
 * HTTP, RTSP, SIP, etc.
 *
 * gcc will warn for defined but unused functions, so we only include the
 * functions requested.  The following macros are used:
 *   NF_NEED_MIME_NEXTLINE      nf_mime_nextline()
 */
#ifndef _NETFILTER_MIME_H
#define _NETFILTER_MIME_H

/* Only include these functions for kernel code. */
#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/ctype.h>

/*
 * Given a buffer and length, advance to the next line and mark the current
 * line.  If the current line is empty, *plinelen will be set to zero.  If
 * not, it will be set to the actual line length (including CRLF).
 *
 * 'line' in this context means logical line (includes LWS continuations).
 * Returns 1 on success, 0 on failure.
 */
#ifdef NF_NEED_MIME_NEXTLINE
static int
nf_mime_nextline(char* p, uint len, uint* poff, uint* plineoff, uint* plinelen)
{
    uint    off = *poff;
    uint    physlen = 0;
    int     is_first_line = 1;

    if (off >= len)
    {
        return 0;
    }

    do
    {
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

        /* check for an empty line */
        if (physlen == 0)
        {
            break;
        }

        /* check for colon on the first physical line */
        if (is_first_line)
        {
            is_first_line = 0;
            if (memchr(p+(*poff), ':', physlen) == NULL)
            {
                return 0;
            }
        }
    }
    while (p[off] == ' ' || p[off] == '\t');

    *plineoff = *poff;
    *plinelen = (physlen == 0) ? 0 : (off - *poff);
    *poff = off;

    return 1;
}
#endif /* NF_NEED_MIME_NEXTLINE */

#endif /* __KERNEL__ */

#endif /* _NETFILTER_MIME_H */
