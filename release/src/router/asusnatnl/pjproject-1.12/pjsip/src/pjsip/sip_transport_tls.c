/* $Id: sip_transport_tls.c 4394 2013-02-27 12:02:42Z ming $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/hash.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0

#define THIS_FILE	"sip_transport_tls.c"

#define MAX_ASYNC_CNT	16
#define POOL_LIS_INIT	512
#define POOL_LIS_INC	512
#define POOL_TP_INIT	512
#define POOL_TP_INC	512

struct tls_listener;
struct tls_transport;

/*
 * Definition of TLS/SSL transport listener, and it's descendant of
 * pjsip_tpfactory.
 */
struct tls_listener
{
    pjsip_tpfactory	     factory;
    pj_bool_t		     is_registered;
    pjsip_endpoint	    *endpt;
    pjsip_tpmgr		    *tpmgr;
    pj_ssl_sock_t	    *ssock;
    pj_ssl_cert_t	    *cert;
    pjsip_tls_setting	     tls_setting;
};


/*
 * This structure is used to keep delayed transmit operation in a list.
 * A delayed transmission occurs when application sends tx_data when
 * the TLS connect/establishment is still in progress. These delayed
 * transmission will be "flushed" once the socket is connected (either
 * successfully or with errors).
 */
struct delayed_tdata
{
    PJ_DECL_LIST_MEMBER(struct delayed_tdata);
    pjsip_tx_data_op_key    *tdata_op_key;
    pj_time_val              timeout;
};


/*
 * TLS/SSL transport, and it's descendant of pjsip_transport.
 */
struct tls_transport
{
    pjsip_transport	     base;
    pj_bool_t		     is_server;
    pj_str_t		     remote_name;

    pj_bool_t		     is_registered;
    pj_bool_t		     is_closing;
    pj_status_t		     close_reason;
    pj_ssl_sock_t	    *ssock;
    pj_bool_t		     has_pending_connect;
    pj_bool_t		     verify_server;

    /* Keep-alive timer. */
    pj_timer_entry	     ka_timer;
    pj_time_val		     last_activity;
    pjsip_tx_data_op_key     ka_op_key;
    pj_str_t		     ka_pkt;

    /* TLS transport can only have  one rdata!
     * Otherwise chunks of incoming PDU may be received on different
     * buffer.
     */
    pjsip_rx_data	     rdata;

    /* Pending transmission list. */
    struct delayed_tdata     delayed_list;
};


/****************************************************************************
 * PROTOTYPES
 */

/* This callback is called when pending accept() operation completes. */
static pj_bool_t on_accept_complete(pj_ssl_sock_t *ssock,
				    pj_ssl_sock_t *new_ssock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);

/* Callback on incoming data */
static pj_bool_t on_data_read(pj_ssl_sock_t *ssock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);

/* Callback when packet is sent */
static pj_bool_t on_data_sent(pj_ssl_sock_t *ssock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory);

/* This callback is called by transport manager to create transport */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_tx_data *tdata,
					pjsip_transport **transport);

/* Common function to create and initialize transport */
static pj_status_t tls_create(struct tls_listener *listener,
			      pj_pool_t *pool,
			      pj_ssl_sock_t *ssock, 
			      pj_bool_t is_server,
			      const pj_sockaddr_in *local,
			      const pj_sockaddr_in *remote,
			      const pj_str_t *remote_name,
			      struct tls_transport **p_tls);


static void tls_perror(const char *sender, const char *title,
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(sender, "%s: %s [code=%d]", title, errmsg, status));
}


static void sockaddr_to_host_port( pj_pool_t *pool,
				   pjsip_host_port *host_port,
				   const pj_sockaddr_in *addr )
{
    host_port->host.ptr = (char*) pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+4);
    pj_sockaddr_print(addr, host_port->host.ptr, PJ_INET6_ADDRSTRLEN+4, 2);
    host_port->host.slen = pj_ansi_strlen(host_port->host.ptr);
    host_port->port = pj_sockaddr_get_port(addr);
}


static void tls_init_shutdown(struct tls_transport *tls, pj_status_t status)
{
    pjsip_tp_state_callback state_cb;

    if (tls->close_reason == PJ_SUCCESS)
	tls->close_reason = status;

    if (tls->base.is_shutdown)
	return;

    /* Prevent immediate transport destroy by application, as transport
     * state notification callback may be stacked and transport instance
     * must remain valid at any point in the callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* Notify application of transport disconnected state */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
	pjsip_transport_state_info state_info;
	pjsip_tls_state_info tls_info;
	pj_ssl_sock_info ssl_info;

	/* Init transport state info */
	pj_bzero(&state_info, sizeof(state_info));
	state_info.status = tls->close_reason;

	if (tls->ssock && 
	    pj_ssl_sock_get_info(tls->ssock, &ssl_info) == PJ_SUCCESS)
	{
	    pj_bzero(&tls_info, sizeof(tls_info));
	    tls_info.ssl_sock_info = &ssl_info;
	    state_info.ext_info = &tls_info;
	}

	(*state_cb)(&tls->base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
    }

    /* We can not destroy the transport since high level objects may
     * still keep reference to this transport. So we can only 
     * instruct transport manager to gracefully start the shutdown
     * procedure for this transport.
     */
    pjsip_transport_shutdown(&tls->base);

    /* Now, it is ok to destroy the transport. */
    pjsip_transport_dec_ref(&tls->base);
}


/****************************************************************************
 * The TLS listener/transport factory.
 */

/*
 * This is the public API to create, initialize, register, and start the
 * TLS listener.
 */
