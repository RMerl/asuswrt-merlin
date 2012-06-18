/* 
   Unix SMB/CIFS implementation.

   KDC Server startup

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2008
   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005

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
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "smbd/service_stream.h"
#include "smbd/process_model.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "system/network.h"
#include "../lib/util/dlinklist.h"
#include "lib/messaging/irpc.h"
#include "lib/stream/packet.h"
#include "librpc/gen_ndr/samr.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "kdc/kdc.h"
#include "librpc/gen_ndr/ndr_misc.h"


/* Disgusting hack to get a mem_ctx and lp_ctx into the hdb plugin, when 
 * used as a keytab */
TALLOC_CTX *hdb_samba4_mem_ctx;
struct tevent_context *hdb_samba4_ev_ctx;
struct loadparm_context *hdb_samba4_lp_ctx;

/* hold all the info needed to send a reply */
struct kdc_reply {
	struct kdc_reply *next, *prev;
	struct socket_address *dest;
	DATA_BLOB packet;
};

typedef bool (*kdc_process_fn_t)(struct kdc_server *kdc,
				 TALLOC_CTX *mem_ctx, 
				 DATA_BLOB *input, 
				 DATA_BLOB *reply,
				 struct socket_address *peer_addr, 
				 struct socket_address *my_addr, 
				 int datagram);

/* hold information about one kdc socket */
struct kdc_socket {
	struct socket_context *sock;
	struct kdc_server *kdc;
	struct tevent_fd *fde;

	/* a queue of outgoing replies that have been deferred */
	struct kdc_reply *send_queue;

	kdc_process_fn_t process;
};
/*
  state of an open tcp connection
*/
struct kdc_tcp_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the kdc_server the connection belongs to */
	struct kdc_server *kdc;

	struct packet_context *packet;

	kdc_process_fn_t process;
};

/*
  handle fd send events on a KDC socket
*/
static void kdc_send_handler(struct kdc_socket *kdc_socket)
{
	while (kdc_socket->send_queue) {
		struct kdc_reply *rep = kdc_socket->send_queue;
		NTSTATUS status;
		size_t sendlen;

		status = socket_sendto(kdc_socket->sock, &rep->packet, &sendlen,
				       rep->dest);
		if (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
			break;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_BUFFER_SIZE)) {
			/* Replace with a krb err, response to big */
		}
		
		DLIST_REMOVE(kdc_socket->send_queue, rep);
		talloc_free(rep);
	}

	if (kdc_socket->send_queue == NULL) {
		EVENT_FD_NOT_WRITEABLE(kdc_socket->fde);
	}
}


/*
  handle fd recv events on a KDC socket
*/
static void kdc_recv_handler(struct kdc_socket *kdc_socket)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(kdc_socket);
	DATA_BLOB blob;
	struct kdc_reply *rep;
	DATA_BLOB reply;
	size_t nread, dsize;
	struct socket_address *src;
	struct socket_address *my_addr;
	int ret;

	status = socket_pending(kdc_socket->sock, &dsize);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	blob = data_blob_talloc(tmp_ctx, NULL, dsize);
	if (blob.data == NULL) {
		/* hope this is a temporary low memory condition */
		talloc_free(tmp_ctx);
		return;
	}

	status = socket_recvfrom(kdc_socket->sock, blob.data, blob.length, &nread,
				 tmp_ctx, &src);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}
	blob.length = nread;
	
	DEBUG(10,("Received krb5 UDP packet of length %lu from %s:%u\n", 
		 (long)blob.length, src->addr, (uint16_t)src->port));
	
	my_addr = socket_get_my_addr(kdc_socket->sock, tmp_ctx);
	if (!my_addr) {
		talloc_free(tmp_ctx);
		return;
	}


	/* Call krb5 */
	ret = kdc_socket->process(kdc_socket->kdc, 
				  tmp_ctx, 
				  &blob,  
				  &reply,
				  src, my_addr,
				  1 /* Datagram */);
	if (!ret) {
		talloc_free(tmp_ctx);
		return;
	}

	/* queue a pending reply */
	rep = talloc(kdc_socket, struct kdc_reply);
	if (rep == NULL) {
		talloc_free(tmp_ctx);
		return;
	}
	rep->dest         = talloc_steal(rep, src);
	rep->packet       = reply;
	talloc_steal(rep, reply.data);

	if (rep->packet.data == NULL) {
		talloc_free(rep);
		talloc_free(tmp_ctx);
		return;
	}

	DLIST_ADD_END(kdc_socket->send_queue, rep, struct kdc_reply *);
	EVENT_FD_WRITEABLE(kdc_socket->fde);
	talloc_free(tmp_ctx);
}

