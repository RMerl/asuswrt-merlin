/*
 *  Unix SMB/CIFS implementation.
 *  NetApi LocalGroup Support
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
#include "../librpc/gen_ndr/cli_samr.h"
#include "../librpc/gen_ndr/cli_lsa.h"

static NTSTATUS libnetapi_samr_lookup_and_open_alias(TALLOC_CTX *mem_ctx,
						     struct rpc_pipe_client *pipe_cli,
						     struct policy_handle *domain_handle,
						     const char *group_name,
						     uint32_t access_rights,
						     struct policy_handle *alias_handle)
{
	NTSTATUS status;

	struct lsa_String lsa_account_name;
	struct samr_Ids user_rids, name_types;

	init_lsa_String(&lsa_account_name, group_name);

	status = rpccli_samr_LookupNames(pipe_cli, mem_ctx,
					 domain_handle,
					 1,
					 &lsa_account_name,
					 &user_rids,
					 &name_types);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (name_types.ids[0]) {
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:
			break;
		default:
			return NT_STATUS_INVALID_SID;
	}

	return rpccli_samr_OpenAlias(pipe_cli, mem_ctx,
				     domain_handle,
				     access_rights,
				     user_rids.ids[0],
				     alias_handle);
}

/****************************************************************
****************************************************************/

static NTSTATUS libnetapi_samr_open_alias_queryinfo(TALLOC_CTX *mem_ctx,
						    struct rpc_pipe_client *pipe_cli,
						    struct policy_handle *handle,
						    uint32_t rid,
						    uint32_t access_rights,
						    enum samr_AliasInfoEnum level,
						    union samr_AliasInfo **alias_info)
{
	NTSTATUS status;
	struct policy_handle alias_handle;
	union samr_AliasInfo *_alias_info = NULL;

	ZERO_STRUCT(alias_handle);

	status = rpccli_samr_OpenAlias(pipe_cli, mem_ctx,
				       handle,
				       access_rights,
				       rid,
				       &alias_handle);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpccli_samr_QueryAliasInfo(pipe_cli, mem_ctx,
					    &alias_handle,
					    level,
					    &_alias_info);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	*alias_info = _alias_info;

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, mem_ctx, &alias_handle);
	}

	return status;
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupAdd_r(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupAdd *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	struct lsa_String lsa_account_name;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;
	uint32_t rid;

	struct LOCALGROUP_INFO_0 *info0 = NULL;
	struct LOCALGROUP_INFO_1 *info1 = NULL;

	const char *alias_name = NULL;

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
			info0 = (struct LOCALGROUP_INFO_0 *)r->in.buffer;
			alias_name = info0->lgrpi0_name;
			break;
		case 1:
			info1 = (struct LOCALGROUP_INFO_1 *)r->in.buffer;
			alias_name = info1->lgrpi1_name;
			break;
		default:
			werr = WERR_UNKNOWN_LEVEL;
			goto done;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &builtin_handle,
						      alias_name,
						      SAMR_ALIAS_ACCESS_LOOKUP_INFO,
						      &alias_handle);
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
	}

	if (NT_STATUS_IS_OK(status)) {
		werr = WERR_ALIAS_EXISTS;
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_CREATE_ALIAS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, alias_name);

	status = rpccli_samr_CreateDomAlias(pipe_cli, talloc_tos(),
					    &domain_handle,
					    &lsa_account_name,
					    SEC_STD_DELETE |
					    SAMR_ALIAS_ACCESS_SET_INFO,
					    &alias_handle,
					    &rid);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (r->in.level == 1 && info1->lgrpi1_comment) {

		union samr_AliasInfo alias_info;

		init_lsa_String(&alias_info.description, info1->lgrpi1_comment);

		status = rpccli_samr_SetAliasInfo(pipe_cli, talloc_tos(),
						  &alias_handle,
						  ALIASINFODESCRIPTION,
						  &alias_info);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, talloc_tos(), &alias_handle);
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

WERROR NetLocalGroupAdd_l(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupAdd *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupAdd);
}

/****************************************************************
****************************************************************/


WERROR NetLocalGroupDel_r(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupDel *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &builtin_handle,
						      r->in.group_name,
						      SEC_STD_DELETE,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
	}

	if (NT_STATUS_IS_OK(status)) {
		goto delete_alias;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_CREATE_ALIAS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &domain_handle,
						      r->in.group_name,
						      SEC_STD_DELETE,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
	}

	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}


 delete_alias:
	status = rpccli_samr_DeleteDomAlias(pipe_cli, talloc_tos(),
					    &alias_handle);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	ZERO_STRUCT(alias_handle);

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, talloc_tos(), &alias_handle);
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

