/* 
   Unix SMB/CIFS implementation.

   endpoint server for the srvsvc pipe

   Copyright (C) Stefan (metze) Metzmacher 2004-2006
   
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
#include "ntvfs/ntvfs.h"
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_srvsvc.h"
#include "rpc_server/common/common.h"
#include "rpc_server/common/share.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "system/time.h"
#include "rpc_server/srvsvc/proto.h"
#include "param/param.h"

#define SRVSVC_CHECK_ADMIN_ACCESS do { \
	struct security_token *t = dce_call->conn->auth_state.session_info->security_token; \
	if (!security_token_has_builtin_administrators(t) && \
	    !security_token_has_sid(t, &global_sid_Builtin_Server_Operators)) { \
	    	return WERR_ACCESS_DENIED; \
	} \
} while (0)

/* 
  srvsvc_NetCharDevEnum 
*/
static WERROR dcesrv_srvsvc_NetCharDevEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct srvsvc_NetCharDevEnum *r)
{
	*r->out.totalentries = 0;

	switch (r->in.info_ctr->level) {
	case 0:
		r->out.info_ctr->ctr.ctr0 = talloc(mem_ctx, struct srvsvc_NetCharDevCtr0);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr0);

		r->out.info_ctr->ctr.ctr0->count = 0;
		r->out.info_ctr->ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;

	case 1:
		r->out.info_ctr->ctr.ctr1 = talloc(mem_ctx, struct srvsvc_NetCharDevCtr1);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr1);

		r->out.info_ctr->ctr.ctr1->count = 0;
		r->out.info_ctr->ctr.ctr1->array = NULL;

		return WERR_NOT_SUPPORTED;

	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetCharDevGetInfo 
