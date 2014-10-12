/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   
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
#include "system/time.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"

/*
 * get the remote time of a server via srvsvc_NetRemoteTOD
 */
static NTSTATUS libnet_RemoteTOD_srvsvc(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_RemoteTOD *r)
{
        NTSTATUS status;
	struct libnet_RpcConnect c;
	struct srvsvc_NetRemoteTOD tod;
	struct srvsvc_NetRemoteTODInfo *info = NULL;
	struct tm tm;

	ZERO_STRUCT(c);

	/* prepare connect to the SRVSVC pipe of a timeserver */
	c.level             = LIBNET_RPC_CONNECT_SERVER;
	c.in.name           = r->srvsvc.in.server_name;
	c.in.dcerpc_iface   = &ndr_table_srvsvc;

	/* 1. connect to the SRVSVC pipe of a timeserver */
	status = libnet_RpcConnect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->srvsvc.out.error_string = talloc_asprintf(mem_ctx,
						"Connection to SRVSVC pipe of server '%s' failed: %s",
						r->srvsvc.in.server_name, nt_errstr(status));
		return status;
	}

	/* prepare srvsvc_NetrRemoteTOD */
	tod.in.server_unc = talloc_asprintf(mem_ctx, "\\%s", c.in.name);
	tod.out.info = &info;

	/* 2. try srvsvc_NetRemoteTOD */
	status = dcerpc_srvsvc_NetRemoteTOD_r(c.out.dcerpc_pipe->binding_handle, mem_ctx, &tod);
	if (!NT_STATUS_IS_OK(status)) {
		r->srvsvc.out.error_string = talloc_asprintf(mem_ctx,
						"srvsvc_NetrRemoteTOD on server '%s' failed: %s",
						r->srvsvc.in.server_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of srvsvc_NetrRemoteTOD */
	if (!W_ERROR_IS_OK(tod.out.result)) {
		r->srvsvc.out.error_string = talloc_asprintf(mem_ctx,
						"srvsvc_NetrRemoteTOD on server '%s' failed: %s",
						r->srvsvc.in.server_name, win_errstr(tod.out.result));
		status = werror_to_ntstatus(tod.out.result);
		goto disconnect;
	}

	/* need to set the out parameters */
	tm.tm_sec = (int)info->secs;
	tm.tm_min = (int)info->mins;
	tm.tm_hour = (int)info->hours;
	tm.tm_mday = (int)info->day;
	tm.tm_mon = (int)info->month -1;
	tm.tm_year = (int)info->year - 1900;
	tm.tm_wday = -1;
	tm.tm_yday = -1;
	tm.tm_isdst = -1;

	r->srvsvc.out.time = timegm(&tm);
	r->srvsvc.out.time_zone = info->timezone * 60;

	goto disconnect;

disconnect:
	/* close connection */
	talloc_free(c.out.dcerpc_pipe);

	return status;
}

static NTSTATUS libnet_RemoteTOD_generic(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_RemoteTOD *r)
{
	NTSTATUS status;
	union libnet_RemoteTOD r2;

	r2.srvsvc.level			= LIBNET_REMOTE_TOD_SRVSVC;
	r2.srvsvc.in.server_name	= r->generic.in.server_name;

	status = libnet_RemoteTOD(ctx, mem_ctx, &r2);

	r->generic.out.time		= r2.srvsvc.out.time;
	r->generic.out.time_zone	= r2.srvsvc.out.time_zone;

	r->generic.out.error_string	= r2.srvsvc.out.error_string;

	return status;
}

NTSTATUS libnet_RemoteTOD(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_RemoteTOD *r)
{
	switch (r->generic.level) {
		case LIBNET_REMOTE_TOD_GENERIC:
			return libnet_RemoteTOD_generic(ctx, mem_ctx, r);
		case LIBNET_REMOTE_TOD_SRVSVC:
			return libnet_RemoteTOD_srvsvc(ctx, mem_ctx, r);
	}

	return NT_STATUS_INVALID_LEVEL;
}
