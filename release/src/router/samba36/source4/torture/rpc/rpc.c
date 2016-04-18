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

#include "includes.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/smbtorture.h"
#include "librpc/ndr/ndr_table.h"
#include "../lib/util/dlinklist.h"

static bool torture_rpc_teardown (struct torture_context *tcase, 
					  void *data)
{
	struct torture_rpc_tcase_data *tcase_data = 
		(struct torture_rpc_tcase_data *)data;
	if (tcase_data->join_ctx != NULL)
	    torture_leave_domain(tcase, tcase_data->join_ctx);
	talloc_free(tcase_data);
	return true;
}

/**
 * Obtain the DCE/RPC binding context associated with a torture context.
 *
 * @param tctx Torture context
 * @param binding Pointer to store DCE/RPC binding
 */
NTSTATUS torture_rpc_binding(struct torture_context *tctx,
			     struct dcerpc_binding **binding)
{
	NTSTATUS status;
	const char *binding_string = torture_setting_string(tctx, "binding", 
							    NULL);

	if (binding_string == NULL) {
		torture_comment(tctx, 
				"You must specify a DCE/RPC binding string\n");
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_parse_binding(tctx, binding_string, binding);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0,("Failed to parse dcerpc binding '%s'\n", 
			 binding_string));
		return status;
	}

	return NT_STATUS_OK;	
}

/**
 * open a rpc connection to the chosen binding string
 */
_PUBLIC_ NTSTATUS torture_rpc_connection(struct torture_context *tctx,
				struct dcerpc_pipe **p, 
				const struct ndr_interface_table *table)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;

	dcerpc_init(tctx->lp_ctx);

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return status;

	status = dcerpc_pipe_connect_b(tctx, 
				     p, binding, table,
				     cmdline_credentials, tctx->ev, tctx->lp_ctx);
 
	if (NT_STATUS_IS_ERR(status)) {
		printf("Failed to connect to remote server: %s %s\n", 
			   dcerpc_binding_string(tctx, binding), nt_errstr(status));
	}

	return status;
}

/**
 * open a rpc connection to a specific transport
 */
NTSTATUS torture_rpc_connection_transport(struct torture_context *tctx, 
					  struct dcerpc_pipe **p, 
					  const struct ndr_interface_table *table,
					  enum dcerpc_transport_t transport,
					  uint32_t assoc_group_id)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return status;

	binding->transport = transport;
	binding->assoc_group_id = assoc_group_id;

	status = dcerpc_pipe_connect_b(tctx, p, binding, table,
				       cmdline_credentials, tctx->ev, tctx->lp_ctx);
					   
	if (NT_STATUS_IS_ERR(status)) {
		*p = NULL;
	}

        return status;
}

static bool torture_rpc_setup_machine_workstation(struct torture_context *tctx,
						  void **data)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;
	struct torture_rpc_tcase *tcase = talloc_get_type(tctx->active_tcase,
						struct torture_rpc_tcase);
	struct torture_rpc_tcase_data *tcase_data;

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return false;

	*data = tcase_data = talloc_zero(tctx, struct torture_rpc_tcase_data);
	tcase_data->credentials = cmdline_credentials;
	tcase_data->join_ctx = torture_join_domain(tctx, tcase->machine_name,
						   ACB_WSTRUST,
						   &tcase_data->credentials);
	if (tcase_data->join_ctx == NULL)
	    torture_fail(tctx, "Failed to join as WORKSTATION");

	status = dcerpc_pipe_connect_b(tctx,
				&(tcase_data->pipe),
				binding,
				tcase->table,
				tcase_data->credentials, tctx->ev, tctx->lp_ctx);

	torture_assert_ntstatus_ok(tctx, status, "Error connecting to server");

	return NT_STATUS_IS_OK(status);
}

static bool torture_rpc_setup_machine_bdc(struct torture_context *tctx,
					  void **data)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;
	struct torture_rpc_tcase *tcase = talloc_get_type(tctx->active_tcase, 
						struct torture_rpc_tcase);
	struct torture_rpc_tcase_data *tcase_data;

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return false;

	*data = tcase_data = talloc_zero(tctx, struct torture_rpc_tcase_data);
	tcase_data->credentials = cmdline_credentials;
	tcase_data->join_ctx = torture_join_domain(tctx, tcase->machine_name,
						   ACB_SVRTRUST, 
						   &tcase_data->credentials);
	if (tcase_data->join_ctx == NULL)
	    torture_fail(tctx, "Failed to join as BDC");

	status = dcerpc_pipe_connect_b(tctx, 
				&(tcase_data->pipe),
				binding,
				tcase->table,
				tcase_data->credentials, tctx->ev, tctx->lp_ctx);

	torture_assert_ntstatus_ok(tctx, status, "Error connecting to server");

	return NT_STATUS_IS_OK(status);
}

