/*
 *  Unix SMB/CIFS implementation.
 *  NetApi User Support
 *  Copyright (C) Guenther Deschner 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#include "librpc/gen_ndr/libnetapi.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/init_samr.h"
#include "../libds/common/flags.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"
#include "../libds/common/flag_mapping.h"
#include "rpc_client/cli_pipe.h"

/****************************************************************
****************************************************************/

static void convert_USER_INFO_X_to_samr_user_info21(struct USER_INFO_X *infoX,
						    struct samr_UserInfo21 *info21)
{
	uint32_t fields_present = 0;
	struct samr_LogonHours zero_logon_hours;
	struct lsa_BinaryString zero_parameters;
	NTTIME password_age;

	ZERO_STRUCTP(info21);
	ZERO_STRUCT(zero_logon_hours);
	ZERO_STRUCT(zero_parameters);

	if (infoX->usriX_flags) {
		fields_present |= SAMR_FIELD_ACCT_FLAGS;
	}
	if (infoX->usriX_name) {
		fields_present |= SAMR_FIELD_ACCOUNT_NAME;
	}
	if (infoX->usriX_password) {
		fields_present |= SAMR_FIELD_NT_PASSWORD_PRESENT;
	}
	if (infoX->usriX_flags) {
		fields_present |= SAMR_FIELD_ACCT_FLAGS;
	}
	if (infoX->usriX_home_dir) {
		fields_present |= SAMR_FIELD_HOME_DIRECTORY;
	}
	if (infoX->usriX_script_path) {
		fields_present |= SAMR_FIELD_LOGON_SCRIPT;
	}
	if (infoX->usriX_comment) {
		fields_present |= SAMR_FIELD_DESCRIPTION;
	}
	if (infoX->usriX_password_age) {
		fields_present |= SAMR_FIELD_FORCE_PWD_CHANGE;
	}
	if (infoX->usriX_full_name) {
		fields_present |= SAMR_FIELD_FULL_NAME;
	}
	if (infoX->usriX_usr_comment) {
		fields_present |= SAMR_FIELD_COMMENT;
	}
	if (infoX->usriX_profile) {
		fields_present |= SAMR_FIELD_PROFILE_PATH;
	}
	if (infoX->usriX_home_dir_drive) {
		fields_present |= SAMR_FIELD_HOME_DRIVE;
	}
	if (infoX->usriX_primary_group_id) {
		fields_present |= SAMR_FIELD_PRIMARY_GID;
	}
	if (infoX->usriX_country_code) {
		fields_present |= SAMR_FIELD_COUNTRY_CODE;
	}
	if (infoX->usriX_workstations) {
		fields_present |= SAMR_FIELD_WORKSTATIONS;
	}

	unix_to_nt_time_abs(&password_age, infoX->usriX_password_age);

	/* TODO: infoX->usriX_priv */

	info21->last_logon		= 0;
	info21->last_logoff		= 0;
	info21->last_password_change	= 0;
	info21->acct_expiry		= 0;
	info21->allow_password_change	= 0;
	info21->force_password_change	= 0;
	info21->account_name.string	= infoX->usriX_name;
	info21->full_name.string	= infoX->usriX_full_name;
	info21->home_directory.string	= infoX->usriX_home_dir;
	info21->home_drive.string	= infoX->usriX_home_dir_drive;
	info21->logon_script.string	= infoX->usriX_script_path;
	info21->profile_path.string	= infoX->usriX_profile;
	info21->description.string	= infoX->usriX_comment;
	info21->workstations.string	= infoX->usriX_workstations;
	info21->comment.string		= infoX->usriX_usr_comment;
	info21->parameters		= zero_parameters;
	info21->lm_owf_password		= zero_parameters;
	info21->nt_owf_password		= zero_parameters;
	info21->private_data.string	= NULL;
	info21->buf_count		= 0;
	info21->buffer			= NULL;
	info21->rid			= infoX->usriX_user_id;
	info21->primary_gid		= infoX->usriX_primary_group_id;
	info21->acct_flags		= infoX->usriX_flags;
	info21->fields_present		= fields_present;
	info21->logon_hours		= zero_logon_hours;
	info21->bad_password_count	= infoX->usriX_bad_pw_count;
	info21->logon_count		= infoX->usriX_num_logons;
	info21->country_code		= infoX->usriX_country_code;
	info21->code_page		= infoX->usriX_code_page;
	info21->lm_password_set		= 0;
	info21->nt_password_set		= 0;
	info21->password_expired	= infoX->usriX_password_expired;
	info21->private_data_sensitive	= 0;
}

/****************************************************************
****************************************************************/

static NTSTATUS construct_USER_INFO_X(uint32_t level,
				      uint8_t *buffer,
				      struct USER_INFO_X *uX)
{
	struct USER_INFO_0 *u0 = NULL;
	struct USER_INFO_1 *u1 = NULL;
	struct USER_INFO_2 *u2 = NULL;
	struct USER_INFO_3 *u3 = NULL;
	struct USER_INFO_1003 *u1003 = NULL;
	struct USER_INFO_1006 *u1006 = NULL;
	struct USER_INFO_1007 *u1007 = NULL;
	struct USER_INFO_1009 *u1009 = NULL;
	struct USER_INFO_1011 *u1011 = NULL;
	struct USER_INFO_1012 *u1012 = NULL;
	struct USER_INFO_1014 *u1014 = NULL;
	struct USER_INFO_1024 *u1024 = NULL;
	struct USER_INFO_1051 *u1051 = NULL;
	struct USER_INFO_1052 *u1052 = NULL;
	struct USER_INFO_1053 *u1053 = NULL;

	if (!buffer || !uX) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCTP(uX);

