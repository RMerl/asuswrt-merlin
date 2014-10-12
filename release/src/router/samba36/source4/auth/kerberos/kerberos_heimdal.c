/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* This file for code taken from the Heimdal code, to preserve licence */
/* Modified by Andrew Bartlett <abartlet@samba.org> */

#include "includes.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"

/* Taken from  accept_sec_context.c,v 1.65 */
krb5_error_code smb_rd_req_return_stuff(krb5_context context, 
					krb5_auth_context *auth_context,
					const krb5_data *inbuf,
					krb5_keytab keytab, 
					krb5_principal acceptor_principal,
					krb5_data *outbuf, 
					krb5_ticket **ticket, 
					krb5_keyblock **keyblock)
{
	krb5_rd_req_in_ctx in = NULL;
	krb5_rd_req_out_ctx out = NULL;
	krb5_error_code kret;

	*keyblock = NULL;
	*ticket = NULL;
	outbuf->length = 0;
	outbuf->data = NULL;

	kret = krb5_rd_req_in_ctx_alloc(context, &in);
	if (kret == 0)
	    kret = krb5_rd_req_in_set_keytab(context, in, keytab);
	if (kret) {
	    if (in)
		krb5_rd_req_in_ctx_free(context, in);
	    return kret;
	}

	kret = krb5_rd_req_ctx(context,
			       auth_context,
			       inbuf,
			       acceptor_principal,
			       in, &out);
	krb5_rd_req_in_ctx_free(context, in);
	if (kret) {
	    return kret;
	}

	/*
	 * We need to remember some data on the context_handle.
	 */
	kret = krb5_rd_req_out_get_ticket(context, out, 
					  ticket);
	if (kret == 0) {
	    kret = krb5_rd_req_out_get_keyblock(context, out,
						keyblock);
	}
	krb5_rd_req_out_ctx_free(context, out);

	if (kret == 0) {
		kret = krb5_mk_rep(context, *auth_context, outbuf);
	}

	if (kret) {
		krb5_free_ticket(context, *ticket);
		krb5_free_keyblock(context, *keyblock);
		krb5_data_free(outbuf);
	}

	return kret;
}
    
