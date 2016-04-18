/* 
   Unix SMB/CIFS implementation.
   ads sasl code
   Copyright (C) Andrew Tridgell 2001
   
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
#include "../libcli/auth/spnego.h"
#include "../libcli/auth/ntlmssp.h"
#include "ads.h"
#include "smb_krb5.h"

#ifdef HAVE_LDAP

static ADS_STATUS ads_sasl_ntlmssp_wrap(ADS_STRUCT *ads, uint8 *buf, uint32 len)
{
	struct ntlmssp_state *ntlmssp_state =
		(struct ntlmssp_state *)ads->ldap.wrap_private_data;
	ADS_STATUS status;
	NTSTATUS nt_status;
	DATA_BLOB sig;
	TALLOC_CTX *frame;
	uint8 *dptr = ads->ldap.out.buf + (4 + NTLMSSP_SIG_SIZE);

	frame = talloc_stackframe();
	/* copy the data to the right location */
	memcpy(dptr, buf, len);

	/* create the signature and may encrypt the data */
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		nt_status = ntlmssp_seal_packet(ntlmssp_state,
						frame,
						dptr, len,
						dptr, len,
						&sig);
	} else {
		nt_status = ntlmssp_sign_packet(ntlmssp_state,
						frame,
						dptr, len,
						dptr, len,
						&sig);
	}
	status = ADS_ERROR_NT(nt_status);
	if (!ADS_ERR_OK(status)) return status;

	/* copy the signature to the right location */
	memcpy(ads->ldap.out.buf + 4,
	       sig.data, NTLMSSP_SIG_SIZE);

	TALLOC_FREE(frame);

	/* set how many bytes must be written to the underlying socket */
	ads->ldap.out.left = 4 + NTLMSSP_SIG_SIZE + len;

	return ADS_SUCCESS;
}

static ADS_STATUS ads_sasl_ntlmssp_unwrap(ADS_STRUCT *ads)
{
	struct ntlmssp_state *ntlmssp_state =
		(struct ntlmssp_state *)ads->ldap.wrap_private_data;
	ADS_STATUS status;
	NTSTATUS nt_status;
	DATA_BLOB sig;
	uint8 *dptr = ads->ldap.in.buf + (4 + NTLMSSP_SIG_SIZE);
	uint32 dlen = ads->ldap.in.ofs - (4 + NTLMSSP_SIG_SIZE);

	/* wrap the signature into a DATA_BLOB */
	sig = data_blob_const(ads->ldap.in.buf + 4, NTLMSSP_SIG_SIZE);

	/* verify the signature and maybe decrypt the data */
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		nt_status = ntlmssp_unseal_packet(ntlmssp_state,
						  dptr, dlen,
						  dptr, dlen,
						  &sig);
	} else {
		nt_status = ntlmssp_check_packet(ntlmssp_state,
						 dptr, dlen,
						 dptr, dlen,
						 &sig);
	}
	status = ADS_ERROR_NT(nt_status);
	if (!ADS_ERR_OK(status)) return status;

	/* set the amount of bytes for the upper layer and set the ofs to the data */
	ads->ldap.in.left	= dlen;
	ads->ldap.in.ofs	= 4 + NTLMSSP_SIG_SIZE;

	return ADS_SUCCESS;
}

static void ads_sasl_ntlmssp_disconnect(ADS_STRUCT *ads)
{
	struct ntlmssp_state *ntlmssp_state =
		(struct ntlmssp_state *)ads->ldap.wrap_private_data;

	TALLOC_FREE(ntlmssp_state);

	ads->ldap.wrap_ops = NULL;
	ads->ldap.wrap_private_data = NULL;
}

static const struct ads_saslwrap_ops ads_sasl_ntlmssp_ops = {
	.name		= "ntlmssp",
	.wrap		= ads_sasl_ntlmssp_wrap,
	.unwrap		= ads_sasl_ntlmssp_unwrap,
	.disconnect	= ads_sasl_ntlmssp_disconnect
};

