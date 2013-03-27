/* 
   Handling of compressed HTTP responses
   Copyright (C) 2001-2006, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ne_request.h"
#include "ne_compress.h"
#include "ne_utils.h"
#include "ne_internal.h"

#ifdef NE_HAVE_ZLIB

#include <zlib.h>

/* Adds support for the 'gzip' Content-Encoding in HTTP.  gzip is a
 * file format which wraps the DEFLATE compression algorithm.  zlib
 * implements DEFLATE: we have to unwrap the gzip format (specified in
 * RFC1952) as it comes off the wire, and hand off chunks of data to
 * be inflated. */

struct ne_decompress_s {
    ne_request *request; /* associated request. */
    ne_session *session; /* associated session. */
    /* temporary buffer for holding inflated data. */
    char outbuf[NE_BUFSIZ];
    z_stream zstr;
    int zstrinit; /* non-zero if zstr has been initialized */

    /* pass blocks back to this. */
    ne_block_reader reader;
    ne_accept_response acceptor;
    void *userdata;

    /* buffer for gzip header bytes. */
    unsigned char header[10];
    size_t hdrcount;    /* bytes in header */

    unsigned char footer[8];
    size_t footcount; /* bytes in footer. */

    /* CRC32 checksum: odd that zlib uses uLong for this since it is a
     * 64-bit integer on LP64 platforms. */
    uLong checksum;

    /* current state. */
    enum state {
	NE_Z_BEFORE_DATA, /* not received any response blocks yet. */
	NE_Z_PASSTHROUGH, /* response not compressed: passing through. */
	NE_Z_IN_HEADER, /* received a few bytes of response data, but not
			 * got past the gzip header yet. */
	NE_Z_POST_HEADER, /* waiting for the end of the NUL-terminated bits. */
	NE_Z_INFLATING, /* inflating response bytes. */
	NE_Z_AFTER_DATA, /* after data; reading CRC32 & ISIZE */
	NE_Z_FINISHED /* stream is finished. */
    } state;
};

/* Convert 'buf' to unsigned int; 'buf' must be 'unsigned char *' */
#define BUF2UINT(buf) (((buf)[3]<<24) + ((buf)[2]<<16) + ((buf)[1]<<8) + (buf)[0])

#define ID1 0x1f
#define ID2 0x8b

#define HDR_DONE 0
#define HDR_EXTENDED 1
#define HDR_ERROR 2

#define HDR_ID1(ctx) ((ctx)->header[0])
#define HDR_ID2(ctx) ((ctx)->header[1])
#define HDR_CMETH(ctx) ((ctx)->header[2])
#define HDR_FLAGS(ctx) ((ctx)->header[3])
#define HDR_MTIME(ctx) (BUF2UINT(&(ctx)->header[4]))
#define HDR_XFLAGS(ctx) ((ctx)->header[8])
#define HDR_OS(ctx) ((ctx)->header[9])

/* parse_header parses the gzip header, sets the next state and returns
 *   HDR_DONE: all done, bytes following are raw DEFLATE data.
 *   HDR_EXTENDED: all done, expect a NUL-termianted string
 *                 before the DEFLATE data
 *   HDR_ERROR: invalid header, give up (session error is set).
 */
