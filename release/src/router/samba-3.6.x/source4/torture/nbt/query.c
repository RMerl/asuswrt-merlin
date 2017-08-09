/* 
   Unix SMB/CIFS implementation.

   NBT name query testing

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
#include "lib/events/events.h"
#include "libcli/resolve/resolve.h"
#include "torture/torture.h"
#include "torture/nbt/proto.h"
#include "param/param.h"

struct result_struct {
	int num_pass;
	int num_fail;
};

static void increment_handler(struct nbt_name_request *req)
{
	struct result_struct *v = talloc_get_type(req->async.private_data, struct result_struct);
	if (req->state != NBT_REQUEST_DONE) {
		v->num_fail++;
	} else {
		v->num_pass++;
	}
	talloc_free(req);
}

/*
  benchmark simple name queries
*/
static bool bench_namequery(struct torture_context *tctx)
{
	struct nbt_name_socket *nbtsock = torture_init_nbt_socket(tctx);
	int num_sent=0;
	struct result_struct *result;
	struct nbt_name_query io;
	struct timeval tv = timeval_current();
	int timelimit = torture_setting_int(tctx, "timelimit", 5);

	const char *address;
	struct nbt_name name;

	if (!torture_nbt_get_name(tctx, &name, &address))
		return false;

	io.in.name = name;
	io.in.dest_addr = address;
	io.in.dest_port = lpcfg_nbt_port(tctx->lp_ctx);
	io.in.broadcast = false;
	io.in.wins_lookup = false;
	io.in.timeout = 1;

	result = talloc_zero(tctx, struct result_struct);

	torture_comment(tctx, "Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		while (num_sent - (result->num_pass+result->num_fail) < 10) {
			struct nbt_name_request *req;
			req = nbt_name_query_send(nbtsock, &io);
			torture_assert(tctx, req != NULL, "Failed to setup request!");
			req->async.fn = increment_handler;
			req->async.private_data = result;
			num_sent++;
			if (num_sent % 1000 == 0) {
				if (torture_setting_bool(tctx, "progress", true)) {
					torture_comment(tctx, "%.1f queries per second (%d failures)  \r", 
					       result->num_pass / timeval_elapsed(&tv),
					       result->num_fail);
					fflush(stdout);
				}
			}
		}

		event_loop_once(nbtsock->event_ctx);
	}

	while (num_sent != (result->num_pass + result->num_fail)) {
		event_loop_once(nbtsock->event_ctx);
	}

	torture_comment(tctx, "%.1f queries per second (%d failures)  \n", 
	       result->num_pass / timeval_elapsed(&tv),
	       result->num_fail);

	return true;
}


/*
  benchmark how fast a server can respond to name queries
*/
struct torture_suite *torture_bench_nbt(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "bench");
	torture_suite_add_simple_test(suite, "namequery", bench_namequery);

	return suite;
}
