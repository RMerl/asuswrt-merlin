/*
  Copyright (c) 2004-2006 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#ifndef HAVE_LUCID_CONTEXT_SUPPORT
#ifdef HAVE_HEIMDAL

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <krb5.h>
#include <gssapi.h>	/* Must use the heimdal copy! */
#ifdef HAVE_COM_ERR_H
#include <com_err.h>
#endif
#include "err_util.h"
#include "gss_oids.h"
#include "write_bytes.h"

int write_heimdal_keyblock(char **p, char *end, krb5_keyblock *key)
{
	gss_buffer_desc tmp;
	int code = -1;

	if (WRITE_BYTES(p, end, key->keytype)) goto out_err;
	tmp.length = key->keyvalue.length;
	tmp.value = key->keyvalue.data;
	if (write_buffer(p, end, &tmp)) goto out_err;
	code = 0;
    out_err:
	return(code);
}

int write_heimdal_enc_key(char **p, char *end, gss_ctx_id_t ctx)
{
	krb5_keyblock enc_key, *key;
	krb5_context context;
	krb5_error_code ret;
	int i;
	char *skd, *dkd, *k5err = NULL;
	int code = -1;

	if ((ret = krb5_init_context(&context))) {
		k5err = gssd_k5_err_msg(NULL, ret);
		printerr(0, "ERROR: initializing krb5_context: %s\n", k5err);
		goto out_err;
	}

	if ((ret = krb5_auth_con_getlocalsubkey(context,
						ctx->auth_context, &key))){
		k5err = gssd_k5_err_msg(context, ret);
		printerr(0, "ERROR: getting auth_context key: %s\n", k5err);
		goto out_err_free_context;
	}

	memset(&enc_key, 0, sizeof(enc_key));
	enc_key.keytype = key->keytype;
	/* XXX current kernel code only handles des-cbc-raw  (4) */
	if (enc_key.keytype != 4) {
		printerr(1, "WARN: write_heimdal_enc_key: "
			    "overriding heimdal keytype (%d => %d)\n",
			 enc_key.keytype, 4);
		enc_key.keytype = 4;
	}
	enc_key.keyvalue.length = key->keyvalue.length;
	if ((enc_key.keyvalue.data =
				calloc(1, enc_key.keyvalue.length)) == NULL) {
		k5err = gssd_k5_err_msg(context, ENOMEM);
		printerr(0, "ERROR: allocating memory for enc key: %s\n",
			 k5err);
		goto out_err_free_key;
	}
	skd = (char *) key->keyvalue.data;
	dkd = (char *) enc_key.keyvalue.data;
	for (i = 0; i < enc_key.keyvalue.length; i++)
		dkd[i] = skd[i] ^ 0xf0;
	if (write_heimdal_keyblock(p, end, &enc_key)) {
		goto out_err_free_enckey;
	}

	code = 0;

    out_err_free_enckey:
	krb5_free_keyblock_contents(context, &enc_key);
    out_err_free_key:
	krb5_free_keyblock(context, key);
    out_err_free_context:
	krb5_free_context(context);
    out_err:
	free(k5err);
	printerr(2, "write_heimdal_enc_key: %s\n", code ? "FAILED" : "SUCCESS");
	return(code);
}

int write_heimdal_seq_key(char **p, char *end, gss_ctx_id_t ctx)
{
	krb5_keyblock *key;
	krb5_context context;
	krb5_error_code ret;
	char *k5err = NULL;
	int code = -1;

	if ((ret = krb5_init_context(&context))) {
		k5err = gssd_k5_err_msg(NULL, ret);
		printerr(0, "ERROR: initializing krb5_context: %s\n", k5err);
		goto out_err;
	}

	if ((ret = krb5_auth_con_getlocalsubkey(context,
						ctx->auth_context, &key))){
		k5err = gssd_k5_err_msg(context, ret);
		printerr(0, "ERROR: getting auth_context key: %s\n", k5err);
		goto out_err_free_context;
	}

	/* XXX current kernel code only handles des-cbc-raw  (4) */
	if (key->keytype != 4) {
		printerr(1, "WARN: write_heimdal_seq_key: "
			    "overriding heimdal keytype (%d => %d)\n",
			 key->keytype, 4);
		key->keytype = 4;
	}

	if (write_heimdal_keyblock(p, end, key)) {
		goto out_err_free_key;
	}

	code = 0;

    out_err_free_key:
	krb5_free_keyblock(context, key);
    out_err_free_context:
	krb5_free_context(context);
    out_err:
	free(k5err);
	printerr(2, "write_heimdal_seq_key: %s\n", code ? "FAILED" : "SUCCESS");
	return(code);
}