static int parse_header(ne_decompress *ctx)
{
    NE_DEBUG(NE_DBG_HTTP, "ID1: %d  ID2: %d, cmeth %d, flags %d\n", 
             HDR_ID1(ctx), HDR_ID2(ctx), HDR_CMETH(ctx), HDR_FLAGS(ctx));
    
    if (HDR_ID1(ctx) != ID1 || HDR_ID2(ctx) != ID2 || HDR_CMETH(ctx) != 8) {
	ne_set_error(ctx->session, "Compressed stream invalid");
	return HDR_ERROR;
    }

    NE_DEBUG(NE_DBG_HTTP, "mtime: %d, xflags: %d, os: %d\n",
	     HDR_MTIME(ctx), HDR_XFLAGS(ctx), HDR_OS(ctx));
    
    /* TODO: we can only handle one NUL-terminated extensions field
     * currently.  Really, we should count the number of bits set, and
     * skip as many fields as bits set (bailing if any reserved bits
     * are set. */
    if (HDR_FLAGS(ctx) == 8) {
	ctx->state = NE_Z_POST_HEADER;
	return HDR_EXTENDED;
    } else if (HDR_FLAGS(ctx) != 0) {
	ne_set_error(ctx->session, "Compressed stream not supported");
	return HDR_ERROR;
    }

    NE_DEBUG(NE_DBG_HTTP, "compress: Good stream.\n");
    
    ctx->state = NE_Z_INFLATING;
    return HDR_DONE;
}

/* Process extra 'len' bytes of 'buf' which were received after the
 * DEFLATE data. */
static int process_footer(ne_decompress *ctx, 
			   const unsigned char *buf, size_t len)
{
    if (len + ctx->footcount > 8) {
        ne_set_error(ctx->session, 
                     "Too many bytes (%" NE_FMT_SIZE_T ") in gzip footer",
                     len);
        return -1;
    } else {
	memcpy(ctx->footer + ctx->footcount, buf, len);
	ctx->footcount += len;
	if (ctx->footcount == 8) {
	    uLong crc = BUF2UINT(ctx->footer) & 0xFFFFFFFF;
	    if (crc == ctx->checksum) {
		ctx->state = NE_Z_FINISHED;
		NE_DEBUG(NE_DBG_HTTP, "compress: End of response; checksum match.\n");
	    } else {
		NE_DEBUG(NE_DBG_HTTP, "compress: End of response; checksum mismatch: "
			 "given %lu vs computed %lu\n", crc, ctx->checksum);
		ne_set_error(ctx->session, 
			     "Checksum invalid for compressed stream");
                return -1;
	    }
	}
    }
    return 0;
}

/* A zlib function failed with 'code'; set the session error string
 * appropriately. */
static void set_zlib_error(ne_decompress *ctx, const char *msg, int code)
{
    if (ctx->zstr.msg)
        ne_set_error(ctx->session, "%s: %s", msg, ctx->zstr.msg);
    else {
        const char *err;
        switch (code) {
        case Z_STREAM_ERROR: err = "stream error"; break;
        case Z_DATA_ERROR: err = "data corrupt"; break;
        case Z_MEM_ERROR: err = "out of memory"; break;
        case Z_BUF_ERROR: err = "buffer error"; break;
        case Z_VERSION_ERROR: err = "library version mismatch"; break;
        default: err = "unknown error"; break;
        }
        ne_set_error(ctx->session, _("%s: %s (code %d)"), msg, err, code);
    }
}

/* Inflate response buffer 'buf' of length 'len'. */
static int do_inflate(ne_decompress *ctx, const char *buf, size_t len)
{
    int ret;

    ctx->zstr.avail_in = len;
    ctx->zstr.next_in = (unsigned char *)buf;
    ctx->zstr.total_in = 0;
    
    do {
	ctx->zstr.avail_out = sizeof ctx->outbuf;
	ctx->zstr.next_out = (unsigned char *)ctx->outbuf;
	ctx->zstr.total_out = 0;
	
	ret = inflate(&ctx->zstr, Z_NO_FLUSH);
	
	NE_DEBUG(NE_DBG_HTTP, 
		 "compress: inflate %d, %ld bytes out, %d remaining\n",
		 ret, ctx->zstr.total_out, ctx->zstr.avail_in);
#if 0
	NE_DEBUG(NE_DBG_HTTPBODY,
		 "Inflated body block (%ld):\n[%.*s]\n", 
		 ctx->zstr.total_out, (int)ctx->zstr.total_out, 
		 ctx->outbuf);
#endif
	/* update checksum. */
	ctx->checksum = crc32(ctx->checksum, (unsigned char *)ctx->outbuf, 
			      ctx->zstr.total_out);

	/* pass on the inflated data, if any */
        if (ctx->zstr.total_out > 0) {
            int rret = ctx->reader(ctx->userdata, ctx->outbuf,
                                   ctx->zstr.total_out);
            if (rret) return rret;
        }	
    } while (ret == Z_OK && ctx->zstr.avail_in > 0);
    
    if (ret == Z_STREAM_END) {
	NE_DEBUG(NE_DBG_HTTP, "compress: end of data stream, %d bytes remain.\n",
		 ctx->zstr.avail_in);
	/* process the footer. */
	ctx->state = NE_Z_AFTER_DATA;
	return process_footer(ctx, ctx->zstr.next_in, ctx->zstr.avail_in);
    } else if (ret != Z_OK) {
        set_zlib_error(ctx, _("Could not inflate data"), ret);
        return NE_ERROR;
    }
    return 0;
}

