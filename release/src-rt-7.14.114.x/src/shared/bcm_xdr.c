/*
 * Utilites for XDR encode and decode of data
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_xdr.c 319672 2012-03-08 22:02:53Z $
 */

#include <osl.h>
#include <typedefs.h>
#include <bcmendian.h>
#include <bcm_xdr.h>

void
bcm_xdr_buf_init(bcm_xdr_buf_t *b, void *buf, size_t len)
{
	b->buf = b->origbuf = buf;
	b->size = b->origsize = (uint)len;
}

int
bcm_xdr_pack_uint8_vec(bcm_xdr_buf_t *b, uint8 *vec, uint32 elems)
{
	int err = 0;
	err = bcm_xdr_pack_opaque(b, elems, vec);

	return err;
}

int
bcm_xdr_unpack_uint8_vec(bcm_xdr_buf_t *b, uint8 *vec, uint32 elems)
{
	int err = 0;
	err = bcm_xdr_unpack_opaque_cpy(b, elems, vec);

	return err;
}

/* Pack 16-bit vectors */
int
bcm_xdr_pack_uint16_vec(bcm_xdr_buf_t *b, uint len, void *vec)
{
	size_t tot_len, r;
	int i;
	uint16	*vec16 = (uint16*)vec, *buf16 = (uint16*)b->buf;
	ASSERT((len % sizeof(uint16)) == 0);

	/* calc residual padding to 4 bytes */
	r = (4 - len) & 3;

	tot_len = len + r;

	if (b->size < tot_len)
		return -1;

	/* Do the 16 bit swap and copy */
	for (i = 0; i < (int)(len/sizeof(uint16)); i++)
		buf16[i] = htol16(vec16[i]);

	/* Padding */
	memset(b->buf + len, 0, r);

	b->size -= tot_len;
	b->buf += tot_len;

	return 0;
}

/* Unpack 16-bit vectors */
int
bcm_xdr_unpack_uint16_vec(bcm_xdr_buf_t *b, uint len, void *vec)
{
	int err = 0, i;
	uint16	*vec16 = (uint16*)vec;
	ASSERT((len % sizeof(uint16)) == 0);

	err = bcm_xdr_unpack_opaque_cpy(b, len, vec);

	/* Do the 16 bit swapping in the copied buffer */
	for (i = 0; i < (int)(len/sizeof(uint16)); i++)
		vec16[i] = ltoh16(vec16[i]);

	return err;
}

/* Pack 32-bit vectors */
int
bcm_xdr_pack_uint32_vec(bcm_xdr_buf_t *b, uint len, void *vec)
{
	int i;
	uint32	*vec32 = (uint32*)vec, *buf32 = (uint32*)b->buf;
	ASSERT((len % sizeof(uint32)) == 0);

	if (b->size < len)
		return -1;

	/* Do the 32 bit swap and copy */
	for (i = 0; i < (int)(len/sizeof(uint32)); i++)
		buf32[i] = htol32(vec32[i]);

	b->size -= len;
	b->buf += len;

	return 0;
}

/* Unpack 32-bit vectors */
int
bcm_xdr_unpack_uint32_vec(bcm_xdr_buf_t *b, uint len, void *vec)
{
	int err = 0, i;
	uint32	*vec32 = (uint32*)vec;
	ASSERT((len % sizeof(uint32)) == 0);

	err = bcm_xdr_unpack_opaque_cpy(b, len, vec);

	/* Do the 32 bit swapping in the copied buffer */
	for (i = 0; i < (int)(len/sizeof(uint32)); i++)
		vec32[i] = ltoh32(vec32[i]);

	return err;
}

int
bcm_xdr_pack_uint32(bcm_xdr_buf_t *b, uint32 val)
{
	if (b->size < 4)
		return -1;

	*(uint32*)(b->buf) = htol32((uint32)val);
	b->size -= 4;
	b->buf += 4;

	return 0;
}

int
bcm_xdr_unpack_uint32(bcm_xdr_buf_t *b, uint32 *pval)
{
	if (b->size < 4)
		return -1;

	*pval = ltoh32(*(uint32*)(b->buf));

	b->size -= 4;
	b->buf += 4;

	return 0;
}

int
bcm_xdr_pack_int32(bcm_xdr_buf_t *b, int32 val)
{
	return bcm_xdr_pack_uint32(b, (uint32)val);
}