WERROR NetLocalGroupDel_l(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupDel *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupDel);
}

/****************************************************************
****************************************************************/

static WERROR map_alias_info_to_buffer(TALLOC_CTX *mem_ctx,
				       const char *alias_name,
				       struct samr_AliasInfoAll *info,
				       uint32_t level,
				       uint32_t *entries_read,
				       uint8_t **buffer)
{
	struct LOCALGROUP_INFO_0 g0;
	struct LOCALGROUP_INFO_1 g1;
	struct LOCALGROUP_INFO_1002 g1002;

	switch (level) {
		case 0:
			g0.lgrpi0_name		= talloc_strdup(mem_ctx, alias_name);
			W_ERROR_HAVE_NO_MEMORY(g0.lgrpi0_name);

			ADD_TO_ARRAY(mem_ctx, struct LOCALGROUP_INFO_0, g0,
				     (struct LOCALGROUP_INFO_0 **)buffer, entries_read);

			break;
		case 1:
			g1.lgrpi1_name		= talloc_strdup(mem_ctx, alias_name);
			g1.lgrpi1_comment	= talloc_strdup(mem_ctx, info->description.string);
			W_ERROR_HAVE_NO_MEMORY(g1.lgrpi1_name);

			ADD_TO_ARRAY(mem_ctx, struct LOCALGROUP_INFO_1, g1,
				     (struct LOCALGROUP_INFO_1 **)buffer, entries_read);

			break;
		case 1002:
			g1002.lgrpi1002_comment	= talloc_strdup(mem_ctx, info->description.string);

			ADD_TO_ARRAY(mem_ctx, struct LOCALGROUP_INFO_1002, g1002,
				     (struct LOCALGROUP_INFO_1002 **)buffer, entries_read);

			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupGetInfo_r(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupGetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;
	union samr_AliasInfo *alias_info = NULL;
	uint32_t entries_read = 0;

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
		case 1002:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &builtin_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_LOOKUP_INFO,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
	}

	if (NT_STATUS_IS_OK(status)) {
		goto query_alias;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_ENUM_DOMAINS |
					  SAMR_ACCESS_LOOKUP_DOMAIN,
					  SAMR_DOMAIN_ACCESS_CREATE_ALIAS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &domain_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_LOOKUP_INFO,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
	}

	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 query_alias:
	status = rpccli_samr_QueryAliasInfo(pipe_cli, talloc_tos(),
					    &alias_handle,
					    ALIASINFOALL,
					    &alias_info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	werr = map_alias_info_to_buffer(ctx,
					r->in.group_name,
					&alias_info->all,
					r->in.level, &entries_read,
					r->out.buffer);

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, talloc_tos(), &alias_handle);
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

WERROR NetLocalGroupGetInfo_l(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupGetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupGetInfo);
}

/****************************************************************
****************************************************************/

static WERROR map_buffer_to_alias_info(TALLOC_CTX *mem_ctx,
				       uint32_t level,
				       uint8_t *buffer,
				       enum samr_AliasInfoEnum *alias_level,
				       union samr_AliasInfo **alias_info)
{
	struct LOCALGROUP_INFO_0 *info0;
	struct LOCALGROUP_INFO_1 *info1;
	struct LOCALGROUP_INFO_1002 *info1002;
	union samr_AliasInfo *info = NULL;

	info = TALLOC_ZERO_P(mem_ctx, union samr_AliasInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (level) {
		case 0:
			info0 = (struct LOCALGROUP_INFO_0 *)buffer;
			init_lsa_String(&info->name, info0->lgrpi0_name);
			*alias_level = ALIASINFONAME;
			break;
		case 1:
			info1 = (struct LOCALGROUP_INFO_1 *)buffer;
			/* group name will be ignored */
			init_lsa_String(&info->description, info1->lgrpi1_comment);
			*alias_level = ALIASINFODESCRIPTION;
			break;
		case 1002:
			info1002 = (struct LOCALGROUP_INFO_1002 *)buffer;
			init_lsa_String(&info->description, info1002->lgrpi1002_comment);
			*alias_level = ALIASINFODESCRIPTION;
			break;
	}

	*alias_info = info;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupSetInfo_r(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupSetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	struct lsa_String lsa_account_name;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;
	enum samr_AliasInfoEnum alias_level = 0;
	union samr_AliasInfo *alias_info = NULL;

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
		case 1002:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.group_name);

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &builtin_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_SET_INFO,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
	}

	if (NT_STATUS_IS_OK(status)) {
		goto set_alias;
	}

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

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &domain_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_SET_INFO,
						      &alias_handle);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
	}

 set_alias:

	werr = map_buffer_to_alias_info(ctx, r->in.level, r->in.buffer,
					&alias_level, &alias_info);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = rpccli_samr_SetAliasInfo(pipe_cli, talloc_tos(),
					  &alias_handle,
					  alias_level,
					  alias_info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, talloc_tos(), &alias_handle);
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

