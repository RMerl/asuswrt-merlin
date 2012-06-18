/*
 * ppp_deflate.c - interface the zlib procedures for Deflate compression
 * and decompression (as used by gzip) to the PPP code.
 * This version is for use with STREAMS under SunOS 4.x, Solaris 2,
 * SVR4, OSF/1 and AIX 4.x.
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
 * $Id: deflate.c,v 1.12 2004/01/17 05:47:55 carlsonj Exp $
 */

#ifdef AIX4
#include <net/net_globals.h>
#endif
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <net/ppp_defs.h>
#include "ppp_mod.h"

#define PACKETPTR	mblk_t *
#include <net/ppp-comp.h>

#ifdef __osf__
#include "zlib.h"
#else
#include "../common/zlib.h"
#endif

#ifdef SOL2
#include <sys/sunddi.h>
#endif

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
static void	*z_alloc_init __P((void *, u_int items, u_int size));
static void	z_free __P((void *, void *ptr));
static void	*z_comp_alloc __P((u_char *options, int opt_len));
static void	*z_decomp_alloc __P((u_char *options, int opt_len));
static void	z_comp_free __P((void *state));
static void	z_decomp_free __P((void *state));
static int	z_comp_init __P((void *state, u_char *options, int opt_len,
				 int unit, int hdrlen, int debug));
static int	z_decomp_init __P((void *state, u_char *options, int opt_len,
				     int unit, int hdrlen, int mru, int debug));
static int	z_compress __P((void *state, mblk_t **mret,
				  mblk_t *mp, int slen, int maxolen));
static void	z_incomp __P((void *state, mblk_t *dmsg));
static int	z_decompress __P((void *state, mblk_t *cmp,
				    mblk_t **dmpp));
static void	z_comp_reset __P((void *state));
static void	z_decomp_reset __P((void *state));
static void	z_comp_stats __P((void *state, struct compstat *stats));

/*
 * Procedures exported to ppp_comp.c.
 */
struct compressor ppp_deflate = {
    CI_DEFLATE,			/* compress_proto */
    z_comp_alloc,		/* comp_alloc */
    z_comp_free,		/* comp_free */
    z_comp_init,		/* comp_init */
    z_comp_reset,		/* comp_reset */
    z_compress,			/* compress */
    z_comp_stats,		/* comp_stat */
    z_decomp_alloc,		/* decomp_alloc */
    z_decomp_free,		/* decomp_free */
    z_decomp_init,		/* decomp_init */
    z_decomp_reset,		/* decomp_reset */
    z_decompress,		/* decompress */
    z_incomp,			/* incomp */
    z_comp_stats,		/* decomp_stat */
};

struct compressor ppp_deflate_draft = {
    CI_DEFLATE_DRAFT,		/* compress_proto */
    z_comp_alloc,		/* comp_alloc */
    z_comp_free,		/* comp_free */
    z_comp_init,		/* comp_init */
    z_comp_reset,		/* comp_reset */
    z_compress,			/* compress */
    z_comp_stats,		/* comp_stat */
    z_decomp_alloc,		/* decomp_alloc */
    z_decomp_free,		/* decomp_free */
    z_decomp_init,		/* decomp_init */
    z_decomp_reset,		/* decomp_reset */
    z_decompress,		/* decompress */
    z_incomp,			/* incomp */
    z_comp_stats,		/* decomp_stat */
};

#define DECOMP_CHUNK	512

/*
 * Space allocation and freeing routines for use by zlib routines.
 */
struct zchunk {
    u_int	size;
    u_int	guard;
};

#define GUARD_MAGIC	0x77a6011a

static void *
z_alloc_init(notused, items, size)
    void *notused;
    u_int items, size;
{
    struct zchunk *z;

    size = items * size + sizeof(struct zchunk);
#ifdef __osf__
    z = (struct zchunk *) ALLOC_SLEEP(size);
#else
    z = (struct zchunk *) ALLOC_NOSLEEP(size);
#endif
    z->size = size;
    z->guard = GUARD_MAGIC;
    return (void *) (z + 1);
}

