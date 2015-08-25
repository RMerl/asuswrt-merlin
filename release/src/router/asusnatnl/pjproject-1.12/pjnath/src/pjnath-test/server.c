/* $Id: server.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "server.h"
#include "test.h"

#define THIS_FILE	"server.c"
#define MAX_STUN_PKT	1500
#define TURN_NONCE	"thenonce"

static pj_bool_t stun_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status);
static pj_bool_t turn_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status);
static pj_bool_t alloc_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status);

pj_status_t create_test_server(pj_stun_config *stun_cfg,
			       pj_uint32_t flags,
			       const char *domain,
			       test_server **p_test_srv)
{
    pj_pool_t *pool;
    test_server *test_srv;
    pj_sockaddr hostip;
    char strbuf[100];
    pj_status_t status;

    PJ_ASSERT_RETURN(stun_cfg && domain && p_test_srv, PJ_EINVAL);

    status = pj_gethostip(pj_AF_INET(), &hostip);
    if (status != PJ_SUCCESS)
	return status;

    pool = pj_pool_create(mem, THIS_FILE, 512, 512, NULL);
    test_srv = (test_server*) PJ_POOL_ZALLOC_T(pool, test_server);
    test_srv->pool = pool;
    test_srv->flags = flags;
    test_srv->stun_cfg = stun_cfg;

    pj_strdup2(pool, &test_srv->domain, domain);
    test_srv->username = pj_str(TURN_USERNAME);
    test_srv->passwd = pj_str(TURN_PASSWD);

    pj_ioqueue_op_key_init(&test_srv->send_key, sizeof(test_srv->send_key));

    if (flags & CREATE_DNS_SERVER) {
	status = pj_dns_server_create(mem, test_srv->stun_cfg->ioqueue,
				      pj_AF_INET(), DNS_SERVER_PORT,
				      0, &test_srv->dns_server);
	if (status != PJ_SUCCESS) {
	    destroy_test_server(test_srv);
	    return status;
	}

	/* Add DNS A record for the domain, for fallback */
	if (flags & CREATE_A_RECORD_FOR_DOMAIN) {
	    pj_dns_parsed_rr rr;
	    pj_str_t res_name;
	    pj_in_addr ip_addr;

	    pj_strdup2(pool, &res_name, domain);
	    ip_addr = hostip.ipv4.sin_addr;
	    pj_dns_init_a_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, &ip_addr);
	    pj_dns_server_add_rec(test_srv->dns_server, 1, &rr);
	}

    }

    if (flags & CREATE_STUN_SERVER) {
	pj_activesock_cb stun_sock_cb;
	pj_sockaddr bound_addr;

	pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
	stun_sock_cb.on_data_recvfrom = &stun_on_data_recvfrom;

	pj_sockaddr_in_init(&bound_addr.ipv4, NULL, STUN_SERVER_PORT);

	status = pj_activesock_create_udp(pool, &bound_addr, NULL, 
					  test_srv->stun_cfg->ioqueue,
					  &stun_sock_cb, test_srv, 
					  &test_srv->stun_sock, NULL);
	if (status != PJ_SUCCESS) {
	    destroy_test_server(test_srv);
	    return status;
	}

	status = pj_activesock_start_recvfrom(test_srv->stun_sock, pool,
					      MAX_STUN_PKT, 0);
	if (status != PJ_SUCCESS) {
	    destroy_test_server(test_srv);
	    return status;
	}

	if (test_srv->dns_server && (flags & CREATE_STUN_SERVER_DNS_SRV)) {
	    pj_str_t res_name, target;
	    pj_dns_parsed_rr rr;
	    pj_in_addr ip_addr;

	    /* Add DNS entries:
	     *  _stun._udp.domain 60 IN SRV 0 0 PORT stun.domain.
	     *  stun.domain IN A 127.0.0.1
	     */
	    pj_ansi_snprintf(strbuf, sizeof(strbuf),
			     "_stun._udp.%s", domain);
	    pj_strdup2(pool, &res_name, strbuf);
	    pj_ansi_snprintf(strbuf, sizeof(strbuf),
			     "stun.%s", domain);
	    pj_strdup2(pool, &target, strbuf);
	    pj_dns_init_srv_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, 0, 0, 
			       STUN_SERVER_PORT, &target);
	    pj_dns_server_add_rec(test_srv->dns_server, 1, &rr);

	    res_name = target;
	    ip_addr = hostip.ipv4.sin_addr;
	    pj_dns_init_a_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, &ip_addr);
	    pj_dns_server_add_rec(test_srv->dns_server, 1, &rr);
	}

    }

    if (flags & CREATE_TURN_SERVER) {
	pj_activesock_cb turn_sock_cb;
	pj_sockaddr bound_addr;

	pj_bzero(&turn_sock_cb, sizeof(turn_sock_cb));
	turn_sock_cb.on_data_recvfrom = &turn_on_data_recvfrom;

	pj_sockaddr_in_init(&bound_addr.ipv4, NULL, TURN_SERVER_PORT);

	status = pj_activesock_create_udp(pool, &bound_addr, NULL, 
					  test_srv->stun_cfg->ioqueue,
					  &turn_sock_cb, test_srv,
					  &test_srv->turn_sock, NULL);
	if (status != PJ_SUCCESS) {
	    destroy_test_server(test_srv);
	    return status;
	}

	status = pj_activesock_start_recvfrom(test_srv->turn_sock, pool,
					      MAX_STUN_PKT, 0);
	if (status != PJ_SUCCESS) {
	    destroy_test_server(test_srv);
	    return status;
	}

	if (test_srv->dns_server && (flags & CREATE_TURN_SERVER_DNS_SRV)) {
	    pj_str_t res_name, target;
	    pj_dns_parsed_rr rr;
	    pj_in_addr ip_addr;

	    /* Add DNS entries:
	     *  _turn._udp.domain 60 IN SRV 0 0 PORT turn.domain.
	     *  turn.domain IN A 127.0.0.1
	     */
	    pj_ansi_snprintf(strbuf, sizeof(strbuf),
			     "_turn._udp.%s", domain);
	    pj_strdup2(pool, &res_name, strbuf);
	    pj_ansi_snprintf(strbuf, sizeof(strbuf),
			     "turn.%s", domain);
	    pj_strdup2(pool, &target, strbuf);
	    pj_dns_init_srv_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, 0, 0, 
			       TURN_SERVER_PORT, &target);
	    pj_dns_server_add_rec(test_srv->dns_server, 1, &rr);

	    res_name = target;
	    ip_addr = hostip.ipv4.sin_addr;
	    pj_dns_init_a_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, &ip_addr);
	    pj_dns_server_add_rec(test_srv->dns_server, 1, &rr);
	}
    }

    *p_test_srv = test_srv;
    return PJ_SUCCESS;
}

