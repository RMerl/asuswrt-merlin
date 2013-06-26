/*
   Unix SMB/CIFS implementation.

   Example echo torture tests

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

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
#include "libcli/resolve/resolve.h"
#include <tevent.h>
#include "libcli/util/ntstatus.h"
#include "libcli/echo/libecho.h"

NTSTATUS torture_libcli_echo_init(void);

/* Basic test function that sends an echo request and checks the reply */
static bool echo_udp_basic(struct torture_context *tctx, const char *address)
{
	struct tevent_req *req;
	NTSTATUS status;
	const char *msg_send = "This is a test string\n";
	char *msg_recv;

	req = echo_request_send(tctx, tctx->ev, address, msg_send);
	torture_assert(tctx, req != NULL,
		       "echo_request_send returned non-null tevent_req");

	while(tevent_req_is_in_progress(req)) {
		tevent_loop_once(tctx->ev);
	}

	status = echo_request_recv(req, tctx, &msg_recv);
	torture_assert_ntstatus_ok(tctx, status,
				   "echo_request_recv returned ok");

	torture_assert_str_equal(tctx, msg_recv, msg_send,
				 "Echo server echoed request string");

	return true;
}

/*Test case to set up the environment and perform UDP-based echo tests */
static bool torture_echo_udp(struct torture_context *tctx)
{
	const char *address;
	struct nbt_name name;
	NTSTATUS status;
	bool ret = true;

	make_nbt_name_server(&name,
			     torture_setting_string(tctx, "host", NULL));
	status = resolve_name(lpcfg_resolve_context(tctx->lp_ctx), &name, tctx,
			      &address, tctx->ev);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to resolve %s - %s\n", name.name,
		       nt_errstr(status));
		return false;
	}

	/* All tests are now called here */
	ret &= echo_udp_basic(tctx, address);

	return ret;
}

/* Test suite that bundles all the libecho tests */
NTSTATUS torture_libcli_echo_init(void)
{
	struct torture_suite *suite;

	suite = torture_suite_create(talloc_autofree_context(), "echo");
	NT_STATUS_HAVE_NO_MEMORY(suite);

	torture_suite_add_simple_test(suite, "udp", torture_echo_udp);

	suite->description = talloc_strdup(suite, "libcli/echo interface tests");
	torture_register_suite(suite);

	return NT_STATUS_OK;
}
