/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Jelmer Vernooij 2006
   
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

#ifndef __TORTURE_RPC_H__
#define __TORTURE_RPC_H__

#include "lib/torture/torture.h"
#include "auth/credentials/credentials.h"
#include "torture/rpc/drsuapi.h"
#include "libnet/libnet_join.h"
#include "librpc/rpc/dcerpc.h"
#include "libcli/raw/libcliraw.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "torture/rpc/proto.h"

struct torture_rpc_tcase {
	struct torture_tcase tcase;
	const struct ndr_interface_table *table;
	const char *machine_name;
};

struct torture_rpc_tcase_data {
	struct test_join *join_ctx;
	struct dcerpc_pipe *pipe;
	struct cli_credentials *credentials;
};

NTSTATUS torture_rpc_connection(struct torture_context *tctx,
				struct dcerpc_pipe **p, 
				const struct ndr_interface_table *table);

struct test_join *torture_join_domain(struct torture_context *tctx,
					       const char *machine_name, 
				      uint32_t acct_flags,
				      struct cli_credentials **machine_credentials);
const struct dom_sid *torture_join_sid(struct test_join *join);
void torture_leave_domain(struct torture_context *tctx, struct test_join *join);
struct torture_rpc_tcase *torture_suite_add_rpc_iface_tcase(struct torture_suite *suite, 
								const char *name,
								const struct ndr_interface_table *table);

struct torture_test *torture_rpc_tcase_add_test(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *));
struct torture_rpc_tcase *torture_suite_add_anon_rpc_iface_tcase(struct torture_suite *suite, 
								const char *name,
								const struct ndr_interface_table *table);

struct torture_test *torture_rpc_tcase_add_test_join(
	struct torture_rpc_tcase *tcase,
	const char *name,
	bool (*fn) (struct torture_context *, struct dcerpc_pipe *,
		    struct cli_credentials *, struct test_join *));

struct torture_test *torture_rpc_tcase_add_test_ex(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *,
								void *),
					void *userdata);
struct torture_rpc_tcase *torture_suite_add_machine_bdc_rpc_iface_tcase(
				struct torture_suite *suite,
				const char *name,
				const struct ndr_interface_table *table,
				const char *machine_name);
struct torture_rpc_tcase *torture_suite_add_machine_workstation_rpc_iface_tcase(
				struct torture_suite *suite, 
				const char *name,
				const struct ndr_interface_table *table,
				const char *machine_name);
struct torture_test *torture_rpc_tcase_add_test_creds(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *, struct cli_credentials *));
bool torture_suite_init_rpc_tcase(struct torture_suite *suite, 
					   struct torture_rpc_tcase *tcase, 
					   const char *name,
					   const struct ndr_interface_table *table);



#endif /* __TORTURE_RPC_H__ */
