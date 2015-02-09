/*
 *  linux/net/sunrpc/gss_krb5_crypto.c
 *
 *  Copyright (c) 2000-2008 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Andy Adamson   <andros@umich.edu>
 *  Bruce Fields   <bfields@umich.edu>
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/err.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/random.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/sunrpc/xdr.h>

#ifdef RPC_DEBUG
# define RPCDBG_FACILITY        RPCDBG_AUTH
#endif

u32
krb5_encrypt(
	struct crypto_blkcipher *tfm,
	void * iv,
	void * in,
	void * out,
	int length)
{
	u32 ret = -EINVAL;
	struct scatterlist sg[1];
	u8 local_iv[GSS_KRB5_MAX_BLOCKSIZE] = {0};
	struct blkcipher_desc desc = { .tfm = tfm, .info = local_iv };

	if (length % crypto_blkcipher_blocksize(tfm) != 0)
		goto out;

	if (crypto_blkcipher_ivsize(tfm) > GSS_KRB5_MAX_BLOCKSIZE) {
		dprintk("RPC:       gss_k5encrypt: tfm iv size too large %d\n",
			crypto_blkcipher_ivsize(tfm));
		goto out;
	}

	if (iv)
		memcpy(local_iv, iv, crypto_blkcipher_ivsize(tfm));

	memcpy(out, in, length);
	sg_init_one(sg, out, length);

	ret = crypto_blkcipher_encrypt_iv(&desc, sg, sg, length);
out:
	dprintk("RPC:       krb5_encrypt returns %d\n", ret);
	return ret;
}

u32
krb5_decrypt(
     struct crypto_blkcipher *tfm,
     void * iv,
     void * in,
     void * out,
     int length)
{
	u32 ret = -EINVAL;
	struct scatterlist sg[1];
	u8 local_iv[GSS_KRB5_MAX_BLOCKSIZE] = {0};
	struct blkcipher_desc desc = { .tfm = tfm, .info = local_iv };

	if (length % crypto_blkcipher_blocksize(tfm) != 0)
		goto out;

	if (crypto_blkcipher_ivsize(tfm) > GSS_KRB5_MAX_BLOCKSIZE) {
		dprintk("RPC:       gss_k5decrypt: tfm iv size too large %d\n",
			crypto_blkcipher_ivsize(tfm));
		goto out;
	}
	if (iv)
		memcpy(local_iv,iv, crypto_blkcipher_ivsize(tfm));

	memcpy(out, in, length);
	sg_init_one(sg, out, length);

	ret = crypto_blkcipher_decrypt_iv(&desc, sg, sg, length);
out:
	dprintk("RPC:       gss_k5decrypt returns %d\n",ret);
	return ret;
}

static int
checksummer(struct scatterlist *sg, void *data)
{
	struct hash_desc *desc = data;

	return crypto_hash_update(desc, sg, sg->length);
}

static int
arcfour_hmac_md5_usage_to_salt(unsigned int usage, u8 salt[4])
{
	unsigned int ms_usage;

	switch (usage) {
	case KG_USAGE_SIGN:
		ms_usage = 15;
		break;
	case KG_USAGE_SEAL:
		ms_usage = 13;
		break;
	default:
		return EINVAL;;
	}
	salt[0] = (ms_usage >> 0) & 0xff;
	salt[1] = (ms_usage >> 8) & 0xff;
	salt[2] = (ms_usage >> 16) & 0xff;
	salt[3] = (ms_usage >> 24) & 0xff;

	return 0;
}

static u32
make_checksum_hmac_md5(struct krb5_ctx *kctx, char *header, int hdrlen,
		       struct xdr_buf *body, int body_offset, u8 *cksumkey,
		       unsigned int usage, struct xdr_netobj *cksumout)
{
	struct hash_desc                desc;
	struct scatterlist              sg[1];
	int err;
	u8 checksumdata[GSS_KRB5_MAX_CKSUM_LEN];
	u8 rc4salt[4];
	struct crypto_hash *md5;
	struct crypto_hash *hmac_md5;

	if (cksumkey == NULL)
		return GSS_S_FAILURE;

	if (cksumout->len < kctx->gk5e->cksumlength) {
		dprintk("%s: checksum buffer length, %u, too small for %s\n",
			__func__, cksumout->len, kctx->gk5e->name);
		return GSS_S_FAILURE;
	}

	if (arcfour_hmac_md5_usage_to_salt(usage, rc4salt)) {
		dprintk("%s: invalid usage value %u\n", __func__, usage);
		return GSS_S_FAILURE;
	}

	md5 = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(md5))
		return GSS_S_FAILURE;

	hmac_md5 = crypto_alloc_hash(kctx->gk5e->cksum_name, 0,
				     CRYPTO_ALG_ASYNC);
	if (IS_ERR(hmac_md5)) {
		crypto_free_hash(md5);
		return GSS_S_FAILURE;
	}

	desc.tfm = md5;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_hash_init(&desc);
	if (err)
		goto out;
	sg_init_one(sg, rc4salt, 4);
	err = crypto_hash_update(&desc, sg, 4);
	if (err)
		goto out;

	sg_init_one(sg, header, hdrlen);
	err = crypto_hash_update(&desc, sg, hdrlen);
	if (err)
		goto out;
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, &desc);
	if (err)
		goto out;
	err = crypto_hash_final(&desc, checksumdata);
	if (err)
		goto out;

	desc.tfm = hmac_md5;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_hash_init(&desc);
	if (err)
		goto out;
	err = crypto_hash_setkey(hmac_md5, cksumkey, kctx->gk5e->keylength);
	if (err)
		goto out;

	sg_init_one(sg, checksumdata, crypto_hash_digestsize(md5));
	err = crypto_hash_digest(&desc, sg, crypto_hash_digestsize(md5),
				 checksumdata);
	if (err)
		goto out;

	memcpy(cksumout->data, checksumdata, kctx->gk5e->cksumlength);
	cksumout->len = kctx->gk5e->cksumlength;
out:
	crypto_free_hash(md5);
	crypto_free_hash(hmac_md5);
	return err ? GSS_S_FAILURE : 0;
}

/*
 * checksum the plaintext data and hdrlen bytes of the token header
 * The checksum is performed over the first 8 bytes of the
 * gss token header and then over the data body
 */
