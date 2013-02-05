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

int
serialize_context_for_kernel(gss_ctx_id_t ctx,
			     gss_buffer_desc *buf,
			     gss_OID mech,
			     int32_t *endtime)
{
	if (g_OID_equal(&krb5oid, mech))
		return serialize_krb5_ctx(ctx, buf, endtime);
#ifdef HAVE_SPKM3_H
	else if (g_OID_equal(&spkm3oid, mech))
		return serialize_spkm3_ctx(ctx, buf, endtime);
#endif
	else {
		printerr(0, "ERROR: attempting to serialize context with "
				"unknown/unsupported mechanism oid\n");
		return -1;
	}
}
