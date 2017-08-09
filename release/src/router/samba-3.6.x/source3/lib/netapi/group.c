/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Group Support
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
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"

/****************************************************************
****************************************************************/

WERROR NetGroupAdd_r(struct libnetapi_ctx *ctx,
		     struct NetGroupAdd *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name;
	struct dom_sid2 *domain_sid = NULL;
	uint32_t rid = 0;
	struct dcerpc_binding_handle *b = NULL;

	struct GROUP_INFO_0 *info0 = NULL;
	struct GROUP_INFO_1 *info1 = NULL;
	struct GROUP_INFO_2 *info2 = NULL;
	struct GROUP_INFO_3 *info3 = NULL;
	union samr_GroupInfo info;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
			info0 = (struct GROUP_INFO_0 *)r->in.buffer;
			break;
		case 1:
			info1 = (struct GROUP_INFO_1 *)r->in.buffer;
			break;
		case 2:
			info2 = (struct GROUP_INFO_2 *)r->in.buffer;
			break;
		case 3:
			info3 = (struct GROUP_INFO_3 *)r->in.buffer;
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
					  SAMR_DOMAIN_ACCESS_CREATE_GROUP |
					  SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					  &connect_handle,
					  &domain_handle,
					  &domain_sid);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	switch (r->in.level) {
		case 0:
			init_lsa_String(&lsa_group_name, info0->grpi0_name);
			break;
		case 1:
			init_lsa_String(&lsa_group_name, info1->grpi1_name);
			break;
		case 2:
			init_lsa_String(&lsa_group_name, info2->grpi2_name);
			break;
		case 3:
			init_lsa_String(&lsa_group_name, info3->grpi3_name);
			break;
	}

	status = dcerpc_samr_CreateDomainGroup(b, talloc_tos(),
					       &domain_handle,
					       &lsa_group_name,
					       SEC_STD_DELETE |
					       SAMR_GROUP_ACCESS_SET_INFO,
					       &group_handle,
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

	switch (r->in.level) {
		case 1:
			if (info1->grpi1_comment) {
				init_lsa_String(&info.description,
						info1->grpi1_comment);

				status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
								  &group_handle,
								  GROUPINFODESCRIPTION,
								  &info,
								  &result);
			}
			break;
		case 2:
			if (info2->grpi2_comment) {
				init_lsa_String(&info.description,
						info2->grpi2_comment);

				status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
								  &group_handle,
								  GROUPINFODESCRIPTION,
								  &info,
								  &result);
				if (!NT_STATUS_IS_OK(status)) {
					werr = ntstatus_to_werror(status);
					goto failed;
				}
				if (!NT_STATUS_IS_OK(result)) {
					werr = ntstatus_to_werror(result);
					goto failed;
				}
			}

			if (info2->grpi2_attributes != 0) {
				info.attributes.attributes = info2->grpi2_attributes;
				status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
								  &group_handle,
								  GROUPINFOATTRIBUTES,
								  &info,
								  &result);

			}
			break;
		case 3:
			if (info3->grpi3_comment) {
				init_lsa_String(&info.description,
						info3->grpi3_comment);

				status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
								  &group_handle,
								  GROUPINFODESCRIPTION,
								  &info,
								  &result);
				if (!NT_STATUS_IS_OK(status)) {
					werr = ntstatus_to_werror(status);
					goto failed;
				}
				if (!NT_STATUS_IS_OK(result)) {
					werr = ntstatus_to_werror(result);
					goto failed;
				}
			}

			if (info3->grpi3_attributes != 0) {
				info.attributes.attributes = info3->grpi3_attributes;
				status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
								  &group_handle,
								  GROUPINFOATTRIBUTES,
								  &info,
								  &result);
			}
			break;
		default:
			break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto failed;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto failed;
	}

	werr = WERR_OK;
	goto done;

 failed:
	dcerpc_samr_DeleteDomainGroup(b, talloc_tos(),
				      &group_handle, &result);

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

