/* 
   Unix SMB/CIFS implementation.

   NBT WINS server testing

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
#include "lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "libcli/resolve/resolve.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "torture/torture.h"
#include "torture/nbt/proto.h"
#include "param/param.h"

#define CHECK_VALUE(tctx, v, correct) \
	torture_assert_int_equal(tctx, v, correct, "Incorrect value")

#define CHECK_STRING(tctx, v, correct) \
	torture_assert_casestr_equal(tctx, v, correct, "Incorrect value")

#define CHECK_NAME(tctx, _name, correct) do { \
	CHECK_STRING(tctx, (_name).name, (correct).name); \
	CHECK_VALUE(tctx, (uint8_t)(_name).type, (uint8_t)(correct).type); \
	CHECK_STRING(tctx, (_name).scope, (correct).scope); \
} while (0)


/*
  test operations against a WINS server
*/
static bool nbt_test_wins_name(struct torture_context *tctx, const char *address,
			       struct nbt_name *name, uint16_t nb_flags,
			       bool try_low_port,
			       uint8_t register_rcode)
{
	struct nbt_name_register_wins io;
	struct nbt_name_register name_register;
	struct nbt_name_query query;
	struct nbt_name_refresh_wins refresh;
	struct nbt_name_release release;
	struct nbt_name_request *req;
	NTSTATUS status;
	struct nbt_name_socket *nbtsock = torture_init_nbt_socket(tctx);
	const char *myaddress;
	struct socket_address *socket_address;
	struct interface *ifaces;
	bool low_port = try_low_port;

	load_interfaces(tctx, lpcfg_interfaces(tctx->lp_ctx), &ifaces);

	myaddress = talloc_strdup(tctx, iface_best_ip(ifaces, address));

	socket_address = socket_address_from_strings(tctx, 
						     nbtsock->sock->backend_name,
						     myaddress, lpcfg_nbt_port(tctx->lp_ctx));
	torture_assert(tctx, socket_address != NULL, 
				   "Error getting address");

	/* we do the listen here to ensure the WINS server receives the packets from
	   the right IP */
	status = socket_listen(nbtsock->sock, socket_address, 0, 0);
	talloc_free(socket_address);
	if (!NT_STATUS_IS_OK(status)) {
		low_port = false;
		socket_address = socket_address_from_strings(tctx,
							     nbtsock->sock->backend_name,
							     myaddress, 0);
		torture_assert(tctx, socket_address != NULL,
			       "Error getting address");

		status = socket_listen(nbtsock->sock, socket_address, 0, 0);
		talloc_free(socket_address);
		torture_assert_ntstatus_ok(tctx, status,
					   "socket_listen for WINS failed");
	}

	torture_comment(tctx, "Testing name registration to WINS with name %s at %s nb_flags=0x%x\n", 
	       nbt_name_string(tctx, name), myaddress, nb_flags);

	torture_comment(tctx, "release the name\n");
	release.in.name = *name;
	release.in.dest_port = lpcfg_nbt_port(tctx->lp_ctx);
	release.in.dest_addr = address;
	release.in.address = myaddress;
	release.in.nb_flags = nb_flags;
	release.in.broadcast = false;
	release.in.timeout = 3;
	release.in.retries = 0;

	status = nbt_name_release(nbtsock, tctx, &release);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "Bad response from %s for name query", address));
	CHECK_VALUE(tctx, release.out.rcode, 0);

	if (nb_flags & NBT_NM_GROUP) {
		/* ignore this for group names */
	} else if (!low_port) {
		torture_comment(tctx, "no low port - skip: register the name with a wrong address\n");
	} else {
		torture_comment(tctx, "register the name with a wrong address (makes the next request slow!)\n");
		io.in.name = *name;
		io.in.wins_port = lpcfg_nbt_port(tctx->lp_ctx);
		io.in.wins_servers = const_str_list(
			str_list_make_single(tctx, address));
		io.in.addresses = const_str_list(
			str_list_make_single(tctx, "127.64.64.1"));
		io.in.nb_flags = nb_flags;
		io.in.ttl = 300000;

		status = nbt_name_register_wins(nbtsock, tctx, &io);
		if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
			torture_assert_ntstatus_ok(tctx, status,
				talloc_asprintf(tctx, "No response from %s for name register\n",
						address));
		}
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "Bad response from %s for name register\n",
					address));

		CHECK_STRING(tctx, io.out.wins_server, address);
		CHECK_VALUE(tctx, io.out.rcode, 0);

		torture_comment(tctx, "register the name correct address\n");
		name_register.in.name		= *name;
		name_register.in.dest_port	= lpcfg_nbt_port(tctx->lp_ctx);
		name_register.in.dest_addr	= address;
		name_register.in.address	= myaddress;
		name_register.in.nb_flags	= nb_flags;
		name_register.in.register_demand= false;
		name_register.in.broadcast	= false;
		name_register.in.multi_homed	= true;
		name_register.in.ttl		= 300000;
		name_register.in.timeout	= 3;
		name_register.in.retries	= 2;

		/*
		 * test if the server ignores resent requests
		 */
		req = nbt_name_register_send(nbtsock, &name_register);
		while (true) {
			event_loop_once(nbtsock->event_ctx);
			if (req->state != NBT_REQUEST_WAIT) {
				break;
			}
			if (req->received_wack) {
				/*
				 * if we received the wack response
				 * we resend the request and the
				 * server should ignore that
				 * and not handle it as new request
				 */
				req->state = NBT_REQUEST_SEND;
				DLIST_ADD_END(nbtsock->send_queue, req,
					      struct nbt_name_request *);
				EVENT_FD_WRITEABLE(nbtsock->fde);
				break;
			}
		}

		status = nbt_name_register_recv(req, tctx, &name_register);
		if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
			torture_assert_ntstatus_ok(tctx, status,
				talloc_asprintf(tctx, "No response from %s for name register\n",
						address));
		}
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "Bad response from %s for name register\n",
					address));

		CHECK_VALUE(tctx, name_register.out.rcode, 0);
		CHECK_STRING(tctx, name_register.out.reply_addr, myaddress);
	}

	torture_comment(tctx, "register the name correct address\n");
	io.in.name = *name;
	io.in.wins_port = lpcfg_nbt_port(tctx->lp_ctx);
	io.in.wins_servers = (const char **)str_list_make_single(tctx, address);
	io.in.addresses = (const char **)str_list_make_single(tctx, myaddress);
	io.in.nb_flags = nb_flags;
	io.in.ttl = 300000;
	
	status = nbt_name_register_wins(nbtsock, tctx, &io);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "Bad response from %s for name register", address));
	
	CHECK_STRING(tctx, io.out.wins_server, address);
	CHECK_VALUE(tctx, io.out.rcode, register_rcode);

	if (register_rcode != NBT_RCODE_OK) {
		return true;
	}

	if (name->type != NBT_NAME_MASTER &&
	    name->type != NBT_NAME_LOGON && 
	    name->type != NBT_NAME_BROWSER && 
	    (nb_flags & NBT_NM_GROUP)) {
		torture_comment(tctx, "Try to register as non-group\n");
		io.in.nb_flags &= ~NBT_NM_GROUP;
		status = nbt_name_register_wins(nbtsock, tctx, &io);
		torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "Bad response from %s for name register\n",
			address));
		CHECK_VALUE(tctx, io.out.rcode, NBT_RCODE_ACT);
	}

	torture_comment(tctx, "query the name to make sure its there\n");
	query.in.name = *name;
	query.in.dest_addr = address;
	query.in.dest_port = lpcfg_nbt_port(tctx->lp_ctx);
	query.in.broadcast = false;
	query.in.wins_lookup = true;
	query.in.timeout = 3;
	query.in.retries = 0;

	status = nbt_name_query(nbtsock, tctx, &query);
	if (name->type == NBT_NAME_MASTER) {
		torture_assert_ntstatus_equal(
			  tctx, status, NT_STATUS_OBJECT_NAME_NOT_FOUND, 
			  talloc_asprintf(tctx, "Bad response from %s for name query", address));
		return true;
	}
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "Bad response from %s for name query", address));
	
	CHECK_NAME(tctx, query.out.name, *name);
	CHECK_VALUE(tctx, query.out.num_addrs, 1);
	if (name->type != NBT_NAME_LOGON &&
	    (nb_flags & NBT_NM_GROUP)) {
		CHECK_STRING(tctx, query.out.reply_addrs[0], "255.255.255.255");
	} else {
		CHECK_STRING(tctx, query.out.reply_addrs[0], myaddress);
	}


	query.in.name.name = strupper_talloc(tctx, name->name);
	if (query.in.name.name &&
	    strcmp(query.in.name.name, name->name) != 0) {
		torture_comment(tctx, "check case sensitivity\n");
		status = nbt_name_query(nbtsock, tctx, &query);
		torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OBJECT_NAME_NOT_FOUND, talloc_asprintf(tctx, "Bad response from %s for name query", address));
	}

	query.in.name = *name;
	if (name->scope) {
		query.in.name.scope = strupper_talloc(tctx, name->scope);
	}
	if (query.in.name.scope &&
	    strcmp(query.in.name.scope, name->scope) != 0) {
		torture_comment(tctx, "check case sensitivity on scope\n");
		status = nbt_name_query(nbtsock, tctx, &query);
		torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OBJECT_NAME_NOT_FOUND, talloc_asprintf(tctx, "Bad response from %s for name query", address));
	}

	torture_comment(tctx, "refresh the name\n");
	refresh.in.name = *name;
	refresh.in.wins_port = lpcfg_nbt_port(tctx->lp_ctx);
	refresh.in.wins_servers = (const char **)str_list_make_single(tctx, address);
	refresh.in.addresses = (const char **)str_list_make_single(tctx, myaddress);
	refresh.in.nb_flags = nb_flags;
	refresh.in.ttl = 12345;
	
	status = nbt_name_refresh_wins(nbtsock, tctx, &refresh);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "No response from %s for name refresh",
					address));
	}
	torture_assert_ntstatus_ok(tctx, status,
		talloc_asprintf(tctx, "Bad response from %s for name refresh",
				address));

	CHECK_STRING(tctx, refresh.out.wins_server, address);
	CHECK_VALUE(tctx, refresh.out.rcode, 0);

	printf("release the name\n");
	release.in.name = *name;
	release.in.dest_port = lpcfg_nbt_port(tctx->lp_ctx);
	release.in.dest_addr = address;
	release.in.address = myaddress;
	release.in.nb_flags = nb_flags;
	release.in.broadcast = false;
	release.in.timeout = 3;
	release.in.retries = 0;

	status = nbt_name_release(nbtsock, tctx, &release);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "No response from %s for name release",
					address));
	}
	torture_assert_ntstatus_ok(tctx, status,
		talloc_asprintf(tctx, "Bad response from %s for name release",
				address));

	CHECK_NAME(tctx, release.out.name, *name);
	CHECK_VALUE(tctx, release.out.rcode, 0);

	if (nb_flags & NBT_NM_GROUP) {
		/* ignore this for group names */
	} else if (!low_port) {
		torture_comment(tctx, "no low port - skip: register the name with a wrong address\n");
	} else {
		torture_comment(tctx, "register the name with a wrong address (makes the next request slow!)\n");
		io.in.name = *name;
		io.in.wins_port = lpcfg_nbt_port(tctx->lp_ctx);
		io.in.wins_servers = const_str_list(
			str_list_make_single(tctx, address));
		io.in.addresses = const_str_list(
			str_list_make_single(tctx, "127.64.64.1"));
		io.in.nb_flags = nb_flags;
		io.in.ttl = 300000;
	
		status = nbt_name_register_wins(nbtsock, tctx, &io);
		if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
			torture_assert_ntstatus_ok(tctx, status,
				talloc_asprintf(tctx, "No response from %s for name register\n",
						address));
		}
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "Bad response from %s for name register\n",
					address));

		CHECK_STRING(tctx, io.out.wins_server, address);
		CHECK_VALUE(tctx, io.out.rcode, 0);
	}

	torture_comment(tctx, "refresh the name with the correct address\n");
	refresh.in.name = *name;
	refresh.in.wins_port = lpcfg_nbt_port(tctx->lp_ctx);
	refresh.in.wins_servers = const_str_list(
			str_list_make_single(tctx, address));
	refresh.in.addresses = const_str_list(
			str_list_make_single(tctx, myaddress));
	refresh.in.nb_flags = nb_flags;
	refresh.in.ttl = 12345;

	status = nbt_name_refresh_wins(nbtsock, tctx, &refresh);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "No response from %s for name refresh",
					address));
	}
	torture_assert_ntstatus_ok(tctx, status,
		talloc_asprintf(tctx, "Bad response from %s for name refresh",
				address));

	CHECK_STRING(tctx, refresh.out.wins_server, address);
	CHECK_VALUE(tctx, refresh.out.rcode, 0);

	torture_comment(tctx, "release the name\n");
	release.in.name = *name;
	release.in.dest_port = lpcfg_nbt_port(tctx->lp_ctx);
	release.in.dest_addr = address;
	release.in.address = myaddress;
	release.in.nb_flags = nb_flags;
	release.in.broadcast = false;
	release.in.timeout = 3;
	release.in.retries = 0;

	status = nbt_name_release(nbtsock, tctx, &release);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "Bad response from %s for name query", address));
	
	CHECK_NAME(tctx, release.out.name, *name);
	CHECK_VALUE(tctx, release.out.rcode, 0);

	torture_comment(tctx, "release again\n");
	status = nbt_name_release(nbtsock, tctx, &release);
	torture_assert_ntstatus_ok(tctx, status, 
				talloc_asprintf(tctx, "Bad response from %s for name query",
		       address));
	
	CHECK_NAME(tctx, release.out.name, *name);
	CHECK_VALUE(tctx, release.out.rcode, 0);


	torture_comment(tctx, "query the name to make sure its gone\n");
	query.in.name = *name;
	status = nbt_name_query(nbtsock, tctx, &query);
	if (name->type != NBT_NAME_LOGON &&
	    (nb_flags & NBT_NM_GROUP)) {
		torture_assert_ntstatus_ok(tctx, status, 
				"ERROR: Name query failed after group release");
	} else {
		torture_assert_ntstatus_equal(tctx, status, 
									  NT_STATUS_OBJECT_NAME_NOT_FOUND,
				"Incorrect response to name query");
	}
	
	return true;
}