WERROR NetLocalGroupSetInfo_l(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupSetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupSetInfo);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupEnum_r(struct libnetapi_ctx *ctx,
			   struct NetLocalGroupEnum *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;
	uint32_t entries_read = 0;
	union samr_DomainInfo *domain_info = NULL;
	union samr_DomainInfo *builtin_info = NULL;
	struct samr_SamArray *domain_sam_array = NULL;
	struct samr_SamArray *builtin_sam_array = NULL;
	int i;

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	if (r->out.total_entries) {
		*r->out.total_entries = 0;
	}
	if (r->out.entries_read) {
		*r->out.entries_read = 0;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
						  SAMR_DOMAIN_ACCESS_ENUM_ACCOUNTS |
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_domain(ctx, pipe_cli,
					  SAMR_ACCESS_LOOKUP_DOMAIN |
					  SAMR_ACCESS_ENUM_DOMAINS,
					  SAMR_DOMAIN_ACCESS_LOOKUP_INFO_2 |
					  SAMR_DOMAIN_ACCESS_ENUM_ACCOUNTS |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = rpccli_samr_QueryDomainInfo(pipe_cli, talloc_tos(),
					     &builtin_handle,
					     2,
					     &builtin_info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (r->out.total_entries) {
		*r->out.total_entries += builtin_info->general.num_aliases;
	}

	status = rpccli_samr_QueryDomainInfo(pipe_cli, talloc_tos(),
					     &domain_handle,
					     2,
					     &domain_info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (r->out.total_entries) {
		*r->out.total_entries += domain_info->general.num_aliases;
	}

	status = rpccli_samr_EnumDomainAliases(pipe_cli, talloc_tos(),
					       &builtin_handle,
					       r->in.resume_handle,
					       &builtin_sam_array,
					       r->in.prefmaxlen,
					       &entries_read);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	for (i=0; i<builtin_sam_array->count; i++) {
		union samr_AliasInfo *alias_info = NULL;

		if (r->in.level == 1) {

			status = libnetapi_samr_open_alias_queryinfo(ctx, pipe_cli,
								     &builtin_handle,
								     builtin_sam_array->entries[i].idx,
								     SAMR_ALIAS_ACCESS_LOOKUP_INFO,
								     ALIASINFOALL,
								     &alias_info);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
		}

		werr = map_alias_info_to_buffer(ctx,
						builtin_sam_array->entries[i].name.string,
						alias_info ? &alias_info->all : NULL,
						r->in.level,
						r->out.entries_read,
						r->out.buffer);
	}

	status = rpccli_samr_EnumDomainAliases(pipe_cli, talloc_tos(),
					       &domain_handle,
					       r->in.resume_handle,
					       &domain_sam_array,
					       r->in.prefmaxlen,
					       &entries_read);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	for (i=0; i<domain_sam_array->count; i++) {

		union samr_AliasInfo *alias_info = NULL;

		if (r->in.level == 1) {
			status = libnetapi_samr_open_alias_queryinfo(ctx, pipe_cli,
								     &domain_handle,
								     domain_sam_array->entries[i].idx,
								     SAMR_ALIAS_ACCESS_LOOKUP_INFO,
								     ALIASINFOALL,
								     &alias_info);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
		}

		werr = map_alias_info_to_buffer(ctx,
						domain_sam_array->entries[i].name.string,
						alias_info ? &alias_info->all : NULL,
						r->in.level,
						r->out.entries_read,
						r->out.buffer);
	}

 done:
	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
		libnetapi_samr_close_connect_handle(ctx, &connect_handle);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupEnum_l(struct libnetapi_ctx *ctx,
			   struct NetLocalGroupEnum *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupEnum);
}

/****************************************************************
****************************************************************/

static NTSTATUS libnetapi_lsa_lookup_names3(TALLOC_CTX *mem_ctx,
					    struct rpc_pipe_client *lsa_pipe,
					    const char *name,
					    struct dom_sid *sid)
{
	NTSTATUS status;
	struct policy_handle lsa_handle;

	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TransSidArray3 sids;
	uint32_t count = 0;

	struct lsa_String names;
	uint32_t num_names = 1;

	if (!sid || !name) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(sids);

	init_lsa_String(&names, name);

	status = rpccli_lsa_open_policy2(lsa_pipe, mem_ctx,
					 false,
					 STD_RIGHT_READ_CONTROL_ACCESS |
					 LSA_POLICY_VIEW_LOCAL_INFORMATION |
					 LSA_POLICY_LOOKUP_NAMES,
					 &lsa_handle);
	NT_STATUS_NOT_OK_RETURN(status);

	status = rpccli_lsa_LookupNames3(lsa_pipe, mem_ctx,
					 &lsa_handle,
					 num_names,
					 &names,
					 &domains,
					 &sids,
					 LSA_LOOKUP_NAMES_ALL, /* sure ? */
					 &count,
					 0, 0);
	NT_STATUS_NOT_OK_RETURN(status);

	if (count != 1 || sids.count != 1) {
		return NT_STATUS_NONE_MAPPED;
	}

	sid_copy(sid, sids.sids[0].sid);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static WERROR NetLocalGroupModifyMembers_r(struct libnetapi_ctx *ctx,
					   struct NetLocalGroupAddMembers *add,
					   struct NetLocalGroupDelMembers *del,
					   struct NetLocalGroupSetMembers *set)
{
	struct NetLocalGroupAddMembers *r = NULL;

	struct rpc_pipe_client *pipe_cli = NULL;
	struct rpc_pipe_client *lsa_pipe = NULL;
	NTSTATUS status;
	WERROR werr;
	struct lsa_String lsa_account_name;
	struct policy_handle connect_handle, domain_handle, builtin_handle, alias_handle;
	struct dom_sid2 *domain_sid = NULL;
	struct dom_sid *member_sids = NULL;
	int i = 0, k = 0;

	struct LOCALGROUP_MEMBERS_INFO_0 *info0 = NULL;
	struct LOCALGROUP_MEMBERS_INFO_3 *info3 = NULL;

	struct dom_sid *add_sids = NULL;
	struct dom_sid *del_sids = NULL;
	size_t num_add_sids = 0;
	size_t num_del_sids = 0;

	if ((!add && !del && !set) || (add && del && set)) {
		return WERR_INVALID_PARAM;
	}

	if (add) {
		r = add;
	}

	if (del) {
		r = (struct NetLocalGroupAddMembers *)del;
	}

	if (set) {
		r = (struct NetLocalGroupAddMembers *)set;
	}

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 3:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	if (r->in.total_entries == 0 || !r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(builtin_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(alias_handle);

	member_sids = TALLOC_ZERO_ARRAY(ctx, struct dom_sid,
					r->in.total_entries);
	W_ERROR_HAVE_NO_MEMORY(member_sids);

	switch (r->in.level) {
		case 0:
			info0 = (struct LOCALGROUP_MEMBERS_INFO_0 *)r->in.buffer;
			for (i=0; i < r->in.total_entries; i++) {
				sid_copy(&member_sids[i], (struct dom_sid *)info0[i].lgrmi0_sid);
			}
			break;
		case 3:
			info3 = (struct LOCALGROUP_MEMBERS_INFO_3 *)r->in.buffer;
			break;
		default:
			break;
	}

	if (r->in.level == 3) {
		werr = libnetapi_open_pipe(ctx, r->in.server_name,
					   &ndr_table_lsarpc.syntax_id,
					   &lsa_pipe);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}

		for (i=0; i < r->in.total_entries; i++) {
			status = libnetapi_lsa_lookup_names3(ctx, lsa_pipe,
							     info3[i].lgrmi3_domainandname,
							     &member_sids[i]);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
		}
		TALLOC_FREE(lsa_pipe);
	}

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_samr.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_samr_open_builtin_domain(ctx, pipe_cli,
						  SAMR_ACCESS_LOOKUP_DOMAIN |
						  SAMR_ACCESS_ENUM_DOMAINS,
						  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						  &connect_handle,
						  &builtin_handle);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_String(&lsa_account_name, r->in.group_name);

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &builtin_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_ADD_MEMBER |
						      SAMR_ALIAS_ACCESS_REMOVE_MEMBER |
						      SAMR_ALIAS_ACCESS_GET_MEMBERS |
						      SAMR_ALIAS_ACCESS_LOOKUP_INFO,
						      &alias_handle);

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_builtin_handle(ctx, &builtin_handle);
	}

	if (NT_STATUS_IS_OK(status)) {
		goto modify_membership;
	}

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

	status = libnetapi_samr_lookup_and_open_alias(ctx, pipe_cli,
						      &domain_handle,
						      r->in.group_name,
						      SAMR_ALIAS_ACCESS_ADD_MEMBER |
						      SAMR_ALIAS_ACCESS_REMOVE_MEMBER |
						      SAMR_ALIAS_ACCESS_GET_MEMBERS |
						      SAMR_ALIAS_ACCESS_LOOKUP_INFO,
						      &alias_handle);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (ctx->disable_policy_handle_cache) {
		libnetapi_samr_close_domain_handle(ctx, &domain_handle);
	}

 modify_membership:

	if (add) {
		for (i=0; i < r->in.total_entries; i++) {
			status = add_sid_to_array_unique(ctx, &member_sids[i],
							 &add_sids,
							 &num_add_sids);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
		}
	}

	if (del) {
		for (i=0; i < r->in.total_entries; i++) {
			status = add_sid_to_array_unique(ctx, &member_sids[i],
							 &del_sids,
							 &num_del_sids);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
		}
	}

	if (set) {

		struct lsa_SidArray current_sids;

		status = rpccli_samr_GetMembersInAlias(pipe_cli, talloc_tos(),
						       &alias_handle,
						       &current_sids);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}

		/* add list */

		for (i=0; i < r->in.total_entries; i++) {
			bool already_member = false;
			for (k=0; k < current_sids.num_sids; k++) {
				if (sid_equal(&member_sids[i],
					      current_sids.sids[k].sid)) {
					already_member = true;
					break;
				}
			}
			if (!already_member) {
				status = add_sid_to_array_unique(ctx,
					&member_sids[i],
					&add_sids, &num_add_sids);
				if (!NT_STATUS_IS_OK(status)) {
					werr = ntstatus_to_werror(status);
					goto done;
				}
			}
		}

		/* del list */

		for (k=0; k < current_sids.num_sids; k++) {
			bool keep_member = false;
			for (i=0; i < r->in.total_entries; i++) {
				if (sid_equal(&member_sids[i],
					      current_sids.sids[k].sid)) {
					keep_member = true;
					break;
				}
			}
			if (!keep_member) {
				status = add_sid_to_array_unique(ctx,
						current_sids.sids[k].sid,
						&del_sids, &num_del_sids);
				if (!NT_STATUS_IS_OK(status)) {
					werr = ntstatus_to_werror(status);
					goto done;
				}
			}
		}
	}

	/* add list */

	for (i=0; i < num_add_sids; i++) {
		status = rpccli_samr_AddAliasMember(pipe_cli, talloc_tos(),
						    &alias_handle,
						    &add_sids[i]);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	/* del list */

	for (i=0; i < num_del_sids; i++) {
		status = rpccli_samr_DeleteAliasMember(pipe_cli, talloc_tos(),
						       &alias_handle,
						       &del_sids[i]);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	werr = WERR_OK;

 done:
	if (is_valid_policy_hnd(&alias_handle)) {
		rpccli_samr_Close(pipe_cli, talloc_tos(), &alias_handle);
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

WERROR NetLocalGroupAddMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupAddMembers *r)
{
	return NetLocalGroupModifyMembers_r(ctx, r, NULL, NULL);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupAddMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupAddMembers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupAddMembers);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupDelMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupDelMembers *r)
{
	return NetLocalGroupModifyMembers_r(ctx, NULL, r, NULL);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupDelMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupDelMembers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupDelMembers);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupGetMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupGetMembers *r)
{
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupGetMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupGetMembers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupGetMembers);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupSetMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupSetMembers *r)
{
	return NetLocalGroupModifyMembers_r(ctx, NULL, NULL, r);
}

/****************************************************************
****************************************************************/

WERROR NetLocalGroupSetMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupSetMembers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetLocalGroupSetMembers);
}