void destroy_test_server(test_server *test_srv)
{
    unsigned i;

    PJ_ASSERT_ON_FAIL(test_srv, return);

    for (i=0; i<test_srv->turn_alloc_cnt; ++i) {
	pj_activesock_close(test_srv->turn_alloc[i].sock);
	pj_pool_release(test_srv->turn_alloc[i].pool);
    }
    test_srv->turn_alloc_cnt = 0;

    if (test_srv->turn_sock) {
	pj_activesock_close(test_srv->turn_sock);
	test_srv->turn_sock = NULL;
    }

    if (test_srv->stun_sock) {
	pj_activesock_close(test_srv->stun_sock);
	test_srv->stun_sock = NULL;
    }

    if (test_srv->dns_server) {
	pj_dns_server_destroy(test_srv->dns_server);
	test_srv->dns_server = NULL;
    }

    if (test_srv->pool) {
	pj_pool_t *pool = test_srv->pool;
	test_srv->pool = NULL;
	pj_pool_release(pool);
    }
}

static pj_bool_t stun_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status)
{
    test_server *test_srv;
    pj_stun_msg *req, *resp = NULL;
    pj_pool_t *pool;
    pj_ssize_t len;

    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    test_srv = (test_server*) pj_activesock_get_user_data(asock);
    pool = pj_pool_create(test_srv->stun_cfg->pf, NULL, 512, 512, NULL);

    status = pj_stun_msg_decode(pool, (pj_uint8_t*)data, size, 
				PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, 
				&req, NULL, NULL);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (req->hdr.type != PJ_STUN_BINDING_REQUEST) {
	pj_stun_msg_create_response(pool, req, PJ_STUN_SC_BAD_REQUEST, 
				    NULL, &resp);
	goto send_pkt;
    }

    status = pj_stun_msg_create_response(pool, req, 0, NULL, &resp);
    if (status != PJ_SUCCESS)
	goto on_return;

    pj_stun_msg_add_sockaddr_attr(pool, resp, PJ_STUN_ATTR_XOR_MAPPED_ADDR,
				  PJ_TRUE, src_addr, addr_len);

send_pkt:
    status = pj_stun_msg_encode(resp, (pj_uint8_t*)data, MAX_STUN_PKT, 
				0, NULL, &size);
    if (status != PJ_SUCCESS)
	goto on_return;

    len = size;
    status = pj_activesock_sendto(asock, &test_srv->send_key, data, &len,
				  0, src_addr, addr_len);

on_return:
    pj_pool_release(pool);
    return PJ_TRUE;
}