	switch (level) {
		case 0:
			u0 = (struct USER_INFO_0 *)buffer;
			uX->usriX_name		= u0->usri0_name;
			break;
		case 1:
			u1 = (struct USER_INFO_1 *)buffer;
			uX->usriX_name		= u1->usri1_name;
			uX->usriX_password	= u1->usri1_password;
			uX->usriX_password_age	= u1->usri1_password_age;
			uX->usriX_priv		= u1->usri1_priv;
			uX->usriX_home_dir	= u1->usri1_home_dir;
			uX->usriX_comment	= u1->usri1_comment;
			uX->usriX_flags		= u1->usri1_flags;
			uX->usriX_script_path	= u1->usri1_script_path;
			break;
		case 2:
			u2 = (struct USER_INFO_2 *)buffer;
			uX->usriX_name		= u2->usri2_name;
			uX->usriX_password	= u2->usri2_password;
			uX->usriX_password_age	= u2->usri2_password_age;
			uX->usriX_priv		= u2->usri2_priv;
			uX->usriX_home_dir	= u2->usri2_home_dir;
			uX->usriX_comment	= u2->usri2_comment;
			uX->usriX_flags		= u2->usri2_flags;
			uX->usriX_script_path	= u2->usri2_script_path;
			uX->usriX_auth_flags	= u2->usri2_auth_flags;
			uX->usriX_full_name	= u2->usri2_full_name;
			uX->usriX_usr_comment	= u2->usri2_usr_comment;
			uX->usriX_parms		= u2->usri2_parms;
			uX->usriX_workstations	= u2->usri2_workstations;
			uX->usriX_last_logon	= u2->usri2_last_logon;
			uX->usriX_last_logoff	= u2->usri2_last_logoff;
			uX->usriX_acct_expires	= u2->usri2_acct_expires;
			uX->usriX_max_storage	= u2->usri2_max_storage;
			uX->usriX_units_per_week= u2->usri2_units_per_week;
			uX->usriX_logon_hours	= u2->usri2_logon_hours;
			uX->usriX_bad_pw_count	= u2->usri2_bad_pw_count;
			uX->usriX_num_logons	= u2->usri2_num_logons;
			uX->usriX_logon_server	= u2->usri2_logon_server;
			uX->usriX_country_code	= u2->usri2_country_code;
			uX->usriX_code_page	= u2->usri2_code_page;
			break;
		case 3:
			u3 = (struct USER_INFO_3 *)buffer;
			uX->usriX_name		= u3->usri3_name;
			uX->usriX_password_age	= u3->usri3_password_age;
			uX->usriX_priv		= u3->usri3_priv;
			uX->usriX_home_dir	= u3->usri3_home_dir;
			uX->usriX_comment	= u3->usri3_comment;
			uX->usriX_flags		= u3->usri3_flags;
			uX->usriX_script_path	= u3->usri3_script_path;
			uX->usriX_auth_flags	= u3->usri3_auth_flags;
			uX->usriX_full_name	= u3->usri3_full_name;
			uX->usriX_usr_comment	= u3->usri3_usr_comment;
			uX->usriX_parms		= u3->usri3_parms;
			uX->usriX_workstations	= u3->usri3_workstations;
			uX->usriX_last_logon	= u3->usri3_last_logon;
			uX->usriX_last_logoff	= u3->usri3_last_logoff;
			uX->usriX_acct_expires	= u3->usri3_acct_expires;
			uX->usriX_max_storage	= u3->usri3_max_storage;
			uX->usriX_units_per_week= u3->usri3_units_per_week;
			uX->usriX_logon_hours	= u3->usri3_logon_hours;
			uX->usriX_bad_pw_count	= u3->usri3_bad_pw_count;
			uX->usriX_num_logons	= u3->usri3_num_logons;
			uX->usriX_logon_server	= u3->usri3_logon_server;
			uX->usriX_country_code	= u3->usri3_country_code;
			uX->usriX_code_page	= u3->usri3_code_page;
			uX->usriX_user_id	= u3->usri3_user_id;
			uX->usriX_primary_group_id = u3->usri3_primary_group_id;
			uX->usriX_profile	= u3->usri3_profile;
			uX->usriX_home_dir_drive = u3->usri3_home_dir_drive;
			uX->usriX_password_expired = u3->usri3_password_expired;
			break;
		case 1003:
			u1003 = (struct USER_INFO_1003 *)buffer;
			uX->usriX_password	= u1003->usri1003_password;
			break;
		case 1006:
			u1006 = (struct USER_INFO_1006 *)buffer;
			uX->usriX_home_dir	= u1006->usri1006_home_dir;
			break;
		case 1007:
			u1007 = (struct USER_INFO_1007 *)buffer;
			uX->usriX_comment	= u1007->usri1007_comment;
			break;
		case 1009:
			u1009 = (struct USER_INFO_1009 *)buffer;
			uX->usriX_script_path	= u1009->usri1009_script_path;
			break;
		case 1011:
			u1011 = (struct USER_INFO_1011 *)buffer;
			uX->usriX_full_name	= u1011->usri1011_full_name;
			break;
		case 1012:
			u1012 = (struct USER_INFO_1012 *)buffer;
			uX->usriX_usr_comment	= u1012->usri1012_usr_comment;
			break;
		case 1014:
			u1014 = (struct USER_INFO_1014 *)buffer;
			uX->usriX_workstations	= u1014->usri1014_workstations;
			break;
		case 1024:
			u1024 = (struct USER_INFO_1024 *)buffer;
			uX->usriX_country_code	= u1024->usri1024_country_code;
			break;
		case 1051:
			u1051 = (struct USER_INFO_1051 *)buffer;
			uX->usriX_primary_group_id = u1051->usri1051_primary_group_id;
			break;
		case 1052:
			u1052 = (struct USER_INFO_1052 *)buffer;
			uX->usriX_profile	= u1052->usri1052_profile;
			break;
		case 1053:
			u1053 = (struct USER_INFO_1053 *)buffer;
			uX->usriX_home_dir_drive = u1053->usri1053_home_dir_drive;
			break;
		case 4:
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS set_user_info_USER_INFO_X(TALLOC_CTX *ctx,
					  struct rpc_pipe_client *pipe_cli,
					  DATA_BLOB *session_key,
					  struct policy_handle *user_handle,
					  struct USER_INFO_X *uX)
{
	union samr_UserInfo user_info;
	struct samr_UserInfo21 info21;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	if (!uX) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	convert_USER_INFO_X_to_samr_user_info21(uX, &info21);

	ZERO_STRUCT(user_info);

	if (uX->usriX_password) {

		user_info.info25.info = info21;

		init_samr_CryptPasswordEx(uX->usriX_password,
					  session_key,
					  &user_info.info25.password);

		status = dcerpc_samr_SetUserInfo2(b, talloc_tos(),
						  user_handle,
						  25,
						  &user_info,
						  &result);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE)) {

			user_info.info23.info = info21;

			init_samr_CryptPassword(uX->usriX_password,
						session_key,
						&user_info.info23.password);

			status = dcerpc_samr_SetUserInfo2(b, talloc_tos(),
							  user_handle,
							  23,
							  &user_info,
							  &result);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else {

		user_info.info21 = info21;

		status = dcerpc_samr_SetUserInfo(b, talloc_tos(),
						 user_handle,
						 21,
						 &user_info,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return result;
}

/****************************************************************
****************************************************************/

WERROR NetUserAdd_r(struct libnetapi_ctx *ctx,
		    struct NetUserAdd *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, user_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	union samr_UserInfo *user_info = NULL;
	struct samr_PwInfo pw_info;
	uint32_t access_granted = 0;
	uint32_t rid = 0;
	struct USER_INFO_X uX;
	struct dcerpc_binding_handle *b = NULL;
	DATA_BLOB session_key;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(user_handle);

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 1:
			break;
		case 2:
		case 3:
		case 4:
		default:
			werr = WERR_NOT_SUPPORTED;
			goto done;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	status = construct_USER_INFO_X(r->in.level, r->in.buffer, &uX);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1 |
					  SAMR_DOMAIN_ACCESS_CREATE_USER |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, uX.usriX_name);

	status = dcerpc_samr_CreateUser2(b, talloc_tos(),
					 &domain_handle,
					 &lsa_account_name,
					 ACB_NORMAL,
					 SEC_STD_WRITE_DAC |
					 SEC_STD_DELETE |
					 SAMR_USER_ACCESS_SET_PASSWORD |
					 SAMR_USER_ACCESS_SET_ATTRIBUTES |
					 SAMR_USER_ACCESS_GET_ATTRIBUTES,
					 &user_handle,
					 &access_granted,
					 &rid,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = dcerpc_samr_QueryUserInfo(b, talloc_tos(),
					   &user_handle,
					   16,
					   &user_info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	if (!(user_info->info16.acct_flags & ACB_NORMAL)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	status = dcerpc_samr_GetUserPwInfo(b, talloc_tos(),
					   &user_handle,
					   &pw_info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	uX.usriX_flags |= ACB_NORMAL;

	status = set_user_info_USER_INFO_X(ctx, pipe_cli,
					   &session_key,
					   &user_handle,
					   &uX);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto failed;
	}

	werr = WERR_OK;
	goto done;

 failed:
	dcerpc_samr_DeleteUser(b, talloc_tos(),
			       &user_handle,
			       &result);

 done:
	if (is_valid_policy_hnd(&user_handle) && b) {
		dcerpc_samr_Close(b, talloc_tos(), &user_handle, &result);
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserAdd_l(struct libnetapi_ctx *ctx,
		    struct NetUserAdd *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserAdd);
}

/****************************************************************
****************************************************************/

WERROR NetUserDel_r(struct libnetapi_ctx *ctx,
		    struct NetUserDel *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, builtin_handle, domain_handle, user_handle;
	struct lsa_String lsa_account_name;
	struct samr_Ids user_rids, name_types;
	struct dom_sid2 *domain_sid = NULL;
	struct dom_sid2 user_sid;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(user_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_samr_OpenDomain(b, talloc_tos(),
					&connect_handle,
					SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					CONST_DISCARD(struct dom_sid *, &global_sid_Builtin),
					&builtin_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, talloc_tos(),
				      &domain_handle,
				      SEC_STD_DELETE,
				      user_rids.ids[0],
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	sid_compose(&user_sid, domain_sid, user_rids.ids[0]);

	status = dcerpc_samr_RemoveMemberFromForeignDomain(b, talloc_tos(),
							   &builtin_handle,
							   &user_sid,
							   &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = dcerpc_samr_DeleteUser(b, talloc_tos(),
					&user_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&user_handle)) {
		dcerpc_samr_Close(b, talloc_tos(), &user_handle, &result);
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserDel_l(struct libnetapi_ctx *ctx,
		    struct NetUserDel *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserDel);
}

/****************************************************************
****************************************************************/

static NTSTATUS libnetapi_samr_lookup_user(TALLOC_CTX *mem_ctx,
					   struct rpc_pipe_client *pipe_cli,
					   struct policy_handle *domain_handle,
					   struct policy_handle *builtin_handle,
					   const char *user_name,
					   const struct dom_sid *domain_sid,
					   uint32_t rid,
					   uint32_t level,
					   struct samr_UserInfo21 **info21,
					   struct sec_desc_buf **sec_desc,
					   uint32_t *auth_flag_p)
{
	NTSTATUS status, result;

	struct policy_handle user_handle;
	union samr_UserInfo *user_info = NULL;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	uint32_t access_mask = SEC_STD_READ_CONTROL |
			       SAMR_USER_ACCESS_GET_ATTRIBUTES |
			       SAMR_USER_ACCESS_GET_NAME_ETC;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	ZERO_STRUCT(user_handle);

	switch (level) {
		case 0:
			break;
		case 1:
			access_mask |= SAMR_USER_ACCESS_GET_LOGONINFO |
				       SAMR_USER_ACCESS_GET_GROUPS;
			break;
		case 2:
		case 3:
		case 4:
		case 11:
			access_mask |= SAMR_USER_ACCESS_GET_LOGONINFO |
				       SAMR_USER_ACCESS_GET_GROUPS |
				       SAMR_USER_ACCESS_GET_LOCALE;
			break;
		case 10:
		case 20:
		case 23:
			break;
		default:
			return NT_STATUS_INVALID_LEVEL;
	}

	if (level == 0) {
		return NT_STATUS_OK;
	}

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      domain_handle,
				      access_mask,
				      rid,
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
					   &user_handle,
					   21,
					   &user_info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_QuerySecurity(b, mem_ctx,
					   &user_handle,
					   SECINFO_DACL,
					   sec_desc,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	if (access_mask & SAMR_USER_ACCESS_GET_GROUPS) {

		struct lsa_SidArray sid_array;
		struct samr_Ids alias_rids;
		int i;
		uint32_t auth_flag = 0;
		struct dom_sid sid;

		status = dcerpc_samr_GetGroupsForUser(b, mem_ctx,
						      &user_handle,
						      &rid_array,
						      &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			goto done;
		}

		sid_array.num_sids = rid_array->count + 1;
		sid_array.sids = talloc_array(mem_ctx, struct lsa_SidPtr,
					      sid_array.num_sids);
		NT_STATUS_HAVE_NO_MEMORY(sid_array.sids);

		for (i=0; i<rid_array->count; i++) {
			sid_compose(&sid, domain_sid, rid_array->rids[i].rid);
			sid_array.sids[i].sid = dom_sid_dup(mem_ctx, &sid);
			NT_STATUS_HAVE_NO_MEMORY(sid_array.sids[i].sid);
		}

		sid_compose(&sid, domain_sid, rid);
		sid_array.sids[i].sid = dom_sid_dup(mem_ctx, &sid);
		NT_STATUS_HAVE_NO_MEMORY(sid_array.sids[i].sid);

		status = dcerpc_samr_GetAliasMembership(b, mem_ctx,
							builtin_handle,
							&sid_array,
							&alias_rids,
							&result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			goto done;
		}

		for (i=0; i<alias_rids.count; i++) {
			switch (alias_rids.ids[i]) {
				case 550: /* Print Operators */
					auth_flag |= AF_OP_PRINT;
					break;
				case 549: /* Server Operators */
					auth_flag |= AF_OP_SERVER;
					break;
				case 548: /* Account Operators */
					auth_flag |= AF_OP_ACCOUNTS;
					break;
				default:
					break;
			}
		}

		if (auth_flag_p) {
			*auth_flag_p = auth_flag;
		}
	}

	*info21 = &user_info->info21;

 done:
	if (is_valid_policy_hnd(&user_handle)) {
		dcerpc_samr_Close(b, mem_ctx, &user_handle, &result);
	}

	return status;
}

/****************************************************************
****************************************************************/

static uint32_t samr_rid_to_priv_level(uint32_t rid)
{
	switch (rid) {
		case DOMAIN_RID_ADMINISTRATOR:
			return USER_PRIV_ADMIN;
		case DOMAIN_RID_GUEST:
			return USER_PRIV_GUEST;
		default:
			return USER_PRIV_USER;
	}
}

/****************************************************************
****************************************************************/

static uint32_t samr_acb_flags_to_netapi_flags(uint32_t acb)
{
	uint32_t fl = UF_SCRIPT; /* god knows why */

	fl |= ds_acb2uf(acb);

	return fl;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_1(TALLOC_CTX *mem_ctx,
				      const struct samr_UserInfo21 *i21,
				      struct USER_INFO_1 *i)
{
	ZERO_STRUCTP(i);
	i->usri1_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri1_name);
	i->usri1_password	= NULL;
	i->usri1_password_age	= time(NULL) - nt_time_to_unix(i21->last_password_change);
	i->usri1_priv		= samr_rid_to_priv_level(i21->rid);
	i->usri1_home_dir	= talloc_strdup(mem_ctx, i21->home_directory.string);
	i->usri1_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri1_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	i->usri1_script_path	= talloc_strdup(mem_ctx, i21->logon_script.string);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_2(TALLOC_CTX *mem_ctx,
				      const struct samr_UserInfo21 *i21,
				      uint32_t auth_flag,
				      struct USER_INFO_2 *i)
{
	ZERO_STRUCTP(i);

	i->usri2_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri2_name);
	i->usri2_password	= NULL;
	i->usri2_password_age	= time(NULL) - nt_time_to_unix(i21->last_password_change);
	i->usri2_priv		= samr_rid_to_priv_level(i21->rid);
	i->usri2_home_dir	= talloc_strdup(mem_ctx, i21->home_directory.string);
	i->usri2_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri2_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	i->usri2_script_path	= talloc_strdup(mem_ctx, i21->logon_script.string);
	i->usri2_auth_flags	= auth_flag;
	i->usri2_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri2_usr_comment	= talloc_strdup(mem_ctx, i21->comment.string);
	i->usri2_parms		= talloc_strndup(mem_ctx, (const char *)i21->parameters.array, i21->parameters.size/2);
	i->usri2_workstations	= talloc_strdup(mem_ctx, i21->workstations.string);
	i->usri2_last_logon	= nt_time_to_unix(i21->last_logon);
	i->usri2_last_logoff	= nt_time_to_unix(i21->last_logoff);
	i->usri2_acct_expires	= nt_time_to_unix(i21->acct_expiry);
	i->usri2_max_storage	= USER_MAXSTORAGE_UNLIMITED; /* FIXME */
	i->usri2_units_per_week	= i21->logon_hours.units_per_week;
	i->usri2_logon_hours	= (uint8_t *)talloc_memdup(mem_ctx, i21->logon_hours.bits, 21);
	i->usri2_bad_pw_count	= i21->bad_password_count;
	i->usri2_num_logons	= i21->logon_count;
	i->usri2_logon_server	= talloc_strdup(mem_ctx, "\\\\*");
	i->usri2_country_code	= i21->country_code;
	i->usri2_code_page	= i21->code_page;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_3(TALLOC_CTX *mem_ctx,
				      const struct samr_UserInfo21 *i21,
				      uint32_t auth_flag,
				      struct USER_INFO_3 *i)
{
	ZERO_STRUCTP(i);

	i->usri3_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri3_name);
	i->usri3_password_age	= time(NULL) - nt_time_to_unix(i21->last_password_change);
	i->usri3_priv		= samr_rid_to_priv_level(i21->rid);
	i->usri3_home_dir	= talloc_strdup(mem_ctx, i21->home_directory.string);
	i->usri3_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri3_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	i->usri3_script_path	= talloc_strdup(mem_ctx, i21->logon_script.string);
	i->usri3_auth_flags	= auth_flag;
	i->usri3_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri3_usr_comment	= talloc_strdup(mem_ctx, i21->comment.string);
	i->usri3_parms		= talloc_strndup(mem_ctx, (const char *)i21->parameters.array, i21->parameters.size/2);
	i->usri3_workstations	= talloc_strdup(mem_ctx, i21->workstations.string);
	i->usri3_last_logon	= nt_time_to_unix(i21->last_logon);
	i->usri3_last_logoff	= nt_time_to_unix(i21->last_logoff);
	i->usri3_acct_expires	= nt_time_to_unix(i21->acct_expiry);
	i->usri3_max_storage	= USER_MAXSTORAGE_UNLIMITED; /* FIXME */
	i->usri3_units_per_week	= i21->logon_hours.units_per_week;
	i->usri3_logon_hours	= (uint8_t *)talloc_memdup(mem_ctx, i21->logon_hours.bits, 21);
	i->usri3_bad_pw_count	= i21->bad_password_count;
	i->usri3_num_logons	= i21->logon_count;
	i->usri3_logon_server	= talloc_strdup(mem_ctx, "\\\\*");
	i->usri3_country_code	= i21->country_code;
	i->usri3_code_page	= i21->code_page;
	i->usri3_user_id	= i21->rid;
	i->usri3_primary_group_id = i21->primary_gid;
	i->usri3_profile	= talloc_strdup(mem_ctx, i21->profile_path.string);
	i->usri3_home_dir_drive	= talloc_strdup(mem_ctx, i21->home_drive.string);
	i->usri3_password_expired = i21->password_expired;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_4(TALLOC_CTX *mem_ctx,
				      const struct samr_UserInfo21 *i21,
				      uint32_t auth_flag,
				      struct dom_sid *domain_sid,
				      struct USER_INFO_4 *i)
{
	struct dom_sid sid;

	ZERO_STRUCTP(i);

	i->usri4_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri4_name);
	i->usri4_password_age	= time(NULL) - nt_time_to_unix(i21->last_password_change);
	i->usri4_password	= NULL;
	i->usri4_priv		= samr_rid_to_priv_level(i21->rid);
	i->usri4_home_dir	= talloc_strdup(mem_ctx, i21->home_directory.string);
	i->usri4_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri4_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	i->usri4_script_path	= talloc_strdup(mem_ctx, i21->logon_script.string);
	i->usri4_auth_flags	= auth_flag;
	i->usri4_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri4_usr_comment	= talloc_strdup(mem_ctx, i21->comment.string);
	i->usri4_parms		= talloc_strndup(mem_ctx, (const char *)i21->parameters.array, i21->parameters.size/2);
	i->usri4_workstations	= talloc_strdup(mem_ctx, i21->workstations.string);
	i->usri4_last_logon	= nt_time_to_unix(i21->last_logon);
	i->usri4_last_logoff	= nt_time_to_unix(i21->last_logoff);
	i->usri4_acct_expires	= nt_time_to_unix(i21->acct_expiry);
	i->usri4_max_storage	= USER_MAXSTORAGE_UNLIMITED; /* FIXME */
	i->usri4_units_per_week	= i21->logon_hours.units_per_week;
	i->usri4_logon_hours	= (uint8_t *)talloc_memdup(mem_ctx, i21->logon_hours.bits, 21);
	i->usri4_bad_pw_count	= i21->bad_password_count;
	i->usri4_num_logons	= i21->logon_count;
	i->usri4_logon_server	= talloc_strdup(mem_ctx, "\\\\*");
	i->usri4_country_code	= i21->country_code;
	i->usri4_code_page	= i21->code_page;
	if (!sid_compose(&sid, domain_sid, i21->rid)) {
		return NT_STATUS_NO_MEMORY;
	}
	i->usri4_user_sid	= (struct domsid *)dom_sid_dup(mem_ctx, &sid);
	i->usri4_primary_group_id = i21->primary_gid;
	i->usri4_profile	= talloc_strdup(mem_ctx, i21->profile_path.string);
	i->usri4_home_dir_drive	= talloc_strdup(mem_ctx, i21->home_drive.string);
	i->usri4_password_expired = i21->password_expired;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_10(TALLOC_CTX *mem_ctx,
				       const struct samr_UserInfo21 *i21,
				       struct USER_INFO_10 *i)
{
	ZERO_STRUCTP(i);

	i->usri10_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri10_name);
	i->usri10_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri10_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri10_usr_comment	= talloc_strdup(mem_ctx, i21->comment.string);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_11(TALLOC_CTX *mem_ctx,
				       const struct samr_UserInfo21 *i21,
				       uint32_t auth_flag,
				       struct USER_INFO_11 *i)
{
	ZERO_STRUCTP(i);

	i->usri11_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri11_name);
	i->usri11_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri11_usr_comment	= talloc_strdup(mem_ctx, i21->comment.string);
	i->usri11_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri11_priv		= samr_rid_to_priv_level(i21->rid);
	i->usri11_auth_flags	= auth_flag;
	i->usri11_password_age	= time(NULL) - nt_time_to_unix(i21->last_password_change);
	i->usri11_home_dir	= talloc_strdup(mem_ctx, i21->home_directory.string);
	i->usri11_parms		= talloc_strndup(mem_ctx, (const char *)i21->parameters.array, i21->parameters.size/2);
	i->usri11_last_logon	= nt_time_to_unix(i21->last_logon);
	i->usri11_last_logoff	= nt_time_to_unix(i21->last_logoff);
	i->usri11_bad_pw_count	= i21->bad_password_count;
	i->usri11_num_logons	= i21->logon_count;
	i->usri11_logon_server	= talloc_strdup(mem_ctx, "\\\\*");
	i->usri11_country_code	= i21->country_code;
	i->usri11_workstations	= talloc_strdup(mem_ctx, i21->workstations.string);
	i->usri11_max_storage	= USER_MAXSTORAGE_UNLIMITED; /* FIXME */
	i->usri11_units_per_week = i21->logon_hours.units_per_week;
	i->usri11_logon_hours	= (uint8_t *)talloc_memdup(mem_ctx, i21->logon_hours.bits, 21);
	i->usri11_code_page	= i21->code_page;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_20(TALLOC_CTX *mem_ctx,
				       const struct samr_UserInfo21 *i21,
				       struct USER_INFO_20 *i)
{
	ZERO_STRUCTP(i);

	i->usri20_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri20_name);
	i->usri20_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri20_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri20_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	i->usri20_user_id	= i21->rid;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS info21_to_USER_INFO_23(TALLOC_CTX *mem_ctx,
				       const struct samr_UserInfo21 *i21,
				       struct dom_sid *domain_sid,
				       struct USER_INFO_23 *i)
{
	struct dom_sid sid;

	ZERO_STRUCTP(i);

	i->usri23_name		= talloc_strdup(mem_ctx, i21->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(i->usri23_name);
	i->usri23_comment	= talloc_strdup(mem_ctx, i21->description.string);
	i->usri23_full_name	= talloc_strdup(mem_ctx, i21->full_name.string);
	i->usri23_flags		= samr_acb_flags_to_netapi_flags(i21->acct_flags);
	if (!sid_compose(&sid, domain_sid, i21->rid)) {
		return NT_STATUS_NO_MEMORY;
	}
	i->usri23_user_sid	= (struct domsid *)dom_sid_dup(mem_ctx, &sid);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS libnetapi_samr_lookup_user_map_USER_INFO(TALLOC_CTX *mem_ctx,
							 struct rpc_pipe_client *pipe_cli,
							 struct dom_sid *domain_sid,
							 struct policy_handle *domain_handle,
							 struct policy_handle *builtin_handle,
							 const char *user_name,
							 uint32_t rid,
							 uint32_t level,
							 uint8_t **buffer,
							 uint32_t *num_entries)
{
	NTSTATUS status;

	struct samr_UserInfo21 *info21 = NULL;
	struct sec_desc_buf *sec_desc = NULL;
	uint32_t auth_flag = 0;

	struct USER_INFO_0 info0;
	struct USER_INFO_1 info1;
	struct USER_INFO_2 info2;
	struct USER_INFO_3 info3;
	struct USER_INFO_4 info4;
	struct USER_INFO_10 info10;
	struct USER_INFO_11 info11;
	struct USER_INFO_20 info20;
	struct USER_INFO_23 info23;

	switch (level) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 10:
		case 11:
		case 20:
		case 23:
			break;
		default:
			return NT_STATUS_INVALID_LEVEL;
	}

	if (level == 0) {
		info0.usri0_name = talloc_strdup(mem_ctx, user_name);
		NT_STATUS_HAVE_NO_MEMORY(info0.usri0_name);

		ADD_TO_ARRAY(mem_ctx, struct USER_INFO_0, info0,
			     (struct USER_INFO_0 **)buffer, num_entries);

		return NT_STATUS_OK;
	}

	status = libnetapi_samr_lookup_user(mem_ctx, pipe_cli,
					    domain_handle,
					    builtin_handle,
					    user_name,
					    domain_sid,
					    rid,
					    level,
					    &info21,
					    &sec_desc,
					    &auth_flag);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	switch (level) {
		case 0:
			/* already returned above */
			break;
		case 1:
			status = info21_to_USER_INFO_1(mem_ctx, info21, &info1);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_1, info1,
				     (struct USER_INFO_1 **)buffer, num_entries);

			break;
		case 2:
			status = info21_to_USER_INFO_2(mem_ctx, info21, auth_flag, &info2);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_2, info2,
				     (struct USER_INFO_2 **)buffer, num_entries);

			break;
		case 3:
			status = info21_to_USER_INFO_3(mem_ctx, info21, auth_flag, &info3);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_3, info3,
				     (struct USER_INFO_3 **)buffer, num_entries);

			break;
		case 4:
			status = info21_to_USER_INFO_4(mem_ctx, info21, auth_flag, domain_sid, &info4);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_4, info4,
				     (struct USER_INFO_4 **)buffer, num_entries);

			break;
		case 10:
			status = info21_to_USER_INFO_10(mem_ctx, info21, &info10);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_10, info10,
				     (struct USER_INFO_10 **)buffer, num_entries);

			break;
		case 11:
			status = info21_to_USER_INFO_11(mem_ctx, info21, auth_flag, &info11);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_11, info11,
				     (struct USER_INFO_11 **)buffer, num_entries);

			break;
		case 20:
			status = info21_to_USER_INFO_20(mem_ctx, info21, &info20);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_20, info20,
				     (struct USER_INFO_20 **)buffer, num_entries);

			break;
		case 23:
			status = info21_to_USER_INFO_23(mem_ctx, info21, domain_sid, &info23);
			NT_STATUS_NOT_OK_RETURN(status);

			ADD_TO_ARRAY(mem_ctx, struct USER_INFO_23, info23,
				     (struct USER_INFO_23 **)buffer, num_entries);
			break;
		default:
			return NT_STATUS_INVALID_LEVEL;
	}

 done:
	return status;
}

/****************************************************************
****************************************************************/

WERROR NetUserEnum_r(struct libnetapi_ctx *ctx,
		     struct NetUserEnum *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle;
	struct dom_sid2 *domain_sid = NULL;
	struct policy_handle domain_handle, builtin_handle;
	struct samr_SamArray *sam = NULL;
	uint32_t filter = ACB_NORMAL;
	int i;
	uint32_t entries_read = 0;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(builtin_handle);

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	*r->out.buffer = NULL;
	*r->out.entries_read = 0;

	switch (r->in.level) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 10:
		case 11:
		case 20:
		case 23:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_ENUM_DOMAINS |
						  SAMR_ACCESS_LOOKUP_DOMAIN,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT |
						  SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
					  SAMR_DOMAIN_ACCESS_ENUM_ACCOUNTS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	switch (r->in.filter) {
		case FILTER_NORMAL_ACCOUNT:
			filter = ACB_NORMAL;
			break;
		case FILTER_TEMP_DUPLICATE_ACCOUNT:
			filter = ACB_TEMPDUP;
			break;
		case FILTER_INTERDOMAIN_TRUST_ACCOUNT:
			filter = ACB_DOMTRUST;
			break;
		case FILTER_WORKSTATION_TRUST_ACCOUNT:
			filter = ACB_WSTRUST;
			break;
		case FILTER_SERVER_TRUST_ACCOUNT:
			filter = ACB_SVRTRUST;
			break;
		default:
			break;
	}

	status = dcerpc_samr_EnumDomainUsers(b,
					     ctx,
					     &domain_handle,
					     r->in.resume_handle,
					     filter,
					     &sam,
					     r->in.prefmaxlen,
					     &entries_read,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	werr = ntstatus_to_werror(result);
	if (NT_STATUS_IS_ERR(result)) {
		goto done;
	}

	for (i=0; i < sam->count; i++) {

		status = libnetapi_samr_lookup_user_map_USER_INFO(ctx, pipe_cli,
								  domain_sid,
								  &domain_handle,
								  &builtin_handle,
								  sam->entries[i].name.string,
								  sam->entries[i].idx,
								  r->in.level,
								  r->out.buffer,
								  r->out.entries_read);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

 done:
	/* if last query */
	if (NT_STATUS_IS_OK(result) ||
	    NT_STATUS_IS_ERR(result)) {

		if (ctx->disable_policy_handle_cache) {
			libnetapi_samr_close_domain_handle(ctx, &domain_handle);
			libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
			libnetapi_samr_close_connect_handle(ctx, &connect_handle);
		}
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserEnum_l(struct libnetapi_ctx *ctx,
		     struct NetUserEnum *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserEnum);
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_dispinfo_to_NET_DISPLAY_USER(TALLOC_CTX *mem_ctx,
							struct samr_DispInfoGeneral *info,
							uint32_t *entries_read,
							void **buffer)
{
	struct NET_DISPLAY_USER *user = NULL;
	int i;

	user = TALLOC_ZERO_ARRAY(mem_ctx,
				 struct NET_DISPLAY_USER,
				 info->count);
	W_ERROR_HAVE_NO_MEMORY(user);

	for (i = 0; i < info->count; i++) {
		user[i].usri1_name = talloc_strdup(mem_ctx,
			info->entries[i].account_name.string);
		user[i].usri1_comment = talloc_strdup(mem_ctx,
			info->entries[i].description.string);
		user[i].usri1_flags =
			info->entries[i].acct_flags;
		user[i].usri1_full_name = talloc_strdup(mem_ctx,
			info->entries[i].full_name.string);
		user[i].usri1_user_id =
			info->entries[i].rid;
		user[i].usri1_next_index =
			info->entries[i].idx;

		if (!user[i].usri1_name) {
			return WERR_NOMEM;
		}
	}

	*buffer = talloc_memdup(mem_ctx, user,
		sizeof(struct NET_DISPLAY_USER) * info->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	*entries_read = info->count;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_dispinfo_to_NET_DISPLAY_MACHINE(TALLOC_CTX *mem_ctx,
							   struct samr_DispInfoFull *info,
							   uint32_t *entries_read,
							   void **buffer)
{
	struct NET_DISPLAY_MACHINE *machine = NULL;
	int i;

	machine = TALLOC_ZERO_ARRAY(mem_ctx,
				    struct NET_DISPLAY_MACHINE,
				    info->count);
	W_ERROR_HAVE_NO_MEMORY(machine);

	for (i = 0; i < info->count; i++) {
		machine[i].usri2_name = talloc_strdup(mem_ctx,
			info->entries[i].account_name.string);
		machine[i].usri2_comment = talloc_strdup(mem_ctx,
			info->entries[i].description.string);
		machine[i].usri2_flags =
			info->entries[i].acct_flags;
		machine[i].usri2_user_id =
			info->entries[i].rid;
		machine[i].usri2_next_index =
			info->entries[i].idx;

		if (!machine[i].usri2_name) {
			return WERR_NOMEM;
		}
	}

	*buffer = talloc_memdup(mem_ctx, machine,
		sizeof(struct NET_DISPLAY_MACHINE) * info->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	*entries_read = info->count;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_dispinfo_to_NET_DISPLAY_GROUP(TALLOC_CTX *mem_ctx,
							 struct samr_DispInfoFullGroups *info,
							 uint32_t *entries_read,
							 void **buffer)
{
	struct NET_DISPLAY_GROUP *group = NULL;
	int i;

	group = TALLOC_ZERO_ARRAY(mem_ctx,
				  struct NET_DISPLAY_GROUP,
				  info->count);
	W_ERROR_HAVE_NO_MEMORY(group);

	for (i = 0; i < info->count; i++) {
		group[i].grpi3_name = talloc_strdup(mem_ctx,
			info->entries[i].account_name.string);
		group[i].grpi3_comment = talloc_strdup(mem_ctx,
			info->entries[i].description.string);
		group[i].grpi3_group_id =
			info->entries[i].rid;
		group[i].grpi3_attributes =
			info->entries[i].acct_flags;
		group[i].grpi3_next_index =
			info->entries[i].idx;

		if (!group[i].grpi3_name) {
			return WERR_NOMEM;
		}
	}

	*buffer = talloc_memdup(mem_ctx, group,
		sizeof(struct NET_DISPLAY_GROUP) * info->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	*entries_read = info->count;

	return WERR_OK;

}

/****************************************************************
****************************************************************/

static WERROR convert_samr_dispinfo_to_NET_DISPLAY(TALLOC_CTX *mem_ctx,
						   union samr_DispInfo *info,
						   uint32_t level,
						   uint32_t *entries_read,
						   void **buffer)
{
	switch (level) {
		case 1:
			return convert_samr_dispinfo_to_NET_DISPLAY_USER(mem_ctx,
									 &info->info1,
									 entries_read,
									 buffer);
		case 2:
			return convert_samr_dispinfo_to_NET_DISPLAY_MACHINE(mem_ctx,
									    &info->info2,
									    entries_read,
									    buffer);
		case 3:
			return convert_samr_dispinfo_to_NET_DISPLAY_GROUP(mem_ctx,
									  &info->info3,
									  entries_read,
									  buffer);
		default:
			break;
	}

	return WERR_UNKNOWN_LEVEL;
}

/****************************************************************
****************************************************************/

WERROR NetQueryDisplayInformation_r(struct libnetapi_ctx *ctx,
				    struct NetQueryDisplayInformation *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle;
	struct dom_sid2 *domain_sid = NULL;
	struct policy_handle domain_handle;
	union samr_DispInfo info;
	struct dcerpc_binding_handle *b = NULL;

	uint32_t total_size = 0;
	uint32_t returned_size = 0;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;
	WERROR werr_tmp;

	*r->out.entries_read = 0;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	switch (r->in.level) {
		case 1:
		case 2:
		case 3:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
					  SAMR_DOMAIN_ACCESS_ENUM_ACCOUNTS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_samr_QueryDisplayInfo2(b,
					       ctx,
					       &domain_handle,
					       r->in.level,
					       r->in.idx,
					       r->in.entries_requested,
					       r->in.prefmaxlen,
					       &total_size,
					       &returned_size,
					       &info,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	werr = ntstatus_to_werror(result);
	if (NT_STATUS_IS_ERR(result)) {
		goto done;
	}

	werr_tmp = convert_samr_dispinfo_to_NET_DISPLAY(ctx, &info,
							r->in.level,
							r->out.entries_read,
							r->out.buffer);
	if (!W_ERROR_IS_OK(werr_tmp)) {
		werr = werr_tmp;
	}
 done:
	/* if last query */
	if (NT_STATUS_IS_OK(result) ||
	    NT_STATUS_IS_ERR(result)) {

		if (ctx->disable_policy_handle_cache) {
			libnetapi_samr_close_domain_handle(ctx, &domain_handle);
			libnetapi_samr_close_connect_handle(ctx, &connect_handle);
		}
	}

	return werr;

}

/****************************************************************
****************************************************************/


WERROR NetQueryDisplayInformation_l(struct libnetapi_ctx *ctx,
				    struct NetQueryDisplayInformation *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetQueryDisplayInformation);
}

/****************************************************************
****************************************************************/

WERROR NetUserChangePassword_r(struct libnetapi_ctx *ctx,
			       struct NetUserChangePassword *r)
{
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR NetUserChangePassword_l(struct libnetapi_ctx *ctx,
			       struct NetUserChangePassword *r)
{
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetInfo_r(struct libnetapi_ctx *ctx,
			struct NetUserGetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;

	struct policy_handle connect_handle, domain_handle, builtin_handle, user_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids user_rids, name_types;
	uint32_t num_entries = 0;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(user_handle);

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 10:
		case 11:
		case 20:
		case 23:
			break;
		default:
			werr = WERR_UNKNOWN_LEVEL;
			goto done;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_ENUM_DOMAINS |
						  SAMR_ACCESS_LOOKUP_DOMAIN,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT |
						  SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = libnetapi_samr_lookup_user_map_USER_INFO(ctx, pipe_cli,
							  domain_sid,
							  &domain_handle,
							  &builtin_handle,
							  r->in.user_name,
							  user_rids.ids[0],
							  r->in.level,
							  r->out.buffer,
							  &num_entries);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (is_valid_policy_hnd(&user_handle) && b) {
		dcerpc_samr_Close(b, talloc_tos(), &user_handle, &result);
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetInfo_l(struct libnetapi_ctx *ctx,
			struct NetUserGetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserGetInfo);
}

/****************************************************************
****************************************************************/

WERROR NetUserSetInfo_r(struct libnetapi_ctx *ctx,
			struct NetUserSetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;

	struct policy_handle connect_handle, domain_handle, builtin_handle, user_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids user_rids, name_types;
	uint32_t user_mask = 0;

	struct USER_INFO_X uX;
	struct dcerpc_binding_handle *b = NULL;
	DATA_BLOB session_key;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(user_handle);

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
			user_mask = SAMR_USER_ACCESS_SET_ATTRIBUTES;
			break;
		case 1003:
			user_mask = SAMR_USER_ACCESS_SET_PASSWORD;
			break;
		case 1006:
		case 1007:
		case 1009:
		case 1011:
		case 1014:
		case 1052:
		case 1053:
			user_mask = SAMR_USER_ACCESS_SET_ATTRIBUTES;
			break;
		case 1012:
		case 1024:
			user_mask = SAMR_USER_ACCESS_SET_LOC_COM;
		case 1051:
			user_mask = SAMR_USER_ACCESS_SET_ATTRIBUTES |
				    SAMR_USER_ACCESS_GET_GROUPS;
			break;
		case 3:
			user_mask = SEC_STD_READ_CONTROL |
				    SEC_STD_WRITE_DAC |
				    SAMR_USER_ACCESS_GET_GROUPS |
				    SAMR_USER_ACCESS_SET_PASSWORD |
				    SAMR_USER_ACCESS_SET_ATTRIBUTES |
				    SAMR_USER_ACCESS_GET_ATTRIBUTES |
				    SAMR_USER_ACCESS_SET_LOC_COM;
			break;
		case 1:
		case 2:
		case 4:
		case 21:
		case 22:
		case 1005:
		case 1008:
		case 1010:
		case 1017:
		case 1020:
			werr = WERR_NOT_SUPPORTED;
			goto done;
		default:
			werr = WERR_UNKNOWN_LEVEL;
			goto done;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1 |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_ENUM_DOMAINS |
						  SAMR_ACCESS_LOOKUP_DOMAIN,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT |
						  SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, talloc_tos(),
				      &domain_handle,
				      user_mask,
				      user_rids.ids[0],
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = construct_USER_INFO_X(r->in.level, r->in.buffer, &uX);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	status = set_user_info_USER_INFO_X(ctx, pipe_cli,
					   &session_key,
					   &user_handle,
					   &uX);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&user_handle) && b) {
		dcerpc_samr_Close(b, talloc_tos(), &user_handle, &result);
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserSetInfo_l(struct libnetapi_ctx *ctx,
			struct NetUserSetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserSetInfo);
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_rpc(TALLOC_CTX *mem_ctx,
					   struct rpc_pipe_client *pipe_cli,
					   struct policy_handle *domain_handle,
					   struct samr_DomInfo1 *info1,
					   struct samr_DomInfo3 *info3,
					   struct samr_DomInfo5 *info5,
					   struct samr_DomInfo6 *info6,
					   struct samr_DomInfo7 *info7,
					   struct samr_DomInfo12 *info12)
{
	NTSTATUS status, result;
	union samr_DomainInfo *dom_info = NULL;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	if (info1) {
		status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
						     domain_handle,
						     1,
						     &dom_info,
						     &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info1 = dom_info->info1;
	}

	if (info3) {
		status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
						     domain_handle,
						     3,
						     &dom_info,
						     &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info3 = dom_info->info3;
	}

	if (info5) {
		status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
						     domain_handle,
						     5,
						     &dom_info,
						     &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info5 = dom_info->info5;
	}

	if (info6) {
		status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
						     domain_handle,
						     6,
						     &dom_info,
						     &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info6 = dom_info->info6;
	}

	if (info7) {
		status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
						     domain_handle,
						     7,
						     &dom_info,
						     &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info7 = dom_info->info7;
	}

	if (info12) {
		status = dcerpc_samr_QueryDomainInfo2(b, mem_ctx,
						      domain_handle,
						      12,
						      &dom_info,
						      &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);

		*info12 = dom_info->info12;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_0(TALLOC_CTX *mem_ctx,
					 struct rpc_pipe_client *pipe_cli,
					 struct policy_handle *domain_handle,
					 struct USER_MODALS_INFO_0 *info0)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info1;
	struct samr_DomInfo3 dom_info3;

	ZERO_STRUCTP(info0);

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info1,
					    &dom_info3,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	info0->usrmod0_min_passwd_len =
		dom_info1.min_password_length;
	info0->usrmod0_max_passwd_age =
		nt_time_to_unix_abs((NTTIME *)&dom_info1.max_password_age);
	info0->usrmod0_min_passwd_age =
		nt_time_to_unix_abs((NTTIME *)&dom_info1.min_password_age);
	info0->usrmod0_password_hist_len =
		dom_info1.password_history_length;

	info0->usrmod0_force_logoff =
		nt_time_to_unix_abs(&dom_info3.force_logoff_time);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_1(TALLOC_CTX *mem_ctx,
					 struct rpc_pipe_client *pipe_cli,
					 struct policy_handle *domain_handle,
					 struct USER_MODALS_INFO_1 *info1)
{
	NTSTATUS status;
	struct samr_DomInfo6 dom_info6;
	struct samr_DomInfo7 dom_info7;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    NULL,
					    NULL,
					    NULL,
					    &dom_info6,
					    &dom_info7,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	info1->usrmod1_primary =
		talloc_strdup(mem_ctx, dom_info6.primary.string);

	info1->usrmod1_role = dom_info7.role;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_2(TALLOC_CTX *mem_ctx,
					 struct rpc_pipe_client *pipe_cli,
					 struct policy_handle *domain_handle,
					 struct dom_sid *domain_sid,
					 struct USER_MODALS_INFO_2 *info2)
{
	NTSTATUS status;
	struct samr_DomInfo5 dom_info5;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    NULL,
					    NULL,
					    &dom_info5,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	info2->usrmod2_domain_name =
		talloc_strdup(mem_ctx, dom_info5.domain_name.string);
	info2->usrmod2_domain_id =
		(struct domsid *)dom_sid_dup(mem_ctx, domain_sid);

	NT_STATUS_HAVE_NO_MEMORY(info2->usrmod2_domain_name);
	NT_STATUS_HAVE_NO_MEMORY(info2->usrmod2_domain_id);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_3(TALLOC_CTX *mem_ctx,
					 struct rpc_pipe_client *pipe_cli,
					 struct policy_handle *domain_handle,
					 struct USER_MODALS_INFO_3 *info3)
{
	NTSTATUS status;
	struct samr_DomInfo12 dom_info12;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    &dom_info12);
	NT_STATUS_NOT_OK_RETURN(status);

	info3->usrmod3_lockout_duration =
		nt_time_to_unix_abs(&dom_info12.lockout_duration);
	info3->usrmod3_lockout_observation_window =
		nt_time_to_unix_abs(&dom_info12.lockout_window);
	info3->usrmod3_lockout_threshold =
		dom_info12.lockout_threshold;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS query_USER_MODALS_INFO_to_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 uint32_t level,
						 struct policy_handle *domain_handle,
						 struct dom_sid *domain_sid,
						 uint8_t **buffer)
{
	NTSTATUS status;

	struct USER_MODALS_INFO_0 info0;
	struct USER_MODALS_INFO_1 info1;
	struct USER_MODALS_INFO_2 info2;
	struct USER_MODALS_INFO_3 info3;

	if (!buffer) {
		return ERROR_INSUFFICIENT_BUFFER;
	}

	switch (level) {
		case 0:
			status = query_USER_MODALS_INFO_0(mem_ctx,
							  pipe_cli,
							  domain_handle,
							  &info0);
			NT_STATUS_NOT_OK_RETURN(status);

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info0,
							   sizeof(info0));
			break;

		case 1:
			status = query_USER_MODALS_INFO_1(mem_ctx,
							  pipe_cli,
							  domain_handle,
							  &info1);
			NT_STATUS_NOT_OK_RETURN(status);

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info1,
							   sizeof(info1));
			break;
		case 2:
			status = query_USER_MODALS_INFO_2(mem_ctx,
							  pipe_cli,
							  domain_handle,
							  domain_sid,
							  &info2);
			NT_STATUS_NOT_OK_RETURN(status);

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info2,
							   sizeof(info2));
			break;
		case 3:
			status = query_USER_MODALS_INFO_3(mem_ctx,
							  pipe_cli,
							  domain_handle,
							  &info3);
			NT_STATUS_NOT_OK_RETURN(status);

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info3,
							   sizeof(info3));
			break;
		default:
			break;
	}

	NT_STATUS_HAVE_NO_MEMORY(*buffer);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetUserModalsGet_r(struct libnetapi_ctx *ctx,
			  struct NetUserModalsGet *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;

	struct policy_handle connect_handle, domain_handle;
	struct dom_sid2 *domain_sid = NULL;
	uint32_t access_mask = SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1 |
				       SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2;
			break;
		case 1:
		case 2:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2;
			break;
		case 3:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1;
			break;
		default:
			werr = WERR_UNKNOWN_LEVEL;
			goto done;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  access_mask,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	/* 0:  1 + 3 */
	/* 1:  6 + 7 */
	/* 2:  5 */
	/* 3: 12 (DomainInfo2) */

	status = query_USER_MODALS_INFO_to_buffer(ctx,
						  pipe_cli,
						  r->in.level,
						  &domain_handle,
						  domain_sid,
						  r->out.buffer);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserModalsGet_l(struct libnetapi_ctx *ctx,
			  struct NetUserModalsGet *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserModalsGet);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_rpc(TALLOC_CTX *mem_ctx,
					 struct rpc_pipe_client *pipe_cli,
					 struct policy_handle *domain_handle,
					 struct samr_DomInfo1 *info1,
					 struct samr_DomInfo3 *info3,
					 struct samr_DomInfo12 *info12)
{
	NTSTATUS status, result;
	union samr_DomainInfo dom_info;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	if (info1) {

		ZERO_STRUCT(dom_info);

		dom_info.info1 = *info1;

		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   domain_handle,
						   1,
						   &dom_info,
						   &result);
		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);
	}

	if (info3) {

		ZERO_STRUCT(dom_info);

		dom_info.info3 = *info3;

		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   domain_handle,
						   3,
						   &dom_info,
						   &result);

		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);
	}

	if (info12) {

		ZERO_STRUCT(dom_info);

		dom_info.info12 = *info12;

		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   domain_handle,
						   12,
						   &dom_info,
						   &result);

		NT_STATUS_NOT_OK_RETURN(status);
		NT_STATUS_NOT_OK_RETURN(result);
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_0_buffer(TALLOC_CTX *mem_ctx,
					      struct rpc_pipe_client *pipe_cli,
					      struct policy_handle *domain_handle,
					      struct USER_MODALS_INFO_0 *info0)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info_1;
	struct samr_DomInfo3 dom_info_3;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info_1,
					    &dom_info_3,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	dom_info_1.min_password_length =
		info0->usrmod0_min_passwd_len;
	dom_info_1.password_history_length =
		info0->usrmod0_password_hist_len;

	unix_to_nt_time_abs((NTTIME *)&dom_info_1.max_password_age,
		info0->usrmod0_max_passwd_age);
	unix_to_nt_time_abs((NTTIME *)&dom_info_1.min_password_age,
		info0->usrmod0_min_passwd_age);

	unix_to_nt_time_abs(&dom_info_3.force_logoff_time,
		info0->usrmod0_force_logoff);

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					&dom_info_1,
					&dom_info_3,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_3_buffer(TALLOC_CTX *mem_ctx,
					      struct rpc_pipe_client *pipe_cli,
					      struct policy_handle *domain_handle,
					      struct USER_MODALS_INFO_3 *info3)
{
	NTSTATUS status;
	struct samr_DomInfo12 dom_info_12;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    &dom_info_12);
	NT_STATUS_NOT_OK_RETURN(status);

	unix_to_nt_time_abs((NTTIME *)&dom_info_12.lockout_duration,
		info3->usrmod3_lockout_duration);
	unix_to_nt_time_abs((NTTIME *)&dom_info_12.lockout_window,
		info3->usrmod3_lockout_observation_window);
	dom_info_12.lockout_threshold = info3->usrmod3_lockout_threshold;

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					NULL,
					NULL,
					&dom_info_12);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_1001_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 struct policy_handle *domain_handle,
						 struct USER_MODALS_INFO_1001 *info1001)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info_1;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info_1,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	dom_info_1.min_password_length =
		info1001->usrmod1001_min_passwd_len;

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					&dom_info_1,
					NULL,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_1002_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 struct policy_handle *domain_handle,
						 struct USER_MODALS_INFO_1002 *info1002)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info_1;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info_1,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	unix_to_nt_time_abs((NTTIME *)&dom_info_1.max_password_age,
		info1002->usrmod1002_max_passwd_age);

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					&dom_info_1,
					NULL,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_1003_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 struct policy_handle *domain_handle,
						 struct USER_MODALS_INFO_1003 *info1003)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info_1;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info_1,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	unix_to_nt_time_abs((NTTIME *)&dom_info_1.min_password_age,
		info1003->usrmod1003_min_passwd_age);

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					&dom_info_1,
					NULL,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_1004_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 struct policy_handle *domain_handle,
						 struct USER_MODALS_INFO_1004 *info1004)
{
	NTSTATUS status;
	struct samr_DomInfo3 dom_info_3;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    NULL,
					    &dom_info_3,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	unix_to_nt_time_abs(&dom_info_3.force_logoff_time,
		info1004->usrmod1004_force_logoff);

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					NULL,
					&dom_info_3,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_1005_buffer(TALLOC_CTX *mem_ctx,
						 struct rpc_pipe_client *pipe_cli,
						 struct policy_handle *domain_handle,
						 struct USER_MODALS_INFO_1005 *info1005)
{
	NTSTATUS status;
	struct samr_DomInfo1 dom_info_1;

	status = query_USER_MODALS_INFO_rpc(mem_ctx,
					    pipe_cli,
					    domain_handle,
					    &dom_info_1,
					    NULL,
					    NULL,
					    NULL,
					    NULL,
					    NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	dom_info_1.password_history_length =
		info1005->usrmod1005_password_hist_len;

	return set_USER_MODALS_INFO_rpc(mem_ctx,
					pipe_cli,
					domain_handle,
					&dom_info_1,
					NULL,
					NULL);
}

/****************************************************************
****************************************************************/

static NTSTATUS set_USER_MODALS_INFO_buffer(TALLOC_CTX *mem_ctx,
					    struct rpc_pipe_client *pipe_cli,
					    uint32_t level,
					    struct policy_handle *domain_handle,
					    struct dom_sid *domain_sid,
					    uint8_t *buffer)
{
	struct USER_MODALS_INFO_0 *info0;
	struct USER_MODALS_INFO_3 *info3;
	struct USER_MODALS_INFO_1001 *info1001;
	struct USER_MODALS_INFO_1002 *info1002;
	struct USER_MODALS_INFO_1003 *info1003;
	struct USER_MODALS_INFO_1004 *info1004;
	struct USER_MODALS_INFO_1005 *info1005;

	if (!buffer) {
		return ERROR_INSUFFICIENT_BUFFER;
	}

	switch (level) {
		case 0:
			info0 = (struct USER_MODALS_INFO_0 *)buffer;
			return set_USER_MODALS_INFO_0_buffer(mem_ctx,
							     pipe_cli,
							     domain_handle,
							     info0);
		case 3:
			info3 = (struct USER_MODALS_INFO_3 *)buffer;
			return set_USER_MODALS_INFO_3_buffer(mem_ctx,
							     pipe_cli,
							     domain_handle,
							     info3);
		case 1001:
			info1001 = (struct USER_MODALS_INFO_1001 *)buffer;
			return set_USER_MODALS_INFO_1001_buffer(mem_ctx,
								pipe_cli,
								domain_handle,
								info1001);
		case 1002:
			info1002 = (struct USER_MODALS_INFO_1002 *)buffer;
			return set_USER_MODALS_INFO_1002_buffer(mem_ctx,
								pipe_cli,
								domain_handle,
								info1002);
		case 1003:
			info1003 = (struct USER_MODALS_INFO_1003 *)buffer;
			return set_USER_MODALS_INFO_1003_buffer(mem_ctx,
								pipe_cli,
								domain_handle,
								info1003);
		case 1004:
			info1004 = (struct USER_MODALS_INFO_1004 *)buffer;
			return set_USER_MODALS_INFO_1004_buffer(mem_ctx,
								pipe_cli,
								domain_handle,
								info1004);
		case 1005:
			info1005 = (struct USER_MODALS_INFO_1005 *)buffer;
			return set_USER_MODALS_INFO_1005_buffer(mem_ctx,
								pipe_cli,
								domain_handle,
								info1005);

		default:
			break;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetUserModalsSet_r(struct libnetapi_ctx *ctx,
			  struct NetUserModalsSet *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;

	struct policy_handle connect_handle, domain_handle;
	struct dom_sid2 *domain_sid = NULL;
	uint32_t access_mask = SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1 |
				       SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
				       SAMR_DOMAIN_ACCESS_SET_INFO_1 |
				       SAMR_DOMAIN_ACCESS_SET_INFO_2;
			break;
		case 3:
		case 1001:
		case 1002:
		case 1003:
		case 1005:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1 |
				       SAMR_DOMAIN_ACCESS_SET_INFO_1;
			break;
		case 1004:
			access_mask |= SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
				       SAMR_DOMAIN_ACCESS_SET_INFO_2;
			break;
		case 1:
		case 2:
		case 1006:
		case 1007:
			werr = WERR_NOT_SUPPORTED;
			break;
		default:
			werr = WERR_UNKNOWN_LEVEL;
			goto done;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  access_mask,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = set_USER_MODALS_INFO_buffer(ctx,
					     pipe_cli,
					     r->in.level,
					     &domain_handle,
					     domain_sid,
					     r->in.buffer);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserModalsSet_l(struct libnetapi_ctx *ctx,
			  struct NetUserModalsSet *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserModalsSet);
}

/****************************************************************
****************************************************************/

NTSTATUS add_GROUP_USERS_INFO_X_buffer(TALLOC_CTX *mem_ctx,
				       uint32_t level,
				       const char *group_name,
				       uint32_t attributes,
				       uint8_t **buffer,
				       uint32_t *num_entries)
{
	struct GROUP_USERS_INFO_0 u0;
	struct GROUP_USERS_INFO_1 u1;

	switch (level) {
		case 0:
			if (group_name) {
				u0.grui0_name = talloc_strdup(mem_ctx, group_name);
				NT_STATUS_HAVE_NO_MEMORY(u0.grui0_name);
			} else {
				u0.grui0_name = NULL;
			}

			ADD_TO_ARRAY(mem_ctx, struct GROUP_USERS_INFO_0, u0,
				     (struct GROUP_USERS_INFO_0 **)buffer, num_entries);
			break;
		case 1:
			if (group_name) {
				u1.grui1_name = talloc_strdup(mem_ctx, group_name);
				NT_STATUS_HAVE_NO_MEMORY(u1.grui1_name);
			} else {
				u1.grui1_name = NULL;
			}

			u1.grui1_attributes = attributes;

			ADD_TO_ARRAY(mem_ctx, struct GROUP_USERS_INFO_1, u1,
				     (struct GROUP_USERS_INFO_1 **)buffer, num_entries);
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetGroups_r(struct libnetapi_ctx *ctx,
			  struct NetUserGetGroups *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle, domain_handle, user_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids user_rids, name_types;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	struct lsa_Strings names;
	struct samr_Ids types;
	uint32_t *rids = NULL;

	int i;
	uint32_t entries_read = 0;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	*r->out.buffer = NULL;
	*r->out.entries_read = 0;
	*r->out.total_entries = 0;

	switch (r->in.level) {
		case 0:
		case 1:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, talloc_tos(),
				      &domain_handle,
				      SAMR_USER_ACCESS_GET_GROUPS,
				      user_rids.ids[0],
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = dcerpc_samr_GetGroupsForUser(b, talloc_tos(),
					      &user_handle,
					      &rid_array,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	rids = talloc_array(ctx, uint32_t, rid_array->count);
	if (!rids) {
		werr = WERR_NOMEM;
		goto done;
	}

	for (i=0; i < rid_array->count; i++) {
		rids[i] = rid_array->rids[i].rid;
	}

	status = dcerpc_samr_LookupRids(b, talloc_tos(),
					&domain_handle,
					rid_array->count,
					rids,
					&names,
					&types,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (names.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	for (i=0; i < names.count; i++) {
		status = add_GROUP_USERS_INFO_X_buffer(ctx,
						       r->in.level,
						       names.names[i].string,
						       rid_array->rids[i].attributes,
						       r->out.buffer,
						       &entries_read);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	*r->out.entries_read = entries_read;
	*r->out.total_entries = entries_read;

 done:
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetGroups_l(struct libnetapi_ctx *ctx,
			  struct NetUserGetGroups *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserGetGroups);
}

/****************************************************************
****************************************************************/

WERROR NetUserSetGroups_r(struct libnetapi_ctx *ctx,
			  struct NetUserSetGroups *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle, domain_handle, user_handle, group_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids user_rids, name_types;
	struct samr_Ids group_rids;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	struct lsa_String *lsa_names = NULL;

	uint32_t *add_rids = NULL;
	uint32_t *del_rids = NULL;
	size_t num_add_rids = 0;
	size_t num_del_rids = 0;

	uint32_t *member_rids = NULL;
	size_t num_member_rids = 0;

	struct GROUP_USERS_INFO_0 *i0 = NULL;
	struct GROUP_USERS_INFO_1 *i1 = NULL;

	int i, k;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, talloc_tos(),
				      &domain_handle,
				      SAMR_USER_ACCESS_GET_GROUPS,
				      user_rids.ids[0],
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	switch (r->in.level) {
		case 0:
			i0 = (struct GROUP_USERS_INFO_0 *)r->in.buffer;
			break;
		case 1:
			i1 = (struct GROUP_USERS_INFO_1 *)r->in.buffer;
			break;
	}

	lsa_names = talloc_array(ctx, struct lsa_String, r->in.num_entries);
	if (!lsa_names) {
		werr = WERR_NOMEM;
		goto done;
	}

	for (i=0; i < r->in.num_entries; i++) {

		switch (r->in.level) {
			case 0:
				init_lsa_String(&lsa_names[i], i0->grui0_name);
				i0++;
				break;
			case 1:
				init_lsa_String(&lsa_names[i], i1->grui1_name);
				i1++;
				break;
		}
	}

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 r->in.num_entries,
					 lsa_names,
					 &group_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (group_rids.count != r->in.num_entries) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != r->in.num_entries) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	member_rids = group_rids.ids;
	num_member_rids = group_rids.count;

	status = dcerpc_samr_GetGroupsForUser(b, talloc_tos(),
					      &user_handle,
					      &rid_array,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	/* add list */

	for (i=0; i < r->in.num_entries; i++) {
		bool already_member = false;
		for (k=0; k < rid_array->count; k++) {
			if (member_rids[i] == rid_array->rids[k].rid) {
				already_member = true;
				break;
			}
		}
		if (!already_member) {
			if (!add_rid_to_array_unique(ctx,
						     member_rids[i],
						     &add_rids, &num_add_rids)) {
				werr = WERR_GENERAL_FAILURE;
				goto done;
			}
		}
	}

	/* del list */

	for (k=0; k < rid_array->count; k++) {
		bool keep_member = false;
		for (i=0; i < r->in.num_entries; i++) {
			if (member_rids[i] == rid_array->rids[k].rid) {
				keep_member = true;
				break;
			}
		}
		if (!keep_member) {
			if (!add_rid_to_array_unique(ctx,
						     rid_array->rids[k].rid,
						     &del_rids, &num_del_rids)) {
				werr = WERR_GENERAL_FAILURE;
				goto done;
			}
		}
	}

	/* add list */

	for (i=0; i < num_add_rids; i++) {
		status = dcerpc_samr_OpenGroup(b, talloc_tos(),
					       &domain_handle,
					       SAMR_GROUP_ACCESS_ADD_MEMBER,
					       add_rids[i],
					       &group_handle,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}

		status = dcerpc_samr_AddGroupMember(b, talloc_tos(),
						    &group_handle,
						    user_rids.ids[0],
						    7 /* ? */,
						    &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}

		if (is_valid_policy_hnd(&group_handle)) {
			dcerpc_samr_Close(b, talloc_tos(), &group_handle, &result);
		}
	}

	/* del list */

	for (i=0; i < num_del_rids; i++) {
		status = dcerpc_samr_OpenGroup(b, talloc_tos(),
					       &domain_handle,
					       SAMR_GROUP_ACCESS_REMOVE_MEMBER,
					       del_rids[i],
					       &group_handle,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}

		status = dcerpc_samr_DeleteGroupMember(b, talloc_tos(),
						       &group_handle,
						       user_rids.ids[0],
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}

		if (is_valid_policy_hnd(&group_handle)) {
			dcerpc_samr_Close(b, talloc_tos(), &group_handle, &result);
		}
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&group_handle)) {
		dcerpc_samr_Close(b, talloc_tos(), &group_handle, &result);
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserSetGroups_l(struct libnetapi_ctx *ctx,
			  struct NetUserSetGroups *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserSetGroups);
}

/****************************************************************
****************************************************************/

static NTSTATUS add_LOCALGROUP_USERS_INFO_X_buffer(TALLOC_CTX *mem_ctx,
						   uint32_t level,
						   const char *group_name,
						   uint8_t **buffer,
						   uint32_t *num_entries)
{
	struct LOCALGROUP_USERS_INFO_0 u0;

	switch (level) {
		case 0:
			u0.lgrui0_name = talloc_strdup(mem_ctx, group_name);
			NT_STATUS_HAVE_NO_MEMORY(u0.lgrui0_name);

			ADD_TO_ARRAY(mem_ctx, struct LOCALGROUP_USERS_INFO_0, u0,
				     (struct LOCALGROUP_USERS_INFO_0 **)buffer, num_entries);
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetLocalGroups_r(struct libnetapi_ctx *ctx,
			       struct NetUserGetLocalGroups *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle, domain_handle, user_handle,
	builtin_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids user_rids, name_types;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	struct lsa_Strings names;
	struct samr_Ids types;
	uint32_t *rids = NULL;
	size_t num_rids = 0;
	struct dom_sid user_sid;
	struct lsa_SidArray sid_array;
	struct samr_Ids domain_rids;
	struct samr_Ids builtin_rids;

	int i;
	uint32_t entries_read = 0;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	*r->out.buffer = NULL;
	*r->out.entries_read = 0;
	*r->out.total_entries = 0;

	switch (r->in.level) {
		case 0:
		case 1:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT |
					  SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_ENUM_DOMAINS |
						  SAMR_ACCESS_LOOKUP_DOMAIN,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT |
						  SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (user_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, talloc_tos(),
				      &domain_handle,
				      SAMR_USER_ACCESS_GET_GROUPS,
				      user_rids.ids[0],
				      &user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = dcerpc_samr_GetGroupsForUser(b, talloc_tos(),
					      &user_handle,
					      &rid_array,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	if (!sid_compose(&user_sid, domain_sid, user_rids.ids[0])) {
		werr = WERR_NOMEM;
		goto done;
	}

	sid_array.num_sids = rid_array->count + 1;
	sid_array.sids = TALLOC_ARRAY(ctx, struct lsa_SidPtr, sid_array.num_sids);
	if (!sid_array.sids) {
		werr = WERR_NOMEM;
		goto done;
	}

	sid_array.sids[0].sid = dom_sid_dup(ctx, &user_sid);
	if (!sid_array.sids[0].sid) {
		werr = WERR_NOMEM;
		goto done;
	}

	for (i=0; i < rid_array->count; i++) {
		struct dom_sid sid;

		if (!sid_compose(&sid, domain_sid, rid_array->rids[i].rid)) {
			werr = WERR_NOMEM;
			goto done;
		}

		sid_array.sids[i+1].sid = dom_sid_dup(ctx, &sid);
		if (!sid_array.sids[i+1].sid) {
			werr = WERR_NOMEM;
			goto done;
		}
	}

	status = dcerpc_samr_GetAliasMembership(b, talloc_tos(),
						&domain_handle,
						&sid_array,
						&domain_rids,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	for (i=0; i < domain_rids.count; i++) {
		if (!add_rid_to_array_unique(ctx, domain_rids.ids[i],
					     &rids, &num_rids)) {
			werr = WERR_NOMEM;
			goto done;
		}
	}

	status = dcerpc_samr_GetAliasMembership(b, talloc_tos(),
						&builtin_handle,
						&sid_array,
						&builtin_rids,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	for (i=0; i < builtin_rids.count; i++) {
		if (!add_rid_to_array_unique(ctx, builtin_rids.ids[i],
					     &rids, &num_rids)) {
			werr = WERR_NOMEM;
			goto done;
		}
	}

	status = dcerpc_samr_LookupRids(b, talloc_tos(),
					&builtin_handle,
					num_rids,
					rids,
					&names,
					&types,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (names.count != num_rids) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != num_rids) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	for (i=0; i < names.count; i++) {
		status = add_LOCALGROUP_USERS_INFO_X_buffer(ctx,
							    r->in.level,
							    names.names[i].string,
							    r->out.buffer,
							    &entries_read);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	*r->out.entries_read = entries_read;
	*r->out.total_entries = entries_read;

 done:
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUserGetLocalGroups_l(struct libnetapi_ctx *ctx,
			       struct NetUserGetLocalGroups *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetUserGetLocalGroups);
}