*/
static WERROR dcesrv_srvsvc_NetCharDevGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetCharDevGetInfo *r)
{
	ZERO_STRUCTP(r->out.info);

	switch (r->in.level) {
	case 0:
	{
		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetCharDevControl 
*/
static WERROR dcesrv_srvsvc_NetCharDevControl(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetCharDevControl *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetCharDevQEnum 
*/
static WERROR dcesrv_srvsvc_NetCharDevQEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct srvsvc_NetCharDevQEnum *r)
{
	*r->out.totalentries = 0;

	switch (r->in.info_ctr->level) {
	case 0:
	{
		r->out.info_ctr->ctr.ctr0 = talloc(mem_ctx, struct srvsvc_NetCharDevQCtr0);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr0);

		r->out.info_ctr->ctr.ctr0->count = 0;
		r->out.info_ctr->ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		r->out.info_ctr->ctr.ctr1 = talloc(mem_ctx, struct srvsvc_NetCharDevQCtr1);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr1);

		r->out.info_ctr->ctr.ctr1->count = 0;
		r->out.info_ctr->ctr.ctr1->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetCharDevQGetInfo 
*/
static WERROR dcesrv_srvsvc_NetCharDevQGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct srvsvc_NetCharDevQGetInfo *r)
{
	ZERO_STRUCTP(r->out.info);

	switch (r->in.level) {
	case 0:
	{
		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetCharDevQSetInfo 
*/
static WERROR dcesrv_srvsvc_NetCharDevQSetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetCharDevQSetInfo *r)
{
	switch (r->in.level) {
	case 0:
	{
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetCharDevQPurge 
*/
static WERROR dcesrv_srvsvc_NetCharDevQPurge(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetCharDevQPurge *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetCharDevQPurgeSelf 
*/
static WERROR dcesrv_srvsvc_NetCharDevQPurgeSelf(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct srvsvc_NetCharDevQPurgeSelf *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);	
}


/* 
  srvsvc_NetConnEnum 
*/
static WERROR dcesrv_srvsvc_NetConnEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetConnEnum *r)
{
	*r->out.totalentries = 0;

	switch (r->in.info_ctr->level) {
	case 0:
	{
		r->out.info_ctr->ctr.ctr0 = talloc(mem_ctx, struct srvsvc_NetConnCtr0);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr0);

		r->out.info_ctr->ctr.ctr0->count = 0;
		r->out.info_ctr->ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		r->out.info_ctr->ctr.ctr1 = talloc(mem_ctx, struct srvsvc_NetConnCtr1);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr1);

		r->out.info_ctr->ctr.ctr1->count = 0;
		r->out.info_ctr->ctr.ctr1->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetFileEnum 
*/
static WERROR dcesrv_srvsvc_NetFileEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct srvsvc_NetFileEnum *r)
{
	*r->out.totalentries = 0;

	switch (r->in.info_ctr->level) {
	case 2:
	{
		r->out.info_ctr->ctr.ctr2 = talloc(mem_ctx, struct srvsvc_NetFileCtr2);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr2);

		r->out.info_ctr->ctr.ctr2->count = 0;
		r->out.info_ctr->ctr.ctr2->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 3:
	{
		r->out.info_ctr->ctr.ctr3 = talloc(mem_ctx, struct srvsvc_NetFileCtr3);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr3);

		r->out.info_ctr->ctr.ctr3->count = 0;
		r->out.info_ctr->ctr.ctr3->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetFileGetInfo 
*/
static WERROR dcesrv_srvsvc_NetFileGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct srvsvc_NetFileGetInfo *r)
{
	ZERO_STRUCTP(r->out.info);

	switch (r->in.level) {
	case 2:
	{
		return WERR_NOT_SUPPORTED;
	}
	case 3:
	{
		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetFileClose 
*/
static WERROR dcesrv_srvsvc_NetFileClose(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetFileClose *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetSessEnum 
*/
static WERROR dcesrv_srvsvc_NetSessEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSessEnum *r)
{
	*r->out.totalentries = 0;

	switch (r->in.info_ctr->level) {
	case 0:
	{
		r->out.info_ctr->ctr.ctr0 = talloc(mem_ctx, struct srvsvc_NetSessCtr0);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr0);

		r->out.info_ctr->ctr.ctr0->count = 0;
		r->out.info_ctr->ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		r->out.info_ctr->ctr.ctr1 = talloc(mem_ctx, struct srvsvc_NetSessCtr1);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr1);

		r->out.info_ctr->ctr.ctr1->count = 0;
		r->out.info_ctr->ctr.ctr1->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 2:
	{
		r->out.info_ctr->ctr.ctr2 = talloc(mem_ctx, struct srvsvc_NetSessCtr2);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr2);

		r->out.info_ctr->ctr.ctr2->count = 0;
		r->out.info_ctr->ctr.ctr2->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 10:
	{
		r->out.info_ctr->ctr.ctr10 = talloc(mem_ctx, struct srvsvc_NetSessCtr10);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr10);

		r->out.info_ctr->ctr.ctr10->count = 0;
		r->out.info_ctr->ctr.ctr10->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 502:
	{
		r->out.info_ctr->ctr.ctr502 = talloc(mem_ctx, struct srvsvc_NetSessCtr502);
		W_ERROR_HAVE_NO_MEMORY(r->out.info_ctr->ctr.ctr502);

		r->out.info_ctr->ctr.ctr502->count = 0;
		r->out.info_ctr->ctr.ctr502->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetSessDel 
*/
static WERROR dcesrv_srvsvc_NetSessDel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSessDel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetShareAdd 
*/
static WERROR dcesrv_srvsvc_NetShareAdd(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareAdd *r)
{
	switch (r->in.level) {
	case 0:
	{
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		return WERR_NOT_SUPPORTED;
	}
	case 2:
	{
		NTSTATUS nterr;
		struct share_info *info;
		struct share_context *sctx;
		unsigned int count = 8;
		unsigned int i;

		nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
		if (!NT_STATUS_IS_OK(nterr)) {
			return ntstatus_to_werror(nterr);
		}

		/* there are no more than 8 options in struct srvsvc_NetShareInfo2 */
		info = talloc_array(mem_ctx, struct share_info, count);
		W_ERROR_HAVE_NO_MEMORY(info);

		i = 0;

		info[i].name = SHARE_TYPE;
		info[i].type = SHARE_INFO_STRING;
		switch (r->in.info->info2->type) {
		case STYPE_DISKTREE:
			info[i].value = talloc_strdup(info, "DISK");
			break;
		case STYPE_PRINTQ:
			info[i].value = talloc_strdup(info, "PRINTER");
			break;
		case STYPE_IPC:
			info[i].value = talloc_strdup(info, "IPC");
			break;
		default:
			return WERR_INVALID_PARAM;
		}
		W_ERROR_HAVE_NO_MEMORY(info[i].value);
		i++;

		if (r->in.info->info2->path && r->in.info->info2->path[0]) {
			info[i].name = SHARE_PATH;
			info[i].type = SHARE_INFO_STRING;

			/* Windows will send a path in a form of C:\example\path */
			if (r->in.info->info2->path[1] == ':') {
				info[i].value = talloc_strdup(info, &r->in.info->info2->path[2]);
			} else {
				/* very strange let's try to set as is */
				info[i].value = talloc_strdup(info, r->in.info->info2->path);
			}
			W_ERROR_HAVE_NO_MEMORY(info[i].value);
			all_string_sub((char *)info[i].value, "\\", "/", 0);

			i++;
		}

		if (r->in.info->info2->comment && r->in.info->info2->comment[0]) {
			info[i].name = SHARE_COMMENT;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, r->in.info->info2->comment);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}

		if (r->in.info->info2->password && r->in.info->info2->password[0]) {
			info[i].name = SHARE_PASSWORD;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, r->in.info->info2->password);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}

		info[i].name = SHARE_MAX_CONNECTIONS;
		info[i].type = SHARE_INFO_INT;
		info[i].value = talloc(info, int);
		*((int *)info[i].value) = r->in.info->info2->max_users;
		i++;

		/* TODO: security descriptor */

		nterr = share_create(sctx, r->in.info->info2->name, info, i);
		if (!NT_STATUS_IS_OK(nterr)) {
			return ntstatus_to_werror(nterr);
		}

		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		
		return WERR_OK;
	}
	case 501:
	{	
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		return WERR_NOT_SUPPORTED;
	}
	case 502:
	{
		NTSTATUS nterr;
		struct share_info *info;
		struct share_context *sctx;
		unsigned int count = 10;
		unsigned int i;

		nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
		if (!NT_STATUS_IS_OK(nterr)) {
			return ntstatus_to_werror(nterr);
		}

		/* there are no more than 10 options in struct srvsvc_NetShareInfo502 */
		info = talloc_array(mem_ctx, struct share_info, count);
		W_ERROR_HAVE_NO_MEMORY(info);

		i = 0;

		info[i].name = SHARE_TYPE;
		info[i].type = SHARE_INFO_STRING;
		switch (r->in.info->info502->type) {
		case 0x00:
			info[i].value = talloc_strdup(info, "DISK");
			break;
		case 0x01:
			info[i].value = talloc_strdup(info, "PRINTER");
			break;
		case 0x03:
			info[i].value = talloc_strdup(info, "IPC");
			break;
		default:
			return WERR_INVALID_PARAM;
		}
		W_ERROR_HAVE_NO_MEMORY(info[i].value);
		i++;

		if (r->in.info->info502->path && r->in.info->info502->path[0]) {
			info[i].name = SHARE_PATH;
			info[i].type = SHARE_INFO_STRING;

			/* Windows will send a path in a form of C:\example\path */
			if (r->in.info->info502->path[1] == ':') {
				info[i].value = talloc_strdup(info, &r->in.info->info502->path[2]);
			} else {
				/* very strange let's try to set as is */
				info[i].value = talloc_strdup(info, r->in.info->info502->path);
			}
			W_ERROR_HAVE_NO_MEMORY(info[i].value);
			all_string_sub((char *)info[i].value, "\\", "/", 0);

			i++;
		}

		if (r->in.info->info502->comment && r->in.info->info502->comment[0]) {
			info[i].name = SHARE_COMMENT;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, r->in.info->info502->comment);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}

		if (r->in.info->info502->password && r->in.info->info502->password[0]) {
			info[i].name = SHARE_PASSWORD;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, r->in.info->info502->password);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}

		info[i].name = SHARE_MAX_CONNECTIONS;
		info[i].type = SHARE_INFO_INT;
		info[i].value = talloc(info, int);
		*((int *)info[i].value) = r->in.info->info502->max_users;
		i++;

		/* TODO: security descriptor */

		nterr = share_create(sctx, r->in.info->info502->name, info, i);
		if (!NT_STATUS_IS_OK(nterr)) {
			return ntstatus_to_werror(nterr);
		}

		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}
		
		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}

static WERROR dcesrv_srvsvc_fiel_ShareInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct share_config *scfg, uint32_t level,
				    union srvsvc_NetShareInfo *info)
{
	struct dcesrv_context *dce_ctx = dce_call->conn->dce_ctx;

	switch (level) {
	case 0:
	{
		info->info0->name	= talloc_strdup(mem_ctx, scfg->name);
		W_ERROR_HAVE_NO_MEMORY(info->info0->name);

		return WERR_OK;
	}
	case 1:
	{
		info->info1->name	= talloc_strdup(mem_ctx, scfg->name);
		W_ERROR_HAVE_NO_MEMORY(info->info1->name);
		info->info1->type	= dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
		info->info1->comment	= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_COMMENT, ""));
		W_ERROR_HAVE_NO_MEMORY(info->info1->comment);

		return WERR_OK;
	}
	case 2:
	{
		info->info2->name		= talloc_strdup(mem_ctx, scfg->name);
		W_ERROR_HAVE_NO_MEMORY(info->info2->name);
		info->info2->type		= dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
		info->info2->comment		= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_COMMENT, ""));
		W_ERROR_HAVE_NO_MEMORY(info->info2->comment);
		info->info2->permissions 	= dcesrv_common_get_share_permissions(mem_ctx, dce_ctx, scfg);
		info->info2->max_users		= share_int_option(scfg, SHARE_MAX_CONNECTIONS, SHARE_MAX_CONNECTIONS_DEFAULT);
		info->info2->current_users	= dcesrv_common_get_share_current_users(mem_ctx, dce_ctx, scfg);
		info->info2->path		= dcesrv_common_get_share_path(mem_ctx, dce_ctx, scfg);
		W_ERROR_HAVE_NO_MEMORY(info->info2->path);
		info->info2->password		= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_PASSWORD, NULL));

		return WERR_OK;
	}
	case 501:
	{
		info->info501->name		= talloc_strdup(mem_ctx, scfg->name);
		W_ERROR_HAVE_NO_MEMORY(info->info501->name);
		info->info501->type		= dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
		info->info501->comment		= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_COMMENT, ""));
		W_ERROR_HAVE_NO_MEMORY(info->info501->comment);
		info->info501->csc_policy	= share_int_option(scfg, SHARE_CSC_POLICY, SHARE_CSC_POLICY_DEFAULT);

		return WERR_OK;
	}
	case 502:
	{
		info->info502->name		= talloc_strdup(mem_ctx, scfg->name);
		W_ERROR_HAVE_NO_MEMORY(info->info502->name);
		info->info502->type		= dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
		info->info502->comment		= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_COMMENT, ""));
		W_ERROR_HAVE_NO_MEMORY(info->info502->comment);
		info->info502->permissions	= dcesrv_common_get_share_permissions(mem_ctx, dce_ctx, scfg);
		info->info502->max_users	= share_int_option(scfg, SHARE_MAX_CONNECTIONS, SHARE_MAX_CONNECTIONS_DEFAULT);
		info->info502->current_users	= dcesrv_common_get_share_current_users(mem_ctx, dce_ctx, scfg);
		info->info502->path		= dcesrv_common_get_share_path(mem_ctx, dce_ctx, scfg);
		W_ERROR_HAVE_NO_MEMORY(info->info502->path);
		info->info502->password		= talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_PASSWORD, NULL));
		info->info502->sd_buf.sd	= dcesrv_common_get_security_descriptor(mem_ctx, dce_ctx, scfg);

		return WERR_OK;
	}
	case 1005:
	{
		info->info1005->dfs_flags	= dcesrv_common_get_share_dfs_flags(mem_ctx, dce_ctx, scfg);

		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}

