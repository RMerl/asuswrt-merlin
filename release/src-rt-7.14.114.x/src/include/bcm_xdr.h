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
 * $Id: bcm_xdr.h 319672 2012-03-08 22:02:53Z $
 */

#ifndef _BCM_XDR_H
#define _BCM_XDR_H

#define XDR_PACK_OPAQUE_VAR_SZ(len) (sizeof(uint32) + ROUNDUP(len, sizeof(uint32)))

/*
 * bcm_xdr_buf_t
 * Structure used for bookkeeping of a buffer being packed or unpacked.
 * Keeps a current read/write pointer and size as well as
 * the original buffer pointer and size.
 *
 */
typedef struct {
	uint8 *buf;	/* pointer to current position in origbuf */
	uint size;	/* current (residual) size in bytes */
	uint8 *origbuf;	/* unmodified pointer to orignal buffer */
	uint origsize;	/* unmodified orignal buffer size in bytes */
} bcm_xdr_buf_t;


void bcm_xdr_buf_init(bcm_xdr_buf_t *b, void *buf, size_t len);

int bcm_xdr_pack_uint32(bcm_xdr_buf_t *b, uint32 val);
int bcm_xdr_unpack_uint32(bcm_xdr_buf_t *b, uint32 *pval);
int bcm_xdr_pack_int32(bcm_xdr_buf_t *b, int32 val);
int bcm_xdr_unpack_int32(bcm_xdr_buf_t *b, int32 *pval);
int bcm_xdr_pack_int8(bcm_xdr_buf_t *b, int8 val);
int bcm_xdr_unpack_int8(bcm_xdr_buf_t *b, int8 *pval);
int bcm_xdr_pack_opaque(bcm_xdr_buf_t *b, uint len, const void *data);
int bcm_xdr_unpack_opaque(bcm_xdr_buf_t *b, uint len, void **pdata);
int bcm_xdr_unpack_opaque_cpy(bcm_xdr_buf_t *b, uint len, void *data);
int bcm_xdr_pack_opaque_varlen(bcm_xdr_buf_t *b, uint len, const void *data);
int bcm_xdr_unpack_opaque_varlen(bcm_xdr_buf_t *b, uint *plen, void **pdata);
int bcm_xdr_pack_string(bcm_xdr_buf_t *b, char *str);
int bcm_xdr_unpack_string(bcm_xdr_buf_t *b, uint *plen, char **pstr);

int bcm_xdr_pack_uint8_vec(bcm_xdr_buf_t *, uint8 *vec, uint32 elems);
int bcm_xdr_unpack_uint8_vec(bcm_xdr_buf_t *, uint8 *vec, uint32 elems);
int bcm_xdr_pack_uint16_vec(bcm_xdr_buf_t *b, uint len, void *vec);
int bcm_xdr_unpack_uint16_vec(bcm_xdr_buf_t *b, uint len, void *vec);
int bcm_xdr_pack_uint32_vec(bcm_xdr_buf_t *b, uint len, void *vec);
int bcm_xdr_unpack_uint32_vec(bcm_xdr_buf_t *b, uint len, void *vec);

int bcm_xdr_pack_opaque_raw(bcm_xdr_buf_t *b, uint len, void *data);
int bcm_xdr_pack_opaque_pad(bcm_xdr_buf_t *b);

int bcm_xdr_opaque_resrv(bcm_xdr_buf_t *b, uint len, void **pdata);
int bcm_xdr_opaque_resrv_varlen(bcm_xdr_buf_t *b, uint len, void **pdata);
#endif /* _BCM_XDR_H */
