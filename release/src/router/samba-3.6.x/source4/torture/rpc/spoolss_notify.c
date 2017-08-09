/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc notify operations

   Copyright (C) Jelmer Vernooij 2007
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
#include "system/filesys.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "torture/rpc/torture_rpc.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/service_rpc.h"
#include "smbd/process_model.h"
#include "smb_server/smb_server.h"
#include "lib/socket/netif.h"
#include "ntvfs/ntvfs.h"
#include "param/param.h"

static NTSTATUS spoolss__op_bind(struct dcesrv_call_state *dce_call,
				 const struct dcesrv_interface *iface,
				 uint32_t if_version)
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

static WERROR _spoolss_ReplyOpenPrinter(struct dcesrv_call_state *dce_call,
					TALLOC_CTX *mem_ctx,
					struct spoolss_ReplyOpenPrinter *r)
{
	DEBUG(1,("_spoolss_ReplyOpenPrinter\n"));

	NDR_PRINT_IN_DEBUG(spoolss_ReplyOpenPrinter, r);

	r->out.handle = talloc(r, struct policy_handle);
	r->out.handle->handle_type = 42;
	r->out.handle->uuid = GUID_random();
	r->out.result = WERR_OK;

	NDR_PRINT_OUT_DEBUG(spoolss_ReplyOpenPrinter, r);

	return WERR_OK;
}

static WERROR _spoolss_ReplyClosePrinter(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct spoolss_ReplyClosePrinter *r)
{
	DEBUG(1,("_spoolss_ReplyClosePrinter\n"));

	NDR_PRINT_IN_DEBUG(spoolss_ReplyClosePrinter, r);

	ZERO_STRUCTP(r->out.handle);
	r->out.result = WERR_OK;

	NDR_PRINT_OUT_DEBUG(spoolss_ReplyClosePrinter, r);

	return WERR_OK;
}

static WERROR _spoolss_RouterReplyPrinterEx(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct spoolss_RouterReplyPrinterEx *r)
{
	DEBUG(1,("_spoolss_RouterReplyPrinterEx\n"));

	NDR_PRINT_IN_DEBUG(spoolss_RouterReplyPrinterEx, r);

	r->out.reply_result = talloc(r, uint32_t);
	*r->out.reply_result = 0;
	r->out.result = WERR_OK;

	NDR_PRINT_OUT_DEBUG(spoolss_RouterReplyPrinterEx, r);

	return WERR_OK;
}

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
		r2->out.result = _spoolss_ReplyOpenPrinter(dce_call, mem_ctx, r2);
		break;
	}
	case 60: {
		struct spoolss_ReplyClosePrinter *r2 = (struct spoolss_ReplyClosePrinter *)r;
		r2->out.result = _spoolss_ReplyClosePrinter(dce_call, mem_ctx, r2);
		break;
	}
	case 66: {
		struct spoolss_RouterReplyPrinterEx *r2 = (struct spoolss_RouterReplyPrinterEx *)r;
		r2->out.result = _spoolss_RouterReplyPrinterEx(dce_call, mem_ctx, r2);
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

static bool test_OpenPrinter(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     struct policy_handle *handle,
			     const char *name)
{
	struct spoolss_OpenPrinter r;
	const char *printername;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r);

	if (name) {
		printername	= talloc_asprintf(tctx, "\\\\%s\\%s", dcerpc_server_name(p), name);
	} else {
		printername	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	}

	r.in.printername	= printername;
	r.in.datatype		= NULL;
	r.in.devmode_ctr.devmode= NULL;
	r.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle		= handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", r.in.printername);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_OpenPrinter_r(b, tctx, &r),
		"OpenPrinter failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"OpenPrinter failed");

	return true;
}

static struct spoolss_NotifyOption *setup_printserver_NotifyOption(struct torture_context *tctx)
{
	struct spoolss_NotifyOption *o;

	o = talloc_zero(tctx, struct spoolss_NotifyOption);

	o->version = 2;
	o->flags = PRINTER_NOTIFY_OPTIONS_REFRESH;

	o->count = 2;
	o->types = talloc_zero_array(o, struct spoolss_NotifyOptionType, o->count);

	o->types[0].type = PRINTER_NOTIFY_TYPE;
	o->types[0].count = 1;
	o->types[0].fields = talloc_array(o->types, union spoolss_Field, o->types[0].count);
	o->types[0].fields[0].field = PRINTER_NOTIFY_FIELD_SERVER_NAME;

	o->types[1].type = JOB_NOTIFY_TYPE;
	o->types[1].count = 1;
	o->types[1].fields = talloc_array(o->types, union spoolss_Field, o->types[1].count);
	o->types[1].fields[0].field = JOB_NOTIFY_FIELD_MACHINE_NAME;

	return o;
}

static struct spoolss_NotifyOption *setup_printer_NotifyOption(struct torture_context *tctx)
{
	struct spoolss_NotifyOption *o;

	o = talloc_zero(tctx, struct spoolss_NotifyOption);

	o->version = 2;
	o->flags = PRINTER_NOTIFY_OPTIONS_REFRESH;

