/*
   Unix SMB/CIFS implementation.

   Echo server service example

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

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
#include "echo_server/echo_server.h"
/* Get at the config file settings */
#include "param/param.h"
/* This defines task_server_terminate */
#include "smbd/process_model.h"
/* We get load_interfaces from here */
#include "socket/netif.h"
/* NTSTATUS-related stuff */
#include "libcli/util/ntstatus.h"
/* tsocket-related functions */
#include "lib/tsocket/tsocket.h"

/* Structure to hold an echo server socket */
struct echo_socket {
	/* This can come handy for the task struct in there */
	struct echo_server *echo;
	struct tsocket_address *local_address;
};

/* Structure to hold udp socket */
struct echo_udp_socket {
	struct echo_socket *echo_socket;
	struct tdgram_context *dgram;
	struct tevent_queue *send_queue;
};

/*
 * Main processing function.
 *
 * This is the start of the package processing.
 * In the echo server it doesn't do much, but for more complicated servers,
 * your code goes here (or at least is called from here.
 */
static NTSTATUS echo_process(struct echo_server *echo,
			     TALLOC_CTX *mem_ctx,
			     DATA_BLOB *in,
			     DATA_BLOB *out)
{
	uint8_t *buf = talloc_memdup(mem_ctx, in->data, in->length);
	NT_STATUS_HAVE_NO_MEMORY(buf);

	out->data = buf;
	out->length = in->length;

	return NT_STATUS_OK;
}

/* Structure keeping track of a single UDP echo server call */
struct echo_udp_call {
	/* The UDP packet came from here, our reply goes there as well */
	struct tsocket_address *src;
	DATA_BLOB in;
	DATA_BLOB out;
};

/** Prototype of the send callback */
static void echo_udp_call_sendto_done(struct tevent_req *subreq);

/* Callback to receive UDP packets */
static void echo_udp_call_loop(struct tevent_req *subreq)
{
	/*
	 * Our socket structure is the callback data. Get it in a
	 * type-safe way
	 */
	struct echo_udp_socket *sock = tevent_req_callback_data(subreq,
				       struct echo_udp_socket);
	struct echo_udp_call *call;
	uint8_t *buf;
	ssize_t len;
	NTSTATUS status;
	int sys_errno;

	call = talloc(sock, struct echo_udp_call);
	if (call == NULL) {
		goto done;
	}

	len = tdgram_recvfrom_recv(subreq, &sys_errno, call, &buf, &call->src);
	TALLOC_FREE(subreq);
	if (len == -1) {
		TALLOC_FREE(call);
		goto done;
	}

	call->in.data = buf;
	call->in.length = len;

	DEBUG(10, ("Received echo UDP packet of %lu bytes from %s\n",
		  (long)len, tsocket_address_string(call->src, call)));

	/* Handle the data coming in and compute the reply */
	status = echo_process(sock->echo_socket->echo, call,
			      &call->in, &call->out);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(call);
		DEBUG(0, ("echo_process returned %s\n",
			  nt_errstr(status)));
		goto done;
	}

	/* I said the task struct would come in handy. */
	subreq = tdgram_sendto_queue_send(call,
				sock->echo_socket->echo->task->event_ctx,
				sock->dgram,
				sock->send_queue,
				call->out.data,
				call->out.length,
				call->src);
	if (subreq == NULL) {
		TALLOC_FREE(call);
		goto done;
	}

	tevent_req_set_callback(subreq, echo_udp_call_sendto_done, call);

done:
	/* Now loop for the next incoming UDP packet, the async way */
	subreq = tdgram_recvfrom_send(sock,
				sock->echo_socket->echo->task->event_ctx,
				sock->dgram);
	if (subreq == NULL) {
		task_server_terminate(sock->echo_socket->echo->task,
				      "no memory for tdgram_recvfrom_send",
				      true);
		return;
	}
	tevent_req_set_callback(subreq, echo_udp_call_loop, sock);
}

/* Callback to send UDP replies */
static void echo_udp_call_sendto_done(struct tevent_req *subreq)
{
	struct echo_udp_call *call = tevent_req_callback_data(subreq,
				     struct echo_udp_call);
	ssize_t ret;
	int sys_errno;

	ret = tdgram_sendto_queue_recv(subreq, &sys_errno);

	/*
	 * We don't actually care about the error, just get on with our life.
	 * We already set a new echo_udp_call_loop callback already, so we're
	 * almost done, just some memory to free.
	 */
	TALLOC_FREE(call);
}

