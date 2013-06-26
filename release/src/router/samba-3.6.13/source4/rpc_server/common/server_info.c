/* 
   Unix SMB/CIFS implementation.

   common server info functions

   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "librpc/gen_ndr/srvsvc.h"
#include "rpc_server/dcerpc_server.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "param/param.h"

/* 
    Here are common server info functions used by some dcerpc server interfaces
*/

/* This hardcoded value should go into a ldb database! */
enum srvsvc_PlatformId dcesrv_common_get_platform_id(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	enum srvsvc_PlatformId id;

	id = lpcfg_parm_int(dce_ctx->lp_ctx, NULL, "server_info", "platform_id", PLATFORM_ID_NT);

	return id;
}

const char *dcesrv_common_get_server_name(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, const char *server_unc)
{
	const char *p = server_unc;

	/* if there's no string return our NETBIOS name */
	if (!p) {
		return talloc_strdup(mem_ctx, lpcfg_netbios_name(dce_ctx->lp_ctx));
	}

	/* if there're '\\\\' in front remove them otherwise just pass the string */
	if (p[0] == '\\' && p[1] == '\\') {
		p += 2;
	}

	return talloc_strdup(mem_ctx, p);
}


/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_server_type(TALLOC_CTX *mem_ctx, struct tevent_context *event_ctx, struct dcesrv_context *dce_ctx)
{
	int default_server_announce = 0;
	default_server_announce |= SV_TYPE_WORKSTATION;
	default_server_announce |= SV_TYPE_SERVER;
	default_server_announce |= SV_TYPE_SERVER_UNIX;

	switch (lpcfg_announce_as(dce_ctx->lp_ctx)) {
		case ANNOUNCE_AS_NT_SERVER:
			default_server_announce |= SV_TYPE_SERVER_NT;
			/* fall through... */
		case ANNOUNCE_AS_NT_WORKSTATION:
			default_server_announce |= SV_TYPE_NT;
			break;
		case ANNOUNCE_AS_WIN95:
			default_server_announce |= SV_TYPE_WIN95_PLUS;
			break;
		case ANNOUNCE_AS_WFW:
			default_server_announce |= SV_TYPE_WFW;
			break;
		default:
			break;
	}

	switch (lpcfg_server_role(dce_ctx->lp_ctx)) {
		case ROLE_DOMAIN_MEMBER:
			default_server_announce |= SV_TYPE_DOMAIN_MEMBER;
			break;
		case ROLE_DOMAIN_CONTROLLER:
		{
			struct ldb_context *samctx;
			TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
			if (!tmp_ctx) {
				break;
			}
			/* open main ldb */
			samctx = samdb_connect(tmp_ctx, event_ctx, dce_ctx->lp_ctx, anonymous_session(tmp_ctx, dce_ctx->lp_ctx), 0);
			if (samctx == NULL) {
				DEBUG(2,("Unable to open samdb in determining server announce flags\n"));
			} else {
				/* Determine if we are the pdc */
				bool is_pdc = samdb_is_pdc(samctx);
				if (is_pdc) {
					default_server_announce |= SV_TYPE_DOMAIN_CTRL;
				} else {
					default_server_announce |= SV_TYPE_DOMAIN_BAKCTRL;
				}
			}
			/* Close it */
			talloc_free(tmp_ctx);
			break;
		}
		case ROLE_STANDALONE:
		default:
			break;
	}
	if (lpcfg_time_server(dce_ctx->lp_ctx))
		default_server_announce |= SV_TYPE_TIME_SOURCE;

	if (lpcfg_host_msdfs(dce_ctx->lp_ctx))
		default_server_announce |= SV_TYPE_DFS_SERVER;


#if 0
	{ 
		/* TODO: announce us as print server when we are a print server */
		bool is_print_server = false;
		if (is_print_server) {
			default_server_announce |= SV_TYPE_PRINTQ_SERVER;
		}
	}
#endif
	return default_server_announce;
}

/* This hardcoded value should go into a ldb database! */
const char *dcesrv_common_get_lan_root(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return talloc_strdup(mem_ctx, "");
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_users(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return -1;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_disc(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return 15;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_hidden(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return 0;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_announce(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return 240;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_anndelta(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return 3000;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_licenses(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return 0;
}

/* This hardcoded value should go into a ldb database! */
const char *dcesrv_common_get_userpath(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx)
{
	return talloc_strdup(mem_ctx, "c:\\");
}

#define INVALID_SHARE_NAME_CHARS " \"*+,./:;<=>?[\\]|"

bool dcesrv_common_validate_share_name(TALLOC_CTX *mem_ctx, const char *share_name)
{
	if (strpbrk(share_name, INVALID_SHARE_NAME_CHARS)) {
		return false;
	}

	return true;
}
