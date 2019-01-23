/*
 * Driver O/S-independent utility routines
 *
 * Copyright (C) 2016, Broadcom. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: bcmxtlv.c 629929 2016-04-06 23:05:49Z $
 */

#ifndef __FreeBSD__
#include <bcm_cfg.h>
#endif

#include <typedefs.h>
#include <bcmdefs.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <machine/stdarg.h>
#else
#include <stdarg.h>
#endif /* __FreeBSD__ */

#ifdef BCMDRIVER
#include <osl.h>
#else /* !BCMDRIVER */
	#include <stdlib.h> /* AS!!! */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef ASSERT
#define ASSERT(exp)
#endif
INLINE void* MALLOCZ(void *o, size_t s) { BCM_REFERENCE(o); return calloc(1, s); }
INLINE void MFREE(void *o, void *p, size_t s) { BCM_REFERENCE(o); BCM_REFERENCE(s); free(p); }
#endif /* !BCMDRIVER */

#include <bcmendian.h>
#include <bcmutils.h>

static INLINE int bcm_xtlv_size_for_data(int dlen, bcm_xtlv_opts_t opts)
{
	return ((opts & BCM_XTLV_OPTION_ALIGN32) ? ALIGN_SIZE(dlen + BCM_XTLV_HDR_SIZE, 4)
		: (dlen + BCM_XTLV_HDR_SIZE));
}

bcm_xtlv_t *
bcm_next_xtlv(bcm_xtlv_t *elt, int *buflen, bcm_xtlv_opts_t opts)
{
	int sz;
#ifdef BCMDBG
	/* validate current elt */
	if (!bcm_valid_xtlv(elt, *buflen, opts))
		return NULL;
#endif
	/* advance to next elt */
	sz = BCM_XTLV_SIZE(elt, opts);
	elt = (bcm_xtlv_t*)((uint8 *)elt + sz);
	*buflen -= sz;

	/* validate next elt */
	if (!bcm_valid_xtlv(elt, *buflen, opts))
		return NULL;

	return elt;
}

int
bcm_xtlv_buf_init(bcm_xtlvbuf_t *tlv_buf, uint8 *buf, uint16 len, bcm_xtlv_opts_t opts)
{
	if (!tlv_buf || !buf || !len)
		return BCME_BADARG;

	tlv_buf->opts = opts;
	tlv_buf->size = len;
	tlv_buf->head = buf;
	tlv_buf->buf  = buf;
	return BCME_OK;
}

uint16
bcm_xtlv_buf_len(bcm_xtlvbuf_t *tbuf)
{
	if (tbuf == NULL) return 0;
	return (uint16)(tbuf->buf - tbuf->head);
}
uint16
bcm_xtlv_buf_rlen(bcm_xtlvbuf_t *tbuf)
{
	if (tbuf == NULL) return 0;
	return tbuf->size - bcm_xtlv_buf_len(tbuf);
}
uint8 *
bcm_xtlv_buf(bcm_xtlvbuf_t *tbuf)
{
	if (tbuf == NULL) return NULL;
	return tbuf->buf;
}
uint8 *
bcm_xtlv_head(bcm_xtlvbuf_t *tbuf)
{
	if (tbuf == NULL) return NULL;
	return tbuf->head;
}
int
bcm_xtlv_put_data(bcm_xtlvbuf_t *tbuf, uint16 type, const void *data, uint16 dlen)
{
	bcm_xtlv_t *xtlv;
	int size;

	if (tbuf == NULL)
		return BCME_BADARG;
	size = bcm_xtlv_size_for_data(dlen, tbuf->opts);
	if (bcm_xtlv_buf_rlen(tbuf) < size)
		return BCME_NOMEM;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(dlen);
	memcpy(xtlv->data, data, dlen);
	tbuf->buf += size;
	return BCME_OK;
}
int
bcm_xtlv_put_8(bcm_xtlvbuf_t *tbuf, uint16 type, const int8 data)
{
	bcm_xtlv_t *xtlv;
	int size;

	if (tbuf == NULL)
		return BCME_BADARG;
	size = bcm_xtlv_size_for_data(1, tbuf->opts);
	if (bcm_xtlv_buf_rlen(tbuf) < size)
		return BCME_NOMEM;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	xtlv->data[0] = data;
	tbuf->buf += size;
	return BCME_OK;
}
int
bcm_xtlv_put_16(bcm_xtlvbuf_t *tbuf, uint16 type, const int16 data)
{
	bcm_xtlv_t *xtlv;
	int size;

	if (tbuf == NULL)
		return BCME_BADARG;
	size = bcm_xtlv_size_for_data(2, tbuf->opts);
	if (bcm_xtlv_buf_rlen(tbuf) < size)
		return BCME_NOMEM;

	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	htol16_ua_store(data, xtlv->data);
	tbuf->buf += size;
	return BCME_OK;
}
int
bcm_xtlv_put_32(bcm_xtlvbuf_t *tbuf, uint16 type, const int32 data)
{
	bcm_xtlv_t *xtlv;
	int size;

	if (tbuf == NULL)
		return BCME_BADARG;
	size = bcm_xtlv_size_for_data(4, tbuf->opts);
	if (bcm_xtlv_buf_rlen(tbuf) < size)
		return BCME_NOMEM;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	htol32_ua_store(data, xtlv->data);
	tbuf->buf += size;
	return BCME_OK;
}

