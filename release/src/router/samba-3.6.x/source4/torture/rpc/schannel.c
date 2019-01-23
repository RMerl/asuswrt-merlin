/* 
   Unix SMB/CIFS implementation.

   test suite for schannel operations

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
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "auth/credentials/credentials.h"
#include "torture/rpc/torture_rpc.h"
#include "lib/cmdline/popt_common.h"
#include "../libcli/auth/schannel.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/security/security.h"
#include "system/filesys.h"
#include "param/param.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "auth/gensec/gensec.h"
#include "libcli/composite/composite.h"
#include "lib/events/events.h"

#define TEST_MACHINE_NAME "schannel"

/*
  try a netlogon SamLogon
*/
bool test_netlogon_ex_ops(struct dcerpc_pipe *p, struct torture_context *tctx, 
			  struct cli_credentials *credentials, 
			  struct netlogon_creds_CredentialState *creds)
{
	NTSTATUS status;
	struct netr_LogonSamLogonEx r;
	struct netr_NetworkInfo ninfo;
	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative = 0;
	uint32_t _flags = 0;
	DATA_BLOB names_blob, chal, lm_resp, nt_resp;
	int i;
	int flags = CLI_CRED_NTLM_AUTH;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (lpcfg_client_lanman_auth(tctx->lp_ctx)) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lpcfg_client_ntlmv2_auth(tctx->lp_ctx)) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	cli_credentials_get_ntlm_username_domain(cmdline_credentials, tctx, 
						 &ninfo.identity_info.account_name.string,
						 &ninfo.identity_info.domain_name.string);
	
	generate_random_buffer(ninfo.challenge, 
			       sizeof(ninfo.challenge));
	chal = data_blob_const(ninfo.challenge, 
			       sizeof(ninfo.challenge));

	names_blob = NTLMv2_generate_names_blob(tctx, cli_credentials_get_workstation(credentials), 
						cli_credentials_get_domain(credentials));

	status = cli_credentials_get_ntlm_response(cmdline_credentials, tctx, 
						   &flags, 
						   chal,
						   names_blob,
						   &lm_resp, &nt_resp,
						   NULL, NULL);
	torture_assert_ntstatus_ok(tctx, status, 
				   "cli_credentials_get_ntlm_response failed");

	ninfo.lm.data = lm_resp.data;
	ninfo.lm.length = lm_resp.length;

	ninfo.nt.data = nt_resp.data;
	ninfo.nt.length = nt_resp.length;

	ninfo.identity_info.parameter_control = 0;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.workstation.string = cli_credentials_get_workstation(credentials);

	logon.network = &ninfo;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.logon_level = 2;
	r.in.logon= &logon;
	r.in.flags = &_flags;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;
	r.out.flags = &_flags;

	torture_comment(tctx, 
			"Testing LogonSamLogonEx with name %s\n", 
			ninfo.identity_info.account_name.string);
	
	for (i=2;i<3;i++) {
		r.in.validation_level = i;
		
		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogonEx_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogon failed");
	}

	return true;
}

/*
  do some samr ops using the schannel connection
 */
static bool test_samr_ops(struct torture_context *tctx,
			  struct dcerpc_binding_handle *b)
{
	struct samr_GetDomPwInfo r;
	struct samr_PwInfo info;
	struct samr_Connect connect_r;
	struct samr_OpenDomain opendom;
	int i;
	struct lsa_String name;
	struct policy_handle handle;
	struct policy_handle domain_handle;

	name.string = lpcfg_workgroup(tctx->lp_ctx);
	r.in.domain_name = &name;
	r.out.info = &info;

	connect_r.in.system_name = 0;
	connect_r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	connect_r.out.connect_handle = &handle;
	