/*
 * The following is the kernel structure that we are filling in:
 *
 * struct krb5_ctx {
 *         int                     initiate;
 *         int                     seed_init;
 *         unsigned char           seed[16];
 *         int                     signalg;
 *         int                     sealalg;
 *         struct crypto_tfm       *enc;
 *         struct crypto_tfm       *seq;
 *         s32                     endtime;
 *         u32                     seq_send;
 *         struct xdr_netobj       mech_used;
 * };
 *
 * However, note that we do not send the data fields in the
 * order they appear in the structure.  The order they are
 * sent down in is:
 *
 *	initiate
 *	seed_init
 *	seed
 *	signalg
 *	sealalg
 *	endtime
 *	seq_send
 *	mech_used
 *	enc key
 *	seq key
 *
 */

int
serialize_krb5_ctx(gss_ctx_id_t ctx, gss_buffer_desc *buf, int32_t *endtime)
{

	char *p, *end;
	static int constant_one = 1;
	static int constant_zero = 0;
	unsigned char fakeseed[16];
	uint32_t algorithm;

	if (!(buf->value = calloc(1, MAX_CTX_LEN)))
		goto out_err;
	p = buf->value;
	end = buf->value + MAX_CTX_LEN;


	/* initiate:  1 => initiating 0 => accepting */
	if (ctx->more_flags & LOCAL) {
		if (WRITE_BYTES(&p, end, constant_one)) goto out_err;
	}
	else {
		if (WRITE_BYTES(&p, end, constant_zero)) goto out_err;
	}

	/* seed_init: not used by kernel code */
	if (WRITE_BYTES(&p, end, constant_zero)) goto out_err;

	/* seed: not used by kernel code */
	memset(&fakeseed, 0, sizeof(fakeseed));
	if (write_bytes(&p, end, &fakeseed, 16)) goto out_err;

	/* signalg */
	algorithm = 0; /* SGN_ALG_DES_MAC_MD5	XXX */
	if (WRITE_BYTES(&p, end, algorithm)) goto out_err;

	/* sealalg */
	algorithm = 0; /* SEAL_ALG_DES		XXX */
	if (WRITE_BYTES(&p, end, algorithm)) goto out_err;

	/* endtime */
	if (WRITE_BYTES(&p, end, ctx->lifetime)) goto out_err;

	if (endtime)
		*endtime = ctx->lifetime;

	/* seq_send */
	if (WRITE_BYTES(&p, end, ctx->auth_context->local_seqnumber))
		goto out_err;
	/* mech_used */
	if (write_buffer(&p, end, (gss_buffer_desc*)&krb5oid)) goto out_err;

	/* enc: derive the encryption key and copy it into buffer */
	if (write_heimdal_enc_key(&p, end, ctx)) goto out_err;

	/* seq: get the sequence number key and copy it into buffer */
	if (write_heimdal_seq_key(&p, end, ctx)) goto out_err;

	buf->length = p - (char *)buf->value;
	printerr(2, "serialize_krb5_ctx: returning buffer "
		    "with %d bytes\n", buf->length);

	return 0;
out_err:
	printerr(0, "ERROR: failed exporting Heimdal krb5 ctx to kernel\n");
	if (buf->value) free(buf->value);
	buf->length = 0;
	return -1;
}

#endif	/* HAVE_HEIMDAL */
#endif	/* HAVE_LUCID_CONTEXT_SUPPORT */
