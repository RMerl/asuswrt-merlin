/* $Id: stun_sock_test.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "test.h"

#define THIS_FILE   "stun_sock_test.c"

enum {
    RESPOND_STUN    = 1,
    WITH_MAPPED	    = 2,
    WITH_XOR_MAPPED = 4,

    ECHO	    = 8
};

/*
 * Simple STUN server
 */
struct stun_srv
{
    pj_activesock_t	*asock;
    unsigned		 flag;
    pj_sockaddr		 addr;
    unsigned		 rx_cnt;
    pj_ioqueue_op_key_t	 send_key;
    pj_str_t		 ip_to_send;
    pj_uint16_t		 port_to_send;
};

static pj_bool_t srv_on_data_recvfrom(pj_activesock_t *asock,
				      void *data,
				      pj_size_t size,
				      const pj_sockaddr_t *src_addr,
				      int addr_len,
				      pj_status_t status)
{
    struct stun_srv *srv;
    pj_ssize_t sent;

    srv = (struct stun_srv*) pj_activesock_get_user_data(asock);

    /* Ignore error */
    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    ++srv->rx_cnt;

    /* Ignore if we're not responding */
    if (srv->flag & RESPOND_STUN) {
	pj_pool_t *pool;
	pj_stun_msg *req_msg, *res_msg;

	pool = pj_pool_create(mem, "stunsrv", 512, 512, NULL);
    
	/* Parse request */
	status = pj_stun_msg_decode(pool, (pj_uint8_t*)data, size, 
				    PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
				    &req_msg, NULL, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror("   pj_stun_msg_decode()", status);
	    pj_pool_release(pool);
	    return PJ_TRUE;
	}

	/* Create response */
	status = pj_stun_msg_create(pool, PJ_STUN_BINDING_RESPONSE, PJ_STUN_MAGIC,
				    req_msg->hdr.tsx_id, &res_msg);
	if (status != PJ_SUCCESS) {
	    app_perror("   pj_stun_msg_create()", status);
	    pj_pool_release(pool);
	    return PJ_TRUE;
	}

	/* Add MAPPED-ADDRESS or XOR-MAPPED-ADDRESS (or don't add) */
	if (srv->flag & WITH_MAPPED) {
	    pj_sockaddr_in addr;

	    pj_sockaddr_in_init(&addr, &srv->ip_to_send, srv->port_to_send);
	    pj_stun_msg_add_sockaddr_attr(pool, res_msg, PJ_STUN_ATTR_MAPPED_ADDR,
					  PJ_FALSE, &addr, sizeof(addr));
	} else if (srv->flag & WITH_XOR_MAPPED) {
	    pj_sockaddr_in addr;

	    pj_sockaddr_in_init(&addr, &srv->ip_to_send, srv->port_to_send);
	    pj_stun_msg_add_sockaddr_attr(pool, res_msg, 
					  PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					  PJ_TRUE, &addr, sizeof(addr));
	}

	/* Encode */
	status = pj_stun_msg_encode(res_msg, (pj_uint8_t*)data, 100, 0, 
				    NULL, &size);
	if (status != PJ_SUCCESS) {
	    app_perror("   pj_stun_msg_encode()", status);
	    pj_pool_release(pool);
	    return PJ_TRUE;
	}

	/* Send back */
	sent = size;
	pj_activesock_sendto(asock, &srv->send_key, data, &sent, 0, 
			     src_addr, addr_len);

	pj_pool_release(pool);

    } else if (srv->flag & ECHO) {
	/* Send back */
	sent = size;
	pj_activesock_sendto(asock, &srv->send_key, data, &sent, 0, 
			     src_addr, addr_len);

    }

    return PJ_TRUE;
}

static pj_status_t create_server(pj_pool_t *pool,
				 pj_ioqueue_t *ioqueue,
				 unsigned flag,
				 struct stun_srv **p_srv)
{
    struct stun_srv *srv;
    pj_activesock_cb activesock_cb;
    pj_status_t status;

    srv = PJ_POOL_ZALLOC_T(pool, struct stun_srv);
    srv->flag = flag;
    srv->ip_to_send = pj_str("1.1.1.1");
    srv->port_to_send = 1000;

