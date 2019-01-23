/*
   Unix SMB/CIFS implementation.
   test suite for rpc bind operations

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "lib/cmdline/popt_common.h"

static bool test_openpolicy(struct torture_context *tctx,
			    struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct policy_handle *handle;

	torture_assert(tctx,
		test_lsa_OpenPolicy2(b, tctx, &handle),
		"failed to open policy");

	torture_assert(tctx,
		test_lsa_Close(b, tctx, handle),
		"failed to close policy");

	return true;
}

static bool test_bind(struct torture_context *tctx,
		      const void *private_data)
{
	struct dcerpc_binding *binding;
	struct dcerpc_pipe *p;
	const uint32_t *flags = (const uint32_t *)private_data;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_binding(tctx, &binding),
		"failed to parse binding string");

	binding->flags &= ~DCERPC_AUTH_OPTIONS;
	binding->flags |= *flags;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_pipe_connect_b(tctx, &p, binding,
				      &ndr_table_lsarpc,
				      cmdline_credentials,
				      tctx->ev,
				      tctx->lp_ctx),
		"failed to connect pipe");

	torture_assert(tctx,
		test_openpolicy(tctx, p),
		"failed to test openpolicy");

	talloc_free(p);

	return true;
}

static void test_bind_op(struct torture_suite *suite,
			 const char *name,
			 uint32_t flags)
{
	uint32_t *flags_p = talloc(suite, uint32_t);

	*flags_p = flags;

	torture_suite_add_simple_tcase_const(suite, name, test_bind, flags_p);
}


struct torture_suite *torture_rpc_bind(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "bind");
	struct {
		const char *test_name;
		uint32_t flags;
	} tests[] = {
		{
			.test_name	= "ntlm,sign",
			.flags		= DCERPC_AUTH_NTLM | DCERPC_SIGN
		},{
			.test_name	= "ntlm,sign,seal",
			.flags		= DCERPC_AUTH_NTLM | DCERPC_SIGN | DCERPC_SEAL
		},{
			.test_name	= "spnego,sign",
			.flags		= DCERPC_AUTH_SPNEGO | DCERPC_SIGN
		},{
			.test_name	= "spnego,sign,seal",
			.flags		= DCERPC_AUTH_SPNEGO | DCERPC_SIGN | DCERPC_SEAL
		}
	};
	int i;

	for (i=0; i < ARRAY_SIZE(tests); i++) {
		test_bind_op(suite, tests[i].test_name, tests[i].flags);
	}
	for (i=0; i < ARRAY_SIZE(tests); i++) {
		test_bind_op(suite, talloc_asprintf(suite, "bigendian,%s", tests[i].test_name), tests[i].flags | DCERPC_PUSH_BIGENDIAN);
	}

	return suite;
}