WERROR NetGroupAdd_l(struct libnetapi_ctx *ctx,
		     struct NetGroupAdd *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupAdd);
}

/****************************************************************
****************************************************************/

WERROR NetGroupDel_r(struct libnetapi_ctx *ctx,
		     struct NetGroupDel *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name;
	struct dom_sid2 *domain_sid = NULL;
	int i = 0;
	struct dcerpc_binding_handle *b = NULL;

	struct samr_Ids rids;
	struct samr_Ids types;
	union samr_GroupInfo *info = NULL;
	struct samr_RidAttrArray *rid_array = NULL;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
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

	init_lsa_String(&lsa_group_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_group_name,
					 &rids,
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

	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_DOM_GRP) {
		werr = WERR_INVALID_DATATYPE;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SEC_STD_DELETE |
				       SAMR_GROUP_ACCESS_GET_MEMBERS |
				       SAMR_GROUP_ACCESS_REMOVE_MEMBER |
				       SAMR_GROUP_ACCESS_ADD_MEMBER |
				       SAMR_GROUP_ACCESS_LOOKUP_INFO,
				       rids.ids[0],
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

	status = dcerpc_samr_QueryGroupInfo(b, talloc_tos(),
					    &group_handle,
					    GROUPINFOATTRIBUTES,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

#if 0
	/* breaks against NT4 */
	if (!(info->attributes.attributes & SE_GROUP_ENABLED)) {
		werr = WERR_ACCESS_DENIED;
		goto done;
	}
#endif
	status = dcerpc_samr_QueryGroupMember(b, talloc_tos(),
					      &group_handle,
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

	{
	struct lsa_Strings names;
	struct samr_Ids member_types;

	status = dcerpc_samr_LookupRids(b, talloc_tos(),
					&domain_handle,
					rid_array->count,
					rid_array->rids,
					&names,
					&member_types,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (names.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (member_types.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	}

	for (i=0; i < rid_array->count; i++) {

		status = dcerpc_samr_DeleteGroupMember(b, talloc_tos(),
						       &group_handle,
						       rid_array->rids[i],
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}
	}

	status = dcerpc_samr_DeleteDomainGroup(b, talloc_tos(),
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

	ZERO_STRUCT(group_handle);

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

WERROR NetGroupDel_l(struct libnetapi_ctx *ctx,
		     struct NetGroupDel *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupDel);
}

/****************************************************************
****************************************************************/

WERROR NetGroupSetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetGroupSetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name;
	struct dom_sid2 *domain_sid = NULL;
	struct dcerpc_binding_handle *b = NULL;

	struct samr_Ids rids;
	struct samr_Ids types;
	union samr_GroupInfo info;
	struct GROUP_INFO_0 *g0;
	struct GROUP_INFO_1 *g1;
	struct GROUP_INFO_2 *g2;
	struct GROUP_INFO_3 *g3;
	struct GROUP_INFO_1002 *g1002;
	struct GROUP_INFO_1005 *g1005;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
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

	init_lsa_String(&lsa_group_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_group_name,
					 &rids,
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
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_DOM_GRP) {
		werr = WERR_INVALID_DATATYPE;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_SET_INFO |
				       SAMR_GROUP_ACCESS_LOOKUP_INFO,
				       rids.ids[0],
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

	switch (r->in.level) {
		case 0:
			g0 = (struct GROUP_INFO_0 *)r->in.buffer;
			init_lsa_String(&info.name, g0->grpi0_name);
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFONAME,
							  &info,
							  &result);
			break;
		case 1:
			g1 = (struct GROUP_INFO_1 *)r->in.buffer;
			init_lsa_String(&info.description, g1->grpi1_comment);
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFODESCRIPTION,
							  &info,
							  &result);
			break;
		case 2:
			g2 = (struct GROUP_INFO_2 *)r->in.buffer;
			init_lsa_String(&info.description, g2->grpi2_comment);
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFODESCRIPTION,
							  &info,
							  &result);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
			if (!NT_STATUS_IS_OK(result)) {
				werr = ntstatus_to_werror(result);
				goto done;
			}

			info.attributes.attributes = g2->grpi2_attributes;
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFOATTRIBUTES,
							  &info,
							  &result);
			break;
		case 3:
			g3 = (struct GROUP_INFO_3 *)r->in.buffer;
			init_lsa_String(&info.description, g3->grpi3_comment);
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFODESCRIPTION,
							  &info,
							  &result);
			if (!NT_STATUS_IS_OK(status)) {
				werr = ntstatus_to_werror(status);
				goto done;
			}
			if (!NT_STATUS_IS_OK(result)) {
				werr = ntstatus_to_werror(result);
				goto done;
			}

			info.attributes.attributes = g3->grpi3_attributes;
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFOATTRIBUTES,
							  &info,
							  &result);
			break;
		case 1002:
			g1002 = (struct GROUP_INFO_1002 *)r->in.buffer;
			init_lsa_String(&info.description, g1002->grpi1002_comment);
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFODESCRIPTION,
							  &info,
							  &result);
			break;
		case 1005:
			g1005 = (struct GROUP_INFO_1005 *)r->in.buffer;
			info.attributes.attributes = g1005->grpi1005_attributes;
			status = dcerpc_samr_SetGroupInfo(b, talloc_tos(),
							  &group_handle,
							  GROUPINFOATTRIBUTES,
							  &info,
							  &result);
			break;
		default:
			status = NT_STATUS_INVALID_LEVEL;
			break;
	}

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

WERROR NetGroupSetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetGroupSetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupSetInfo);
}