u32
make_checksum(struct krb5_ctx *kctx, char *header, int hdrlen,
	      struct xdr_buf *body, int body_offset, u8 *cksumkey,
	      unsigned int usage, struct xdr_netobj *cksumout)
{
	struct hash_desc                desc;
	struct scatterlist              sg[1];
	int err;
	u8 checksumdata[GSS_KRB5_MAX_CKSUM_LEN];
	unsigned int checksumlen;

	if (kctx->gk5e->ctype == CKSUMTYPE_HMAC_MD5_ARCFOUR)
		return make_checksum_hmac_md5(kctx, header, hdrlen,
					      body, body_offset,
					      cksumkey, usage, cksumout);

	if (cksumout->len < kctx->gk5e->cksumlength) {
		dprintk("%s: checksum buffer length, %u, too small for %s\n",
			__func__, cksumout->len, kctx->gk5e->name);
		return GSS_S_FAILURE;
	}

	desc.tfm = crypto_alloc_hash(kctx->gk5e->cksum_name, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm))
		return GSS_S_FAILURE;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	checksumlen = crypto_hash_digestsize(desc.tfm);

	if (cksumkey != NULL) {
		err = crypto_hash_setkey(desc.tfm, cksumkey,
					 kctx->gk5e->keylength);
		if (err)
			goto out;
	}

	err = crypto_hash_init(&desc);
	if (err)
		goto out;
	sg_init_one(sg, header, hdrlen);
	err = crypto_hash_update(&desc, sg, hdrlen);
	if (err)
		goto out;
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, &desc);
	if (err)
		goto out;
	err = crypto_hash_final(&desc, checksumdata);
	if (err)
		goto out;

	switch (kctx->gk5e->ctype) {
	case CKSUMTYPE_RSA_MD5:
		err = kctx->gk5e->encrypt(kctx->seq, NULL, checksumdata,
					  checksumdata, checksumlen);
		if (err)
			goto out;
		memcpy(cksumout->data,
		       checksumdata + checksumlen - kctx->gk5e->cksumlength,
		       kctx->gk5e->cksumlength);
		break;
	case CKSUMTYPE_HMAC_SHA1_DES3:
		memcpy(cksumout->data, checksumdata, kctx->gk5e->cksumlength);
		break;
	default:
		BUG();
		break;
	}
	cksumout->len = kctx->gk5e->cksumlength;