static pj_stun_msg* create_success_response(test_server *test_srv,
					    turn_allocation *alloc,
					    pj_stun_msg *req,
					    pj_pool_t *pool,
					    unsigned lifetime,
					    pj_str_t *auth_key)
{
    pj_stun_msg *resp;
    pj_str_t tmp;
    pj_status_t status;

    /* Create response */
    status = pj_stun_msg_create_response(pool, req, 0, NULL, &resp);
    if (status != PJ_SUCCESS) {
	return NULL;
    }
    /* Add TURN_NONCE */
    pj_stun_msg_add_string_attr(pool, resp, PJ_STUN_ATTR_NONCE, pj_cstr(&tmp, TURN_NONCE));
    /* Add LIFETIME */
    pj_stun_msg_add_uint_attr(pool, resp, PJ_STUN_ATTR_LIFETIME, lifetime);
    if (lifetime != 0) {
	/* Add XOR-RELAYED-ADDRESS */
	pj_stun_msg_add_sockaddr_attr(pool, resp, PJ_STUN_ATTR_XOR_RELAYED_ADDR, PJ_TRUE, &alloc->alloc_addr,
				      pj_sockaddr_get_len(&alloc->alloc_addr));
	/* Add XOR-MAPPED-ADDRESS */
	pj_stun_msg_add_sockaddr_attr(pool, resp, PJ_STUN_ATTR_XOR_MAPPED_ADDR, PJ_TRUE, &alloc->client_addr,
				      pj_sockaddr_get_len(&alloc->client_addr));
    }

    /* Add blank MESSAGE-INTEGRITY */
    pj_stun_msg_add_msgint_attr(pool, resp);

    /* Set auth key */
    pj_stun_create_key(pool, auth_key, &test_srv->domain, &test_srv->username,
		       PJ_STUN_PASSWD_PLAIN, &test_srv->passwd);

    return resp;
}


