/* 
   Unix SMB/CIFS implementation.

   local test for irpc code

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
#include "librpc/gen_ndr/ndr_echo.h"
#include "librpc/gen_ndr/ndr_echo_c.h"
#include "torture/torture.h"
#include "cluster/cluster.h"
#include "param/param.h"

const uint32_t MSG_ID1 = 1, MSG_ID2 = 2;

static bool test_debug;

struct irpc_test_data
{
	struct messaging_context *msg_ctx1, *msg_ctx2;
	struct tevent_context *ev;
};

/*
  serve up AddOne over the irpc system
*/
static NTSTATUS irpc_AddOne(struct irpc_message *irpc, struct echo_AddOne *r)
{
	*r->out.out_data = r->in.in_data + 1;
	if (test_debug) {
		printf("irpc_AddOne: in=%u in+1=%u out=%u\n", 
			r->in.in_data, r->in.in_data+1, *r->out.out_data);
	}
	return NT_STATUS_OK;
}

/*
  a deferred reply to echodata
*/
static void deferred_echodata(struct tevent_context *ev, struct tevent_timer *te, 
			      struct timeval t, void *private_data)
{
	struct irpc_message *irpc = talloc_get_type(private_data, struct irpc_message);
	struct echo_EchoData *r = (struct echo_EchoData *)irpc->data;
	r->out.out_data = (uint8_t *)talloc_memdup(r, r->in.in_data, r->in.len);
	if (r->out.out_data == NULL) {
		irpc_send_reply(irpc, NT_STATUS_NO_MEMORY);
	}
	printf("sending deferred reply\n");
	irpc_send_reply(irpc, NT_STATUS_OK);
}


/*
  serve up EchoData over the irpc system
*/
static NTSTATUS irpc_EchoData(struct irpc_message *irpc, struct echo_EchoData *r)
{
	irpc->defer_reply = true;
	event_add_timed(irpc->ev, irpc, timeval_zero(), deferred_echodata, irpc);
	return NT_STATUS_OK;
}


/*
  test a addone call over the internal messaging system
*/
static bool test_addone(struct torture_context *test, const void *_data,
			const void *_value)
{
	struct echo_AddOne r;
	NTSTATUS status;
	const struct irpc_test_data *data = (const struct irpc_test_data *)_data;
	uint32_t value = *(const uint32_t *)_value;
	struct dcerpc_binding_handle *irpc_handle;

	irpc_handle = irpc_binding_handle(test, data->msg_ctx1,
					  cluster_id(0, MSG_ID2),
					  &ndr_table_rpcecho);
	torture_assert(test, irpc_handle, "no memory");

	/* make the call */
	r.in.in_data = value;

	test_debug = true;
	status = dcerpc_echo_AddOne_r(irpc_handle, test, &r);
	test_debug = false;
	torture_assert_ntstatus_ok(test, status, "AddOne failed");

	/* check the answer */
	torture_assert(test, *r.out.out_data == r.in.in_data + 1, 
				   "AddOne wrong answer");

	torture_comment(test, "%u + 1 = %u\n", r.in.in_data, *r.out.out_data);
	return true;
}

/*
  test a echodata call over the internal messaging system
*/
static bool test_echodata(struct torture_context *tctx,
						  const void *tcase_data,
						  const void *test_data)
{
	struct echo_EchoData r;
	NTSTATUS status;
	const struct irpc_test_data *data = (const struct irpc_test_data *)tcase_data;
	TALLOC_CTX *mem_ctx = tctx;
	struct dcerpc_binding_handle *irpc_handle;

	irpc_handle = irpc_binding_handle(mem_ctx, data->msg_ctx1,
					  cluster_id(0, MSG_ID2),
					  &ndr_table_rpcecho);
	torture_assert(tctx, irpc_handle, "no memory");

	/* make the call */
	r.in.in_data = (unsigned char *)talloc_strdup(mem_ctx, "0123456789");
	r.in.len = strlen((char *)r.in.in_data);

	status = dcerpc_echo_EchoData_r(irpc_handle, mem_ctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "EchoData failed");

	/* check the answer */
	if (memcmp(r.out.out_data, r.in.in_data, r.in.len) != 0) {
		NDR_PRINT_OUT_DEBUG(echo_EchoData, &r);
		torture_fail(tctx, "EchoData wrong answer");
	}

	torture_comment(tctx, "Echo '%*.*s' -> '%*.*s'\n", 
	       r.in.len, r.in.len,
	       r.in.in_data,
	       r.in.len, r.in.len,
	       r.out.out_data);
	return true;
}

struct irpc_callback_state {
	struct echo_AddOne r;
	int *pong_count;
};