out:
	crypto_free_hash(desc.tfm);
	return err ? GSS_S_FAILURE : 0;
}

/*
 * checksum the plaintext data and hdrlen bytes of the token header
 * Per rfc4121, sec. 4.2.4, the checksum is performed over the data
 * body then over the first 16 octets of the MIC token
 * Inclusion of the header data in the calculation of the
 * checksum is optional.
 */
u32
make_checksum_v2(struct krb5_ctx *kctx, char *header, int hdrlen,
		 struct xdr_buf *body, int body_offset, u8 *cksumkey,
		 unsigned int usage, struct xdr_netobj *cksumout)
{
	struct hash_desc desc;
	struct scatterlist sg[1];
	int err;
	u8 checksumdata[GSS_KRB5_MAX_CKSUM_LEN];
	unsigned int checksumlen;

	if (kctx->gk5e->keyed_cksum == 0) {
		dprintk("%s: expected keyed hash for %s\n",
			__func__, kctx->gk5e->name);
		return GSS_S_FAILURE;
	}
	if (cksumkey == NULL) {
		dprintk("%s: no key supplied for %s\n",
			__func__, kctx->gk5e->name);
		return GSS_S_FAILURE;
	}

	desc.tfm = crypto_alloc_hash(kctx->gk5e->cksum_name, 0,
							CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm))
		return GSS_S_FAILURE;
	checksumlen = crypto_hash_digestsize(desc.tfm);
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_hash_setkey(desc.tfm, cksumkey, kctx->gk5e->keylength);
	if (err)
		goto out;

	err = crypto_hash_init(&desc);
	if (err)
		goto out;
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, &desc);
	if (err)
		goto out;
	if (header != NULL) {
		sg_init_one(sg, header, hdrlen);
		err = crypto_hash_update(&desc, sg, hdrlen);
		if (err)
			goto out;
	}
	err = crypto_hash_final(&desc, checksumdata);
	if (err)
		goto out;

	cksumout->len = kctx->gk5e->cksumlength;

	switch (kctx->gk5e->ctype) {
	case CKSUMTYPE_HMAC_SHA1_96_AES128:
	case CKSUMTYPE_HMAC_SHA1_96_AES256:
		/* note that this truncates the hash */
		memcpy(cksumout->data, checksumdata, kctx->gk5e->cksumlength);
		break;
	default:
		BUG();
		break;
	}
out:
	crypto_free_hash(desc.tfm);
	return err ? GSS_S_FAILURE : 0;
}

struct encryptor_desc {
	u8 iv[GSS_KRB5_MAX_BLOCKSIZE];
	struct blkcipher_desc desc;
	int pos;
	struct xdr_buf *outbuf;
	struct page **pages;
	struct scatterlist infrags[4];
	struct scatterlist outfrags[4];
	int fragno;
	int fraglen;
};