    status = pj_sockaddr_in_init(&srv->addr.ipv4, NULL, 0);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(&activesock_cb, sizeof(activesock_cb));
    activesock_cb.on_data_recvfrom = &srv_on_data_recvfrom;
    status = pj_activesock_create_udp(pool, &srv->addr, NULL, ioqueue,
				      &activesock_cb, srv, &srv->asock, 
				      &srv->addr);
    if (status != PJ_SUCCESS)
	return status;

    pj_ioqueue_op_key_init(&srv->send_key, sizeof(srv->send_key));

    status = pj_activesock_start_recvfrom(srv->asock, pool, 512, 0);
    if (status != PJ_SUCCESS) {
	pj_activesock_close(srv->asock);
	return status;
    }

    *p_srv = srv;
    return PJ_SUCCESS;
}

static void destroy_server(struct stun_srv *srv)
{
    pj_activesock_close(srv->asock);
}


struct stun_client
{
    pj_pool_t		*pool;
    pj_stun_sock	*sock;

    pj_ioqueue_op_key_t	 send_key;
    pj_bool_t		 destroy_on_err;

    unsigned		 on_status_cnt;
    pj_stun_sock_op	 last_op;
    pj_status_t		 last_status;

    unsigned		 on_rx_data_cnt;
};

static pj_bool_t stun_sock_on_status(pj_stun_sock *stun_sock, 
				     pj_stun_sock_op op,
				     pj_status_t status)
{
    struct stun_client *client;

    client = (struct stun_client*) pj_stun_sock_get_user_data(stun_sock);
    client->on_status_cnt++;
    client->last_op = op;
    client->last_status = status;

    if (status != PJ_SUCCESS && client->destroy_on_err) {
	pj_stun_sock_destroy(client->sock);
	client->sock = NULL;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t stun_sock_on_rx_data(pj_stun_sock *stun_sock,
				      void *pkt,
				      unsigned pkt_len,
				      const pj_sockaddr_t *src_addr,
				      unsigned addr_len)
{
    struct stun_client *client;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(addr_len);

    client = (struct stun_client*) pj_stun_sock_get_user_data(stun_sock);
    client->on_rx_data_cnt++;

    return PJ_TRUE;
}

static pj_status_t create_client(pj_stun_config *cfg,
				 struct stun_client **p_client,
				 pj_bool_t destroy_on_err)
{
    pj_pool_t *pool;
    struct stun_client *client;
    pj_stun_sock_cfg sock_cfg;
    pj_stun_sock_cb cb;
    pj_status_t status;

    pool = pj_pool_create(mem, "test", 512, 512, NULL);
    client = PJ_POOL_ZALLOC_T(pool, struct stun_client);
    client->pool = pool;

    pj_stun_sock_cfg_default(&sock_cfg);

    pj_bzero(&cb, sizeof(cb));
    cb.on_status = &stun_sock_on_status;
    cb.on_rx_data = &stun_sock_on_rx_data;
    status = pj_stun_sock_create(cfg, NULL, pj_AF_INET(), &cb,
				 &sock_cfg, client, &client->sock);
    if (status != PJ_SUCCESS) {
	app_perror("   pj_stun_sock_create()", status);
	pj_pool_release(pool);
	return status;
    }

    pj_stun_sock_set_user_data(client->sock, client);

    pj_ioqueue_op_key_init(&client->send_key, sizeof(client->send_key));

    client->destroy_on_err = destroy_on_err;

    *p_client = client;

    return PJ_SUCCESS;
}


static void destroy_client(struct stun_client *client)
{
    if (client->sock) {
	pj_stun_sock_destroy(client->sock);
	client->sock = NULL;
    }
    pj_pool_release(client->pool);
}

static void handle_events(pj_stun_config *cfg, unsigned msec_delay)
{
    pj_time_val delay;

    pj_timer_heap_poll(cfg->timer_heap, NULL);

    delay.sec = 0;
    delay.msec = msec_delay;
    pj_time_val_normalize(&delay);

    pj_ioqueue_poll(cfg->ioqueue, &delay);
}

/*
 * Timeout test: scenario when no response is received from server
 */
static int timeout_test(pj_stun_config *cfg, pj_bool_t destroy_on_err)
{
    struct stun_srv *srv;
    struct stun_client *client;
    pj_str_t srv_addr;
    pj_time_val timeout, t;
    int ret = 0;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  timeout test [%d]", destroy_on_err));

    status =  create_client(cfg, &client, destroy_on_err);
    if (status != PJ_SUCCESS)
	return -10;

    status = create_server(client->pool, cfg->ioqueue, 0, &srv);
    if (status != PJ_SUCCESS) {
	destroy_client(client);
	return -20;
    }

    srv_addr = pj_str("127.0.0.1");
    status = pj_stun_sock_start(client->sock, &srv_addr, 
				pj_ntohs(srv->addr.ipv4.sin_port), NULL);
    if (status != PJ_SUCCESS) {
	destroy_server(srv);
	destroy_client(client);
	return -30;
    }

    /* Wait until on_status() callback is called with the failure */
    pj_gettimeofday(&timeout);
    timeout.sec += 60;
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (client->on_status_cnt==0 && PJ_TIME_VAL_LT(t, timeout));

    /* Check that callback with correct operation is called */
    if (client->last_op != PJ_STUN_SOCK_BINDING_OP) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting Binding operation status"));
	ret = -40;
	goto on_return;
    }
    /* .. and with the correct status */
    if (client->last_status != PJNATH_ESTUNTIMEDOUT) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting PJNATH_ESTUNTIMEDOUT"));
	ret = -50;
	goto on_return;
    }
    /* Check that server received correct retransmissions */
    if (srv->rx_cnt != PJ_STUN_MAX_TRANSMIT_COUNT) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting %d retransmissions, got %d",
		   PJ_STUN_MAX_TRANSMIT_COUNT, srv->rx_cnt));
	ret = -60;
	goto on_return;
    }
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -70;
	goto on_return;
    }