PJ_DEF(pj_status_t) pjsip_tls_transport_start (pjsip_endpoint *endpt,
					       const pjsip_tls_setting *opt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory)
{
    pj_pool_t *pool;
    struct tls_listener *listener;
    pj_ssl_sock_param ssock_param;
    pj_sockaddr_in *listener_addr;
    pj_bool_t has_listener;
    pj_status_t status;

	int inst_id;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && async_cnt, PJ_EINVAL);

	inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Verify that address given in a_name (if any) is valid */
    if (a_name && a_name->host.slen) {
	pj_sockaddr_in tmp;

	status = pj_sockaddr_in_init(&tmp, &a_name->host, 
				     (pj_uint16_t)a_name->port);
	if (status != PJ_SUCCESS || tmp.sin_addr.s_addr == PJ_INADDR_ANY ||
	    tmp.sin_addr.s_addr == PJ_INADDR_NONE)
	{
	    /* Invalid address */
	    return PJ_EINVAL;
	}
    }

    pool = pjsip_endpt_create_pool(endpt, "tlslis", POOL_LIS_INIT, 
				   POOL_LIS_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    listener = PJ_POOL_ZALLOC_T(pool, struct tls_listener);
    listener->factory.pool = pool;
    listener->factory.type = PJSIP_TRANSPORT_TLS;
    listener->factory.type_name = "tls";
    listener->factory.flag = 
	pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TLS);

    pj_ansi_strcpy(listener->factory.obj_name, "tlslis");

    if (opt)
	pjsip_tls_setting_copy(pool, &listener->tls_setting, opt);
    else
	pjsip_tls_setting_default(&listener->tls_setting);

    status = pj_lock_create_recursive_mutex(pool, "tlslis", 
					    &listener->factory.lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (async_cnt > MAX_ASYNC_CNT) 
	async_cnt = MAX_ASYNC_CNT;

    /* Build SSL socket param */
    pj_ssl_sock_param_default(&ssock_param);
    ssock_param.cb.on_accept_complete = &on_accept_complete;
    ssock_param.cb.on_data_read = &on_data_read;
    ssock_param.cb.on_data_sent = &on_data_sent;
    ssock_param.async_cnt = async_cnt;
    ssock_param.ioqueue = pjsip_endpt_get_ioqueue(endpt);
    ssock_param.require_client_cert = listener->tls_setting.require_client_cert;
    ssock_param.timeout = listener->tls_setting.timeout;
    ssock_param.user_data = listener;
    ssock_param.verify_peer = PJ_FALSE; /* avoid SSL socket closing the socket
					 * due to verification error */
    if (ssock_param.send_buffer_size < PJSIP_MAX_PKT_LEN)
	ssock_param.send_buffer_size = PJSIP_MAX_PKT_LEN;
    if (ssock_param.read_buffer_size < PJSIP_MAX_PKT_LEN)
	ssock_param.read_buffer_size = PJSIP_MAX_PKT_LEN;
    ssock_param.ciphers_num = listener->tls_setting.ciphers_num;
    ssock_param.ciphers = listener->tls_setting.ciphers;
    ssock_param.qos_type = listener->tls_setting.qos_type;
    ssock_param.qos_ignore_error = listener->tls_setting.qos_ignore_error;
    pj_memcpy(&ssock_param.qos_params, &listener->tls_setting.qos_params,
	      sizeof(ssock_param.qos_params));

    has_listener = PJ_FALSE;

    switch(listener->tls_setting.method) {
    case PJSIP_TLSV1_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_TLS1;
	break;
    case PJSIP_SSLV2_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL2;
	break;
    case PJSIP_SSLV3_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL3;
	break;
    case PJSIP_SSLV23_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL23;
	break;
    default:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_DEFAULT;
	break;
    }

    /* Create SSL socket */
    status = pj_ssl_sock_create(pool, &ssock_param, &listener->ssock);
    if (status != PJ_SUCCESS)
	goto on_error;

    listener_addr = (pj_sockaddr_in*)&listener->factory.local_addr;
	if (local) {
	pj_sockaddr_cp((pj_sockaddr_t*)listener_addr, 
		       (const pj_sockaddr_t*)local);
    } else {
	pj_sockaddr_in_init(listener_addr, NULL, 0);
    }

    /* Check if certificate/CA list for SSL socket is set */
    if (listener->tls_setting.cert_file.slen ||
	listener->tls_setting.ca_list_file.slen) 
    {
	status = pj_ssl_cert_load_from_files(pool,
			&listener->tls_setting.ca_list_file,
			&listener->tls_setting.cert_file,
			&listener->tls_setting.privkey_file,
			&listener->tls_setting.password,
			&listener->cert);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pj_ssl_sock_set_certificate(listener->ssock, pool, 
					     listener->cert);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Start accepting incoming connections. Note that some TLS/SSL backends
     * may not support for SSL socket server.
     */
    has_listener = PJ_FALSE;

    status = pj_ssl_sock_start_accept(listener->ssock, pool, 
			  (pj_sockaddr_t*)listener_addr, 
			  pj_sockaddr_get_len((pj_sockaddr_t*)listener_addr));
    if (status == PJ_SUCCESS || status == PJ_EPENDING) {
	pj_ssl_sock_info info;
	has_listener = PJ_TRUE;

	/* Retrieve the bound address */
	status = pj_ssl_sock_get_info(listener->ssock, &info);
	if (status == PJ_SUCCESS) {
	    pj_sockaddr_cp(listener_addr, (pj_sockaddr_t*)&info.local_addr);
	}
    } else if (status != PJ_ENOTSUP) {
	goto on_error;
    }

    /* If published host/IP is specified, then use that address as the
     * listener advertised address.
     */
    if (a_name && a_name->host.slen) {
	/* Copy the address */
	listener->factory.addr_name = *a_name;
	pj_strdup(listener->factory.pool, &listener->factory.addr_name.host, 
		  &a_name->host);
	listener->factory.addr_name.port = a_name->port;

    } else {
	/* No published address is given, use the bound address */

	/* If the address returns 0.0.0.0, use the default
	 * interface address as the transport's address.
	 */
	if (listener_addr->sin_addr.s_addr == 0) {
	    pj_sockaddr hostip;

	    status = pj_gethostip(pj_AF_INET(), &hostip);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    listener_addr->sin_addr.s_addr = hostip.ipv4.sin_addr.s_addr;
	}

	/* Save the address name */
	sockaddr_to_host_port(listener->factory.pool, 
			      &listener->factory.addr_name, listener_addr);
    }

    /* If port is zero, get the bound port */
    if (listener->factory.addr_name.port == 0) {
	listener->factory.addr_name.port = pj_ntohs(listener_addr->sin_port);
    }

    pj_ansi_snprintf(listener->factory.obj_name, 
		     sizeof(listener->factory.obj_name),
		     "tlslis:%d",  listener->factory.addr_name.port);

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport2 = lis_create_transport;
    listener->factory.destroy = lis_destroy;
    listener->is_registered = PJ_TRUE;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS) {
	listener->is_registered = PJ_FALSE;
	goto on_error;
    }

    if (has_listener) {
	PJ_LOG(4,(listener->factory.obj_name, 
		 "SIP TLS listener is ready for incoming connections "
		 "at %.*s:%d",
		 (int)listener->factory.addr_name.host.slen,
		 listener->factory.addr_name.host.ptr,
		 listener->factory.addr_name.port));
    } else {
	PJ_LOG(4,(listener->factory.obj_name, "SIP TLS is ready "
		  "(client only)"));
    }

    /* Return the pointer to user */
    if (p_factory) *p_factory = &listener->factory;

    return PJ_SUCCESS;

on_error:
    lis_destroy(&listener->factory);
    return status;
}