	printf("Testing Connect and OpenDomain on BUILTIN\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect_r(b, tctx, &connect_r),
		"Connect failed");
	if (!NT_STATUS_IS_OK(connect_r.out.result)) {
		if (NT_STATUS_EQUAL(connect_r.out.result, NT_STATUS_ACCESS_DENIED)) {
			printf("Connect failed (expected, schannel mapped to anonymous): %s\n",
			       nt_errstr(connect_r.out.result));
		} else {
			printf("Connect failed - %s\n", nt_errstr(connect_r.out.result));
			return false;
		}
	} else {
		opendom.in.connect_handle = &handle;
		opendom.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		opendom.in.sid = dom_sid_parse_talloc(tctx, "S-1-5-32");
		opendom.out.domain_handle = &domain_handle;
		
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenDomain_r(b, tctx, &opendom),
			"OpenDomain failed");
		if (!NT_STATUS_IS_OK(opendom.out.result)) {
			printf("OpenDomain failed - %s\n", nt_errstr(opendom.out.result));
			return false;
		}
	}

	printf("Testing GetDomPwInfo with name %s\n", r.in.domain_name->string);
	
	/* do several ops to test credential chaining */
	for (i=0;i<5;i++) {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &r),
			"GetDomPwInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_ACCESS_DENIED)) {
				printf("GetDomPwInfo op %d failed - %s\n", i, nt_errstr(r.out.result));
				return false;
			}
		}
	}

	return true;
}


/*
  do some lsa ops using the schannel connection
 */
static bool test_lsa_ops(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct lsa_GetUserName r;
	bool ret = true;
	struct lsa_String *account_name_p = NULL;
	struct lsa_String *authority_name_p = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	printf("\nTesting GetUserName\n");

	r.in.system_name = "\\";	
	r.in.account_name = &account_name_p;
	r.in.authority_name = &authority_name_p;
	r.out.account_name = &account_name_p;

	/* do several ops to test credential chaining and various operations */
	torture_assert_ntstatus_ok(tctx, dcerpc_lsa_GetUserName_r(b, tctx, &r),
		"lsa_GetUserName failed");

	authority_name_p = *r.out.authority_name;

	if (!NT_STATUS_IS_OK(r.out.result)) {
		printf("GetUserName failed - %s\n", nt_errstr(r.out.result));
		return false;
	} else {
		if (!r.out.account_name) {
			return false;
		}
		
		if (strcmp(account_name_p->string, "ANONYMOUS LOGON") != 0) {
			printf("GetUserName returned wrong user: %s, expected %s\n",
			       account_name_p->string, "ANONYMOUS LOGON");
			/* FIXME: gd */
			if (!torture_setting_bool(tctx, "samba3", false)) {
				return false;
			}
		}
		if (!authority_name_p || !authority_name_p->string) {
			return false;
		}
		
		if (strcmp(authority_name_p->string, "NT AUTHORITY") != 0) {
			printf("GetUserName returned wrong user: %s, expected %s\n",
			       authority_name_p->string, "NT AUTHORITY");
			/* FIXME: gd */
			if (!torture_setting_bool(tctx, "samba3", false)) {
				return false;
			}
		}
	}
	if (!test_many_LookupSids(p, tctx, NULL)) {
		printf("LsaLookupSids3 failed!\n");
		return false;
	}

	return ret;
}


/*
  test a schannel connection with the given flags
 */