/* 
   perform a LDAP/SASL/SPNEGO/NTLMSSP bind (just how many layers can
   we fit on one socket??)
*/
static ADS_STATUS ads_sasl_spnego_ntlmssp_bind(ADS_STRUCT *ads)
{
	DATA_BLOB msg1 = data_blob_null;
	DATA_BLOB blob = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;
	struct berval cred, *scred = NULL;
	int rc;
	NTSTATUS nt_status;
	ADS_STATUS status;
	int turn = 1;
	uint32 features = 0;

	struct ntlmssp_state *ntlmssp_state;

	nt_status = ntlmssp_client_start(NULL,
					 global_myname(),
					 lp_workgroup(),
					 lp_client_ntlmv2_auth(),
					 &ntlmssp_state);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return ADS_ERROR_NT(nt_status);
	}
	ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_SIGN;

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(ntlmssp_state, ads->auth.user_name))) {
		return ADS_ERROR_NT(nt_status);
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(ntlmssp_state, ads->auth.realm))) {
		return ADS_ERROR_NT(nt_status);
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_password(ntlmssp_state, ads->auth.password))) {
		return ADS_ERROR_NT(nt_status);
	}

	switch (ads->ldap.wrap_type) {
	case ADS_SASLWRAP_TYPE_SEAL:
		features = NTLMSSP_FEATURE_SIGN | NTLMSSP_FEATURE_SEAL;
		break;
	case ADS_SASLWRAP_TYPE_SIGN:
		if (ads->auth.flags & ADS_AUTH_SASL_FORCE) {
			features = NTLMSSP_FEATURE_SIGN;
		} else {
			/*
			 * windows servers are broken with sign only,
			 * so we need to use seal here too
			 */
			features = NTLMSSP_FEATURE_SIGN | NTLMSSP_FEATURE_SEAL;
			ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SEAL;
		}
		break;
	case ADS_SASLWRAP_TYPE_PLAIN:
		break;
	}

	ntlmssp_want_feature(ntlmssp_state, features);

	blob_in = data_blob_null;

	do {
		nt_status = ntlmssp_update(ntlmssp_state, 
					   blob_in, &blob_out);
		data_blob_free(&blob_in);
		if ((NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) 
		     || NT_STATUS_IS_OK(nt_status))
		    && blob_out.length) {
			if (turn == 1) {
				const char *OIDs_ntlm[] = {OID_NTLMSSP, NULL};
				/* and wrap it in a SPNEGO wrapper */
				msg1 = spnego_gen_negTokenInit(talloc_tos(),
						OIDs_ntlm, &blob_out, NULL);
			} else {
				/* wrap it in SPNEGO */
				msg1 = spnego_gen_auth(talloc_tos(), blob_out);
			}

			data_blob_free(&blob_out);

			cred.bv_val = (char *)msg1.data;
			cred.bv_len = msg1.length;
			scred = NULL;
			rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSS-SPNEGO", &cred, NULL, NULL, &scred);
			data_blob_free(&msg1);
			if ((rc != LDAP_SASL_BIND_IN_PROGRESS) && (rc != 0)) {
				if (scred) {
					ber_bvfree(scred);
				}

				TALLOC_FREE(ntlmssp_state);
				return ADS_ERROR(rc);
			}
			if (scred) {
				blob = data_blob(scred->bv_val, scred->bv_len);
				ber_bvfree(scred);
			} else {
				blob = data_blob_null;
			}

		} else {

			TALLOC_FREE(ntlmssp_state);
			data_blob_free(&blob_out);
			return ADS_ERROR_NT(nt_status);
		}
		
		if ((turn == 1) && 
		    (rc == LDAP_SASL_BIND_IN_PROGRESS)) {
			DATA_BLOB tmp_blob = data_blob_null;
			/* the server might give us back two challenges */
			if (!spnego_parse_challenge(talloc_tos(), blob, &blob_in, 
						    &tmp_blob)) {

				TALLOC_FREE(ntlmssp_state);
				data_blob_free(&blob);
				DEBUG(3,("Failed to parse challenges\n"));
				return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
			}
			data_blob_free(&tmp_blob);
		} else if (rc == LDAP_SASL_BIND_IN_PROGRESS) {
			if (!spnego_parse_auth_response(talloc_tos(), blob, nt_status, OID_NTLMSSP, 
							&blob_in)) {

				TALLOC_FREE(ntlmssp_state);
				data_blob_free(&blob);
				DEBUG(3,("Failed to parse auth response\n"));
				return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
			}
		}
		data_blob_free(&blob);
		data_blob_free(&blob_out);
		turn++;
	} while (rc == LDAP_SASL_BIND_IN_PROGRESS && !NT_STATUS_IS_OK(nt_status));
	
	/* we have a reference conter on ntlmssp_state, if we are signing
	   then the state will be kept by the signing engine */

	if (ads->ldap.wrap_type >= ADS_SASLWRAP_TYPE_SEAL) {
		bool ok;

		ok = ntlmssp_have_feature(ntlmssp_state,
					  NTLMSSP_FEATURE_SEAL);
		if (!ok) {
			DEBUG(0,("The ntlmssp feature sealing request, but unavailable\n"));
			TALLOC_FREE(ntlmssp_state);
			return ADS_ERROR_NT(NT_STATUS_INVALID_NETWORK_RESPONSE);
		}

		ok = ntlmssp_have_feature(ntlmssp_state,
					  NTLMSSP_FEATURE_SIGN);
		if (!ok) {
			DEBUG(0,("The ntlmssp feature signing request, but unavailable\n"));
			TALLOC_FREE(ntlmssp_state);
			return ADS_ERROR_NT(NT_STATUS_INVALID_NETWORK_RESPONSE);
		}

	} else if (ads->ldap.wrap_type >= ADS_SASLWRAP_TYPE_SIGN) {
		bool ok;

		ok = ntlmssp_have_feature(ntlmssp_state,
					  NTLMSSP_FEATURE_SIGN);
		if (!ok) {
			DEBUG(0,("The gensec feature signing request, but unavailable\n"));
			TALLOC_FREE(ntlmssp_state);
			return ADS_ERROR_NT(NT_STATUS_INVALID_NETWORK_RESPONSE);
		}
	}

	if (ads->ldap.wrap_type > ADS_SASLWRAP_TYPE_PLAIN) {
		ads->ldap.out.max_unwrapped = ADS_SASL_WRAPPING_OUT_MAX_WRAPPED - NTLMSSP_SIG_SIZE;
		ads->ldap.out.sig_size = NTLMSSP_SIG_SIZE;
		ads->ldap.in.min_wrapped = ads->ldap.out.sig_size;
		ads->ldap.in.max_wrapped = ADS_SASL_WRAPPING_IN_MAX_WRAPPED;
		status = ads_setup_sasl_wrapping(ads, &ads_sasl_ntlmssp_ops, ntlmssp_state);
		if (!ADS_ERR_OK(status)) {
			DEBUG(0, ("ads_setup_sasl_wrapping() failed: %s\n",
				ads_errstr(status)));
			TALLOC_FREE(ntlmssp_state);
			return status;
		}
	} else {
		TALLOC_FREE(ntlmssp_state);
	}

	return ADS_ERROR(rc);
}