/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory)
{
    struct tls_listener *listener = (struct tls_listener *)factory;

    if (listener->is_registered) {
	pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
	listener->is_registered = PJ_FALSE;
    }

    if (listener->ssock) {
	pj_ssl_sock_close(listener->ssock);
	listener->ssock = NULL;
    }

    if (listener->factory.lock) {
	pj_lock_destroy(listener->factory.lock);
	listener->factory.lock = NULL;
    }

    if (listener->factory.pool) {
	pj_pool_t *pool = listener->factory.pool;

	PJ_LOG(4,(listener->factory.obj_name,  "SIP TLS listener destroyed"));

	listener->factory.pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/***************************************************************************/
/*
 * TLS Transport
 */

/*
 * Prototypes.
 */
/* Called by transport manager to send message */
static pj_status_t tls_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				pjsip_transport_callback callback);

/* Called by transport manager to shutdown */
static pj_status_t tls_shutdown(pjsip_transport *transport);

/* Called by transport manager to destroy transport */
static pj_status_t tls_destroy_transport(pjsip_transport *transport);

/* Utility to destroy transport */
static pj_status_t tls_destroy(pjsip_transport *transport,
			       pj_status_t reason);

/* Callback when connect completes */
static pj_bool_t on_connect_complete(pj_ssl_sock_t *ssock,
				     pj_status_t status);

/* TLS keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e);

/*
 * Common function to create TLS transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tls_create( struct tls_listener *listener,
			       pj_pool_t *pool,
			       pj_ssl_sock_t *ssock,
			       pj_bool_t is_server,
			       const pj_sockaddr_in *local,
			       const pj_sockaddr_in *remote,
			       const pj_str_t *remote_name,
			       struct tls_transport **p_tls)
{
    struct tls_transport *tls;
    const pj_str_t ka_pkt = PJSIP_TLS_KEEP_ALIVE_DATA;
    pj_status_t status;

    PJ_ASSERT_RETURN(listener && ssock && local && remote && p_tls, PJ_EINVAL);

    if (pool == NULL) {
	pool = pjsip_endpt_create_pool(listener->endpt, "tls",
				       POOL_TP_INIT, POOL_TP_INC);
	PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);
    }    

    /*
     * Create and initialize basic transport structure.
     */
    tls = PJ_POOL_ZALLOC_T(pool, struct tls_transport);
    tls->is_server = is_server;
    tls->verify_server = listener->tls_setting.verify_server;
    pj_list_init(&tls->delayed_list);
    tls->base.pool = pool;

    pj_ansi_snprintf(tls->base.obj_name, PJ_MAX_OBJ_NAME, 
		     (is_server ? "tlss%p" :"tlsc%p"), tls);

    status = pj_atomic_create(pool, 0, &tls->base.ref_cnt);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    status = pj_lock_create_recursive_mutex(pool, "tls", &tls->base.lock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    if (remote_name)
	pj_strdup(pool, &tls->remote_name, remote_name);

    tls->base.key.type = PJSIP_TRANSPORT_TLS;
    pj_memcpy(&tls->base.key.rem_addr, remote, sizeof(pj_sockaddr_in));
    tls->base.type_name = "tls";
    tls->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TLS);

    tls->base.info = (char*) pj_pool_alloc(pool, 64);
    pj_ansi_snprintf(tls->base.info, 64, "TLS to %s:%d",
		     pj_inet_ntoa(remote->sin_addr), 
		     (int)pj_ntohs(remote->sin_port));

    tls->base.addr_len = sizeof(pj_sockaddr_in);
    tls->base.dir = is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;
    
    /* Set initial local address */
	if (!pj_sockaddr_has_addr(local)) {
        pj_sockaddr_cp(&tls->base.local_addr,
                       &listener->factory.local_addr);
	} else {
	pj_sockaddr_cp(&tls->base.local_addr, local);
    }
    
    sockaddr_to_host_port(pool, &tls->base.local_name, 
			  (pj_sockaddr_in*)&tls->base.local_addr);
    if (tls->remote_name.slen) {
	tls->base.remote_name.host = tls->remote_name;
	tls->base.remote_name.port = pj_sockaddr_in_get_port(remote);
    } else {
	sockaddr_to_host_port(pool, &tls->base.remote_name, remote);
    }

    tls->base.endpt = listener->endpt;
    tls->base.tpmgr = listener->tpmgr;
    tls->base.send_msg = &tls_send_msg;
    tls->base.do_shutdown = &tls_shutdown;
    tls->base.destroy = &tls_destroy_transport;

    tls->ssock = ssock;

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tls->base);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tls->is_registered = PJ_TRUE;

    /* Initialize keep-alive timer */
    tls->ka_timer.user_data = (void*)tls;
    tls->ka_timer.cb = &tls_keep_alive_timer;
    pj_ioqueue_op_key_init(&tls->ka_op_key.key, sizeof(pj_ioqueue_op_key_t));
    pj_strdup(tls->base.pool, &tls->ka_pkt, &ka_pkt);
    
    /* Done setting up basic transport. */
    *p_tls = tls;

    PJ_LOG(4,(tls->base.obj_name, "TLS %s transport created",
	      (tls->is_server ? "server" : "client")));

    return PJ_SUCCESS;

