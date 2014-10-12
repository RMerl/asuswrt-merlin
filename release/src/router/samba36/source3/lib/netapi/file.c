/*
 *  Unix SMB/CIFS implementation.
 *  NetApi File Support
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

WERROR NetFileClose_r(struct libnetapi_ctx *ctx,
		      struct NetFileClose *r)
{
	WERROR werr;
	NTSTATUS status;
	struct dcerpc_binding_handle *b;

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetFileClose(b, talloc_tos(),
					    r->in.server_name,
					    r->in.fileid,
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

WERROR NetFileClose_l(struct libnetapi_ctx *ctx,
		      struct NetFileClose *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetFileClose);
}

/****************************************************************
****************************************************************/

static NTSTATUS map_srvsvc_FileInfo_to_FILE_INFO_buffer(TALLOC_CTX *mem_ctx,
							uint32_t level,
							union srvsvc_NetFileInfo *info,
							uint8_t **buffer,
							uint32_t *num_entries)
{
	struct FILE_INFO_2 i2;
	struct FILE_INFO_3 i3;

	switch (level) {
		case 2:
			i2.fi2_id		= info->info2->fid;

			ADD_TO_ARRAY(mem_ctx, struct FILE_INFO_2, i2,
				     (struct FILE_INFO_2 **)buffer,
				     num_entries);
			break;
		case 3:
			i3.fi3_id		= info->info3->fid;
			i3.fi3_permissions	= info->info3->permissions;
			i3.fi3_num_locks	= info->info3->num_locks;
			i3.fi3_pathname		= talloc_strdup(mem_ctx, info->info3->path);
			i3.fi3_username		= talloc_strdup(mem_ctx, info->info3->user);

			NT_STATUS_HAVE_NO_MEMORY(i3.fi3_pathname);
			NT_STATUS_HAVE_NO_MEMORY(i3.fi3_username);

			ADD_TO_ARRAY(mem_ctx, struct FILE_INFO_3, i3,
				     (struct FILE_INFO_3 **)buffer,
				     num_entries);
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetFileGetInfo_r(struct libnetapi_ctx *ctx,
			struct NetFileGetInfo *r)
{
	WERROR werr;
	NTSTATUS status;
	union srvsvc_NetFileInfo info;
	uint32_t num_entries = 0;
	struct dcerpc_binding_handle *b;

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 2:
		case 3:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetFileGetInfo(b, talloc_tos(),
					      r->in.server_name,
					      r->in.fileid,
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

	status = map_srvsvc_FileInfo_to_FILE_INFO_buffer(ctx,
							 r->in.level,
							 &info,
							 r->out.buffer,
							 &num_entries);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetFileGetInfo_l(struct libnetapi_ctx *ctx,
			struct NetFileGetInfo *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetFileGetInfo);
}

/****************************************************************
****************************************************************/

WERROR NetFileEnum_r(struct libnetapi_ctx *ctx,
		     struct NetFileEnum *r)
{
	WERROR werr;
	NTSTATUS status;
	struct srvsvc_NetFileInfoCtr info_ctr;
	struct srvsvc_NetFileCtr2 ctr2;
	struct srvsvc_NetFileCtr3 ctr3;
	uint32_t num_entries = 0;
	uint32_t i;
	struct dcerpc_binding_handle *b;

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 2:
		case 3:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	ZERO_STRUCT(info_ctr);

	info_ctr.level = r->in.level;
	switch (r->in.level) {
		case 2:
			ZERO_STRUCT(ctr2);
			info_ctr.ctr.ctr2 = &ctr2;
			break;
		case 3:
			ZERO_STRUCT(ctr3);
			info_ctr.ctr.ctr3 = &ctr3;
			break;
	}

	status = dcerpc_srvsvc_NetFileEnum(b, talloc_tos(),
					   r->in.server_name,
					   r->in.base_path,
					   r->in.user_name,
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

	for (i=0; i < info_ctr.ctr.ctr2->count; i++) {
		union srvsvc_NetFileInfo _i;
		switch (r->in.level) {
			case 2:
				_i.info2 = &info_ctr.ctr.ctr2->array[i];
				break;
			case 3:
				_i.info3 = &info_ctr.ctr.ctr3->array[i];
				break;
		}

		status = map_srvsvc_FileInfo_to_FILE_INFO_buffer(ctx,
								 r->in.level,
								 &_i,
								 r->out.buffer,
								 &num_entries);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
	}

	if (r->out.entries_read) {
		*r->out.entries_read = num_entries;
	}

	if (r->out.total_entries) {
		*r->out.total_entries = num_entries;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetFileEnum_l(struct libnetapi_ctx *ctx,
		     struct NetFileEnum *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetFileEnum);
}
