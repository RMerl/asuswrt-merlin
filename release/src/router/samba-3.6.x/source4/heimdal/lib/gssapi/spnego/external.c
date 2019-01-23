/*
 * Copyright (c) 2004, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "spnego_locl.h"
#include <gssapi_mech.h>

/*
 * RFC2478, SPNEGO:
 *  The security mechanism of the initial
 *  negotiation token is identified by the Object Identifier
 *  iso.org.dod.internet.security.mechanism.snego (1.3.6.1.5.5.2).
 */

static gss_mo_desc spnego_mo[] = {
    {
	GSS_C_MA_SASL_MECH_NAME,
	GSS_MO_MA,
	"SASL mech name",
	"SPNEGO",
	_gss_mo_get_ctx_as_string,
	NULL
    },
    {
	GSS_C_MA_MECH_NAME,
	GSS_MO_MA,
	"Mechanism name",
	"SPNEGO",
	_gss_mo_get_ctx_as_string,
	NULL
    },
    {
	GSS_C_MA_MECH_DESCRIPTION,
	GSS_MO_MA,
	"Mechanism description",
	"Heimdal SPNEGO Mechanism",
	_gss_mo_get_ctx_as_string,
	NULL
    },
    {
	GSS_C_MA_MECH_NEGO,
	GSS_MO_MA
    },
    {
	GSS_C_MA_MECH_PSEUDO,
	GSS_MO_MA
    }
};

static gssapi_mech_interface_desc spnego_mech = {
    GMI_VERSION,
    "spnego",
    {6, (void *)"\x2b\x06\x01\x05\x05\x02"},
    0,
    _gss_spnego_acquire_cred,
    _gss_spnego_release_cred,
    _gss_spnego_init_sec_context,
    _gss_spnego_accept_sec_context,
    _gss_spnego_process_context_token,
    _gss_spnego_internal_delete_sec_context,
    _gss_spnego_context_time,
    _gss_spnego_get_mic,
    _gss_spnego_verify_mic,
    _gss_spnego_wrap,
    _gss_spnego_unwrap,
    NULL, /* gm_display_status */
    NULL, /* gm_indicate_mechs */
    _gss_spnego_compare_name,
    _gss_spnego_display_name,
    _gss_spnego_import_name,
    _gss_spnego_export_name,
    _gss_spnego_release_name,
    _gss_spnego_inquire_cred,
    _gss_spnego_inquire_context,
    _gss_spnego_wrap_size_limit,
    gss_add_cred,
    _gss_spnego_inquire_cred_by_mech,
    _gss_spnego_export_sec_context,
    _gss_spnego_import_sec_context,
    NULL /* _gss_spnego_inquire_names_for_mech */,
    _gss_spnego_inquire_mechs_for_name,
    _gss_spnego_canonicalize_name,
    _gss_spnego_duplicate_name,
    _gss_spnego_inquire_sec_context_by_oid,
    _gss_spnego_inquire_cred_by_oid,
    _gss_spnego_set_sec_context_option,
    _gss_spnego_set_cred_option,
    _gss_spnego_pseudo_random,
    _gss_spnego_wrap_iov,
    _gss_spnego_unwrap_iov,
    _gss_spnego_wrap_iov_length,
    NULL,
    _gss_spnego_export_cred,
    _gss_spnego_import_cred,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    spnego_mo,
    sizeof(spnego_mo) / sizeof(spnego_mo[0])
};

gssapi_mech_interface
__gss_spnego_initialize(void)
{
	return &spnego_mech;
}