static int
encryptor(struct scatterlist *sg, void *data)
{
	struct encryptor_desc *desc = data;
	struct xdr_buf *outbuf = desc->outbuf;
	struct page *in_page;
	int thislen = desc->fraglen + sg->length;
	int fraglen, ret;
	int page_pos;

	/* Worst case is 4 fragments: head, end of page 1, start
	 * of page 2, tail.  Anything more is a bug. */
	BUG_ON(desc->fragno > 3);

	page_pos = desc->pos - outbuf->head[0].iov_len;
	if (page_pos >= 0 && page_pos < outbuf->page_len) {
		/* pages are not in place: */
		int i = (page_pos + outbuf->page_base) >> PAGE_CACHE_SHIFT;
		in_page = desc->pages[i];
	} else {
		in_page = sg_page(sg);
	}
	sg_set_page(&desc->infrags[desc->fragno], in_page, sg->length,
		    sg->offset);
	sg_set_page(&desc->outfrags[desc->fragno], sg_page(sg), sg->length,
		    sg->offset);
	desc->fragno++;
	desc->fraglen += sg->length;
	desc->pos += sg->length;

	fraglen = thislen & (crypto_blkcipher_blocksize(desc->desc.tfm) - 1);
	thislen -= fraglen;

	if (thislen == 0)
		return 0;

	sg_mark_end(&desc->infrags[desc->fragno - 1]);
	sg_mark_end(&desc->outfrags[desc->fragno - 1]);

	ret = crypto_blkcipher_encrypt_iv(&desc->desc, desc->outfrags,
					  desc->infrags, thislen);
	if (ret)
		return ret;

	sg_init_table(desc->infrags, 4);
	sg_init_table(desc->outfrags, 4);

	if (fraglen) {
		sg_set_page(&desc->outfrags[0], sg_page(sg), fraglen,
				sg->offset + sg->length - fraglen);
		desc->infrags[0] = desc->outfrags[0];
		sg_assign_page(&desc->infrags[0], in_page);
		desc->fragno = 1;
		desc->fraglen = fraglen;
	} else {
		desc->fragno = 0;
		desc->fraglen = 0;
	}
	return 0;
}

int
gss_encrypt_xdr_buf(struct crypto_blkcipher *tfm, struct xdr_buf *buf,
		    int offset, struct page **pages)
{
	int ret;
	struct encryptor_desc desc;

	BUG_ON((buf->len - offset) % crypto_blkcipher_blocksize(tfm) != 0);

	memset(desc.iv, 0, sizeof(desc.iv));
	desc.desc.tfm = tfm;
	desc.desc.info = desc.iv;
	desc.desc.flags = 0;
	desc.pos = offset;
	desc.outbuf = buf;
	desc.pages = pages;
	desc.fragno = 0;
	desc.fraglen = 0;

	sg_init_table(desc.infrags, 4);
	sg_init_table(desc.outfrags, 4);

	ret = xdr_process_buf(buf, offset, buf->len - offset, encryptor, &desc);
	return ret;
}

struct decryptor_desc {
	u8 iv[GSS_KRB5_MAX_BLOCKSIZE];
	struct blkcipher_desc desc;
	struct scatterlist frags[4];
	int fragno;
	int fraglen;
};

static int
decryptor(struct scatterlist *sg, void *data)
{
	struct decryptor_desc *desc = data;
	int thislen = desc->fraglen + sg->length;
	int fraglen, ret;

	/* Worst case is 4 fragments: head, end of page 1, start
	 * of page 2, tail.  Anything more is a bug. */
	BUG_ON(desc->fragno > 3);
	sg_set_page(&desc->frags[desc->fragno], sg_page(sg), sg->length,
		    sg->offset);
	desc->fragno++;
	desc->fraglen += sg->length;

	fraglen = thislen & (crypto_blkcipher_blocksize(desc->desc.tfm) - 1);
	thislen -= fraglen;

	if (thislen == 0)
		return 0;

	sg_mark_end(&desc->frags[desc->fragno - 1]);

	ret = crypto_blkcipher_decrypt_iv(&desc->desc, desc->frags,
					  desc->frags, thislen);
	if (ret)
		return ret;

	sg_init_table(desc->frags, 4);

	if (fraglen) {
		sg_set_page(&desc->frags[0], sg_page(sg), fraglen,
				sg->offset + sg->length - fraglen);
		desc->fragno = 1;
		desc->fraglen = fraglen;
	} else {
		desc->fragno = 0;
		desc->fraglen = 0;
	}
	return 0;
}

