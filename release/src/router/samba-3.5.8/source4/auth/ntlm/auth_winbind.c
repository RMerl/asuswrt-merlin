/* 
   Unix SMB/CIFS implementation.

   Winbind authentication mechnism

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Bartlett 2001 - 2002
   Copyright (C) Stefan Metzmacher 2005
   
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
#include "auth/ntlm/auth_proto.h"
#include "auth/auth_sam_reply.h"
#include "nsswitch/winbind_client.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_winbind.h"
#include "lib/messaging/irpc.h"
#include "param/param.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "libcli/security/dom_sid.h"

static NTSTATUS get_info3_from_ndr(TALLOC_CTX *mem_ctx, struct smb_iconv_convenience *iconv_convenience, struct winbindd_response *response, struct netr_SamInfo3 *info3)
{
	size_t len = response->length - sizeof(struct winbindd_response);
	if (len > 4) {
		enum ndr_err_code ndr_err;
		DATA_BLOB blob;
		blob.length = len - 4;
		blob.data = (uint8_t *)(((char *)response->extra_data.data) + 4);

		ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, 
			       iconv_convenience, info3,
			      (ndr_pull_flags_fn_t)ndr_pull_netr_SamInfo3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}

		return NT_STATUS_OK;
	} else {
		DEBUG(2, ("get_info3_from_ndr: No info3 struct found!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
}

static NTSTATUS get_info3_from_wbcAuthUserInfo(TALLOC_CTX *mem_ctx,
					       struct smb_iconv_convenience *ic,
					       struct wbcAuthUserInfo *info,
					       struct netr_SamInfo3 *info3)
{
	int i, j;
	struct samr_RidWithAttribute *rids = NULL;

	info3->base.last_logon = info->logon_time;
	info3->base.last_logoff = info->logoff_time;
	info3->base.acct_expiry = info->kickoff_time;
	info3->base.last_password_change = info->pass_last_set_time;
	info3->base.allow_password_change = info->pass_can_change_time;
	info3->base.force_password_change = info->pass_must_change_time;

	info3->base.account_name.string = talloc_strdup(mem_ctx,
							info->account_name);
	info3->base.full_name.string = talloc_strdup(mem_ctx,
						     info->full_name);
	info3->base.logon_script.string = talloc_strdup(mem_ctx,
							info->logon_script);
	info3->base.profile_path.string = talloc_strdup(mem_ctx,
							info->profile_path);
	info3->base.home_directory.string = talloc_strdup(mem_ctx,
							  info->home_directory);
	info3->base.home_drive.string = talloc_strdup(mem_ctx,
						      info->home_drive);
	info3->base.logon_server.string = talloc_strdup(mem_ctx,
							info->logon_server);
	info3->base.domain.string = talloc_strdup(mem_ctx,
						  info->domain_name);

	info3->base.logon_count = info->logon_count;
	info3->base.bad_password_count = info->bad_password_count;
	info3->base.user_flags = info->user_flags;
	memcpy(info3->base.key.key, info->user_session_key,
	       sizeof(info3->base.key.key));
	memcpy(info3->base.LMSessKey.key, info->lm_session_key,
	       sizeof(info3->base.LMSessKey.key));
	info3->base.acct_flags = info->acct_flags;
	memset(info3->base.unknown, 0, sizeof(info3->base.unknown));

	if (info->num_sids < 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	dom_sid_split_rid(mem_ctx, (struct dom_sid2 *) &info->sids[0].sid,
			  &info3->base.domain_sid,
			  &info3->base.rid);
	dom_sid_split_rid(mem_ctx, (struct dom_sid2 *) &info->sids[1].sid, NULL,
			  &info3->base.primary_gid);

	/* We already handled the first two, now take care of the rest */
	info3->base.groups.count = info->num_sids - 2;

	rids = talloc_array(mem_ctx, struct samr_RidWithAttribute,
			    info3->base.groups.count);
	NT_STATUS_HAVE_NO_MEMORY(rids);

	for (i = 2, j = 0; i < info->num_sids; ++i, ++j) {
		rids[j].attributes = info->sids[i].attributes;
		dom_sid_split_rid(mem_ctx,
				  (struct dom_sid2 *) &info->sids[i].sid,
				  NULL, &rids[j].rid);
	}
	info3->base.groups.rids = rids;

	return NT_STATUS_OK;
}