static bool test_schannel(struct torture_context *tctx,
			  uint16_t acct_flags, uint32_t dcerpc_flags,
			  int i)
{
	struct test_join *join_ctx;
	NTSTATUS status;
	const char *binding = torture_setting_string(tctx, "binding", NULL);
	struct dcerpc_binding *b;
	struct dcerpc_pipe *p = NULL;
	struct dcerpc_pipe *p_netlogon = NULL;
	struct dcerpc_pipe *p_netlogon2 = NULL;
	struct dcerpc_pipe *p_netlogon3 = NULL;
	struct dcerpc_pipe *p_samr2 = NULL;
	struct dcerpc_pipe *p_lsa = NULL;
	struct netlogon_creds_CredentialState *creds;
	struct cli_credentials *credentials;

	join_ctx = torture_join_domain(tctx, 
				       talloc_asprintf(tctx, "%s%d", TEST_MACHINE_NAME, i), 
				       acct_flags, &credentials);
	torture_assert(tctx, join_ctx != NULL, "Failed to join domain");

	status = dcerpc_parse_binding(tctx, binding, &b);
	torture_assert_ntstatus_ok(tctx, status, "Bad binding string");

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= dcerpc_flags;

	status = dcerpc_pipe_connect_b(tctx, &p, b, &ndr_table_samr,
				       credentials, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, 
		"Failed to connect with schannel");

	torture_assert(tctx, test_samr_ops(tctx, p->binding_handle),
		       "Failed to process schannel secured SAMR ops");

	/* Also test that when we connect to the netlogon pipe, that
	 * the credentials we setup on the first pipe are valid for
	 * the second */

	/* Swap the binding details from SAMR to NETLOGON */
	status = dcerpc_epm_map_binding(tctx, b, &ndr_table_netlogon, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, "epm map");

	status = dcerpc_secondary_connection(p, &p_netlogon, 
					     b);
	torture_assert_ntstatus_ok(tctx, status, "seconday connection");

	status = dcerpc_bind_auth(p_netlogon, &ndr_table_netlogon, 
				  credentials, lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				  DCERPC_AUTH_TYPE_SCHANNEL,
				  dcerpc_auth_level(p->conn),
				  NULL);

	torture_assert_ntstatus_ok(tctx, status, "bind auth");

	status = dcerpc_schannel_creds(p_netlogon->conn->security_state.generic_state, tctx, &creds);
	torture_assert_ntstatus_ok(tctx, status, "schannel creds");

	/* do a couple of logins */
	torture_assert(tctx, test_netlogon_ops(p_netlogon, tctx, credentials, creds),
		"Failed to process schannel secured NETLOGON ops");

	torture_assert(tctx, test_netlogon_ex_ops(p_netlogon, tctx, credentials, creds),
		"Failed to process schannel secured NETLOGON EX ops");

	/* Swap the binding details from SAMR to LSARPC */
	status = dcerpc_epm_map_binding(tctx, b, &ndr_table_lsarpc, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, "epm map");

	status = dcerpc_secondary_connection(p, &p_lsa, 
					     b);

	torture_assert_ntstatus_ok(tctx, status, "seconday connection");

	status = dcerpc_bind_auth(p_lsa, &ndr_table_lsarpc,
				  credentials, lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				  DCERPC_AUTH_TYPE_SCHANNEL,
				  dcerpc_auth_level(p->conn),
				  NULL);

	torture_assert_ntstatus_ok(tctx, status, "bind auth");

	torture_assert(tctx, test_lsa_ops(tctx, p_lsa), 
		"Failed to process schannel secured LSA ops");

	/* Drop the socket, we want to start from scratch */
	talloc_free(p);
	p = NULL;

	/* Now see what we are still allowed to do */
	
	status = dcerpc_parse_binding(tctx, binding, &b);
	torture_assert_ntstatus_ok(tctx, status, "Bad binding string");

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= dcerpc_flags;

	status = dcerpc_pipe_connect_b(tctx, &p_samr2, b, &ndr_table_samr,
				       credentials, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, 
		"Failed to connect with schannel");

	/* do a some SAMR operations.  We have *not* done a new serverauthenticate */
	torture_assert (tctx, test_samr_ops(tctx, p_samr2->binding_handle),
			"Failed to process schannel secured SAMR ops (on fresh connection)");

	/* Swap the binding details from SAMR to NETLOGON */
	status = dcerpc_epm_map_binding(tctx, b, &ndr_table_netlogon, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, "epm");

	status = dcerpc_secondary_connection(p_samr2, &p_netlogon2, 
					     b);
	torture_assert_ntstatus_ok(tctx, status, "seconday connection");

	/* and now setup an SCHANNEL bind on netlogon */
	status = dcerpc_bind_auth(p_netlogon2, &ndr_table_netlogon,
				  credentials, lpcfg_gensec_settings(tctx, tctx->lp_ctx),
				  DCERPC_AUTH_TYPE_SCHANNEL,
				  dcerpc_auth_level(p_samr2->conn),
				  NULL);

	torture_assert_ntstatus_ok(tctx, status, "auth failed");
	
	/* Try the schannel-only SamLogonEx operation */
	torture_assert(tctx, test_netlogon_ex_ops(p_netlogon2, tctx, credentials, creds), 
		       "Failed to process schannel secured NETLOGON EX ops (on fresh connection)");
		

	/* And the more traditional style, proving that the
	 * credentials chaining state is fully present */
	torture_assert(tctx, test_netlogon_ops(p_netlogon2, tctx, credentials, creds),
			     "Failed to process schannel secured NETLOGON ops (on fresh connection)");

	/* Drop the socket, we want to start from scratch (again) */
	talloc_free(p_samr2);

	/* We don't want schannel for this test */
	b->flags &= ~DCERPC_AUTH_OPTIONS;

	status = dcerpc_pipe_connect_b(tctx, &p_netlogon3, b, &ndr_table_netlogon,
				       credentials, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, "Failed to connect without schannel");

	torture_assert(tctx, !test_netlogon_ex_ops(p_netlogon3, tctx, credentials, creds),
			"Processed NOT schannel secured NETLOGON EX ops without SCHANNEL (unsafe)");

	/* Required because the previous call will mark the current context as having failed */
	tctx->last_result = TORTURE_OK;
	tctx->last_reason = NULL;

	torture_assert(tctx, test_netlogon_ops(p_netlogon3, tctx, credentials, creds),
			"Failed to processed NOT schannel secured NETLOGON ops without new ServerAuth");

	torture_leave_domain(tctx, join_ctx);
	return true;
}