int
gss_decrypt_xdr_buf(struct crypto_blkcipher *tfm, struct xdr_buf *buf,
		    int offset)
{
	struct decryptor_desc desc;

	/* XXXJBF: */
	BUG_ON((buf->len - offset) % crypto_blkcipher_blocksize(tfm) != 0);

	memset(desc.iv, 0, sizeof(desc.iv));
	desc.desc.tfm = tfm;
	desc.desc.info = desc.iv;
	desc.desc.flags = 0;
	desc.fragno = 0;
	desc.fraglen = 0;

	sg_init_table(desc.frags, 4);

	return xdr_process_buf(buf, offset, buf->len - offset, decryptor, &desc);
}

/*
 * This function makes the assumption that it was ultimately called
 * from gss_wrap().
 *
 * The client auth_gss code moves any existing tail data into a
 * separate page before calling gss_wrap.
 * The server svcauth_gss code ensures that both the head and the
 * tail have slack space of RPC_MAX_AUTH_SIZE before calling gss_wrap.
 *
 * Even with that guarantee, this function may be called more than
 * once in the processing of gss_wrap().  The best we can do is
 * verify at compile-time (see GSS_KRB5_SLACK_CHECK) that the
 * largest expected shift will fit within RPC_MAX_AUTH_SIZE.
 * At run-time we can verify that a single invocation of this
 * function doesn't attempt to use more the RPC_MAX_AUTH_SIZE.
 */

int
xdr_extend_head(struct xdr_buf *buf, unsigned int base, unsigned int shiftlen)
{
	u8 *p;

	if (shiftlen == 0)
		return 0;

	BUILD_BUG_ON(GSS_KRB5_MAX_SLACK_NEEDED > RPC_MAX_AUTH_SIZE);
	BUG_ON(shiftlen > RPC_MAX_AUTH_SIZE);

	p = buf->head[0].iov_base + base;

	memmove(p + shiftlen, p, buf->head[0].iov_len - base);

	buf->head[0].iov_len += shiftlen;
	buf->len += shiftlen;

	return 0;
}

static u32
gss_krb5_cts_crypt(struct crypto_blkcipher *cipher, struct xdr_buf *buf,
		   u32 offset, u8 *iv, struct page **pages, int encrypt)
{
	u32 ret;
	struct scatterlist sg[1];
	struct blkcipher_desc desc = { .tfm = cipher, .info = iv };
	u8 data[crypto_blkcipher_blocksize(cipher) * 2];
	struct page **save_pages;
	u32 len = buf->len - offset;

	BUG_ON(len > crypto_blkcipher_blocksize(cipher) * 2);

	/*
	 * For encryption, we want to read from the cleartext
	 * page cache pages, and write the encrypted data to
	 * the supplied xdr_buf pages.
	 */
	save_pages = buf->pages;
	if (encrypt)
		buf->pages = pages;

	ret = read_bytes_from_xdr_buf(buf, offset, data, len);
	buf->pages = save_pages;
	if (ret)
		goto out;

	sg_init_one(sg, data, len);

	if (encrypt)
		ret = crypto_blkcipher_encrypt_iv(&desc, sg, sg, len);
	else
		ret = crypto_blkcipher_decrypt_iv(&desc, sg, sg, len);

	if (ret)
		goto out;

	ret = write_bytes_to_xdr_buf(buf, offset, data, len);

out:
	return ret;
}