/* Start listening on a given address */
static NTSTATUS echo_add_socket(struct echo_server *echo,
				const struct model_ops *ops,
				const char *name,
				const char *address,
				uint16_t port)
{
	struct echo_socket *echo_socket;
	struct echo_udp_socket *echo_udp_socket;
	struct tevent_req *udpsubreq;
	NTSTATUS status;
	int ret;

	echo_socket = talloc(echo, struct echo_socket);
	NT_STATUS_HAVE_NO_MEMORY(echo_socket);

	echo_socket->echo = echo;

	/*
	 * Initialize the tsocket_address.
	 * The nifty part is the "ip" string. This tells tsocket to autodetect
	 * ipv4 or ipv6 based on the IP address string passed.
	 */
	ret = tsocket_address_inet_from_strings(echo_socket, "ip",
						address, port,
						&echo_socket->local_address);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	/* Now set up the udp socket */
	echo_udp_socket = talloc(echo_socket, struct echo_udp_socket);
	NT_STATUS_HAVE_NO_MEMORY(echo_udp_socket);

	echo_udp_socket->echo_socket = echo_socket;

	ret = tdgram_inet_udp_socket(echo_socket->local_address,
				     NULL,
				     echo_udp_socket,
				     &echo_udp_socket->dgram);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0, ("Failed to bind to %s:%u UDP - %s\n",
			  address, port, nt_errstr(status)));
		return status;
	}

	/*
	 * We set up a send queue so we can have multiple UDP packets in flight
	 */
	echo_udp_socket->send_queue = tevent_queue_create(echo_udp_socket,
							"echo_udp_send_queue");
	NT_STATUS_HAVE_NO_MEMORY(echo_udp_socket->send_queue);

	/*
	 * To handle the UDP requests, set up a new tevent request as a
	 * subrequest of the current one.
	 */
	udpsubreq = tdgram_recvfrom_send(echo_udp_socket,
					 echo->task->event_ctx,
					 echo_udp_socket->dgram);
	NT_STATUS_HAVE_NO_MEMORY(udpsubreq);
	tevent_req_set_callback(udpsubreq, echo_udp_call_loop, echo_udp_socket);

	return NT_STATUS_OK;
}

/* Set up the listening sockets */
static NTSTATUS echo_startup_interfaces(struct echo_server *echo,
					struct loadparm_context *lp_ctx,
					struct interface *ifaces)
{
	const struct model_ops *model_ops;
	int num_interfaces;
	TALLOC_CTX *tmp_ctx = talloc_new(echo);
	NTSTATUS status;
	int i;

	/*
	 * Samba allows subtask to set their own process model.
	 * Available models currently are:
	 * - onefork  (forks exactly one child process)
	 * - prefork  (keep a couple of child processes around)
	 * - single   (only run a single process)
	 * - standard (fork one subprocess per incoming connection)
	 * - thread   (use threads instead of forks)
	 *
	 * For the echo server, the "single" process model works fine,
	 * you probably don't want to use the thread model unless you really
	 * know what you're doing.
	 */

	model_ops = process_model_startup("single");
	if (model_ops == NULL) {
		DEBUG(0, ("Can't find 'single' proces model_ops\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	num_interfaces = iface_count(ifaces);

	for(i=0; i<num_interfaces; i++) {
		const char *address = talloc_strdup(tmp_ctx, iface_n_ip(ifaces, i));

		status = echo_add_socket(echo, model_ops, "echo", address, ECHO_SERVICE_PORT);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	TALLOC_FREE(tmp_ctx);
	return NT_STATUS_OK;
}


/* Do the basic task initialization, check if the task should run */

static void echo_task_init(struct task_server *task)
{
	struct interface *ifaces;
	struct echo_server *echo;
	NTSTATUS status;

	/*
	 * For the purpose of the example, let's only start the server in DC
	 * and standalone modes, and not as a member server.
	 */
	switch(lpcfg_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		/* Yes, we want to run the echo server */
		break;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "echo: Not starting echo server " \
				      "for domain members", false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want to run the echo server */
		break;
	}

	load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);

	if (iface_count(ifaces) == 0) {
		task_server_terminate(task,
				      "echo: No network interfaces configured",
				      false);
		return;
	}

	task_server_set_title(task, "task[echo]");

	echo = talloc_zero(task, struct echo_server);
	if (echo == NULL) {
		task_server_terminate(task, "echo: Out of memory", true);
		return;
	}

	echo->task = task;

	status = echo_startup_interfaces(echo, task->lp_ctx, ifaces);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "echo: Failed to set up interfaces",
				      true);
		return;
	}
}

/*
 * Register this server service with the main samba process.
 *
 * This is the function you need to put into the wscript_build file as
 * init_function. All the real work happens in "echo_task_init" above.
 */
NTSTATUS server_service_echo_init(void)
{
	return register_server_service("echo", echo_task_init);
}