on_return:
    destroy_server(srv);
    destroy_client(client);
    return ret;
}


/*
 * Invalid response scenario: when server returns no MAPPED-ADDRESS or
 * XOR-MAPPED-ADDRESS attribute.
 */
static int missing_attr_test(pj_stun_config *cfg, pj_bool_t destroy_on_err)
{
    struct stun_srv *srv;
    struct stun_client *client;
    pj_str_t srv_addr;
    pj_time_val timeout, t;
    int ret = 0;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  missing attribute test [%d]", destroy_on_err));

    status =  create_client(cfg, &client, destroy_on_err);
    if (status != PJ_SUCCESS)
	return -110;

    status = create_server(client->pool, cfg->ioqueue, RESPOND_STUN, &srv);
    if (status != PJ_SUCCESS) {
	destroy_client(client);
	return -120;
    }

    srv_addr = pj_str("127.0.0.1");
    status = pj_stun_sock_start(client->sock, &srv_addr, 
				pj_ntohs(srv->addr.ipv4.sin_port), NULL);
    if (status != PJ_SUCCESS) {
	destroy_server(srv);
	destroy_client(client);
	return -130;
    }

    /* Wait until on_status() callback is called with the failure */
    pj_gettimeofday(&timeout);
    timeout.sec += 60;
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (client->on_status_cnt==0 && PJ_TIME_VAL_LT(t, timeout));

    /* Check that callback with correct operation is called */
    if (client->last_op != PJ_STUN_SOCK_BINDING_OP) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting Binding operation status"));
	ret = -140;
	goto on_return;
    }
    if (client->last_status != PJNATH_ESTUNNOMAPPEDADDR) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting PJNATH_ESTUNNOMAPPEDADDR"));
	ret = -150;
	goto on_return;
    }
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -170;
	goto on_return;
    }

on_return:
    destroy_server(srv);
    destroy_client(client);
    return ret;
}

/*
 * Keep-alive test.
 */
