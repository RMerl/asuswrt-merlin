/*
   Unix SMB/CIFS implementation.
   NT Domain Authentication SMB / MSRPC client
   Copyright (C) Andrew Tridgell 1992-2000
   Copyright (C) Jeremy Allison                    1998.
   Largely re-written by Jeremy Allison (C)	   2005.
   Copyright (C) Guenther Deschner                 2008.

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
#include "rpc_client/rpc_client.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_netlogon_c.h"
#include "rpc_client/cli_netlogon.h"
#include "rpc_client/init_netlogon.h"
#include "rpc_client/util_netlogon.h"
#include "../libcli/security/security.h"

/****************************************************************************
 Wrapper function that uses the auth and auth2 calls to set up a NETLOGON
 credentials chain. Stores the credentials in the struct dcinfo in the
 netlogon pipe struct.
****************************************************************************/

NTSTATUS rpccli_netlogon_setup_creds(struct rpc_pipe_client *cli,
				     const char *server_name,
				     const char *domain,
				     const char *clnt_name,
				     const char *machine_account,
				     const unsigned char machine_pwd[16],
				     enum netr_SchannelType sec_chan_type,
				     uint32_t *neg_flags_inout)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct netr_Credential clnt_chal_send;
	struct netr_Credential srv_chal_recv;
	struct samr_Password password;
	bool retried = false;
	fstring mach_acct;
	uint32_t neg_flags = *neg_flags_inout;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (!ndr_syntax_id_equal(&cli->abstract_syntax,
				 &ndr_table_netlogon.syntax_id)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	TALLOC_FREE(cli->dc);

	/* Store the machine account password we're going to use. */
	memcpy(password.hash, machine_pwd, 16);

	fstr_sprintf( mach_acct, "%s$", machine_account);

 again:
	/* Create the client challenge. */
	generate_random_buffer(clnt_chal_send.data, 8);

	/* Get the server challenge. */
	status = dcerpc_netr_ServerReqChallenge(b, talloc_tos(),
						cli->srv_name_slash,
						clnt_name,
						&clnt_chal_send,
						&srv_chal_recv,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/* Calculate the session key and client credentials */

	cli->dc = netlogon_creds_client_init(cli,
				    mach_acct,
				    clnt_name,
				    &clnt_chal_send,
				    &srv_chal_recv,
				    &password,
				    &clnt_chal_send,
				    neg_flags);

	if (!cli->dc) {
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * Send client auth-2 challenge and receive server repy.
	 */

	status = dcerpc_netr_ServerAuthenticate2(b, talloc_tos(),
						 cli->srv_name_slash,
						 cli->dc->account_name,
						 sec_chan_type,
						 cli->dc->computer_name,
						 &clnt_chal_send, /* input. */
						 &srv_chal_recv, /* output. */
						 &neg_flags,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	/* we might be talking to NT4, so let's downgrade in that case and retry
	 * with the returned neg_flags - gd */

	if (NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED) && !retried) {
		retried = true;
		TALLOC_FREE(cli->dc);
		goto again;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/*
	 * Check the returned value using the initial
	 * server received challenge.
	 */

	if (!netlogon_creds_client_check(cli->dc, &srv_chal_recv)) {
		/*
		 * Server replied with bad credential. Fail.
		 */
		DEBUG(0,("rpccli_netlogon_setup_creds: server %s "
			"replied with bad credential\n",
			cli->desthost ));
		return NT_STATUS_ACCESS_DENIED;
	}

	DEBUG(5,("rpccli_netlogon_setup_creds: server %s credential "
		"chain established.\n",
		cli->desthost ));

	cli->dc->negotiate_flags = neg_flags;
	*neg_flags_inout = neg_flags;

	return NT_STATUS_OK;
}

/* Logon domain user */

NTSTATUS rpccli_netlogon_sam_logon(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   uint32 logon_parameters,
				   const char *domain,
				   const char *username,
				   const char *password,
				   const char *workstation,
				   uint16_t validation_level,
				   int logon_type)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS status;
	struct netr_Authenticator clnt_creds;
	struct netr_Authenticator ret_creds;
	union netr_LogonLevel *logon;
	union netr_Validation validation;
	uint8_t authoritative;
	fstring clnt_name_slash;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(ret_creds);

	logon = TALLOC_ZERO_P(mem_ctx, union netr_LogonLevel);
	if (!logon) {
		return NT_STATUS_NO_MEMORY;
	}

	if (workstation) {
		fstr_sprintf( clnt_name_slash, "\\\\%s", workstation );
	} else {
		fstr_sprintf( clnt_name_slash, "\\\\%s", global_myname() );
	}

	/* Initialise input parameters */

	netlogon_creds_client_authenticator(cli->dc, &clnt_creds);

	switch (logon_type) {
	case NetlogonInteractiveInformation: {

		struct netr_PasswordInfo *password_info;

		struct samr_Password lmpassword;
		struct samr_Password ntpassword;

		password_info = TALLOC_ZERO_P(mem_ctx, struct netr_PasswordInfo);
		if (!password_info) {
			return NT_STATUS_NO_MEMORY;
		}

		nt_lm_owf_gen(password, ntpassword.hash, lmpassword.hash);

		if (cli->dc->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(cli->dc, lmpassword.hash, 16);
			netlogon_creds_arcfour_crypt(cli->dc, ntpassword.hash, 16);
		} else {
			netlogon_creds_des_encrypt(cli->dc, &lmpassword);
			netlogon_creds_des_encrypt(cli->dc, &ntpassword);
		}

		password_info->identity_info.domain_name.string		= domain;
		password_info->identity_info.parameter_control		= logon_parameters;
		password_info->identity_info.logon_id_low		= 0xdead;
		password_info->identity_info.logon_id_high		= 0xbeef;
		password_info->identity_info.account_name.string	= username;
		password_info->identity_info.workstation.string		= clnt_name_slash;

		password_info->lmpassword = lmpassword;
		password_info->ntpassword = ntpassword;

		logon->password = password_info;

		break;
	}
	case NetlogonNetworkInformation: {
		struct netr_NetworkInfo *network_info;
		uint8 chal[8];
		unsigned char local_lm_response[24];
		unsigned char local_nt_response[24];
		struct netr_ChallengeResponse lm;
		struct netr_ChallengeResponse nt;

		ZERO_STRUCT(lm);
		ZERO_STRUCT(nt);

		network_info = TALLOC_ZERO_P(mem_ctx, struct netr_NetworkInfo);
		if (!network_info) {
			return NT_STATUS_NO_MEMORY;
		}

		generate_random_buffer(chal, 8);

		SMBencrypt(password, chal, local_lm_response);
		SMBNTencrypt(password, chal, local_nt_response);

		lm.length = 24;
		lm.data = local_lm_response;

		nt.length = 24;
		nt.data = local_nt_response;

		network_info->identity_info.domain_name.string		= domain;
		network_info->identity_info.parameter_control		= logon_parameters;
		network_info->identity_info.logon_id_low		= 0xdead;
		network_info->identity_info.logon_id_high		= 0xbeef;
		network_info->identity_info.account_name.string		= username;
		network_info->identity_info.workstation.string		= clnt_name_slash;

		memcpy(network_info->challenge, chal, 8);
		network_info->nt = nt;
		network_info->lm = lm;

		logon->network = network_info;

		break;
	}
	default:
		DEBUG(0, ("switch value %d not supported\n",
			logon_type));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	status = dcerpc_netr_LogonSamLogon(b, mem_ctx,
					   cli->srv_name_slash,
					   global_myname(),
					   &clnt_creds,
					   &ret_creds,
					   logon_type,
					   logon,
					   validation_level,
					   &validation,
					   &authoritative,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Always check returned credentials */
	if (!netlogon_creds_client_check(cli->dc, &ret_creds.cred)) {
		DEBUG(0,("rpccli_netlogon_sam_logon: credentials chain check failed\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	return result;
}

static NTSTATUS map_validation_to_info3(TALLOC_CTX *mem_ctx,
					uint16_t validation_level,
					union netr_Validation *validation,
					struct netr_SamInfo3 **info3_p)
{
	struct netr_SamInfo3 *info3;
	NTSTATUS status;

	if (validation == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (validation_level) {
	case 3:
		if (validation->sam3 == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		info3 = talloc_move(mem_ctx, &validation->sam3);
		break;
	case 6:
		if (validation->sam6 == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		info3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
		if (info3 == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		status = copy_netr_SamBaseInfo(info3, &validation->sam6->base, &info3->base);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(info3);
			return status;
		}

		info3->sidcount = validation->sam6->sidcount;
		info3->sids = talloc_move(info3, &validation->sam6->sids);
		break;
	default:
		return NT_STATUS_BAD_VALIDATION_CLASS;
	}

	*info3_p = info3;

	return NT_STATUS_OK;
}

/**
 * Logon domain user with an 'network' SAM logon
 *
 * @param info3 Pointer to a NET_USER_INFO_3 already allocated by the caller.
 **/

NTSTATUS rpccli_netlogon_sam_network_logon(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   uint32 logon_parameters,
					   const char *server,
					   const char *username,
					   const char *domain,
					   const char *workstation,
					   const uint8 chal[8],
					   uint16_t validation_level,
					   DATA_BLOB lm_response,
					   DATA_BLOB nt_response,
					   struct netr_SamInfo3 **info3)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS status;
	const char *workstation_name_slash;
	const char *server_name_slash;
	struct netr_Authenticator clnt_creds;
	struct netr_Authenticator ret_creds;
	union netr_LogonLevel *logon = NULL;
	struct netr_NetworkInfo *network_info;
	uint8_t authoritative;
	union netr_Validation validation;
	struct netr_ChallengeResponse lm;
	struct netr_ChallengeResponse nt;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	*info3 = NULL;

	ZERO_STRUCT(ret_creds);

	ZERO_STRUCT(lm);
	ZERO_STRUCT(nt);

	logon = TALLOC_ZERO_P(mem_ctx, union netr_LogonLevel);
	if (!logon) {
		return NT_STATUS_NO_MEMORY;
	}

	network_info = TALLOC_ZERO_P(mem_ctx, struct netr_NetworkInfo);
	if (!network_info) {
		return NT_STATUS_NO_MEMORY;
	}

	netlogon_creds_client_authenticator(cli->dc, &clnt_creds);

	if (server[0] != '\\' && server[1] != '\\') {
		server_name_slash = talloc_asprintf(mem_ctx, "\\\\%s", server);
	} else {
		server_name_slash = server;
	}

	if (workstation[0] != '\\' && workstation[1] != '\\') {
		workstation_name_slash = talloc_asprintf(mem_ctx, "\\\\%s", workstation);
	} else {
		workstation_name_slash = workstation;
	}

	if (!workstation_name_slash || !server_name_slash) {
		DEBUG(0, ("talloc_asprintf failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Initialise input parameters */

	lm.data = lm_response.data;
	lm.length = lm_response.length;
	nt.data = nt_response.data;
	nt.length = nt_response.length;

	network_info->identity_info.domain_name.string		= domain;
	network_info->identity_info.parameter_control		= logon_parameters;
	network_info->identity_info.logon_id_low		= 0xdead;
	network_info->identity_info.logon_id_high		= 0xbeef;
	network_info->identity_info.account_name.string		= username;
	network_info->identity_info.workstation.string		= workstation_name_slash;

	memcpy(network_info->challenge, chal, 8);
	network_info->nt = nt;
	network_info->lm = lm;

	logon->network = network_info;

	/* Marshall data and send request */

	status = dcerpc_netr_LogonSamLogon(b, mem_ctx,
					   server_name_slash,
					   global_myname(),
					   &clnt_creds,
					   &ret_creds,
					   NetlogonNetworkInformation,
					   logon,
					   validation_level,
					   &validation,
					   &authoritative,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Always check returned credentials. */
	if (!netlogon_creds_client_check(cli->dc, &ret_creds.cred)) {
		DEBUG(0,("rpccli_netlogon_sam_network_logon: credentials chain check failed\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	netlogon_creds_decrypt_samlogon(cli->dc, validation_level, &validation);

	result = map_validation_to_info3(mem_ctx, validation_level, &validation, info3);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	return result;
}

NTSTATUS rpccli_netlogon_sam_network_logon_ex(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      uint32 logon_parameters,
					      const char *server,
					      const char *username,
					      const char *domain,
					      const char *workstation,
					      const uint8 chal[8],
					      uint16_t validation_level,
					      DATA_BLOB lm_response,
					      DATA_BLOB nt_response,
					      struct netr_SamInfo3 **info3)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS status;
	const char *workstation_name_slash;
	const char *server_name_slash;
	union netr_LogonLevel *logon = NULL;
	struct netr_NetworkInfo *network_info;
	uint8_t authoritative;
	union netr_Validation validation;
	struct netr_ChallengeResponse lm;
	struct netr_ChallengeResponse nt;
	uint32_t flags = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	*info3 = NULL;

	ZERO_STRUCT(lm);
	ZERO_STRUCT(nt);

	logon = TALLOC_ZERO_P(mem_ctx, union netr_LogonLevel);
	if (!logon) {
		return NT_STATUS_NO_MEMORY;
	}

	network_info = TALLOC_ZERO_P(mem_ctx, struct netr_NetworkInfo);
	if (!network_info) {
		return NT_STATUS_NO_MEMORY;
	}

	if (server[0] != '\\' && server[1] != '\\') {
		server_name_slash = talloc_asprintf(mem_ctx, "\\\\%s", server);
	} else {
		server_name_slash = server;
	}

	if (workstation[0] != '\\' && workstation[1] != '\\') {
		workstation_name_slash = talloc_asprintf(mem_ctx, "\\\\%s", workstation);
	} else {
		workstation_name_slash = workstation;
	}

	if (!workstation_name_slash || !server_name_slash) {
		DEBUG(0, ("talloc_asprintf failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Initialise input parameters */

	lm.data = lm_response.data;
	lm.length = lm_response.length;
	nt.data = nt_response.data;
	nt.length = nt_response.length;

	network_info->identity_info.domain_name.string		= domain;
	network_info->identity_info.parameter_control		= logon_parameters;
	network_info->identity_info.logon_id_low		= 0xdead;
	network_info->identity_info.logon_id_high		= 0xbeef;
	network_info->identity_info.account_name.string		= username;
	network_info->identity_info.workstation.string		= workstation_name_slash;

	memcpy(network_info->challenge, chal, 8);
	network_info->nt = nt;
	network_info->lm = lm;

	logon->network = network_info;

        /* Marshall data and send request */

	status = dcerpc_netr_LogonSamLogonEx(b, mem_ctx,
					     server_name_slash,
					     global_myname(),
					     NetlogonNetworkInformation,
					     logon,
					     validation_level,
					     &validation,
					     &authoritative,
					     &flags,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	netlogon_creds_decrypt_samlogon(cli->dc, validation_level, &validation);

	result = map_validation_to_info3(mem_ctx, validation_level, &validation, info3);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	return result;
}

/*********************************************************
 Change the domain password on the PDC.

 Just changes the password betwen the two values specified.

 Caller must have the cli connected to the netlogon pipe
 already.
**********************************************************/

NTSTATUS rpccli_netlogon_set_trust_password(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    const char *account_name,
					    const unsigned char orig_trust_passwd_hash[16],
					    const char *new_trust_pwd_cleartext,
					    const unsigned char new_trust_passwd_hash[16],
					    enum netr_SchannelType sec_channel_type)
{
	NTSTATUS result, status;
	struct netr_Authenticator clnt_creds, srv_cred;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (!cli->dc) {
		uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
		result = rpccli_netlogon_setup_creds(cli,
						     cli->desthost, /* server name */
						     lp_workgroup(), /* domain */
						     global_myname(), /* client name */
						     account_name, /* machine account name */
						     orig_trust_passwd_hash,
						     sec_channel_type,
						     &neg_flags);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(3,("rpccli_netlogon_set_trust_password: unable to setup creds (%s)!\n",
				 nt_errstr(result)));
			return result;
		}
	}

	netlogon_creds_client_authenticator(cli->dc, &clnt_creds);

	if (cli->dc->negotiate_flags & NETLOGON_NEG_PASSWORD_SET2) {

		struct netr_CryptPassword new_password;
		uint32_t old_timeout;

		init_netr_CryptPassword(new_trust_pwd_cleartext,
					cli->dc->session_key,
					&new_password);

		old_timeout = dcerpc_binding_handle_set_timeout(b, 600000);

		status = dcerpc_netr_ServerPasswordSet2(b, mem_ctx,
							cli->srv_name_slash,
							cli->dc->account_name,
							sec_channel_type,
							cli->dc->computer_name,
							&clnt_creds,
							&srv_cred,
							&new_password,
							&result);

		dcerpc_binding_handle_set_timeout(b, old_timeout);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcerpc_netr_ServerPasswordSet2 failed: %s\n",
				nt_errstr(status)));
			return status;
		}
	} else {

		struct samr_Password new_password;
		uint32_t old_timeout;

		memcpy(new_password.hash, new_trust_passwd_hash, sizeof(new_password.hash));
		netlogon_creds_des_encrypt(cli->dc, &new_password);

		old_timeout = dcerpc_binding_handle_set_timeout(b, 600000);

		status = dcerpc_netr_ServerPasswordSet(b, mem_ctx,
						       cli->srv_name_slash,
						       cli->dc->account_name,
						       sec_channel_type,
						       cli->dc->computer_name,
						       &clnt_creds,
						       &srv_cred,
						       &new_password,
						       &result);

		dcerpc_binding_handle_set_timeout(b, old_timeout);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcerpc_netr_ServerPasswordSet failed: %s\n",
				nt_errstr(status)));
			return status;
		}
	}

	/* Always check returned credentials. */
	if (!netlogon_creds_client_check(cli->dc, &srv_cred.cred)) {
		DEBUG(0,("credentials chain check failed\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0,("dcerpc_netr_ServerPasswordSet{2} failed: %s\n",
			nt_errstr(result)));
		return result;
	}

	return result;
}

