/* 
   Unix SMB/CIFS implementation.

   module to store/fetch session keys for the schannel server

   Copyright (C) Andrew Tridgell 2004
   
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
#include "lib/ldb/include/ldb.h"
#include "ldb_wrap.h"
#include "../lib/util/util_ldb.h"
#include "libcli/auth/libcli_auth.h"
#include "auth/auth.h"
#include "param/param.h"
#include "auth/gensec/schannel_state.h"

/**
  connect to the schannel ldb
*/
struct ldb_context *schannel_db_connect(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx,
					struct loadparm_context *lp_ctx)
{
	char *path;
	struct ldb_context *ldb;
	bool existed;
	const char *init_ldif = 
		"dn: @ATTRIBUTES\n" \
		"computerName: CASE_INSENSITIVE\n" \
		"flatname: CASE_INSENSITIVE\n";

	path = private_path(mem_ctx, lp_ctx, "schannel.ldb");
	if (!path) {
		return NULL;
	}

	existed = file_exist(path);
	
	ldb = ldb_wrap_connect(mem_ctx, ev_ctx, lp_ctx, path, 
			       system_session(mem_ctx, lp_ctx), 
			       NULL, LDB_FLG_NOSYNC, NULL);
	talloc_free(path);
	if (!ldb) {
		return NULL;
	}
	
	if (!existed) {
		gendb_add_ldif(ldb, init_ldif);
	}

	return ldb;
}