/* 
  srvsvc_NetShareEnumAll
*/
static WERROR dcesrv_srvsvc_NetShareEnumAll(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct srvsvc_NetShareEnumAll *r)
{
	NTSTATUS nterr;
	int numshares = 0;
	const char **snames;
	struct share_context *sctx;
	struct share_config *scfg;

	*r->out.totalentries = 0;

	/* TODO: - paging of results 
	 */

	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	nterr = share_list_all(mem_ctx, sctx, &numshares, &snames);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	switch (r->in.info_ctr->level) {
	case 0:
	{
		unsigned int i;
		struct srvsvc_NetShareCtr0 *ctr0;

		ctr0 = talloc(mem_ctx, struct srvsvc_NetShareCtr0);
		W_ERROR_HAVE_NO_MEMORY(ctr0);

		ctr0->count = numshares;
		ctr0->array = NULL;

		if (ctr0->count == 0) {
			r->out.info_ctr->ctr.ctr0	= ctr0;
			return WERR_OK;
		}

		ctr0->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo0, ctr0->count);
		W_ERROR_HAVE_NO_MEMORY(ctr0->array);

		for (i = 0; i < ctr0->count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			info.info0 = &ctr0->array[i];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			if (!W_ERROR_IS_OK(status)) {
				return status;
			}
			talloc_free(scfg);
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr0	= ctr0;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr0->count;
		return WERR_OK;
	}
	case 1:
	{
		unsigned int i;
		struct srvsvc_NetShareCtr1 *ctr1;

		ctr1 = talloc(mem_ctx, struct srvsvc_NetShareCtr1);
		W_ERROR_HAVE_NO_MEMORY(ctr1);

		ctr1->count = numshares;
		ctr1->array = NULL;

		if (ctr1->count == 0) {
			r->out.info_ctr->ctr.ctr1	= ctr1;
			return WERR_OK;
		}

		ctr1->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo1, ctr1->count);
		W_ERROR_HAVE_NO_MEMORY(ctr1->array);

		for (i=0; i < ctr1->count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			info.info1 = &ctr1->array[i];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			if (!W_ERROR_IS_OK(status)) {
				return status;
			}
			talloc_free(scfg);
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr1	= ctr1;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr1->count;

		return WERR_OK;
	}
	case 2:
	{
		unsigned int i;
		struct srvsvc_NetShareCtr2 *ctr2;

		SRVSVC_CHECK_ADMIN_ACCESS;

		ctr2 = talloc(mem_ctx, struct srvsvc_NetShareCtr2);
		W_ERROR_HAVE_NO_MEMORY(ctr2);

		ctr2->count = numshares;
		ctr2->array = NULL;

		if (ctr2->count == 0) {
			r->out.info_ctr->ctr.ctr2	= ctr2;
			return WERR_OK;
		}

		ctr2->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo2, ctr2->count);
		W_ERROR_HAVE_NO_MEMORY(ctr2->array);

		for (i=0; i < ctr2->count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			info.info2 = &ctr2->array[i];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			if (!W_ERROR_IS_OK(status)) {
				return status;
			}
			talloc_free(scfg);
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr2	= ctr2;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr2->count;

		return WERR_OK;
	}
	case 501:
	{
		unsigned int i;
		struct srvsvc_NetShareCtr501 *ctr501;

		SRVSVC_CHECK_ADMIN_ACCESS;

		ctr501 = talloc(mem_ctx, struct srvsvc_NetShareCtr501);
		W_ERROR_HAVE_NO_MEMORY(ctr501);

		ctr501->count = numshares;
		ctr501->array = NULL;

		if (ctr501->count == 0) {
			r->out.info_ctr->ctr.ctr501	= ctr501;
			return WERR_OK;
		}

		ctr501->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo501, ctr501->count);
		W_ERROR_HAVE_NO_MEMORY(ctr501->array);

		for (i=0; i < ctr501->count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			info.info501 = &ctr501->array[i];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			if (!W_ERROR_IS_OK(status)) {
				return status;
			}
			talloc_free(scfg);
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr501	= ctr501;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr501->count;

		return WERR_OK;
	}
	case 502:
	{
		unsigned int i;
		struct srvsvc_NetShareCtr502 *ctr502;

		SRVSVC_CHECK_ADMIN_ACCESS;

		ctr502 = talloc(mem_ctx, struct srvsvc_NetShareCtr502);
		W_ERROR_HAVE_NO_MEMORY(ctr502);

		ctr502->count = numshares;
		ctr502->array = NULL;

		if (ctr502->count == 0) {
			r->out.info_ctr->ctr.ctr502	= ctr502;
			return WERR_OK;
		}

		ctr502->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo502, ctr502->count);
		W_ERROR_HAVE_NO_MEMORY(ctr502->array);

		for (i=0; i < ctr502->count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			info.info502 = &ctr502->array[i];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			if (!W_ERROR_IS_OK(status)) {
				return status;
			}
			talloc_free(scfg);
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr502	= ctr502;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr502->count;

		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetShareGetInfo 
*/
static WERROR dcesrv_srvsvc_NetShareGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct srvsvc_NetShareGetInfo *r)
{
	NTSTATUS nterr;
	struct share_context *sctx = NULL;
	struct share_config *scfg = NULL;

	ZERO_STRUCTP(r->out.info);

	/* TODO: - access check
	 */

	if (strcmp("", r->in.share_name) == 0) {
		return WERR_INVALID_PARAM;
	}

	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	nterr = share_get_config(mem_ctx, sctx, r->in.share_name, &scfg);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	switch (r->in.level) {
	case 0:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		info.info0 = talloc(mem_ctx, struct srvsvc_NetShareInfo0);
		W_ERROR_HAVE_NO_MEMORY(info.info0);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info0 = info.info0;
		return WERR_OK;
	}
	case 1:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		info.info1 = talloc(mem_ctx, struct srvsvc_NetShareInfo1);
		W_ERROR_HAVE_NO_MEMORY(info.info1);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info1 = info.info1;
		return WERR_OK;
	}
	case 2:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		SRVSVC_CHECK_ADMIN_ACCESS;

		info.info2 = talloc(mem_ctx, struct srvsvc_NetShareInfo2);
		W_ERROR_HAVE_NO_MEMORY(info.info2);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info2 = info.info2;
		return WERR_OK;
	}
	case 501:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		info.info501 = talloc(mem_ctx, struct srvsvc_NetShareInfo501);
		W_ERROR_HAVE_NO_MEMORY(info.info501);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info501 = info.info501;
		return WERR_OK;
	}
	case 502:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		SRVSVC_CHECK_ADMIN_ACCESS;

		info.info502 = talloc(mem_ctx, struct srvsvc_NetShareInfo502);
		W_ERROR_HAVE_NO_MEMORY(info.info502);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info502 = info.info502;
		return WERR_OK;
	}
	case 1005:
	{
		WERROR status;
		union srvsvc_NetShareInfo info;

		info.info1005 = talloc(mem_ctx, struct srvsvc_NetShareInfo1005);
		W_ERROR_HAVE_NO_MEMORY(info.info1005);

		status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.level, &info);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

		r->out.info->info1005 = info.info1005;
		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}