/* Callback which is passed blocks of the response body. */
static int gz_reader(void *ud, const char *buf, size_t len)
{
    ne_decompress *ctx = ud;
    const char *zbuf;
    size_t count;
    const char *hdr;

    if (len == 0) {
        /* End of response: */
        switch (ctx->state) {
        case NE_Z_BEFORE_DATA:
            hdr = ne_get_response_header(ctx->request, "Content-Encoding");
            if (hdr && ne_strcasecmp(hdr, "gzip") == 0) {
                /* response was truncated: return error. */
                break;
            }
            /* else, fall through */
        case NE_Z_FINISHED: /* complete gzip response */
        case NE_Z_PASSTHROUGH: /* complete uncompressed response */
            return ctx->reader(ctx->userdata, buf, 0);
        default:
            /* invalid state: truncated response. */
            break;
        }
	/* else: truncated response, fail. */
	ne_set_error(ctx->session, "Compressed response was truncated");
	return NE_ERROR;
    }        

    switch (ctx->state) {
    case NE_Z_PASSTHROUGH:
	/* move along there. */
	return ctx->reader(ctx->userdata, buf, len);

    case NE_Z_FINISHED:
	/* Could argue for tolerance, and ignoring trailing content;
	 * but it could mean something more serious. */
	if (len > 0) {
	    ne_set_error(ctx->session,
			 "Unexpected content received after compressed stream");
            return NE_ERROR;
	}
        break;

    case NE_Z_BEFORE_DATA:
	/* work out whether this is a compressed response or not. */
        hdr = ne_get_response_header(ctx->request, "Content-Encoding");
        if (hdr && ne_strcasecmp(hdr, "gzip") == 0) {
            int ret;
	    NE_DEBUG(NE_DBG_HTTP, "compress: got gzipped stream.\n");

            /* inflateInit2() works here where inflateInit() doesn't. */
            ret = inflateInit2(&ctx->zstr, -MAX_WBITS);
            if (ret != Z_OK) {
                set_zlib_error(ctx, _("Could not initialize zlib"), ret);
                return -1;
            }
	    ctx->zstrinit = 1;

	} else {
	    /* No Content-Encoding header: pass it on.  TODO: we could
	     * hack it and register the real callback now. But that
	     * would require add_resp_body_rdr to have defined
	     * ordering semantics etc etc */
	    ctx->state = NE_Z_PASSTHROUGH;
	    return ctx->reader(ctx->userdata, buf, len);
	}

	ctx->state = NE_Z_IN_HEADER;
	/* FALLTHROUGH */

    case NE_Z_IN_HEADER:
	/* copy as many bytes as possible into the buffer. */
	if (len + ctx->hdrcount > 10) {
	    count = 10 - ctx->hdrcount;
	} else {
	    count = len;
	}
	memcpy(ctx->header + ctx->hdrcount, buf, count);
	ctx->hdrcount += count;
	/* have we got the full header yet? */
	if (ctx->hdrcount != 10) {
	    return 0;
	}

	buf += count;
	len -= count;

	switch (parse_header(ctx)) {
	case HDR_EXTENDED:
	    if (len == 0)
		return 0;
	    break;
        case HDR_ERROR:
            return NE_ERROR;
	case HDR_DONE:
	    if (len > 0) {
		return do_inflate(ctx, buf, len);
	    }
            break;
	}

	/* FALLTHROUGH */

    case NE_Z_POST_HEADER:
	/* eating the filename string. */
	zbuf = memchr(buf, '\0', len);
	if (zbuf == NULL) {
	    /* not found it yet. */
	    return 0;
	}

	NE_DEBUG(NE_DBG_HTTP,
		 "compresss: skipped %" NE_FMT_SIZE_T " header bytes.\n", 
		 zbuf - buf);
	/* found end of string. */
	len -= (1 + zbuf - buf);
	buf = zbuf + 1;
	ctx->state = NE_Z_INFLATING;
	if (len == 0) {
	    /* end of string was at end of buffer. */
	    return 0;
	}

	/* FALLTHROUGH */

    case NE_Z_INFLATING:
	return do_inflate(ctx, buf, len);

    case NE_Z_AFTER_DATA:
	return process_footer(ctx, (unsigned char *)buf, len);
    }

    return 0;
}