	o->count = 1;
	o->types = talloc_zero_array(o, struct spoolss_NotifyOptionType, o->count);

	o->types[0].type = PRINTER_NOTIFY_TYPE;
	o->types[0].count = 1;
	o->types[0].fields = talloc_array(o->types, union spoolss_Field, o->types[0].count);
	o->types[0].fields[0].field = PRINTER_NOTIFY_FIELD_COMMENT;

	return o;
}


static bool test_RemoteFindFirstPrinterChangeNotifyEx(struct torture_context *tctx,
						      struct dcerpc_binding_handle *b,
						      struct policy_handle *handle,
						      const char *address,
						      struct spoolss_NotifyOption *option)
{
	struct spoolss_RemoteFindFirstPrinterChangeNotifyEx r;
	const char *local_machine = talloc_asprintf(tctx, "\\\\%s", address);

	torture_comment(tctx, "Testing RemoteFindFirstPrinterChangeNotifyEx(%s)\n", local_machine);

	r.in.flags = 0;
	r.in.local_machine = local_machine;
	r.in.options = 0;
	r.in.printer_local = 0;
	r.in.notify_options = option;
	r.in.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_r(b, tctx, &r),
		"RemoteFindFirstPrinterChangeNotifyEx failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"error return code for RemoteFindFirstPrinterChangeNotifyEx");

	return true;
}

static bool test_RouterRefreshPrinterChangeNotify(struct torture_context *tctx,
						  struct dcerpc_binding_handle *b,
						  struct policy_handle *handle,
						  struct spoolss_NotifyOption *options)
{
	struct spoolss_RouterRefreshPrinterChangeNotify r;
	struct spoolss_NotifyInfo *info;

	torture_comment(tctx, "Testing RouterRefreshPrinterChangeNotify\n");

	r.in.handle = handle;
	r.in.change_low = 0;
	r.in.options = options;
	r.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_RouterRefreshPrinterChangeNotify_r(b, tctx, &r),
		"RouterRefreshPrinterChangeNotify failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"error return code for RouterRefreshPrinterChangeNotify");

	return true;
}

static bool test_SetPrinter(struct torture_context *tctx,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle)
{
	union spoolss_PrinterInfo info;
	struct spoolss_SetPrinter r;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	info2.servername	= info.info2.servername;
	info2.printername	= info.info2.printername;
	info2.sharename		= info.info2.sharename;
	info2.portname		= info.info2.portname;
	info2.drivername	= info.info2.drivername;
	info2.comment		= talloc_asprintf(tctx, "torture_comment %d\n", (int)time(NULL));
	info2.location		= info.info2.location;
	info2.devmode_ptr	= 0;
	info2.sepfile		= info.info2.sepfile;
	info2.printprocessor	= info.info2.printprocessor;
	info2.datatype		= info.info2.datatype;
	info2.parameters	= info.info2.parameters;
	info2.secdesc_ptr	= 0;
	info2.attributes	= info.info2.attributes;
	info2.priority		= info.info2.priority;
	info2.defaultpriority	= info.info2.defaultpriority;
	info2.starttime		= info.info2.starttime;
	info2.untiltime		= info.info2.untiltime;
	info2.status		= info.info2.status;
	info2.cjobs		= info.info2.cjobs;
	info2.averageppm	= info.info2.averageppm;

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	r.in.handle = handle;
	r.in.info_ctr = &info_ctr;
	r.in.devmode_ctr = &devmode_ctr;
	r.in.secdesc_ctr = &secdesc_ctr;
	r.in.command = 0;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter_r(b, tctx, &r), "SetPrinter failed");
	torture_assert_werr_ok(tctx, r.out.result, "SetPrinter failed");

	return true;
}

static bool test_start_dcerpc_server(struct torture_context *tctx,
				     struct tevent_context *event_ctx,
				     struct dcesrv_context **dce_ctx_p,
				     const char **address_p)
{
	struct dcesrv_endpoint_server ep_server;
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	const char *endpoints[] = { "spoolss", NULL };
	struct dcesrv_endpoint *e;
	const char *address;
	struct interface *ifaces;

	ntvfs_init(tctx->lp_ctx);

	/* fill in our name */
	ep_server.name = "spoolss";

	/* fill in all the operations */
	ep_server.init_server = spoolss__op_init_server;

	ep_server.interface_by_uuid = spoolss__op_interface_by_uuid;
	ep_server.interface_by_name = spoolss__op_interface_by_name;

	torture_assert_ntstatus_ok(tctx, dcerpc_register_ep_server(&ep_server),
				  "unable to register spoolss server");

	lpcfg_set_cmdline(tctx->lp_ctx, "dcerpc endpoint servers", "spoolss");

	load_interfaces(tctx, lpcfg_interfaces(tctx->lp_ctx), &ifaces);
	address = iface_n_ip(ifaces, 0);

	torture_comment(tctx, "Listening for callbacks on %s\n", address);

	status = process_model_init(tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status,
				   "unable to initialize process models");

	status = smbsrv_add_socket(tctx, event_ctx, tctx->lp_ctx, process_model_startup("single"), address);
	torture_assert_ntstatus_ok(tctx, status, "starting smb server");

	status = dcesrv_init_context(tctx, tctx->lp_ctx, endpoints, &dce_ctx);
	torture_assert_ntstatus_ok(tctx, status,
				   "unable to initialize DCE/RPC server");

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		status = dcesrv_add_ep(dce_ctx, tctx->lp_ctx,
				       e, tctx->ev, process_model_startup("single"));
		torture_assert_ntstatus_ok(tctx, status,
				"unable listen on dcerpc endpoint server");
	}

	*dce_ctx_p = dce_ctx;
	*address_p = address;

	return true;
}

