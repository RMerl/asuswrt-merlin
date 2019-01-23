/* 
   Unix SMB/CIFS implementation.

   test suite for netlogon PAC operations

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008
   
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
#include "auth/auth.h"
#include "auth/auth_sam_reply.h"
#include "auth/gensec/gensec.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"

#define TEST_MACHINE_NAME_BDC "torturepacbdc"
#define TEST_MACHINE_NAME_WKSTA "torturepacwksta"
#define TEST_MACHINE_NAME_WKSTA_DES "torturepacwkdes"
#define TEST_MACHINE_NAME_S2U4SELF_BDC "tests2u4selfbdc"
#define TEST_MACHINE_NAME_S2U4SELF_WKSTA "tests2u4selfwk"

/* Check to see if we can pass the PAC across to the NETLOGON server for validation */

/* Also happens to be a really good one-step verfication of our Kerberos stack */

static bool test_PACVerify(struct torture_context *tctx, 
			   struct dcerpc_pipe *p,
			   struct cli_credentials *credentials,
			   enum netr_SchannelType secure_channel_type,
			   const char *test_machine_name)
{
	NTSTATUS status;

	struct netr_LogonSamLogon r;

	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative;
	struct netr_Authenticator return_authenticator;

	struct netr_GenericInfo generic;
	struct netr_Authenticator auth, auth2;
	
	struct netlogon_creds_CredentialState *creds;
	struct gensec_security *gensec_client_context;
	struct gensec_security *gensec_server_context;

	DATA_BLOB client_to_server, server_to_client, pac_wrapped, payload;
	struct PAC_Validate pac_wrapped_struct;
	
	enum ndr_err_code ndr_err;

	struct auth_session_info *session_info;

	struct dcerpc_binding_handle *b = p->binding_handle;
	TALLOC_CTX *tmp_ctx = talloc_new(tctx);
	torture_assert(tctx, tmp_ctx != NULL, "talloc_new() failed");

	if (!test_SetupCredentials2(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, 
				    credentials, secure_channel_type,
				    &creds)) {
		return false;
	}

	status = gensec_client_start(tctx, &gensec_client_context, tctx->ev, 
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	torture_assert_ntstatus_ok(tctx, status, "gensec_client_start (client) failed");

	status = gensec_set_target_hostname(gensec_client_context, test_machine_name);

	status = gensec_set_credentials(gensec_client_context, cmdline_credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (client) failed");

	status = gensec_start_mech_by_sasl_name(gensec_client_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (client) failed");

	status = gensec_server_start(tctx, tctx->ev, 
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				     NULL, &gensec_server_context);
	torture_assert_ntstatus_ok(tctx, status, "gensec_server_start (server) failed");

	status = gensec_set_credentials(gensec_server_context, credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (server) failed");

	status = gensec_start_mech_by_sasl_name(gensec_server_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (server) failed");

	server_to_client = data_blob(NULL, 0);
	
	do {
		/* Do a client-server update dance */
		status = gensec_update(gensec_client_context, tmp_ctx, server_to_client, &client_to_server);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (client) failed");
		}

		status = gensec_update(gensec_server_context, tmp_ctx, client_to_server, &server_to_client);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (server) failed");
		}

		if (NT_STATUS_IS_OK(status)) {
			break;
		}
	} while (1);

	/* Extract the PAC using Samba's code */

	status = gensec_session_info(gensec_server_context, &session_info);
	torture_assert_ntstatus_ok(tctx, status, "gensec_session_info failed");
	torture_assert(tctx, session_info->torture != NULL, "gensec_session_info failed to fill in torture sub struct");
	torture_assert(tctx, session_info->torture->pac_srv_sig != NULL, "pac_srv_sig not present");
	torture_assert(tctx, session_info->torture->pac_kdc_sig != NULL, "pac_kdc_sig not present");

	pac_wrapped_struct.ChecksumLength = session_info->torture->pac_srv_sig->signature.length;
	pac_wrapped_struct.SignatureType = session_info->torture->pac_kdc_sig->type;
	pac_wrapped_struct.SignatureLength = session_info->torture->pac_kdc_sig->signature.length;
	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->torture->pac_srv_sig->signature.data,
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->torture->pac_kdc_sig->signature.data,
	       pac_wrapped_struct.SignatureLength);

	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, &pac_wrapped_struct,
				       (ndr_push_flags_fn_t)ndr_push_PAC_Validate);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), "ndr_push_struct_blob of PACValidate structure failed");
		
	torture_assert(tctx, (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR), "not willing to even try a PACValidate without RC4 encryption");
	netlogon_creds_arcfour_crypt(creds, pac_wrapped.data, pac_wrapped.length);

	generic.length = pac_wrapped.length;
	generic.data = pac_wrapped.data;

	/* Validate it over the netlogon pipe */

	generic.identity_info.parameter_control = 0;
	generic.identity_info.logon_id_high = 0;
	generic.identity_info.logon_id_low = 0;
	generic.identity_info.domain_name.string = session_info->info->domain_name;
	generic.identity_info.account_name.string = session_info->info->account_name;
	generic.identity_info.workstation.string = test_machine_name;

	generic.package_name.string = "Kerberos";

	logon.generic = &generic;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon = &logon;
	r.in.logon_level = NetlogonGenericInformation;
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.validation_level = NetlogonValidationGenericInfo2;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;
	r.out.return_authenticator = &return_authenticator;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
		"LogonSamLogon failed");

	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogon failed");
	
	/* This will break the signature nicely (even in the crypto wrapping), check we get a logon failure */
	generic.data[generic.length-1]++;

	logon.generic = &generic;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = NetlogonGenericInformation;
	r.in.logon = &logon;
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.validation_level = NetlogonValidationGenericInfo2;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
		"LogonSamLogon failed");

	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_LOGON_FAILURE, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	/* This will break the parsing nicely (even in the crypto wrapping), check we get INVALID_PARAMETER */
	generic.length--;

	logon.generic = &generic;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = NetlogonGenericInformation;
	r.in.logon = &logon;
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.validation_level = NetlogonValidationGenericInfo2;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
		"LogonSamLogon failed");

	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_PARAMETER, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, 
							 &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	pac_wrapped_struct.ChecksumLength = session_info->torture->pac_srv_sig->signature.length;
	pac_wrapped_struct.SignatureType = session_info->torture->pac_kdc_sig->type;
	
	/* Break the SignatureType */
	pac_wrapped_struct.SignatureType++;

	pac_wrapped_struct.SignatureLength = session_info->torture->pac_kdc_sig->signature.length;
	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->torture->pac_srv_sig->signature.data,
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->torture->pac_kdc_sig->signature.data,
	       pac_wrapped_struct.SignatureLength);
	
	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, &pac_wrapped_struct,
				       (ndr_push_flags_fn_t)ndr_push_PAC_Validate);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), "ndr_push_struct_blob of PACValidate structure failed");
	
	torture_assert(tctx, (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR), "not willing to even try a PACValidate without RC4 encryption");
	netlogon_creds_arcfour_crypt(creds, pac_wrapped.data, pac_wrapped.length);
	
	generic.length = pac_wrapped.length;
	generic.data = pac_wrapped.data;

	logon.generic = &generic;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = NetlogonGenericInformation;
	r.in.logon = &logon;
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.validation_level = NetlogonValidationGenericInfo2;
	
	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
		"LogonSamLogon failed");
	
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_LOGON_FAILURE, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	pac_wrapped_struct.ChecksumLength = session_info->torture->pac_srv_sig->signature.length;
	pac_wrapped_struct.SignatureType = session_info->torture->pac_kdc_sig->type;
	pac_wrapped_struct.SignatureLength = session_info->torture->pac_kdc_sig->signature.length;

	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->torture->pac_srv_sig->signature.data,
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->torture->pac_kdc_sig->signature.data,
	       pac_wrapped_struct.SignatureLength);
	
	/* Break the signature length */
	pac_wrapped_struct.SignatureLength++;

	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, &pac_wrapped_struct,
				       (ndr_push_flags_fn_t)ndr_push_PAC_Validate);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), "ndr_push_struct_blob of PACValidate structure failed");
	
	torture_assert(tctx, (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR), "not willing to even try a PACValidate without RC4 encryption");
	netlogon_creds_arcfour_crypt(creds, pac_wrapped.data, pac_wrapped.length);
	
	generic.length = pac_wrapped.length;
	generic.data = pac_wrapped.data;

	logon.generic = &generic;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = NetlogonGenericInformation;
	r.in.logon = &logon;
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.validation_level = NetlogonValidationGenericInfo2;
	
	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
		"LogonSamLogon failed");
	
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_PARAMETER, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	return true;
}