_PUBLIC_ struct torture_rpc_tcase *torture_suite_add_machine_workstation_rpc_iface_tcase(
				struct torture_suite *suite,
				const char *name,
				const struct ndr_interface_table *table,
				const char *machine_name)
{
	struct torture_rpc_tcase *tcase = talloc(suite,
						 struct torture_rpc_tcase);

	torture_suite_init_rpc_tcase(suite, tcase, name, table);

	tcase->machine_name = talloc_strdup(tcase, machine_name);
	tcase->tcase.setup = torture_rpc_setup_machine_workstation;
	tcase->tcase.teardown = torture_rpc_teardown;

	return tcase;
}

_PUBLIC_ struct torture_rpc_tcase *torture_suite_add_machine_bdc_rpc_iface_tcase(
				struct torture_suite *suite, 
				const char *name,
				const struct ndr_interface_table *table,
				const char *machine_name)
{
	struct torture_rpc_tcase *tcase = talloc(suite, 
						 struct torture_rpc_tcase);

	torture_suite_init_rpc_tcase(suite, tcase, name, table);

	tcase->machine_name = talloc_strdup(tcase, machine_name);
	tcase->tcase.setup = torture_rpc_setup_machine_bdc;
	tcase->tcase.teardown = torture_rpc_teardown;

	return tcase;
}

_PUBLIC_ bool torture_suite_init_rpc_tcase(struct torture_suite *suite, 
					   struct torture_rpc_tcase *tcase, 
					   const char *name,
					   const struct ndr_interface_table *table)
{
	if (!torture_suite_init_tcase(suite, (struct torture_tcase *)tcase, name))
		return false;

	tcase->table = table;

	return true;
}

static bool torture_rpc_setup_anonymous(struct torture_context *tctx, 
					void **data)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;
	struct torture_rpc_tcase_data *tcase_data;
	struct torture_rpc_tcase *tcase = talloc_get_type(tctx->active_tcase, 
							  struct torture_rpc_tcase);

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return false;

	*data = tcase_data = talloc_zero(tctx, struct torture_rpc_tcase_data);
	tcase_data->credentials = cli_credentials_init_anon(tctx);

	status = dcerpc_pipe_connect_b(tctx, 
				&(tcase_data->pipe),
				binding,
				tcase->table,
				tcase_data->credentials, tctx->ev, tctx->lp_ctx);

	torture_assert_ntstatus_ok(tctx, status, "Error connecting to server");

	return NT_STATUS_IS_OK(status);
}

static bool torture_rpc_setup (struct torture_context *tctx, void **data)
{
	NTSTATUS status;
	struct torture_rpc_tcase *tcase = talloc_get_type(
						tctx->active_tcase, struct torture_rpc_tcase);
	struct torture_rpc_tcase_data *tcase_data;

	*data = tcase_data = talloc_zero(tctx, struct torture_rpc_tcase_data);
	tcase_data->credentials = cmdline_credentials;
	
	status = torture_rpc_connection(tctx, 
				&(tcase_data->pipe),
				tcase->table);

	torture_assert_ntstatus_ok(tctx, status, "Error connecting to server");

	return NT_STATUS_IS_OK(status);
}



_PUBLIC_ struct torture_rpc_tcase *torture_suite_add_anon_rpc_iface_tcase(struct torture_suite *suite, 
								const char *name,
								const struct ndr_interface_table *table)
{
	struct torture_rpc_tcase *tcase = talloc(suite, struct torture_rpc_tcase);

	torture_suite_init_rpc_tcase(suite, tcase, name, table);

	tcase->tcase.setup = torture_rpc_setup_anonymous;
	tcase->tcase.teardown = torture_rpc_teardown;

	return tcase;
}


_PUBLIC_ struct torture_rpc_tcase *torture_suite_add_rpc_iface_tcase(struct torture_suite *suite, 
								const char *name,
								const struct ndr_interface_table *table)
{
	struct torture_rpc_tcase *tcase = talloc(suite, struct torture_rpc_tcase);

	torture_suite_init_rpc_tcase(suite, tcase, name, table);

	tcase->tcase.setup = torture_rpc_setup;
	tcase->tcase.teardown = torture_rpc_teardown;

	return tcase;
}