static struct received_packet *last_packet(struct received_packet *p)
{
	struct received_packet *tmp;
	for (tmp = p; tmp->next; tmp = tmp->next) ;;
	return tmp;
}

static bool test_RFFPCNEx(struct torture_context *tctx,
			  struct dcerpc_pipe *p)
{
	struct dcesrv_context *dce_ctx;
	struct policy_handle handle;
	const char *address;
	struct received_packet *tmp;
	struct spoolss_NotifyOption *server_option = setup_printserver_NotifyOption(tctx);
#if 0
	struct spoolss_NotifyOption *printer_option = setup_printer_NotifyOption(tctx);
#endif
	struct dcerpc_binding_handle *b = p->binding_handle;

	received_packets = NULL;

	/* Start DCE/RPC server */
	torture_assert(tctx, test_start_dcerpc_server(tctx, p->conn->event_ctx, &dce_ctx, &address), "");

	torture_assert(tctx, test_OpenPrinter(tctx, p, &handle, NULL), "");
	torture_assert(tctx, test_RemoteFindFirstPrinterChangeNotifyEx(tctx, b, &handle, address, server_option), "");
	torture_assert(tctx, received_packets, "no packets received");
	torture_assert_int_equal(tctx, received_packets->opnum, NDR_SPOOLSS_REPLYOPENPRINTER,
		"no ReplyOpenPrinter packet after RemoteFindFirstPrinterChangeNotifyEx");
	torture_assert(tctx, test_RouterRefreshPrinterChangeNotify(tctx, b, &handle, NULL), "");
	torture_assert(tctx, test_RouterRefreshPrinterChangeNotify(tctx, b, &handle, server_option), "");
	torture_assert(tctx, test_ClosePrinter(tctx, b, &handle), "");
	tmp = last_packet(received_packets);
	torture_assert_int_equal(tctx, tmp->opnum, NDR_SPOOLSS_REPLYCLOSEPRINTER,
		"no ReplyClosePrinter packet after ClosePrinter");
#if 0
	torture_assert(tctx, test_OpenPrinter(tctx, p, &handle, "Epson AL-2600"), "");
	torture_assert(tctx, test_RemoteFindFirstPrinterChangeNotifyEx(tctx, p, &handle, address, printer_option), "");
	tmp = last_packet(received_packets);
	torture_assert_int_equal(tctx, tmp->opnum, NDR_SPOOLSS_REPLYOPENPRINTER,
		"no ReplyOpenPrinter packet after RemoteFindFirstPrinterChangeNotifyEx");
	torture_assert(tctx, test_RouterRefreshPrinterChangeNotify(tctx, p, &handle, NULL), "");
	torture_assert(tctx, test_RouterRefreshPrinterChangeNotify(tctx, p, &handle, printer_option), "");
	torture_assert(tctx, test_SetPrinter(tctx, p, &handle), "");
	tmp = last_packet(received_packets);
	torture_assert_int_equal(tctx, tmp->opnum, NDR_SPOOLSS_ROUTERREPLYPRINTEREX,
		"no RouterReplyPrinterEx packet after ClosePrinter");
	torture_assert(tctx, test_ClosePrinter(tctx, p, &handle), "");
	tmp = last_packet(received_packets);
	torture_assert_int_equal(tctx, tmp->opnum, NDR_SPOOLSS_REPLYCLOSEPRINTER,
		"no ReplyClosePrinter packet after ClosePrinter");
#endif
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
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skipping ReplyOpenPrinter server implementation test against s3\n");
	}

	r.in.server_name = "earth";
	r.in.printer_local = 2;
	r.in.type = REG_DWORD;
	r.in.bufsize = 0;
	r.in.buffer = NULL;
	r.out.handle = &h;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_ReplyOpenPrinter_r(b, tctx, &r),
			"spoolss_ReplyOpenPrinter call failed");

	torture_assert_werr_ok(tctx, r.out.result, "error return code");

	s.in.handle = &h;
	s.out.handle = &h;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_ReplyClosePrinter_r(b, tctx, &s),
			"spoolss_ReplyClosePrinter call failed");

	torture_assert_werr_ok(tctx, r.out.result, "error return code");

	return true;
}

struct torture_suite *torture_rpc_spoolss_notify(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "spoolss.notify");

	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite,
							"notify", &ndr_table_spoolss);

	torture_rpc_tcase_add_test(tcase, "testRFFPCNEx", test_RFFPCNEx);
	torture_rpc_tcase_add_test(tcase, "testReplyOpenPrinter", test_ReplyOpenPrinter);

	return suite;
}
