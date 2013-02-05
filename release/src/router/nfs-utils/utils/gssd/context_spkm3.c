/*
  Copyright (c) 2004 The Regents of the University of Michigan.
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

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <gssapi/gssapi.h>
#include <rpc/rpc.h>
#include <rpc/auth_gss.h>
#include "gss_util.h"
#include "gss_oids.h"
#include "err_util.h"
#include "context.h"

#ifdef HAVE_SPKM3_H

#include <spkm3.h>

/*
 * Function: prepare_spkm3_ctx_buffer()
 *
 * Prepare spkm3 lucid context for the kernel
 *
 *	buf->length should be:
 *
 *      version 4
 *	ctx_id 4 + 12
 *	qop 4
 *	mech_used 4 + 7
 *	ret_fl  4
 *	req_fl  4
 *      share   4 + key_len
 *      conf_alg 4 + oid_len
 *      d_conf_key 4 + key_len
 *      intg_alg 4 + oid_len
 *      d_intg_key 4 + key_len
 *      kyestb 4 + oid_len
 *      owl alg 4 + oid_len
*/
static int
prepare_spkm3_ctx_buffer(gss_spkm3_lucid_ctx_t *lctx, gss_buffer_desc *buf)
{
	char *p, *end;
	unsigned int buf_size = 0;

	buf_size = sizeof(lctx->version) +
		lctx->ctx_id.length + sizeof(lctx->ctx_id.length) +
		sizeof(lctx->endtime) +
		sizeof(lctx->mech_used.length) + lctx->mech_used.length +
		sizeof(lctx->ret_flags) +
		sizeof(lctx->conf_alg.length) + lctx->conf_alg.length +
		sizeof(lctx->derived_conf_key.length) +
		lctx->derived_conf_key.length +
		sizeof(lctx->intg_alg.length) + lctx->intg_alg.length +
		sizeof(lctx->derived_integ_key.length) +
		lctx->derived_integ_key.length;

	if (!(buf->value = calloc(1, buf_size)))
		goto out_err;
	p = buf->value;
	end = buf->value + buf_size;

	if (WRITE_BYTES(&p, end, lctx->version))
		goto out_err;
	printerr(2, "DEBUG: exporting version = %d\n", lctx->version);

	if (write_buffer(&p, end, &lctx->ctx_id))
		goto out_err;
	printerr(2, "DEBUG: exporting ctx_id(%d)\n", lctx->ctx_id.length);

	if (WRITE_BYTES(&p, end, lctx->endtime))
		goto out_err;
	printerr(2, "DEBUG: exporting endtime = %d\n", lctx->endtime);

	if (write_buffer(&p, end, &lctx->mech_used))
		goto out_err;
	printerr(2, "DEBUG: exporting mech oid (%d)\n", lctx->mech_used.length);

	if (WRITE_BYTES(&p, end, lctx->ret_flags))
		goto out_err;
	printerr(2, "DEBUG: exporting ret_flags = %d\n", lctx->ret_flags);

	if (write_buffer(&p, end, &lctx->conf_alg))
		goto out_err;
	printerr(2, "DEBUG: exporting conf_alg oid (%d)\n", lctx->conf_alg.length);

	if (write_buffer(&p, end, &lctx->derived_conf_key))
		goto out_err;
	printerr(2, "DEBUG: exporting conf key (%d)\n", lctx->derived_conf_key.length);

	if (write_buffer(&p, end, &lctx->intg_alg))
		goto out_err;
	printerr(2, "DEBUG: exporting intg_alg oid (%d)\n", lctx->intg_alg.length);

	if (write_buffer(&p, end, &lctx->derived_integ_key))
		goto out_err;
	printerr(2, "DEBUG: exporting intg key (%d)\n", lctx->derived_integ_key.length);

	buf->length = p - (char *)buf->value;
	return 0;
out_err:
	printerr(0, "ERROR: failed serializing spkm3 context for kernel\n");
	if (buf->value) free(buf->value);
	buf->length = 0;

	return -1;
}

/* ANDROS: need to determine which fields of the spkm3_gss_ctx_id_desc_t
 * are needed in the kernel for get_mic, validate, wrap, unwrap, and destroy
 * and only export those fields to the kernel.
 */
int
serialize_spkm3_ctx(gss_ctx_id_t ctx, gss_buffer_desc *buf, int32_t *endtime)
{
	OM_uint32 vers, ret, maj_stat, min_stat;
	void *ret_ctx = 0;
	gss_spkm3_lucid_ctx_t     *lctx;

	printerr(1, "serialize_spkm3_ctx called\n");

	printerr(2, "DEBUG: serialize_spkm3_ctx: lucid version!\n");
	maj_stat = gss_export_lucid_sec_context(&min_stat, &ctx, 1, &ret_ctx);
	if (maj_stat != GSS_S_COMPLETE)
		goto out_err;

	lctx = (gss_spkm3_lucid_ctx_t *)ret_ctx;

	vers = lctx->version;
	if (vers != 1) {
		printerr(0, "ERROR: unsupported spkm3 context version %d\n",
			vers);
		goto out_err;
	}
	ret = prepare_spkm3_ctx_buffer(lctx, buf);

	if (endtime)
		*endtime = lctx->endtime;

	maj_stat = gss_free_lucid_sec_context(&min_stat, ctx, ret_ctx);

	if (maj_stat != GSS_S_COMPLETE)
		printerr(0, "WARN: failed to free lucid sec context\n");
	if (ret)
		goto out_err;
	printerr(2, "DEBUG: serialize_spkm3_ctx: success\n");
	return 0;

out_err:
	printerr(2, "DEBUG: serialize_spkm3_ctx: failed\n");
	return -1;
}
#endif /* HAVE_SPKM3_H */