u32
gss_krb5_aes_encrypt(struct krb5_ctx *kctx, u32 offset,
		     struct xdr_buf *buf, int ec, struct page **pages)
{
	u32 err;
	struct xdr_netobj hmac;
	u8 *cksumkey;
	u8 *ecptr;
	struct crypto_blkcipher *cipher, *aux_cipher;
	int blocksize;
	struct page **save_pages;
	int nblocks, nbytes;
	struct encryptor_desc desc;
	u32 cbcbytes;
	unsigned int usage;

	if (kctx->initiate) {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		cksumkey = kctx->initiator_integ;
		usage = KG_USAGE_INITIATOR_SEAL;
	} else {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		cksumkey = kctx->acceptor_integ;
		usage = KG_USAGE_ACCEPTOR_SEAL;
	}
	blocksize = crypto_blkcipher_blocksize(cipher);

	/* hide the gss token header and insert the confounder */
	offset += GSS_KRB5_TOK_HDR_LEN;
	if (xdr_extend_head(buf, offset, kctx->gk5e->conflen))
		return GSS_S_FAILURE;
	gss_krb5_make_confounder(buf->head[0].iov_base + offset, kctx->gk5e->conflen);
	offset -= GSS_KRB5_TOK_HDR_LEN;

	if (buf->tail[0].iov_base != NULL) {
		ecptr = buf->tail[0].iov_base + buf->tail[0].iov_len;
	} else {
		buf->tail[0].iov_base = buf->head[0].iov_base
							+ buf->head[0].iov_len;
		buf->tail[0].iov_len = 0;
		ecptr = buf->tail[0].iov_base;
	}

	memset(ecptr, 'X', ec);
	buf->tail[0].iov_len += ec;
	buf->len += ec;

	/* copy plaintext gss token header after filler (if any) */
	memcpy(ecptr + ec, buf->head[0].iov_base + offset,
						GSS_KRB5_TOK_HDR_LEN);
	buf->tail[0].iov_len += GSS_KRB5_TOK_HDR_LEN;
	buf->len += GSS_KRB5_TOK_HDR_LEN;

	/* Do the HMAC */
	hmac.len = GSS_KRB5_MAX_CKSUM_LEN;
	hmac.data = buf->tail[0].iov_base + buf->tail[0].iov_len;

	/*
	 * When we are called, pages points to the real page cache
	 * data -- which we can't go and encrypt!  buf->pages points
	 * to scratch pages which we are going to send off to the
	 * client/server.  Swap in the plaintext pages to calculate
	 * the hmac.
	 */
	save_pages = buf->pages;
	buf->pages = pages;

	err = make_checksum_v2(kctx, NULL, 0, buf,
			       offset + GSS_KRB5_TOK_HDR_LEN,
			       cksumkey, usage, &hmac);
	buf->pages = save_pages;
	if (err)
		return GSS_S_FAILURE;

	nbytes = buf->len - offset - GSS_KRB5_TOK_HDR_LEN;
	nblocks = (nbytes + blocksize - 1) / blocksize;
	cbcbytes = 0;
	if (nblocks > 2)
		cbcbytes = (nblocks - 2) * blocksize;

	memset(desc.iv, 0, sizeof(desc.iv));

	if (cbcbytes) {
		desc.pos = offset + GSS_KRB5_TOK_HDR_LEN;
		desc.fragno = 0;
		desc.fraglen = 0;
		desc.pages = pages;
		desc.outbuf = buf;
		desc.desc.info = desc.iv;
		desc.desc.flags = 0;
		desc.desc.tfm = aux_cipher;

		sg_init_table(desc.infrags, 4);
		sg_init_table(desc.outfrags, 4);

		err = xdr_process_buf(buf, offset + GSS_KRB5_TOK_HDR_LEN,
				      cbcbytes, encryptor, &desc);
		if (err)
			goto out_err;
	}

	/* Make sure IV carries forward from any CBC results. */
	err = gss_krb5_cts_crypt(cipher, buf,
				 offset + GSS_KRB5_TOK_HDR_LEN + cbcbytes,
				 desc.iv, pages, 1);
	if (err) {
		err = GSS_S_FAILURE;
		goto out_err;
	}

