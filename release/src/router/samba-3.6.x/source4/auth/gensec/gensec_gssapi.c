/* 
   Unix SMB/CIFS implementation.

   Kerberos backend for GENSEC
   
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2004-2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/events/events.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/krb5pac.h"
#include "auth/auth.h"
#include <ldb.h>
#include "auth/auth_sam.h"
#include "librpc/rpc/dcerpc.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "param/param.h"
#include "auth/session_proto.h"
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <gssapi/gssapi_spnego.h>
#include "auth/gensec/gensec_gssapi.h"
#include "lib/util/util_net.h"

static size_t gensec_gssapi_max_input_size(struct gensec_security *gensec_security);
static size_t gensec_gssapi_max_wrapped_size(struct gensec_security *gensec_security);

static char *gssapi_error_string(TALLOC_CTX *mem_ctx, 
				 OM_uint32 maj_stat, OM_uint32 min_stat, 
				 const gss_OID mech)
{
	OM_uint32 disp_min_stat, disp_maj_stat;
	gss_buffer_desc maj_error_message;
	gss_buffer_desc min_error_message;
	char *maj_error_string, *min_error_string;
	OM_uint32 msg_ctx = 0;

	char *ret;

	maj_error_message.value = NULL;
	min_error_message.value = NULL;
	maj_error_message.length = 0;
	min_error_message.length = 0;
	
	disp_maj_stat = gss_display_status(&disp_min_stat, maj_stat, GSS_C_GSS_CODE,
			   mech, &msg_ctx, &maj_error_message);
	disp_maj_stat = gss_display_status(&disp_min_stat, min_stat, GSS_C_MECH_CODE,
			   mech, &msg_ctx, &min_error_message);
	
	maj_error_string = talloc_strndup(mem_ctx, (char *)maj_error_message.value, maj_error_message.length);

	min_error_string = talloc_strndup(mem_ctx, (char *)min_error_message.value, min_error_message.length);

	ret = talloc_asprintf(mem_ctx, "%s: %s", maj_error_string, min_error_string);

	talloc_free(maj_error_string);
	talloc_free(min_error_string);

	gss_release_buffer(&disp_min_stat, &maj_error_message);
	gss_release_buffer(&disp_min_stat, &min_error_message);

	return ret;
}


static int gensec_gssapi_destructor(struct gensec_gssapi_state *gensec_gssapi_state)
{
	OM_uint32 maj_stat, min_stat;
	
	if (gensec_gssapi_state->delegated_cred_handle != GSS_C_NO_CREDENTIAL) {
		maj_stat = gss_release_cred(&min_stat, 
					    &gensec_gssapi_state->delegated_cred_handle);
	}

	if (gensec_gssapi_state->gssapi_context != GSS_C_NO_CONTEXT) {
		maj_stat = gss_delete_sec_context (&min_stat,
						   &gensec_gssapi_state->gssapi_context,
						   GSS_C_NO_BUFFER);
	}

	if (gensec_gssapi_state->server_name != GSS_C_NO_NAME) {
		maj_stat = gss_release_name(&min_stat, &gensec_gssapi_state->server_name);
	}
	if (gensec_gssapi_state->client_name != GSS_C_NO_NAME) {
		maj_stat = gss_release_name(&min_stat, &gensec_gssapi_state->client_name);
	}

	if (gensec_gssapi_state->lucid) {
		gss_krb5_free_lucid_sec_context(&min_stat, gensec_gssapi_state->lucid);
	}

	return 0;
}

static NTSTATUS gensec_gssapi_init_lucid(struct gensec_gssapi_state *gensec_gssapi_state)
{
	OM_uint32 maj_stat, min_stat;

	if (gensec_gssapi_state->lucid) {
		return NT_STATUS_OK;
	}

	maj_stat = gss_krb5_export_lucid_sec_context(&min_stat,
						     &gensec_gssapi_state->gssapi_context,
						     1,
						     (void **)&gensec_gssapi_state->lucid);
	if (maj_stat != GSS_S_COMPLETE) {
		DEBUG(0,("gensec_gssapi_init_lucid: %s\n",
			gssapi_error_string(gensec_gssapi_state,
					    maj_stat, min_stat,
					    gensec_gssapi_state->gss_oid)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (gensec_gssapi_state->lucid->version != 1) {
		DEBUG(0,("gensec_gssapi_init_lucid: lucid version[%d] != 1\n",
			gensec_gssapi_state->lucid->version));
		gss_krb5_free_lucid_sec_context(&min_stat, gensec_gssapi_state->lucid);
		gensec_gssapi_state->lucid = NULL;
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_start(struct gensec_security *gensec_security)
{
	struct gensec_gssapi_state *gensec_gssapi_state;
	krb5_error_code ret;
	const char *realm;

	gensec_gssapi_state = talloc_zero(gensec_security, struct gensec_gssapi_state);
	if (!gensec_gssapi_state) {
		return NT_STATUS_NO_MEMORY;
	}

	gensec_security->private_data = gensec_gssapi_state;

	gensec_gssapi_state->gssapi_context = GSS_C_NO_CONTEXT;

	/* TODO: Fill in channel bindings */
	gensec_gssapi_state->input_chan_bindings = GSS_C_NO_CHANNEL_BINDINGS;

	gensec_gssapi_state->server_name = GSS_C_NO_NAME;
	gensec_gssapi_state->client_name = GSS_C_NO_NAME;
	
	gensec_gssapi_state->want_flags = 0;

	if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "delegation_by_kdc_policy", true)) {
		gensec_gssapi_state->want_flags |= GSS_C_DELEG_POLICY_FLAG;
	}
	if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "mutual", true)) {
		gensec_gssapi_state->want_flags |= GSS_C_MUTUAL_FLAG;
	}
	if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "delegation", true)) {
		gensec_gssapi_state->want_flags |= GSS_C_DELEG_FLAG;
	}
	if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "replay", true)) {
		gensec_gssapi_state->want_flags |= GSS_C_REPLAY_FLAG;
	}
	if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "sequence", true)) {
		gensec_gssapi_state->want_flags |= GSS_C_SEQUENCE_FLAG;
	}

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		gensec_gssapi_state->want_flags |= GSS_C_INTEG_FLAG;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		gensec_gssapi_state->want_flags |= GSS_C_CONF_FLAG;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_DCE_STYLE) {
		gensec_gssapi_state->want_flags |= GSS_C_DCE_STYLE;
	}

	gensec_gssapi_state->got_flags = 0;

	switch (gensec_security->ops->auth_type) {
	case DCERPC_AUTH_TYPE_SPNEGO:
		gensec_gssapi_state->gss_oid = gss_mech_spnego;
		break;
	case DCERPC_AUTH_TYPE_KRB5:
	default:
		gensec_gssapi_state->gss_oid = gss_mech_krb5;
		break;
	}

	gensec_gssapi_state->session_key = data_blob(NULL, 0);
	gensec_gssapi_state->pac = data_blob(NULL, 0);

	ret = smb_krb5_init_context(gensec_gssapi_state,
				    NULL,
				    gensec_security->settings->lp_ctx,
				    &gensec_gssapi_state->smb_krb5_context);
	if (ret) {
		DEBUG(1,("gensec_krb5_start: krb5_init_context failed (%s)\n",
			 error_message(ret)));
		talloc_free(gensec_gssapi_state);
		return NT_STATUS_INTERNAL_ERROR;
	}

	gensec_gssapi_state->client_cred = NULL;
	gensec_gssapi_state->server_cred = NULL;

	gensec_gssapi_state->lucid = NULL;

	gensec_gssapi_state->delegated_cred_handle = GSS_C_NO_CREDENTIAL;

	gensec_gssapi_state->sasl = false;
	gensec_gssapi_state->sasl_state = STAGE_GSS_NEG;
	gensec_gssapi_state->sasl_protection = 0;

	gensec_gssapi_state->max_wrap_buf_size
		= gensec_setting_int(gensec_security->settings, "gensec_gssapi", "max wrap buf size", 65536);
	gensec_gssapi_state->gss_exchange_count = 0;
	gensec_gssapi_state->sig_size = 0;

	talloc_set_destructor(gensec_gssapi_state, gensec_gssapi_destructor);

	realm = lpcfg_realm(gensec_security->settings->lp_ctx);
	if (realm != NULL) {
		ret = gsskrb5_set_default_realm(realm);
		if (ret) {
			DEBUG(1,("gensec_krb5_start: gsskrb5_set_default_realm failed\n"));
			talloc_free(gensec_gssapi_state);
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	/* don't do DNS lookups of any kind, it might/will fail for a netbios name */
	ret = gsskrb5_set_dns_canonicalize(gensec_setting_bool(gensec_security->settings, "krb5", "set_dns_canonicalize", false));
	if (ret) {
		DEBUG(1,("gensec_krb5_start: gsskrb5_set_dns_canonicalize failed\n"));
		talloc_free(gensec_gssapi_state);
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	int ret;
	struct gensec_gssapi_state *gensec_gssapi_state;
	struct cli_credentials *machine_account;
	struct gssapi_creds_container *gcc;

	nt_status = gensec_gssapi_start(gensec_security);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	gensec_gssapi_state = talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);

	machine_account = gensec_get_credentials(gensec_security);
	
	if (!machine_account) {
		DEBUG(3, ("No machine account credentials specified\n"));
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	} else {
		ret = cli_credentials_get_server_gss_creds(machine_account, 
							   gensec_security->settings->lp_ctx, &gcc);
		if (ret) {
			DEBUG(1, ("Aquiring acceptor credentials failed: %s\n", 
				  error_message(ret)));
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	gensec_gssapi_state->server_cred = gcc;
	return NT_STATUS_OK;

}

static NTSTATUS gensec_gssapi_sasl_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	struct gensec_gssapi_state *gensec_gssapi_state;
	nt_status = gensec_gssapi_server_start(gensec_security);

	if (NT_STATUS_IS_OK(nt_status)) {
		gensec_gssapi_state = talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
		gensec_gssapi_state->sasl = true;
	}
	return nt_status;
}

static NTSTATUS gensec_gssapi_client_start(struct gensec_security *gensec_security)
{
	struct gensec_gssapi_state *gensec_gssapi_state;
	struct cli_credentials *creds = gensec_get_credentials(gensec_security);
	krb5_error_code ret;
	NTSTATUS nt_status;
	gss_buffer_desc name_token;
	gss_OID name_type;
	OM_uint32 maj_stat, min_stat;
	const char *hostname = gensec_get_target_hostname(gensec_security);
	struct gssapi_creds_container *gcc;
	const char *error_string;

	if (!hostname) {
		DEBUG(1, ("Could not determine hostname for target computer, cannot use kerberos\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (is_ipaddress(hostname)) {
		DEBUG(2, ("Cannot do GSSAPI to an IP address\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (strcmp(hostname, "localhost") == 0) {
		DEBUG(2, ("GSSAPI to 'localhost' does not make sense\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	nt_status = gensec_gssapi_start(gensec_security);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	gensec_gssapi_state = talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);

	gensec_gssapi_state->target_principal = gensec_get_target_principal(gensec_security);
	if (gensec_gssapi_state->target_principal) {
		name_type = GSS_C_NULL_OID;
	} else {
		gensec_gssapi_state->target_principal = talloc_asprintf(gensec_gssapi_state, "%s/%s@%s",
					    gensec_get_target_service(gensec_security), 
					    hostname, lpcfg_realm(gensec_security->settings->lp_ctx));

		name_type = GSS_C_NT_USER_NAME;
	}
	name_token.value  = discard_const_p(uint8_t, gensec_gssapi_state->target_principal);
	name_token.length = strlen(gensec_gssapi_state->target_principal);


	maj_stat = gss_import_name (&min_stat,
				    &name_token,
				    name_type,
				    &gensec_gssapi_state->server_name);
	if (maj_stat) {
		DEBUG(2, ("GSS Import name of %s failed: %s\n",
			  (char *)name_token.value,
			  gssapi_error_string(gensec_gssapi_state, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ret = cli_credentials_get_client_gss_creds(creds, 
						   gensec_security->event_ctx, 
						   gensec_security->settings->lp_ctx, &gcc, &error_string);
	switch (ret) {
	case 0:
		break;
	case KRB5KDC_ERR_PREAUTH_FAILED:
	case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
		DEBUG(1, ("Wrong username or password: %s\n", error_string));
		return NT_STATUS_LOGON_FAILURE;
	case KRB5_KDC_UNREACH:
		DEBUG(3, ("Cannot reach a KDC we require to contact %s : %s\n", gensec_gssapi_state->target_principal, error_string));
		return NT_STATUS_NO_LOGON_SERVERS;
	case KRB5_CC_NOTFOUND:
	case KRB5_CC_END:
		DEBUG(2, ("Error obtaining ticket we require to contact %s: (possibly due to clock skew between us and the KDC) %s\n", gensec_gssapi_state->target_principal, error_string));
		return NT_STATUS_TIME_DIFFERENCE_AT_DC;
	default:
		DEBUG(1, ("Aquiring initiator credentials failed: %s\n", error_string));
		return NT_STATUS_UNSUCCESSFUL;
	}

	gensec_gssapi_state->client_cred = gcc;
	if (!talloc_reference(gensec_gssapi_state, gcc)) {
		return NT_STATUS_NO_MEMORY;
	}
	
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_sasl_client_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	struct gensec_gssapi_state *gensec_gssapi_state;
	nt_status = gensec_gssapi_client_start(gensec_security);

	if (NT_STATUS_IS_OK(nt_status)) {
		gensec_gssapi_state = talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
		gensec_gssapi_state->sasl = true;
	}
	return nt_status;
}


/**
 * Check if the packet is one for this mechansim
 * 
 * @param gensec_security GENSEC state
 * @param in The request, as a DATA_BLOB
 * @return Error, INVALID_PARAMETER if it's not a packet for us
 *                or NT_STATUS_OK if the packet is ok. 
 */

static NTSTATUS gensec_gssapi_magic(struct gensec_security *gensec_security, 
				    const DATA_BLOB *in) 
{
	if (gensec_gssapi_check_oid(in, GENSEC_OID_KERBEROS5)) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}
}


/**
 * Next state function for the GSSAPI GENSEC mechanism
 * 
 * @param gensec_gssapi_state GSSAPI State
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */

static NTSTATUS gensec_gssapi_update(struct gensec_security *gensec_security, 
				   TALLOC_CTX *out_mem_ctx, 
				   const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	NTSTATUS nt_status = NT_STATUS_LOGON_FAILURE;
	OM_uint32 maj_stat, min_stat;
	OM_uint32 min_stat2;
	gss_buffer_desc input_token, output_token;
	gss_OID gss_oid_p = NULL;
	input_token.length = in.length;
	input_token.value = in.data;

	switch (gensec_gssapi_state->sasl_state) {
	case STAGE_GSS_NEG:
	{
		switch (gensec_security->gensec_role) {
		case GENSEC_CLIENT:
		{
			struct gsskrb5_send_to_kdc send_to_kdc;
			krb5_error_code ret;
			send_to_kdc.func = smb_krb5_send_and_recv_func;
			send_to_kdc.ptr = gensec_security->event_ctx;

			min_stat = gsskrb5_set_send_to_kdc(&send_to_kdc);
			if (min_stat) {
				DEBUG(1,("gensec_krb5_start: gsskrb5_set_send_to_kdc failed\n"));
				return NT_STATUS_INTERNAL_ERROR;
			}

			maj_stat = gss_init_sec_context(&min_stat, 
							gensec_gssapi_state->client_cred->creds,
							&gensec_gssapi_state->gssapi_context, 
							gensec_gssapi_state->server_name, 
							gensec_gssapi_state->gss_oid,
							gensec_gssapi_state->want_flags, 
							0, 
							gensec_gssapi_state->input_chan_bindings,
							&input_token, 
							&gss_oid_p,
							&output_token, 
							&gensec_gssapi_state->got_flags, /* ret flags */
							NULL);
			if (gss_oid_p) {
				gensec_gssapi_state->gss_oid = gss_oid_p;
			}

			send_to_kdc.func = smb_krb5_send_and_recv_func;
			send_to_kdc.ptr = NULL;

			ret = gsskrb5_set_send_to_kdc(&send_to_kdc);
			if (ret) {
				DEBUG(1,("gensec_krb5_start: gsskrb5_set_send_to_kdc failed\n"));
				return NT_STATUS_INTERNAL_ERROR;
			}

			break;
		}
		case GENSEC_SERVER:
		{
			maj_stat = gss_accept_sec_context(&min_stat, 
							  &gensec_gssapi_state->gssapi_context, 
							  gensec_gssapi_state->server_cred->creds,
							  &input_token, 
							  gensec_gssapi_state->input_chan_bindings,
							  &gensec_gssapi_state->client_name, 
							  &gss_oid_p,
							  &output_token, 
							  &gensec_gssapi_state->got_flags, 
							  NULL, 
							  &gensec_gssapi_state->delegated_cred_handle);
			if (gss_oid_p) {
				gensec_gssapi_state->gss_oid = gss_oid_p;
			}
			break;
		}
		default:
			return NT_STATUS_INVALID_PARAMETER;
			
		}

		gensec_gssapi_state->gss_exchange_count++;

		if (maj_stat == GSS_S_COMPLETE) {
			*out = data_blob_talloc(out_mem_ctx, output_token.value, output_token.length);
			gss_release_buffer(&min_stat2, &output_token);
			
			if (gensec_gssapi_state->got_flags & GSS_C_DELEG_FLAG) {
				DEBUG(5, ("gensec_gssapi: credentials were delegated\n"));
			} else {
				DEBUG(5, ("gensec_gssapi: NO credentials were delegated\n"));
			}

			/* We may have been invoked as SASL, so there
			 * is more work to do */
			if (gensec_gssapi_state->sasl) {
				gensec_gssapi_state->sasl_state = STAGE_SASL_SSF_NEG;
				return NT_STATUS_MORE_PROCESSING_REQUIRED;
			} else {
				gensec_gssapi_state->sasl_state = STAGE_DONE;

				if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
					DEBUG(5, ("GSSAPI Connection will be cryptographically sealed\n"));
				} else if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
					DEBUG(5, ("GSSAPI Connection will be cryptographically signed\n"));
				} else {
					DEBUG(5, ("GSSAPI Connection will have no cryptographic protection\n"));
				}

				return NT_STATUS_OK;
			}
		} else if (maj_stat == GSS_S_CONTINUE_NEEDED) {
			*out = data_blob_talloc(out_mem_ctx, output_token.value, output_token.length);
			gss_release_buffer(&min_stat2, &output_token);
			
			return NT_STATUS_MORE_PROCESSING_REQUIRED;
		} else if (gss_oid_equal(gensec_gssapi_state->gss_oid, gss_mech_krb5)) {
			switch (min_stat) {
			case KRB5KRB_AP_ERR_TKT_NYV:
				DEBUG(1, ("Error with ticket to contact %s: possible clock skew between us and the KDC or target server: %s\n",
					  gensec_gssapi_state->target_principal,
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_TIME_DIFFERENCE_AT_DC; /* Make SPNEGO ignore us, we can't go any further here */
			case KRB5KRB_AP_ERR_TKT_EXPIRED:
				DEBUG(1, ("Error with ticket to contact %s: ticket is expired, possible clock skew between us and the KDC or target server: %s\n",
					  gensec_gssapi_state->target_principal,
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_INVALID_PARAMETER; /* Make SPNEGO ignore us, we can't go any further here */
			case KRB5_KDC_UNREACH:
				DEBUG(3, ("Cannot reach a KDC we require in order to obtain a ticetk to %s: %s\n",
					  gensec_gssapi_state->target_principal,
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_NO_LOGON_SERVERS; /* Make SPNEGO ignore us, we can't go any further here */
			case KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN:
				DEBUG(3, ("Server %s is not registered with our KDC: %s\n",
					  gensec_gssapi_state->target_principal,
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_INVALID_PARAMETER; /* Make SPNEGO ignore us, we can't go any further here */
			case KRB5KRB_AP_ERR_MSG_TYPE:
				/* garbage input, possibly from the auto-mech detection */
				return NT_STATUS_INVALID_PARAMETER;
			default:
				DEBUG(1, ("GSS %s Update(krb5)(%d) Update failed: %s\n",
					  gensec_security->gensec_role == GENSEC_CLIENT ? "client" : "server",
					  gensec_gssapi_state->gss_exchange_count,
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return nt_status;
			}
		} else {
			DEBUG(1, ("GSS %s Update(%d) failed: %s\n",
				  gensec_security->gensec_role == GENSEC_CLIENT ? "client" : "server",
				  gensec_gssapi_state->gss_exchange_count,
				  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
			return nt_status;
		}
		break;
	}

	/* These last two stages are only done if we were invoked as SASL */
	case STAGE_SASL_SSF_NEG:
	{
		switch (gensec_security->gensec_role) {
		case GENSEC_CLIENT:
		{
			uint8_t maxlength_proposed[4]; 
			uint8_t maxlength_accepted[4]; 
			uint8_t security_supported;
			int conf_state;
			gss_qop_t qop_state;
			input_token.length = in.length;
			input_token.value = in.data;

			/* As a client, we have just send a
			 * zero-length blob to the server (after the
			 * normal GSSAPI exchange), and it has replied
			 * with it's SASL negotiation */
			
			maj_stat = gss_unwrap(&min_stat, 
					      gensec_gssapi_state->gssapi_context, 
					      &input_token,
					      &output_token, 
					      &conf_state,
					      &qop_state);
			if (GSS_ERROR(maj_stat)) {
				DEBUG(1, ("gensec_gssapi_update: GSS UnWrap of SASL protection negotiation failed: %s\n", 
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_ACCESS_DENIED;
			}
			
			if (output_token.length < 4) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			memcpy(maxlength_proposed, output_token.value, 4);
			gss_release_buffer(&min_stat, &output_token);

			/* first byte is the proposed security */
			security_supported = maxlength_proposed[0];
			maxlength_proposed[0] = '\0';
			
			/* Rest is the proposed max wrap length */
			gensec_gssapi_state->max_wrap_buf_size = MIN(RIVAL(maxlength_proposed, 0), 
								     gensec_gssapi_state->max_wrap_buf_size);
			gensec_gssapi_state->sasl_protection = 0;
			if (security_supported & NEG_SEAL) {
				if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
					gensec_gssapi_state->sasl_protection |= NEG_SEAL;
				}
			}
			if (security_supported & NEG_SIGN) {
				if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
					gensec_gssapi_state->sasl_protection |= NEG_SIGN;
				}
			}
			if (security_supported & NEG_NONE) {
				gensec_gssapi_state->sasl_protection |= NEG_NONE;
			}
			if (gensec_gssapi_state->sasl_protection == 0) {
				DEBUG(1, ("Remote server does not support unprotected connections\n"));
				return NT_STATUS_ACCESS_DENIED;
			}

			/* Send back the negotiated max length */

			RSIVAL(maxlength_accepted, 0, gensec_gssapi_state->max_wrap_buf_size);

			maxlength_accepted[0] = gensec_gssapi_state->sasl_protection;
			
			input_token.value = maxlength_accepted;
			input_token.length = sizeof(maxlength_accepted);

			maj_stat = gss_wrap(&min_stat, 
					    gensec_gssapi_state->gssapi_context, 
					    false,
					    GSS_C_QOP_DEFAULT,
					    &input_token,
					    &conf_state,
					    &output_token);
			if (GSS_ERROR(maj_stat)) {
				DEBUG(1, ("GSS Update(SSF_NEG): GSS Wrap failed: %s\n", 
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_ACCESS_DENIED;
			}
			
			*out = data_blob_talloc(out_mem_ctx, output_token.value, output_token.length);
			gss_release_buffer(&min_stat, &output_token);

			/* quirk:  This changes the value that gensec_have_feature returns, to be that after SASL negotiation */
			gensec_gssapi_state->sasl_state = STAGE_DONE;

			if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
				DEBUG(3, ("SASL/GSSAPI Connection to server will be cryptographically sealed\n"));
			} else if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
				DEBUG(3, ("SASL/GSSAPI Connection to server will be cryptographically signed\n"));
			} else {
				DEBUG(3, ("SASL/GSSAPI Connection to server will have no cryptographically protection\n"));
			}

			return NT_STATUS_OK;
		}
		case GENSEC_SERVER:
		{
			uint8_t maxlength_proposed[4]; 
			uint8_t security_supported = 0x0;
			int conf_state;

			/* As a server, we have just been sent a zero-length blob (note this, but it isn't fatal) */
			if (in.length != 0) {
				DEBUG(1, ("SASL/GSSAPI: client sent non-zero length starting SASL negotiation!\n"));
			}
			
			/* Give the client some idea what we will support */
			  
			RSIVAL(maxlength_proposed, 0, gensec_gssapi_state->max_wrap_buf_size);
			/* first byte is the proposed security */
			maxlength_proposed[0] = '\0';
			
			gensec_gssapi_state->sasl_protection = 0;
			if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
				security_supported |= NEG_SEAL;
			} 
			if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
				security_supported |= NEG_SIGN;
			}
			if (security_supported == 0) {
				/* If we don't support anything, this must be 0 */
				RSIVAL(maxlength_proposed, 0, 0x0);
			}

			/* TODO:  We may not wish to support this */
			security_supported |= NEG_NONE;
			maxlength_proposed[0] = security_supported;
			
			input_token.value = maxlength_proposed;
			input_token.length = sizeof(maxlength_proposed);

			maj_stat = gss_wrap(&min_stat, 
					    gensec_gssapi_state->gssapi_context, 
					    false,
					    GSS_C_QOP_DEFAULT,
					    &input_token,
					    &conf_state,
					    &output_token);
			if (GSS_ERROR(maj_stat)) {
				DEBUG(1, ("GSS Update(SSF_NEG): GSS Wrap failed: %s\n", 
					  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
				return NT_STATUS_ACCESS_DENIED;
			}
			
			*out = data_blob_talloc(out_mem_ctx, output_token.value, output_token.length);
			gss_release_buffer(&min_stat, &output_token);

			gensec_gssapi_state->sasl_state = STAGE_SASL_SSF_ACCEPT;
			return NT_STATUS_MORE_PROCESSING_REQUIRED;
		}
		default:
  			return NT_STATUS_INVALID_PARAMETER;
			
		}
	}
	/* This is s server-only stage */
	case STAGE_SASL_SSF_ACCEPT:
	{
		uint8_t maxlength_accepted[4]; 
		uint8_t security_accepted;
		int conf_state;
		gss_qop_t qop_state;
		input_token.length = in.length;
		input_token.value = in.data;
			
		maj_stat = gss_unwrap(&min_stat, 
				      gensec_gssapi_state->gssapi_context, 
				      &input_token,
				      &output_token, 
				      &conf_state,
				      &qop_state);
		if (GSS_ERROR(maj_stat)) {
			DEBUG(1, ("gensec_gssapi_update: GSS UnWrap of SASL protection negotiation failed: %s\n", 
				  gssapi_error_string(out_mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
			return NT_STATUS_ACCESS_DENIED;
		}
			
		if (output_token.length < 4) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		memcpy(maxlength_accepted, output_token.value, 4);
		gss_release_buffer(&min_stat, &output_token);
		
		/* first byte is the proposed security */
		security_accepted = maxlength_accepted[0];
		maxlength_accepted[0] = '\0';

		/* Rest is the proposed max wrap length */
		gensec_gssapi_state->max_wrap_buf_size = MIN(RIVAL(maxlength_accepted, 0), 
							     gensec_gssapi_state->max_wrap_buf_size);

		gensec_gssapi_state->sasl_protection = 0;
		if (security_accepted & NEG_SEAL) {
			if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
				DEBUG(1, ("Remote client wanted seal, but gensec refused\n"));
				return NT_STATUS_ACCESS_DENIED;
			}
			gensec_gssapi_state->sasl_protection |= NEG_SEAL;
		}
		if (security_accepted & NEG_SIGN) {
			if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
				DEBUG(1, ("Remote client wanted sign, but gensec refused\n"));
				return NT_STATUS_ACCESS_DENIED;
			}
			gensec_gssapi_state->sasl_protection |= NEG_SIGN;
		}
		if (security_accepted & NEG_NONE) {
			gensec_gssapi_state->sasl_protection |= NEG_NONE;
		}

		/* quirk:  This changes the value that gensec_have_feature returns, to be that after SASL negotiation */
		gensec_gssapi_state->sasl_state = STAGE_DONE;
		if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
			DEBUG(5, ("SASL/GSSAPI Connection from client will be cryptographically sealed\n"));
		} else if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
			DEBUG(5, ("SASL/GSSAPI Connection from client will be cryptographically signed\n"));
		} else {
			DEBUG(5, ("SASL/GSSAPI Connection from client will have no cryptographic protection\n"));
		}

		*out = data_blob(NULL, 0);
		return NT_STATUS_OK;	
	}
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}
}

static NTSTATUS gensec_gssapi_wrap(struct gensec_security *gensec_security, 
				   TALLOC_CTX *mem_ctx, 
				   const DATA_BLOB *in, 
				   DATA_BLOB *out)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	input_token.length = in->length;
	input_token.value = in->data;

	maj_stat = gss_wrap(&min_stat, 
			    gensec_gssapi_state->gssapi_context, 
			    gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL),
			    GSS_C_QOP_DEFAULT,
			    &input_token,
			    &conf_state,
			    &output_token);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("gensec_gssapi_wrap: GSS Wrap failed: %s\n", 
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	*out = data_blob_talloc(mem_ctx, output_token.value, output_token.length);
	gss_release_buffer(&min_stat, &output_token);

	if (gensec_gssapi_state->sasl) {
		size_t max_wrapped_size = gensec_gssapi_max_wrapped_size(gensec_security);
		if (max_wrapped_size < out->length) {
			DEBUG(1, ("gensec_gssapi_wrap: when wrapped, INPUT data (%u) is grew to be larger than SASL negotiated maximum output size (%u > %u)\n",
				  (unsigned)in->length, 
				  (unsigned)out->length, 
				  (unsigned int)max_wrapped_size));
			return NT_STATUS_INVALID_PARAMETER;
		}
	}
	
	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_unwrap(struct gensec_security *gensec_security, 
				     TALLOC_CTX *mem_ctx, 
				     const DATA_BLOB *in, 
				     DATA_BLOB *out)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	gss_qop_t qop_state;
	input_token.length = in->length;
	input_token.value = in->data;
	
	if (gensec_gssapi_state->sasl) {
		size_t max_wrapped_size = gensec_gssapi_max_wrapped_size(gensec_security);
		if (max_wrapped_size < in->length) {
			DEBUG(1, ("gensec_gssapi_unwrap: WRAPPED data is larger than SASL negotiated maximum size\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
	}
	
	maj_stat = gss_unwrap(&min_stat, 
			      gensec_gssapi_state->gssapi_context, 
			      &input_token,
			      &output_token, 
			      &conf_state,
			      &qop_state);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("gensec_gssapi_unwrap: GSS UnWrap failed: %s\n", 
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	*out = data_blob_talloc(mem_ctx, output_token.value, output_token.length);
	gss_release_buffer(&min_stat, &output_token);
	
	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

/* Find out the maximum input size negotiated on this connection */

static size_t gensec_gssapi_max_input_size(struct gensec_security *gensec_security) 
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	OM_uint32 max_input_size;

	maj_stat = gss_wrap_size_limit(&min_stat, 
				       gensec_gssapi_state->gssapi_context,
				       gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL),
				       GSS_C_QOP_DEFAULT,
				       gensec_gssapi_state->max_wrap_buf_size,
				       &max_input_size);
	if (GSS_ERROR(maj_stat)) {
		TALLOC_CTX *mem_ctx = talloc_new(NULL); 
		DEBUG(1, ("gensec_gssapi_max_input_size: determinaing signature size with gss_wrap_size_limit failed: %s\n", 
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		talloc_free(mem_ctx);
		return 0;
	}

	return max_input_size;
}

/* Find out the maximum output size negotiated on this connection */
static size_t gensec_gssapi_max_wrapped_size(struct gensec_security *gensec_security) 
{
	struct gensec_gssapi_state *gensec_gssapi_state = talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);;
	return gensec_gssapi_state->max_wrap_buf_size;
}

static NTSTATUS gensec_gssapi_seal_packet(struct gensec_security *gensec_security, 
					  TALLOC_CTX *mem_ctx, 
					  uint8_t *data, size_t length, 
					  const uint8_t *whole_pdu, size_t pdu_length, 
					  DATA_BLOB *sig)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	ssize_t sig_length;

	input_token.length = length;
	input_token.value = data;
	
	maj_stat = gss_wrap(&min_stat, 
			    gensec_gssapi_state->gssapi_context,
			    gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL),
			    GSS_C_QOP_DEFAULT,
			    &input_token,
			    &conf_state,
			    &output_token);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("gensec_gssapi_seal_packet: GSS Wrap failed: %s\n", 
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (output_token.length < input_token.length) {
		DEBUG(1, ("gensec_gssapi_seal_packet: GSS Wrap length [%ld] *less* than caller length [%ld]\n", 
			  (long)output_token.length, (long)length));
		return NT_STATUS_INTERNAL_ERROR;
	}
	sig_length = output_token.length - input_token.length;

	memcpy(data, ((uint8_t *)output_token.value) + sig_length, length);
	*sig = data_blob_talloc(mem_ctx, (uint8_t *)output_token.value, sig_length);

	dump_data_pw("gensec_gssapi_seal_packet: sig\n", sig->data, sig->length);
	dump_data_pw("gensec_gssapi_seal_packet: clear\n", data, length);
	dump_data_pw("gensec_gssapi_seal_packet: sealed\n", ((uint8_t *)output_token.value) + sig_length, output_token.length - sig_length);

	gss_release_buffer(&min_stat, &output_token);

	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_unseal_packet(struct gensec_security *gensec_security, 
					    TALLOC_CTX *mem_ctx, 
					    uint8_t *data, size_t length, 
					    const uint8_t *whole_pdu, size_t pdu_length,
					    const DATA_BLOB *sig)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	gss_qop_t qop_state;
	DATA_BLOB in;

	dump_data_pw("gensec_gssapi_unseal_packet: sig\n", sig->data, sig->length);

	in = data_blob_talloc(mem_ctx, NULL, sig->length + length);

	memcpy(in.data, sig->data, sig->length);
	memcpy(in.data + sig->length, data, length);

	input_token.length = in.length;
	input_token.value = in.data;
	
	maj_stat = gss_unwrap(&min_stat, 
			      gensec_gssapi_state->gssapi_context, 
			      &input_token,
			      &output_token, 
			      &conf_state,
			      &qop_state);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("gensec_gssapi_unseal_packet: GSS UnWrap failed: %s\n", 
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (output_token.length != length) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	memcpy(data, output_token.value, length);

	gss_release_buffer(&min_stat, &output_token);
	
	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_sign_packet(struct gensec_security *gensec_security, 
					  TALLOC_CTX *mem_ctx, 
					  const uint8_t *data, size_t length, 
					  const uint8_t *whole_pdu, size_t pdu_length, 
					  DATA_BLOB *sig)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		input_token.length = pdu_length;
		input_token.value = discard_const_p(uint8_t *, whole_pdu);
	} else {
		input_token.length = length;
		input_token.value = discard_const_p(uint8_t *, data);
	}

	maj_stat = gss_get_mic(&min_stat,
			    gensec_gssapi_state->gssapi_context,
			    GSS_C_QOP_DEFAULT,
			    &input_token,
			    &output_token);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("GSS GetMic failed: %s\n",
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	*sig = data_blob_talloc(mem_ctx, (uint8_t *)output_token.value, output_token.length);

	dump_data_pw("gensec_gssapi_seal_packet: sig\n", sig->data, sig->length);

	gss_release_buffer(&min_stat, &output_token);

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gssapi_check_packet(struct gensec_security *gensec_security, 
					   TALLOC_CTX *mem_ctx, 
					   const uint8_t *data, size_t length, 
					   const uint8_t *whole_pdu, size_t pdu_length, 
					   const DATA_BLOB *sig)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token;
	gss_buffer_desc input_message;
	gss_qop_t qop_state;

	dump_data_pw("gensec_gssapi_seal_packet: sig\n", sig->data, sig->length);

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		input_message.length = pdu_length;
		input_message.value = discard_const(whole_pdu);
	} else {
		input_message.length = length;
		input_message.value = discard_const(data);
	}

	input_token.length = sig->length;
	input_token.value = sig->data;

	maj_stat = gss_verify_mic(&min_stat,
			      gensec_gssapi_state->gssapi_context, 
			      &input_message,
			      &input_token,
			      &qop_state);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("GSS VerifyMic failed: %s\n",
			  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

/* Try to figure out what features we actually got on the connection */
static bool gensec_gssapi_have_feature(struct gensec_security *gensec_security, 
				       uint32_t feature) 
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	if (feature & GENSEC_FEATURE_SIGN) {
		/* If we are going GSSAPI SASL, then we honour the second negotiation */
		if (gensec_gssapi_state->sasl 
		    && gensec_gssapi_state->sasl_state == STAGE_DONE) {
			return ((gensec_gssapi_state->sasl_protection & NEG_SIGN) 
				&& (gensec_gssapi_state->got_flags & GSS_C_INTEG_FLAG));
		}
		return gensec_gssapi_state->got_flags & GSS_C_INTEG_FLAG;
	}
	if (feature & GENSEC_FEATURE_SEAL) {
		/* If we are going GSSAPI SASL, then we honour the second negotiation */
		if (gensec_gssapi_state->sasl 
		    && gensec_gssapi_state->sasl_state == STAGE_DONE) {
			return ((gensec_gssapi_state->sasl_protection & NEG_SEAL) 
				 && (gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG));
		}
		return gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG;
	}
	if (feature & GENSEC_FEATURE_SESSION_KEY) {
		/* Only for GSSAPI/Krb5 */
		if (gss_oid_equal(gensec_gssapi_state->gss_oid, gss_mech_krb5)) {
			return true;
		}
	}
	if (feature & GENSEC_FEATURE_DCE_STYLE) {
		return gensec_gssapi_state->got_flags & GSS_C_DCE_STYLE;
	}
	if (feature & GENSEC_FEATURE_NEW_SPNEGO) {
		NTSTATUS status;

		if (!(gensec_gssapi_state->got_flags & GSS_C_INTEG_FLAG)) {
			return false;
		}

		if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "force_new_spnego", false)) {
			return true;
		}
		if (gensec_setting_bool(gensec_security->settings, "gensec_gssapi", "disable_new_spnego", false)) {
			return false;
		}

		status = gensec_gssapi_init_lucid(gensec_gssapi_state);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}

		if (gensec_gssapi_state->lucid->protocol == 1) {
			return true;
		}

		return false;
	}
	/* We can always do async (rather than strict request/reply) packets.  */
	if (feature & GENSEC_FEATURE_ASYNC_REPLIES) {
		return true;
	}
	return false;
}

/*
 * Extract the 'sesssion key' needed by SMB signing and ncacn_np 
 * (for encrypting some passwords).
 * 
 * This breaks all the abstractions, but what do you expect...
 */
static NTSTATUS gensec_gssapi_session_key(struct gensec_security *gensec_security, 
					  DATA_BLOB *session_key) 
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	OM_uint32 maj_stat, min_stat;
	krb5_keyblock *subkey;

	if (gensec_gssapi_state->sasl_state != STAGE_DONE) {
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	if (gensec_gssapi_state->session_key.data) {
		*session_key = gensec_gssapi_state->session_key;
		return NT_STATUS_OK;
	}

	maj_stat = gsskrb5_get_subkey(&min_stat,
				      gensec_gssapi_state->gssapi_context,
				      &subkey);
	if (maj_stat != 0) {
		DEBUG(1, ("NO session key for this mech\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}
	
	DEBUG(10, ("Got KRB5 session key of length %d%s\n",
		   (int)KRB5_KEY_LENGTH(subkey),
		   (gensec_gssapi_state->sasl_state == STAGE_DONE)?" (done)":""));
	*session_key = data_blob_talloc(gensec_gssapi_state,
					KRB5_KEY_DATA(subkey), KRB5_KEY_LENGTH(subkey));
	krb5_free_keyblock(gensec_gssapi_state->smb_krb5_context->krb5_context, subkey);
	gensec_gssapi_state->session_key = *session_key;
	dump_data_pw("KRB5 Session Key:\n", session_key->data, session_key->length);

	return NT_STATUS_OK;
}

/* Get some basic (and authorization) information about the user on
 * this session.  This uses either the PAC (if present) or a local
 * database lookup */
static NTSTATUS gensec_gssapi_session_info(struct gensec_security *gensec_security,
					   struct auth_session_info **_session_info) 
{
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx;
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	struct auth_user_info_dc *user_info_dc = NULL;
	struct auth_session_info *session_info = NULL;
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc pac;
	DATA_BLOB pac_blob;
	struct PAC_SIGNATURE_DATA *pac_srv_sig = NULL;
	struct PAC_SIGNATURE_DATA *pac_kdc_sig = NULL;
	
	if ((gensec_gssapi_state->gss_oid->length != gss_mech_krb5->length)
	    || (memcmp(gensec_gssapi_state->gss_oid->elements, gss_mech_krb5->elements, 
		       gensec_gssapi_state->gss_oid->length) != 0)) {
		DEBUG(1, ("NO session info available for this mech\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
		
	mem_ctx = talloc_named(gensec_gssapi_state, 0, "gensec_gssapi_session_info context"); 
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	maj_stat = gsskrb5_extract_authz_data_from_sec_context(&min_stat, 
							       gensec_gssapi_state->gssapi_context, 
							       KRB5_AUTHDATA_WIN2K_PAC,
							       &pac);
	
	
	if (maj_stat == 0) {
		pac_blob = data_blob_talloc(mem_ctx, pac.value, pac.length);
		gss_release_buffer(&min_stat, &pac);

	} else {
		pac_blob = data_blob(NULL, 0);
	}
	
	/* IF we have the PAC - otherwise we need to get this
	 * data from elsewere - local ldb, or (TODO) lookup of some
	 * kind... 
	 */
	if (pac_blob.length) {
		pac_srv_sig = talloc(mem_ctx, struct PAC_SIGNATURE_DATA);
		if (!pac_srv_sig) {
			talloc_free(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		pac_kdc_sig = talloc(mem_ctx, struct PAC_SIGNATURE_DATA);
		if (!pac_kdc_sig) {
			talloc_free(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		nt_status = kerberos_pac_blob_to_user_info_dc(mem_ctx,
							      pac_blob,
							      gensec_gssapi_state->smb_krb5_context->krb5_context,
							      &user_info_dc,
							      pac_srv_sig,
							      pac_kdc_sig);
		if (!NT_STATUS_IS_OK(nt_status)) {
			talloc_free(mem_ctx);
			return nt_status;
		}
	} else {
		gss_buffer_desc name_token;
		char *principal_string;

		maj_stat = gss_display_name (&min_stat,
					     gensec_gssapi_state->client_name,
					     &name_token,
					     NULL);
		if (GSS_ERROR(maj_stat)) {
			DEBUG(1, ("GSS display_name failed: %s\n", 
				  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
			talloc_free(mem_ctx);
			return NT_STATUS_FOOBAR;
		}
		
		principal_string = talloc_strndup(mem_ctx, 
						  (const char *)name_token.value, 
						  name_token.length);
		
		gss_release_buffer(&min_stat, &name_token);
		
		if (!principal_string) {
			talloc_free(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		if (gensec_security->auth_context && 
		    !gensec_setting_bool(gensec_security->settings, "gensec", "require_pac", false)) {
			DEBUG(1, ("Unable to find PAC, resorting to local user lookup: %s\n",
				  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
			nt_status = gensec_security->auth_context->get_user_info_dc_principal(mem_ctx,
											     gensec_security->auth_context, 
											     principal_string,
											     NULL,
											     &user_info_dc);
			
			if (!NT_STATUS_IS_OK(nt_status)) {
				talloc_free(mem_ctx);
				return nt_status;
			}
		} else {
			DEBUG(1, ("Unable to find PAC in ticket from %s, failing to allow access: %s\n",
				  principal_string,
				  gssapi_error_string(mem_ctx, maj_stat, min_stat, gensec_gssapi_state->gss_oid)));
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/* references the user_info_dc into the session_info */
	nt_status = gensec_generate_session_info(mem_ctx, gensec_security,
						 user_info_dc, &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	nt_status = gensec_gssapi_session_key(gensec_security, &session_info->session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	/* Allow torture tests to check the PAC signatures */
	if (session_info->torture) {
		session_info->torture->pac_srv_sig = talloc_steal(session_info->torture, pac_srv_sig);
		session_info->torture->pac_kdc_sig = talloc_steal(session_info->torture, pac_kdc_sig);
	}

	if (!(gensec_gssapi_state->got_flags & GSS_C_DELEG_FLAG)) {
		DEBUG(10, ("gensec_gssapi: NO delegated credentials supplied by client\n"));
	} else {
		krb5_error_code ret;
		const char *error_string;

		DEBUG(10, ("gensec_gssapi: delegated credentials supplied by client\n"));
		session_info->credentials = cli_credentials_init(session_info);
		if (!session_info->credentials) {
			talloc_free(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		cli_credentials_set_conf(session_info->credentials, gensec_security->settings->lp_ctx);
		/* Just so we don't segfault trying to get at a username */
		cli_credentials_set_anonymous(session_info->credentials);
		
		ret = cli_credentials_set_client_gss_creds(session_info->credentials, 
							   gensec_security->settings->lp_ctx,
							   gensec_gssapi_state->delegated_cred_handle,
							   CRED_SPECIFIED, &error_string);
		if (ret) {
			talloc_free(mem_ctx);
			DEBUG(2,("Failed to get gss creds: %s\n", error_string));
			return NT_STATUS_NO_MEMORY;
		}
		
		/* This credential handle isn't useful for password authentication, so ensure nobody tries to do that */
		cli_credentials_set_kerberos_state(session_info->credentials, CRED_MUST_USE_KERBEROS);

		/* It has been taken from this place... */
		gensec_gssapi_state->delegated_cred_handle = GSS_C_NO_CREDENTIAL;
	}
	talloc_steal(gensec_gssapi_state, session_info);
	talloc_free(mem_ctx);
	*_session_info = session_info;

	return NT_STATUS_OK;
}

static size_t gensec_gssapi_sig_size(struct gensec_security *gensec_security, size_t data_size)
{
	struct gensec_gssapi_state *gensec_gssapi_state
		= talloc_get_type(gensec_security->private_data, struct gensec_gssapi_state);
	NTSTATUS status;

	if (gensec_gssapi_state->sig_size) {
		return gensec_gssapi_state->sig_size;
	}

	if (gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG) {
		gensec_gssapi_state->sig_size = 45;
	} else {
		gensec_gssapi_state->sig_size = 37;
	}

	status = gensec_gssapi_init_lucid(gensec_gssapi_state);
	if (!NT_STATUS_IS_OK(status)) {
		return gensec_gssapi_state->sig_size;
	}

	if (gensec_gssapi_state->lucid->protocol == 1) {
		if (gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG) {
			/*
			 * TODO: windows uses 76 here, but we don't know
			 *       gss_wrap works with aes keys yet
			 */
			gensec_gssapi_state->sig_size = 76;
		} else {
			gensec_gssapi_state->sig_size = 28;
		}
	} else if (gensec_gssapi_state->lucid->protocol == 0) {
		switch (gensec_gssapi_state->lucid->rfc1964_kd.ctx_key.type) {
		case KEYTYPE_DES:
		case KEYTYPE_ARCFOUR:
		case KEYTYPE_ARCFOUR_56:
			if (gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG) {
				gensec_gssapi_state->sig_size = 45;
			} else {
				gensec_gssapi_state->sig_size = 37;
			}
			break;
		case KEYTYPE_DES3:
			if (gensec_gssapi_state->got_flags & GSS_C_CONF_FLAG) {
				gensec_gssapi_state->sig_size = 57;
			} else {
				gensec_gssapi_state->sig_size = 49;
			}
			break;
		}
	}

	return gensec_gssapi_state->sig_size;
}

static const char *gensec_gssapi_krb5_oids[] = { 
	GENSEC_OID_KERBEROS5_OLD,
	GENSEC_OID_KERBEROS5,
	NULL 
};

static const char *gensec_gssapi_spnego_oids[] = { 
	GENSEC_OID_SPNEGO,
	NULL 
};

/* As a server, this could in theory accept any GSSAPI mech */
static const struct gensec_security_ops gensec_gssapi_spnego_security_ops = {
	.name		= "gssapi_spnego",
	.sasl_name	= "GSS-SPNEGO",
	.auth_type	= DCERPC_AUTH_TYPE_SPNEGO,
	.oid            = gensec_gssapi_spnego_oids,
	.client_start   = gensec_gssapi_client_start,
	.server_start   = gensec_gssapi_server_start,
	.magic  	= gensec_gssapi_magic,
	.update 	= gensec_gssapi_update,
	.session_key	= gensec_gssapi_session_key,
	.session_info	= gensec_gssapi_session_info,
	.sign_packet	= gensec_gssapi_sign_packet,
	.check_packet	= gensec_gssapi_check_packet,
	.seal_packet	= gensec_gssapi_seal_packet,
	.unseal_packet	= gensec_gssapi_unseal_packet,
	.wrap           = gensec_gssapi_wrap,
	.unwrap         = gensec_gssapi_unwrap,
	.have_feature   = gensec_gssapi_have_feature,
	.enabled        = false,
	.kerberos       = true,
	.priority       = GENSEC_GSSAPI
};

/* As a server, this could in theory accept any GSSAPI mech */
static const struct gensec_security_ops gensec_gssapi_krb5_security_ops = {
	.name		= "gssapi_krb5",
	.auth_type	= DCERPC_AUTH_TYPE_KRB5,
	.oid            = gensec_gssapi_krb5_oids,
	.client_start   = gensec_gssapi_client_start,
	.server_start   = gensec_gssapi_server_start,
	.magic  	= gensec_gssapi_magic,
	.update 	= gensec_gssapi_update,
	.session_key	= gensec_gssapi_session_key,
	.session_info	= gensec_gssapi_session_info,
	.sig_size	= gensec_gssapi_sig_size,
	.sign_packet	= gensec_gssapi_sign_packet,
	.check_packet	= gensec_gssapi_check_packet,
	.seal_packet	= gensec_gssapi_seal_packet,
	.unseal_packet	= gensec_gssapi_unseal_packet,
	.wrap           = gensec_gssapi_wrap,
	.unwrap         = gensec_gssapi_unwrap,
	.have_feature   = gensec_gssapi_have_feature,
	.enabled        = true,
	.kerberos       = true,
	.priority       = GENSEC_GSSAPI
};

/* As a server, this could in theory accept any GSSAPI mech */
static const struct gensec_security_ops gensec_gssapi_sasl_krb5_security_ops = {
	.name		  = "gssapi_krb5_sasl",
	.sasl_name        = "GSSAPI",
	.client_start     = gensec_gssapi_sasl_client_start,
	.server_start     = gensec_gssapi_sasl_server_start,
	.update 	  = gensec_gssapi_update,
	.session_key	  = gensec_gssapi_session_key,
	.session_info	  = gensec_gssapi_session_info,
	.max_input_size	  = gensec_gssapi_max_input_size,
	.max_wrapped_size = gensec_gssapi_max_wrapped_size,
	.wrap             = gensec_gssapi_wrap,
	.unwrap           = gensec_gssapi_unwrap,
	.have_feature     = gensec_gssapi_have_feature,
	.enabled          = true,
	.kerberos         = true,
	.priority         = GENSEC_GSSAPI
};

_PUBLIC_ NTSTATUS gensec_gssapi_init(void)
{
	NTSTATUS ret;

	ret = gensec_register(&gensec_gssapi_spnego_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_gssapi_spnego_security_ops.name));
		return ret;
	}

	ret = gensec_register(&gensec_gssapi_krb5_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_gssapi_krb5_security_ops.name));
		return ret;
	}

	ret = gensec_register(&gensec_gssapi_sasl_krb5_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_gssapi_sasl_krb5_security_ops.name));
		return ret;
	}

	return ret;
}