static WERROR dcesrv_srvsvc_fill_share_info(struct share_info *info, int *count,
					const char *share_name, int level,
					const char *name,
					const char *path,
					const char *comment,
					const char *password,
					enum srvsvc_ShareType type,
					int32_t max_users,
					uint32_t csc_policy,
					struct security_descriptor *sd)
{
	unsigned int i = 0;

	if (level == 501) {
		info[i].name = SHARE_CSC_POLICY;
		info[i].type = SHARE_INFO_INT;
		info[i].value = talloc(info, int);
		*((int *)info[i].value) = csc_policy;
		i++;
	}
	
	switch(level) {

	case 502:
		/* TODO: check if unknown is csc_policy */

		/* TODO: security descriptor */

	case 2:
		if (path && path[0]) {
			info[i].name = SHARE_PATH;
			info[i].type = SHARE_INFO_STRING;

			/* Windows will send a path in a form of C:\example\path */
			if (path[1] == ':') {
				info[i].value = talloc_strdup(info, &path[2]);
			} else {
				/* very strange let's try to set as is */
				info[i].value = talloc_strdup(info, path);
			}
			W_ERROR_HAVE_NO_MEMORY(info[i].value);
			all_string_sub((char *)info[i].value, "\\", "/", 0);

			i++;
		}

		if (password && password[0]) {
			info[i].name = SHARE_PASSWORD;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, password);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}

		info[i].name = SHARE_MAX_CONNECTIONS;
		info[i].type = SHARE_INFO_INT;
		info[i].value = talloc(info, int);
		*((int *)info[i].value) = max_users;
		i++;

	case 501:
	case 1:
		info[i].name = SHARE_TYPE;
		info[i].type = SHARE_INFO_STRING;
		switch (type) {
		case 0x00:
			info[i].value = talloc_strdup(info, "DISK");
			break;
		case 0x01:
			info[i].value = talloc_strdup(info, "PRINTER");
			break;
		case 0x03:
			info[i].value = talloc_strdup(info, "IPC");
			break;
		default:
			return WERR_INVALID_PARAM;
		}
		W_ERROR_HAVE_NO_MEMORY(info[i].value);
		i++;

	case 1004:
		if (comment) {
			info[i].name = SHARE_COMMENT;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, comment);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);

			i++;
		}
	case 0:
		if (name &&
		    strcasecmp(share_name, name) != 0) {
			info[i].name = SHARE_NAME;
			info[i].type = SHARE_INFO_STRING;
			info[i].value = talloc_strdup(info, name);
			W_ERROR_HAVE_NO_MEMORY(info[i].value);
			i++;
		}

		break;

	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*count = i;

	return WERR_OK;
}

