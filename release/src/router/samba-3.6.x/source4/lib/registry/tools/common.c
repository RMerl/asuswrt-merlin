/*
   Unix SMB/CIFS implementation.
   Popt routines specifically for registry

   Copyright (C) Jelmer Vernooij 2007

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
#include "auth/credentials/credentials.h"
#include "lib/registry/registry.h"
#include "lib/registry/tools/common.h"

struct registry_context *reg_common_open_remote(const char *remote,
						struct tevent_context *ev_ctx,
						struct loadparm_context *lp_ctx,
						struct cli_credentials *creds)
{
	struct registry_context *h = NULL;
	WERROR error;

	error = reg_open_remote(&h, NULL, creds, lp_ctx, remote, ev_ctx);

	if (!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Unable to open remote registry at %s:%s \n",
			remote, win_errstr(error));
		return NULL;
	}

	return h;
}

struct registry_key *reg_common_open_file(const char *path,
					  struct tevent_context *ev_ctx,
					  struct loadparm_context *lp_ctx,
					  struct cli_credentials *creds)
{
	struct hive_key *hive_root;
	struct registry_context *h = NULL;
	WERROR error;

	error = reg_open_hive(ev_ctx, path, NULL, creds, ev_ctx, lp_ctx, &hive_root);

	if(!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Unable to open '%s': %s \n",
			path, win_errstr(error));
		return NULL;
	}

	error = reg_open_local(NULL, &h);
	if (!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Unable to initialize local registry: %s\n",
			win_errstr(error));
		return NULL;
	}

	return reg_import_hive_key(h, hive_root, -1, NULL);
}

struct registry_context *reg_common_open_local(struct cli_credentials *creds, 
					       struct tevent_context *ev_ctx, 
					       struct loadparm_context *lp_ctx)
{
	WERROR error;
	struct registry_context *h = NULL;

	error = reg_open_samba(NULL, &h, ev_ctx, lp_ctx, NULL, creds);

	if(!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Unable to open local registry:%s \n",
			win_errstr(error));
		return NULL;
	}

	return h;
}
