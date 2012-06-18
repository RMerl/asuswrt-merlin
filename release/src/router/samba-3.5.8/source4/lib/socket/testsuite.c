/* 
   Unix SMB/CIFS implementation.

   local testing of socket routines.

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
#include "lib/socket/socket.h"
#include "lib/events/events.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "torture/torture.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

/**
  basic testing of udp routines
*/
static bool test_udp(struct torture_context *tctx)
{
	struct socket_context *sock1, *sock2;
	NTSTATUS status;
	struct socket_address *srv_addr, *from_addr, *localhost;
	size_t size = 100 + (random() % 100);
	DATA_BLOB blob, blob2;
	size_t sent, nread;
	TALLOC_CTX *mem_ctx = tctx;
	struct interface *ifaces;

	load_interfaces(tctx, lp_interfaces(tctx->lp_ctx), &ifaces);

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &sock1, 0);
	torture_assert_ntstatus_ok(tctx, status, "creating DGRAM IP socket 1");
	talloc_steal(mem_ctx, sock1);

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &sock2, 0);
	torture_assert_ntstatus_ok(tctx, status, "creating DGRAM IP socket 1");
	talloc_steal(mem_ctx, sock2);

	localhost = socket_address_from_strings(sock1, sock1->backend_name, 
						iface_best_ip(ifaces, "127.0.0.1"), 0);

	torture_assert(tctx, localhost, "Localhost not found");

	status = socket_listen(sock1, localhost, 0, 0);
	torture_assert_ntstatus_ok(tctx, status, "listen on socket 1");

	srv_addr = socket_get_my_addr(sock1, mem_ctx);
	torture_assert(tctx, srv_addr != NULL && 
		       strcmp(srv_addr->addr, iface_best_ip(ifaces, "127.0.0.1")) == 0,
				   talloc_asprintf(tctx, 
		"Expected server address of %s but got %s",
		      iface_best_ip(ifaces, "127.0.0.1"), srv_addr ? srv_addr->addr : NULL));

	torture_comment(tctx, "server port is %d\n", srv_addr->port);

	blob  = data_blob_talloc(mem_ctx, NULL, size);
	blob2 = data_blob_talloc(mem_ctx, NULL, size);
	generate_random_buffer(blob.data, blob.length);

	sent = size;
	status = socket_sendto(sock2, &blob, &sent, srv_addr);
	torture_assert_ntstatus_ok(tctx, status, "sendto() on socket 2");

	status = socket_recvfrom(sock1, blob2.data, size, &nread, 
				 sock1, &from_addr);
	torture_assert_ntstatus_ok(tctx, status, "recvfrom() on socket 1");

	torture_assert_str_equal(tctx, from_addr->addr, srv_addr->addr, 
							 "different address");

	torture_assert_int_equal(tctx, nread, size, "Unexpected recvfrom size");

	torture_assert_mem_equal(tctx, blob2.data, blob.data, size,
		"Bad data in recvfrom");

	generate_random_buffer(blob.data, blob.length);
	status = socket_sendto(sock1, &blob, &sent, from_addr);
	torture_assert_ntstatus_ok(tctx, status, "sendto() on socket 1");

	status = socket_recvfrom(sock2, blob2.data, size, &nread, 
				 sock2, &from_addr);
	torture_assert_ntstatus_ok(tctx, status, "recvfrom() on socket 2");
	torture_assert_str_equal(tctx, from_addr->addr, srv_addr->addr, 
							 "Unexpected recvfrom addr");
	
	torture_assert_int_equal(tctx, nread, size, "Unexpected recvfrom size");

	torture_assert_int_equal(tctx, from_addr->port, srv_addr->port, 
				   "Unexpected recvfrom port");

	torture_assert_mem_equal(tctx, blob2.data, blob.data, size,
		"Bad data in recvfrom");

	talloc_free(sock1);
	talloc_free(sock2);
	return true;
}

/*
  basic testing of tcp routines
*/
static bool test_tcp(struct torture_context *tctx)
{
	struct socket_context *sock1, *sock2, *sock3;
	NTSTATUS status;
	struct socket_address *srv_addr, *from_addr, *localhost;
	size_t size = 100 + (random() % 100);
	DATA_BLOB blob, blob2;
	size_t sent, nread;
	TALLOC_CTX *mem_ctx = tctx;
	struct tevent_context *ev = tctx->ev;
	struct interface *ifaces;

	status = socket_create("ip", SOCKET_TYPE_STREAM, &sock1, 0);
	torture_assert_ntstatus_ok(tctx, status, "creating IP stream socket 1");
	talloc_steal(mem_ctx, sock1);

	status = socket_create("ip", SOCKET_TYPE_STREAM, &sock2, 0);
	torture_assert_ntstatus_ok(tctx, status, "creating IP stream socket 1");
	talloc_steal(mem_ctx, sock2);

	load_interfaces(tctx, lp_interfaces(tctx->lp_ctx), &ifaces);
	localhost = socket_address_from_strings(sock1, sock1->backend_name, 
						iface_best_ip(ifaces, "127.0.0.1"), 0);
	torture_assert(tctx, localhost, "Localhost not found");

	status = socket_listen(sock1, localhost, 0, 0);
	torture_assert_ntstatus_ok(tctx, status, "listen on socket 1");

	srv_addr = socket_get_my_addr(sock1, mem_ctx);
	torture_assert(tctx, srv_addr && srv_addr->addr, 
				   "Unexpected socket_get_my_addr NULL\n");

	torture_assert_str_equal(tctx, srv_addr->addr, iface_best_ip(ifaces, "127.0.0.1"), 
			"Unexpected server address");

	torture_comment(tctx, "server port is %d\n", srv_addr->port);

	status = socket_connect_ev(sock2, NULL, srv_addr, 0, ev);
	torture_assert_ntstatus_ok(tctx, status, "connect() on socket 2");

	status = socket_accept(sock1, &sock3);
	torture_assert_ntstatus_ok(tctx, status, "accept() on socket 1");
	talloc_steal(mem_ctx, sock3);
	talloc_free(sock1);

	blob  = data_blob_talloc(mem_ctx, NULL, size);
	blob2 = data_blob_talloc(mem_ctx, NULL, size);
	generate_random_buffer(blob.data, blob.length);

	sent = size;
	status = socket_send(sock2, &blob, &sent);
	torture_assert_ntstatus_ok(tctx, status, "send() on socket 2");

	status = socket_recv(sock3, blob2.data, size, &nread);
	torture_assert_ntstatus_ok(tctx, status, "recv() on socket 3");

	from_addr = socket_get_peer_addr(sock3, mem_ctx);

	torture_assert(tctx, from_addr && from_addr->addr, 
		"Unexpected recvfrom addr NULL");

	torture_assert_str_equal(tctx, from_addr->addr, srv_addr->addr, 
							 "Unexpected recvfrom addr");

	torture_assert_int_equal(tctx, nread, size, "Unexpected recvfrom size");

	torture_assert_mem_equal(tctx, blob2.data, blob.data, size, 
				   "Bad data in recv");
	return true;
}

struct torture_suite *torture_local_socket(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, 
													   "SOCKET");

	torture_suite_add_simple_test(suite, "udp", test_udp);
	torture_suite_add_simple_test(suite, "tcp", test_tcp);

	return suite;
}