static void *
z_alloc(notused, items, size)
    void *notused;
    u_int items, size;
{
    struct zchunk *z;

    size = items * size + sizeof(struct zchunk);
    z = (struct zchunk *) ALLOC_NOSLEEP(size);
    z->size = size;
    z->guard = GUARD_MAGIC;
    return (void *) (z + 1);
}

static void
z_free(notused, ptr)
    void *notused;
    void *ptr;
{
    struct zchunk *z = ((struct zchunk *) ptr) - 1;

    if (z->guard != GUARD_MAGIC) {
	printf("ppp: z_free of corrupted chunk at %x (%x, %x)\n",
	       z, z->size, z->guard);
	return;
    }
    FREE(z, z->size);
}

/*
 * Allocate space for a compressor.
 */
static void *
z_comp_alloc(options, opt_len)
    u_char *options;
    int opt_len;
{
    struct deflate_state *state;
    int w_size;

    if (opt_len != CILEN_DEFLATE
	|| (options[0] != CI_DEFLATE && options[0] != CI_DEFLATE_DRAFT)
	|| options[1] != CILEN_DEFLATE
	|| DEFLATE_METHOD(options[2]) != DEFLATE_METHOD_VAL
	|| options[3] != DEFLATE_CHK_SEQUENCE)
	return NULL;
    w_size = DEFLATE_SIZE(options[2]);
    /*
     * N.B. the 9 below should be DEFLATE_MIN_SIZE (8), but using
     * 8 will cause kernel crashes because of a bug in zlib.
     */
    if (w_size < 9 || w_size > DEFLATE_MAX_SIZE)
	return NULL;


#ifdef __osf__
    state = (struct deflate_state *) ALLOC_SLEEP(sizeof(*state));
#else
    state = (struct deflate_state *) ALLOC_NOSLEEP(sizeof(*state));
#endif

    if (state == NULL)
	return NULL;

    state->strm.next_in = NULL;
    state->strm.zalloc = (alloc_func) z_alloc_init;
    state->strm.zfree = (free_func) z_free;
    if (deflateInit2(&state->strm, Z_DEFAULT_COMPRESSION, DEFLATE_METHOD_VAL,
		     -w_size, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
	FREE(state, sizeof(*state));
	return NULL;
    }

    state->strm.zalloc = (alloc_func) z_alloc;
    state->w_size = w_size;
    bzero(&state->stats, sizeof(state->stats));
    return (void *) state;
}

static void
z_comp_free(arg)
    void *arg;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    deflateEnd(&state->strm);
    FREE(state, sizeof(*state));
}

static int
z_comp_init(arg, options, opt_len, unit, hdrlen, debug)
    void *arg;
    u_char *options;
    int opt_len, unit, hdrlen, debug;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    if (opt_len < CILEN_DEFLATE
	|| (options[0] != CI_DEFLATE && options[0] != CI_DEFLATE_DRAFT)
	|| options[1] != CILEN_DEFLATE
	|| DEFLATE_METHOD(options[2]) != DEFLATE_METHOD_VAL
	|| DEFLATE_SIZE(options[2]) != state->w_size
	|| options[3] != DEFLATE_CHK_SEQUENCE)
	return 0;

    state->seqno = 0;
    state->unit = unit;
    state->hdrlen = hdrlen;
    state->debug = debug;

    deflateReset(&state->strm);

    return 1;
}

static void
z_comp_reset(arg)
    void *arg;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    state->seqno = 0;
    deflateReset(&state->strm);
}