#ifdef HAVE_GSSAPI
static ADS_STATUS ads_sasl_gssapi_wrap(ADS_STRUCT *ads, uint8 *buf, uint32 len)
{
	gss_ctx_id_t context_handle = (gss_ctx_id_t)ads->ldap.wrap_private_data;
	ADS_STATUS status;
	int gss_rc;
	uint32 minor_status;
	gss_buffer_desc unwrapped, wrapped;
	int conf_req_flag, conf_state;

	unwrapped.value		= buf;
	unwrapped.length	= len;

	/* for now request sign and seal */
	conf_req_flag	= (ads->ldap.wrap_type == ADS_SASLWRAP_TYPE_SEAL);

	gss_rc = gss_wrap(&minor_status, context_handle,
			  conf_req_flag, GSS_C_QOP_DEFAULT,
			  &unwrapped, &conf_state,
			  &wrapped);
	status = ADS_ERROR_GSS(gss_rc, minor_status);
	if (!ADS_ERR_OK(status)) return status;

	if (conf_req_flag && conf_state == 0) {
		return ADS_ERROR_NT(NT_STATUS_ACCESS_DENIED);
	}

	if ((ads->ldap.out.size - 4) < wrapped.length) {
		return ADS_ERROR_NT(NT_STATUS_INTERNAL_ERROR);
	}

	/* copy the wrapped blob to the right location */
	memcpy(ads->ldap.out.buf + 4, wrapped.value, wrapped.length);

	/* set how many bytes must be written to the underlying socket */
	ads->ldap.out.left = 4 + wrapped.length;

	gss_release_buffer(&minor_status, &wrapped);

	return ADS_SUCCESS;
}

static ADS_STATUS ads_sasl_gssapi_unwrap(ADS_STRUCT *ads)
{
	gss_ctx_id_t context_handle = (gss_ctx_id_t)ads->ldap.wrap_private_data;
	ADS_STATUS status;
	int gss_rc;
	uint32 minor_status;
	gss_buffer_desc unwrapped, wrapped;
	int conf_state;

	wrapped.value	= ads->ldap.in.buf + 4;
	wrapped.length	= ads->ldap.in.ofs - 4;

	gss_rc = gss_unwrap(&minor_status, context_handle,
			    &wrapped, &unwrapped,
			    &conf_state, GSS_C_QOP_DEFAULT);
	status = ADS_ERROR_GSS(gss_rc, minor_status);
	if (!ADS_ERR_OK(status)) return status;

	if (ads->ldap.wrap_type == ADS_SASLWRAP_TYPE_SEAL && conf_state == 0) {
		return ADS_ERROR_NT(NT_STATUS_ACCESS_DENIED);
	}

	if (wrapped.length < unwrapped.length) {
		return ADS_ERROR_NT(NT_STATUS_INTERNAL_ERROR);
	}

	/* copy the wrapped blob to the right location */
	memcpy(ads->ldap.in.buf + 4, unwrapped.value, unwrapped.length);

	/* set how many bytes must be written to the underlying socket */
	ads->ldap.in.left	= unwrapped.length;
	ads->ldap.in.ofs	= 4;

	gss_release_buffer(&minor_status, &unwrapped);

	return ADS_SUCCESS;
}

static void ads_sasl_gssapi_disconnect(ADS_STRUCT *ads)
{
	gss_ctx_id_t context_handle = (gss_ctx_id_t)ads->ldap.wrap_private_data;
	uint32 minor_status;

	gss_delete_sec_context(&minor_status, &context_handle, GSS_C_NO_BUFFER);

	ads->ldap.wrap_ops = NULL;
	ads->ldap.wrap_private_data = NULL;
}

static const struct ads_saslwrap_ops ads_sasl_gssapi_ops = {
	.name		= "gssapi",
	.wrap		= ads_sasl_gssapi_wrap,
	.unwrap		= ads_sasl_gssapi_unwrap,
	.disconnect	= ads_sasl_gssapi_disconnect
};