/*
  a schannel test suite
 */
bool torture_rpc_schannel(struct torture_context *torture)
{
	bool ret = true;
	struct {
		uint16_t acct_flags;
		uint32_t dcerpc_flags;
	} tests[] = {
		{ ACB_WSTRUST,   DCERPC_SCHANNEL | DCERPC_SIGN},
		{ ACB_WSTRUST,   DCERPC_SCHANNEL | DCERPC_SEAL},
		{ ACB_WSTRUST,   DCERPC_SCHANNEL | DCERPC_SIGN | DCERPC_SCHANNEL_128},
		{ ACB_WSTRUST,   DCERPC_SCHANNEL | DCERPC_SEAL | DCERPC_SCHANNEL_128 },
		{ ACB_SVRTRUST,  DCERPC_SCHANNEL | DCERPC_SIGN },
		{ ACB_SVRTRUST,  DCERPC_SCHANNEL | DCERPC_SEAL },
		{ ACB_SVRTRUST,  DCERPC_SCHANNEL | DCERPC_SIGN | DCERPC_SCHANNEL_128 },
		{ ACB_SVRTRUST,  DCERPC_SCHANNEL | DCERPC_SEAL | DCERPC_SCHANNEL_128 }
	};
	int i;

	for (i=0;i<ARRAY_SIZE(tests);i++) {
		if (!test_schannel(torture, 
				   tests[i].acct_flags, tests[i].dcerpc_flags,
				   i)) {
			torture_comment(torture, "Failed with acct_flags=0x%x dcerpc_flags=0x%x \n",
			       tests[i].acct_flags, tests[i].dcerpc_flags);
			ret = false;
		}
	}

	return ret;
}

/*
  test two schannel connections
 */