/*
  handle fd events on a KDC socket
*/
static void kdc_socket_handler(struct tevent_context *ev, struct tevent_fd *fde,
			       uint16_t flags, void *private_data)
{
	struct kdc_socket *kdc_socket = talloc_get_type(private_data, struct kdc_socket);
	if (flags & EVENT_FD_WRITE) {
		kdc_send_handler(kdc_socket);
	} 
	if (flags & EVENT_FD_READ) {
		kdc_recv_handler(kdc_socket);
	}
}

static void kdc_tcp_terminate_connection(struct kdc_tcp_connection *kdcconn, const char *reason)
{
	stream_terminate_connection(kdcconn->conn, reason);
}

/*
  receive a full packet on a KDC connection
*/
static NTSTATUS kdc_tcp_recv(void *private_data, DATA_BLOB blob)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(private_data,
							     struct kdc_tcp_connection);
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *tmp_ctx = talloc_new(kdcconn);
	int ret;
	DATA_BLOB input, reply;
	struct socket_address *src_addr;
	struct socket_address *my_addr;

	talloc_steal(tmp_ctx, blob.data);

	src_addr = socket_get_peer_addr(kdcconn->conn->socket, tmp_ctx);
	if (!src_addr) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	my_addr = socket_get_my_addr(kdcconn->conn->socket, tmp_ctx);
	if (!my_addr) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* Call krb5 */
	input = data_blob_const(blob.data + 4, blob.length - 4); 

	ret = kdcconn->process(kdcconn->kdc, 
			       tmp_ctx,
			       &input,
			       &reply,
			       src_addr,
			       my_addr,
			       0 /* Not datagram */);
	if (!ret) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* and now encode the reply */
	blob = data_blob_talloc(kdcconn, NULL, reply.length + 4);
	if (!blob.data) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	RSIVAL(blob.data, 0, reply.length);
	memcpy(blob.data + 4, reply.data, reply.length);	

	status = packet_send(kdcconn->packet, blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	/* the call isn't needed any more */
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/*
  receive some data on a KDC connection
*/
static void kdc_tcp_recv_handler(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private_data,
							     struct kdc_tcp_connection);
	packet_recv(kdcconn->packet);
}

/*
  called on a tcp recv error
*/
static void kdc_tcp_recv_error(void *private_data, NTSTATUS status)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(private_data,
					     struct kdc_tcp_connection);
	kdc_tcp_terminate_connection(kdcconn, nt_errstr(status));
}

/*
  called when we can write to a connection
*/
static void kdc_tcp_send(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private_data,
							     struct kdc_tcp_connection);
	packet_queue_run(kdcconn->packet);
}

/**
   Wrapper for krb5_kdc_process_krb5_request, converting to/from Samba
   calling conventions
*/

static bool kdc_process(struct kdc_server *kdc,
			TALLOC_CTX *mem_ctx, 
			DATA_BLOB *input, 
			DATA_BLOB *reply,
			struct socket_address *peer_addr, 
			struct socket_address *my_addr,
			int datagram_reply)
{
	int ret;	
	krb5_data k5_reply;
	krb5_data_zero(&k5_reply);

	krb5_kdc_update_time(NULL);

	DEBUG(10,("Received KDC packet of length %lu from %s:%d\n", 
		  (long)input->length - 4, peer_addr->addr, peer_addr->port));

	ret = krb5_kdc_process_krb5_request(kdc->smb_krb5_context->krb5_context, 
					    kdc->config,
					    input->data, input->length,
					    &k5_reply,
					    peer_addr->addr,
					    peer_addr->sockaddr,
					    datagram_reply);
	if (ret == -1) {
		*reply = data_blob(NULL, 0);
		return false;
	}
	if (k5_reply.length) {
		*reply = data_blob_talloc(mem_ctx, k5_reply.data, k5_reply.length);
		krb5_data_free(&k5_reply);
	} else {
		*reply = data_blob(NULL, 0);	
	}
	return true;
}