/* 
  srvsvc_NetShareSetInfo 
*/
static WERROR dcesrv_srvsvc_NetShareSetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareSetInfo *r)
{
	NTSTATUS nterr;
	WERROR status;
	struct share_context *sctx = NULL;
	struct share_info *info;
	int count;

	/* TODO: - access check
	 */

	/* there are no more than 10 options in all struct srvsvc_NetShareInfoXXX */
	info = talloc_array(mem_ctx, struct share_info, 10);
	W_ERROR_HAVE_NO_MEMORY(info);

	if (strcmp("", r->in.share_name) == 0) {
		return WERR_INVALID_PARAM;
	}

	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	switch (r->in.level) {
	case 0:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					r->in.info->info0->name,
					NULL,
					NULL,
					NULL,
					0,
					0,
					0,
					NULL);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 1:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					r->in.info->info1->name,
					NULL,
					r->in.info->info1->comment,
					NULL,
					r->in.info->info1->type,
					0,
					0,
					NULL);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 2:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					r->in.info->info2->name,
					r->in.info->info2->path,
					r->in.info->info2->comment,
					r->in.info->info2->password,
					r->in.info->info2->type,
					r->in.info->info2->max_users,
					0,
					NULL);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 501:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					r->in.info->info501->name,
					NULL,
					r->in.info->info501->comment,
					NULL,
					r->in.info->info501->type,
					0,
					r->in.info->info501->csc_policy,
					NULL);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 502:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					r->in.info->info502->name,
					r->in.info->info502->path,
					r->in.info->info502->comment,
					r->in.info->info502->password,
					r->in.info->info502->type,
					r->in.info->info502->max_users,
					0,
					r->in.info->info502->sd_buf.sd);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 1004:
	{
		status = dcesrv_srvsvc_fill_share_info(info, &count,
					r->in.share_name, r->in.level,
					NULL,
					NULL,
					r->in.info->info1004->comment,
					NULL,
					0,
					0,
					0,
					NULL);
		if (!W_ERROR_EQUAL(status, WERR_OK)) {
			return status;
		}
		break;
	}
	case 1005:
	{
		/* r->in.info.dfs_flags; */
		
		if (r->in.parm_error) {
			r->out.parm_error = r->in.parm_error;
		}

		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	nterr = share_set(sctx, r->in.share_name, info, count);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	if (r->in.parm_error) {
		r->out.parm_error = r->in.parm_error;
	}
		
	return WERR_OK;
}