static int
z_compress(arg, mret, mp, orig_len, maxolen)
    void *arg;
    mblk_t **mret;		/* compressed packet (out) */
    mblk_t *mp;		/* uncompressed packet (in) */
    int orig_len, maxolen;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    u_char *rptr, *wptr;
    int proto, olen, wspace, r, flush;
    mblk_t *m;

    /*
     * Check that the protocol is in the range we handle.
     */
    *mret = NULL;
    rptr = mp->b_rptr;
    if (rptr + PPP_HDRLEN > mp->b_wptr) {
	if (!pullupmsg(mp, PPP_HDRLEN))
	    return 0;
	rptr = mp->b_rptr;
    }
    proto = PPP_PROTOCOL(rptr);
    if (proto > 0x3fff || proto == 0xfd || proto == 0xfb)
	return orig_len;

    /* Allocate one mblk initially. */
    if (maxolen > orig_len)
	maxolen = orig_len;
    if (maxolen <= PPP_HDRLEN + 2) {
	wspace = 0;
	m = NULL;
    } else {
	wspace = maxolen + state->hdrlen;
	if (wspace > 4096)
	    wspace = 4096;
	m = allocb(wspace, BPRI_MED);
    }
    if (m != NULL) {
	*mret = m;
	if (state->hdrlen + PPP_HDRLEN + 2 < wspace) {
	    m->b_rptr += state->hdrlen;
	    m->b_wptr = m->b_rptr;
	    wspace -= state->hdrlen;
	}
	wptr = m->b_wptr;

	/*
	 * Copy over the PPP header and store the 2-byte sequence number.
	 */
	wptr[0] = PPP_ADDRESS(rptr);
	wptr[1] = PPP_CONTROL(rptr);
	wptr[2] = PPP_COMP >> 8;
	wptr[3] = PPP_COMP;
	wptr += PPP_HDRLEN;
	wptr[0] = state->seqno >> 8;
	wptr[1] = state->seqno;
	wptr += 2;
	state->strm.next_out = wptr;
	state->strm.avail_out = wspace - (PPP_HDRLEN + 2);
    } else {
	state->strm.next_out = NULL;
	state->strm.avail_out = 1000000;
    }
    ++state->seqno;

    rptr += (proto > 0xff)? 2: 3;	/* skip 1st proto byte if 0 */
    state->strm.next_in = rptr;
    state->strm.avail_in = mp->b_wptr - rptr;
    mp = mp->b_cont;
    flush = (mp == NULL)? Z_PACKET_FLUSH: Z_NO_FLUSH;
    olen = 0;
    for (;;) {
	r = deflate(&state->strm, flush);
	if (r != Z_OK) {
	    printf("z_compress: deflate returned %d (%s)\n",
		   r, (state->strm.msg? state->strm.msg: ""));
	    break;
	}
	if (flush != Z_NO_FLUSH && state->strm.avail_out != 0)
	    break;		/* all done */
	if (state->strm.avail_in == 0 && mp != NULL) {
	    state->strm.next_in = mp->b_rptr;
	    state->strm.avail_in = mp->b_wptr - mp->b_rptr;
	    mp = mp->b_cont;
	    if (mp == NULL)
		flush = Z_PACKET_FLUSH;
	}
	if (state->strm.avail_out == 0) {
	    if (m != NULL) {
		m->b_wptr += wspace;
		olen += wspace;
		wspace = maxolen - olen;
		if (wspace <= 0) {
		    wspace = 0;
		    m->b_cont = NULL;
		} else {
		    if (wspace < 32)
			wspace = 32;
		    else if (wspace > 4096)
			wspace = 4096;
		    m->b_cont = allocb(wspace, BPRI_MED);
		}
		m = m->b_cont;
		if (m != NULL) {
		    state->strm.next_out = m->b_wptr;
		    state->strm.avail_out = wspace;
		}
	    }
	    if (m == NULL) {
		state->strm.next_out = NULL;
		state->strm.avail_out = 1000000;
	    }
	}
    }
    if (m != NULL) {
	m->b_wptr += wspace - state->strm.avail_out;
	olen += wspace - state->strm.avail_out;
    }

    /*
     * See if we managed to reduce the size of the packet.
     */
    if (olen < orig_len && m != NULL) {
	state->stats.comp_bytes += olen;
	state->stats.comp_packets++;
    } else {
	if (*mret != NULL) {
	    freemsg(*mret);
	    *mret = NULL;
	}
	state->stats.inc_bytes += orig_len;
	state->stats.inc_packets++;
	olen = orig_len;
    }
    state->stats.unc_bytes += orig_len;
    state->stats.unc_packets++;

    return olen;
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

    if (opt_len != CILEN_DEFLATE
	|| (options[0] != CI_DEFLATE && options[0] != CI_DEFLATE_DRAFT)
	|| options[1] != CILEN_DEFLATE
	|| DEFLATE_METHOD(options[2]) != DEFLATE_METHOD_VAL
	|| options[3] != DEFLATE_CHK_SEQUENCE)
	return NULL;
    w_size = DEFLATE_SIZE(options[2]);
    /*
     * N.B. the 9 below should be DEFLATE_MIN_SIZE (8), but using
     * 8 will cause kernel crashes because of a bug in zlib.
     */
    if (w_size < 9 || w_size > DEFLATE_MAX_SIZE)
	return NULL;

