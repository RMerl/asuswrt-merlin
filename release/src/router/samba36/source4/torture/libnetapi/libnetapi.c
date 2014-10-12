/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Guenther Deschner 2009

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
#include "torture/smbtorture.h"
#include "auth/credentials/credentials.h"
#include "lib/cmdline/popt_common.h"
#include <netapi.h>
#include "torture/libnetapi/proto.h"

bool torture_libnetapi_init_context(struct torture_context *tctx,
				    struct libnetapi_ctx **ctx_p)
{
	NET_API_STATUS status;
	struct libnetapi_ctx *ctx;

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return false;
	}

	libnetapi_set_debuglevel(ctx,
		talloc_asprintf(ctx, "%d", DEBUGLEVEL));
	libnetapi_set_username(ctx,
		cli_credentials_get_username(cmdline_credentials));
	libnetapi_set_password(ctx,
		cli_credentials_get_password(cmdline_credentials));

	*ctx_p = ctx;

	return true;
}

static bool torture_libnetapi_initialize(struct torture_context *tctx)
{
        NET_API_STATUS status;
	struct libnetapi_ctx *ctx;

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return false;
	}

	libnetapi_free(ctx);

	return true;
}

NTSTATUS torture_libnetapi_init(void)
{
	struct torture_suite *suite;

	suite = torture_suite_create(talloc_autofree_context(), "netapi");

	torture_suite_add_simple_test(suite, "server", torture_libnetapi_server);
	torture_suite_add_simple_test(suite, "group", torture_libnetapi_group);
	torture_suite_add_simple_test(suite, "user", torture_libnetapi_user);
	torture_suite_add_simple_test(suite, "initialize", torture_libnetapi_initialize);

	suite->description = talloc_strdup(suite, "libnetapi convenience interface tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
