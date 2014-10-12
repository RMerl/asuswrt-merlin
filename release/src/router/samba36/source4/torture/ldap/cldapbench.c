/* 
   Unix SMB/CIFS implementation.

   CLDAP benchmark test

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/cldap/cldap.h"
#include "libcli/resolve/resolve.h"
#include "torture/torture.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"

#define CHECK_VAL(v, correct) torture_assert_int_equal(tctx, (v), (correct), "incorrect value");

struct bench_state {
	struct torture_context *tctx;
	int pass_count, fail_count;
};

static void request_netlogon_handler(struct tevent_req *req)
{
	struct cldap_netlogon io;
	struct bench_state *state = tevent_req_callback_data(req, struct bench_state);
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	io.in.version = 6;
	status = cldap_netlogon_recv(req, tmp_ctx, &io);
	talloc_free(req);
	if (NT_STATUS_IS_OK(status)) {
		state->pass_count++;
	} else {
		state->fail_count++;
	}
	talloc_free(tmp_ctx);
}

/*
  benchmark cldap netlogon calls
*/
static bool bench_cldap_netlogon(struct torture_context *tctx, const char *address)
{
	struct cldap_socket *cldap;
	int num_sent=0;
	struct timeval tv = timeval_current();
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	struct cldap_netlogon search;
	struct bench_state *state;
	NTSTATUS status;
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						address,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	status = cldap_socket_init(tctx, tctx->ev, NULL, dest_addr, &cldap);
	torture_assert_ntstatus_ok(tctx, status, "cldap_socket_init");

	state = talloc_zero(tctx, struct bench_state);
	state->tctx = tctx;

	ZERO_STRUCT(search);
	search.in.dest_address = NULL;
	search.in.dest_port = 0;
	search.in.acct_control = -1;
	search.in.version = 6;

	printf("Running CLDAP/netlogon for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		while (num_sent - (state->pass_count+state->fail_count) < 10) {
			struct tevent_req *req;
			req = cldap_netlogon_send(state, cldap, &search);

			tevent_req_set_callback(req, request_netlogon_handler, state);

			num_sent++;
			if (num_sent % 50 == 0) {
				if (torture_setting_bool(tctx, "progress", true)) {
					printf("%.1f queries per second (%d failures)  \r", 
					       state->pass_count / timeval_elapsed(&tv),
					       state->fail_count);
					fflush(stdout);
				}
			}
		}

		tevent_loop_once(tctx->ev);
	}

	while (num_sent != (state->pass_count + state->fail_count)) {
		tevent_loop_once(tctx->ev);
	}

	printf("%.1f queries per second (%d failures)  \n", 
	       state->pass_count / timeval_elapsed(&tv),
	       state->fail_count);

	talloc_free(cldap);
	return true;
}

static void request_rootdse_handler(struct tevent_req *req)
{
	struct cldap_search io;
	struct bench_state *state = tevent_req_callback_data(req, struct bench_state);
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	status = cldap_search_recv(req, tmp_ctx, &io);
	talloc_free(req);
	if (NT_STATUS_IS_OK(status)) {
		state->pass_count++;
	} else {
		state->fail_count++;
	}
	talloc_free(tmp_ctx);
}

/*
  benchmark cldap netlogon calls
*/
static bool bench_cldap_rootdse(struct torture_context *tctx, const char *address)
{
	struct cldap_socket *cldap;
	int num_sent=0;
	struct timeval tv = timeval_current();
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	struct cldap_search search;
	struct bench_state *state;
	NTSTATUS status;
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						address,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	/* cldap_socket_init should now know about the dest. address */
	status = cldap_socket_init(tctx, tctx->ev, NULL, dest_addr, &cldap);
	torture_assert_ntstatus_ok(tctx, status, "cldap_socket_init");

	state = talloc_zero(tctx, struct bench_state);

	ZERO_STRUCT(search);
	search.in.dest_address	= NULL;
	search.in.dest_port	= 0;
	search.in.filter	= "(objectClass=*)";
	search.in.timeout	= 2;
	search.in.retries	= 1;

	printf("Running CLDAP/rootdse for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		while (num_sent - (state->pass_count+state->fail_count) < 10) {
			struct tevent_req *req;
			req = cldap_search_send(state, cldap, &search);

			tevent_req_set_callback(req, request_rootdse_handler, state);

			num_sent++;
			if (num_sent % 50 == 0) {
				if (torture_setting_bool(tctx, "progress", true)) {
					printf("%.1f queries per second (%d failures)  \r",
					       state->pass_count / timeval_elapsed(&tv),
					       state->fail_count);
					fflush(stdout);
				}
			}
		}

		tevent_loop_once(tctx->ev);
	}

	while (num_sent != (state->pass_count + state->fail_count)) {
		tevent_loop_once(tctx->ev);
	}

	printf("%.1f queries per second (%d failures)  \n",
	       state->pass_count / timeval_elapsed(&tv),
	       state->fail_count);

	talloc_free(cldap);
	return true;
}

/*
  benchmark how fast a CLDAP server can respond to a series of parallel
  requests 
*/
bool torture_bench_cldap(struct torture_context *torture)
{
	const char *address;
	struct nbt_name name;
	NTSTATUS status;
	bool ret = true;
	
	make_nbt_name_server(&name, torture_setting_string(torture, "host", NULL));

	/* do an initial name resolution to find its IP */
	status = resolve_name(lpcfg_resolve_context(torture->lp_ctx), &name, torture, &address, torture->ev);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to resolve %s - %s\n",
		       name.name, nt_errstr(status));
		return false;
	}

	ret &= bench_cldap_netlogon(torture, address);
	ret &= bench_cldap_rootdse(torture, address);

	return ret;
}