static bool torture_rpc_wrap_test(struct torture_context *tctx, 
								  struct torture_tcase *tcase,
								  struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct dcerpc_pipe *);
	struct torture_rpc_tcase_data *tcase_data = 
		(struct torture_rpc_tcase_data *)tcase->data;

	fn = test->fn;

	return fn(tctx, tcase_data->pipe);
}

static bool torture_rpc_wrap_test_ex(struct torture_context *tctx, 
								  struct torture_tcase *tcase,
								  struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct dcerpc_pipe *, const void *);
	struct torture_rpc_tcase_data *tcase_data = 
		(struct torture_rpc_tcase_data *)tcase->data;

	fn = test->fn;

	return fn(tctx, tcase_data->pipe, test->data);
}


static bool torture_rpc_wrap_test_creds(struct torture_context *tctx, 
								  struct torture_tcase *tcase,
								  struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct dcerpc_pipe *, struct cli_credentials *);
	struct torture_rpc_tcase_data *tcase_data = 
		(struct torture_rpc_tcase_data *)tcase->data;

	fn = test->fn;

	return fn(tctx, tcase_data->pipe, tcase_data->credentials);
}

static bool torture_rpc_wrap_test_join(struct torture_context *tctx,
				       struct torture_tcase *tcase,
				       struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct dcerpc_pipe *, struct cli_credentials *, struct test_join *);
	struct torture_rpc_tcase_data *tcase_data =
		(struct torture_rpc_tcase_data *)tcase->data;

	fn = test->fn;

	return fn(tctx, tcase_data->pipe, tcase_data->credentials, tcase_data->join_ctx);
}

_PUBLIC_ struct torture_test *torture_rpc_tcase_add_test(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *))
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = torture_rpc_wrap_test;
	test->dangerous = false;
	test->data = NULL;
	test->fn = fn;

	DLIST_ADD(tcase->tcase.tests, test);

	return test;
}

_PUBLIC_ struct torture_test *torture_rpc_tcase_add_test_creds(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *, struct cli_credentials *))
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = torture_rpc_wrap_test_creds;
	test->dangerous = false;
	test->data = NULL;
	test->fn = fn;

	DLIST_ADD(tcase->tcase.tests, test);

	return test;
}

_PUBLIC_ struct torture_test *torture_rpc_tcase_add_test_join(
					struct torture_rpc_tcase *tcase,
					const char *name,
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *,
						    struct cli_credentials *, struct test_join *))
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = torture_rpc_wrap_test_join;
	test->dangerous = false;
	test->data = NULL;
	test->fn = fn;

	DLIST_ADD(tcase->tcase.tests, test);

	return test;
}

_PUBLIC_ struct torture_test *torture_rpc_tcase_add_test_ex(
					struct torture_rpc_tcase *tcase, 
					const char *name, 
					bool (*fn) (struct torture_context *, struct dcerpc_pipe *,
								void *),
					void *userdata)
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = torture_rpc_wrap_test_ex;
	test->dangerous = false;
	test->data = userdata;
	test->fn = fn;

	DLIST_ADD(tcase->tcase.tests, test);

	return test;
}