/****************************************************************
****************************************************************/

static WERROR map_group_info_to_buffer(TALLOC_CTX *mem_ctx,
				       uint32_t level,
				       struct samr_GroupInfoAll *info,
				       struct dom_sid2 *domain_sid,
				       uint32_t rid,
				       uint8_t **buffer)
{
	struct GROUP_INFO_0 info0;
	struct GROUP_INFO_1 info1;
	struct GROUP_INFO_2 info2;
	struct GROUP_INFO_3 info3;
	struct dom_sid sid;

	switch (level) {
		case 0:
			info0.grpi0_name	= info->name.string;

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info0, sizeof(info0));

			break;
		case 1:
			info1.grpi1_name	= info->name.string;
			info1.grpi1_comment	= info->description.string;

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info1, sizeof(info1));

			break;
		case 2:
			info2.grpi2_name	= info->name.string;
			info2.grpi2_comment	= info->description.string;
			info2.grpi2_group_id	= rid;
			info2.grpi2_attributes	= info->attributes;

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info2, sizeof(info2));

			break;
		case 3:
			if (!sid_compose(&sid, domain_sid, rid)) {
				return WERR_NOMEM;
			}

			info3.grpi3_name	= info->name.string;
			info3.grpi3_comment	= info->description.string;
			info3.grpi3_attributes	= info->attributes;
			info3.grpi3_group_sid	= (struct domsid *)dom_sid_dup(mem_ctx, &sid);

			*buffer = (uint8_t *)talloc_memdup(mem_ctx, &info3, sizeof(info3));

			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	W_ERROR_HAVE_NO_MEMORY(*buffer);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR NetGroupGetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetGroupGetInfo *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name;
	struct dom_sid2 *domain_sid = NULL;
	struct dcerpc_binding_handle *b = NULL;

	struct samr_Ids rids;
	struct samr_Ids types;
	union samr_GroupInfo *info = NULL;
	bool group_info_all = false;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
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

	init_lsa_String(&lsa_group_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_group_name,
					 &rids,
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
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_DOM_GRP) {
		werr = WERR_INVALID_DATATYPE;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_LOOKUP_INFO,
				       rids.ids[0],
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

	status = dcerpc_samr_QueryGroupInfo(b, talloc_tos(),
					    &group_handle,
					    GROUPINFOALL2,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (NT_STATUS_EQUAL(result, NT_STATUS_INVALID_INFO_CLASS)) {
		status = dcerpc_samr_QueryGroupInfo(b, talloc_tos(),
						    &group_handle,
						    GROUPINFOALL,
						    &info,
						    &result);
		group_info_all = true;
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	werr = map_group_info_to_buffer(ctx, r->in.level,
					group_info_all ? &info->all : &info->all2,
					domain_sid, rids.ids[0],
					r->out.buffer);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}
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

WERROR NetGroupGetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetGroupGetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupGetInfo);
}