static NTSTATUS winbind_want_check(struct auth_method_context *ctx,
				   TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info)
{
	if (!user_info->mapped.account_name || !*user_info->mapped.account_name) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	/* TODO: maybe limit the user scope to remote users only */
	return NT_STATUS_OK;
}

/*
 Authenticate a user with a challenge/response
 using the samba3 winbind protocol
*/
static NTSTATUS winbind_check_password_samba3(struct auth_method_context *ctx,
					      TALLOC_CTX *mem_ctx,
					      const struct auth_usersupplied_info *user_info, 
					      struct auth_serversupplied_info **server_info)
{
	struct winbindd_request request;
	struct winbindd_response response;
        NSS_STATUS result;
	NTSTATUS nt_status;
	struct netr_SamInfo3 info3;		

	/* Send off request */
	const struct auth_usersupplied_info *user_info_temp;	
	nt_status = encrypt_user_info(mem_ctx, ctx->auth_ctx, 
				      AUTH_PASSWORD_RESPONSE, 
				      user_info, &user_info_temp);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	user_info = user_info_temp;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);
	request.flags = WBFLAG_PAM_INFO3_NDR;

	request.data.auth_crap.logon_parameters = user_info->logon_parameters;

	safe_strcpy(request.data.auth_crap.user,
		       user_info->client.account_name, sizeof(fstring));
	safe_strcpy(request.data.auth_crap.domain,
		       user_info->client.domain_name, sizeof(fstring));
	safe_strcpy(request.data.auth_crap.workstation,
		       user_info->workstation_name, sizeof(fstring));

	memcpy(request.data.auth_crap.chal, ctx->auth_ctx->challenge.data.data, sizeof(request.data.auth_crap.chal));

	request.data.auth_crap.lm_resp_len = MIN(user_info->password.response.lanman.length,
						 sizeof(request.data.auth_crap.lm_resp));
	request.data.auth_crap.nt_resp_len = MIN(user_info->password.response.nt.length, 
						 sizeof(request.data.auth_crap.nt_resp));

	memcpy(request.data.auth_crap.lm_resp, user_info->password.response.lanman.data,
	       request.data.auth_crap.lm_resp_len);
	memcpy(request.data.auth_crap.nt_resp, user_info->password.response.nt.data,
	       request.data.auth_crap.nt_resp_len);

	result = winbindd_request_response(WINBINDD_PAM_AUTH_CRAP, &request, &response);

	nt_status = NT_STATUS(response.data.auth.nt_status);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (result == NSS_STATUS_SUCCESS && response.extra_data.data) {
		union netr_Validation validation;

		nt_status = get_info3_from_ndr(mem_ctx, lp_iconv_convenience(ctx->auth_ctx->lp_ctx), &response, &info3);
		SAFE_FREE(response.extra_data.data);
		NT_STATUS_NOT_OK_RETURN(nt_status); 

		validation.sam3 = &info3;
		nt_status = make_server_info_netlogon_validation(mem_ctx, 
								 user_info->client.account_name, 
								 3, &validation,
								 server_info);
		return nt_status;
	} else if (result == NSS_STATUS_SUCCESS && !response.extra_data.data) {
		DEBUG(0, ("Winbindd authenticated the user [%s]\\[%s], "
			  "but did not include the required info3 reply!\n", 
			  user_info->client.domain_name, user_info->client.account_name));
		return NT_STATUS_INSUFFICIENT_LOGON_INFO;
	} else if (NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("Winbindd authentication for [%s]\\[%s] failed, "
			  "but no error code is available!\n", 
			  user_info->client.domain_name, user_info->client.account_name));
		return NT_STATUS_NO_LOGON_SERVERS;
	}

        return nt_status;
}

