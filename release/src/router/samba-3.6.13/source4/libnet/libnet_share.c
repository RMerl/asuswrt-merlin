/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Grégory LEOCADIE <gleocadie@idealx.com>
   
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
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"


NTSTATUS libnet_ListShares(struct libnet_context *ctx, 
			   TALLOC_CTX *mem_ctx, struct libnet_ListShares *r)
{
	NTSTATUS status;
	struct libnet_RpcConnect c;
	struct srvsvc_NetShareEnumAll s;
	struct srvsvc_NetShareInfoCtr info_ctr;
	uint32_t resume_handle = 0;
	uint32_t totalentries = 0;
	struct srvsvc_NetShareCtr0 ctr0;
	struct srvsvc_NetShareCtr1 ctr1;
	struct srvsvc_NetShareCtr2 ctr2;
	struct srvsvc_NetShareCtr501 ctr501;
	struct srvsvc_NetShareCtr502 ctr502;

	ZERO_STRUCT(c);

	c.level               = LIBNET_RPC_CONNECT_SERVER;
	c.in.name             = r->in.server_name;
	c.in.dcerpc_iface     = &ndr_table_srvsvc;

	s.in.server_unc = talloc_asprintf(mem_ctx, "\\\\%s", c.in.name);

	status = libnet_RpcConnect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "Connection to SRVSVC pipe of server %s "
						      "failed: %s",
						      r->in.server_name,
						      nt_errstr(status));
		return status;
	}

	info_ctr.level = r->in.level;
	switch (info_ctr.level) {
	case 0:
		info_ctr.ctr.ctr0 = &ctr0;
		ZERO_STRUCT(ctr0);
		break;
	case 1:
		info_ctr.ctr.ctr1 = &ctr1;
		ZERO_STRUCT(ctr1);
		break;
	case 2:
		info_ctr.ctr.ctr2 = &ctr2;
		ZERO_STRUCT(ctr2);
		break;
	case 501:
		info_ctr.ctr.ctr501 = &ctr501;
		ZERO_STRUCT(ctr501);
		break;
	case 502:
		info_ctr.ctr.ctr502 = &ctr502;
		ZERO_STRUCT(ctr502);
		break;
	default:
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "libnet_ListShares: Invalid info level requested: %d",
						      info_ctr.level);
		return NT_STATUS_INVALID_PARAMETER;
	}
	s.in.max_buffer = ~0;
	s.in.resume_handle = &resume_handle;
	s.in.info_ctr = &info_ctr;
	s.out.info_ctr = &info_ctr;
	s.out.totalentries = &totalentries;

	status = dcerpc_srvsvc_NetShareEnumAll_r(c.out.dcerpc_pipe->binding_handle, mem_ctx, &s);
	
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareEnumAll on server '%s' failed"
						      ": %s",
						      r->in.server_name, nt_errstr(status));
		goto disconnect;
	}

	if (!W_ERROR_IS_OK(s.out.result) && !W_ERROR_EQUAL(s.out.result, WERR_MORE_DATA)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareEnumAll on server '%s' failed: %s",
						      r->in.server_name, win_errstr(s.out.result));
		goto disconnect;
	}

	r->out.ctr = s.out.info_ctr->ctr;

disconnect:
	talloc_free(c.out.dcerpc_pipe);

	return status;	
}


NTSTATUS libnet_AddShare(struct libnet_context *ctx,
			 TALLOC_CTX *mem_ctx, struct libnet_AddShare *r)
{
	NTSTATUS status;
	struct libnet_RpcConnect c;
	struct srvsvc_NetShareAdd s;
	union srvsvc_NetShareInfo info;

	ZERO_STRUCT(c);

	c.level              = LIBNET_RPC_CONNECT_SERVER;
	c.in.name            = r->in.server_name;
	c.in.dcerpc_iface    = &ndr_table_srvsvc;

	status = libnet_RpcConnect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "Connection to SRVSVC pipe of server %s "
						      "failed: %s",
						      r->in.server_name, nt_errstr(status));
		return status;
	}

	info.info2		= &r->in.share;

	s.in.level 		= 2;
	s.in.info		= &info;
	s.in.server_unc		= talloc_asprintf(mem_ctx, "\\\\%s", r->in.server_name);
 
	status = dcerpc_srvsvc_NetShareAdd_r(c.out.dcerpc_pipe->binding_handle, mem_ctx, &s);

	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareAdd '%s' on server '%s' failed"
						      ": %s",
						      r->in.share.name, r->in.server_name, 
						      nt_errstr(status));
	} else if (!W_ERROR_IS_OK(s.out.result)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareAdd '%s' on server '%s' failed"
						      ": %s",
						      r->in.share.name, r->in.server_name, 
						      win_errstr(s.out.result));
		status = werror_to_ntstatus(s.out.result);
	}

	talloc_free(c.out.dcerpc_pipe);
	
	return status;
}


NTSTATUS libnet_DelShare(struct libnet_context *ctx,
			 TALLOC_CTX *mem_ctx, struct libnet_DelShare *r)
{
	NTSTATUS status;
	struct libnet_RpcConnect c;
	struct srvsvc_NetShareDel s;

	ZERO_STRUCT(c);

	c.level               = LIBNET_RPC_CONNECT_SERVER;
	c.in.name             = r->in.server_name;
	c.in.dcerpc_iface     = &ndr_table_srvsvc;

	status = libnet_RpcConnect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "Connection to SRVSVC pipe of server %s "
						      "failed: %s",
						      r->in.server_name, nt_errstr(status));
		return status;
	} 
		
	s.in.server_unc = talloc_asprintf(mem_ctx, "\\\\%s", r->in.server_name);
	s.in.share_name = r->in.share_name;

	status = dcerpc_srvsvc_NetShareDel_r(c.out.dcerpc_pipe->binding_handle, mem_ctx, &s);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareDel '%s' on server '%s' failed"
						      ": %s",
						      r->in.share_name, r->in.server_name, 
						      nt_errstr(status));
	} else if (!W_ERROR_IS_OK(s.out.result)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "srvsvc_NetShareDel '%s' on server '%s' failed"
						      ": %s",
						      r->in.share_name, r->in.server_name, 
						      win_errstr(s.out.result));
		status = werror_to_ntstatus(s.out.result);
	}

	talloc_free(c.out.dcerpc_pipe);

	return status;
}