static pj_bool_t turn_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status)
{
    test_server *test_srv;
    pj_pool_t *pool;
    turn_allocation *alloc;
    pj_stun_msg *req, *resp = NULL;
    pj_str_t auth_key = { NULL, 0 };
    char client_info[PJ_INET6_ADDRSTRLEN+10];
    unsigned i;
    pj_ssize_t len;

    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    pj_sockaddr_print(src_addr, client_info, sizeof(client_info), 3);

    test_srv = (test_server*) pj_activesock_get_user_data(asock);
    pool = pj_pool_create(test_srv->stun_cfg->pf, NULL, 512, 512, NULL);

    /* Find the client */
    for (i=0; i<test_srv->turn_alloc_cnt; i++) {
	if (pj_sockaddr_cmp(&test_srv->turn_alloc[i].client_addr, src_addr)==0)
	    break;
    }


    if (pj_stun_msg_check((pj_uint8_t*)data, size, PJ_STUN_NO_FINGERPRINT_CHECK)!=PJ_SUCCESS)  {
	/* Not STUN message, this probably is a ChannelData */
	pj_turn_channel_data cd;
	const pj_turn_channel_data *pcd = (const pj_turn_channel_data*)data;
	pj_ssize_t sent;

	if (i==test_srv->turn_alloc_cnt) {
	    /* Invalid data */
	    PJ_LOG(1,(THIS_FILE, 
		      "TURN Server received strayed data"));
	    goto on_return;
	}

	alloc = &test_srv->turn_alloc[i];

	cd.ch_number = pj_ntohs(pcd->ch_number);
	cd.length = pj_ntohs(pcd->length);

	/* For UDP check the packet length */
	if (size < cd.length+sizeof(cd)) {
	    PJ_LOG(1,(THIS_FILE, 
		      "TURN Server: ChannelData discarded: UDP size error"));
	    goto on_return;
	}

	/* Lookup peer */
	for (i=0; i<alloc->perm_cnt; ++i) {
	    if (alloc->chnum[i] == cd.ch_number)
		break;
	}

	if (i==alloc->perm_cnt) {
	    PJ_LOG(1,(THIS_FILE, 
		      "TURN Server: ChannelData discarded: invalid channel number"));
	    goto on_return;
	}

	/* Relay the data to peer */
	sent = cd.length;
	pj_activesock_sendto(alloc->sock, &alloc->send_key,
			     pcd+1, &sent, 0,
			     &alloc->perm[i],
			     pj_sockaddr_get_len(&alloc->perm[i]));

	/* Done */
	goto on_return;
    }

    status = pj_stun_msg_decode(pool, (pj_uint8_t*)data, size, 
				PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET |
				    PJ_STUN_NO_FINGERPRINT_CHECK, 
				&req, NULL, NULL);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(1,("", "STUN message decode error from client %s: %s", client_info, errmsg));
	goto on_return;
    }

    if (i==test_srv->turn_alloc_cnt) {
	/* New client */
	//pj_str_t ip_addr;
	pj_stun_username_attr *uname;
	pj_activesock_cb alloc_sock_cb;
	turn_allocation *alloc;

	/* Must be Allocate request */
	if (req->hdr.type != PJ_STUN_ALLOCATE_REQUEST) {
	    PJ_LOG(1,(THIS_FILE, "Invalid %s %s from client %s",
		      pj_stun_get_method_name(req->hdr.type),
		      pj_stun_get_class_name(req->hdr.type),
		      client_info));

	    if (PJ_STUN_IS_REQUEST(req->hdr.type))
		pj_stun_msg_create_response(pool, req, PJ_STUN_SC_BAD_REQUEST, NULL, &resp);
	    goto send_pkt;
	}

	test_srv->turn_stat.rx_allocate_cnt++;

	/* Skip if we're not responding to Allocate request */
	if (!test_srv->turn_respond_allocate)
	    return PJ_TRUE;

	/* Check if we have too many clients */
	if (test_srv->turn_alloc_cnt == MAX_TURN_ALLOC) {
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_INSUFFICIENT_CAPACITY, NULL, &resp);
	    goto send_pkt;
	}

	/* Get USERNAME attribute */
	uname = (pj_stun_username_attr*)
		pj_stun_msg_find_attr(req, PJ_STUN_ATTR_USERNAME, 0);

	/* Reject if it doesn't have MESSAGE-INTEGRITY or USERNAME attributes or
	 * the user is incorrect
	 */
	if (pj_stun_msg_find_attr(req, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0) == NULL ||
	    uname==NULL || pj_stricmp2(&uname->value, TURN_USERNAME) != 0) 
	{
	    pj_str_t tmp;

	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_UNAUTHORIZED, NULL, &resp);
	    pj_stun_msg_add_string_attr(pool, resp, PJ_STUN_ATTR_REALM, &test_srv->domain);
	    pj_stun_msg_add_string_attr(pool, resp, PJ_STUN_ATTR_NONCE, pj_cstr(&tmp, TURN_NONCE));
	    goto send_pkt;
	}

	pj_bzero(&alloc_sock_cb, sizeof(alloc_sock_cb));
	alloc_sock_cb.on_data_recvfrom = &alloc_on_data_recvfrom;

	/* Create allocation */
	alloc = &test_srv->turn_alloc[test_srv->turn_alloc_cnt];
	alloc->perm_cnt = 0;
	alloc->test_srv = test_srv;
	pj_memcpy(&alloc->client_addr, src_addr, addr_len);
	pj_ioqueue_op_key_init(&alloc->send_key, sizeof(alloc->send_key));

	alloc->pool = pj_pool_create(test_srv->stun_cfg->pf, "alloc", 512, 512, NULL);

	/* Create relay socket */
	pj_sockaddr_in_init(&alloc->alloc_addr.ipv4, NULL, 0);
	pj_gethostip(pj_AF_INET(), &alloc->alloc_addr);

	status = pj_activesock_create_udp(alloc->pool, &alloc->alloc_addr, NULL, 
					  test_srv->stun_cfg->ioqueue,
					  &alloc_sock_cb, alloc, 
					  &alloc->sock, &alloc->alloc_addr);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(alloc->pool);
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_SERVER_ERROR, NULL, &resp);
	    goto send_pkt;
	}
	//pj_sockaddr_set_str_addr(pj_AF_INET(), &alloc->alloc_addr, &ip_addr);

	pj_activesock_set_user_data(alloc->sock, alloc);

	status = pj_activesock_start_recvfrom(alloc->sock, alloc->pool, 1500, 0);
	if (status != PJ_SUCCESS) {
	    pj_activesock_close(alloc->sock);
	    pj_pool_release(alloc->pool);
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_SERVER_ERROR, NULL, &resp);
	    goto send_pkt;
	}

	/* Create Data indication */
	status = pj_stun_msg_create(alloc->pool, PJ_STUN_DATA_INDICATION,
				    PJ_STUN_MAGIC, NULL, &alloc->data_ind);
	if (status != PJ_SUCCESS) {
	    pj_activesock_close(alloc->sock);
	    pj_pool_release(alloc->pool);
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_SERVER_ERROR, NULL, &resp);
	    goto send_pkt;
	}
	pj_stun_msg_add_sockaddr_attr(alloc->pool, alloc->data_ind, 
				      PJ_STUN_ATTR_XOR_PEER_ADDR, PJ_TRUE,
				      &alloc->alloc_addr,
				      pj_sockaddr_get_len(&alloc->alloc_addr));
	pj_stun_msg_add_binary_attr(alloc->pool, alloc->data_ind,
				    PJ_STUN_ATTR_DATA, (pj_uint8_t*)"", 1);

	/* Create response */
	resp = create_success_response(test_srv, alloc, req, pool, 600, &auth_key);
	if (resp == NULL) {
	    pj_activesock_close(alloc->sock);
	    pj_pool_release(alloc->pool);
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_SERVER_ERROR, NULL, &resp);
	    goto send_pkt;
	}

	++test_srv->turn_alloc_cnt;

    } else {
	alloc = &test_srv->turn_alloc[i];

	if (req->hdr.type == PJ_STUN_ALLOCATE_REQUEST) {

	    test_srv->turn_stat.rx_allocate_cnt++;

	    /* Skip if we're not responding to Allocate request */
	    if (!test_srv->turn_respond_allocate)
		return PJ_TRUE;

	    resp = create_success_response(test_srv, alloc, req, pool, 0, &auth_key);

	} else if (req->hdr.type == PJ_STUN_REFRESH_REQUEST) {
	    pj_stun_lifetime_attr *lf_attr;

	    test_srv->turn_stat.rx_refresh_cnt++;

	    /* Skip if we're not responding to Refresh request */
	    if (!test_srv->turn_respond_refresh)
		return PJ_TRUE;

	    lf_attr = (pj_stun_lifetime_attr*)
		      pj_stun_msg_find_attr(req, PJ_STUN_ATTR_LIFETIME, 0);
	    if (lf_attr && lf_attr->value != 0) {
		resp = create_success_response(test_srv, alloc, req, pool, 600, &auth_key);
		pj_array_erase(test_srv->turn_alloc, sizeof(test_srv->turn_alloc[0]),
			       test_srv->turn_alloc_cnt, i);
		--test_srv->turn_alloc_cnt;
	    } else
		resp = create_success_response(test_srv, alloc, req, pool, 0, &auth_key);
	} else if (req->hdr.type == PJ_STUN_CREATE_PERM_REQUEST) {
	    for (i=0; i<req->attr_count; ++i) {
		if (req->attr[i]->type == PJ_STUN_ATTR_XOR_PEER_ADDR) {
		    pj_stun_xor_peer_addr_attr *pa = (pj_stun_xor_peer_addr_attr*)req->attr[i];
		    unsigned j;

		    for (j=0; j<alloc->perm_cnt; ++j) {
			if (pj_sockaddr_cmp(&alloc->perm[j], &pa->sockaddr)==0)
			    break;
		    }

		    if (j==alloc->perm_cnt && alloc->perm_cnt < MAX_TURN_PERM) {
			char peer_info[PJ_INET6_ADDRSTRLEN];
			pj_sockaddr_print(&pa->sockaddr, peer_info, sizeof(peer_info), 3);

			pj_sockaddr_cp(&alloc->perm[alloc->perm_cnt], &pa->sockaddr);
			++alloc->perm_cnt;

			PJ_LOG(5,("", "Permission %s added to client %s, perm_cnt=%d", 
				      peer_info, client_info, alloc->perm_cnt));
		    }

		}
	    }
	    resp = create_success_response(test_srv, alloc, req, pool, 0, &auth_key);
	} else if (req->hdr.type == PJ_STUN_SEND_INDICATION) {
	    pj_stun_xor_peer_addr_attr *pa;
	    pj_stun_data_attr *da;

	    test_srv->turn_stat.rx_send_ind_cnt++;

	    pa = (pj_stun_xor_peer_addr_attr*)
		 pj_stun_msg_find_attr(req, PJ_STUN_ATTR_XOR_PEER_ADDR, 0);
	    da = (pj_stun_data_attr*)
		 pj_stun_msg_find_attr(req, PJ_STUN_ATTR_DATA, 0);
	    if (pa && da) {
		unsigned j;
		char peer_info[PJ_INET6_ADDRSTRLEN];
		pj_ssize_t sent;

		pj_sockaddr_print(&pa->sockaddr, peer_info, sizeof(peer_info), 3);

		for (j=0; j<alloc->perm_cnt; ++j) {
		    if (pj_sockaddr_cmp(&alloc->perm[j], &pa->sockaddr)==0)
			break;
		}

		if (j==alloc->perm_cnt) {
		    PJ_LOG(5,("", "SendIndication to %s is rejected (no permission)", 
			          peer_info, client_info, alloc->perm_cnt));
		} else {
		    PJ_LOG(5,(THIS_FILE, "Relaying %d bytes data from client %s to peer %s, "
					 "perm_cnt=%d", 
			      da->length, client_info, peer_info, alloc->perm_cnt));

		    sent = da->length;
		    pj_activesock_sendto(alloc->sock, &alloc->send_key,
					 da->data, &sent, 0,
					 &pa->sockaddr,
					 pj_sockaddr_get_len(&pa->sockaddr));
		}
	    } else {
		PJ_LOG(1,(THIS_FILE, "Invalid Send Indication from %s", client_info));
	    }
	} else if (req->hdr.type == PJ_STUN_CHANNEL_BIND_REQUEST) {
	    pj_stun_xor_peer_addr_attr *pa;
	    pj_stun_channel_number_attr *cna;
	    unsigned j, cn;

	    pa = (pj_stun_xor_peer_addr_attr*)
		 pj_stun_msg_find_attr(req, PJ_STUN_ATTR_XOR_PEER_ADDR, 0);
	    cna = (pj_stun_channel_number_attr*)
		 pj_stun_msg_find_attr(req, PJ_STUN_ATTR_CHANNEL_NUMBER, 0);
	    cn = PJ_STUN_GET_CH_NB(cna->value);

	    resp = create_success_response(test_srv, alloc, req, pool, 0, &auth_key);

	    for (j=0; j<alloc->perm_cnt; ++j) {
		if (pj_sockaddr_cmp(&alloc->perm[j], &pa->sockaddr)==0)
		    break;
	    }

	    if (i==alloc->perm_cnt) {
		if (alloc->perm_cnt==MAX_TURN_PERM) {
		    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_INSUFFICIENT_CAPACITY, NULL, &resp);
		    goto send_pkt;
		}
		pj_sockaddr_cp(&alloc->perm[i], &pa->sockaddr);
		++alloc->perm_cnt;
	    }
	    alloc->chnum[i] = cn;

	    resp = create_success_response(test_srv, alloc, req, pool, 0, &auth_key);

	} else if (PJ_STUN_IS_REQUEST(req->hdr.type)) {
	    pj_stun_msg_create_response(pool, req, PJ_STUN_SC_BAD_REQUEST, NULL, &resp);
	}
    }