on_error:
    tls_destroy(&tls->base, status);
    return status;
}


/* Flush all delayed transmision once the socket is connected. */
static void tls_flush_pending_tx(struct tls_transport *tls)
{
    pj_time_val now;

    pj_gettickcount(&now);
    pj_lock_acquire(tls->base.lock);
    while (!pj_list_empty(&tls->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pjsip_tx_data *tdata;
	pj_ioqueue_op_key_t *op_key;
	pj_ssize_t size;
	pj_status_t status;

	pending_tx = tls->delayed_list.next;
	pj_list_erase(pending_tx);

	tdata = pending_tx->tdata_op_key->tdata;
	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

        if (pending_tx->timeout.sec > 0 &&
            PJ_TIME_VAL_GT(now, pending_tx->timeout))
        {
            continue;
        }

	/* send! */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_ssl_sock_send(tls->ssock, op_key, tdata->buf.start, 
				  &size, 0);

	if (status != PJ_EPENDING) {
            pj_lock_release(tls->base.lock);
	    on_data_sent(tls->ssock, op_key, size);
            pj_lock_acquire(tls->base.lock);
	}
    }
    pj_lock_release(tls->base.lock);
}


/* Called by transport manager to destroy transport */
static pj_status_t tls_destroy_transport(pjsip_transport *transport)
{
    struct tls_transport *tls = (struct tls_transport*)transport;

    /* Transport would have been unregistered by now since this callback
     * is called by transport manager.
     */
    tls->is_registered = PJ_FALSE;

    return tls_destroy(transport, tls->close_reason);
}


/* Destroy TLS transport */
static pj_status_t tls_destroy(pjsip_transport *transport, 
			       pj_status_t reason)
{
    struct tls_transport *tls = (struct tls_transport*)transport;

    if (tls->close_reason == 0)
	tls->close_reason = reason;

    if (tls->is_registered) {
	tls->is_registered = PJ_FALSE;
	pjsip_transport_destroy(transport);

	/* pjsip_transport_destroy will recursively call this function
	 * again.
	 */
	return PJ_SUCCESS;
    }

    /* Mark transport as closing */
    tls->is_closing = PJ_TRUE;

    /* Stop keep-alive timer. */
    if (tls->ka_timer.id) {
	pjsip_endpt_cancel_timer(tls->base.endpt, &tls->ka_timer);
	tls->ka_timer.id = PJ_FALSE;
    }

    /* Cancel all delayed transmits */
    while (!pj_list_empty(&tls->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pj_ioqueue_op_key_t *op_key;

	pending_tx = tls->delayed_list.next;
	pj_list_erase(pending_tx);

	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	on_data_sent(tls->ssock, op_key, -reason);
    }

    if (tls->rdata.tp_info.pool) {
	pj_pool_release(tls->rdata.tp_info.pool);
	tls->rdata.tp_info.pool = NULL;
    }

    if (tls->ssock) {
	pj_ssl_sock_close(tls->ssock);
	tls->ssock = NULL;
    }
    if (tls->base.lock) {
	pj_lock_destroy(tls->base.lock);
	tls->base.lock = NULL;
    }

    if (tls->base.ref_cnt) {
	pj_atomic_destroy(tls->base.ref_cnt);
	tls->base.ref_cnt = NULL;
    }

    if (tls->base.pool) {
	pj_pool_t *pool;

	if (reason != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(reason, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(tls->base.obj_name, 
		      "TLS transport destroyed with reason %d: %s", 
		      reason, errmsg));

	} else {

	    PJ_LOG(4,(tls->base.obj_name, 
		      "TLS transport destroyed normally"));

	}

	pool = tls->base.pool;
	tls->base.pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket. It is called after
 * accept() or connect() operation complete.
 */
static pj_status_t tls_start_read(struct tls_transport *tls)
{
    pj_pool_t *pool;
    pj_ssize_t size;
    pj_sockaddr_in *rem_addr;
    void *readbuf[1];
    pj_status_t status;

    /* Init rdata */
    pool = pjsip_endpt_create_pool(tls->base.endpt,
				   "rtd%p",
				   PJSIP_POOL_RDATA_LEN,
				   PJSIP_POOL_RDATA_INC);
    if (!pool) {
	tls_perror(tls->base.obj_name, "Unable to create pool", PJ_ENOMEM);
	return PJ_ENOMEM;
    }

    tls->rdata.tp_info.pool = pool;

    tls->rdata.tp_info.transport = &tls->base;
    tls->rdata.tp_info.tp_data = tls;
    tls->rdata.tp_info.op_key.rdata = &tls->rdata;
    pj_ioqueue_op_key_init(&tls->rdata.tp_info.op_key.op_key, 
			   sizeof(pj_ioqueue_op_key_t));

    tls->rdata.pkt_info.src_addr = tls->base.key.rem_addr;
    tls->rdata.pkt_info.src_addr_len = sizeof(pj_sockaddr_in);
    rem_addr = (pj_sockaddr_in*) &tls->base.key.rem_addr;
    pj_ansi_strcpy(tls->rdata.pkt_info.src_name,
		   pj_inet_ntoa(rem_addr->sin_addr));
    tls->rdata.pkt_info.src_port = pj_ntohs(rem_addr->sin_port);

    size = sizeof(tls->rdata.pkt_info.packet);
    readbuf[0] = tls->rdata.pkt_info.packet;
    status = pj_ssl_sock_start_read2(tls->ssock, tls->base.pool, size,
				     readbuf, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	PJ_LOG(4, (tls->base.obj_name, 
		   "pj_ssl_sock_start_read() error, status=%d", 
		   status));
	return status;
    }

    return PJ_SUCCESS;
}


/* This callback is called by transport manager for the TLS factory
 * to create outgoing transport to the specified destination.
 */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_tx_data *tdata,
					pjsip_transport **p_transport)
{
    struct tls_listener *listener;
    struct tls_transport *tls;
    pj_pool_t *pool;
    pj_ssl_sock_t *ssock;
    pj_ssl_sock_param ssock_param;
    pj_sockaddr_in local_addr;
    pj_str_t remote_name;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(factory && mgr && endpt && rem_addr &&
		     addr_len && p_transport, PJ_EINVAL);

    /* Check that address is a sockaddr_in */
    PJ_ASSERT_RETURN(rem_addr->addr.sa_family == pj_AF_INET() &&
		     addr_len == sizeof(pj_sockaddr_in), PJ_EINVAL);

    listener = (struct tls_listener*)factory;

    pool = pjsip_endpt_create_pool(listener->endpt, "tls",
				   POOL_TP_INIT, POOL_TP_INC);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    /* Get remote host name from tdata */
    if (tdata)
	remote_name = tdata->dest_info.name;
    else
	pj_bzero(&remote_name, sizeof(remote_name));

    /* Build SSL socket param */
    pj_ssl_sock_param_default(&ssock_param);
    ssock_param.cb.on_connect_complete = &on_connect_complete;
    ssock_param.cb.on_data_read = &on_data_read;
    ssock_param.cb.on_data_sent = &on_data_sent;
    ssock_param.async_cnt = 1;
    ssock_param.ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    ssock_param.server_name = remote_name;
    ssock_param.timeout = listener->tls_setting.timeout;
    ssock_param.user_data = NULL; /* pending, must be set later */
    ssock_param.verify_peer = PJ_FALSE; /* avoid SSL socket closing the socket
					 * due to verification error */
    if (ssock_param.send_buffer_size < PJSIP_MAX_PKT_LEN)
	ssock_param.send_buffer_size = PJSIP_MAX_PKT_LEN;
    if (ssock_param.read_buffer_size < PJSIP_MAX_PKT_LEN)
	ssock_param.read_buffer_size = PJSIP_MAX_PKT_LEN;
    ssock_param.ciphers_num = listener->tls_setting.ciphers_num;
    ssock_param.ciphers = listener->tls_setting.ciphers;
    ssock_param.qos_type = listener->tls_setting.qos_type;
	ssock_param.qos_ignore_error = listener->tls_setting.qos_ignore_error;
	ssock_param.timer_heap = pjsip_endpt_get_timer_heap(endpt);  // dean : for ssl handshake timeout detection.
    pj_memcpy(&ssock_param.qos_params, &listener->tls_setting.qos_params,
	      sizeof(ssock_param.qos_params));

    switch(listener->tls_setting.method) {
    case PJSIP_TLSV1_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_TLS1;
	break;
    case PJSIP_SSLV2_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL2;
	break;
    case PJSIP_SSLV3_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL3;
	break;
    case PJSIP_SSLV23_METHOD:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_SSL23;
	break;
    default:
	ssock_param.proto = PJ_SSL_SOCK_PROTO_DEFAULT;
	break;
    }

    status = pj_ssl_sock_create(pool, &ssock_param, &ssock);
    if (status != PJ_SUCCESS)
	return status;

    /* Apply SSL certificate */
    if (listener->cert) {
	status = pj_ssl_sock_set_certificate(ssock, pool, listener->cert);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Initially set bind address to PJ_INADDR_ANY port 0 */
    pj_sockaddr_in_init(&local_addr, NULL, 0);

    /* Create the transport descriptor */
    status = tls_create(listener, pool, ssock, PJ_FALSE, &local_addr, 
			(pj_sockaddr_in*)rem_addr, &remote_name, &tls);
    if (status != PJ_SUCCESS)
	return status;

    /* Set the "pending" SSL socket user data */
    pj_ssl_sock_set_user_data(tls->ssock, tls);

    /* Start asynchronous connect() operation */
    tls->has_pending_connect = PJ_TRUE;
    status = pj_ssl_sock_start_connect(tls->ssock, tls->base.pool, 
				       (pj_sockaddr_t*)&local_addr,
				       (pj_sockaddr_t*)rem_addr,
				       addr_len);
    if (status == PJ_SUCCESS) {
	on_connect_complete(tls->ssock, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
	tls_destroy(&tls->base, status);
	return status;
    }

    if (tls->has_pending_connect) {
	pj_ssl_sock_info info;

	/* Update local address, just in case local address currently set is 
	 * different now that asynchronous connect() is started.
	 */

	/* Retrieve the bound address */
	status = pj_ssl_sock_get_info(tls->ssock, &info);
	if (status == PJ_SUCCESS) {
	    pj_uint16_t new_port;

	    new_port = pj_sockaddr_get_port((pj_sockaddr_t*)&info.local_addr);

	    if (pj_sockaddr_has_addr((pj_sockaddr_t*)&info.local_addr)) {
			/* Update sockaddr */
		pj_sockaddr_cp((pj_sockaddr_t*)&tls->base.local_addr,
			       (pj_sockaddr_t*)&info.local_addr);
	    } else if (new_port && new_port != pj_sockaddr_get_port(
					(pj_sockaddr_t*)&tls->base.local_addr))
	    {
		/* Update port only */
		pj_sockaddr_set_port(&tls->base.local_addr, 
				     new_port);
	    }

	    sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
				  (pj_sockaddr_in*)&tls->base.local_addr);
	}

	PJ_LOG(4,(tls->base.obj_name, 
		  "TLS transport %.*s:%d is connecting to %.*s:%d...",
		  (int)tls->base.local_name.host.slen,
		  tls->base.local_name.host.ptr,
		  tls->base.local_name.port,
		  (int)tls->base.remote_name.host.slen,
		  tls->base.remote_name.host.ptr,
		  tls->base.remote_name.port));
    }

    /* Done */
    *p_transport = &tls->base;

    return PJ_SUCCESS;
}


/*
 * This callback is called by SSL socket when pending accept() operation
 * has completed.
 */
static pj_bool_t on_accept_complete(pj_ssl_sock_t *ssock,
				    pj_ssl_sock_t *new_ssock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len)
{
    struct tls_listener *listener;
    struct tls_transport *tls;
    pj_ssl_sock_info ssl_info;
    char addr[PJ_INET6_ADDRSTRLEN+10];
    pjsip_tp_state_callback state_cb;
    pj_bool_t is_shutdown;
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr_len);

    listener = (struct tls_listener*) pj_ssl_sock_get_user_data(ssock);

    PJ_ASSERT_RETURN(new_ssock, PJ_TRUE);

    PJ_LOG(4,(listener->factory.obj_name, 
	      "TLS listener %.*s:%d: got incoming TLS connection "
	      "from %s, sock=%d",
	      (int)listener->factory.addr_name.host.slen,
	      listener->factory.addr_name.host.ptr,
	      listener->factory.addr_name.port,
	      pj_sockaddr_print(src_addr, addr, sizeof(addr), 3),
	      new_ssock));

    /* Retrieve SSL socket info, close the socket if this is failed
     * as the SSL socket info availability is rather critical here.
     */
    status = pj_ssl_sock_get_info(new_ssock, &ssl_info);
    if (status != PJ_SUCCESS) {
	pj_ssl_sock_close(new_ssock);
	return PJ_TRUE;
    }

    /* 
     * Incoming connection!
     * Create TLS transport for the new socket.
     */
    status = tls_create( listener, NULL, new_ssock, PJ_TRUE,
			 (const pj_sockaddr_in*)&listener->factory.local_addr,
			 (const pj_sockaddr_in*)src_addr, NULL, &tls);
    
    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    /* Set the "pending" SSL socket user data */
    pj_ssl_sock_set_user_data(new_ssock, tls);

    /* Prevent immediate transport destroy as application may access it 
     * (getting info, etc) in transport state notification callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* If there is verification error and verification is mandatory, shutdown
     * and destroy the transport.
     */
    if (ssl_info.verify_status && listener->tls_setting.verify_client) {
	if (tls->close_reason == PJ_SUCCESS) 
	    tls->close_reason = PJSIP_TLS_ECERTVERIF;
	pjsip_transport_shutdown(&tls->base);
    }

    /* Notify transport state to application */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
	pjsip_transport_state_info state_info;
	pjsip_tls_state_info tls_info;
	pjsip_transport_state tp_state;

	/* Init transport state info */
	pj_bzero(&tls_info, sizeof(tls_info));
	pj_bzero(&state_info, sizeof(state_info));
	tls_info.ssl_sock_info = &ssl_info;
	state_info.ext_info = &tls_info;

	/* Set transport state based on verification status */
	if (ssl_info.verify_status && listener->tls_setting.verify_client)
	{
	    tp_state = PJSIP_TP_STATE_DISCONNECTED;
	    state_info.status = PJSIP_TLS_ECERTVERIF;
	} else {
	    tp_state = PJSIP_TP_STATE_CONNECTED;
	    state_info.status = PJ_SUCCESS;
	}

	(*state_cb)(&tls->base, tp_state, &state_info);
    }

    /* Release transport reference. If transport is shutting down, it may
     * get destroyed here.
     */
    is_shutdown = tls->base.is_shutdown;
    pjsip_transport_dec_ref(&tls->base);
    if (is_shutdown)
	return PJ_TRUE;


    status = tls_start_read(tls);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(tls->base.obj_name, "New transport cancelled"));
	tls_init_shutdown(tls, status);
	tls_destroy(&tls->base, status);
    } else {
	/* Start keep-alive timer */
	if (PJSIP_TLS_KEEP_ALIVE_INTERVAL) {
	    pj_time_val delay = {PJSIP_TLS_KEEP_ALIVE_INTERVAL, 0};
	    pjsip_endpt_schedule_timer(listener->endpt, 
				       &tls->ka_timer, 
				       &delay);
	    tls->ka_timer.id = PJ_TRUE;
	    pj_gettimeofday(&tls->last_activity);
	}
    }

    return PJ_TRUE;
}