bool torture_rpc_schannel2(struct torture_context *torture)
{
	struct test_join *join_ctx;
	NTSTATUS status;
	const char *binding = torture_setting_string(torture, "binding", NULL);
	struct dcerpc_binding *b;
	struct dcerpc_pipe *p1 = NULL, *p2 = NULL;
	struct cli_credentials *credentials1, *credentials2;
	uint32_t dcerpc_flags = DCERPC_SCHANNEL | DCERPC_SIGN;

	join_ctx = torture_join_domain(torture, talloc_asprintf(torture, "%s2", TEST_MACHINE_NAME), 
				       ACB_WSTRUST, &credentials1);
	torture_assert(torture, join_ctx != NULL, 
		       "Failed to join domain with acct_flags=ACB_WSTRUST");

	credentials2 = (struct cli_credentials *)talloc_memdup(torture, credentials1, sizeof(*credentials1));
	credentials1->netlogon_creds = NULL;
	credentials2->netlogon_creds = NULL;

	status = dcerpc_parse_binding(torture, binding, &b);
	torture_assert_ntstatus_ok(torture, status, "Bad binding string");

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= dcerpc_flags;

	printf("Opening first connection\n");
	status = dcerpc_pipe_connect_b(torture, &p1, b, &ndr_table_netlogon,
				       credentials1, torture->ev, torture->lp_ctx);
	torture_assert_ntstatus_ok(torture, status, "Failed to connect with schannel");

	torture_comment(torture, "Opening second connection\n");
	status = dcerpc_pipe_connect_b(torture, &p2, b, &ndr_table_netlogon,
				       credentials2, torture->ev, torture->lp_ctx);
	torture_assert_ntstatus_ok(torture, status, "Failed to connect with schannel");

	credentials1->netlogon_creds = NULL;
	credentials2->netlogon_creds = NULL;

	torture_comment(torture, "Testing logon on pipe1\n");
	if (!test_netlogon_ex_ops(p1, torture, credentials1, NULL))
		return false;

	torture_comment(torture, "Testing logon on pipe2\n");
	if (!test_netlogon_ex_ops(p2, torture, credentials2, NULL))
		return false;

	torture_comment(torture, "Again on pipe1\n");
	if (!test_netlogon_ex_ops(p1, torture, credentials1, NULL))
		return false;

	torture_comment(torture, "Again on pipe2\n");
	if (!test_netlogon_ex_ops(p2, torture, credentials2, NULL))
		return false;

	torture_leave_domain(torture, join_ctx);
	return true;
}

struct torture_schannel_bench;

struct torture_schannel_bench_conn {
	struct torture_schannel_bench *s;
	int index;
	struct cli_credentials *wks_creds;
	struct dcerpc_pipe *pipe;
	struct netr_LogonSamLogonEx r;
	struct netr_NetworkInfo ninfo;
	TALLOC_CTX *tmp;
	uint64_t total;
	uint32_t count;
};

struct torture_schannel_bench {
	struct torture_context *tctx;
	bool progress;
	int timelimit;
	int nprocs;
	int nconns;
	struct torture_schannel_bench_conn *conns;
	struct test_join *join_ctx1;
	struct cli_credentials *wks_creds1;
	struct test_join *join_ctx2;
	struct cli_credentials *wks_creds2;
	struct cli_credentials *user1_creds;
	struct cli_credentials *user2_creds;
	struct dcerpc_binding *b;
	NTSTATUS error;
	uint64_t total;
	uint32_t count;
	bool stopped;
};

static void torture_schannel_bench_connected(struct composite_context *c)
{
	struct torture_schannel_bench_conn *conn =
		(struct torture_schannel_bench_conn *)c->async.private_data;
	struct torture_schannel_bench *s = talloc_get_type(conn->s,
					   struct torture_schannel_bench);

	s->error = dcerpc_pipe_connect_b_recv(c, s->conns, &conn->pipe);
	torture_comment(s->tctx, "conn[%u]: %s\n", conn->index, nt_errstr(s->error));
	if (NT_STATUS_IS_OK(s->error)) {
		s->nconns++;
	}
}

static void torture_schannel_bench_recv(struct tevent_req *subreq);