/* 
  srvsvc_NetShareDelSticky 
*/
static WERROR dcesrv_srvsvc_NetShareDelSticky(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareDelSticky *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetShareCheck 
*/
static WERROR dcesrv_srvsvc_NetShareCheck(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareCheck *r)
{
	NTSTATUS nterr;
	struct share_context *sctx = NULL;
	struct share_config *scfg = NULL;
	char *device;
	const char **names;
	int count;
	int i;

	*r->out.type = 0;

	/* TODO: - access check
	 */

	if (strcmp("", r->in.device_name) == 0) {
		*r->out.type = STYPE_IPC;
		return WERR_OK;
	}

	/* copy the path skipping C:\ */
	if (strncasecmp(r->in.device_name, "C:", 2) == 0) {
		device = talloc_strdup(mem_ctx, &r->in.device_name[2]);
	} else {
		/* no chance we have a share that doesn't start with C:\ */
		return WERR_DEVICE_NOT_SHARED;
	}
	all_string_sub(device, "\\", "/", 0);

	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	nterr = share_list_all(mem_ctx, sctx, &count, &names);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	for (i = 0; i < count; i++) {
		const char *path;
		const char *type;

		nterr = share_get_config(mem_ctx, sctx, names[i], &scfg);
		if (!NT_STATUS_IS_OK(nterr)) {
			return ntstatus_to_werror(nterr);
		}
		path = share_string_option(scfg, SHARE_PATH, NULL);
		if (!path) continue;

		if (strcmp(device, path) == 0) {		
			type = share_string_option(scfg, SHARE_TYPE, NULL);
			if (!type) continue;

			if (strcmp(type, "DISK") == 0) {
				*r->out.type = STYPE_DISKTREE;
				return WERR_OK;
			}

			if (strcmp(type, "IPC") == 0) {
				*r->out.type = STYPE_IPC;
				return WERR_OK;
			}

			if (strcmp(type, "PRINTER") == 0) {
				*r->out.type = STYPE_PRINTQ;
				return WERR_OK;
			}
		}
	}

	return WERR_DEVICE_NOT_SHARED;
}


/* 
  srvsvc_NetSrvGetInfo 
*/
static WERROR dcesrv_srvsvc_NetSrvGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSrvGetInfo *r)
{
	struct dcesrv_context *dce_ctx = dce_call->conn->dce_ctx;
	struct dcerpc_server_info *server_info = lpcfg_dcerpc_server_info(mem_ctx, dce_ctx->lp_ctx);

	ZERO_STRUCTP(r->out.info);

	switch (r->in.level) {
	case 100:
	{
		struct srvsvc_NetSrvInfo100 *info100;

		info100 = talloc(mem_ctx, struct srvsvc_NetSrvInfo100);
		W_ERROR_HAVE_NO_MEMORY(info100);

		info100->platform_id	= dcesrv_common_get_platform_id(mem_ctx, dce_ctx);
		info100->server_name	= dcesrv_common_get_server_name(mem_ctx, dce_ctx, r->in.server_unc);
		W_ERROR_HAVE_NO_MEMORY(info100->server_name);

		r->out.info->info100 = info100;
		return WERR_OK;
	}
	case 101:
	{
		struct srvsvc_NetSrvInfo101 *info101;

		info101 = talloc(mem_ctx, struct srvsvc_NetSrvInfo101);
		W_ERROR_HAVE_NO_MEMORY(info101);

		info101->platform_id	= dcesrv_common_get_platform_id(mem_ctx, dce_ctx);
		info101->server_name	= dcesrv_common_get_server_name(mem_ctx, dce_ctx, r->in.server_unc);
		W_ERROR_HAVE_NO_MEMORY(info101->server_name);

		info101->version_major	= server_info->version_major;
		info101->version_minor	= server_info->version_minor;
		info101->server_type	= dcesrv_common_get_server_type(mem_ctx, dce_call->event_ctx, dce_ctx);
		info101->comment	= talloc_strdup(mem_ctx, lpcfg_serverstring(dce_ctx->lp_ctx));
		W_ERROR_HAVE_NO_MEMORY(info101->comment);

		r->out.info->info101 = info101;
		return WERR_OK;
	}
	case 102:
	{
		struct srvsvc_NetSrvInfo102 *info102;

		info102 = talloc(mem_ctx, struct srvsvc_NetSrvInfo102);
		W_ERROR_HAVE_NO_MEMORY(info102);

		info102->platform_id	= dcesrv_common_get_platform_id(mem_ctx, dce_ctx);
		info102->server_name	= dcesrv_common_get_server_name(mem_ctx, dce_ctx, r->in.server_unc);
		W_ERROR_HAVE_NO_MEMORY(info102->server_name);

		info102->version_major	= server_info->version_major;
		info102->version_minor	= server_info->version_minor;
		info102->server_type	= dcesrv_common_get_server_type(mem_ctx, dce_call->event_ctx, dce_ctx);
		info102->comment	= talloc_strdup(mem_ctx, lpcfg_serverstring(dce_ctx->lp_ctx));
		W_ERROR_HAVE_NO_MEMORY(info102->comment);

		info102->users		= dcesrv_common_get_users(mem_ctx, dce_ctx);
		info102->disc		= dcesrv_common_get_disc(mem_ctx, dce_ctx);
		info102->hidden		= dcesrv_common_get_hidden(mem_ctx, dce_ctx);
		info102->announce	= dcesrv_common_get_announce(mem_ctx, dce_ctx);
		info102->anndelta	= dcesrv_common_get_anndelta(mem_ctx, dce_ctx);
		info102->licenses	= dcesrv_common_get_licenses(mem_ctx, dce_ctx);
		info102->userpath	= dcesrv_common_get_userpath(mem_ctx, dce_ctx);
		W_ERROR_HAVE_NO_MEMORY(info102->userpath);

		r->out.info->info102 = info102;
		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetSrvSetInfo 
*/
static WERROR dcesrv_srvsvc_NetSrvSetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSrvSetInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetDiskEnum 
*/
static WERROR dcesrv_srvsvc_NetDiskEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetDiskEnum *r)
{
	r->out.info->disks = NULL;
	r->out.info->count = 0;
	*r->out.totalentries = 0;

	switch (r->in.level) {
	case 0:
	{
		/* we can safely hardcode the reply and report we have only one disk (C:) */
		/* for some reason Windows wants 2 entries with the second being empty */
		r->out.info->disks = talloc_array(mem_ctx, struct srvsvc_NetDiskInfo0, 2);
		W_ERROR_HAVE_NO_MEMORY(r->out.info->disks);
		r->out.info->count = 2;

		r->out.info->disks[0].disk = talloc_strdup(mem_ctx, "C:");
		W_ERROR_HAVE_NO_MEMORY(r->out.info->disks[0].disk);

		r->out.info->disks[1].disk = talloc_strdup(mem_ctx, "");
		W_ERROR_HAVE_NO_MEMORY(r->out.info->disks[1].disk);

		*r->out.totalentries = 1;
		r->out.resume_handle = r->in.resume_handle;

		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetServerStatisticsGet 
*/
static WERROR dcesrv_srvsvc_NetServerStatisticsGet(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetServerStatisticsGet *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetTransportAdd 
*/
static WERROR dcesrv_srvsvc_NetTransportAdd(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetTransportAdd *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetTransportEnum 
*/
static WERROR dcesrv_srvsvc_NetTransportEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetTransportEnum *r)
{
	r->out.transports->level = r->in.transports->level;
	*r->out.totalentries = 0;
	if (r->out.resume_handle) {
		*r->out.resume_handle = 0;
	}

	switch (r->in.transports->level) {
	case 0:
	{
		r->out.transports->ctr.ctr0 = talloc(mem_ctx, struct srvsvc_NetTransportCtr0);
		W_ERROR_HAVE_NO_MEMORY(r->out.transports->ctr.ctr0);

		r->out.transports->ctr.ctr0->count = 0;
		r->out.transports->ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 1:
	{
		r->out.transports->ctr.ctr1 = talloc(mem_ctx, struct srvsvc_NetTransportCtr1);
		W_ERROR_HAVE_NO_MEMORY(r->out.transports->ctr.ctr1);

		r->out.transports->ctr.ctr1->count = 0;
		r->out.transports->ctr.ctr1->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 2:
	{
		r->out.transports->ctr.ctr2 = talloc(mem_ctx, struct srvsvc_NetTransportCtr2);
		W_ERROR_HAVE_NO_MEMORY(r->out.transports->ctr.ctr2);

		r->out.transports->ctr.ctr2->count = 0;
		r->out.transports->ctr.ctr2->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	case 3:
	{
		r->out.transports->ctr.ctr3 = talloc(mem_ctx, struct srvsvc_NetTransportCtr3);
		W_ERROR_HAVE_NO_MEMORY(r->out.transports->ctr.ctr3);

		r->out.transports->ctr.ctr3->count = 0;
		r->out.transports->ctr.ctr3->array = NULL;

		return WERR_NOT_SUPPORTED;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}

/* 
  srvsvc_NetTransportDel 
*/
static WERROR dcesrv_srvsvc_NetTransportDel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetTransportDel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetRemoteTOD 
*/
static WERROR dcesrv_srvsvc_NetRemoteTOD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetRemoteTOD *r)
{
	struct timeval tval;
	time_t t;
	struct tm tm;
	struct srvsvc_NetRemoteTODInfo *info;

	info = talloc(mem_ctx, struct srvsvc_NetRemoteTODInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	GetTimeOfDay(&tval);
	t = tval.tv_sec;

	gmtime_r(&t, &tm);

	info->elapsed	= t;
	/* TODO: fake the uptime: just return the milliseconds till 0:00:00 today */
	info->msecs	= (tm.tm_hour*60*60*1000)
			+ (tm.tm_min*60*1000)
			+ (tm.tm_sec*1000)
			+ (tval.tv_usec/1000);
	info->hours	= tm.tm_hour;
	info->mins	= tm.tm_min;
	info->secs	= tm.tm_sec;
	info->hunds	= tval.tv_usec/10000;
	info->timezone	= get_time_zone(t)/60;
	info->tinterval	= 310; /* just return the same as windows */
	info->day	= tm.tm_mday;
	info->month	= tm.tm_mon + 1;
	info->year	= tm.tm_year + 1900;
	info->weekday	= tm.tm_wday;

	*r->out.info = info;

	return WERR_OK;
}

/* 
  srvsvc_NetPathType 
*/
static WERROR dcesrv_srvsvc_NetPathType(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetPathType *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetPathCanonicalize 
*/
static WERROR dcesrv_srvsvc_NetPathCanonicalize(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetPathCanonicalize *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetPathCompare 
*/
static WERROR dcesrv_srvsvc_NetPathCompare(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetPathCompare *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetNameValidate 
*/
static WERROR dcesrv_srvsvc_NetNameValidate(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetNameValidate *r)
{
	int len;

	if ((r->in.flags != 0x0) && (r->in.flags != 0x80000000)) {
		return WERR_INVALID_NAME;
	}

	switch (r->in.name_type) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		return WERR_NOT_SUPPORTED;

	case 9: /* validate share name */

		len = strlen_m(r->in.name);
		if ((r->in.flags == 0x0) && (len > 81)) {
			return WERR_INVALID_NAME;
		}
		if ((r->in.flags == 0x80000000) && (len > 13)) {
			return WERR_INVALID_NAME;
		}
		if (! dcesrv_common_validate_share_name(mem_ctx, r->in.name)) {
			return WERR_INVALID_NAME;
		}
		return WERR_OK;

	case 10:
	case 11:
	case 12:
	case 13:
		return WERR_NOT_SUPPORTED;
	default:
		return WERR_INVALID_PARAM;
	}
}


/* 
  srvsvc_NetPRNameCompare 
*/
static WERROR dcesrv_srvsvc_NetPRNameCompare(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetPRNameCompare *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetShareEnum 
*/
static WERROR dcesrv_srvsvc_NetShareEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareEnum *r)
{
	NTSTATUS nterr;
	int numshares = 0;
	const char **snames;
	struct share_context *sctx;
	struct share_config *scfg;
	struct dcesrv_context *dce_ctx = dce_call->conn->dce_ctx;

	*r->out.totalentries = 0;

	/* TODO: - paging of results 
	 */

	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	nterr = share_list_all(mem_ctx, sctx, &numshares, &snames);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	switch (r->in.info_ctr->level) {
	case 0:
	{
		unsigned int i, y = 0;
		unsigned int count;
		struct srvsvc_NetShareCtr0 *ctr0;

		ctr0 = talloc(mem_ctx, struct srvsvc_NetShareCtr0);
		W_ERROR_HAVE_NO_MEMORY(ctr0);

		count = numshares;
		ctr0->count = count;
		ctr0->array = NULL;

		if (ctr0->count == 0) {
			r->out.info_ctr->ctr.ctr0	= ctr0;
			return WERR_OK;
		}

		ctr0->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo0, count);
		W_ERROR_HAVE_NO_MEMORY(ctr0->array);

		for (i=0; i < count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;
			enum srvsvc_ShareType type;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}
			
			type = dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
			if (type & STYPE_HIDDEN) {
				ctr0->count--;
				talloc_free(scfg);
				continue;
			}

			info.info0 = &ctr0->array[y];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			W_ERROR_NOT_OK_RETURN(status);
			talloc_free(scfg);
			y++;
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr0	= ctr0;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr0->count;

		return WERR_OK;
	}
	case 1:
	{
		unsigned int i, y = 0;
		unsigned int count;
		struct srvsvc_NetShareCtr1 *ctr1;

		ctr1 = talloc(mem_ctx, struct srvsvc_NetShareCtr1);
		W_ERROR_HAVE_NO_MEMORY(ctr1);

		count = numshares;
		ctr1->count = count;
		ctr1->array = NULL;

		if (ctr1->count == 0) {
			r->out.info_ctr->ctr.ctr1	= ctr1;
			return WERR_OK;
		}

		ctr1->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo1, count);
		W_ERROR_HAVE_NO_MEMORY(ctr1->array);

		for (i=0; i < count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;
			enum srvsvc_ShareType type;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}

			type = dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
			if (type & STYPE_HIDDEN) {
				ctr1->count--;
				talloc_free(scfg);
				continue;
			}

			info.info1 = &ctr1->array[y];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			W_ERROR_NOT_OK_RETURN(status);
			talloc_free(scfg);
			y++;
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr1	= ctr1;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr1->count;

		return WERR_OK;
	}
	case 2:
	{
		unsigned int i, y = 0;
		unsigned int count;
		struct srvsvc_NetShareCtr2 *ctr2;

		SRVSVC_CHECK_ADMIN_ACCESS;

		ctr2 = talloc(mem_ctx, struct srvsvc_NetShareCtr2);
		W_ERROR_HAVE_NO_MEMORY(ctr2);

		count = numshares;
		ctr2->count = count;
		ctr2->array = NULL;

		if (ctr2->count == 0) {
			r->out.info_ctr->ctr.ctr2	= ctr2;
			return WERR_OK;
		}

		ctr2->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo2, count);
		W_ERROR_HAVE_NO_MEMORY(ctr2->array);

		for (i=0; i < count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;
			enum srvsvc_ShareType type;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}

			type = dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
			if (type & STYPE_HIDDEN) {
				ctr2->count--;
				talloc_free(scfg);
				continue;
			}

			info.info2 = &ctr2->array[y];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			W_ERROR_NOT_OK_RETURN(status);
			talloc_free(scfg);
			y++;
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr2	= ctr2;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr2->count;

		return WERR_OK;
	}
	case 502:
	{
		unsigned int i, y = 0;
		unsigned int count;
		struct srvsvc_NetShareCtr502 *ctr502;

		SRVSVC_CHECK_ADMIN_ACCESS;

		ctr502 = talloc(mem_ctx, struct srvsvc_NetShareCtr502);
		W_ERROR_HAVE_NO_MEMORY(ctr502);

		count = numshares;
		ctr502->count = count;
		ctr502->array = NULL;

		if (ctr502->count == 0) {
			r->out.info_ctr->ctr.ctr502	= ctr502;
			return WERR_OK;
		}

		ctr502->array = talloc_array(mem_ctx, struct srvsvc_NetShareInfo502, count);
		W_ERROR_HAVE_NO_MEMORY(ctr502->array);

		for (i=0; i < count; i++) {
			WERROR status;
			union srvsvc_NetShareInfo info;
			enum srvsvc_ShareType type;

			nterr = share_get_config(mem_ctx, sctx, snames[i], &scfg);
			if (!NT_STATUS_IS_OK(nterr)) {
				DEBUG(1, ("ERROR: Service [%s] disappeared after enumeration", snames[i]));
				return WERR_GENERAL_FAILURE;
			}

		       	type = dcesrv_common_get_share_type(mem_ctx, dce_ctx, scfg);
			if (type & STYPE_HIDDEN) {
				ctr502->count--;
				talloc_free(scfg);
				continue;
			}

			info.info502 = &ctr502->array[y];
			status = dcesrv_srvsvc_fiel_ShareInfo(dce_call, mem_ctx, scfg, r->in.info_ctr->level, &info);
			W_ERROR_NOT_OK_RETURN(status);
			talloc_free(scfg);
			y++;
		}
		talloc_free(snames);

		r->out.info_ctr->ctr.ctr502	= ctr502;
		*r->out.totalentries		= r->out.info_ctr->ctr.ctr502->count;

		return WERR_OK;
	}
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/* 
  srvsvc_NetShareDelStart 
*/
static WERROR dcesrv_srvsvc_NetShareDelStart(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareDelStart *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetShareDelCommit 
*/
static WERROR dcesrv_srvsvc_NetShareDelCommit(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareDelCommit *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetGetFileSecurity 
*/
static WERROR dcesrv_srvsvc_NetGetFileSecurity(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetGetFileSecurity *r)
{
	struct sec_desc_buf *sd_buf;
	struct ntvfs_context *ntvfs_ctx = NULL;
	struct ntvfs_request *ntvfs_req;
	union smb_fileinfo *io;
	NTSTATUS nt_status;

	nt_status = srvsvc_create_ntvfs_context(dce_call, mem_ctx, r->in.share, &ntvfs_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) return ntstatus_to_werror(nt_status);

	ntvfs_req = ntvfs_request_create(ntvfs_ctx, mem_ctx,
					 dce_call->conn->auth_state.session_info,
					 0,
					 dce_call->time,
					 NULL, NULL, 0);
	W_ERROR_HAVE_NO_MEMORY(ntvfs_req);

	sd_buf = talloc(mem_ctx, struct sec_desc_buf);
	W_ERROR_HAVE_NO_MEMORY(sd_buf);

	io = talloc(mem_ctx, union smb_fileinfo);
	W_ERROR_HAVE_NO_MEMORY(io);

	io->query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
	io->query_secdesc.in.file.path		= r->in.file;
	io->query_secdesc.in.secinfo_flags	= r->in.securityinformation;

	nt_status = ntvfs_qpathinfo(ntvfs_req, io);
	if (!NT_STATUS_IS_OK(nt_status)) return ntstatus_to_werror(nt_status);

	sd_buf->sd = io->query_secdesc.out.sd;

	*r->out.sd_buf = sd_buf;
	return WERR_OK;
}


/* 
  srvsvc_NetSetFileSecurity 
*/
static WERROR dcesrv_srvsvc_NetSetFileSecurity(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSetFileSecurity *r)
{
	struct ntvfs_context *ntvfs_ctx;
	struct ntvfs_request *ntvfs_req;
	union smb_setfileinfo *io;
	NTSTATUS nt_status;

	nt_status = srvsvc_create_ntvfs_context(dce_call, mem_ctx, r->in.share, &ntvfs_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) return ntstatus_to_werror(nt_status);

	ntvfs_req = ntvfs_request_create(ntvfs_ctx, mem_ctx,
					 dce_call->conn->auth_state.session_info,
					 0,
					 dce_call->time,
					 NULL, NULL, 0);
	W_ERROR_HAVE_NO_MEMORY(ntvfs_req);

	io = talloc(mem_ctx, union smb_setfileinfo);
	W_ERROR_HAVE_NO_MEMORY(io);

	io->set_secdesc.level			= RAW_FILEINFO_SEC_DESC;
	io->set_secdesc.in.file.path		= r->in.file;
	io->set_secdesc.in.secinfo_flags	= r->in.securityinformation;
	io->set_secdesc.in.sd			= r->in.sd_buf->sd;

	nt_status = ntvfs_setpathinfo(ntvfs_req, io);
	if (!NT_STATUS_IS_OK(nt_status)) return ntstatus_to_werror(nt_status);

	return WERR_OK;
}


/* 
  srvsvc_NetServerTransportAddEx 
*/
static WERROR dcesrv_srvsvc_NetServerTransportAddEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetServerTransportAddEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NetServerSetServiceBitsEx 
*/
static WERROR dcesrv_srvsvc_NetServerSetServiceBitsEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetServerSetServiceBitsEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSGETVERSION 
*/
static WERROR dcesrv_srvsvc_NETRDFSGETVERSION(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSGETVERSION *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSCREATELOCALPARTITION 
*/
static WERROR dcesrv_srvsvc_NETRDFSCREATELOCALPARTITION(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSCREATELOCALPARTITION *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSDELETELOCALPARTITION 
*/
static WERROR dcesrv_srvsvc_NETRDFSDELETELOCALPARTITION(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSDELETELOCALPARTITION *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSSETLOCALVOLUMESTATE 
*/
static WERROR dcesrv_srvsvc_NETRDFSSETLOCALVOLUMESTATE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSSETLOCALVOLUMESTATE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSSETSERVERINFO 
*/
static WERROR dcesrv_srvsvc_NETRDFSSETSERVERINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSSETSERVERINFO *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSCREATEEXITPOINT 
*/
static WERROR dcesrv_srvsvc_NETRDFSCREATEEXITPOINT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSCREATEEXITPOINT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSDELETEEXITPOINT 
*/
static WERROR dcesrv_srvsvc_NETRDFSDELETEEXITPOINT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSDELETEEXITPOINT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSMODIFYPREFIX 
*/
static WERROR dcesrv_srvsvc_NETRDFSMODIFYPREFIX(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSMODIFYPREFIX *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSFIXLOCALVOLUME 
*/
static WERROR dcesrv_srvsvc_NETRDFSFIXLOCALVOLUME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSFIXLOCALVOLUME *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRDFSMANAGERREPORTSITEINFO 
*/
static WERROR dcesrv_srvsvc_NETRDFSMANAGERREPORTSITEINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRDFSMANAGERREPORTSITEINFO *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  srvsvc_NETRSERVERTRANSPORTDELEX 
*/
static WERROR dcesrv_srvsvc_NETRSERVERTRANSPORTDELEX(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRSERVERTRANSPORTDELEX *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  srvsvc_NetShareDel 
*/
static WERROR dcesrv_srvsvc_NetShareDel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetShareDel *r)
{
	NTSTATUS nterr;
	struct share_context *sctx;
		
	nterr = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}
			
	nterr = share_remove(sctx, r->in.share_name);
	if (!NT_STATUS_IS_OK(nterr)) {
		return ntstatus_to_werror(nterr);
	}

	return WERR_OK;
}

/* 
  srvsvc_NetSetServiceBits 
*/
static WERROR dcesrv_srvsvc_NetSetServiceBits(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NetSetServiceBits *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  srvsvc_NETRPRNAMECANONICALIZE 
*/
static WERROR dcesrv_srvsvc_NETRPRNAMECANONICALIZE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct srvsvc_NETRPRNAMECANONICALIZE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_srvsvc_s.c"
