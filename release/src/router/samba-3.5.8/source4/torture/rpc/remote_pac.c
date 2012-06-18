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
#include "torture/torture.h"
#include "lib/events/events.h"
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/rpc.h"
#include "torture/rpc/netlogon.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "param/param.h"
#include "lib/messaging/irpc.h"
#include "cluster/cluster.h"

#define TEST_MACHINE_NAME "torturepactest"

/* Check to see if we can pass the PAC across to the NETLOGON server for validation */

/* Also happens to be a really good one-step verfication of our Kerberos stack */

static bool test_PACVerify(struct torture_context *tctx, 
			   struct dcerpc_pipe *p,
			   struct cli_credentials *credentials)
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

	char *tmp_dir;

	TALLOC_CTX *tmp_ctx = talloc_new(tctx);
	
	torture_assert(tctx, tmp_ctx != NULL, "talloc_new() failed");

	if (!test_SetupCredentials2(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, 
				    credentials, SEC_CHAN_BDC, 
				    &creds)) {
		return false;
	}

	status = torture_temp_dir(tctx, "PACVerify", &tmp_dir);
	torture_assert_ntstatus_ok(tctx, status, "torture_temp_dir failed");

	status = gensec_client_start(tctx, &gensec_client_context, tctx->ev, 
				     lp_gensec_settings(tctx, tctx->lp_ctx));
	torture_assert_ntstatus_ok(tctx, status, "gensec_client_start (client) failed");

	status = gensec_set_target_hostname(gensec_client_context, TEST_MACHINE_NAME);

	status = gensec_set_credentials(gensec_client_context, cmdline_credentials);
	torture_assert_ntstatus_ok(tctx, status, "gensec_set_credentials (client) failed");

	status = gensec_start_mech_by_sasl_name(gensec_client_context, "GSSAPI");
	torture_assert_ntstatus_ok(tctx, status, "gensec_start_mech_by_sasl_name (client) failed");

	status = gensec_server_start(tctx, tctx->ev, 
				     lp_gensec_settings(tctx, tctx->lp_ctx), 
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
	
	pac_wrapped_struct.ChecksumLength = session_info->server_info->pac_srv_sig.signature.length;
	pac_wrapped_struct.SignatureType = session_info->server_info->pac_kdc_sig.type;
	pac_wrapped_struct.SignatureLength = session_info->server_info->pac_kdc_sig.signature.length;
	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->server_info->pac_srv_sig.signature.data, 
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->server_info->pac_kdc_sig.signature.data, 
	       pac_wrapped_struct.SignatureLength);

	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, lp_iconv_convenience(tctx->lp_ctx), &pac_wrapped_struct,
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
	generic.identity_info.domain_name.string = session_info->server_info->domain_name;
	generic.identity_info.account_name.string = session_info->server_info->account_name;
	generic.identity_info.workstation.string = TEST_MACHINE_NAME;

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

	status = dcerpc_netr_LogonSamLogon(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "LogonSamLogon failed");
	
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

	status = dcerpc_netr_LogonSamLogon(p, tctx, &r);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_LOGON_FAILURE, "LogonSamLogon failed");
	
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

	status = dcerpc_netr_LogonSamLogon(p, tctx, &r);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_INVALID_PARAMETER, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, 
							 &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	pac_wrapped_struct.ChecksumLength = session_info->server_info->pac_srv_sig.signature.length;
	pac_wrapped_struct.SignatureType = session_info->server_info->pac_kdc_sig.type;
	
	/* Break the SignatureType */
	pac_wrapped_struct.SignatureType++;

	pac_wrapped_struct.SignatureLength = session_info->server_info->pac_kdc_sig.signature.length;
	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->server_info->pac_srv_sig.signature.data, 
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->server_info->pac_kdc_sig.signature.data, 
	       pac_wrapped_struct.SignatureLength);
	
	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, lp_iconv_convenience(tctx->lp_ctx), &pac_wrapped_struct,
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
	
	status = dcerpc_netr_LogonSamLogon(p, tctx, &r);
	
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_LOGON_FAILURE, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, &r.out.return_authenticator->cred), 
		       "Credential chaining failed");

	pac_wrapped_struct.ChecksumLength = session_info->server_info->pac_srv_sig.signature.length;
	pac_wrapped_struct.SignatureType = session_info->server_info->pac_kdc_sig.type;
	pac_wrapped_struct.SignatureLength = session_info->server_info->pac_kdc_sig.signature.length;

	pac_wrapped_struct.ChecksumAndSignature = payload
		= data_blob_talloc(tmp_ctx, NULL, 
				   pac_wrapped_struct.ChecksumLength
				   + pac_wrapped_struct.SignatureLength);
	memcpy(&payload.data[0], 
	       session_info->server_info->pac_srv_sig.signature.data, 
	       pac_wrapped_struct.ChecksumLength);
	memcpy(&payload.data[pac_wrapped_struct.ChecksumLength], 
	       session_info->server_info->pac_kdc_sig.signature.data, 
	       pac_wrapped_struct.SignatureLength);
	
	/* Break the signature length */
	pac_wrapped_struct.SignatureLength++;

	ndr_err = ndr_push_struct_blob(&pac_wrapped, tmp_ctx, lp_iconv_convenience(tctx->lp_ctx), &pac_wrapped_struct,
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
	
	status = dcerpc_netr_LogonSamLogon(p, tctx, &r);
	
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_INVALID_PARAMETER, "LogonSamLogon failed");
	
	torture_assert(tctx, netlogon_creds_client_check(creds, &r.out.return_authenticator->cred), 
		       "Credential chaining failed");
	return true;
}

struct torture_suite *torture_rpc_remote_pac(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "PAC");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon, TEST_MACHINE_NAME);
	torture_rpc_tcase_add_test_creds(tcase, "verify", test_PACVerify);

	return suite;
}
