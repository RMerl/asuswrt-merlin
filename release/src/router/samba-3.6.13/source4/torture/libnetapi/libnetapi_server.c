/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Guenther Deschner 2010

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
#include <netapi.h>
#include "torture/libnetapi/proto.h"

#define NETAPI_STATUS(tctx, x,y,fn) \
	torture_warning(tctx, "FAILURE: line %d: %s failed with status: %s (%d)\n", \
		__LINE__, fn, libnetapi_get_error_string(x,y), y);

bool torture_libnetapi_server(struct torture_context *tctx)
{
	NET_API_STATUS status = 0;
	uint8_t *buffer = NULL;
	int i;

	const char *hostname = torture_setting_string(tctx, "host", NULL);
	struct libnetapi_ctx *ctx;

	torture_assert(tctx, torture_libnetapi_init_context(tctx, &ctx),
		       "failed to initialize libnetapi");

	torture_comment(tctx, "NetServer tests\n");

	torture_comment(tctx, "Testing NetRemoteTOD\n");

	status = NetRemoteTOD(hostname, &buffer);
	if (status) {
		NETAPI_STATUS(tctx, ctx, status, "NetRemoteTOD");
		goto out;
	}
	NetApiBufferFree(buffer);

	torture_comment(tctx, "Testing NetRemoteTOD 10 times\n");

	for (i=0; i<10; i++) {
		status = NetRemoteTOD(hostname, &buffer);
		if (status) {
			NETAPI_STATUS(tctx, ctx, status, "NetRemoteTOD");
			goto out;
		}
		NetApiBufferFree(buffer);
	}

	status = 0;

	torture_comment(tctx, "NetServer tests succeeded\n");
 out:
	if (status != 0) {
		torture_comment(tctx, "NetServer testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		libnetapi_free(ctx);
		return false;
	}

	libnetapi_free(ctx);
	return true;
}
