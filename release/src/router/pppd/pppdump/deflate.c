/*
 * ppp_deflate.c - interface the zlib procedures for Deflate compression
 * and decompression (as used by gzip) to the PPP code.
 *
 * Copyright (c) 1994 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: deflate.c,v 1.5 2004/01/17 05:47:55 carlsonj Exp $
 */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ppp_defs.h"
#include "ppp-comp.h"
#include "zlib.h"

#if DO_DEFLATE

#define DEFLATE_DEBUG	1

/*
 * State for a Deflate (de)compressor.
 */
struct deflate_state {
    int		seqno;
    int		w_size;
    int		unit;
    int		hdrlen;
    int		mru;
    int		debug;
    z_stream	strm;
    struct compstat stats;
};

#define DEFLATE_OVHD	2		/* Deflate overhead/packet */

static void	*z_alloc __P((void *, u_int items, u_int size));
static void	z_free __P((void *, void *ptr, u_int nb));
static void	*z_decomp_alloc __P((u_char *options, int opt_len));
static void	z_decomp_free __P((void *state));
static int	z_decomp_init __P((void *state, u_char *options, int opt_len,
				     int unit, int hdrlen, int mru, int debug));
static void	z_incomp __P((void *state, u_char *dmsg, int len));
static int	z_decompress __P((void *state, u_char *cmp, int inlen,
				    u_char *dmp, int *outlenp));
static void	z_decomp_reset __P((void *state));
static void	z_comp_stats __P((void *state, struct compstat *stats));

/*
 * Procedures exported to if_ppp.c.
 */
struct compressor ppp_deflate = {
    CI_DEFLATE,			/* compress_proto */
    z_decomp_alloc,		/* decomp_alloc */
    z_decomp_free,		/* decomp_free */
    z_decomp_init,		/* decomp_init */
    z_decomp_reset,		/* decomp_reset */
    z_decompress,		/* decompress */
    z_incomp,			/* incomp */
    z_comp_stats,		/* decomp_stat */
};

/*
 * Space allocation and freeing routines for use by zlib routines.
 */
static void *
z_alloc(notused, items, size)
    void *notused;
    u_int items, size;
{
    return malloc(items * size);
}

static void
z_free(notused, ptr, nbytes)
    void *notused;
    void *ptr;
    u_int nbytes;
{
    free(ptr);
}

static void
z_comp_stats(arg, stats)
    void *arg;
    struct compstat *stats;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    u_int out;

    *stats = state->stats;
    stats->ratio = stats->unc_bytes;
    out = stats->comp_bytes + stats->unc_bytes;
    if (stats->ratio <= 0x7ffffff)
	stats->ratio <<= 8;
    else
	out >>= 8;
    if (out != 0)
	stats->ratio /= out;
}

/*
 * Allocate space for a decompressor.
 */
static void *
z_decomp_alloc(options, opt_len)
    u_char *options;
    int opt_len;
{
    struct deflate_state *state;
    int w_size;

    if (opt_len != CILEN_DEFLATE || options[0] != CI_DEFLATE
	|| options[1] != CILEN_DEFLATE
	|| DEFLATE_METHOD(options[2]) != DEFLATE_METHOD_VAL
	|| options[3] != DEFLATE_CHK_SEQUENCE)
	return NULL;
    w_size = DEFLATE_SIZE(options[2]);
    if (w_size < DEFLATE_MIN_SIZE || w_size > DEFLATE_MAX_SIZE)
	return NULL;

    state = (struct deflate_state *) malloc(sizeof(*state));
    if (state == NULL)
	return NULL;

    state->strm.next_out = NULL;
    state->strm.zalloc = (alloc_func) z_alloc;
    state->strm.zfree = (free_func) z_free;
    if (inflateInit2(&state->strm, -w_size) != Z_OK) {
	free(state);
	return NULL;
    }

    state->w_size = w_size;
    memset(&state->stats, 0, sizeof(state->stats));
    return (void *) state;
}

static void
z_decomp_free(arg)
    void *arg;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    inflateEnd(&state->strm);
    free(state);
}

static int
z_decomp_init(arg, options, opt_len, unit, hdrlen, mru, debug)
    void *arg;
    u_char *options;
    int opt_len, unit, hdrlen, mru, debug;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    if (opt_len < CILEN_DEFLATE || options[0] != CI_DEFLATE
	|| options[1] != CILEN_DEFLATE
	|| DEFLATE_METHOD(options[2]) != DEFLATE_METHOD_VAL
	|| DEFLATE_SIZE(options[2]) != state->w_size
	|| options[3] != DEFLATE_CHK_SEQUENCE)
	return 0;

    state->seqno = 0;
    state->unit = unit;
    state->hdrlen = hdrlen;
    state->debug = debug;
    state->mru = mru;

    inflateReset(&state->strm);

    return 1;
}