/*
  called when we get a new connection
*/
static void kdc_tcp_generic_accept(struct stream_connection *conn, kdc_process_fn_t process_fn)
{
	struct kdc_server *kdc = talloc_get_type(conn->private_data, struct kdc_server);
	struct kdc_tcp_connection *kdcconn;

	kdcconn = talloc_zero(conn, struct kdc_tcp_connection);
	if (!kdcconn) {
		stream_terminate_connection(conn, "kdc_tcp_accept: out of memory");
		return;
	}
	kdcconn->conn	 = conn;
	kdcconn->kdc	 = kdc;
	kdcconn->process = process_fn;
	conn->private_data    = kdcconn;

	kdcconn->packet = packet_init(kdcconn);
	if (kdcconn->packet == NULL) {
		kdc_tcp_terminate_connection(kdcconn, "kdc_tcp_accept: out of memory");
		return;
	}
	packet_set_private(kdcconn->packet, kdcconn);
	packet_set_socket(kdcconn->packet, conn->socket);
	packet_set_callback(kdcconn->packet, kdc_tcp_recv);
	packet_set_full_request(kdcconn->packet, packet_full_request_u32);
	packet_set_error_handler(kdcconn->packet, kdc_tcp_recv_error);
	packet_set_event_context(kdcconn->packet, conn->event.ctx);
	packet_set_fde(kdcconn->packet, conn->event.fde);
	packet_set_serialise(kdcconn->packet);
}

static void kdc_tcp_accept(struct stream_connection *conn)
{
	kdc_tcp_generic_accept(conn, kdc_process);
}

static const struct stream_server_ops kdc_tcp_stream_ops = {
	.name			= "kdc_tcp",
	.accept_connection	= kdc_tcp_accept,
	.recv_handler		= kdc_tcp_recv_handler,
	.send_handler		= kdc_tcp_send
};

static void kpasswdd_tcp_accept(struct stream_connection *conn)
{
	kdc_tcp_generic_accept(conn, kpasswdd_process);
}

static const struct stream_server_ops kpasswdd_tcp_stream_ops = {
	.name			= "kpasswdd_tcp",
	.accept_connection	= kpasswdd_tcp_accept,
	.recv_handler		= kdc_tcp_recv_handler,
	.send_handler		= kdc_tcp_send
};

/*
  start listening on the given address
*/
static NTSTATUS kdc_add_socket(struct kdc_server *kdc, const char *address,
			       uint16_t kdc_port, uint16_t kpasswd_port)
{
	const struct model_ops *model_ops;
 	struct kdc_socket *kdc_socket;
 	struct kdc_socket *kpasswd_socket;
	struct socket_address *kdc_address, *kpasswd_address;
	NTSTATUS status;

	kdc_socket = talloc(kdc, struct kdc_socket);
	NT_STATUS_HAVE_NO_MEMORY(kdc_socket);

	kpasswd_socket = talloc(kdc, struct kdc_socket);
	NT_STATUS_HAVE_NO_MEMORY(kpasswd_socket);

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &kdc_socket->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(kdc_socket);
		return status;
	}

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &kpasswd_socket->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(kpasswd_socket);
		return status;
	}

	kdc_socket->kdc = kdc;
	kdc_socket->send_queue = NULL;
	kdc_socket->process = kdc_process;

	talloc_steal(kdc_socket, kdc_socket->sock);

	kdc_socket->fde = event_add_fd(kdc->task->event_ctx, kdc, 
				       socket_get_fd(kdc_socket->sock), EVENT_FD_READ,
				       kdc_socket_handler, kdc_socket);

	kdc_address = socket_address_from_strings(kdc_socket, kdc_socket->sock->backend_name, 
						  address, kdc_port);
	NT_STATUS_HAVE_NO_MEMORY(kdc_address);

	status = socket_listen(kdc_socket->sock, kdc_address, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%d UDP for kdc - %s\n", 
			 address, kdc_port, nt_errstr(status)));
		talloc_free(kdc_socket);
		return status;
	}

	kpasswd_socket->kdc = kdc;
	kpasswd_socket->send_queue = NULL;
	kpasswd_socket->process = kpasswdd_process;

	talloc_steal(kpasswd_socket, kpasswd_socket->sock);

	kpasswd_socket->fde = event_add_fd(kdc->task->event_ctx, kdc, 
					   socket_get_fd(kpasswd_socket->sock), EVENT_FD_READ,
					   kdc_socket_handler, kpasswd_socket);
	
	kpasswd_address = socket_address_from_strings(kpasswd_socket, kpasswd_socket->sock->backend_name, 
						      address, kpasswd_port);
	NT_STATUS_HAVE_NO_MEMORY(kpasswd_address);

	status = socket_listen(kpasswd_socket->sock, kpasswd_address, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%d UDP for kpasswd - %s\n", 
			 address, kpasswd_port, nt_errstr(status)));
		talloc_free(kpasswd_socket);
		return status;
	}

	/* within the kdc task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup(kdc->task->event_ctx, "single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops\n"));
		talloc_free(kdc_socket);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = stream_setup_socket(kdc->task->event_ctx, 
				     kdc->task->lp_ctx,
				     model_ops, 
				     &kdc_tcp_stream_ops, 
				     "ip", address, &kdc_port, 
				     lp_socket_options(kdc->task->lp_ctx), 
				     kdc);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%u TCP - %s\n",
			 address, kdc_port, nt_errstr(status)));
		talloc_free(kdc_socket);
		return status;
	}

	status = stream_setup_socket(kdc->task->event_ctx, 
				     kdc->task->lp_ctx,
				     model_ops, 
				     &kpasswdd_tcp_stream_ops, 
				     "ip", address, &kpasswd_port, 
				     lp_socket_options(kdc->task->lp_ctx), 
				     kdc);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%u TCP - %s\n",
			 address, kpasswd_port, nt_errstr(status)));
		talloc_free(kdc_socket);
		return status;
	}

	return NT_STATUS_OK;
}


/*
  setup our listening sockets on the configured network interfaces
*/
static NTSTATUS kdc_startup_interfaces(struct kdc_server *kdc, struct loadparm_context *lp_ctx,
				       struct interface *ifaces)
{
	int num_interfaces;
	TALLOC_CTX *tmp_ctx = talloc_new(kdc);
	NTSTATUS status;
	int i;