static void irpc_callback(struct tevent_req *subreq)
{
	struct irpc_callback_state *s =
		tevent_req_callback_data(subreq,
		struct irpc_callback_state);
	NTSTATUS status;

	status = dcerpc_echo_AddOne_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		printf("irpc call failed - %s\n", nt_errstr(status));
	}
	if (*s->r.out.out_data != s->r.in.in_data + 1) {
		printf("AddOne wrong answer - %u + 1 = %u should be %u\n", 
		       s->r.in.in_data, *s->r.out.out_data, s->r.in.in_data+1);
	}
	(*s->pong_count)++;
}

/*
  test echo speed
*/
static bool test_speed(struct torture_context *tctx,
					   const void *tcase_data,
					   const void *test_data)
{
	int ping_count = 0;
	int pong_count = 0;
	const struct irpc_test_data *data = (const struct irpc_test_data *)tcase_data;
	struct timeval tv;
	TALLOC_CTX *mem_ctx = tctx;
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	struct dcerpc_binding_handle *irpc_handle;

	irpc_handle = irpc_binding_handle(mem_ctx, data->msg_ctx1,
					  cluster_id(0, MSG_ID2),
					  &ndr_table_rpcecho);
	torture_assert(tctx, irpc_handle, "no memory");

	tv = timeval_current();

	torture_comment(tctx, "Sending echo for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		struct tevent_req *subreq;
		struct irpc_callback_state *s;

		s = talloc_zero(mem_ctx, struct irpc_callback_state);
		torture_assert(tctx, s != NULL, "no mem");

		s->pong_count = &pong_count;

		subreq = dcerpc_echo_AddOne_r_send(mem_ctx,
						   tctx->ev,
						   irpc_handle,
						   &s->r);
		torture_assert(tctx, subreq != NULL, "AddOne send failed");

		tevent_req_set_callback(subreq, irpc_callback, s);

		ping_count++;

		while (ping_count > pong_count + 20) {
			event_loop_once(data->ev);
		}
	}

	torture_comment(tctx, "waiting for %d remaining replies (done %d)\n", 
	       ping_count - pong_count, pong_count);
	while (timeval_elapsed(&tv) < 30 && pong_count < ping_count) {
		event_loop_once(data->ev);
	}

	torture_assert_int_equal(tctx, ping_count, pong_count, "ping test failed");

	torture_comment(tctx, "echo rate of %.0f messages/sec\n", 
	       (ping_count+pong_count)/timeval_elapsed(&tv));
	return true;
}


static bool irpc_setup(struct torture_context *tctx, void **_data)
{
	struct irpc_test_data *data;

	*_data = data = talloc(tctx, struct irpc_test_data);

	lpcfg_set_cmdline(tctx->lp_ctx, "pid directory", "piddir.tmp");

	data->ev = tctx->ev;
	torture_assert(tctx, data->msg_ctx1 = 
		       messaging_init(tctx, 
				      lpcfg_messaging_path(tctx, tctx->lp_ctx),
				      cluster_id(0, MSG_ID1),
				      data->ev),
		       "Failed to init first messaging context");

	torture_assert(tctx, data->msg_ctx2 = 
		       messaging_init(tctx, 
				      lpcfg_messaging_path(tctx, tctx->lp_ctx),
				      cluster_id(0, MSG_ID2), 
				      data->ev),
		       "Failed to init second messaging context");

	/* register the server side function */
	IRPC_REGISTER(data->msg_ctx1, rpcecho, ECHO_ADDONE, irpc_AddOne, NULL);
	IRPC_REGISTER(data->msg_ctx2, rpcecho, ECHO_ADDONE, irpc_AddOne, NULL);

	IRPC_REGISTER(data->msg_ctx1, rpcecho, ECHO_ECHODATA, irpc_EchoData, NULL);
	IRPC_REGISTER(data->msg_ctx2, rpcecho, ECHO_ECHODATA, irpc_EchoData, NULL);

	return true;
}

struct torture_suite *torture_local_irpc(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "irpc");
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "irpc");
	int i;
	uint32_t *values = talloc_array(tcase, uint32_t, 5);

	values[0] = 0;
	values[1] = 0x7FFFFFFE;
	values[2] = 0xFFFFFFFE;
	values[3] = 0xFFFFFFFF;
	values[4] = random() & 0xFFFFFFFF;

	tcase->setup = irpc_setup;

	for (i = 0; i < 5; i++) {
		torture_tcase_add_test_const(tcase, "addone", test_addone,
				(void *)&values[i]);
	}

	torture_tcase_add_test_const(tcase, "echodata", test_echodata, NULL);
	torture_tcase_add_test_const(tcase, "speed", test_speed, NULL);

	return suite;
}