/* 
 * Callback from ioqueue when packet is sent.
 */
static pj_bool_t on_data_sent(pj_ssl_sock_t *ssock,
			      pj_ioqueue_op_key_t *op_key,
			      pj_ssize_t bytes_sent)
{
    struct tls_transport *tls = (struct tls_transport*) 
    				pj_ssl_sock_get_user_data(ssock);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    /* Note that op_key may be the op_key from keep-alive, thus
     * it will not have tdata etc.
     */

    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
	/*
	 * Notify sip_transport.c that packet has been sent.
	 */
	if (bytes_sent == 0)
	    bytes_sent = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

	tdata_op_key->callback(&tls->base, tdata_op_key->token, bytes_sent);

	/* Mark last activity time */
	pj_gettimeofday(&tls->last_activity);

    }

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	PJ_LOG(5,(tls->base.obj_name, "TLS send() error, sent=%d", 
		  bytes_sent));

	status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
				     -bytes_sent;

	tls_init_shutdown(tls, status);

	return PJ_FALSE;
    }
    
    return PJ_TRUE;
}


/* 
 * This callback is called by transport manager to send SIP message 
 */
static pj_status_t tls_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				pjsip_transport_callback callback)
{
    struct tls_transport *tls = (struct tls_transport*)transport;
    pj_ssize_t size;
    pj_bool_t delayed = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    /* Sanity check */
    PJ_ASSERT_RETURN(transport && tdata, PJ_EINVAL);

    /* Check that there's no pending operation associated with the tdata */
    PJ_ASSERT_RETURN(tdata->op_key.tdata == NULL, PJSIP_EPENDINGTX);
    
    /* Check the address is supported */
    PJ_ASSERT_RETURN(rem_addr && addr_len==sizeof(pj_sockaddr_in), PJ_EINVAL);



    /* Init op key. */
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    /* If asynchronous connect() has not completed yet, just put the
     * transmit data in the pending transmission list since we can not
     * use the socket yet.
     */
    if (tls->has_pending_connect) {

	/*
	 * Looks like connect() is still in progress. Check again (this time
	 * with holding the lock) to be sure.
	 */
	pj_lock_acquire(tls->base.lock);

	if (tls->has_pending_connect) {
	    struct delayed_tdata *delayed_tdata;

	    /*
	     * connect() is still in progress. Put the transmit data to
	     * the delayed list.
             * Starting from #1583 (https://trac.pjsip.org/repos/ticket/1583),
             * we also add timeout value for the transmit data. When the
             * connect() is completed, the timeout value will be checked to
             * determine whether the transmit data needs to be sent.
	     */
	    delayed_tdata = PJ_POOL_ZALLOC_T(tdata->pool, 
					    struct delayed_tdata);
	    delayed_tdata->tdata_op_key = &tdata->op_key;
            if (tdata->msg && tdata->msg->type == PJSIP_REQUEST_MSG) {
                pj_gettickcount(&delayed_tdata->timeout);
                delayed_tdata->timeout.msec += pjsip_cfg()->tsx.td;
                pj_time_val_normalize(&delayed_tdata->timeout);
            }

	    pj_list_push_back(&tls->delayed_list, delayed_tdata);
	    status = PJ_EPENDING;

	    /* Prevent pj_ioqueue_send() to be called below */
	    delayed = PJ_TRUE;
	}

	pj_lock_release(tls->base.lock);
    } 
    
    if (!delayed) {
	/*
	 * Transport is ready to go. Send the packet to ioqueue to be
	 * sent asynchronously.
	 */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_ssl_sock_send(tls->ssock, 
				    (pj_ioqueue_op_key_t*)&tdata->op_key,
				    tdata->buf.start, &size, 0);

	if (status != PJ_EPENDING) {
	    /* Not pending (could be immediate success or error) */
	    tdata->op_key.tdata = NULL;

	    /* Shutdown transport on closure/errors */
	    if (size <= 0) {

		PJ_LOG(5,(tls->base.obj_name, "TLS send() error, sent=%d", 
			  size));

		if (status == PJ_SUCCESS) 
		    status = PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

		tls_init_shutdown(tls, status);
	    }
	}
    }

    return status;
}