NTSTATUS torture_rpc_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "rpc");

	ndr_table_init();

	torture_suite_add_simple_test(suite, "lsa", torture_rpc_lsa);
	torture_suite_add_simple_test(suite, "lsalookup", torture_rpc_lsa_lookup);
	torture_suite_add_simple_test(suite, "lsa-getuser", torture_rpc_lsa_get_user);
	torture_suite_add_suite(suite, torture_rpc_lsa_lookup_sids(suite));
	torture_suite_add_suite(suite, torture_rpc_lsa_lookup_names(suite));
	torture_suite_add_suite(suite, torture_rpc_lsa_secrets(suite));
	torture_suite_add_suite(suite, torture_rpc_lsa_trusted_domains(suite));
	torture_suite_add_suite(suite, torture_rpc_lsa_forest_trust(suite));
	torture_suite_add_suite(suite, torture_rpc_lsa_privileges(suite));
	torture_suite_add_suite(suite, torture_rpc_echo(suite));
	torture_suite_add_suite(suite, torture_rpc_dfs(suite));
	torture_suite_add_suite(suite, torture_rpc_frsapi(suite));
	torture_suite_add_suite(suite, torture_rpc_unixinfo(suite));
	torture_suite_add_suite(suite, torture_rpc_eventlog(suite));
	torture_suite_add_suite(suite, torture_rpc_atsvc(suite));
	torture_suite_add_suite(suite, torture_rpc_wkssvc(suite));
	torture_suite_add_suite(suite, torture_rpc_handles(suite));
	torture_suite_add_suite(suite, torture_rpc_object_uuid(suite));
	torture_suite_add_suite(suite, torture_rpc_winreg(suite));
	torture_suite_add_suite(suite, torture_rpc_spoolss(suite));
	torture_suite_add_suite(suite, torture_rpc_spoolss_notify(suite));
	torture_suite_add_suite(suite, torture_rpc_spoolss_win(suite));
	torture_suite_add_suite(suite, torture_rpc_spoolss_driver(suite));
	torture_suite_add_suite(suite, torture_rpc_spoolss_access(suite));
	torture_suite_add_simple_test(suite, "samr", torture_rpc_samr);
	torture_suite_add_simple_test(suite, "samr.users", torture_rpc_samr_users);
	torture_suite_add_simple_test(suite, "samr.passwords", torture_rpc_samr_passwords);
	torture_suite_add_suite(suite, torture_rpc_netlogon(suite));
	torture_suite_add_suite(suite, torture_rpc_netlogon_s3(suite));
	torture_suite_add_suite(suite, torture_rpc_netlogon_admin(suite));
	torture_suite_add_suite(suite, torture_rpc_remote_pac(suite));
	torture_suite_add_simple_test(suite, "samlogon", torture_rpc_samlogon);
	torture_suite_add_simple_test(suite, "samsync", torture_rpc_samsync);
	torture_suite_add_simple_test(suite, "schannel", torture_rpc_schannel);
	torture_suite_add_simple_test(suite, "schannel2", torture_rpc_schannel2);
	torture_suite_add_simple_test(suite, "bench-schannel1", torture_rpc_schannel_bench1);
	torture_suite_add_suite(suite, torture_rpc_srvsvc(suite));
	torture_suite_add_suite(suite, torture_rpc_svcctl(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_accessmask(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_workstation_auth(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_passwords_pwdlastset(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_passwords_badpwdcount(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_passwords_lockout(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_passwords_validate(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_user_privileges(suite));
	torture_suite_add_suite(suite, torture_rpc_samr_large_dc(suite));
	torture_suite_add_suite(suite, torture_rpc_epmapper(suite));
	torture_suite_add_suite(suite, torture_rpc_initshutdown(suite));
	torture_suite_add_suite(suite, torture_rpc_oxidresolve(suite));
	torture_suite_add_suite(suite, torture_rpc_remact(suite));
	torture_suite_add_simple_test(suite, "mgmt", torture_rpc_mgmt);
	torture_suite_add_simple_test(suite, "scanner", torture_rpc_scanner);
	torture_suite_add_simple_test(suite, "autoidl", torture_rpc_autoidl);
	torture_suite_add_simple_test(suite, "countcalls", torture_rpc_countcalls);
	torture_suite_add_simple_test(suite, "multibind", torture_multi_bind);
	torture_suite_add_simple_test(suite, "authcontext", torture_bind_authcontext);
	torture_suite_add_suite(suite, torture_rpc_samba3(suite));
	torture_rpc_drsuapi_tcase(suite);
	torture_rpc_drsuapi_cracknames_tcase(suite);
	torture_suite_add_suite(suite, torture_rpc_dssetup(suite));
	torture_suite_add_suite(suite, torture_rpc_browser(suite));
	torture_suite_add_simple_test(suite, "altercontext", torture_rpc_alter_context);
	torture_suite_add_simple_test(suite, "join", torture_rpc_join);
	torture_drs_rpc_dsgetinfo_tcase(suite);
	torture_suite_add_simple_test(suite, "bench-rpc", torture_bench_rpc);
	torture_suite_add_simple_test(suite, "asyncbind", torture_async_bind);
	torture_suite_add_suite(suite, torture_rpc_ntsvcs(suite));
	torture_suite_add_suite(suite, torture_rpc_bind(suite));
	torture_suite_add_suite(suite, torture_rpc_backupkey(suite));

	suite->description = talloc_strdup(suite, "DCE/RPC protocol and interface tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