	/* Now update buf to account for HMAC */
	buf->tail[0].iov_len += kctx->gk5e->cksumlength;
	buf->len += kctx->gk5e->cksumlength;

out_err:
	if (err)
		err = GSS_S_FAILURE;
	return err;
}

u32
gss_krb5_aes_decrypt(struct krb5_ctx *kctx, u32 offset, struct xdr_buf *buf,
		     u32 *headskip, u32 *tailskip)
{
	struct xdr_buf subbuf;
	u32 ret = 0;
	u8 *cksum_key;
	struct crypto_blkcipher *cipher, *aux_cipher;
	struct xdr_netobj our_hmac_obj;
	u8 our_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	u8 pkt_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	int nblocks, blocksize, cbcbytes;
	struct decryptor_desc desc;
	unsigned int usage;

	if (kctx->initiate) {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		cksum_key = kctx->acceptor_integ;
		usage = KG_USAGE_ACCEPTOR_SEAL;
	} else {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		cksum_key = kctx->initiator_integ;
		usage = KG_USAGE_INITIATOR_SEAL;
	}
	blocksize = crypto_blkcipher_blocksize(cipher);


	/* create a segment skipping the header and leaving out the checksum */
	xdr_buf_subsegment(buf, &subbuf, offset + GSS_KRB5_TOK_HDR_LEN,
				    (buf->len - offset - GSS_KRB5_TOK_HDR_LEN -
				     kctx->gk5e->cksumlength));

	nblocks = (subbuf.len + blocksize - 1) / blocksize;

	cbcbytes = 0;
	if (nblocks > 2)
		cbcbytes = (nblocks - 2) * blocksize;

	memset(desc.iv, 0, sizeof(desc.iv));

	if (cbcbytes) {
		desc.fragno = 0;
		desc.fraglen = 0;
		desc.desc.info = desc.iv;
		desc.desc.flags = 0;
		desc.desc.tfm = aux_cipher;

		sg_init_table(desc.frags, 4);

		ret = xdr_process_buf(&subbuf, 0, cbcbytes, decryptor, &desc);
		if (ret)
			goto out_err;
	}

	/* Make sure IV carries forward from any CBC results. */
	ret = gss_krb5_cts_crypt(cipher, &subbuf, cbcbytes, desc.iv, NULL, 0);
	if (ret)
		goto out_err;


	/* Calculate our hmac over the plaintext data */
	our_hmac_obj.len = sizeof(our_hmac);
	our_hmac_obj.data = our_hmac;

	ret = make_checksum_v2(kctx, NULL, 0, &subbuf, 0,
			       cksum_key, usage, &our_hmac_obj);
	if (ret)
		goto out_err;

	/* Get the packet's hmac value */
	ret = read_bytes_from_xdr_buf(buf, buf->len - kctx->gk5e->cksumlength,
				      pkt_hmac, kctx->gk5e->cksumlength);
	if (ret)
		goto out_err;

	if (memcmp(pkt_hmac, our_hmac, kctx->gk5e->cksumlength) != 0) {
		ret = GSS_S_BAD_SIG;
		goto out_err;
	}
	*headskip = kctx->gk5e->conflen;
	*tailskip = kctx->gk5e->cksumlength;
out_err:
	if (ret && ret != GSS_S_BAD_SIG)
		ret = GSS_S_FAILURE;
	return ret;
}

/*
 * Compute Kseq given the initial session key and the checksum.
 * Set the key of the given cipher.
 */