send_pkt:
    if (resp) {
	status = pj_stun_msg_encode(resp, (pj_uint8_t*)data, MAX_STUN_PKT, 
				    0, &auth_key, &size);
	if (status != PJ_SUCCESS)
	    goto on_return;

	len = size;
	status = pj_activesock_sendto(asock, &test_srv->send_key, data, &len,
				      0, src_addr, addr_len);
    }

on_return:
    pj_pool_release(pool);
    return PJ_TRUE;
}

/* On received data from peer */
static pj_bool_t alloc_on_data_recvfrom(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       const pj_sockaddr_t *src_addr,
				       int addr_len,
				       pj_status_t status)
{
    turn_allocation *alloc;
    pj_stun_xor_peer_addr_attr *pa;
    pj_stun_data_attr *da;
    char peer_info[PJ_INET6_ADDRSTRLEN+10];
    char client_info[PJ_INET6_ADDRSTRLEN+10];
    pj_uint8_t buffer[1500];
    pj_ssize_t sent;
    unsigned i;

    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    alloc = (turn_allocation*) pj_activesock_get_user_data(asock);

    pj_sockaddr_print(&alloc->client_addr, client_info, sizeof(client_info), 3);
    pj_sockaddr_print(src_addr, peer_info, sizeof(peer_info), 3);

    /* Check that this peer has a permission */
    for (i=0; i<alloc->perm_cnt; ++i) {
	if (pj_sockaddr_get_len(&alloc->perm[i]) == (unsigned)addr_len &&
	    pj_memcmp(pj_sockaddr_get_addr(&alloc->perm[i]),
		      pj_sockaddr_get_addr(src_addr),
		      addr_len) == 0)
	{
	    break;
	}
    }
    if (i==alloc->perm_cnt) {
	PJ_LOG(5,("", "Client %s received %d bytes unauthorized data from peer %s", 
		      client_info, size, peer_info));
	if (alloc->perm_cnt == 0)
	    PJ_LOG(5,("", "Client %s has no permission", client_info));
	return PJ_TRUE;
    }

    /* Format a Data indication */
    pa = (pj_stun_xor_peer_addr_attr*)
	 pj_stun_msg_find_attr(alloc->data_ind, PJ_STUN_ATTR_XOR_PEER_ADDR, 0);
    da = (pj_stun_data_attr*)
	 pj_stun_msg_find_attr(alloc->data_ind, PJ_STUN_ATTR_DATA, 0);
    pj_assert(pa && da);

    pj_sockaddr_cp(&pa->sockaddr, src_addr);
    da->data = (pj_uint8_t*)data;
    da->length = size;

    /* Encode Data indication */
    status = pj_stun_msg_encode(alloc->data_ind, buffer, sizeof(buffer), 0,
				NULL, &size);
    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    /* Send */
    sent = size;
    PJ_LOG(5,("", "Forwarding %d bytes data from peer %s to client %s", 
		   sent, peer_info, client_info));

    pj_activesock_sendto(alloc->test_srv->turn_sock, &alloc->send_key, buffer,
			 &sent, 0, &alloc->client_addr,
			 pj_sockaddr_get_len(&alloc->client_addr));

    return PJ_TRUE;
}

