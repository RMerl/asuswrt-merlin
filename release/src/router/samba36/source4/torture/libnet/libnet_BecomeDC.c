/* 
   Unix SMB/CIFS implementation.

   libnet_BecomeDC() tests

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006
   
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
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "libnet/libnet.h"
#include "dsdb/samdb/samdb.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "system/time.h"
#include "ldb_wrap.h"
#include "auth/auth.h"
#include "param/param.h"
#include "param/provision.h"
#include "libcli/resolve/resolve.h"

bool torture_net_become_dc(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	struct libnet_BecomeDC b;
	struct libnet_UnbecomeDC u;
	struct libnet_vampire_cb_state *s;
	struct ldb_message *msg;
	int ldb_ret;
	uint32_t i;
	char *sam_ldb_path;
	const char *address;
	struct nbt_name name;
	const char *netbios_name;
	struct cli_credentials *machine_account;
	struct test_join *tj;
	struct loadparm_context *lp_ctx;
	struct ldb_context *ldb;
	struct libnet_context *ctx;
	struct dsdb_schema *schema;

	char *location = NULL;
	torture_assert_ntstatus_ok(torture, torture_temp_dir(torture, "libnet_BecomeDC", &location), 
				   "torture_temp_dir should return NT_STATUS_OK" );

	netbios_name = lpcfg_parm_string(torture->lp_ctx, NULL, "become dc", "smbtorture dc");
	if (!netbios_name || !netbios_name[0]) {
		netbios_name = "smbtorturedc";
	}

	make_nbt_name_server(&name, torture_setting_string(torture, "host", NULL));

	/* do an initial name resolution to find its IP */
	status = resolve_name(lpcfg_resolve_context(torture->lp_ctx),
			      &name, torture, &address, torture->ev);
	torture_assert_ntstatus_ok(torture, status, talloc_asprintf(torture,
				   "Failed to resolve %s - %s\n",
				   name.name, nt_errstr(status)));


	/* Join domain as a member server. */
	tj = torture_join_domain(torture, netbios_name,
				 ACB_WSTRUST,
				 &machine_account);
	torture_assert(torture, tj, talloc_asprintf(torture,
						    "%s failed to join domain as workstation\n",
						    netbios_name));

	s = libnet_vampire_cb_state_init(torture, torture->lp_ctx, torture->ev,
			       netbios_name,
			       torture_join_dom_netbios_name(tj),
			       torture_join_dom_dns_name(tj),
			       location);
	torture_assert(torture, s, "libnet_vampire_cb_state_init");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	ZERO_STRUCT(b);
	b.in.domain_dns_name		= torture_join_dom_dns_name(tj);
	b.in.domain_netbios_name	= torture_join_dom_netbios_name(tj);
	b.in.domain_sid			= torture_join_sid(tj);
	b.in.source_dsa_address		= address;
	b.in.dest_dsa_netbios_name	= netbios_name;

	b.in.callbacks.private_data	= s;
	b.in.callbacks.check_options	= libnet_vampire_cb_check_options;
	b.in.callbacks.prepare_db       = libnet_vampire_cb_prepare_db;
	b.in.callbacks.schema_chunk	= libnet_vampire_cb_schema_chunk;
	b.in.callbacks.config_chunk	= libnet_vampire_cb_store_chunk;
	b.in.callbacks.domain_chunk	= libnet_vampire_cb_store_chunk;

	status = libnet_BecomeDC(ctx, s, &b);
	torture_assert_ntstatus_ok_goto(torture, status, ret, cleanup, talloc_asprintf(torture,
				   "libnet_BecomeDC() failed - %s %s\n",
				   nt_errstr(status), b.out.error_string));
	ldb = libnet_vampire_cb_ldb(s);

	msg = ldb_msg_new(s);
	torture_assert_int_equal_goto(torture, (msg?1:0), 1, ret, cleanup,
				      "ldb_msg_new() failed\n");
	msg->dn = ldb_dn_new(msg, ldb, "@ROOTDSE");
	torture_assert_int_equal_goto(torture, (msg->dn?1:0), 1, ret, cleanup,
				      "ldb_msg_new(@ROOTDSE) failed\n");

	ldb_ret = ldb_msg_add_string(msg, "isSynchronized", "TRUE");
	torture_assert_int_equal_goto(torture, ldb_ret, LDB_SUCCESS, ret, cleanup,
				      "ldb_msg_add_string(msg, isSynchronized, TRUE) failed\n");

	for (i=0; i < msg->num_elements; i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	torture_comment(torture, "mark ROOTDSE with isSynchronized=TRUE\n");
	ldb_ret = ldb_modify(libnet_vampire_cb_ldb(s), msg);
	torture_assert_int_equal_goto(torture, ldb_ret, LDB_SUCCESS, ret, cleanup,
				      "ldb_modify() failed\n");
	
	/* commit the transaction now we know the secrets were written
	 * out properly
	*/
	ldb_ret = ldb_transaction_commit(ldb);
	torture_assert_int_equal_goto(torture, ldb_ret, LDB_SUCCESS, ret, cleanup,
				      "ldb_transaction_commit() failed\n");

	/* reopen the ldb */
	talloc_unlink(s, ldb);

	lp_ctx = libnet_vampire_cb_lp_ctx(s);
	sam_ldb_path = talloc_asprintf(s, "%s/%s", location, "private/sam.ldb");
	lpcfg_set_cmdline(lp_ctx, "sam database", sam_ldb_path);
	torture_comment(torture, "Reopen the SAM LDB with system credentials and all replicated data: %s\n", sam_ldb_path);
	ldb = samdb_connect(s, torture->ev, lp_ctx, system_session(lp_ctx), 0);
	torture_assert_goto(torture, ldb != NULL, ret, cleanup,
				      talloc_asprintf(torture,
				      "Failed to open '%s'\n", sam_ldb_path));

	torture_assert_goto(torture, dsdb_uses_global_schema(ldb), ret, cleanup,
						"Uses global schema");

	schema = dsdb_get_schema(ldb, s);
	torture_assert_goto(torture, schema != NULL, ret, cleanup,
				      "Failed to get loaded dsdb_schema\n");

	/* Make sure we get this from the command line */
	if (lpcfg_parm_bool(torture->lp_ctx, NULL, "become dc", "do not unjoin", false)) {
		talloc_free(s);
		return ret;
	}

cleanup:
	ZERO_STRUCT(u);
	u.in.domain_dns_name		= torture_join_dom_dns_name(tj);
	u.in.domain_netbios_name	= torture_join_dom_netbios_name(tj);
	u.in.source_dsa_address		= address;
	u.in.dest_dsa_netbios_name	= netbios_name;

	status = libnet_UnbecomeDC(ctx, s, &u);
	torture_assert_ntstatus_ok(torture, status, talloc_asprintf(torture,
				   "libnet_UnbecomeDC() failed - %s %s\n",
				   nt_errstr(status), u.out.error_string));

	/* Leave domain. */
	torture_leave_domain(torture, tj);

	talloc_free(s);
	return ret;
}
