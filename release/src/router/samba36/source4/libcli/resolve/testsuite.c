/* 
   Unix SMB/CIFS implementation.

   local test for async resolve code

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
#include "lib/events/events.h"
#include "libcli/resolve/resolve.h"
#include "torture/torture.h"
#include "system/network.h"
#include "lib/util/util_net.h"

static bool test_async_resolve(struct torture_context *tctx)
{
	struct nbt_name n;
	struct tevent_context *ev;
	int timelimit = torture_setting_int(tctx, "timelimit", 2);
	const char *host = torture_setting_string(tctx, "host", NULL);
	int count = 0;
	struct timeval tv = timeval_current();
	TALLOC_CTX *mem_ctx = tctx;

	ev = tctx->ev;

	ZERO_STRUCT(n);
	n.name = host;

	torture_comment(tctx, "Testing async resolve of '%s' for %d seconds\n",
			host, timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		struct socket_address **s;
		struct composite_context *c = resolve_name_host_send(mem_ctx, ev, NULL, 0, 0, &n);
		torture_assert(tctx, c != NULL, "resolve_name_host_send");
		torture_assert_ntstatus_ok(tctx, resolve_name_host_recv(c, mem_ctx, &s, NULL),
								   "async resolve failed");
		count++;
	}

	torture_comment(tctx, "async rate of %.1f resolves/sec\n", 
			count/timeval_elapsed(&tv));
	return true;
}

/*
  test resolution using sync method
*/
static bool test_sync_resolve(struct torture_context *tctx)
{
	int timelimit = torture_setting_int(tctx, "timelimit", 2);
	struct timeval tv = timeval_current();
	int count = 0;
	const char *host = torture_setting_string(tctx, "host", NULL);

	torture_comment(tctx, "Testing sync resolve of '%s' for %d seconds\n", 
			host, timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		inet_ntoa(interpret_addr2(host));
		count++;
	}
	
	torture_comment(tctx, "sync rate of %.1f resolves/sec\n", 
			count/timeval_elapsed(&tv));
	return true;
}


struct torture_suite *torture_local_resolve(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "resolve");

	torture_suite_add_simple_test(suite, "async", test_async_resolve);
	torture_suite_add_simple_test(suite, "sync", test_sync_resolve);

	return suite;
}