#ifdef __osf__
    state = (struct deflate_state *) ALLOC_SLEEP(sizeof(*state));
#else
    state = (struct deflate_state *) ALLOC_NOSLEEP(sizeof(*state));
#endif
    if (state == NULL)
	return NULL;

    state->strm.next_out = NULL;
    state->strm.zalloc = (alloc_func) z_alloc_init;
    state->strm.zfree = (free_func) z_free;
    if (inflateInit2(&state->strm, -w_size) != Z_OK) {
	FREE(state, sizeof(*state));
	return NULL;
    }

    state->strm.zalloc = (alloc_func) z_alloc;
    state->w_size = w_size;
    bzero(&state->stats, sizeof(state->stats));
    return (void *) state;
}

static void
z_decomp_free(arg)
    void *arg;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    inflateEnd(&state->strm);
    FREE(state, sizeof(*state));
}

static int
z_decomp_init(arg, options, opt_len, unit, hdrlen, mru, debug)
    void *arg;
    u_char *options;
    int opt_len, unit, hdrlen, mru, debug;
{
    struct deflate_state *state = (struct deflate_state *) arg;

    if (opt_len < CILEN_DEFLATE
	|| (options[0] != CI_DEFLATE && options[0] != CI_DEFLATE_DRAFT)
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
z_decompress(arg, mi, mop)
    void *arg;
    mblk_t *mi, **mop;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    mblk_t *mo, *mo_head;
    u_char *rptr, *wptr;
    int rlen, olen, ospace;
    int seq, i, flush, r, decode_proto;
    u_char hdr[PPP_HDRLEN + DEFLATE_OVHD];

    *mop = NULL;
    rptr = mi->b_rptr;
    for (i = 0; i < PPP_HDRLEN + DEFLATE_OVHD; ++i) {
	while (rptr >= mi->b_wptr) {
	    mi = mi->b_cont;
	    if (mi == NULL)
		return DECOMP_ERROR;
	    rptr = mi->b_rptr;
	}
	hdr[i] = *rptr++;
    }

    /* Check the sequence number. */
    seq = (hdr[PPP_HDRLEN] << 8) + hdr[PPP_HDRLEN+1];
    if (seq != state->seqno) {
#if !DEFLATE_DEBUG
	if (state->debug)
#endif
	    printf("z_decompress%d: bad seq # %d, expected %d\n",
		   state->unit, seq, state->seqno);
	return DECOMP_ERROR;
    }
    ++state->seqno;

    /* Allocate an output message block. */
    mo = allocb(DECOMP_CHUNK + state->hdrlen, BPRI_MED);
    if (mo == NULL)
	return DECOMP_ERROR;
    mo_head = mo;
    mo->b_cont = NULL;
    mo->b_rptr += state->hdrlen;
    mo->b_wptr = wptr = mo->b_rptr;
    ospace = DECOMP_CHUNK;
    olen = 0;

    /*
     * Fill in the first part of the PPP header.  The protocol field
     * comes from the decompressed data.
     */
    wptr[0] = PPP_ADDRESS(hdr);
    wptr[1] = PPP_CONTROL(hdr);
    wptr[2] = 0;

    /*
     * Set up to call inflate.  We set avail_out to 1 initially so we can
     * look at the first byte of the output and decide whether we have
     * a 1-byte or 2-byte protocol field.
     */
    state->strm.next_in = rptr;
    state->strm.avail_in = mi->b_wptr - rptr;
    mi = mi->b_cont;
    flush = (mi == NULL)? Z_PACKET_FLUSH: Z_NO_FLUSH;
    rlen = state->strm.avail_in + PPP_HDRLEN + DEFLATE_OVHD;
    state->strm.next_out = wptr + 3;
    state->strm.avail_out = 1;
    decode_proto = 1;

    /*
     * Call inflate, supplying more input or output as needed.
     */
    for (;;) {
	r = inflate(&state->strm, flush);
	if (r != Z_OK) {
#if !DEFLATE_DEBUG
	    if (state->debug)
#endif
		printf("z_decompress%d: inflate returned %d (%s)\n",
		       state->unit, r, (state->strm.msg? state->strm.msg: ""));
	    freemsg(mo_head);
	    return DECOMP_FATALERROR;
	}
	if (flush != Z_NO_FLUSH && state->strm.avail_out != 0)
	    break;		/* all done */
	if (state->strm.avail_in == 0 && mi != NULL) {
	    state->strm.next_in = mi->b_rptr;
	    state->strm.avail_in = mi->b_wptr - mi->b_rptr;
	    rlen += state->strm.avail_in;
	    mi = mi->b_cont;
	    if (mi == NULL)
		flush = Z_PACKET_FLUSH;
	}
	if (state->strm.avail_out == 0) {
	    if (decode_proto) {
		state->strm.avail_out = ospace - PPP_HDRLEN;
		if ((wptr[3] & 1) == 0) {
		    /* 2-byte protocol field */
		    wptr[2] = wptr[3];
		    --state->strm.next_out;
		    ++state->strm.avail_out;
		}
		decode_proto = 0;
	    } else {
		mo->b_wptr += ospace;
		olen += ospace;
		mo->b_cont = allocb(DECOMP_CHUNK, BPRI_MED);
		mo = mo->b_cont;
		if (mo == NULL) {
		    freemsg(mo_head);
		    return DECOMP_ERROR;
		}
		state->strm.next_out = mo->b_rptr;
		state->strm.avail_out = ospace = DECOMP_CHUNK;
	    }
	}
    }
    if (decode_proto) {
	freemsg(mo_head);
	return DECOMP_ERROR;
    }
    mo->b_wptr += ospace - state->strm.avail_out;
    olen += ospace - state->strm.avail_out;

#if DEFLATE_DEBUG
    if (olen > state->mru + PPP_HDRLEN)
	printf("ppp_deflate%d: exceeded mru (%d > %d)\n",
	       state->unit, olen, state->mru + PPP_HDRLEN);
#endif

    state->stats.unc_bytes += olen;
    state->stats.unc_packets++;
    state->stats.comp_bytes += rlen;
    state->stats.comp_packets++;

    *mop = mo_head;
    return DECOMP_OK;
}

/*
 * Incompressible data has arrived - add it to the history.
 */
static void
z_incomp(arg, mi)
    void *arg;
    mblk_t *mi;
{
    struct deflate_state *state = (struct deflate_state *) arg;
    u_char *rptr;
    int rlen, proto, r;

    /*
     * Check that the protocol is one we handle.
     */
    rptr = mi->b_rptr;
    if (rptr + PPP_HDRLEN > mi->b_wptr) {
	if (!pullupmsg(mi, PPP_HDRLEN))
	    return;
	rptr = mi->b_rptr;
    }
    proto = PPP_PROTOCOL(rptr);
    if (proto > 0x3fff || proto == 0xfd || proto == 0xfb)
	return;

    ++state->seqno;

    /*
     * Iterate through the message blocks, adding the characters in them
     * to the decompressor's history.  For the first block, we start
     * at the either the 1st or 2nd byte of the protocol field,
     * depending on whether the protocol value is compressible.
     */
    rlen = mi->b_wptr - mi->b_rptr;
    state->strm.next_in = rptr + 3;
    state->strm.avail_in = rlen - 3;
    if (proto > 0xff) {
	--state->strm.next_in;
	++state->strm.avail_in;
    }
    for (;;) {
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
	mi = mi->b_cont;
	if (mi == NULL)
	    break;
	state->strm.next_in = mi->b_rptr;
	state->strm.avail_in = mi->b_wptr - mi->b_rptr;
	rlen += state->strm.avail_in;
    }

    /*
     * Update stats.
     */
    state->stats.inc_bytes += rlen;
    state->stats.inc_packets++;
    state->stats.unc_bytes += rlen;
    state->stats.unc_packets++;
}

#endif /* DO_DEFLATE */