/* 
 * This callback is called by transport manager to shutdown transport.
 */
static pj_status_t tls_shutdown(pjsip_transport *transport)
{
    struct tls_transport *tls = (struct tls_transport*)transport;
    
    /* Stop keep-alive timer. */
    if (tls->ka_timer.id) {
	pjsip_endpt_cancel_timer(tls->base.endpt, &tls->ka_timer);
	tls->ka_timer.id = PJ_FALSE;
    }

    return PJ_SUCCESS;
}


/* 
 * Callback from ioqueue that an incoming data is received from the socket.
 */
static pj_bool_t on_data_read(pj_ssl_sock_t *ssock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    struct tls_transport *tls;
    pjsip_rx_data *rdata;

    PJ_UNUSED_ARG(data);

    tls = (struct tls_transport*) pj_ssl_sock_get_user_data(ssock);
    rdata = &tls->rdata;

    /* Don't do anything if transport is closing. */
    if (tls->is_closing) {
	tls->is_closing++;
	return PJ_FALSE;
    }

    /* Houston, we have packet! Report the packet to transport manager
     * to be parsed.
     */
    if (status == PJ_SUCCESS) {
	pj_size_t size_eaten;

	/* Mark this as an activity */
	pj_gettimeofday(&tls->last_activity);

	pj_assert((void*)rdata->pkt_info.packet == data);

	/* Init pkt_info part. */
	rdata->pkt_info.len = size;
	rdata->pkt_info.zero = 0;
	pj_gettimeofday(&rdata->pkt_info.timestamp);

	/* Report to transport manager.
	 * The transport manager will tell us how many bytes of the packet
	 * have been processed (as valid SIP message).
	 */
	size_eaten = 
	    pjsip_tpmgr_receive_packet(rdata->tp_info.transport->tpmgr, 
				       rdata);

	pj_assert(size_eaten <= (pj_size_t)rdata->pkt_info.len);

	/* Move unprocessed data to the front of the buffer */
	*remainder = size - size_eaten;
	if (*remainder > 0 && *remainder != size) {
	    pj_memmove(rdata->pkt_info.packet,
		       rdata->pkt_info.packet + size_eaten,
		       *remainder);
	}

    } else {

	/* Transport is closed */
	PJ_LOG(4,(tls->base.obj_name, "TLS connection closed"));

	tls_init_shutdown(tls, status);

	return PJ_FALSE;

    }

    /* Reset pool. */
    pj_pool_reset(rdata->tp_info.pool);

    return PJ_TRUE;
}


