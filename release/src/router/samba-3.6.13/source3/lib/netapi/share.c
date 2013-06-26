/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Share Support
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
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"

/****************************************************************
****************************************************************/

static NTSTATUS map_srvsvc_share_info_to_SHARE_INFO_buffer(TALLOC_CTX *mem_ctx,
							   uint32_t level,
							   union srvsvc_NetShareInfo *info,
							   uint8_t **buffer,
							   uint32_t *num_shares)
{
	struct SHARE_INFO_0 i0;
	struct SHARE_INFO_1 i1;
	struct SHARE_INFO_2 i2;
	struct SHARE_INFO_501 i501;
	struct SHARE_INFO_1005 i1005;

	struct srvsvc_NetShareInfo0 *s0;
	struct srvsvc_NetShareInfo1 *s1;
	struct srvsvc_NetShareInfo2 *s2;
	struct srvsvc_NetShareInfo501 *s501;
	struct srvsvc_NetShareInfo1005 *s1005;

	if (!buffer) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (level) {
		case 0:
			s0 = info->info0;

			i0.shi0_netname		= talloc_strdup(mem_ctx, s0->name);

			ADD_TO_ARRAY(mem_ctx, struct SHARE_INFO_0, i0,
				     (struct SHARE_INFO_0 **)buffer,
				     num_shares);
			break;

		case 1:
			s1 = info->info1;

			i1.shi1_netname		= talloc_strdup(mem_ctx, s1->name);
			i1.shi1_type		= s1->type;
			i1.shi1_remark		= talloc_strdup(mem_ctx, s1->comment);

			ADD_TO_ARRAY(mem_ctx, struct SHARE_INFO_1, i1,
				     (struct SHARE_INFO_1 **)buffer,
				     num_shares);
			break;

		case 2:
			s2 = info->info2;

			i2.shi2_netname		= talloc_strdup(mem_ctx, s2->name);
			i2.shi2_type		= s2->type;
			i2.shi2_remark		= talloc_strdup(mem_ctx, s2->comment);
			i2.shi2_permissions	= s2->permissions;
			i2.shi2_max_uses	= s2->max_users;
			i2.shi2_current_uses	= s2->current_users;
			i2.shi2_path		= talloc_strdup(mem_ctx, s2->path);
			i2.shi2_passwd		= talloc_strdup(mem_ctx, s2->password);

			ADD_TO_ARRAY(mem_ctx, struct SHARE_INFO_2, i2,
				     (struct SHARE_INFO_2 **)buffer,
				     num_shares);
			break;

		case 501:
			s501 = info->info501;

			i501.shi501_netname		= talloc_strdup(mem_ctx, s501->name);
			i501.shi501_type		= s501->type;
			i501.shi501_remark		= talloc_strdup(mem_ctx, s501->comment);
			i501.shi501_flags		= s501->csc_policy;

			ADD_TO_ARRAY(mem_ctx, struct SHARE_INFO_501, i501,
				     (struct SHARE_INFO_501 **)buffer,
				     num_shares);
			break;

		case 1005:
			s1005 = info->info1005;

			i1005.shi1005_flags		= s1005->dfs_flags;

			ADD_TO_ARRAY(mem_ctx, struct SHARE_INFO_1005, i1005,
				     (struct SHARE_INFO_1005 **)buffer,
				     num_shares);
			break;

		default:
			return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS map_SHARE_INFO_buffer_to_srvsvc_share_info(TALLOC_CTX *mem_ctx,
							   uint8_t *buffer,
							   uint32_t level,
							   union srvsvc_NetShareInfo *info)
{
	struct SHARE_INFO_2 *i2 = NULL;
	struct SHARE_INFO_1004 *i1004 = NULL;
	struct srvsvc_NetShareInfo2 *s2 = NULL;
	struct srvsvc_NetShareInfo1004 *s1004 = NULL;

	if (!buffer) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (level) {
		case 2:
			i2 = (struct SHARE_INFO_2 *)buffer;

			s2 = TALLOC_P(mem_ctx, struct srvsvc_NetShareInfo2);
			NT_STATUS_HAVE_NO_MEMORY(s2);

			s2->name		= i2->shi2_netname;
			s2->type		= i2->shi2_type;
			s2->comment		= i2->shi2_remark;
			s2->permissions		= i2->shi2_permissions;
			s2->max_users		= i2->shi2_max_uses;
			s2->current_users	= i2->shi2_current_uses;
			s2->path		= i2->shi2_path;
			s2->password		= i2->shi2_passwd;

			info->info2 = s2;

			break;
		case 1004:
			i1004 = (struct SHARE_INFO_1004 *)buffer;

			s1004 = TALLOC_P(mem_ctx, struct srvsvc_NetShareInfo1004);
			NT_STATUS_HAVE_NO_MEMORY(s1004);

			s1004->comment		= i1004->shi1004_remark;

			info->info1004 = s1004;

			break;
		default:
			return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetShareAdd_r(struct libnetapi_ctx *ctx,
		     struct NetShareAdd *r)
{
	WERROR werr;
	NTSTATUS status;
	union srvsvc_NetShareInfo info;
	struct dcerpc_binding_handle *b;

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 2:
			break;
		case 502:
		case 503:
			return WERR_NOT_SUPPORTED;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = map_SHARE_INFO_buffer_to_srvsvc_share_info(ctx,
							    r->in.buffer,
							    r->in.level,
							    &info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	status = dcerpc_srvsvc_NetShareAdd(b, talloc_tos(),
					   r->in.server_name,
					   r->in.level,
					   &info,
					   r->out.parm_err,
					   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShareAdd_l(struct libnetapi_ctx *ctx,
		     struct NetShareAdd *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShareAdd);
}

/****************************************************************
****************************************************************/

WERROR NetShareDel_r(struct libnetapi_ctx *ctx,
		     struct NetShareDel *r)
{
	WERROR werr;
	NTSTATUS status;
	struct dcerpc_binding_handle *b;

	if (!r->in.net_name) {
		return WERR_INVALID_PARAM;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetShareDel(b, talloc_tos(),
					   r->in.server_name,
					   r->in.net_name,
					   r->in.reserved,
					   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShareDel_l(struct libnetapi_ctx *ctx,
		     struct NetShareDel *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShareDel);
}

/****************************************************************
****************************************************************/

WERROR NetShareEnum_r(struct libnetapi_ctx *ctx,
		      struct NetShareEnum *r)
{
	WERROR werr;
	NTSTATUS status;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr0 ctr0;
	struct srvsvc_NetShareCtr1 ctr1;
	struct srvsvc_NetShareCtr2 ctr2;
	uint32_t i;
	struct dcerpc_binding_handle *b;

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
		case 2:
			break;
		case 502:
		case 503:
			return WERR_NOT_SUPPORTED;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	ZERO_STRUCT(info_ctr);

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	info_ctr.level = r->in.level;
	switch (r->in.level) {
		case 0:
			ZERO_STRUCT(ctr0);
			info_ctr.ctr.ctr0 = &ctr0;
			break;
		case 1:
			ZERO_STRUCT(ctr1);
			info_ctr.ctr.ctr1 = &ctr1;
			break;
		case 2:
			ZERO_STRUCT(ctr2);
			info_ctr.ctr.ctr2 = &ctr2;
			break;
	}

	status = dcerpc_srvsvc_NetShareEnumAll(b, talloc_tos(),
					       r->in.server_name,
					       &info_ctr,
					       r->in.prefmaxlen,
					       r->out.total_entries,
					       r->out.resume_handle,
					       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr) && !W_ERROR_EQUAL(werr, WERR_MORE_DATA)) {
		goto done;
	}

	for (i=0; i < info_ctr.ctr.ctr1->count; i++) {
		union srvsvc_NetShareInfo _i;
		switch (r->in.level) {
			case 0:
				_i.info0 = &info_ctr.ctr.ctr0->array[i];
				break;
			case 1:
				_i.info1 = &info_ctr.ctr.ctr1->array[i];
				break;
			case 2:
				_i.info2 = &info_ctr.ctr.ctr2->array[i];
				break;
		}

		status = map_srvsvc_share_info_to_SHARE_INFO_buffer(ctx,
								    r->in.level,
								    &_i,
								    r->out.buffer,
								    r->out.entries_read);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShareEnum_l(struct libnetapi_ctx *ctx,
		      struct NetShareEnum *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShareEnum);
}

/****************************************************************
****************************************************************/

WERROR NetShareGetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetShareGetInfo *r)
{
	WERROR werr;
	NTSTATUS status;
	union srvsvc_NetShareInfo info;
	uint32_t num_entries = 0;
	struct dcerpc_binding_handle *b;

	if (!r->in.net_name || !r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 0:
		case 1:
		case 2:
		case 501:
		case 1005:
			break;
		case 502:
		case 503:
			return WERR_NOT_SUPPORTED;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetShareGetInfo(b, talloc_tos(),
					       r->in.server_name,
					       r->in.net_name,
					       r->in.level,
					       &info,
					       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = map_srvsvc_share_info_to_SHARE_INFO_buffer(ctx,
							    r->in.level,
							    &info,
							    r->out.buffer,
							    &num_entries);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShareGetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetShareGetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShareGetInfo);
}

/****************************************************************
****************************************************************/

WERROR NetShareSetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetShareSetInfo *r)
{
	WERROR werr;
	NTSTATUS status;
	union srvsvc_NetShareInfo info;
	struct dcerpc_binding_handle *b;

	if (!r->in.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 2:
		case 1004:
			break;
		case 1:
		case 502:
		case 503:
		case 1005:
		case 1006:
		case 1501:
			return WERR_NOT_SUPPORTED;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = map_SHARE_INFO_buffer_to_srvsvc_share_info(ctx,
							    r->in.buffer,
							    r->in.level,
							    &info);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	status = dcerpc_srvsvc_NetShareSetInfo(b, talloc_tos(),
					       r->in.server_name,
					       r->in.net_name,
					       r->in.level,
					       &info,
					       r->out.parm_err,
					       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShareSetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetShareSetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShareSetInfo);
}