static int keep_alive_test(pj_stun_config *cfg)
{
    struct stun_srv *srv;
    struct stun_client *client;
    pj_sockaddr_in mapped_addr;
    pj_stun_sock_info info;
    pj_str_t srv_addr;
    pj_time_val timeout, t;
    int ret = 0;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  normal operation"));

    status =  create_client(cfg, &client, PJ_TRUE);
    if (status != PJ_SUCCESS)
	return -310;

    status = create_server(client->pool, cfg->ioqueue, RESPOND_STUN|WITH_XOR_MAPPED, &srv);
    if (status != PJ_SUCCESS) {
	destroy_client(client);
	return -320;
    }

    /*
     * Part 1: initial Binding resolution.
     */
    PJ_LOG(3,(THIS_FILE, "    initial Binding request"));
    srv_addr = pj_str("127.0.0.1");
    status = pj_stun_sock_start(client->sock, &srv_addr, 
				pj_ntohs(srv->addr.ipv4.sin_port), NULL);
    if (status != PJ_SUCCESS) {
	destroy_server(srv);
	destroy_client(client);
	return -330;
    }

    /* Wait until on_status() callback is called with success status */
    pj_gettimeofday(&timeout);
    timeout.sec += 60;
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (client->on_status_cnt==0 && PJ_TIME_VAL_LT(t, timeout));

    /* Check that callback with correct operation is called */
    if (client->last_op != PJ_STUN_SOCK_BINDING_OP) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting Binding operation status"));
	ret = -340;
	goto on_return;
    }
    if (client->last_status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting PJ_SUCCESS status"));
	ret = -350;
	goto on_return;
    }
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -370;
	goto on_return;
    }

    /* Get info */
    pj_bzero(&info, sizeof(info));
    pj_stun_sock_get_info(client->sock, &info);

    /* Check that we have server address */
    if (!pj_sockaddr_has_addr(&info.srv_addr)) {
	PJ_LOG(3,(THIS_FILE, "    error: missing server address"));
	ret = -380;
	goto on_return;
    }
    /* .. and bound address port must not be zero */
    if (pj_sockaddr_get_port(&info.bound_addr)==0) {
	PJ_LOG(3,(THIS_FILE, "    error: bound address is zero"));
	ret = -381;
	goto on_return;
    }
    /* .. and mapped address */
    if (!pj_sockaddr_has_addr(&info.mapped_addr)) {
	PJ_LOG(3,(THIS_FILE, "    error: missing mapped address"));
	ret = -382;
	goto on_return;
    }
    /* verify the mapped address */
    pj_sockaddr_in_init(&mapped_addr, &srv->ip_to_send, srv->port_to_send);
    if (pj_sockaddr_cmp(&info.mapped_addr, &mapped_addr) != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: mapped address mismatched"));
	ret = -383;
	goto on_return;
    }

    /* .. and at least one alias */
    if (info.alias_cnt == 0) {
	PJ_LOG(3,(THIS_FILE, "    error: must have at least one alias"));
	ret = -384;
	goto on_return;
    }
    if (!pj_sockaddr_has_addr(&info.aliases[0])) {
	PJ_LOG(3,(THIS_FILE, "    error: missing alias"));
	ret = -386;
	goto on_return;
    }


    /*
     * Part 2: sending and receiving data
     */
    PJ_LOG(3,(THIS_FILE, "    sending/receiving data"));

    /* Change server operation mode to echo back data */
    srv->flag = ECHO;

    /* Reset server */
    srv->rx_cnt = 0;

    /* Client sending data to echo server */
    {
	char txt[100];
	PJ_LOG(3,(THIS_FILE, "     sending to %s", pj_sockaddr_print(&info.srv_addr, txt, sizeof(txt), 3)));
    }
    status = pj_stun_sock_sendto(client->sock, NULL, &ret, sizeof(ret),
				 0, &info.srv_addr, 
				 pj_sockaddr_get_len(&info.srv_addr));
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	app_perror("    error: server sending data", status);
	ret = -390;
	goto on_return;
    }

    /* Wait for a short period until client receives data. We can't wait for
     * too long otherwise the keep-alive will kick in.
     */
    pj_gettimeofday(&timeout);
    timeout.sec += 1;
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (client->on_rx_data_cnt==0 && PJ_TIME_VAL_LT(t, timeout));

    /* Check that data is received in server */
    if (srv->rx_cnt == 0) {
	PJ_LOG(3,(THIS_FILE, "    error: server didn't receive data"));
	ret = -395;
	goto on_return;
    }

    /* Check that status is still OK */
    if (client->last_status != PJ_SUCCESS) {
	app_perror("    error: client has failed", client->last_status);
	ret = -400;
	goto on_return;
    }
    /* Check that data has been received */
    if (client->on_rx_data_cnt == 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client doesn't receive data"));
	ret = -410;
	goto on_return;
    }

    /*
     * Part 3: Successful keep-alive,
     */
    PJ_LOG(3,(THIS_FILE, "    successful keep-alive scenario"));

    /* Change server operation mode to normal mode */
    srv->flag = RESPOND_STUN | WITH_XOR_MAPPED;

    /* Reset server */
    srv->rx_cnt = 0;

    /* Reset client */
    client->on_status_cnt = 0;
    client->last_status = PJ_SUCCESS;
    client->on_rx_data_cnt = 0;

    /* Wait for keep-alive duration to see if client actually sends the
     * keep-alive.
     */
    pj_gettimeofday(&timeout);
    timeout.sec += (PJ_STUN_KEEP_ALIVE_SEC + 1);
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (PJ_TIME_VAL_LT(t, timeout));

    /* Check that server receives some packets */
    if (srv->rx_cnt == 0) {
	PJ_LOG(3, (THIS_FILE, "    error: no keep-alive was received"));
	ret = -420;
	goto on_return;
    }
    /* Check that client status is still okay and on_status() callback is NOT
     * called
     */
    /* No longer valid due to this ticket:
     *  http://trac.pjsip.org/repos/ticket/742

    if (client->on_status_cnt != 0) {
	PJ_LOG(3, (THIS_FILE, "    error: on_status() must not be called on successful"
			      "keep-alive when mapped-address does not change"));
	ret = -430;
	goto on_return;
    }
    */
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -440;
	goto on_return;
    }


    /*
     * Part 4: Successful keep-alive with IP address change
     */
    PJ_LOG(3,(THIS_FILE, "    mapped IP address change"));

    /* Change server operation mode to normal mode */
    srv->flag = RESPOND_STUN | WITH_XOR_MAPPED;

    /* Change mapped address in the response */
    srv->ip_to_send = pj_str("2.2.2.2");
    srv->port_to_send++;

    /* Reset server */
    srv->rx_cnt = 0;

    /* Reset client */
    client->on_status_cnt = 0;
    client->last_status = PJ_SUCCESS;
    client->on_rx_data_cnt = 0;

    /* Wait for keep-alive duration to see if client actually sends the
     * keep-alive.
     */
    pj_gettimeofday(&timeout);
    timeout.sec += (PJ_STUN_KEEP_ALIVE_SEC + 1);
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (PJ_TIME_VAL_LT(t, timeout));

    /* Check that server receives some packets */
    if (srv->rx_cnt == 0) {
	PJ_LOG(3, (THIS_FILE, "    error: no keep-alive was received"));
	ret = -450;
	goto on_return;
    }
    /* Check that on_status() callback is called (because mapped address
     * has changed)
     */
    if (client->on_status_cnt != 1) {
	PJ_LOG(3, (THIS_FILE, "    error: on_status() was not called"));
	ret = -460;
	goto on_return;
    }
    /* Check that callback was called with correct operation */
    if (client->last_op != PJ_STUN_SOCK_MAPPED_ADDR_CHANGE) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting keep-alive operation status"));
	ret = -470;
	goto on_return;
    }
    /* Check that last status is still success */
    if (client->last_status != PJ_SUCCESS) {
	PJ_LOG(3, (THIS_FILE, "    error: expecting successful status"));
	ret = -480;
	goto on_return;
    }
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -490;
	goto on_return;
    }

    /* Get info */
    pj_bzero(&info, sizeof(info));
    pj_stun_sock_get_info(client->sock, &info);

    /* Check that we have server address */
    if (!pj_sockaddr_has_addr(&info.srv_addr)) {
	PJ_LOG(3,(THIS_FILE, "    error: missing server address"));
	ret = -500;
	goto on_return;
    }
    /* .. and mapped address */
    if (!pj_sockaddr_has_addr(&info.mapped_addr)) {
	PJ_LOG(3,(THIS_FILE, "    error: missing mapped address"));
	ret = -510;
	goto on_return;
    }
    /* verify the mapped address */
    pj_sockaddr_in_init(&mapped_addr, &srv->ip_to_send, srv->port_to_send);
    if (pj_sockaddr_cmp(&info.mapped_addr, &mapped_addr) != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: mapped address mismatched"));
	ret = -520;
	goto on_return;
    }

    /* .. and at least one alias */
    if (info.alias_cnt == 0) {
	PJ_LOG(3,(THIS_FILE, "    error: must have at least one alias"));
	ret = -530;
	goto on_return;
    }
    if (!pj_sockaddr_has_addr(&info.aliases[0])) {
	PJ_LOG(3,(THIS_FILE, "    error: missing alias"));
	ret = -540;
	goto on_return;
    }


    /*
     * Part 5: Failed keep-alive
     */
    PJ_LOG(3,(THIS_FILE, "    failed keep-alive scenario"));
    
    /* Change server operation mode to respond without attribute */
    srv->flag = RESPOND_STUN;

    /* Reset server */
    srv->rx_cnt = 0;

    /* Reset client */
    client->on_status_cnt = 0;
    client->last_status = PJ_SUCCESS;
    client->on_rx_data_cnt = 0;

    /* Wait until on_status() is called with failure. */
    pj_gettimeofday(&timeout);
    timeout.sec += (PJ_STUN_KEEP_ALIVE_SEC + PJ_STUN_TIMEOUT_VALUE + 5);
    do {
	handle_events(cfg, 100);
	pj_gettimeofday(&t);
    } while (client->on_status_cnt==0 && PJ_TIME_VAL_LT(t, timeout));

    /* Check that callback with correct operation is called */
    if (client->last_op != PJ_STUN_SOCK_KEEP_ALIVE_OP) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting keep-alive operation status"));
	ret = -600;
	goto on_return;
    }
    if (client->last_status == PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting failed keep-alive"));
	ret = -610;
	goto on_return;
    }
    /* Check that client doesn't receive anything */
    if (client->on_rx_data_cnt != 0) {
	PJ_LOG(3,(THIS_FILE, "    error: client shouldn't have received anything"));
	ret = -620;
	goto on_return;
    }