int
krb5_rc4_setup_seq_key(struct krb5_ctx *kctx, struct crypto_blkcipher *cipher,
		       unsigned char *cksum)
{
	struct crypto_hash *hmac;
	struct hash_desc desc;
	struct scatterlist sg[1];
	u8 Kseq[GSS_KRB5_MAX_KEYLEN];
	u32 zeroconstant = 0;
	int err;

	dprintk("%s: entered\n", __func__);

	hmac = crypto_alloc_hash(kctx->gk5e->cksum_name, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(hmac)) {
		dprintk("%s: error %ld, allocating hash '%s'\n",
			__func__, PTR_ERR(hmac), kctx->gk5e->cksum_name);
		return PTR_ERR(hmac);
	}

	desc.tfm = hmac;
	desc.flags = 0;

	err = crypto_hash_init(&desc);
	if (err)
		goto out_err;

	/* Compute intermediate Kseq from session key */
	err = crypto_hash_setkey(hmac, kctx->Ksess, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	sg_init_table(sg, 1);
	sg_set_buf(sg, &zeroconstant, 4);

	err = crypto_hash_digest(&desc, sg, 4, Kseq);
	if (err)
		goto out_err;

	/* Compute final Kseq from the checksum and intermediate Kseq */
	err = crypto_hash_setkey(hmac, Kseq, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	sg_set_buf(sg, cksum, 8);

	err = crypto_hash_digest(&desc, sg, 8, Kseq);
	if (err)
		goto out_err;

	err = crypto_blkcipher_setkey(cipher, Kseq, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	err = 0;

out_err:
	crypto_free_hash(hmac);
	dprintk("%s: returning %d\n", __func__, err);
	return err;
}

/*
 * Compute Kcrypt given the initial session key and the plaintext seqnum.
 * Set the key of cipher kctx->enc.
 */
int
krb5_rc4_setup_enc_key(struct krb5_ctx *kctx, struct crypto_blkcipher *cipher,
		       s32 seqnum)
{
	struct crypto_hash *hmac;
	struct hash_desc desc;
	struct scatterlist sg[1];
	u8 Kcrypt[GSS_KRB5_MAX_KEYLEN];
	u8 zeroconstant[4] = {0};
	u8 seqnumarray[4];
	int err, i;

	dprintk("%s: entered, seqnum %u\n", __func__, seqnum);

	hmac = crypto_alloc_hash(kctx->gk5e->cksum_name, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(hmac)) {
		dprintk("%s: error %ld, allocating hash '%s'\n",
			__func__, PTR_ERR(hmac), kctx->gk5e->cksum_name);
		return PTR_ERR(hmac);
	}

	desc.tfm = hmac;
	desc.flags = 0;

	err = crypto_hash_init(&desc);
	if (err)
		goto out_err;

	/* Compute intermediate Kcrypt from session key */
	for (i = 0; i < kctx->gk5e->keylength; i++)
		Kcrypt[i] = kctx->Ksess[i] ^ 0xf0;

	err = crypto_hash_setkey(hmac, Kcrypt, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	sg_init_table(sg, 1);
	sg_set_buf(sg, zeroconstant, 4);

	err = crypto_hash_digest(&desc, sg, 4, Kcrypt);
	if (err)
		goto out_err;

	/* Compute final Kcrypt from the seqnum and intermediate Kcrypt */
	err = crypto_hash_setkey(hmac, Kcrypt, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	seqnumarray[0] = (unsigned char) ((seqnum >> 24) & 0xff);
	seqnumarray[1] = (unsigned char) ((seqnum >> 16) & 0xff);
	seqnumarray[2] = (unsigned char) ((seqnum >> 8) & 0xff);
	seqnumarray[3] = (unsigned char) ((seqnum >> 0) & 0xff);

	sg_set_buf(sg, seqnumarray, 4);

	err = crypto_hash_digest(&desc, sg, 4, Kcrypt);
	if (err)
		goto out_err;

	err = crypto_blkcipher_setkey(cipher, Kcrypt, kctx->gk5e->keylength);
	if (err)
		goto out_err;

	err = 0;

out_err:
	crypto_free_hash(hmac);
	dprintk("%s: returning %d\n", __func__, err);
	return err;
}
