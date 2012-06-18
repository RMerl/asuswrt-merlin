/* 
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc notify operations

   Copyright (C) Jelmer Vernooij 2007
   
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
#include "system/filesys.h"
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/service_rpc.h"
#include "lib/events/events.h"
#include "smbd/process_model.h"
#include "smb_server/smb_server.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "lib/socket/netif.h"
#include "../lib/util/dlinklist.h"
#include "ntvfs/ntvfs.h"
#include "param/param.h"

static NTSTATUS spoolss__op_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
	return NT_STATUS_OK;
}

static void spoolss__op_unbind(struct dcesrv_connection_context *context, const struct dcesrv_interface *iface)
{
}

static NTSTATUS spoolss__op_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull, void **r)
{
	enum ndr_err_code ndr_err;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

	dce_call->fault_code = 0;

	if (opnum >= ndr_table_spoolss.num_calls) {
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	*r = talloc_size(mem_ctx, ndr_table_spoolss.calls[opnum].struct_size);
	NT_STATUS_HAVE_NO_MEMORY(*r);

        /* unravel the NDR for the packet */
	ndr_err = ndr_table_spoolss.calls[opnum].ndr_pull(pull, NDR_IN, *r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		dcerpc_log_packet(dce_call->conn->packet_log_dir,
						  &ndr_table_spoolss, opnum, NDR_IN,
				  &dce_call->pkt.u.request.stub_and_verifier);
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

/* Note that received_packets are allocated in talloc_autofree_context(), 
 * because no other context appears to stay around long enough. */
static struct received_packet {
	uint16_t opnum;
	void *r;
	struct received_packet *prev, *next;
} *received_packets = NULL;


static NTSTATUS spoolss__op_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	uint16_t opnum = dce_call->pkt.u.request.opnum;
	struct received_packet *rp;

	rp = talloc_zero(talloc_autofree_context(), struct received_packet);
	rp->opnum = opnum;
	rp->r = talloc_reference(rp, r);

	DLIST_ADD_END(received_packets, rp, struct received_packet *);

	switch (opnum) {
	case 58: {
		struct spoolss_ReplyOpenPrinter *r2 = (struct spoolss_ReplyOpenPrinter *)r;
		r2->out.result = WERR_OK;
		break;
	}

	default:
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		break;
	}

	if (dce_call->fault_code != 0) {
		dcerpc_log_packet(dce_call->conn->packet_log_dir,
						  &ndr_table_spoolss, opnum, NDR_IN,
				  &dce_call->pkt.u.request.stub_and_verifier);
		return NT_STATUS_NET_WRITE_FAULT;
	}
	return NT_STATUS_OK;
}


static NTSTATUS spoolss__op_reply(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}


static NTSTATUS spoolss__op_ndr_push(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_push *push, const void *r)
{
	enum ndr_err_code ndr_err;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

	ndr_err = ndr_table_spoolss.calls[opnum].ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

const static struct dcesrv_interface notify_test_spoolss_interface = {
	.name		= "spoolss",
	.syntax_id  = {{0x12345678,0x1234,0xabcd,{0xef,0x00},{0x01,0x23,0x45,0x67,0x89,0xab}},1.0},
	.bind		= spoolss__op_bind,
	.unbind		= spoolss__op_unbind,
	.ndr_pull	= spoolss__op_ndr_pull,
	.dispatch	= spoolss__op_dispatch,
	.reply		= spoolss__op_reply,
	.ndr_push	= spoolss__op_ndr_push
};

static bool spoolss__op_interface_by_uuid(struct dcesrv_interface *iface, const struct GUID *uuid, uint32_t if_version)
{
	if (notify_test_spoolss_interface.syntax_id.if_version == if_version &&
		GUID_equal(&notify_test_spoolss_interface.syntax_id.uuid, uuid)) {
		memcpy(iface,&notify_test_spoolss_interface, sizeof(*iface));
		return true;
	}

	return false;
}

static bool spoolss__op_interface_by_name(struct dcesrv_interface *iface, const char *name)
{
	if (strcmp(notify_test_spoolss_interface.name, name)==0) {
		memcpy(iface, &notify_test_spoolss_interface, sizeof(*iface));
		return true;
	}

	return false;	
}

static NTSTATUS spoolss__op_init_server(struct dcesrv_context *dce_ctx, const struct dcesrv_endpoint_server *ep_server)
{
	int i;

	for (i=0;i<ndr_table_spoolss.endpoints->count;i++) {
		NTSTATUS ret;
		const char *name = ndr_table_spoolss.endpoints->names[i];

		ret = dcesrv_interface_register(dce_ctx, name, &notify_test_spoolss_interface, NULL);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1,("spoolss_op_init_server: failed to register endpoint '%s'\n",name));
			return ret;
		}
	}

	return NT_STATUS_OK;
}

static bool test_RFFPCNEx(struct torture_context *tctx, 
			  struct dcerpc_pipe *p)
{
	struct spoolss_OpenPrinter q;
	struct spoolss_RemoteFindFirstPrinterChangeNotifyEx r;
	struct dcesrv_endpoint_server ep_server;
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	const char *endpoints[] = { "spoolss", NULL };
	struct dcesrv_endpoint *e;
	struct spoolss_NotifyOption t1;
	struct spoolss_ClosePrinter cp;

	struct policy_handle handle;
	const char *address;
	struct interface *ifaces;

	received_packets = NULL;

	ntvfs_init(tctx->lp_ctx);

	ZERO_STRUCT(q);