static bool test_PACVerify_bdc(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       struct cli_credentials *credentials)
{
	return test_PACVerify(tctx, p, credentials, SEC_CHAN_BDC, TEST_MACHINE_NAME_BDC);
}

static bool test_PACVerify_workstation(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct cli_credentials *credentials)
{
	return test_PACVerify(tctx, p, credentials, SEC_CHAN_WKSTA, TEST_MACHINE_NAME_WKSTA);
}

static bool test_PACVerify_workstation_des(struct torture_context *tctx,
					   struct dcerpc_pipe *p, struct cli_credentials *credentials, struct test_join *join_ctx)
{
	struct samr_SetUserInfo r;
	union samr_UserInfo user_info;
	struct dcerpc_pipe *samr_pipe = torture_join_samr_pipe(join_ctx);
	struct smb_krb5_context *smb_krb5_context;
	krb5_error_code ret;

	ret = cli_credentials_get_krb5_context(cmdline_credentials, tctx->lp_ctx, &smb_krb5_context);
	torture_assert_int_equal(tctx, ret, 0, "cli_credentials_get_krb5_context() failed");

	if (krb5_config_get_bool_default(smb_krb5_context->krb5_context, NULL, FALSE,
					 "libdefaults",
					 "allow_weak_crypto", NULL) == FALSE) {
		torture_skip(tctx, "Cannot test DES without [libdefaults] allow_weak_crypto = yes");
	}

