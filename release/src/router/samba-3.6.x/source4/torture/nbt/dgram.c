/* 
   Unix SMB/CIFS implementation.

   NBT dgram testing

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
#include "libcli/dgram/libdgram.h"
#include "lib/socket/socket.h"
#include "lib/events/events.h"
#include "torture/rpc/torture_rpc.h"
#include "libcli/resolve/resolve.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"

#define TEST_NAME "TORTURE_TEST"

/*
  reply handler for netlogon request
*/
static void netlogon_handler(struct dgram_mailslot_handler *dgmslot, 
			     struct nbt_dgram_packet *packet, 
			     struct socket_address *src)
{
	NTSTATUS status;
	struct nbt_netlogon_response *netlogon = dgmslot->private_data;

	dgmslot->private_data = netlogon = talloc(dgmslot, struct nbt_netlogon_response);

	if (!dgmslot->private_data) {
		return;
	}
       
	printf("netlogon reply from %s:%d\n", src->addr, src->port);

	/* Fills in the netlogon pointer */
	status = dgram_mailslot_netlogon_parse_response(dgmslot, netlogon, packet, netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to parse netlogon packet from %s:%d\n",
		       src->addr, src->port);
		return;
	}

}


/* test UDP/138 netlogon requests */
static bool nbt_test_netlogon(struct torture_context *tctx)
{
	struct dgram_mailslot_handler *dgmslot;
	struct nbt_dgram_socket *dgmsock = nbt_dgram_socket_init(tctx, tctx->ev);
	struct socket_address *dest;
	const char *myaddress;
	struct nbt_netlogon_packet logon;
	struct nbt_netlogon_response *response;
	struct nbt_name myname;
	NTSTATUS status;
	struct timeval tv = timeval_current();

	struct socket_address *socket_address;

	const char *address;
	struct nbt_name name;

	struct interface *ifaces;

	name.name = lpcfg_workgroup(tctx->lp_ctx);
	name.type = NBT_NAME_LOGON;
	name.scope = NULL;

	/* do an initial name resolution to find its IP */
	torture_assert_ntstatus_ok(tctx, 
				   resolve_name(lpcfg_resolve_context(tctx->lp_ctx), &name, tctx, &address, tctx->ev),
				   talloc_asprintf(tctx, "Failed to resolve %s", name.name));

	load_interfaces(tctx, lpcfg_interfaces(tctx->lp_ctx), &ifaces);
	myaddress = talloc_strdup(dgmsock, iface_best_ip(ifaces, address));


	socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
						     myaddress, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, socket_address != NULL, "Error getting address");

	/* try receiving replies on port 138 first, which will only
	   work if we are root and smbd/nmbd are not running - fall
	   back to listening on any port, which means replies from
	   most windows versions won't be seen */
	status = socket_listen(dgmsock->sock, socket_address, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(socket_address);
		socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
							     myaddress, 0);
		torture_assert(tctx, socket_address != NULL, "Error getting address");

		socket_listen(dgmsock->sock, socket_address, 0, 0);
	}

	/* setup a temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");

	ZERO_STRUCT(logon);
	logon.command = LOGON_PRIMARY_QUERY;
	logon.req.pdc.computer_name = TEST_NAME;
	logon.req.pdc.mailslot_name = dgmslot->mailslot_name;
	logon.req.pdc.unicode_name  = TEST_NAME;
	logon.req.pdc.nt_version    = 1;
	logon.req.pdc.lmnt_token    = 0xFFFF;
	logon.req.pdc.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, dest != NULL, "Error getting address");

	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");

	while (timeval_elapsed(&tv) < 5 && !dgmslot->private_data) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert(tctx, response->response_type == NETLOGON_GET_PDC, "Got incorrect type of netlogon response");
	torture_assert(tctx, response->data.get_pdc.command == NETLOGON_RESPONSE_FROM_PDC, "Got incorrect netlogon response command");

	return true;
}


/* test UDP/138 netlogon requests */
static bool nbt_test_netlogon2(struct torture_context *tctx)
{
	struct dgram_mailslot_handler *dgmslot;
	struct nbt_dgram_socket *dgmsock = nbt_dgram_socket_init(tctx, tctx->ev);
	struct socket_address *dest;
	const char *myaddress;
	struct nbt_netlogon_packet logon;
	struct nbt_netlogon_response *response;
	struct nbt_name myname;
	NTSTATUS status;
	struct timeval tv = timeval_current();

	struct socket_address *socket_address;

	const char *address;
	struct nbt_name name;

	struct interface *ifaces;
	struct test_join *join_ctx;
	struct cli_credentials *machine_credentials;
	const struct dom_sid *dom_sid;
	
	name.name = lpcfg_workgroup(tctx->lp_ctx);
	name.type = NBT_NAME_LOGON;
	name.scope = NULL;

	/* do an initial name resolution to find its IP */
	torture_assert_ntstatus_ok(tctx, 
				   resolve_name(lpcfg_resolve_context(tctx->lp_ctx), &name, tctx, &address, tctx->ev),
				   talloc_asprintf(tctx, "Failed to resolve %s", name.name));

	load_interfaces(tctx, lpcfg_interfaces(tctx->lp_ctx), &ifaces);
	myaddress = talloc_strdup(dgmsock, iface_best_ip(ifaces, address));

	socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
						     myaddress, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, socket_address != NULL, "Error getting address");

	/* try receiving replies on port 138 first, which will only
	   work if we are root and smbd/nmbd are not running - fall
	   back to listening on any port, which means replies from
	   some windows versions won't be seen */
	status = socket_listen(dgmsock->sock, socket_address, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(socket_address);
		socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
							     myaddress, 0);
		torture_assert(tctx, socket_address != NULL, "Error getting address");

		socket_listen(dgmsock->sock, socket_address, 0, 0);
	}

	/* setup a temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");

	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = "";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.nt_version    = NETLOGON_NT_VERSION_5EX_WITH_IP|NETLOGON_NT_VERSION_5|NETLOGON_NT_VERSION_1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));

	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");

	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX, "Got incorrect netlogon response command");
	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.nt_version, NETLOGON_NT_VERSION_5EX_WITH_IP|NETLOGON_NT_VERSION_5EX|NETLOGON_NT_VERSION_1, "Got incorrect netlogon response command");

	/* setup (another) temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");
	
	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));

	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");

	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN, "Got incorrect netlogon response command");

	torture_assert_str_equal(tctx, response->data.samlogon.data.nt5_ex.user_name, TEST_NAME"$", "Got incorrect user in netlogon response");

	join_ctx = torture_join_domain(tctx, TEST_NAME, 
				       ACB_WSTRUST, &machine_credentials);

	torture_assert(tctx, join_ctx != NULL,
		       talloc_asprintf(tctx, "Failed to join domain %s as %s\n",
				       lpcfg_workgroup(tctx->lp_ctx), TEST_NAME));

	dom_sid = torture_join_sid(join_ctx);

	/* setup (another) temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");
	
	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.sid           = *dom_sid;
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));

	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");


	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN, "Got incorrect netlogon response command");

	/* setup (another) temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error getting a Mailslot for GetDC reply");

	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.sid           = *dom_sid;
	logon.req.logon.acct_control  = ACB_WSTRUST;
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));

	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");


	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE, "Got incorrect netlogon response command");

	dgmslot->private_data = NULL;

	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.sid           = *dom_sid;
	logon.req.logon.acct_control  = ACB_NORMAL;
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));

	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, &name, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send netlogon request");


	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN, "Got incorrect netlogon response command");

	torture_leave_domain(tctx, join_ctx);
	return true;
}


/* test UDP/138 ntlogon requests */
static bool nbt_test_ntlogon(struct torture_context *tctx)
{
	struct dgram_mailslot_handler *dgmslot;
	struct nbt_dgram_socket *dgmsock = nbt_dgram_socket_init(tctx, tctx->ev);
	struct socket_address *dest;
	struct test_join *join_ctx;
	const struct dom_sid *dom_sid;
	struct cli_credentials *machine_credentials;

	const char *myaddress;
	struct nbt_netlogon_packet logon;
	struct nbt_netlogon_response *response;
	struct nbt_name myname;
	NTSTATUS status;
	struct timeval tv = timeval_current();

	struct socket_address *socket_address;
	const char *address;
	struct nbt_name name;

	struct interface *ifaces;
	
	name.name = lpcfg_workgroup(tctx->lp_ctx);
	name.type = NBT_NAME_LOGON;
	name.scope = NULL;

	/* do an initial name resolution to find its IP */
	torture_assert_ntstatus_ok(tctx, 
				   resolve_name(lpcfg_resolve_context(tctx->lp_ctx), &name, tctx, &address, tctx->ev),
				   talloc_asprintf(tctx, "Failed to resolve %s", name.name));

	load_interfaces(tctx, lpcfg_interfaces(tctx->lp_ctx), &ifaces);
	myaddress = talloc_strdup(dgmsock, iface_best_ip(ifaces, address));

	socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
						     myaddress, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, socket_address != NULL, "Error getting address");

	/* try receiving replies on port 138 first, which will only
	   work if we are root and smbd/nmbd are not running - fall
	   back to listening on any port, which means replies from
	   most windows versions won't be seen */
	status = socket_listen(dgmsock->sock, socket_address, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(socket_address);
		socket_address = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name,
							     myaddress, 0);
		torture_assert(tctx, socket_address != NULL, "Error getting address");

		socket_listen(dgmsock->sock, socket_address, 0, 0);
	}

	join_ctx = torture_join_domain(tctx, TEST_NAME, 
				       ACB_WSTRUST, &machine_credentials);
	dom_sid = torture_join_sid(join_ctx);

	torture_assert(tctx, join_ctx != NULL,
		       talloc_asprintf(tctx, "Failed to join domain %s as %s\n",
				       lpcfg_workgroup(tctx->lp_ctx), TEST_NAME));

	/* setup a temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");

	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.acct_control  = ACB_WSTRUST;
	/* Try with a SID this time */
	logon.req.logon.sid           = *dom_sid;
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, 
					      &name, dest, 
					      NBT_MAILSLOT_NTLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send ntlogon request");

	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE, "Got incorrect netlogon response command");

	torture_assert_str_equal(tctx, response->data.samlogon.data.nt5_ex.user_name, TEST_NAME"$", "Got incorrect user in netlogon response");


	/* setup a temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");

	ZERO_STRUCT(logon);
	logon.command = LOGON_SAM_LOGON_REQUEST;
	logon.req.logon.request_count = 0;
	logon.req.logon.computer_name = TEST_NAME;
	logon.req.logon.user_name     = TEST_NAME"$";
	logon.req.logon.mailslot_name = dgmslot->mailslot_name;
	logon.req.logon.acct_control  = ACB_WSTRUST;
	/* Leave sid as all zero */
	logon.req.logon.nt_version    = 1;
	logon.req.logon.lmnt_token    = 0xFFFF;
	logon.req.logon.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, 
					      &name, dest, 
					      NBT_MAILSLOT_NTLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send ntlogon request");

	while (timeval_elapsed(&tv) < 5 && dgmslot->private_data == NULL) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_SAMLOGON, "Got incorrect type of netlogon response");
	map_netlogon_samlogon_response(&response->data.samlogon);

	torture_assert_int_equal(tctx, response->data.samlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE, "Got incorrect netlogon response command");

	torture_assert_str_equal(tctx, response->data.samlogon.data.nt5_ex.user_name, TEST_NAME"$", "Got incorrect user in netlogon response");


	/* setup (another) temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");
	
	ZERO_STRUCT(logon);
	logon.command = LOGON_PRIMARY_QUERY;
	logon.req.pdc.computer_name = TEST_NAME;
	logon.req.pdc.mailslot_name = dgmslot->mailslot_name;
	logon.req.pdc.unicode_name  = TEST_NAME;
	logon.req.pdc.nt_version    = 1;
	logon.req.pdc.lmnt_token    = 0xFFFF;
	logon.req.pdc.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, 
					      &name, dest, 
					      NBT_MAILSLOT_NTLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send ntlogon request");

	while (timeval_elapsed(&tv) < 5 && !dgmslot->private_data) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_GET_PDC, "Got incorrect type of ntlogon response");
	torture_assert_int_equal(tctx, response->data.get_pdc.command, NETLOGON_RESPONSE_FROM_PDC, "Got incorrect ntlogon response command");

	torture_leave_domain(tctx, join_ctx);

	/* setup (another) temporary mailslot listener for replies */
	dgmslot = dgram_mailslot_temp(dgmsock, NBT_MAILSLOT_GETDC,
				      netlogon_handler, NULL);
	torture_assert(tctx, dgmslot != NULL, "Error temporary mailslot for GetDC");
	
	ZERO_STRUCT(logon);
	logon.command = LOGON_PRIMARY_QUERY;
	logon.req.pdc.computer_name = TEST_NAME;
	logon.req.pdc.mailslot_name = dgmslot->mailslot_name;
	logon.req.pdc.unicode_name  = TEST_NAME;
	logon.req.pdc.nt_version    = 1;
	logon.req.pdc.lmnt_token    = 0xFFFF;
	logon.req.pdc.lm20_token    = 0xFFFF;

	make_nbt_name_client(&myname, TEST_NAME);

	dest = socket_address_from_strings(dgmsock, dgmsock->sock->backend_name, 
					   address, lpcfg_dgram_port(tctx->lp_ctx));
	torture_assert(tctx, dest != NULL, "Error getting address");
	status = dgram_mailslot_netlogon_send(dgmsock, 
					      &name, dest, 
					      NBT_MAILSLOT_NTLOGON, 
					      &myname, &logon);
	torture_assert_ntstatus_ok(tctx, status, "Failed to send ntlogon request");

	while (timeval_elapsed(&tv) < 5 && !dgmslot->private_data) {
		event_loop_once(dgmsock->event_ctx);
	}

	response = talloc_get_type(dgmslot->private_data, struct nbt_netlogon_response);

	torture_assert(tctx, response != NULL, "Failed to receive a netlogon reply packet");

	torture_assert_int_equal(tctx, response->response_type, NETLOGON_GET_PDC, "Got incorrect type of ntlogon response");
	torture_assert_int_equal(tctx, response->data.get_pdc.command, NETLOGON_RESPONSE_FROM_PDC, "Got incorrect ntlogon response command");


	return true;
}


/*
  test nbt dgram operations
*/
struct torture_suite *torture_nbt_dgram(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dgram");

	torture_suite_add_simple_test(suite, "netlogon", nbt_test_netlogon);
	torture_suite_add_simple_test(suite, "netlogon2", nbt_test_netlogon2);
	torture_suite_add_simple_test(suite, "ntlogon", nbt_test_ntlogon);

	return suite;
}