	num_interfaces = iface_count(ifaces);
	
	for (i=0; i<num_interfaces; i++) {
		const char *address = talloc_strdup(tmp_ctx, iface_n_ip(ifaces, i));
		status = kdc_add_socket(kdc, address, lp_krb5_port(lp_ctx), 
					lp_kpasswd_port(lp_ctx));
		NT_STATUS_NOT_OK_RETURN(status);
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}


static NTSTATUS kdc_check_generic_kerberos(struct irpc_message *msg, 
				 struct kdc_check_generic_kerberos *r)
{
	struct PAC_Validate pac_validate;
	DATA_BLOB srv_sig;
	struct PAC_SIGNATURE_DATA kdc_sig;
	struct kdc_server *kdc = talloc_get_type(msg->private_data, struct kdc_server);
	enum ndr_err_code ndr_err;
	krb5_enctype etype;
	int ret;
	hdb_entry_ex ent;
	krb5_principal principal;
	krb5_keyblock keyblock;
	Key *key;

	/* There is no reply to this request */
	r->out.generic_reply = data_blob(NULL, 0);

	ndr_err = ndr_pull_struct_blob(&r->in.generic_request, msg, 
				       lp_iconv_convenience(kdc->task->lp_ctx), 
				       &pac_validate,
				       (ndr_pull_flags_fn_t)ndr_pull_PAC_Validate);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	if (pac_validate.MessageType != 3) {
		/* We don't implement any other message types - such as certificate validation - yet */
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (pac_validate.ChecksumAndSignature.length != (pac_validate.ChecksumLength + pac_validate.SignatureLength)
	    || pac_validate.ChecksumAndSignature.length < pac_validate.ChecksumLength
	    || pac_validate.ChecksumAndSignature.length < pac_validate.SignatureLength ) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	srv_sig = data_blob_const(pac_validate.ChecksumAndSignature.data, 
				  pac_validate.ChecksumLength);
	
	if (pac_validate.SignatureType == CKSUMTYPE_HMAC_MD5) {
		etype = ETYPE_ARCFOUR_HMAC_MD5;
	} else {
		ret = krb5_cksumtype_to_enctype(kdc->smb_krb5_context->krb5_context, pac_validate.SignatureType,
						&etype);
		if (ret != 0) {
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	ret = krb5_make_principal(kdc->smb_krb5_context->krb5_context, &principal, 
				  lp_realm(kdc->task->lp_ctx), 
				  "krbtgt", lp_realm(kdc->task->lp_ctx), 
				  NULL);

	if (ret != 0) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = kdc->config->db[0]->hdb_fetch(kdc->smb_krb5_context->krb5_context, 
					    kdc->config->db[0],
					    principal,
					    HDB_F_GET_KRBTGT | HDB_F_DECRYPT,
					    &ent);

	if (ret != 0) {
		hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
		krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);
	
		return NT_STATUS_LOGON_FAILURE;
	}
	
	ret = hdb_enctype2key(kdc->smb_krb5_context->krb5_context, &ent.entry, etype, &key);

	if (ret != 0) {
		hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
		krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);
		return NT_STATUS_LOGON_FAILURE;
	}

	keyblock = key->key;
	
	kdc_sig.type = pac_validate.SignatureType;
	kdc_sig.signature = data_blob_const(&pac_validate.ChecksumAndSignature.data[pac_validate.ChecksumLength],
					    pac_validate.SignatureLength);
	ret = check_pac_checksum(msg, srv_sig, &kdc_sig, 
			   kdc->smb_krb5_context->krb5_context, &keyblock);

	hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
	krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);