/* 
 * Callback from ioqueue when asynchronous connect() operation completes.
 */
static pj_bool_t on_connect_complete(pj_ssl_sock_t *ssock,
				     pj_status_t status)
{
    struct tls_transport *tls;
    pj_ssl_sock_info ssl_info;
    pj_sockaddr_in addr, *tp_addr;
    pjsip_tp_state_callback state_cb;
    pj_bool_t is_shutdown;

    tls = (struct tls_transport*) pj_ssl_sock_get_user_data(ssock);

    /* Check connect() status */
    if (status != PJ_SUCCESS) {

	tls_perror(tls->base.obj_name, "TLS connect() error", status);

	/* Cancel all delayed transmits */
	while (!pj_list_empty(&tls->delayed_list)) {
	    struct delayed_tdata *pending_tx;
	    pj_ioqueue_op_key_t *op_key;

	    pending_tx = tls->delayed_list.next;
	    pj_list_erase(pending_tx);

	    op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	    on_data_sent(tls->ssock, op_key, -status);
	}

	goto on_error;
    }

    /* Retrieve SSL socket info, shutdown the transport if this is failed
     * as the SSL socket info availability is rather critical here.
     */
    status = pj_ssl_sock_get_info(tls->ssock, &ssl_info);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Update (again) local address, just in case local address currently
     * set is different now that the socket is connected (could happen
     * on some systems, like old Win32 probably?).
     */
	tp_addr = (pj_sockaddr_in*)&tls->base.local_addr;
    pj_sockaddr_cp((pj_sockaddr_t*)&addr, 
		   (pj_sockaddr_t*)&ssl_info.local_addr);
    if (tp_addr->sin_addr.s_addr != addr.sin_addr.s_addr) {
	tp_addr->sin_addr.s_addr = addr.sin_addr.s_addr;
	tp_addr->sin_port = addr.sin_port;
	sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
			      tp_addr);
    }

    /* Server identity verification based on server certificate. */
    if (ssl_info.remote_cert_info->version) {
	pj_str_t *remote_name;
	pj_ssl_cert_info *serv_cert = ssl_info.remote_cert_info;
	pj_bool_t matched = PJ_FALSE;
	unsigned i;

	/* Remote name may be hostname or IP address */
	if (tls->remote_name.slen)
	    remote_name = &tls->remote_name;
	else
	    remote_name = &tls->base.remote_name.host;

	/* Start matching remote name with SubjectAltName fields of 
	 * server certificate.
	 */
	for (i = 0; i < serv_cert->subj_alt_name.cnt && !matched; ++i) {
	    pj_str_t *cert_name = &serv_cert->subj_alt_name.entry[i].name;

	    switch (serv_cert->subj_alt_name.entry[i].type) {
	    case PJ_SSL_CERT_NAME_DNS:
	    case PJ_SSL_CERT_NAME_IP:
		matched = !pj_stricmp(remote_name, cert_name);
		break;
	    case PJ_SSL_CERT_NAME_URI:
		if (pj_strnicmp2(cert_name, "sip:", 4) == 0 ||
		    pj_strnicmp2(cert_name, "sips:", 5) == 0)
		{
		    pj_str_t host_part;
		    char *p;

		    p = pj_strchr(cert_name, ':') + 1;
		    pj_strset(&host_part, p, cert_name->slen - 
					     (p - cert_name->ptr));
		    matched = !pj_stricmp(remote_name, &host_part);
		}
		break;
	    default:
		break;
	    }
	}
    	
	/* When still not matched or no SubjectAltName fields in server
	 * certificate, try with Common Name of Subject field.
	 */
	if (!matched) {
	    matched = !pj_stricmp(remote_name, &serv_cert->subject.cn);
	}

	if (!matched)
	    ssl_info.verify_status |= PJ_SSL_CERT_EIDENTITY_NOT_MATCH;
    }

    /* Prevent immediate transport destroy as application may access it 
     * (getting info, etc) in transport state notification callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* If there is verification error and verification is mandatory, shutdown
     * and destroy the transport.
     */
    if (ssl_info.verify_status && tls->verify_server) {
	if (tls->close_reason == PJ_SUCCESS) 
	    tls->close_reason = PJSIP_TLS_ECERTVERIF;
	pjsip_transport_shutdown(&tls->base);
    }

    /* Notify transport state to application */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
	pjsip_transport_state_info state_info;
	pjsip_tls_state_info tls_info;
	pjsip_transport_state tp_state;

	/* Init transport state info */
	pj_bzero(&state_info, sizeof(state_info));
	pj_bzero(&tls_info, sizeof(tls_info));
	state_info.ext_info = &tls_info;
	tls_info.ssl_sock_info = &ssl_info;

	/* Set transport state based on verification status */
	if (ssl_info.verify_status && tls->verify_server)
	{
	    tp_state = PJSIP_TP_STATE_DISCONNECTED;
	    state_info.status = PJSIP_TLS_ECERTVERIF;
	} else {
	    tp_state = PJSIP_TP_STATE_CONNECTED;
	    state_info.status = PJ_SUCCESS;
	}

	(*state_cb)(&tls->base, tp_state, &state_info);
    }

    /* Release transport reference. If transport is shutting down, it may
     * get destroyed here.
     */
    is_shutdown = tls->base.is_shutdown;
    pjsip_transport_dec_ref(&tls->base);
    if (is_shutdown)
	return PJ_FALSE;


    /* Mark that pending connect() operation has completed. */
    tls->has_pending_connect = PJ_FALSE;

    PJ_LOG(4,(tls->base.obj_name, 
	      "TLS transport %.*s:%d is connected to %.*s:%d",
	      (int)tls->base.local_name.host.slen,
	      tls->base.local_name.host.ptr,
	      tls->base.local_name.port,
	      (int)tls->base.remote_name.host.slen,
	      tls->base.remote_name.host.ptr,
	      tls->base.remote_name.port));

    /* Start pending read */
    status = tls_start_read(tls);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Flush all pending send operations */
    tls_flush_pending_tx(tls);

    /* Start keep-alive timer */
    if (PJSIP_TLS_KEEP_ALIVE_INTERVAL) {
	pj_time_val delay = { PJSIP_TLS_KEEP_ALIVE_INTERVAL, 0 };
	pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
				   &delay);
	tls->ka_timer.id = PJ_TRUE;
	pj_gettimeofday(&tls->last_activity);
    }

    return PJ_TRUE;

