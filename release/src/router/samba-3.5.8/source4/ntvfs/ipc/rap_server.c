/* 
   Unix SMB/CIFS implementation.
   RAP handlers

   Copyright (C) Volker Lendecke 2004

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
#include "param/share.h"
#include "libcli/rap/rap.h"
#include "libcli/raw/interfaces.h"
#include "librpc/gen_ndr/srvsvc.h"
#include "rpc_server/common/common.h"
#include "param/param.h"
#include "ntvfs/ipc/ipc.h"
#include "ntvfs/ipc/proto.h"

/* At this moment these are just dummy functions, but you might get the
 * idea. */

NTSTATUS rap_netshareenum(TALLOC_CTX *mem_ctx,
			  struct tevent_context *event_ctx,
			  struct loadparm_context *lp_ctx,
			  struct rap_NetShareEnum *r)
{
	NTSTATUS nterr;
	const char **snames;
	struct share_context *sctx;
	struct share_config *scfg;
	int i, j, count;

	r->out.status = 0;
	r->out.available = 0;
	r->out.info = NULL;

	nterr = share_get_context_by_name(mem_ctx, lp_share_backend(lp_ctx), event_ctx, lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return nterr;
	}

	nterr = share_list_all(mem_ctx, sctx, &count, &snames);
	if (!NT_STATUS_IS_OK(nterr)) {
		return nterr;
	}

	r->out.available = count;
	r->out.info = talloc_array(mem_ctx,
				   union rap_shareenum_info, r->out.available);

	for (i = 0, j = 0; i < r->out.available; i++) {
		if (!NT_STATUS_IS_OK(share_get_config(mem_ctx, sctx, snames[i], &scfg))) {
			DEBUG(3, ("WARNING: Service [%s] disappeared after enumeration!\n", snames[i]));
			continue;
		}
		strncpy(r->out.info[j].info1.name,
			snames[i],
			sizeof(r->out.info[0].info1.name));
		r->out.info[i].info1.pad = 0;
		r->out.info[i].info1.type = dcesrv_common_get_share_type(mem_ctx, NULL, scfg);
		r->out.info[i].info1.comment = talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_COMMENT, ""));
		talloc_free(scfg);
		j++;
	}
	r->out.available = j;
	
	return NT_STATUS_OK;
}

NTSTATUS rap_netserverenum2(TALLOC_CTX *mem_ctx,
			    struct loadparm_context *lp_ctx,
			    struct rap_NetServerEnum2 *r)
{
	r->out.status = 0;
	r->out.available = 0;
	return NT_STATUS_OK;
}