/*
 *  upacks xtlv record from buf checks the type
 *  copies data to callers buffer
 *  advances tlv pointer to next record
 *  caller's resposible for dst space check
 */
int
bcm_unpack_xtlv_entry(uint8 **tlv_buf, uint16 xpct_type, uint16 xpct_len, void *dst,
	bcm_xtlv_opts_t opts)
{
	bcm_xtlv_t *ptlv = (bcm_xtlv_t *)*tlv_buf;
	uint16 len;
	uint16 type;

	ASSERT(ptlv);
	/* tlv headr is always packed in LE order */
	len = ltoh16(ptlv->len);
	type = ltoh16(ptlv->id);
	if	(len == 0) {
		/* z-len tlv headers: allow, but don't process */
		printf("z-len, skip unpack\n");
	} else  {
		if ((type != xpct_type) ||
			(len > xpct_len)) {
			printf("xtlv_unpack Error: found[type:%d,len:%d] != xpct[type:%d,len:%d]\n",
				type, len, xpct_type, xpct_len);
			return BCME_BADARG;
		}
		/* copy tlv record to caller's buffer */
		memcpy(dst, ptlv->data, ptlv->len);
	}
	*tlv_buf += BCM_XTLV_SIZE(ptlv, opts);
	return BCME_OK;
}

/*
 *  packs user data into tlv record
 *  advances tlv pointer to next xtlv slot
 *  buflen is used for tlv_buf space check
 */
int
bcm_pack_xtlv_entry(uint8 **tlv_buf, uint16 *buflen, uint16 type, uint16 len, void *src,
	bcm_xtlv_opts_t opts)
{
	bcm_xtlv_t *ptlv = (bcm_xtlv_t *)*tlv_buf;
	int size;

	ASSERT(ptlv);
	ASSERT(src);

	size = bcm_xtlv_size_for_data(len, opts);

	/* copy data from tlv buffer to dst provided by user */

	if (size > *buflen - BCM_XTLV_HDR_SIZE) {
		printf("bcm_pack_xtlv_entry: no space tlv_buf: requested:%d, available:%d\n",
			size, *buflen);
		return BCME_BADLEN;
	}
	ptlv->id = htol16(type);
	ptlv->len = htol16(len);

	/* copy callers data */
	memcpy(ptlv->data, src, len);

	/* advance callers pointer to tlv buff */
	*tlv_buf += size;
	/* decrement the len */
	*buflen -= (uint16)size;
	return BCME_OK;
}

/*
 *  unpack all xtlv records from the issue a callback
 *  to set function one call per found tlv record
 */
int
bcm_unpack_xtlv_buf(void *ctx, uint8 *tlv_buf, uint16 buflen, bcm_xtlv_opts_t opts,
	bcm_xtlv_unpack_cbfn_t *cbfn)
{
	uint16 len;
	uint16 type;
	int res = BCME_OK;
	int size;
	bcm_xtlv_t *ptlv;
	int sbuflen = buflen;

	ASSERT(!buflen || tlv_buf);
	ASSERT(!buflen || cbfn);

	while (sbuflen >= (int)BCM_XTLV_HDR_SIZE) {
		ptlv = (bcm_xtlv_t *)tlv_buf;

		/* tlv header is always packed in LE order */
		len = ltoh16(ptlv->len);
		type = ltoh16(ptlv->id);

		size = bcm_xtlv_size_for_data(len, opts);

		sbuflen -= size;
		/* check for possible buffer overrun */
		if (sbuflen < 0)
			break;

		if ((res = cbfn(ctx, ptlv->data, type, len)) != BCME_OK)
			break;
		tlv_buf += size;
	}
	return res;
}