struct winbind_check_password_state {
	struct winbind_SamLogon req;
};

/*
 Authenticate a user with a challenge/response
 using IRPC to the winbind task
*/
static NTSTATUS winbind_check_password(struct auth_method_context *ctx,
				       TALLOC_CTX *mem_ctx,
				       const struct auth_usersupplied_info *user_info, 
				       struct auth_serversupplied_info **server_info)
{
	NTSTATUS status;
	struct server_id *winbind_servers;
	struct winbind_check_password_state *s;
	const struct auth_usersupplied_info *user_info_new;
	struct netr_IdentityInfo *identity_info;

	s = talloc(mem_ctx, struct winbind_check_password_state);
	NT_STATUS_HAVE_NO_MEMORY(s);

	winbind_servers = irpc_servers_byname(ctx->auth_ctx->msg_ctx, s, "winbind_server");
	if ((winbind_servers == NULL) || (winbind_servers[0].id == 0)) {
		DEBUG(0, ("Winbind authentication for [%s]\\[%s] failed, " 
			  "no winbind_server running!\n",
			  user_info->client.domain_name, user_info->client.account_name));
		return NT_STATUS_NO_LOGON_SERVERS;
	}

	if (user_info->flags & USER_INFO_INTERACTIVE_LOGON) {
		struct netr_PasswordInfo *password_info;

		status = encrypt_user_info(s, ctx->auth_ctx, AUTH_PASSWORD_HASH,
					   user_info, &user_info_new);
		NT_STATUS_NOT_OK_RETURN(status);
		user_info = user_info_new;

		password_info = talloc(s, struct netr_PasswordInfo);
		NT_STATUS_HAVE_NO_MEMORY(password_info);

		password_info->lmpassword = *user_info->password.hash.lanman;
		password_info->ntpassword = *user_info->password.hash.nt;

		identity_info = &password_info->identity_info;
		s->req.in.logon_level	= 1;
		s->req.in.logon.password= password_info;
	} else {
		struct netr_NetworkInfo *network_info;
		const uint8_t *challenge;

		status = encrypt_user_info(s, ctx->auth_ctx, AUTH_PASSWORD_RESPONSE,
					   user_info, &user_info_new);
		NT_STATUS_NOT_OK_RETURN(status);
		user_info = user_info_new;

		network_info = talloc(s, struct netr_NetworkInfo);
		NT_STATUS_HAVE_NO_MEMORY(network_info);

		status = auth_get_challenge(ctx->auth_ctx, &challenge);
		NT_STATUS_NOT_OK_RETURN(status);

		memcpy(network_info->challenge, challenge, sizeof(network_info->challenge));

		network_info->nt.length = user_info->password.response.nt.length;
		network_info->nt.data	= user_info->password.response.nt.data;

		network_info->lm.length = user_info->password.response.lanman.length;
		network_info->lm.data	= user_info->password.response.lanman.data;

		identity_info = &network_info->identity_info;
		s->req.in.logon_level	= 2;
		s->req.in.logon.network = network_info;
	}

	identity_info->domain_name.string	= user_info->client.domain_name;
	identity_info->parameter_control	= user_info->logon_parameters; /* see MSV1_0_* */
	identity_info->logon_id_low		= 0;
	identity_info->logon_id_high		= 0;
	identity_info->account_name.string	= user_info->client.account_name;
	identity_info->workstation.string	= user_info->workstation_name;

	s->req.in.validation_level	= 3;

	status = IRPC_CALL(ctx->auth_ctx->msg_ctx, winbind_servers[0],
			   winbind, WINBIND_SAMLOGON,
			   &s->req, s);
	NT_STATUS_NOT_OK_RETURN(status);

