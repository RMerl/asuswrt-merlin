/*
 *  linux/include/linux/sunrpc/gss_spkm3.h
 *
 *  Copyright (c) 2000 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Andy Adamson   <andros@umich.edu>
 */

#include <linux/sunrpc/auth_gss.h>
#include <linux/sunrpc/gss_err.h>
#include <linux/sunrpc/gss_asn1.h>

struct spkm3_ctx {
	struct xdr_netobj	ctx_id;  /* per message context id */
	int			endtime; /* endtime of the context */
	struct xdr_netobj	mech_used;
	unsigned int		ret_flags ;
	struct xdr_netobj	conf_alg;
	struct xdr_netobj	derived_conf_key;
	struct xdr_netobj	intg_alg;
	struct xdr_netobj 	derived_integ_key;
};

/* OIDs declarations for K-ALG, I-ALG, C-ALG, and OWF-ALG */
extern const struct xdr_netobj hmac_md5_oid;
extern const struct xdr_netobj cast5_cbc_oid;

/* SPKM InnerContext Token types */

#define SPKM_ERROR_TOK	3
#define SPKM_MIC_TOK	4
#define SPKM_WRAP_TOK	5
#define SPKM_DEL_TOK	6

u32 spkm3_make_token(struct spkm3_ctx *ctx, struct xdr_buf * text, struct xdr_netobj * token, int toktype);

u32 spkm3_read_token(struct spkm3_ctx *ctx, struct xdr_netobj *read_token, struct xdr_buf *message_buffer, int toktype);

#define CKSUMTYPE_RSA_MD5            0x0007
#define CKSUMTYPE_HMAC_MD5           0x0008

s32 make_spkm3_checksum(s32 cksumtype, struct xdr_netobj *key, char *header,
		unsigned int hdrlen, struct xdr_buf *body,
		unsigned int body_offset, struct xdr_netobj *cksum);
void asn1_bitstring_len(struct xdr_netobj *in, int *enclen, int *zerobits);
int decode_asn1_bitstring(struct xdr_netobj *out, char *in, int enclen,
                   int explen);
void spkm3_mic_header(unsigned char **hdrbuf, unsigned int *hdrlen, 
                   unsigned char *ctxhdr, int elen, int zbit);
void spkm3_make_mic_token(unsigned  char **tokp, int toklen, 
                   struct xdr_netobj *mic_hdr,
                   struct xdr_netobj *md5cksum, int md5elen, int md5zbit);
u32 spkm3_verify_mic_token(unsigned char **tokp, int *mic_hdrlen, 
                   unsigned char **cksum);