static bool torture_schannel_bench_start(struct torture_schannel_bench_conn *conn)
{
	struct torture_schannel_bench *s = conn->s;
	NTSTATUS status;
	DATA_BLOB names_blob, chal, lm_resp, nt_resp;
	int flags = CLI_CRED_NTLM_AUTH;
	struct tevent_req *subreq;
	struct cli_credentials *user_creds;

	if (conn->total % 2) {
		user_creds = s->user1_creds;
	} else {
		user_creds = s->user2_creds;
	}

	if (lpcfg_client_lanman_auth(s->tctx->lp_ctx)) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lpcfg_client_ntlmv2_auth(s->tctx->lp_ctx)) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	talloc_free(conn->tmp);
	conn->tmp = talloc_new(s);
	ZERO_STRUCT(conn->ninfo);
	ZERO_STRUCT(conn->r);

	cli_credentials_get_ntlm_username_domain(user_creds, conn->tmp,
						 &conn->ninfo.identity_info.account_name.string,
						 &conn->ninfo.identity_info.domain_name.string);

	generate_random_buffer(conn->ninfo.challenge,
			       sizeof(conn->ninfo.challenge));
	chal = data_blob_const(conn->ninfo.challenge,
			       sizeof(conn->ninfo.challenge));

	names_blob = NTLMv2_generate_names_blob(conn->tmp, 
						cli_credentials_get_workstation(conn->wks_creds),
						cli_credentials_get_domain(conn->wks_creds));

	status = cli_credentials_get_ntlm_response(user_creds, conn->tmp,
						   &flags,
						   chal,
						   names_blob,
						   &lm_resp, &nt_resp,
						   NULL, NULL);
	torture_assert_ntstatus_ok(s->tctx, status,
				   "cli_credentials_get_ntlm_response failed");

	conn->ninfo.lm.data = lm_resp.data;
	conn->ninfo.lm.length = lm_resp.length;

	conn->ninfo.nt.data = nt_resp.data;
	conn->ninfo.nt.length = nt_resp.length;

	conn->ninfo.identity_info.parameter_control = 0;
	conn->ninfo.identity_info.logon_id_low = 0;
	conn->ninfo.identity_info.logon_id_high = 0;
	conn->ninfo.identity_info.workstation.string = cli_credentials_get_workstation(conn->wks_creds);

	conn->r.in.server_name = talloc_asprintf(conn->tmp, "\\\\%s", dcerpc_server_name(conn->pipe));
	conn->r.in.computer_name = cli_credentials_get_workstation(conn->wks_creds);
	conn->r.in.logon_level = 2;
	conn->r.in.logon = talloc(conn->tmp, union netr_LogonLevel);
	conn->r.in.logon->network = &conn->ninfo;
	conn->r.in.flags = talloc(conn->tmp, uint32_t);
	conn->r.in.validation_level = 2;
	conn->r.out.validation = talloc(conn->tmp, union netr_Validation);
	conn->r.out.authoritative = talloc(conn->tmp, uint8_t);
	conn->r.out.flags = conn->r.in.flags;

	subreq = dcerpc_netr_LogonSamLogonEx_r_send(s, s->tctx->ev,
						    conn->pipe->binding_handle,
						    &conn->r);
	torture_assert(s->tctx, subreq, "Failed to setup LogonSamLogonEx request");

	tevent_req_set_callback(subreq, torture_schannel_bench_recv, conn);

	return true;
}

static void torture_schannel_bench_recv(struct tevent_req *subreq)
{
	bool ret;
	struct torture_schannel_bench_conn *conn =
		(struct torture_schannel_bench_conn *)tevent_req_callback_data_void(subreq);
	struct torture_schannel_bench *s = talloc_get_type(conn->s,
					   struct torture_schannel_bench);

	s->error = dcerpc_netr_LogonSamLogonEx_r_recv(subreq, subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(s->error)) {
		return;
	}

	conn->total++;
	conn->count++;

	if (s->stopped) {
		return;
	}

	ret = torture_schannel_bench_start(conn);
	if (!ret) {
		s->error = NT_STATUS_INTERNAL_ERROR;
	}
}

/*
  test multiple schannel connection in parallel
 */