	q.in.printername	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	q.in.datatype		= NULL;
	q.in.devmode_ctr.devmode= NULL;
	q.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	q.out.handle		= &handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", q.in.printername);

	status = dcerpc_spoolss_OpenPrinter(p, tctx, &q);

	torture_assert_ntstatus_ok(tctx, status, "OpenPrinter failed");

	torture_assert_werr_ok(tctx, q.out.result, "OpenPrinter failed");

	/* Start DCE/RPC server */

	/* fill in our name */
	ep_server.name = "spoolss";

	/* fill in all the operations */
	ep_server.init_server = spoolss__op_init_server;

	ep_server.interface_by_uuid = spoolss__op_interface_by_uuid;
	ep_server.interface_by_name = spoolss__op_interface_by_name;

	torture_assert_ntstatus_ok(tctx, dcerpc_register_ep_server(&ep_server),
				  "unable to register spoolss server");

	lp_set_cmdline(tctx->lp_ctx, "dcerpc endpoint servers", "spoolss");

	load_interfaces(tctx, lp_interfaces(tctx->lp_ctx), &ifaces);
	address = iface_n_ip(ifaces, 0);
	torture_comment(tctx, "Listening for callbacks on %s\n", address);
	status = smbsrv_add_socket(p->conn->event_ctx, tctx->lp_ctx, &single_ops, address);
	torture_assert_ntstatus_ok(tctx, status, "starting smb server");

	status = dcesrv_init_context(tctx, tctx->lp_ctx, endpoints, &dce_ctx);
	torture_assert_ntstatus_ok(tctx, status, 
				   "unable to initialize DCE/RPC server");

	/* Make sure the directory for NCALRPC exists */
	if (!directory_exist(lp_ncalrpc_dir(tctx->lp_ctx))) {
		int ret;
		ret = mkdir(lp_ncalrpc_dir(tctx->lp_ctx), 0755);
		torture_assert(tctx, (ret == 0), talloc_asprintf(tctx,
			       "failed to mkdir(%s) ret[%d] errno[%d - %s]",
			       lp_ncalrpc_dir(tctx->lp_ctx), ret,
			       errno, strerror(errno)));
	}

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		status = dcesrv_add_ep(dce_ctx, tctx->lp_ctx,
				       e, tctx->ev, &single_ops);
		torture_assert_ntstatus_ok(tctx, status,
				"unable listen on dcerpc endpoint server");
	}

	r.in.flags = 0;
	r.in.local_machine = talloc_asprintf(tctx, "\\\\%s", address);
	r.in.options = 0;
	r.in.printer_local = 123;
	t1.version = 2;
	t1.flags = 0;
	t1.count = 2;
	t1.types = talloc_zero_array(tctx, struct spoolss_NotifyOptionType, 2);
	t1.types[0].type = PRINTER_NOTIFY_TYPE;
	t1.types[0].count = 1;
	t1.types[0].fields = talloc_array(t1.types, union spoolss_Field, 1);
	t1.types[0].fields[0].field = PRINTER_NOTIFY_FIELD_SERVER_NAME;

	t1.types[1].type = JOB_NOTIFY_TYPE;
	t1.types[1].count = 1;
	t1.types[1].fields = talloc_array(t1.types, union spoolss_Field, 1);
	t1.types[1].fields[0].field = PRINTER_NOTIFY_FIELD_PRINTER_NAME;

	r.in.notify_options = &t1;
	r.in.handle = &handle;

	status = dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "FFPCNEx failed");
	
	torture_assert_werr_ok(tctx, r.out.result, "error return code for FFPCNEx");

	cp.in.handle = &handle;
	cp.out.handle = &handle;

	torture_comment(tctx, "Testing ClosePrinter\n");

	status = dcerpc_spoolss_ClosePrinter(p, tctx, &cp);
	torture_assert_ntstatus_ok(tctx, status, "ClosePrinter failed");

	/* We should've had an incoming packet 58 (ReplyOpenPrinter) */
	torture_assert(tctx, received_packets != NULL, "no packets received");
	torture_assert_int_equal(tctx, received_packets->opnum, 58, "invalid opnum");

	/* Shut down DCE/RPC server */
	talloc_free(dce_ctx);

	return true;
}

/** Test that makes sure that calling ReplyOpenPrinter()
 * on Samba 4 will cause an irpc broadcast call.
 */
static bool test_ReplyOpenPrinter(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct spoolss_ReplyOpenPrinter r;
	struct spoolss_ReplyClosePrinter s;
	struct policy_handle h;

	r.in.server_name = "earth";
	r.in.printer_local = 2;
	r.in.type = REG_DWORD;
	r.in.bufsize = 0;
	r.in.buffer = NULL;
	r.out.handle = &h;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_ReplyOpenPrinter(p, tctx, &r),
			"spoolss_ReplyOpenPrinter call failed");

	torture_assert_werr_ok(tctx, r.out.result, "error return code");

	s.in.handle = &h;
	s.out.handle = &h;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_ReplyClosePrinter(p, tctx, &s),
			"spoolss_ReplyClosePrinter call failed");

	torture_assert_werr_ok(tctx, r.out.result, "error return code");

	return true;
}

struct torture_suite *torture_rpc_spoolss_notify(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "SPOOLSS-NOTIFY");
	
	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite, 
							"notify", &ndr_table_spoolss);

	torture_rpc_tcase_add_test(tcase, "testRFFPCNEx", test_RFFPCNEx);
	torture_rpc_tcase_add_test(tcase, "testReplyOpenPrinter", test_ReplyOpenPrinter);
	
	return suite;
}