static void
z_decomp_reset(arg)
    void *arg;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    state->seqno = 0;
    inflateReset(&state->strm);
}

/*
 * Decompress a Deflate-compressed packet.
 *
 * Because of patent problems, we return DECOMP_ERROR for errors
 * found by inspecting the input data and for system problems, but
 * DECOMP_FATALERROR for any errors which could possibly be said to
 * be being detected "after" decompression.  For DECOMP_ERROR,
 * we can issue a CCP reset-request; for DECOMP_FATALERROR, we may be
 * infringing a patent of Motorola's if we do, so we take CCP down
 * instead.
 *
 * Given that the frame has the correct sequence number and a good FCS,
 * errors such as invalid codes in the input most likely indicate a
 * bug, so we return DECOMP_FATALERROR for them in order to turn off
 * compression, even though they are detected by inspecting the input.
 */
static int
z_decompress(arg, mi, inlen, mo, outlenp)
    void *arg;
    u_char *mi, *mo;
    int inlen, *outlenp;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    u_char *rptr, *wptr;
    int rlen, olen;
    int seq, r;

    rptr = mi;
    if (*rptr == 0)
	++rptr;
    ++rptr;

    /* Check the sequence number. */
    seq = (rptr[0] << 8) + rptr[1];
    rptr += 2;
    if (seq != state->seqno) {
#if !DEFLATE_DEBUG
	if (state->debug)
#endif
	    printf("z_decompress%d: bad seq # %d, expected %d\n",
		   state->unit, seq, state->seqno);
	return DECOMP_ERROR;
    }
    ++state->seqno;

    /*
     * Set up to call inflate.
     */
    wptr = mo;
    state->strm.next_in = rptr;
    state->strm.avail_in = mi + inlen - rptr;
    rlen = state->strm.avail_in + PPP_HDRLEN + DEFLATE_OVHD;
    state->strm.next_out = wptr;
    state->strm.avail_out = state->mru + 2;

    r = inflate(&state->strm, Z_PACKET_FLUSH);
    if (r != Z_OK) {
#if !DEFLATE_DEBUG
	if (state->debug)
#endif
	    printf("z_decompress%d: inflate returned %d (%s)\n",
		   state->unit, r, (state->strm.msg? state->strm.msg: ""));
	return DECOMP_FATALERROR;
    }
    olen = state->mru + 2 - state->strm.avail_out;
    *outlenp = olen;

    if ((wptr[0] & 1) != 0)
	++olen;			/* for suppressed protocol high byte */
    olen += 2;			/* for address, control */

#if DEFLATE_DEBUG
    if (olen > state->mru + PPP_HDRLEN)
	printf("ppp_deflate%d: exceeded mru (%d > %d)\n",
	       state->unit, olen, state->mru + PPP_HDRLEN);
#endif

    state->stats.unc_bytes += olen;
    state->stats.unc_packets++;
    state->stats.comp_bytes += rlen;
    state->stats.comp_packets++;

    return DECOMP_OK;
}

/*
 * Incompressible data has arrived - add it to the history.
 */
static void
z_incomp(arg, mi, mlen)
    void *arg;
    u_char *mi;
    int mlen;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    u_char *rptr;
    int rlen, proto, r;

    /*
     * Check that the protocol is one we handle.
     */
    rptr = mi;
    proto = rptr[0];
    if ((proto & 1) == 0)
	proto = (proto << 8) + rptr[1];
    if (proto > 0x3fff || proto == 0xfd || proto == 0xfb)
	return;

    ++state->seqno;

    if (rptr[0] == 0)
	++rptr;
    rlen = mi + mlen - rptr;
    state->strm.next_in = rptr;
    state->strm.avail_in = rlen;
    r = inflateIncomp(&state->strm);
    if (r != Z_OK) {
	/* gak! */
#if !DEFLATE_DEBUG
	if (state->debug)
#endif
	    printf("z_incomp%d: inflateIncomp returned %d (%s)\n",
		   state->unit, r, (state->strm.msg? state->strm.msg: ""));
	return;
    }

    /*
     * Update stats.
     */
    if (proto <= 0xff)
	++rlen;
    rlen += 2;
    state->stats.inc_bytes += rlen;
    state->stats.inc_packets++;
    state->stats.unc_bytes += rlen;
    state->stats.unc_packets++;
}

#endif /* DO_DEFLATE */