	if (ret != 0) {
		return NT_STATUS_LOGON_FAILURE;
	}
	
	return NT_STATUS_OK;
}


/*
  startup the kdc task
*/
static void kdc_task_init(struct task_server *task)
{
	struct kdc_server *kdc;
	NTSTATUS status;
	krb5_error_code ret;
	struct interface *ifaces;

	switch (lp_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "kdc: no KDC required in standalone configuration", false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "kdc: no KDC required in member server configuration", false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want a KDC */
		break;
	}

	load_interfaces(task, lp_interfaces(task->lp_ctx), &ifaces);

	if (iface_count(ifaces) == 0) {
		task_server_terminate(task, "kdc: no network interfaces configured", false);
		return;
	}

	task_server_set_title(task, "task[kdc]");

	kdc = talloc(task, struct kdc_server);
	if (kdc == NULL) {
		task_server_terminate(task, "kdc: out of memory", true);
		return;
	}

	kdc->task = task;

	initialize_krb5_error_table();

	ret = smb_krb5_init_context(kdc, task->event_ctx, task->lp_ctx, &kdc->smb_krb5_context);
	if (ret) {
		DEBUG(1,("kdc_task_init: krb5_init_context failed (%s)\n", 
			 error_message(ret)));
		task_server_terminate(task, "kdc: krb5_init_context failed", true);
		return; 
	}

	krb5_add_et_list(kdc->smb_krb5_context->krb5_context, initialize_hdb_error_table_r);

	ret = krb5_kdc_get_config(kdc->smb_krb5_context->krb5_context, 
				  &kdc->config);
	if(ret) {
		task_server_terminate(task, "kdc: failed to get KDC configuration", true);
		return;
	}
 
	kdc->config->logf = kdc->smb_krb5_context->logf;
	kdc->config->db = talloc(kdc, struct HDB *);
	if (!kdc->config->db) {
		task_server_terminate(task, "kdc: out of memory", true);
		return;
	}
	kdc->config->num_db = 1;
		
	status = hdb_samba4_create_kdc(kdc, task->event_ctx, task->lp_ctx, 
				       kdc->smb_krb5_context->krb5_context, 
				       &kdc->config->db[0]);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc: hdb_samba4_create_kdc (setup KDC database) failed", true);
		return; 
	}

	/* Register hdb-samba4 hooks for use as a keytab */

	kdc->hdb_samba4_context = talloc(kdc, struct hdb_samba4_context);
	if (!kdc->hdb_samba4_context) {
		task_server_terminate(task, "kdc: out of memory", true);
		return; 
	}

	kdc->hdb_samba4_context->ev_ctx = task->event_ctx;
	kdc->hdb_samba4_context->lp_ctx = task->lp_ctx;

	ret = krb5_plugin_register(kdc->smb_krb5_context->krb5_context, 
				   PLUGIN_TYPE_DATA, "hdb",
				   &hdb_samba4);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register hdb keytab", true);
		return;
	}

	ret = krb5_kt_register(kdc->smb_krb5_context->krb5_context, &hdb_kt_ops);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register hdb keytab", true);
		return;
	}

	/* Registar WinDC hooks */
	ret = krb5_plugin_register(kdc->smb_krb5_context->krb5_context, 
				   PLUGIN_TYPE_DATA, "windc",
				   &windc_plugin_table);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register hdb keytab", true);
		return;
	}

	krb5_kdc_windc_init(kdc->smb_krb5_context->krb5_context);

	/* start listening on the configured network interfaces */
	status = kdc_startup_interfaces(kdc, task->lp_ctx, ifaces);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc failed to setup interfaces", true);
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, KDC_CHECK_GENERIC_KERBEROS, 
			       kdc_check_generic_kerberos, kdc);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup monitoring", true);
		return;
	}

	irpc_add_name(task->msg_ctx, "kdc_server");
}


/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_kdc_init(void)
{
	return register_server_service("kdc", kdc_task_init);
}
