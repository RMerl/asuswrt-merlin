/*
   Unix SMB/CIFS implementation.
   test suite for nbt ndr operations

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
#include "torture/ndr/ndr.h"
#include "librpc/gen_ndr/ndr_nbt.h"

static const uint8_t netlogon_logon_request_req_data[] = {
	0x00, 0x00, 0x57, 0x49, 0x4e, 0x39, 0x38, 0x00, 0x47, 0x44, 0x00, 0x5c,
	0x4d, 0x41, 0x49, 0x4c, 0x53, 0x4c, 0x4f, 0x54, 0x5c, 0x54, 0x45, 0x4d,
	0x50, 0x5c, 0x4e, 0x45, 0x54, 0x4c, 0x4f, 0x47, 0x4f, 0x4e, 0x00, 0x01,
	0x01, 0x00, 0xff, 0xff
};

static bool netlogon_logon_request_req_check(struct torture_context *tctx,
					     struct nbt_netlogon_packet *r)
{
	torture_assert_int_equal(tctx, r->command, LOGON_REQUEST, "command");
	torture_assert_str_equal(tctx, r->req.logon0.computer_name, "WIN98", "computer name");
	torture_assert_str_equal(tctx, r->req.logon0.user_name, "GD", "user_name");
	torture_assert_str_equal(tctx, r->req.logon0.mailslot_name, "\\MAILSLOT\\TEMP\\NETLOGON", "mailslot_name");
	torture_assert_int_equal(tctx, r->req.logon0.request_count, 1, "request_count");
	torture_assert_int_equal(tctx, r->req.logon0.lmnt_token, 1, "lmnt_token");
	torture_assert_int_equal(tctx, r->req.logon0.lm20_token, 0xffff, "lm20_token");

	return true;
}

static const uint8_t netlogon_logon_request_resp_data[] = {
	0x06, 0x00, 0x5c, 0x5c, 0x4d, 0x54, 0x48, 0x45, 0x4c, 0x45, 0x4e, 0x41,
	0x00, 0xff, 0xff
};

static bool netlogon_logon_request_resp_check(struct torture_context *tctx,
					      struct nbt_netlogon_response2 *r)
{
	torture_assert_int_equal(tctx, r->command, LOGON_RESPONSE2, "command");
	torture_assert_str_equal(tctx, r->pdc_name, "\\\\MTHELENA", "pdc_name");
	torture_assert_int_equal(tctx, r->lm20_token, 0xffff, "lm20_token");

	return true;
}

struct torture_suite *ndr_nbt_suite(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "nbt");

	torture_suite_add_ndr_pull_test(suite, nbt_netlogon_packet, netlogon_logon_request_req_data, netlogon_logon_request_req_check);

	torture_suite_add_ndr_pull_test(suite, nbt_netlogon_response2, netlogon_logon_request_resp_data, netlogon_logon_request_resp_check);

	return suite;
}