on_error:
    tls_init_shutdown(tls, status);

    return PJ_FALSE;
}


/* Transport keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
    struct tls_transport *tls = (struct tls_transport*) e->user_data;
    pj_time_val delay;
    pj_time_val now;
    pj_ssize_t size;
    pj_status_t status;

    PJ_UNUSED_ARG(th);

    tls->ka_timer.id = PJ_TRUE;

    pj_gettimeofday(&now);
    PJ_TIME_VAL_SUB(now, tls->last_activity);

    if (now.sec > 0 && now.sec < PJSIP_TLS_KEEP_ALIVE_INTERVAL) {
	/* There has been activity, so don't send keep-alive */
	delay.sec = PJSIP_TLS_KEEP_ALIVE_INTERVAL - now.sec;
	delay.msec = 0;

	pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
				   &delay);
	tls->ka_timer.id = PJ_TRUE;
	return;
    }

    PJ_LOG(5,(tls->base.obj_name, "Sending %d byte(s) keep-alive to %.*s:%d", 
	      (int)tls->ka_pkt.slen, (int)tls->base.remote_name.host.slen,
	      tls->base.remote_name.host.ptr,
	      tls->base.remote_name.port));

    /* Send the data */
    size = tls->ka_pkt.slen;
    status = pj_ssl_sock_send(tls->ssock, &tls->ka_op_key.key,
			      tls->ka_pkt.ptr, &size, 0);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	tls_perror(tls->base.obj_name, 
		   "Error sending keep-alive packet", status);

	tls_init_shutdown(tls, status);
	return;
    }

    /* Register next keep-alive */
    delay.sec = PJSIP_TLS_KEEP_ALIVE_INTERVAL;
    delay.msec = 0;

    pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
			       &delay);
    tls->ka_timer.id = PJ_TRUE;
}

#endif /* PJSIP_HAS_TLS_TRANSPORT */