int
bcm_xdr_unpack_int32(bcm_xdr_buf_t *b, int32 *pval)
{
	return bcm_xdr_unpack_uint32(b, (uint32*)pval);
}

int
bcm_xdr_pack_int8(bcm_xdr_buf_t *b, int8 val)
{
	return bcm_xdr_pack_uint32(b, (uint32)val);
}

int
bcm_xdr_unpack_int8(bcm_xdr_buf_t *b, int8 *pval)
{
	uint32 val = 0;
	int err;

	err = bcm_xdr_unpack_uint32(b, &val);
	*pval = (int8)val;

	return err;
}

int
bcm_xdr_pack_opaque_raw(bcm_xdr_buf_t *b, uint len, void *data)
{
	if (b->size < len)
		return -1;
	memcpy(b->buf, data, len);

	b->size -= len;
	b->buf += len;

	return 0;
}

/* Pad 0 at the remaining part of a rpc buffer */
int
bcm_xdr_pack_opaque_pad(bcm_xdr_buf_t *b)
{
	size_t len;
	size_t r;

	/* calc residual padding to 4 bytes */
	len = (size_t)(b->buf - b->origbuf);
	r = (4 - len) & 0x3;

	if (r == 0)
		return 0;

	if (b->size < r)
		return -1;

	memset(b->buf, 0, r);

	b->size -= r;
	b->buf += r;

	return 0;
}

/* pack a word-aligned buffer without dealing with the endianess, pad 0 at the end and make
 * it word-aligned if len is not multiple of 4
 */
int
bcm_xdr_pack_opaque(bcm_xdr_buf_t *b, uint len, const void *data)
{
	size_t tot_len;
	size_t r;

	/* calc residual padding to 4 bytes */
	r = (4 - len) & 3;

	tot_len = len + r;

	if (b->size < tot_len)
		return -1;

	memcpy(b->buf, data, len);
	memset(b->buf + len, 0, r);

	b->size -= tot_len;
	b->buf += tot_len;

	return 0;
}

int
bcm_xdr_unpack_opaque(bcm_xdr_buf_t *b, uint len, void **pdata)
{
	size_t tot_len;
	size_t r;

	/* calc residual padding to 4 bytes */
	r = (4 - len) & 3;
	tot_len = len + r;

	if (b->size < tot_len)
		return -1;

	*pdata = b->buf;

	b->size -= tot_len;
	b->buf += tot_len;

	return 0;
}

int
bcm_xdr_unpack_opaque_cpy(bcm_xdr_buf_t *b, uint len, void *data)
{
	void *xdr_data;
	int err;

	err = bcm_xdr_unpack_opaque(b, len, &xdr_data);
	if (err)
		return err;

	memcpy(data, xdr_data, len);

	return 0;
}

int
bcm_xdr_pack_opaque_varlen(bcm_xdr_buf_t *b, uint len, const void *data)
{
	int err;

	err = bcm_xdr_pack_uint32(b, len);
	if (err)
		return err;

	return bcm_xdr_pack_opaque(b, len, data);
}

int
bcm_xdr_unpack_opaque_varlen(bcm_xdr_buf_t *b, uint *plen, void **pdata)
{
	int err;

	err = bcm_xdr_unpack_uint32(b, plen);
	if (err)
		return err;

	return bcm_xdr_unpack_opaque(b, *plen, pdata);
}

int
bcm_xdr_pack_string(bcm_xdr_buf_t *b, char *str)
{
	return bcm_xdr_pack_opaque_varlen(b, strlen(str), str);
}

int
bcm_xdr_unpack_string(bcm_xdr_buf_t *b, uint *plen, char **pstr)
{
	return bcm_xdr_unpack_opaque_varlen(b, plen, (void**)pstr);
}

int
bcm_xdr_opaque_resrv(bcm_xdr_buf_t *b, uint len, void **pdata)
{
	return bcm_xdr_unpack_opaque(b, len, pdata);
}

int
bcm_xdr_opaque_resrv_varlen(bcm_xdr_buf_t *b, uint len, void **pdata)
{
	int err;
	err =  bcm_xdr_pack_uint32(b, len);

	if (err)
		return err;

	return bcm_xdr_opaque_resrv(b, len, pdata);
}