/* Prepare for a compressed response; may be called many times per
 * request, for auth retries etc. */
static void gz_pre_send(ne_request *r, void *ud, ne_buffer *req)
{
    ne_decompress *ctx = ud;

    if (ctx->request == r) {
        NE_DEBUG(NE_DBG_HTTP, "compress: Initialization.\n");
        
        /* (Re-)Initialize the context */
        ctx->state = NE_Z_BEFORE_DATA;
        if (ctx->zstrinit) inflateEnd(&ctx->zstr);
        ctx->zstrinit = 0;
        ctx->hdrcount = ctx->footcount = 0;
        ctx->checksum = crc32(0L, Z_NULL, 0);
    }
}

/* Wrapper for user-passed acceptor function. */
static int gz_acceptor(void *userdata, ne_request *req, const ne_status *st)
{
    ne_decompress *ctx = userdata;
    return ctx->acceptor(ctx->userdata, req, st);
}

/* A slightly ugly hack: the pre_send hook is scoped per-session, so
 * must check that the invoking request is this one, before doing
 * anything, and must be unregistered when the context is
 * destroyed. */
ne_decompress *ne_decompress_reader(ne_request *req, ne_accept_response acpt,
				    ne_block_reader rdr, void *userdata)
{
    ne_decompress *ctx = ne_calloc(sizeof *ctx);

    ne_add_request_header(req, "Accept-Encoding", "gzip");

    ne_add_response_body_reader(req, gz_acceptor, gz_reader, ctx);

    ctx->reader = rdr;
    ctx->userdata = userdata;
    ctx->session = ne_get_session(req);
    ctx->request = req;
    ctx->acceptor = acpt;

    ne_hook_pre_send(ne_get_session(req), gz_pre_send, ctx);

    return ctx;    
}

void ne_decompress_destroy(ne_decompress *ctx)
{
    if (ctx->zstrinit) inflateEnd(&ctx->zstr);

    ne_unhook_pre_send(ctx->session, gz_pre_send, ctx);

    ne_free(ctx);
}

#else /* !NE_HAVE_ZLIB */

/* Pass-through interface present to provide ABI compatibility. */

ne_decompress *ne_decompress_reader(ne_request *req, ne_accept_response acpt,
				    ne_block_reader rdr, void *userdata)
{
    ne_add_response_body_reader(req, acpt, rdr, userdata);
    /* an arbitrary return value: don't confuse them by returning NULL. */
    return (ne_decompress *)req;
}

void ne_decompress_destroy(ne_decompress *dc)
{
}

#endif /* NE_HAVE_ZLIB */