/****************************************************************
****************************************************************/

WERROR NetGroupAddUser_r(struct libnetapi_ctx *ctx,
			 struct NetGroupAddUser *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name, lsa_user_name;
	struct dom_sid2 *domain_sid = NULL;
	struct dcerpc_binding_handle *b = NULL;

	struct samr_Ids rids;
	struct samr_Ids types;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
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

	init_lsa_String(&lsa_group_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_group_name,
					 &rids,
					 &types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = WERR_GROUPNOTFOUND;
		goto done;
	}
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_DOM_GRP) {
		werr = WERR_GROUPNOTFOUND;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_ADD_MEMBER,
				       rids.ids[0],
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

	init_lsa_String(&lsa_user_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_user_name,
					 &rids,
					 &types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = WERR_USER_NOT_FOUND;
		goto done;
	}
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_USER) {
		werr = WERR_USER_NOT_FOUND;
		goto done;
	}

	status = dcerpc_samr_AddGroupMember(b, talloc_tos(),
					    &group_handle,
					    rids.ids[0],
					    7, /* why ? */
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

WERROR NetGroupAddUser_l(struct libnetapi_ctx *ctx,
			 struct NetGroupAddUser *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupAddUser);
}

/****************************************************************
****************************************************************/

WERROR NetGroupDelUser_r(struct libnetapi_ctx *ctx,
			 struct NetGroupDelUser *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status, result;
	WERROR werr;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_group_name, lsa_user_name;
	struct dom_sid2 *domain_sid = NULL;
	struct dcerpc_binding_handle *b = NULL;

	struct samr_Ids rids;
	struct samr_Ids types;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);
	ZERO_STRUCT(group_handle);

	if (!r->in.group_name) {
		return WERR_INVALID_PARAM;
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

	init_lsa_String(&lsa_group_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_group_name,
					 &rids,
					 &types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = WERR_GROUPNOTFOUND;
		goto done;
	}
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_DOM_GRP) {
		werr = WERR_GROUPNOTFOUND;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_REMOVE_MEMBER,
				       rids.ids[0],
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

	init_lsa_String(&lsa_user_name, r->in.user_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_user_name,
					 &rids,
					 &types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		werr = WERR_USER_NOT_FOUND;
		goto done;
	}
	if (rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	if (types.ids[0] != SID_NAME_USER) {
		werr = WERR_USER_NOT_FOUND;
		goto done;
	}

	status = dcerpc_samr_DeleteGroupMember(b, talloc_tos(),
					       &group_handle,
					       rids.ids[0],
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

WERROR NetGroupDelUser_l(struct libnetapi_ctx *ctx,
			 struct NetGroupDelUser *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupDelUser);
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_disp_groups_to_GROUP_INFO_0_buffer(TALLOC_CTX *mem_ctx,
							      struct samr_DispInfoFullGroups *groups,
							      uint8_t **buffer)
{
	struct GROUP_INFO_0 *g0;
	int i;

	g0 = TALLOC_ZERO_ARRAY(mem_ctx, struct GROUP_INFO_0, groups->count);
	W_ERROR_HAVE_NO_MEMORY(g0);

	for (i=0; i<groups->count; i++) {
		g0[i].grpi0_name = talloc_strdup(mem_ctx,
			groups->entries[i].account_name.string);
		W_ERROR_HAVE_NO_MEMORY(g0[i].grpi0_name);
	}

	*buffer = (uint8_t *)talloc_memdup(mem_ctx, g0,
					   sizeof(struct GROUP_INFO_0) * groups->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_disp_groups_to_GROUP_INFO_1_buffer(TALLOC_CTX *mem_ctx,
							      struct samr_DispInfoFullGroups *groups,
							      uint8_t **buffer)
{
	struct GROUP_INFO_1 *g1;
	int i;

	g1 = TALLOC_ZERO_ARRAY(mem_ctx, struct GROUP_INFO_1, groups->count);
	W_ERROR_HAVE_NO_MEMORY(g1);

	for (i=0; i<groups->count; i++) {
		g1[i].grpi1_name = talloc_strdup(mem_ctx,
			groups->entries[i].account_name.string);
		g1[i].grpi1_comment = talloc_strdup(mem_ctx,
			groups->entries[i].description.string);
		W_ERROR_HAVE_NO_MEMORY(g1[i].grpi1_name);
	}

	*buffer = (uint8_t *)talloc_memdup(mem_ctx, g1,
					   sizeof(struct GROUP_INFO_1) * groups->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_disp_groups_to_GROUP_INFO_2_buffer(TALLOC_CTX *mem_ctx,
							      struct samr_DispInfoFullGroups *groups,
							      uint8_t **buffer)
{
	struct GROUP_INFO_2 *g2;
	int i;

	g2 = TALLOC_ZERO_ARRAY(mem_ctx, struct GROUP_INFO_2, groups->count);
	W_ERROR_HAVE_NO_MEMORY(g2);

	for (i=0; i<groups->count; i++) {
		g2[i].grpi2_name = talloc_strdup(mem_ctx,
			groups->entries[i].account_name.string);
		g2[i].grpi2_comment = talloc_strdup(mem_ctx,
			groups->entries[i].description.string);
		g2[i].grpi2_group_id = groups->entries[i].rid;
		g2[i].grpi2_attributes = groups->entries[i].acct_flags;
		W_ERROR_HAVE_NO_MEMORY(g2[i].grpi2_name);
	}

	*buffer = (uint8_t *)talloc_memdup(mem_ctx, g2,
					   sizeof(struct GROUP_INFO_2) * groups->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_disp_groups_to_GROUP_INFO_3_buffer(TALLOC_CTX *mem_ctx,
							      struct samr_DispInfoFullGroups *groups,
							      const struct dom_sid *domain_sid,
							      uint8_t **buffer)
{
	struct GROUP_INFO_3 *g3;
	int i;

	g3 = TALLOC_ZERO_ARRAY(mem_ctx, struct GROUP_INFO_3, groups->count);
	W_ERROR_HAVE_NO_MEMORY(g3);

	for (i=0; i<groups->count; i++) {

		struct dom_sid sid;

		if (!sid_compose(&sid, domain_sid, groups->entries[i].rid)) {
			return WERR_NOMEM;
		}

		g3[i].grpi3_name = talloc_strdup(mem_ctx,
			groups->entries[i].account_name.string);
		g3[i].grpi3_comment = talloc_strdup(mem_ctx,
			groups->entries[i].description.string);
		g3[i].grpi3_group_sid = (struct domsid *)dom_sid_dup(mem_ctx, &sid);
		g3[i].grpi3_attributes = groups->entries[i].acct_flags;
		W_ERROR_HAVE_NO_MEMORY(g3[i].grpi3_name);
	}

	*buffer = (uint8_t *)talloc_memdup(mem_ctx, g3,
					   sizeof(struct GROUP_INFO_3) * groups->count);
	W_ERROR_HAVE_NO_MEMORY(*buffer);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR convert_samr_disp_groups_to_GROUP_INFO_buffer(TALLOC_CTX *mem_ctx,
							    uint32_t level,
							    struct samr_DispInfoFullGroups *groups,
							    const struct dom_sid *domain_sid,
							    uint32_t *entries_read,
							    uint8_t **buffer)
{
	if (entries_read) {
		*entries_read = groups->count;
	}

	switch (level) {
		case 0:
			return convert_samr_disp_groups_to_GROUP_INFO_0_buffer(mem_ctx, groups, buffer);
		case 1:
			return convert_samr_disp_groups_to_GROUP_INFO_1_buffer(mem_ctx, groups, buffer);
		case 2:
			return convert_samr_disp_groups_to_GROUP_INFO_2_buffer(mem_ctx, groups, buffer);
		case 3:
			return convert_samr_disp_groups_to_GROUP_INFO_3_buffer(mem_ctx, groups, domain_sid, buffer);
		default:
			return WERR_UNKNOWN_LEVEL;
	}
}

/****************************************************************
****************************************************************/

WERROR NetGroupEnum_r(struct libnetapi_ctx *ctx,
		      struct NetGroupEnum *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle;
	struct dom_sid2 *domain_sid = NULL;
	struct policy_handle domain_handle;
	union samr_DispInfo info;
	union samr_DomainInfo *domain_info = NULL;
	struct dcerpc_binding_handle *b = NULL;

	uint32_t total_size = 0;
	uint32_t returned_size = 0;

	NTSTATUS result = NT_STATUS_OK;
	NTSTATUS status = NT_STATUS_OK;
	WERROR werr, tmp_werr;

	ZERO_STRUCT(connect_handle);
	ZERO_STRUCT(domain_handle);

	switch (r->in.level) {
		case 0:
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

	status = dcerpc_samr_QueryDomainInfo(b, talloc_tos(),
					     &domain_handle,
					     2,
					     &domain_info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	if (r->out.total_entries) {
		*r->out.total_entries = domain_info->general.num_groups;
	}

	status = dcerpc_samr_QueryDisplayInfo2(b,
					       ctx,
					       &domain_handle,
					       3,
					       r->in.resume_handle ?
					       *r->in.resume_handle : 0,
					       (uint32_t)-1,
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

	if (r->out.resume_handle && info.info3.count > 0) {
		*r->out.resume_handle =
			info.info3.entries[info.info3.count-1].idx;
	}

	tmp_werr = convert_samr_disp_groups_to_GROUP_INFO_buffer(ctx,
								 r->in.level,
								 &info.info3,
								 domain_sid,
								 r->out.entries_read,
								 r->out.buffer);
	if (!W_ERROR_IS_OK(tmp_werr)) {
		werr = tmp_werr;
		goto done;
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

WERROR NetGroupEnum_l(struct libnetapi_ctx *ctx,
		      struct NetGroupEnum *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupEnum);
}

/****************************************************************
****************************************************************/

WERROR NetGroupGetUsers_r(struct libnetapi_ctx *ctx,
			  struct NetGroupGetUsers *r)
{
	/* FIXME: this call needs to cope with large replies */

	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	struct samr_Ids group_rids, name_types;
	struct samr_RidAttrArray *rid_array = NULL;
	struct lsa_Strings names;
	struct samr_Ids member_types;
	struct dcerpc_binding_handle *b = NULL;

	int i;
	uint32_t entries_read = 0;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;

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

	init_lsa_String(&lsa_account_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
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
	if (group_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (name_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_GET_MEMBERS,
				       group_rids.ids[0],
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

	status = dcerpc_samr_QueryGroupMember(b, talloc_tos(),
					      &group_handle,
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

	status = dcerpc_samr_LookupRids(b, talloc_tos(),
					&domain_handle,
					rid_array->count,
					rid_array->rids,
					&names,
					&member_types,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (names.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (member_types.count != rid_array->count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	for (i=0; i < names.count; i++) {

		if (member_types.ids[i] != SID_NAME_USER) {
			continue;
		}

		status = add_GROUP_USERS_INFO_X_buffer(ctx,
						       r->in.level,
						       names.names[i].string,
						       7,
						       r->out.buffer,
						       &entries_read);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	*r->out.entries_read = entries_read;
	*r->out.total_entries = entries_read;

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

WERROR NetGroupGetUsers_l(struct libnetapi_ctx *ctx,
			  struct NetGroupGetUsers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupGetUsers);
}

/****************************************************************
****************************************************************/

WERROR NetGroupSetUsers_r(struct libnetapi_ctx *ctx,
			  struct NetGroupSetUsers *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct policy_handle connect_handle, domain_handle, group_handle;
	struct lsa_String lsa_account_name;
	struct dom_sid2 *domain_sid = NULL;
	union samr_GroupInfo *group_info = NULL;
	struct samr_Ids user_rids, name_types;
	struct samr_Ids group_rids, group_types;
	struct samr_RidAttrArray *rid_array = NULL;
	struct lsa_String *lsa_names = NULL;
	struct dcerpc_binding_handle *b = NULL;

	uint32_t *add_rids = NULL;
	uint32_t *del_rids = NULL;
	size_t num_add_rids = 0;
	size_t num_del_rids = 0;

	uint32_t *member_rids = NULL;

	struct GROUP_USERS_INFO_0 *i0 = NULL;
	struct GROUP_USERS_INFO_1 *i1 = NULL;

	int i, k;

	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	WERROR werr;

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

	init_lsa_String(&lsa_account_name, r->in.group_name);

	status = dcerpc_samr_LookupNames(b, talloc_tos(),
					 &domain_handle,
					 1,
					 &lsa_account_name,
					 &group_rids,
					 &group_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}
	if (group_rids.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (group_types.count != 1) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, talloc_tos(),
				       &domain_handle,
				       SAMR_GROUP_ACCESS_GET_MEMBERS |
				       SAMR_GROUP_ACCESS_ADD_MEMBER |
				       SAMR_GROUP_ACCESS_REMOVE_MEMBER |
				       SAMR_GROUP_ACCESS_LOOKUP_INFO,
				       group_rids.ids[0],
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

	status = dcerpc_samr_QueryGroupInfo(b, talloc_tos(),
					    &group_handle,
					    GROUPINFOATTRIBUTES,
					    &group_info,
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

	if (r->in.num_entries != user_rids.count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}
	if (r->in.num_entries != name_types.count) {
		werr = WERR_BAD_NET_RESP;
		goto done;
	}

	member_rids = user_rids.ids;

	status = dcerpc_samr_QueryGroupMember(b, talloc_tos(),
					      &group_handle,
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
			if (member_rids[i] == rid_array->rids[k]) {
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
			if (member_rids[i] == rid_array->rids[k]) {
				keep_member = true;
				break;
			}
		}
		if (!keep_member) {
			if (!add_rid_to_array_unique(ctx,
						     rid_array->rids[k],
						     &del_rids, &num_del_rids)) {
				werr = WERR_GENERAL_FAILURE;
				goto done;
			}
		}
	}

	/* add list */

	for (i=0; i < num_add_rids; i++) {
		status = dcerpc_samr_AddGroupMember(b, talloc_tos(),
						    &group_handle,
						    add_rids[i],
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
	}

	/* del list */

	for (i=0; i < num_del_rids; i++) {
		status = dcerpc_samr_DeleteGroupMember(b, talloc_tos(),
						       &group_handle,
						       del_rids[i],
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
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

WERROR NetGroupSetUsers_l(struct libnetapi_ctx *ctx,
			  struct NetGroupSetUsers *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetGroupSetUsers);
}
