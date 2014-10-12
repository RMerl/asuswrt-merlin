/* 
   Unix SMB/CIFS implementation.

   local test for messaging code

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
#include "lib/messaging/irpc.h"
#include "torture/torture.h"
#include "cluster/cluster.h"
#include "param/param.h"


static uint32_t msg_pong;

static void ping_message(struct messaging_context *msg, void *private_data,
			 uint32_t msg_type, struct server_id src, DATA_BLOB *data)
{
	NTSTATUS status;
	status = messaging_send(msg, src, msg_pong, data);
	if (!NT_STATUS_IS_OK(status)) {
		printf("pong failed - %s\n", nt_errstr(status));
	}
}

static void pong_message(struct messaging_context *msg, void *private_data,
			 uint32_t msg_type, struct server_id src, DATA_BLOB *data)
{
	int *count = (int *)private_data;
	(*count)++;
}

static void exit_message(struct messaging_context *msg, void *private_data,
			 uint32_t msg_type, struct server_id src, DATA_BLOB *data)
{
	talloc_free(private_data);
	exit(0);
}

/*
  test ping speed
*/
static bool test_ping_speed(struct torture_context *tctx)
{
	struct tevent_context *ev;
	struct messaging_context *msg_client_ctx;
	struct messaging_context *msg_server_ctx;
	int ping_count = 0;
	int pong_count = 0;
	struct timeval tv;
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	uint32_t msg_ping, msg_exit;

	lpcfg_set_cmdline(tctx->lp_ctx, "pid directory", "piddir.tmp");

	ev = tctx->ev;

	msg_server_ctx = messaging_init(tctx, 
					lpcfg_messaging_path(tctx, tctx->lp_ctx), cluster_id(0, 1),
					ev);
	
	torture_assert(tctx, msg_server_ctx != NULL, "Failed to init ping messaging context");
		
	messaging_register_tmp(msg_server_ctx, NULL, ping_message, &msg_ping);
	messaging_register_tmp(msg_server_ctx, tctx, exit_message, &msg_exit);

	msg_client_ctx = messaging_init(tctx, 
					lpcfg_messaging_path(tctx, tctx->lp_ctx),
					cluster_id(0, 2), 
					ev);

	torture_assert(tctx, msg_client_ctx != NULL, 
		       "msg_client_ctx messaging_init() failed");

	messaging_register_tmp(msg_client_ctx, &pong_count, pong_message, &msg_pong);

	tv = timeval_current();

	torture_comment(tctx, "Sending pings for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		DATA_BLOB data;
		NTSTATUS status1, status2;

		data.data = discard_const_p(uint8_t, "testing");
		data.length = strlen((const char *)data.data);

		status1 = messaging_send(msg_client_ctx, cluster_id(0, 1), msg_ping, &data);
		status2 = messaging_send(msg_client_ctx, cluster_id(0, 1), msg_ping, NULL);

		torture_assert_ntstatus_ok(tctx, status1, "msg1 failed");
		ping_count++;

		torture_assert_ntstatus_ok(tctx, status2, "msg2 failed");
		ping_count++;

		while (ping_count > pong_count + 20) {
			event_loop_once(ev);
		}
	}

	torture_comment(tctx, "waiting for %d remaining replies (done %d)\n", 
	       ping_count - pong_count, pong_count);
	while (timeval_elapsed(&tv) < 30 && pong_count < ping_count) {
		event_loop_once(ev);
	}

	torture_comment(tctx, "sending exit\n");
	messaging_send(msg_client_ctx, cluster_id(0, 1), msg_exit, NULL);

	torture_assert_int_equal(tctx, ping_count, pong_count, "ping test failed");

	torture_comment(tctx, "ping rate of %.0f messages/sec\n", 
	       (ping_count+pong_count)/timeval_elapsed(&tv));

	talloc_free(msg_client_ctx);
	talloc_free(msg_server_ctx);

	return true;
}

struct torture_suite *torture_local_messaging(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *s = torture_suite_create(mem_ctx, "messaging");
	torture_suite_add_simple_test(s, "ping_speed", test_ping_speed);
	return s;
}