static char *test_nbt_wins_scope_string(TALLOC_CTX *mem_ctx, uint8_t count)
{
	char *res;
	uint8_t i;

	res = talloc_array(mem_ctx, char, count+1);
	if (res == NULL) {
		return NULL;
	}

	for (i=0; i < count; i++) {
		switch (i) {
		case 63:
		case 63 + 1 + 63:
		case 63 + 1 + 63 + 1 + 63:
			res[i] = '.';
			break;
		default:
			res[i] = '0' + (i%10);
			break;
		}
	}

	res[count] = '\0';

	talloc_set_name_const(res, res);

	return res;
}

/*
  test operations against a WINS server
*/
static bool nbt_test_wins(struct torture_context *tctx)
{
	struct nbt_name name;
	uint32_t r = (uint32_t)(random() % (100000));
	const char *address;
	bool ret = true;

	if (!torture_nbt_get_name(tctx, &name, &address))
		return false;

	name.name = talloc_asprintf(tctx, "_TORTURE-%5u", r);

	name.type = NBT_NAME_CLIENT;
	name.scope = NULL;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, true, NBT_RCODE_OK);

	name.type = NBT_NAME_MASTER;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H | NBT_NM_GROUP, false, NBT_RCODE_OK);

	name.type = NBT_NAME_SERVER;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, true, NBT_RCODE_OK);

	name.type = NBT_NAME_LOGON;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H | NBT_NM_GROUP, false, NBT_RCODE_OK);

	name.type = NBT_NAME_BROWSER;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H | NBT_NM_GROUP, false, NBT_RCODE_OK);

	name.type = NBT_NAME_PDC;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, true, NBT_RCODE_OK);

	name.type = 0xBF;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, true, NBT_RCODE_OK);

	name.type = 0xBE;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.scope = "example";
	name.type = 0x72;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, true, NBT_RCODE_OK);

	name.scope = "example";
	name.type = 0x71;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H | NBT_NM_GROUP, false, NBT_RCODE_OK);

	name.scope = "foo.example.com";
	name.type = 0x72;
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.name = talloc_asprintf(tctx, "_T\01-%5u.foo", r);
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.name = "";
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.name = talloc_asprintf(tctx, ".");
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.name = talloc_asprintf(tctx, "%5u-\377\200\300FOO", r);
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.scope = test_nbt_wins_scope_string(tctx, 237);
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_OK);

	name.scope = test_nbt_wins_scope_string(tctx, 238);
	ret &= nbt_test_wins_name(tctx, address, &name,
				  NBT_NODE_H, false, NBT_RCODE_SVR);

	return ret;
}

/*
  test WINS operations
*/
struct torture_suite *torture_nbt_wins(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "wins");

	torture_suite_add_simple_test(suite, "wins", nbt_test_wins);

	return suite;
}