on_return:
    destroy_server(srv);
    destroy_client(client);
    return ret;
}


#define DO_TEST(expr)	    \
	    capture_pjlib_state(&stun_cfg, &pjlib_state); \
	    ret = expr; \
	    if (ret != 0) goto on_return; \
	    ret = check_pjlib_state(&stun_cfg, &pjlib_state); \
	    if (ret != 0) goto on_return;


int stun_sock_test(void)
{
    struct pjlib_state pjlib_state;
    pj_stun_config stun_cfg;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer_heap = NULL;
    pj_pool_t *pool = NULL;
    pj_status_t status;
    int ret = 0;

    pool = pj_pool_create(mem, NULL, 512, 512, NULL);

    status = pj_ioqueue_create(pool, 12, &ioqueue);
    if (status != PJ_SUCCESS) {
	app_perror("   pj_ioqueue_create()", status);
	ret = -4;
	goto on_return;
    }

    status = pj_timer_heap_create(pool, 100, &timer_heap);
    if (status != PJ_SUCCESS) {
	app_perror("   pj_timer_heap_create()", status);
	ret = -8;
	goto on_return;
    }
    
    pj_stun_config_init(0, &stun_cfg, mem, 0, ioqueue, timer_heap);

    DO_TEST(timeout_test(&stun_cfg, PJ_FALSE));
    DO_TEST(timeout_test(&stun_cfg, PJ_TRUE));

    DO_TEST(missing_attr_test(&stun_cfg, PJ_FALSE));
    DO_TEST(missing_attr_test(&stun_cfg, PJ_TRUE));

    DO_TEST(keep_alive_test(&stun_cfg));

on_return:
    if (timer_heap) pj_timer_heap_destroy(timer_heap);
    if (ioqueue) pj_ioqueue_destroy(ioqueue);
    if (pool) pj_pool_release(pool);
    return ret;
}