	/* Mark this workstation with DES-only */
	user_info.info16.acct_flags = ACB_USE_DES_KEY_ONLY | ACB_WSTRUST;
	r.in.user_handle = torture_join_samr_user_policy(join_ctx);
	r.in.level = 16;
	r.in.info = &user_info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(samr_pipe->binding_handle, tctx, &r),
		"failed to set DES info account flags");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"failed to set DES into account flags");

	return test_PACVerify(tctx, p, credentials, SEC_CHAN_WKSTA, TEST_MACHINE_NAME_WKSTA_DES);
}


/* Check various ways to get the PAC, in particular check the group membership and other details between the PAC from a normal kinit, S2U4Self and a SamLogon */
static bool test_S2U4Self(struct torture_context *tctx,
			  struct dcerpc_pipe *p,
			  struct cli_credentials *credentials,
			  enum netr_SchannelType secure_channel_type,
			  const char *test_machine_name)
{
	NTSTATUS status;
	struct dcerpc_binding_handle *b = p->binding_handle;

	struct netr_LogonSamLogon r;

	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative;

	struct netr_Authenticator auth, auth2;

	DATA_BLOB client_to_server, server_to_client;

	struct netlogon_creds_CredentialState *creds;
	struct gensec_security *gensec_client_context;
	struct gensec_security *gensec_server_context;

	struct auth_session_info *kinit_session_info;
	struct auth_session_info *s2u4self_session_info;
	struct auth_user_info_dc *netlogon_user_info_dc;

	struct netr_NetworkInfo ninfo;
	DATA_BLOB names_blob, chal, lm_resp, nt_resp;
	size_t i;
	int flags = CLI_CRED_NTLMv2_AUTH;

	struct dom_sid *builtin_domain;

	TALLOC_CTX *tmp_ctx = talloc_new(tctx);

	torture_assert(tctx, tmp_ctx != NULL, "talloc_new() failed");

	/* First, do a normal Kerberos connection */

	status = gensec_client_start(tctx, &gensec_client_context, tctx->ev,
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	torture_assert_ntstatus_ok(tctx, status, "gensec_client_start (client) failed");

	status = gensec_set_target_hostname(gensec_client_context, test_machine_name);

	status = gensec_set_credentials(gensec_client_context, cmdline_credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (client) failed");

	status = gensec_start_mech_by_sasl_name(gensec_client_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (client) failed");

	status = gensec_server_start(tctx, tctx->ev,
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				     NULL, &gensec_server_context);
	torture_assert_ntstatus_ok(tctx, status, "gensec_server_start (server) failed");

	status = gensec_set_credentials(gensec_server_context, credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (server) failed");

	status = gensec_start_mech_by_sasl_name(gensec_server_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (server) failed");

	server_to_client = data_blob(NULL, 0);

	do {
		/* Do a client-server update dance */
		status = gensec_update(gensec_client_context, tmp_ctx, server_to_client, &client_to_server);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (client) failed");
		}

		status = gensec_update(gensec_server_context, tmp_ctx, client_to_server, &server_to_client);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (server) failed");
		}

		if (NT_STATUS_IS_OK(status)) {
			break;
		}
	} while (1);

	/* Extract the PAC using Samba's code */

	status = gensec_session_info(gensec_server_context, &kinit_session_info);
	torture_assert_ntstatus_ok(tctx, status, "gensec_session_info failed");


	/* Now do the dance with S2U4Self */

	/* Wipe out any existing ccache */
	cli_credentials_invalidate_ccache(credentials, CRED_SPECIFIED);
	cli_credentials_set_target_service(credentials, talloc_asprintf(tmp_ctx, "host/%s", test_machine_name));
	cli_credentials_set_impersonate_principal(credentials, cli_credentials_get_principal(cmdline_credentials, tmp_ctx));

	status = gensec_client_start(tctx, &gensec_client_context, tctx->ev,
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	torture_assert_ntstatus_ok(tctx, status, "gensec_client_start (client) failed");

	status = gensec_set_target_hostname(gensec_client_context, test_machine_name);

	/* We now set the same credentials on both client and server contexts */
	status = gensec_set_credentials(gensec_client_context, credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (client) failed");

	status = gensec_start_mech_by_sasl_name(gensec_client_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (client) failed");

	status = gensec_server_start(tctx, tctx->ev,
				     lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				     NULL, &gensec_server_context);
	torture_assert_ntstatus_ok(tctx, status, "gensec_server_start (server) failed");

	status = gensec_set_credentials(gensec_server_context, credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (server) failed");

	status = gensec_start_mech_by_sasl_name(gensec_server_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (server) failed");

	server_to_client = data_blob(NULL, 0);

	do {
		/* Do a client-server update dance */
		status = gensec_update(gensec_client_context, tmp_ctx, server_to_client, &client_to_server);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (client) failed");
		}

		status = gensec_update(gensec_server_context, tmp_ctx, client_to_server, &server_to_client);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {;
			torture_assert_ntstatus_ok(tctx, status, "gensec_update (server) failed");
		}

		if (NT_STATUS_IS_OK(status)) {
			break;
		}
	} while (1);

	/* Don't pollute the remaining tests with the changed credentials */
	cli_credentials_invalidate_ccache(credentials, CRED_SPECIFIED);
	cli_credentials_set_target_service(credentials, NULL);
	cli_credentials_set_impersonate_principal(credentials, NULL);

	/* Extract the PAC using Samba's code */

	status = gensec_session_info(gensec_server_context, &s2u4self_session_info);
	torture_assert_ntstatus_ok(tctx, status, "gensec_session_info failed");

	cli_credentials_get_ntlm_username_domain(cmdline_credentials, tctx,
						 &ninfo.identity_info.account_name.string,
						 &ninfo.identity_info.domain_name.string);

	/* Now try with SamLogon */
	generate_random_buffer(ninfo.challenge,
			       sizeof(ninfo.challenge));
	chal = data_blob_const(ninfo.challenge,
			       sizeof(ninfo.challenge));

	names_blob = NTLMv2_generate_names_blob(tctx, cli_credentials_get_workstation(credentials),
						cli_credentials_get_domain(credentials));

	status = cli_credentials_get_ntlm_response(cmdline_credentials, tctx,
						   &flags,
						   chal,
						   names_blob,
						   &lm_resp, &nt_resp,
						   NULL, NULL);
	torture_assert_ntstatus_ok(tctx, status, "cli_credentials_get_ntlm_response failed");

	ninfo.lm.data = lm_resp.data;
	ninfo.lm.length = lm_resp.length;

	ninfo.nt.data = nt_resp.data;
	ninfo.nt.length = nt_resp.length;

	ninfo.identity_info.parameter_control = 0;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.workstation.string = cli_credentials_get_workstation(credentials);

	logon.network = &ninfo;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = 2;
	r.in.logon = &logon;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;

	if (!test_SetupCredentials2(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS,
				    credentials, secure_channel_type,
				    &creds)) {
		return false;
	}

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);

	r.in.validation_level = 3;

	status = dcerpc_netr_LogonSamLogon_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonSamLogon failed");

	torture_assert(tctx, netlogon_creds_client_check(creds,
							 &r.out.return_authenticator->cred),
		       "Credential chaining failed");

	status = make_user_info_dc_netlogon_validation(tmp_ctx,
						      ninfo.identity_info.account_name.string,
						      r.in.validation_level,
						      r.out.validation,
						      &netlogon_user_info_dc);

	torture_assert_ntstatus_ok(tctx, status, "make_user_info_dc_netlogon_validation failed");

	torture_assert_str_equal(tctx, netlogon_user_info_dc->info->account_name == NULL ? "" : netlogon_user_info_dc->info->account_name,
				 kinit_session_info->info->account_name, "Account name differs for kinit-based PAC");
	torture_assert_str_equal(tctx,netlogon_user_info_dc->info->account_name == NULL ? "" : netlogon_user_info_dc->info->account_name,
				 s2u4self_session_info->info->account_name, "Account name differs for S2U4Self");
	torture_assert_str_equal(tctx, netlogon_user_info_dc->info->full_name == NULL ? "" : netlogon_user_info_dc->info->full_name, kinit_session_info->info->full_name, "Full name differs for kinit-based PAC");
	torture_assert_str_equal(tctx, netlogon_user_info_dc->info->full_name == NULL ? "" : netlogon_user_info_dc->info->full_name, s2u4self_session_info->info->full_name, "Full name differs for S2U4Self");
	torture_assert_int_equal(tctx, netlogon_user_info_dc->num_sids, kinit_session_info->torture->num_dc_sids, "Different numbers of domain groups for kinit-based PAC");
	torture_assert_int_equal(tctx, netlogon_user_info_dc->num_sids, s2u4self_session_info->torture->num_dc_sids, "Different numbers of domain groups for S2U4Self");

	builtin_domain = dom_sid_parse_talloc(tmp_ctx, SID_BUILTIN);

	for (i = 0; i < kinit_session_info->torture->num_dc_sids; i++) {
		torture_assert(tctx, dom_sid_equal(&netlogon_user_info_dc->sids[i], &kinit_session_info->torture->dc_sids[i]), "Different domain groups for kinit-based PAC");
		torture_assert(tctx, dom_sid_equal(&netlogon_user_info_dc->sids[i], &s2u4self_session_info->torture->dc_sids[i]), "Different domain groups for S2U4Self");
		torture_assert(tctx, !dom_sid_in_domain(builtin_domain, &s2u4self_session_info->torture->dc_sids[i]), "Returned BUILTIN domain in groups for S2U4Self");
		torture_assert(tctx, !dom_sid_in_domain(builtin_domain, &kinit_session_info->torture->dc_sids[i]), "Returned BUILTIN domain in groups kinit-based PAC");
		torture_assert(tctx, !dom_sid_in_domain(builtin_domain, &netlogon_user_info_dc->sids[i]), "Returned BUILTIN domian in groups from NETLOGON SamLogon reply");
	}

	return true;
}

static bool test_S2U4Self_bdc(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       struct cli_credentials *credentials)
{
	return test_S2U4Self(tctx, p, credentials, SEC_CHAN_BDC, TEST_MACHINE_NAME_S2U4SELF_BDC);
}

static bool test_S2U4Self_workstation(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct cli_credentials *credentials)
{
	return test_S2U4Self(tctx, p, credentials, SEC_CHAN_WKSTA, TEST_MACHINE_NAME_S2U4SELF_WKSTA);
}

struct torture_suite *torture_rpc_remote_pac(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "pac");
	struct torture_rpc_tcase *tcase;

	/* It is important to use different names, so that old entries in our credential cache are not used */
	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon-bdc",
							      &ndr_table_netlogon, TEST_MACHINE_NAME_BDC);
	torture_rpc_tcase_add_test_creds(tcase, "verify-sig", test_PACVerify_bdc);

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite, "netlogon-member",
								      &ndr_table_netlogon, TEST_MACHINE_NAME_WKSTA);
	torture_rpc_tcase_add_test_creds(tcase, "verify-sig", test_PACVerify_workstation);

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite, "netlogon-member-des",
								      &ndr_table_netlogon, TEST_MACHINE_NAME_WKSTA_DES);
	torture_rpc_tcase_add_test_join(tcase, "verify-sig", test_PACVerify_workstation_des);

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon-bdc",
							      &ndr_table_netlogon, TEST_MACHINE_NAME_S2U4SELF_BDC);
	torture_rpc_tcase_add_test_creds(tcase, "s2u4self", test_S2U4Self_bdc);

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite, "netlogon-member",
								      &ndr_table_netlogon, TEST_MACHINE_NAME_S2U4SELF_WKSTA);

	torture_rpc_tcase_add_test_creds(tcase, "s2u4self", test_S2U4Self_workstation);
	return suite;
}