bool torture_rpc_schannel_bench1(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	const char *binding = torture_setting_string(torture, "binding", NULL);
	struct torture_schannel_bench *s;
	struct timeval start;
	struct timeval end;
	int i;
	const char *tmp;

	s = talloc_zero(torture, struct torture_schannel_bench);
	s->tctx = torture;
	s->progress = torture_setting_bool(torture, "progress", true);
	s->timelimit = torture_setting_int(torture, "timelimit", 10);
	s->nprocs = torture_setting_int(torture, "nprocs", 4);
	s->conns = talloc_zero_array(s, struct torture_schannel_bench_conn, s->nprocs);

	s->user1_creds = (struct cli_credentials *)talloc_memdup(s,
								 cmdline_credentials,
								 sizeof(*s->user1_creds));
	tmp = torture_setting_string(s->tctx, "extra_user1", NULL);
	if (tmp) {
		cli_credentials_parse_string(s->user1_creds, tmp, CRED_SPECIFIED);
	}
	s->user2_creds = (struct cli_credentials *)talloc_memdup(s,
								 cmdline_credentials,
								 sizeof(*s->user1_creds));
	tmp = torture_setting_string(s->tctx, "extra_user2", NULL);
	if (tmp) {
		cli_credentials_parse_string(s->user1_creds, tmp, CRED_SPECIFIED);
	}

	s->join_ctx1 = torture_join_domain(s->tctx, talloc_asprintf(s, "%sb", TEST_MACHINE_NAME),
					   ACB_WSTRUST, &s->wks_creds1);
	torture_assert(torture, s->join_ctx1 != NULL,
		       "Failed to join domain with acct_flags=ACB_WSTRUST");
	s->join_ctx2 = torture_join_domain(s->tctx, talloc_asprintf(s, "%sc", TEST_MACHINE_NAME),
					   ACB_WSTRUST, &s->wks_creds2);
	torture_assert(torture, s->join_ctx2 != NULL,
		       "Failed to join domain with acct_flags=ACB_WSTRUST");

	cli_credentials_set_kerberos_state(s->wks_creds1, CRED_DONT_USE_KERBEROS);
	cli_credentials_set_kerberos_state(s->wks_creds2, CRED_DONT_USE_KERBEROS);

	for (i=0; i < s->nprocs; i++) {
		s->conns[i].s = s;
		s->conns[i].index = i;
		s->conns[i].wks_creds = (struct cli_credentials *)talloc_memdup(
			s->conns, s->wks_creds1,sizeof(*s->wks_creds1));
		if ((i % 2) && (torture_setting_bool(torture, "multijoin", false))) {
			memcpy(s->conns[i].wks_creds, s->wks_creds2,
			       talloc_get_size(s->conns[i].wks_creds));
		}
		s->conns[i].wks_creds->netlogon_creds = NULL;
	}

	status = dcerpc_parse_binding(s, binding, &s->b);
	torture_assert_ntstatus_ok(torture, status, "Bad binding string");
	s->b->flags &= ~DCERPC_AUTH_OPTIONS;
	s->b->flags |= DCERPC_SCHANNEL | DCERPC_SIGN;

	torture_comment(torture, "Opening %d connections in parallel\n", s->nprocs);
	for (i=0; i < s->nprocs; i++) {
#if 1
		s->error = dcerpc_pipe_connect_b(s->conns, &s->conns[i].pipe, s->b,
						 &ndr_table_netlogon,
						 s->conns[i].wks_creds,
						 torture->ev, torture->lp_ctx);
		torture_assert_ntstatus_ok(torture, s->error, "Failed to connect with schannel");
#else
		/*
		 * This path doesn't work against windows,
		 * because of windows drops the connections
		 * which haven't reached a session setup yet
		 *
		 * The same as the reset on zero vc stuff.
		 */
		struct composite_context *c;
		c = dcerpc_pipe_connect_b_send(s->conns, s->b,
					       &ndr_table_netlogon,
					       s->conns[i].wks_creds,
					       torture->ev,
					       torture->lp_ctx);
		torture_assert(torture, c != NULL, "Failed to setup connect");
		c->async.fn = torture_schannel_bench_connected;
		c->async.private_data = &s->conns[i];
	}

	while (NT_STATUS_IS_OK(s->error) && s->nprocs != s->nconns) {
		int ev_ret = event_loop_once(torture->ev);
		torture_assert(torture, ev_ret == 0, "event_loop_once failed");
#endif
	}
	torture_assert_ntstatus_ok(torture, s->error, "Failed establish a connect");

	/*
	 * Change the workstation password after establishing the netlogon
	 * schannel connections to prove that existing connections are not
	 * affected by a wks pwchange.
	 */

	{
		struct netr_ServerPasswordSet pwset;
		char *password = generate_random_password(s->join_ctx1, 8, 255);
		struct netlogon_creds_CredentialState *creds_state;
		struct dcerpc_pipe *net_pipe;
		struct netr_Authenticator credential, return_authenticator;
		struct samr_Password new_password;

		status = dcerpc_pipe_connect_b(s, &net_pipe, s->b,
					       &ndr_table_netlogon,
					       s->wks_creds1,
					       torture->ev, torture->lp_ctx);

		torture_assert_ntstatus_ok(torture, status,
					   "dcerpc_pipe_connect_b failed");

		pwset.in.server_name = talloc_asprintf(
			net_pipe, "\\\\%s", dcerpc_server_name(net_pipe));
		pwset.in.computer_name =
			cli_credentials_get_workstation(s->wks_creds1);
		pwset.in.account_name = talloc_asprintf(
			net_pipe, "%s$", pwset.in.computer_name);
		pwset.in.secure_channel_type = SEC_CHAN_WKSTA;
		pwset.in.credential = &credential;
		pwset.in.new_password = &new_password;
		pwset.out.return_authenticator = &return_authenticator;

		E_md4hash(password, new_password.hash);

		creds_state = cli_credentials_get_netlogon_creds(
			s->wks_creds1);
		netlogon_creds_des_encrypt(creds_state, &new_password);
		netlogon_creds_client_authenticator(creds_state, &credential);

		torture_assert_ntstatus_ok(torture, dcerpc_netr_ServerPasswordSet_r(net_pipe->binding_handle, torture, &pwset),
			"ServerPasswordSet failed");
		torture_assert_ntstatus_ok(torture, pwset.out.result,
					   "ServerPasswordSet failed");

		if (!netlogon_creds_client_check(creds_state,
					&pwset.out.return_authenticator->cred)) {
			printf("Credential chaining failed\n");
		}

		cli_credentials_set_password(s->wks_creds1, password,
					     CRED_SPECIFIED);

		talloc_free(net_pipe);

		/* Just as a test, connect with the new creds */

		talloc_free(s->wks_creds1->netlogon_creds);
		s->wks_creds1->netlogon_creds = NULL;

		status = dcerpc_pipe_connect_b(s, &net_pipe, s->b,
					       &ndr_table_netlogon,
					       s->wks_creds1,
					       torture->ev, torture->lp_ctx);

		torture_assert_ntstatus_ok(torture, status,
					   "dcerpc_pipe_connect_b failed");

		talloc_free(net_pipe);
	}

	torture_comment(torture, "Start looping LogonSamLogonEx on %d connections for %d secs\n",
			s->nprocs, s->timelimit);
	for (i=0; i < s->nprocs; i++) {
		ret = torture_schannel_bench_start(&s->conns[i]);
		torture_assert(torture, ret, "Failed to setup LogonSamLogonEx");
	}

	start = timeval_current();
	end = timeval_add(&start, s->timelimit, 0);

	while (NT_STATUS_IS_OK(s->error) && !timeval_expired(&end)) {
		int ev_ret = event_loop_once(torture->ev);
		torture_assert(torture, ev_ret == 0, "event_loop_once failed");
	}
	torture_assert_ntstatus_ok(torture, s->error, "Failed some request");
	s->stopped = true;
	talloc_free(s->conns);

	for (i=0; i < s->nprocs; i++) {
		s->total += s->conns[i].total;
	}

	torture_comment(torture,
			"Total ops[%llu] (%u ops/s)\n",
			(unsigned long long)s->total,
			(unsigned)s->total/s->timelimit);

	torture_leave_domain(torture, s->join_ctx1);
	torture_leave_domain(torture, s->join_ctx2);
	return true;
}