/* 
   perform a LDAP/SASL/SPNEGO/GSSKRB5 bind
*/
static ADS_STATUS ads_sasl_spnego_gsskrb5_bind(ADS_STRUCT *ads, const gss_name_t serv_name)
{
	ADS_STATUS status;
	bool ok;
	uint32 minor_status;
	int gss_rc, rc;
	gss_OID_desc krb5_mech_type =
	{9, CONST_DISCARD(char *, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02") };
	gss_OID mech_type = &krb5_mech_type;
	gss_OID actual_mech_type = GSS_C_NULL_OID;
	const char *spnego_mechs[] = {OID_KERBEROS5_OLD, OID_KERBEROS5, OID_NTLMSSP, NULL};
	gss_ctx_id_t context_handle = GSS_C_NO_CONTEXT;
	gss_buffer_desc input_token, output_token;
	uint32 req_flags, ret_flags;
	uint32 req_tmp, ret_tmp;
	DATA_BLOB unwrapped;
	DATA_BLOB wrapped;
	struct berval cred, *scred = NULL;

	input_token.value = NULL;
	input_token.length = 0;

	req_flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG;
	switch (ads->ldap.wrap_type) {
	case ADS_SASLWRAP_TYPE_SEAL:
		req_flags |= GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG;
		break;
	case ADS_SASLWRAP_TYPE_SIGN:
		req_flags |= GSS_C_INTEG_FLAG;
		break;
	case ADS_SASLWRAP_TYPE_PLAIN:
		break;
	}

	/* Note: here we explicit ask for the krb5 mech_type */
	gss_rc = gss_init_sec_context(&minor_status,
				      GSS_C_NO_CREDENTIAL,
				      &context_handle,
				      serv_name,
				      mech_type,
				      req_flags,
				      0,
				      NULL,
				      &input_token,
				      &actual_mech_type,
				      &output_token,
				      &ret_flags,
				      NULL);
	if (gss_rc && gss_rc != GSS_S_CONTINUE_NEEDED) {
		status = ADS_ERROR_GSS(gss_rc, minor_status);
		goto failed;
	}

	/*
	 * As some gssapi krb5 mech implementations
	 * automaticly add GSS_C_INTEG_FLAG and GSS_C_CONF_FLAG
	 * to req_flags internaly, it's not possible to
	 * use plain or signing only connection via
	 * the gssapi interface.
	 *
	 * Because of this we need to check it the ret_flags
	 * has more flags as req_flags and correct the value
	 * of ads->ldap.wrap_type.
	 *
	 * I ads->auth.flags has ADS_AUTH_SASL_FORCE
	 * we need to give an error.
	 */
	req_tmp = req_flags & (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG);
	ret_tmp = ret_flags & (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG);

	if (req_tmp == ret_tmp) {
		/* everythings fine... */

	} else if (req_flags & GSS_C_CONF_FLAG) {
		/*
		 * here we wanted sealing but didn't got it
		 * from the gssapi library
		 */
		status = ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
		goto failed;

	} else if ((req_flags & GSS_C_INTEG_FLAG) &&
		   !(ret_flags & GSS_C_INTEG_FLAG)) {
		/*
		 * here we wanted siging but didn't got it
		 * from the gssapi library
		 */
		status = ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
		goto failed;

	} else if (ret_flags & GSS_C_CONF_FLAG) {
		/*
		 * here we didn't want sealing
		 * but the gssapi library forces it
		 * so correct the needed wrap_type if
		 * the caller didn't forced siging only
		 */
		if (ads->auth.flags & ADS_AUTH_SASL_FORCE) {
			status = ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
			goto failed;
		}

		ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SEAL;
		req_flags = ret_flags;

	} else if (ret_flags & GSS_C_INTEG_FLAG) {
		/*
		 * here we didn't want signing
		 * but the gssapi library forces it
		 * so correct the needed wrap_type if
		 * the caller didn't forced plain
		 */
		if (ads->auth.flags & ADS_AUTH_SASL_FORCE) {
			status = ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
			goto failed;
		}

		ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SIGN;
		req_flags = ret_flags;
	} else {
		/*
		 * This could (should?) not happen
		 */
		status = ADS_ERROR_NT(NT_STATUS_INTERNAL_ERROR);
		goto failed;
	
	}

	/* and wrap that in a shiny SPNEGO wrapper */
	unwrapped = data_blob_const(output_token.value, output_token.length);
	wrapped = spnego_gen_negTokenInit(talloc_tos(),
			spnego_mechs, &unwrapped, NULL);
	gss_release_buffer(&minor_status, &output_token);
	if (unwrapped.length > wrapped.length) {
		status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
		goto failed;
	}

	cred.bv_val = (char *)wrapped.data;
	cred.bv_len = wrapped.length;

	rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSS-SPNEGO", &cred, NULL, NULL, 
			      &scred);
	data_blob_free(&wrapped);
	if (rc != LDAP_SUCCESS) {
		status = ADS_ERROR(rc);
		goto failed;
	}

	if (scred) {
		wrapped = data_blob_const(scred->bv_val, scred->bv_len);
	} else {
		wrapped = data_blob_null;
	}

	ok = spnego_parse_auth_response(talloc_tos(), wrapped, NT_STATUS_OK,
					OID_KERBEROS5_OLD,
					&unwrapped);
	if (scred) ber_bvfree(scred);
	if (!ok) {
		status = ADS_ERROR_NT(NT_STATUS_INVALID_NETWORK_RESPONSE);
		goto failed;
	}

	input_token.value	= unwrapped.data;
	input_token.length	= unwrapped.length;

	/* 
	 * As we asked for mutal authentication
	 * we need to pass the servers response
	 * to gssapi
	 */
	gss_rc = gss_init_sec_context(&minor_status,
				      GSS_C_NO_CREDENTIAL,
				      &context_handle,
				      serv_name,
				      mech_type,
				      req_flags,
				      0,
				      NULL,
				      &input_token,
				      &actual_mech_type,
				      &output_token,
				      &ret_flags,
				      NULL);
	data_blob_free(&unwrapped);
	if (gss_rc) {
		status = ADS_ERROR_GSS(gss_rc, minor_status);
		goto failed;
	}

	gss_release_buffer(&minor_status, &output_token);

	/*
	 * If we the sign and seal options
	 * doesn't match after getting the response
	 * from the server, we don't want to use the connection
	 */
	req_tmp = req_flags & (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG);
	ret_tmp = ret_flags & (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG);

	if (req_tmp != ret_tmp) {
		/* everythings fine... */
		status = ADS_ERROR_NT(NT_STATUS_INVALID_NETWORK_RESPONSE);
		goto failed;
	}

	if (ads->ldap.wrap_type > ADS_SASLWRAP_TYPE_PLAIN) {
		uint32 max_msg_size = ADS_SASL_WRAPPING_OUT_MAX_WRAPPED;

		gss_rc = gss_wrap_size_limit(&minor_status, context_handle,
					     (ads->ldap.wrap_type == ADS_SASLWRAP_TYPE_SEAL),
					     GSS_C_QOP_DEFAULT,
					     max_msg_size, &ads->ldap.out.max_unwrapped);
		if (gss_rc) {
			status = ADS_ERROR_GSS(gss_rc, minor_status);
			goto failed;
		}

		ads->ldap.out.sig_size = max_msg_size - ads->ldap.out.max_unwrapped;
		ads->ldap.in.min_wrapped = 0x2C; /* taken from a capture with LDAP unbind */
		ads->ldap.in.max_wrapped = max_msg_size;
		status = ads_setup_sasl_wrapping(ads, &ads_sasl_gssapi_ops, context_handle);
		if (!ADS_ERR_OK(status)) {
			DEBUG(0, ("ads_setup_sasl_wrapping() failed: %s\n",
				ads_errstr(status)));
			goto failed;
		}
		/* make sure we don't free context_handle */
		context_handle = GSS_C_NO_CONTEXT;
	}

	status = ADS_SUCCESS;

failed:
	if (context_handle != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&minor_status, &context_handle, GSS_C_NO_BUFFER);
	return status;
}

#endif /* HAVE_GSSAPI */

#ifdef HAVE_KRB5
struct ads_service_principal {
	 char *string;
#ifdef HAVE_GSSAPI
	 gss_name_t name;
#endif
};

static void ads_free_service_principal(struct ads_service_principal *p)
{
	SAFE_FREE(p->string);

#ifdef HAVE_GSSAPI
	if (p->name) {
		uint32 minor_status;
		gss_release_name(&minor_status, &p->name);
	}
#endif
	ZERO_STRUCTP(p);
}


static ADS_STATUS ads_guess_service_principal(ADS_STRUCT *ads,
					      char **returned_principal)
{
	char *princ = NULL;

	if (ads->server.realm && ads->server.ldap_server) {
		char *server, *server_realm;

		server = SMB_STRDUP(ads->server.ldap_server);
		server_realm = SMB_STRDUP(ads->server.realm);

		if (!server || !server_realm) {
			SAFE_FREE(server);
			SAFE_FREE(server_realm);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		strlower_m(server);
		strupper_m(server_realm);
		if (asprintf(&princ, "ldap/%s@%s", server, server_realm) == -1) {
			SAFE_FREE(server);
			SAFE_FREE(server_realm);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		SAFE_FREE(server);
		SAFE_FREE(server_realm);

		if (!princ) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	} else if (ads->config.realm && ads->config.ldap_server_name) {
		char *server, *server_realm;

		server = SMB_STRDUP(ads->config.ldap_server_name);
		server_realm = SMB_STRDUP(ads->config.realm);

		if (!server || !server_realm) {
			SAFE_FREE(server);
			SAFE_FREE(server_realm);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		strlower_m(server);
		strupper_m(server_realm);
		if (asprintf(&princ, "ldap/%s@%s", server, server_realm) == -1) {
			SAFE_FREE(server);
			SAFE_FREE(server_realm);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		SAFE_FREE(server);
		SAFE_FREE(server_realm);

		if (!princ) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	if (!princ) {
		return ADS_ERROR(LDAP_PARAM_ERROR);
	}

	*returned_principal = princ;

	return ADS_SUCCESS;
}

static ADS_STATUS ads_generate_service_principal(ADS_STRUCT *ads,
						 const char *given_principal,
						 struct ads_service_principal *p)
{
	ADS_STATUS status;
#ifdef HAVE_GSSAPI
	gss_buffer_desc input_name;
	/* GSS_KRB5_NT_PRINCIPAL_NAME */
	gss_OID_desc nt_principal =
	{10, CONST_DISCARD(char *, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01")};
	uint32 minor_status;
	int gss_rc;
#endif

	ZERO_STRUCTP(p);

	/* I've seen a child Windows 2000 domain not send
	   the principal name back in the first round of
	   the SASL bind reply.  So we guess based on server
	   name and realm.  --jerry  */
	/* Also try best guess when we get the w2k8 ignore principal
	   back, or when we are configured to ignore it - gd,
	   abartlet */

	if (!lp_client_use_spnego_principal() ||
	    !given_principal ||
	    strequal(given_principal, ADS_IGNORE_PRINCIPAL)) {

		status = ads_guess_service_principal(ads, &p->string);
		if (!ADS_ERR_OK(status)) {
			return status;
		}
	} else {
		p->string = SMB_STRDUP(given_principal);
		if (!p->string) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

#ifdef HAVE_GSSAPI
	input_name.value = p->string;
	input_name.length = strlen(p->string);

	gss_rc = gss_import_name(&minor_status, &input_name, &nt_principal, &p->name);
	if (gss_rc) {
		ads_free_service_principal(p);
		return ADS_ERROR_GSS(gss_rc, minor_status);
	}
#endif

	return ADS_SUCCESS;
}

/* 
   perform a LDAP/SASL/SPNEGO/KRB5 bind
*/
static ADS_STATUS ads_sasl_spnego_rawkrb5_bind(ADS_STRUCT *ads, const char *principal)
{
	DATA_BLOB blob = data_blob_null;
	struct berval cred, *scred = NULL;
	DATA_BLOB session_key = data_blob_null;
	int rc;

	if (ads->ldap.wrap_type > ADS_SASLWRAP_TYPE_PLAIN) {
		return ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
	}

	rc = spnego_gen_krb5_negTokenInit(talloc_tos(), principal,
				     ads->auth.time_offset, &blob, &session_key, 0,
				     &ads->auth.tgs_expire);

	if (rc) {
		return ADS_ERROR_KRB5(rc);
	}

	/* now send the auth packet and we should be done */
	cred.bv_val = (char *)blob.data;
	cred.bv_len = blob.length;

	rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSS-SPNEGO", &cred, NULL, NULL, &scred);

	data_blob_free(&blob);
	data_blob_free(&session_key);
	if(scred)
		ber_bvfree(scred);

	return ADS_ERROR(rc);
}

static ADS_STATUS ads_sasl_spnego_krb5_bind(ADS_STRUCT *ads,
					    struct ads_service_principal *p)
{
#ifdef HAVE_GSSAPI
	/*
	 * we only use the gsskrb5 based implementation
	 * when sasl sign or seal is requested.
	 *
	 * This has the following reasons:
	 * - it's likely that the gssapi krb5 mech implementation
	 *   doesn't support to negotiate plain connections
	 * - the ads_sasl_spnego_rawkrb5_bind is more robust
	 *   against clock skew errors
	 */
	if (ads->ldap.wrap_type > ADS_SASLWRAP_TYPE_PLAIN) {
		return ads_sasl_spnego_gsskrb5_bind(ads, p->name);
	}
#endif
	return ads_sasl_spnego_rawkrb5_bind(ads, p->string);
}
#endif /* HAVE_KRB5 */

/* 
   this performs a SASL/SPNEGO bind
*/
static ADS_STATUS ads_sasl_spnego_bind(ADS_STRUCT *ads)
{
	struct berval *scred=NULL;
	int rc, i;
	ADS_STATUS status;
	DATA_BLOB blob;
	char *given_principal = NULL;
	char *OIDs[ASN1_MAX_OIDS];
#ifdef HAVE_KRB5
	bool got_kerberos_mechanism = False;
#endif

	rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSS-SPNEGO", NULL, NULL, NULL, &scred);

	if (rc != LDAP_SASL_BIND_IN_PROGRESS) {
		status = ADS_ERROR(rc);
		goto failed;
	}

	blob = data_blob(scred->bv_val, scred->bv_len);

	ber_bvfree(scred);

#if 0
	file_save("sasl_spnego.dat", blob.data, blob.length);
#endif

	/* the server sent us the first part of the SPNEGO exchange in the negprot 
	   reply */
	if (!spnego_parse_negTokenInit(talloc_tos(), blob, OIDs, &given_principal, NULL) ||
			OIDs[0] == NULL) {
		data_blob_free(&blob);
		status = ADS_ERROR(LDAP_OPERATIONS_ERROR);
		goto failed;
	}
	data_blob_free(&blob);

	/* make sure the server understands kerberos */
	for (i=0;OIDs[i];i++) {
		DEBUG(3,("ads_sasl_spnego_bind: got OID=%s\n", OIDs[i]));
#ifdef HAVE_KRB5
		if (strcmp(OIDs[i], OID_KERBEROS5_OLD) == 0 ||
		    strcmp(OIDs[i], OID_KERBEROS5) == 0) {
			got_kerberos_mechanism = True;
		}
#endif
		talloc_free(OIDs[i]);
	}
	DEBUG(3,("ads_sasl_spnego_bind: got server principal name = %s\n", given_principal));

#ifdef HAVE_KRB5
	if (!(ads->auth.flags & ADS_AUTH_DISABLE_KERBEROS) &&
	    got_kerberos_mechanism) 
	{
		struct ads_service_principal p;

		status = ads_generate_service_principal(ads, given_principal, &p);
		TALLOC_FREE(given_principal);
		if (!ADS_ERR_OK(status)) {
			return status;
		}

		status = ads_sasl_spnego_krb5_bind(ads, &p);
		if (ADS_ERR_OK(status)) {
			ads_free_service_principal(&p);
			return status;
		}

		DEBUG(10,("ads_sasl_spnego_krb5_bind failed with: %s, "
			  "calling kinit\n", ads_errstr(status)));

		status = ADS_ERROR_KRB5(ads_kinit_password(ads)); 

		if (ADS_ERR_OK(status)) {
			status = ads_sasl_spnego_krb5_bind(ads, &p);
			if (!ADS_ERR_OK(status)) {
				DEBUG(0,("kinit succeeded but "
					"ads_sasl_spnego_krb5_bind failed: %s\n",
					ads_errstr(status)));
			}
		}

		ads_free_service_principal(&p);

		/* only fallback to NTLMSSP if allowed */
		if (ADS_ERR_OK(status) || 
		    !(ads->auth.flags & ADS_AUTH_ALLOW_NTLMSSP)) {
			return status;
		}
	} else
#endif
	{
		TALLOC_FREE(given_principal);
	}

	/* lets do NTLMSSP ... this has the big advantage that we don't need
	   to sync clocks, and we don't rely on special versions of the krb5 
	   library for HMAC_MD4 encryption */
	return ads_sasl_spnego_ntlmssp_bind(ads);

failed:
	return status;
}

#ifdef HAVE_GSSAPI
#define MAX_GSS_PASSES 3

/* this performs a SASL/gssapi bind
   we avoid using cyrus-sasl to make Samba more robust. cyrus-sasl
   is very dependent on correctly configured DNS whereas
   this routine is much less fragile
   see RFC2078 and RFC2222 for details
*/
static ADS_STATUS ads_sasl_gssapi_do_bind(ADS_STRUCT *ads, const gss_name_t serv_name)
{
	uint32 minor_status;
	gss_ctx_id_t context_handle = GSS_C_NO_CONTEXT;
	gss_OID mech_type = GSS_C_NULL_OID;
	gss_buffer_desc output_token, input_token;
	uint32 req_flags, ret_flags;
	int conf_state;
	struct berval cred;
	struct berval *scred = NULL;
	int i=0;
	int gss_rc, rc;
	uint8 *p;
	uint32 max_msg_size = ADS_SASL_WRAPPING_OUT_MAX_WRAPPED;
	uint8 wrap_type = ADS_SASLWRAP_TYPE_PLAIN;
	ADS_STATUS status;

	input_token.value = NULL;
	input_token.length = 0;

	/*
	 * Note: here we always ask the gssapi for sign and seal
	 *       as this is negotiated later after the mutal
	 *       authentication
	 */
	req_flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG;

	for (i=0; i < MAX_GSS_PASSES; i++) {
		gss_rc = gss_init_sec_context(&minor_status,
					  GSS_C_NO_CREDENTIAL,
					  &context_handle,
					  serv_name,
					  mech_type,
					  req_flags,
					  0,
					  NULL,
					  &input_token,
					  NULL,
					  &output_token,
					  &ret_flags,
					  NULL);
		if (scred) {
			ber_bvfree(scred);
			scred = NULL;
		}
		if (gss_rc && gss_rc != GSS_S_CONTINUE_NEEDED) {
			status = ADS_ERROR_GSS(gss_rc, minor_status);
			goto failed;
		}

		cred.bv_val = (char *)output_token.value;
		cred.bv_len = output_token.length;

		rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSSAPI", &cred, NULL, NULL, 
				      &scred);
		if (rc != LDAP_SASL_BIND_IN_PROGRESS) {
			status = ADS_ERROR(rc);
			goto failed;
		}

		if (output_token.value) {
			gss_release_buffer(&minor_status, &output_token);
		}

		if (scred) {
			input_token.value = scred->bv_val;
			input_token.length = scred->bv_len;
		} else {
			input_token.value = NULL;
			input_token.length = 0;
		}

		if (gss_rc == 0) break;
	}

	gss_rc = gss_unwrap(&minor_status,context_handle,&input_token,&output_token,
			    &conf_state,NULL);
	if (scred) {
		ber_bvfree(scred);
		scred = NULL;
	}
	if (gss_rc) {
		status = ADS_ERROR_GSS(gss_rc, minor_status);
		goto failed;
	}

	p = (uint8 *)output_token.value;

#if 0
	file_save("sasl_gssapi.dat", output_token.value, output_token.length);
#endif

	if (p) {
		wrap_type = CVAL(p,0);
		SCVAL(p,0,0);
		max_msg_size = RIVAL(p,0);
	}

	gss_release_buffer(&minor_status, &output_token);

	if (!(wrap_type & ads->ldap.wrap_type)) {
		/*
		 * the server doesn't supports the wrap
		 * type we want :-(
		 */
		DEBUG(0,("The ldap sasl wrap type doesn't match wanted[%d] server[%d]\n",
			ads->ldap.wrap_type, wrap_type));
		DEBUGADD(0,("You may want to set the 'client ldap sasl wrapping' option\n"));
		status = ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
		goto failed;
	}

	/* 0x58 is the minimum windows accepts */
	if (max_msg_size < 0x58) {
		max_msg_size = 0x58;
	}

	output_token.length = 4;
	output_token.value = SMB_MALLOC(output_token.length);
	if (!output_token.value) {
		output_token.length = 0;
		status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
		goto failed;
	}
	p = (uint8 *)output_token.value;

	RSIVAL(p,0,max_msg_size);
	SCVAL(p,0,ads->ldap.wrap_type);

	/*
	 * we used to add sprintf("dn:%s", ads->config.bind_path) here.
	 * but using ads->config.bind_path is the wrong! It should be
	 * the DN of the user object!
	 *
	 * w2k3 gives an error when we send an incorrect DN, but sending nothing
	 * is ok and matches the information flow used in GSS-SPNEGO.
	 */

	gss_rc = gss_wrap(&minor_status, context_handle,0,GSS_C_QOP_DEFAULT,
			&output_token, /* used as *input* here. */
			&conf_state,
			&input_token); /* Used as *output* here. */
	if (gss_rc) {
		status = ADS_ERROR_GSS(gss_rc, minor_status);
		output_token.length = 0;
		SAFE_FREE(output_token.value);
		goto failed;
	}

	/* We've finished with output_token. */
	SAFE_FREE(output_token.value);
	output_token.length = 0;

	cred.bv_val = (char *)input_token.value;
	cred.bv_len = input_token.length;

	rc = ldap_sasl_bind_s(ads->ldap.ld, NULL, "GSSAPI", &cred, NULL, NULL, 
			      &scred);
	gss_release_buffer(&minor_status, &input_token);
	status = ADS_ERROR(rc);
	if (!ADS_ERR_OK(status)) {
		goto failed;
	}

	if (ads->ldap.wrap_type > ADS_SASLWRAP_TYPE_PLAIN) {
		gss_rc = gss_wrap_size_limit(&minor_status, context_handle,
					     (ads->ldap.wrap_type == ADS_SASLWRAP_TYPE_SEAL),
					     GSS_C_QOP_DEFAULT,
					     max_msg_size, &ads->ldap.out.max_unwrapped);
		if (gss_rc) {
			status = ADS_ERROR_GSS(gss_rc, minor_status);
			goto failed;
		}

		ads->ldap.out.sig_size = max_msg_size - ads->ldap.out.max_unwrapped;
		ads->ldap.in.min_wrapped = 0x2C; /* taken from a capture with LDAP unbind */
		ads->ldap.in.max_wrapped = max_msg_size;
		status = ads_setup_sasl_wrapping(ads, &ads_sasl_gssapi_ops, context_handle);
		if (!ADS_ERR_OK(status)) {
			DEBUG(0, ("ads_setup_sasl_wrapping() failed: %s\n",
				ads_errstr(status)));
			goto failed;
		}
		/* make sure we don't free context_handle */
		context_handle = GSS_C_NO_CONTEXT;
	}

failed:

	if (context_handle != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&minor_status, &context_handle, GSS_C_NO_BUFFER);

	if(scred)
		ber_bvfree(scred);
	return status;
}

static ADS_STATUS ads_sasl_gssapi_bind(ADS_STRUCT *ads)
{
	ADS_STATUS status;
	struct ads_service_principal p;

	status = ads_generate_service_principal(ads, NULL, &p);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	status = ads_sasl_gssapi_do_bind(ads, p.name);
	if (ADS_ERR_OK(status)) {
		ads_free_service_principal(&p);
		return status;
	}

	DEBUG(10,("ads_sasl_gssapi_do_bind failed with: %s, "
		  "calling kinit\n", ads_errstr(status)));

	status = ADS_ERROR_KRB5(ads_kinit_password(ads));

	if (ADS_ERR_OK(status)) {
		status = ads_sasl_gssapi_do_bind(ads, p.name);
	}

	ads_free_service_principal(&p);

	return status;
}

#endif /* HAVE_GSSAPI */

/* mapping between SASL mechanisms and functions */
static struct {
	const char *name;
	ADS_STATUS (*fn)(ADS_STRUCT *);
} sasl_mechanisms[] = {
	{"GSS-SPNEGO", ads_sasl_spnego_bind},
#ifdef HAVE_GSSAPI
	{"GSSAPI", ads_sasl_gssapi_bind}, /* doesn't work with .NET RC1. No idea why */
#endif
	{NULL, NULL}
};

ADS_STATUS ads_sasl_bind(ADS_STRUCT *ads)
{
	const char *attrs[] = {"supportedSASLMechanisms", NULL};
	char **values;
	ADS_STATUS status;
	int i, j;
	LDAPMessage *res;

	/* get a list of supported SASL mechanisms */
	status = ads_do_search(ads, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) return status;

	values = ldap_get_values(ads->ldap.ld, res, "supportedSASLMechanisms");

	if (ads->auth.flags & ADS_AUTH_SASL_SEAL) {
		ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SEAL;
	} else if (ads->auth.flags & ADS_AUTH_SASL_SIGN) {
		ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SIGN;
	} else {
		ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_PLAIN;
	}

	/* try our supported mechanisms in order */
	for (i=0;sasl_mechanisms[i].name;i++) {
		/* see if the server supports it */
		for (j=0;values && values[j];j++) {
			if (strcmp(values[j], sasl_mechanisms[i].name) == 0) {
				DEBUG(4,("Found SASL mechanism %s\n", values[j]));
retry:
				status = sasl_mechanisms[i].fn(ads);
				if (status.error_type == ENUM_ADS_ERROR_LDAP &&
				    status.err.rc == LDAP_STRONG_AUTH_REQUIRED &&
				    ads->ldap.wrap_type == ADS_SASLWRAP_TYPE_PLAIN)
				{
					DEBUG(3,("SASL bin got LDAP_STRONG_AUTH_REQUIRED "
						 "retrying with signing enabled\n"));
					ads->ldap.wrap_type = ADS_SASLWRAP_TYPE_SIGN;
					goto retry;
				}
				ldap_value_free(values);
				ldap_msgfree(res);
				return status;
			}
		}
	}

	ldap_value_free(values);
	ldap_msgfree(res);
	return ADS_ERROR(LDAP_AUTH_METHOD_NOT_SUPPORTED);
}

#endif /* HAVE_LDAP */