	status = make_server_info_netlogon_validation(mem_ctx,
						      user_info->client.account_name,
						      s->req.in.validation_level,
						      &s->req.out.validation,
						      server_info);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

/*
 Authenticate a user with a challenge/response
 using the samba3 winbind protocol via libwbclient
*/
static NTSTATUS winbind_check_password_wbclient(struct auth_method_context *ctx,
						TALLOC_CTX *mem_ctx,
						const struct auth_usersupplied_info *user_info,
						struct auth_serversupplied_info **server_info)
{
	struct wbcAuthUserParams params;
	struct wbcAuthUserInfo *info = NULL;
	struct wbcAuthErrorInfo *err = NULL;
	wbcErr wbc_status;
	NTSTATUS nt_status;
	struct netr_SamInfo3 info3;
	union netr_Validation validation;


	/* Send off request */
	const struct auth_usersupplied_info *user_info_temp;
	nt_status = encrypt_user_info(mem_ctx, ctx->auth_ctx,
				      AUTH_PASSWORD_RESPONSE,
				      user_info, &user_info_temp);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	user_info = user_info_temp;

	ZERO_STRUCT(params);
	ZERO_STRUCT(info3);
	/*params.flags = WBFLAG_PAM_INFO3_NDR;*/

	params.parameter_control = user_info->logon_parameters;
	params.parameter_control |= WBC_MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT |
				    WBC_MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT;
	params.level = WBC_AUTH_USER_LEVEL_RESPONSE;

	params.account_name     = user_info->client.account_name;
	params.domain_name      = user_info->client.domain_name;
	params.workstation_name = user_info->workstation_name;

	d_fprintf(stderr, "looking up %s@%s logging in from %s\n",
		  params.account_name, params.domain_name,
		  params.workstation_name);

	memcpy(params.password.response.challenge,
	       ctx->auth_ctx->challenge.data.data,
	       sizeof(params.password.response.challenge));

	params.password.response.lm_length =
		user_info->password.response.lanman.length;
	params.password.response.nt_length =
		user_info->password.response.nt.length;

	params.password.response.lm_data =
		user_info->password.response.lanman.data;
	params.password.response.nt_data =
		user_info->password.response.nt.data;

	wbc_status = wbcAuthenticateUserEx(&params, &info, &err);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(1, ("error was %s (0x%08x)\nerror message was '%s'\n",
		      err->nt_string, err->nt_status, err->display_string));

		nt_status = NT_STATUS(err->nt_status);
		wbcFreeMemory(err);
		NT_STATUS_NOT_OK_RETURN(nt_status);
	}
	nt_status = get_info3_from_wbcAuthUserInfo(mem_ctx,
				lp_iconv_convenience(ctx->auth_ctx->lp_ctx),
				info, &info3);
	wbcFreeMemory(info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	validation.sam3 = &info3;
	nt_status = make_server_info_netlogon_validation(mem_ctx,
					user_info->client.account_name,
					3, &validation, server_info);
	return nt_status;

}

static const struct auth_operations winbind_samba3_ops = {
	.name		= "winbind_samba3",
	.get_challenge	= auth_get_challenge_not_implemented,
	.want_check	= winbind_want_check,
	.check_password	= winbind_check_password_samba3
};

static const struct auth_operations winbind_ops = {
	.name		= "winbind",
	.get_challenge	= auth_get_challenge_not_implemented,
	.want_check	= winbind_want_check,
	.check_password	= winbind_check_password
};

static const struct auth_operations winbind_wbclient_ops = {
	.name		= "winbind_wbclient",
	.get_challenge	= auth_get_challenge_not_implemented,
	.want_check	= winbind_want_check,
	.check_password	= winbind_check_password_wbclient
};

_PUBLIC_ NTSTATUS auth_winbind_init(void)
{
	NTSTATUS ret;

	ret = auth_register(&winbind_samba3_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'winbind_samba3' auth backend!\n"));
		return ret;
	}

	ret = auth_register(&winbind_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'winbind' auth backend!\n"));
		return ret;
	}

	ret = auth_register(&winbind_wbclient_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'winbind_wbclient' auth backend!\n"));
		return ret;
	}

	return NT_STATUS_OK;
}
