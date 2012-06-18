/*	$KAME: base64.c,v 1.1 2004/06/08 07:26:56 jinmei Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>

typedef enum { FALSE = 0, TRUE = 1 } boolean_t;

static const char base64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/*
 * State of a base64 decoding process in progress.
 */
typedef struct {
	int length;		/* Desired length of binary data or -1 */
	int digits;		/* Number of buffered base64 digits */
	boolean_t seen_end;	/* True if "=" end marker seen */
	int val[4];

	char *dst;		/* Head of the available space for resulting
				 * binary data */
	char *dstend;		/* End of the buffer */
} base64_decode_ctx_t;

static int
mem_tobuffer(base64_decode_ctx_t *ctx, void *base, unsigned int length)
{
	if (ctx->dst + length >= ctx->dstend)
		return (-1);
	memcpy(ctx->dst, base, length);
	ctx->dst += length;
	return (0);
}

static inline void
base64_decode_init(base64_decode_ctx_t *ctx, int length,
    char *result, size_t resultlen)
{
	ctx->digits = 0;
	ctx->seen_end = FALSE;
	ctx->length = length;
	ctx->dst = result;
	ctx->dstend = result + resultlen;
}

static inline int
base64_decode_char(base64_decode_ctx_t *ctx, int c)
{
	char *s;

	if (ctx->seen_end == TRUE)
		return (-1);
	if ((s = strchr(base64, c)) == NULL)
		return (-1);
	ctx->val[ctx->digits++] = s - base64;
	if (ctx->digits == 4) {
		int n;
		unsigned char buf[3];
		if (ctx->val[0] == 64 || ctx->val[1] == 64)
			return (-1);
		if (ctx->val[2] == 64 && ctx->val[3] != 64)
			return (-1);
		/*
		 * Check that bits that should be zero are.
		 */
		if (ctx->val[2] == 64 && (ctx->val[1] & 0xf) != 0)
			return (-1);
		/*
		 * We don't need to test for ctx->val[2] != 64 as
		 * the bottom two bits of 64 are zero.
		 */
		if (ctx->val[3] == 64 && (ctx->val[2] & 0x3) != 0)
			return (-1);
		n = (ctx->val[2] == 64) ? 1 :
			(ctx->val[3] == 64) ? 2 : 3;
		if (n != 3) {
			ctx->seen_end = TRUE;
			if (ctx->val[2] == 64)
				ctx->val[2] = 0;
			if (ctx->val[3] == 64)
				ctx->val[3] = 0;
		}
		buf[0] = (ctx->val[0]<<2)|(ctx->val[1]>>4);
		buf[1] = (ctx->val[1]<<4)|(ctx->val[2]>>2);
		buf[2] = (ctx->val[2]<<6)|(ctx->val[3]);
		if (mem_tobuffer(ctx, buf, n))
			return (-1);
		if (ctx->length >= 0) {
			if (n > ctx->length)
				return (-1);
			else
				ctx->length -= n;
		}
		ctx->digits = 0;
	}
	return (0);
}

static inline int
base64_decode_finish(base64_decode_ctx_t *ctx)
{
	if (ctx->length > 0)
		return (-1);
	if (ctx->digits != 0)
		return (-1);
	return (0);
}

int
base64_decodestring(const char *cstr, char *result, size_t resultlen)
{
	base64_decode_ctx_t ctx;

	base64_decode_init(&ctx, -1, result, resultlen);
	for (;;) {
		int c = *cstr++;
		if (c == '\0')
			break;
		if (c == ' ' || c == '\t' || c == '\n' || c== '\r')
			continue;
		if (base64_decode_char(&ctx, c))
			return (-1);
	}
	if (base64_decode_finish(&ctx))
		return (-1);
	return (ctx.dst - result);
}