int
bcm_pack_xtlv_buf(void *ctx, void *tlv_buf, uint16 buflen, bcm_xtlv_opts_t opts,
	bcm_pack_xtlv_next_info_cbfn_t get_next, bcm_pack_xtlv_pack_next_cbfn_t pack_next,
	int *outlen)
{
	int res = BCME_OK;
	uint16 tlv_id;
	uint16 tlv_len;
	uint8 *startp;
	uint8 *endp;
	uint8 *buf;
	bool more;
	int size;

	ASSERT(get_next && pack_next);

	buf = (uint8 *)tlv_buf;
	startp = buf;
	endp = (uint8 *)buf + buflen;
	more = TRUE;
	while (more && (buf < endp)) {
		more = get_next(ctx, &tlv_id, &tlv_len);
		size = bcm_xtlv_size_for_data(tlv_len, opts);
		if ((buf + size) >= endp) {
			res = BCME_BUFTOOSHORT;
			goto done;
		}

		htol16_ua_store(tlv_id, buf);
		htol16_ua_store(tlv_len, buf + sizeof(tlv_id));
		pack_next(ctx, tlv_id, tlv_len, buf + BCM_XTLV_HDR_SIZE);
		buf += size;
	}

	if (more)
		res = BCME_BUFTOOSHORT;

done:
	if (outlen) {
		*outlen = (int)(buf - startp);
	}
	return res;
}

/*
 *  pack xtlv buffer from memory according to xtlv_desc_t
 */
int
bcm_pack_xtlv_buf_from_mem(void **tlv_buf, uint16 *buflen, xtlv_desc_t *items,
	bcm_xtlv_opts_t opts)
{
	int res = BCME_OK;
	uint8 *ptlv = (uint8 *)*tlv_buf;

	while (items->type != 0) {
		if ((res = bcm_pack_xtlv_entry(&ptlv,
			buflen, items->type,
			items->len, items->ptr, opts) != BCME_OK)) {
			break;
		}
		items++;
	}
	*tlv_buf = ptlv; /* update the external pointer */
	return res;
}

/*
 *  unpack xtlv buffer to memory according to xtlv_desc_t
 *
 */
int
bcm_unpack_xtlv_buf_to_mem(void *tlv_buf, int *buflen, xtlv_desc_t *items, bcm_xtlv_opts_t opts)
{
	int res = BCME_OK;
	bcm_xtlv_t *elt;

	elt =  bcm_valid_xtlv((bcm_xtlv_t *)tlv_buf, *buflen, opts) ? (bcm_xtlv_t *)tlv_buf : NULL;
	if (!elt || !items) {
		res = BCME_BADARG;
		return res;
	}

	for (; elt != NULL && res == BCME_OK; elt = bcm_next_xtlv(elt, buflen, opts)) {
		/*  find matches in desc_t items  */
		xtlv_desc_t *dst_desc = items;
		uint16 len = ltoh16(elt->len);

		while (dst_desc->type != 0) {
			if (ltoh16(elt->id) == dst_desc->type) {
				if (len != dst_desc->len) {
					res = BCME_BADLEN;
				} else {
					memcpy(dst_desc->ptr, elt->data, len);
				}
				break;
			}
			dst_desc++;
		}
	}

	if (res == BCME_OK && *buflen != 0)
		res =  BCME_BUFTOOSHORT;

	return res;
}

/*
 * return data pointer of a given ID from xtlv buffer.
 * If the specified xTLV ID is found, on return *data_len_out will contain
 * the the data length of the xTLV ID.
 */
void *
bcm_get_data_from_xtlv_buf(uint8 *tlv_buf, uint16 buflen, uint16 id,
	uint16 *datalen_out, bcm_xtlv_opts_t opts)
{
	void *retptr = NULL;
	uint16 type, len;
	int size;
	bcm_xtlv_t *ptlv;
	int sbuflen = buflen;

	while (sbuflen >= (int)BCM_XTLV_HDR_SIZE) {
		ptlv = (bcm_xtlv_t *)tlv_buf;

		/* tlv header is always packed in LE order */
		type = ltoh16(ptlv->id);
		len = ltoh16(ptlv->len);
		size = bcm_xtlv_size_for_data(len, opts);

		sbuflen -= size;
		/* check for possible buffer overrun */
		if (sbuflen < 0) {
			printf("%s %d: Invalid sbuflen %d\n",
				__FUNCTION__, __LINE__, sbuflen);
			break;
		}

		if (id == type) {
			retptr = ptlv->data;
			if (datalen_out) {
				*datalen_out = len;
			}
			break;
		}
		tlv_buf += size;
	}

	return retptr;
}

int bcm_xtlv_size(const bcm_xtlv_t *elt, bcm_xtlv_opts_t opts)
{
	int size; /* entire size of the XTLV including header, data, and optional padding */
	int len; /* XTLV's value real length wthout padding */

	len = BCM_XTLV_LEN(elt);

	size = bcm_xtlv_size_for_data(len, opts);

	return size;
}
