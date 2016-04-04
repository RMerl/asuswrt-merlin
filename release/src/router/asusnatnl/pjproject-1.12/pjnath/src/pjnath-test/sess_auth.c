/* $Id: sess_auth.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "sess_auth.c"

#define REALM	    "STUN session test"
#define USERNAME    "theusername"
#define PASSWORD    "thepassword"
#define NONCE	    "thenonce"


/* STUN config */
static pj_stun_config stun_cfg;


//////////////////////////////////////////////////////////////////////////////////////////
//
// SERVER PART
//


/* Server instance */
static struct server
{
    pj_pool_t		*pool;
    pj_sockaddr		 addr;
    pj_stun_session	*sess;

    pj_bool_t		 responding;
    unsigned		 recv_count;
    pj_stun_auth_type	 auth_type;

    pj_sock_t		 sock;

    pj_bool_t		 quit;
    pj_thread_t		*thread;
} *server;


static pj_status_t server_send_msg(pj_stun_session *sess,
				   void *token,
				   const void *pkt,
				   pj_size_t pkt_size,
				   const pj_sockaddr_t *dst_addr,
				   unsigned addr_len)
{
    pj_ssize_t len = pkt_size;

    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(token);

    return pj_sock_sendto(server->sock, pkt, &len, 0, dst_addr, addr_len);
}

static pj_status_t server_on_rx_request(pj_stun_session *sess,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_stun_rx_data *rdata,
					void *token,
					const pj_sockaddr_t *src_addr,
					unsigned src_addr_len)
{
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(token);

    return pj_stun_session_respond(sess, rdata, 0, NULL, NULL, PJ_TRUE, 
				   src_addr, src_addr_len);
}


static pj_status_t server_get_auth(void *user_data,
				   pj_pool_t *pool,
				   pj_str_t *realm,
				   pj_str_t *nonce)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    if (server->auth_type == PJ_STUN_AUTH_SHORT_TERM) {
	realm->slen = nonce->slen = 0;
    } else {
	*realm = pj_str(REALM);
	*nonce = pj_str(NONCE);
    }

    return PJ_SUCCESS;
}


static pj_status_t server_get_password( const pj_stun_msg *msg,
					void *user_data, 
				        const pj_str_t *realm,
				        const pj_str_t *username,
					pj_pool_t *pool,
					pj_stun_passwd_type *data_type,
					pj_str_t *data)
{
    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    if (server->auth_type == PJ_STUN_AUTH_SHORT_TERM) {
	if (realm && realm->slen) {
	    PJ_LOG(4,(THIS_FILE, "    server expecting short term"));
	    return -1;
	}
    } else {
	if (realm==NULL || realm->slen==0) {
	    PJ_LOG(4,(THIS_FILE, "    realm not present"));
	    return -1;
	}
    }

    if (pj_strcmp2(username, USERNAME) != 0) {
	PJ_LOG(4,(THIS_FILE, "    wrong username"));
	return -1;
    }

    *data_type = PJ_STUN_PASSWD_PLAIN;
    *data = pj_str(PASSWORD);

    return PJ_SUCCESS;
}


static pj_bool_t server_verify_nonce(const pj_stun_msg *msg,
				     void *user_data,
				     const pj_str_t *realm,
				     const pj_str_t *username,
				     const pj_str_t *nonce)
{
    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(realm);
    PJ_UNUSED_ARG(username);

    if (pj_strcmp2(nonce, NONCE) != 0)
	return PJ_FALSE;

    return PJ_TRUE;
}


static int server_thread(void *unused)
{
    PJ_UNUSED_ARG(unused);

    PJ_LOG(5,("", "    server thread started"));

    while (!server->quit) {
	pj_fd_set_t readset;
	pj_time_val delay = {0, 10};

	PJ_FD_ZERO(&readset);
	PJ_FD_SET(server->sock, &readset);

	if (pj_sock_select((int)server->sock+1, &readset, NULL, NULL, &delay)==1 
	    && PJ_FD_ISSET(server->sock, &readset)) 
	{
	    char pkt[1000];
	    pj_ssize_t len;
	    pj_status_t status;
	    pj_sockaddr src_addr;
	    int src_addr_len;

	    len = sizeof(pkt);
	    src_addr_len = sizeof(src_addr);

	    status = pj_sock_recvfrom(server->sock, pkt, &len, 0, &src_addr, &src_addr_len);
	    if (status != PJ_SUCCESS)
		continue;

	    /* Increment server's receive count */
	    server->recv_count++;

	    /* Only pass to server if we allow to respond */
	    if (!server->responding)
		continue;

	    pj_stun_session_on_rx_pkt(server->sess, pkt, len, 
				      PJ_STUN_CHECK_PACKET | PJ_STUN_IS_DATAGRAM,
				      NULL, NULL, &src_addr, src_addr_len);
	}
    }

    return 0;
}


/* Destroy server */
static void destroy_server(void)
{
    if (server->thread) {
	server->quit = PJ_TRUE;
	pj_thread_join(server->thread);
	pj_thread_destroy(server->thread);
    }

    if (server->sock) {
	pj_sock_close(server->sock);
    }

    if (server->sess) {
	pj_stun_session_destroy(server->sess);
    }

    pj_pool_release(server->pool);
    server = NULL;
}

/* Instantiate standard server */
static int create_std_server(pj_stun_auth_type auth_type,
			     pj_bool_t responding)
{
    pj_pool_t *pool;
    pj_stun_session_cb sess_cb;
    pj_stun_auth_cred cred;
    pj_status_t status;
    
    /* Create server */
    pool = pj_pool_create(mem, "server", 1000, 1000, NULL);
    server = PJ_POOL_ZALLOC_T(pool, struct server);
    server->pool = pool;
    server->auth_type = auth_type;
    server->responding = responding;

    /* Create STUN session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_rx_request = &server_on_rx_request;
    sess_cb.on_send_msg = &server_send_msg;
    status = pj_stun_session_create(&stun_cfg, "server", &sess_cb, PJ_FALSE, NULL, &server->sess);
    if (status != PJ_SUCCESS) {
	destroy_server();
	return -10;
    }

    /* Configure credential */
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_DYNAMIC;
    cred.data.dyn_cred.get_auth = &server_get_auth;
    cred.data.dyn_cred.get_password = &server_get_password;
    cred.data.dyn_cred.verify_nonce = &server_verify_nonce;
    status = pj_stun_session_set_credential(server->sess, auth_type, &cred);
    if (status != PJ_SUCCESS) {
	destroy_server();
	return -20;
    }

    /* Create socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &server->sock);
    if (status != PJ_SUCCESS) {
	destroy_server();
	return -30;
    }

    /* Bind */
    pj_sockaddr_in_init(&server->addr.ipv4, NULL, 0);
    status = pj_sock_bind(server->sock, &server->addr, pj_sockaddr_get_len(&server->addr));
    if (status != PJ_SUCCESS) {
	destroy_server();
	return -40;
    } else {
	/* Get the bound IP address */
	int namelen = sizeof(server->addr);
	pj_sockaddr addr;

	status = pj_sock_getsockname(server->sock, &server->addr, &namelen);
	if (status != PJ_SUCCESS) {
	    destroy_server();
	    return -43;
	}

	status = pj_gethostip(pj_AF_INET(), &addr);
	if (status != PJ_SUCCESS) {
	    destroy_server();
	    return -45;
	}

	pj_sockaddr_copy_addr(&server->addr, &addr);
    }


    /* Create worker thread */
    status = pj_thread_create(pool, "server", &server_thread, 0, 0, 0, &server->thread);
    if (status != PJ_SUCCESS) {
	destroy_server();
	return -30;
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// CLIENT PART
//

static struct client
{
    pj_pool_t	    *pool;
    pj_stun_session *sess;
    pj_sem_t	    *test_complete;
    pj_sock_t	     sock;

    pj_bool_t	     responding;
    unsigned	     recv_count;

    pj_status_t	     response_status;
    pj_stun_msg	    *response;

    pj_bool_t	     quit;
    pj_thread_t	    *thread;
} *client;


static pj_status_t client_send_msg(pj_stun_session *sess,
				   void *token,
				   const void *pkt,
				   pj_size_t pkt_size,
				   const pj_sockaddr_t *dst_addr,
				   unsigned addr_len)
{
    pj_ssize_t len = pkt_size;

    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(token);

    return pj_sock_sendto(client->sock, pkt, &len, 0, dst_addr, addr_len);
}


static void client_on_request_complete( pj_stun_session *sess,
					pj_status_t status,
					void *token,
					pj_stun_tx_data *tdata,
					const pj_stun_msg *response,
					const pj_sockaddr_t *src_addr,
					unsigned src_addr_len)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(tdata);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    client->response_status = status;
    if (response)
	client->response = pj_stun_msg_clone(client->pool, response);

    pj_sem_post(client->test_complete);
}


static int client_thread(void *unused)
{
    PJ_UNUSED_ARG(unused);

    while (!client->quit) {
	pj_fd_set_t readset;
	pj_time_val delay = {0, 10};

	/* Also poll the timer heap */
	pj_timer_heap_poll(stun_cfg.timer_heap, NULL);

	/* Poll client socket */
	PJ_FD_ZERO(&readset);
	PJ_FD_SET(client->sock, &readset);

	if (pj_sock_select((int)client->sock+1, &readset, NULL, NULL, &delay)==1 
	    && PJ_FD_ISSET(client->sock, &readset)) 
	{
	    char pkt[1000];
	    pj_ssize_t len;
	    pj_status_t status;
	    pj_sockaddr src_addr;
	    int src_addr_len;

	    len = sizeof(pkt);
	    src_addr_len = sizeof(src_addr);

	    status = pj_sock_recvfrom(client->sock, pkt, &len, 0, &src_addr, &src_addr_len);
	    if (status != PJ_SUCCESS)
		continue;

	    /* Increment client's receive count */
	    client->recv_count++;

	    /* Only pass to client if we allow to respond */
	    if (!client->responding)
		continue;

	    pj_stun_session_on_rx_pkt(client->sess, pkt, len, 
				      PJ_STUN_CHECK_PACKET | PJ_STUN_IS_DATAGRAM,
				      NULL, NULL, &src_addr, src_addr_len);
	}
 
    }

    return 0;
}


static void destroy_client_server(void)
{
    if (client->thread) {
	client->quit = 1;
	pj_thread_join(client->thread);
	pj_thread_destroy(client->thread);
    }

    if (client->sess)
	pj_stun_session_destroy(client->sess);

    if (client->sock)
	pj_sock_close(client->sock);

    if (client->test_complete)
	pj_sem_destroy(client->test_complete);

    if (server)
	destroy_server();
}

static int run_client_test(const char *title,

			   pj_bool_t server_responding,
			   pj_stun_auth_type server_auth_type,

			   pj_stun_auth_type client_auth_type,
			   const char *realm,
			   const char *username,
			   const char *nonce,
			   const char *password,
			   pj_bool_t dummy_mi,

			   pj_bool_t expected_error,
			   pj_status_t expected_code,
			   const char *expected_realm,
			   const char *expected_nonce,
			   
			   int (*more_check)(void))
{
    pj_pool_t *pool;
    pj_stun_session_cb sess_cb;
    pj_stun_auth_cred cred;
    pj_stun_tx_data *tdata;
    pj_status_t status;
    int rc = 0;
    
    PJ_LOG(3,(THIS_FILE, "   %s test", title));

    /* Create client */
    pool = pj_pool_create(mem, "client", 1000, 1000, NULL);
    client = PJ_POOL_ZALLOC_T(pool, struct client);
    client->pool = pool;
    client->responding = PJ_TRUE;

    /* Create STUN session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &client_on_request_complete;
    sess_cb.on_send_msg = &client_send_msg;
    status = pj_stun_session_create(&stun_cfg, "client", &sess_cb, PJ_FALSE, NULL, &client->sess);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -200;
    }

    /* Create semaphore */
    status = pj_sem_create(pool, "client", 0, 1, &client->test_complete);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -205;
    }

    /* Create client socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &client->sock);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -210;
    }

    /* Bind client socket */
    status = pj_sock_bind_in(client->sock, 0, 0);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -220;
    }

    /* Create client thread */
    status = pj_thread_create(pool, "client", &client_thread, NULL, 0, 0, &client->thread);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -230;
    }

    /* Initialize credential */
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    if (realm) cred.data.static_cred.realm = pj_str((char*)realm);
    if (username) cred.data.static_cred.username = pj_str((char*)username);
    if (nonce) cred.data.static_cred.nonce = pj_str((char*)nonce);
    if (password) cred.data.static_cred.data = pj_str((char*)password);
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    status = pj_stun_session_set_credential(client->sess, client_auth_type, &cred);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -240;
    }

    /* Create the server */
    status = create_std_server(server_auth_type, server_responding);
    if (status != 0) {
	destroy_client_server();
	return status;
    }

    /* Create request */
    status = pj_stun_session_create_req(client->sess, PJ_STUN_BINDING_REQUEST, 
					PJ_STUN_MAGIC, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -250;
    }

    /* Add our own attributes if client authentication is set to none */
    if (client_auth_type == PJ_STUN_AUTH_NONE) {
	pj_str_t tmp;
	if (realm)
	    pj_stun_msg_add_string_attr(tdata->pool, tdata->msg, PJ_STUN_ATTR_REALM, pj_cstr(&tmp, realm));
	if (username)
	    pj_stun_msg_add_string_attr(tdata->pool, tdata->msg, PJ_STUN_ATTR_USERNAME, pj_cstr(&tmp, username));
	if (nonce)
	    pj_stun_msg_add_string_attr(tdata->pool, tdata->msg, PJ_STUN_ATTR_NONCE, pj_cstr(&tmp, nonce));
	if (password) {
	    // ignored
	}
	if (dummy_mi) {
	    pj_stun_msgint_attr *mi;

	    pj_stun_msgint_attr_create(tdata->pool, &mi);
	    pj_stun_msg_add_attr(tdata->msg, &mi->hdr);
	}
	   
    }

    /* Send the request */
    status = pj_stun_session_send_msg(client->sess, NULL, PJ_FALSE, PJ_TRUE, &server->addr,
				      pj_sockaddr_get_len(&server->addr), tdata);
    if (status != PJ_SUCCESS) {
	destroy_client_server();
	return -270;
    }

    /* Wait until test complete */
    pj_sem_wait(client->test_complete);


    /* Verify response */
    if (expected_error) {
	if (expected_code != client->response_status) {
	    char e1[PJ_ERR_MSG_SIZE], e2[PJ_ERR_MSG_SIZE];

	    pj_strerror(expected_code, e1, sizeof(e1));
	    pj_strerror(client->response_status, e2, sizeof(e2));

	    PJ_LOG(3,(THIS_FILE, "    err: expecting %d (%s) but got %d (%s) response",
		      expected_code, e1, client->response_status, e2));
	    rc = -500;
	} 

    } else {
	int res_code = 0;
	pj_stun_realm_attr *arealm;
	pj_stun_nonce_attr *anonce;

	if (client->response_status != 0) {
	    PJ_LOG(3,(THIS_FILE, "    err: expecting successful operation but got error %d", 
		      client->response_status));
	    rc = -600;
	    goto done;
	} 

	if (PJ_STUN_IS_ERROR_RESPONSE(client->response->hdr.type)) {
	    pj_stun_errcode_attr *aerr = NULL;

	    aerr = (pj_stun_errcode_attr*)
		   pj_stun_msg_find_attr(client->response, 
					 PJ_STUN_ATTR_ERROR_CODE, 0);
	    if (aerr == NULL) {
		PJ_LOG(3,(THIS_FILE, "    err: received error response without ERROR-CODE"));
		rc = -610;
		goto done;
	    }

	    res_code = aerr->err_code;
	} else {
	    res_code = 0;
	}

	/* Check that code matches */
	if (expected_code != res_code) {
	    PJ_LOG(3,(THIS_FILE, "    err: expecting response code %d but got %d",
		      expected_code, res_code));
	    rc = -620;
	    goto done;
	}

	/* Find REALM and NONCE attributes */
	arealm = (pj_stun_realm_attr*)
	         pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_REALM, 0);
	anonce = (pj_stun_nonce_attr*)
	         pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_NONCE, 0);

	if (expected_realm) {
	    if (arealm == NULL) {
		PJ_LOG(3,(THIS_FILE, "    err: expecting REALM in esponse"));
		rc = -630;
		goto done;
	    }
	    if (pj_strcmp2(&arealm->value, expected_realm)!=0) {
		PJ_LOG(3,(THIS_FILE, "    err: REALM mismatch in response"));
		rc = -640;
		goto done;
	    }
	} else {
	    if (arealm != NULL) {
		PJ_LOG(3,(THIS_FILE, "    err: non expecting REALM in response"));
		rc = -650;
		goto done;
	    }
	}

	if (expected_nonce) {
	    if (anonce == NULL) {
		PJ_LOG(3,(THIS_FILE, "    err: expecting NONCE in esponse"));
		rc = -660;
		goto done;
	    }
	    if (pj_strcmp2(&anonce->value, expected_nonce)!=0) {
		PJ_LOG(3,(THIS_FILE, "    err: NONCE mismatch in response"));
		rc = -670;
		goto done;
	    }
	} else {
	    if (anonce != NULL) {
		PJ_LOG(3,(THIS_FILE, "    err: non expecting NONCE in response"));
		rc = -680;
		goto done;
	    }
	}
    }

    /* Our tests are okay so far. Let caller do some more tests if
     * it wants to.
     */
    if (rc==0 && more_check) {
	rc = (*more_check)();
    }


done:
    destroy_client_server();
    return rc;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// More verification
//

/* Retransmission test */
static int retransmit_check(void)
{

    if (server->recv_count != PJ_STUN_MAX_TRANSMIT_COUNT) {
	PJ_LOG(3,("", "    expecting %d retransmissions, got %d",
		  PJ_STUN_MAX_TRANSMIT_COUNT, server->recv_count));
	return -700;
    }
    if (client->recv_count != 0)
	return -710;

    return 0;
}

static int long_term_check1(void)
{
    /* SHOULD NOT contain USERNAME or MESSAGE-INTEGRITY */
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_USERNAME, 0))
	return -800;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0))
	return -800;

    return 0;
}

static int long_term_check2(void)
{
    /* response SHOULD NOT include a USERNAME, NONCE, REALM or 
     * MESSAGE-INTEGRITY attribute. 
     */
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_USERNAME, 0))
	return -900;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_NONCE, 0))
	return -910;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_REALM, 0))
	return -920;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0))
	return -930;

    return 0;
}

static int long_term_check3(void)
{
    /* response SHOULD NOT include a USERNAME, NONCE, and REALM */
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_USERNAME, 0))
	return -1000;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_NONCE, 0))
	return -1010;
    if (pj_stun_msg_find_attr(client->response, PJ_STUN_ATTR_REALM, 0))
	return -1020;

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// TEST MAIN
//


int sess_auth_test(void)
{
    pj_pool_t *pool;
    int rc;

    PJ_LOG(3,(THIS_FILE, "  STUN session authentication test"));

    /* Init STUN config */
    pj_stun_config_init(0, &stun_cfg, mem, 0, NULL, NULL);

    /* Create pool and timer heap */
    pool = pj_pool_create(mem, "authtest", 200, 200, NULL);
    if (pj_timer_heap_create(pool, 20, &stun_cfg.timer_heap)) {
	pj_pool_release(pool);
	return -5;
    }

    /* Basic retransmission test */
    rc = run_client_test("Retransmission",  // title
			 PJ_FALSE,	    // server responding
			 PJ_STUN_AUTH_NONE, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 NULL,		    // realm
			 NULL,		    // username
			 NULL,		    // nonce
			 NULL,		    // password
			 PJ_FALSE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJNATH_ESTUNTIMEDOUT,// expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &retransmit_check  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /*
     * Short term credential.
     * draft-ietf-behave-rfc3489bis-15#section-10.1.2
     */

    /*
     * If the message does not contain both a MESSAGE-INTEGRITY and a
     * USERNAME attribute, If the message is a request, the server MUST
     * reject the request with an error response.  This response MUST
     * use an error code of 400 (Bad Request).
     */
    rc = run_client_test("Missing MESSAGE-INTEGRITY (short term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 NULL,		    // realm
			 NULL,		    // username
			 NULL,		    // nonce
			 NULL,		    // password
			 PJ_FALSE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(400),// expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 NULL		    // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* If the USERNAME does not contain a username value currently valid
     * within the server: If the message is a request, the server MUST 
     * reject the request with an error response.  This response MUST use
     * an error code of 401 (Unauthorized).
     */
    rc = run_client_test("USERNAME mismatch (short term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_SHORT_TERM, // client auth
			 NULL,		    // realm
			 "anotheruser",	    // username
			 NULL,		    // nonce
			 "anotherpass",	    // password
			 PJ_FALSE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401),// expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 NULL		    // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* Using the password associated with the username, compute the value
     * for the message-integrity as described in Section 15.4.  If the
     * resulting value does not match the contents of the MESSAGE-
     * INTEGRITY attribute:
     *
     * - If the message is a request, the server MUST reject the request
     *   with an error response.  This response MUST use an error code
     *   of 401 (Unauthorized).
     */
    rc = run_client_test("MESSAGE-INTEGRITY mismatch (short term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_SHORT_TERM, // client auth
			 NULL,		    // realm
			 USERNAME,	    // username
			 NULL,		    // nonce
			 "anotherpass",	    // password
			 PJ_FALSE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401),// expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 NULL		    // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* USERNAME is not present, server must respond with 400 (Bad
     * Request).
     */
    rc = run_client_test("Missing USERNAME (short term)",// title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 NULL,		    // realm
			 NULL,		    // username
			 NULL,		    // nonce
			 NULL,		    // password
			 PJ_TRUE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(400),	    // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 NULL		    // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* Successful short term authentication */
    rc = run_client_test("Successful scenario (short term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_SHORT_TERM, // client auth
			 NULL,		    // realm
			 USERNAME,	    // username
			 NULL,		    // nonce
			 PASSWORD,	    // password
			 PJ_FALSE,	    // dummy MI
			 PJ_FALSE,	    // expected error
			 PJ_SUCCESS,	    // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 NULL		    // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /*
     * (our own) Extended tests for long term credential
     */

    /* When server wants to use short term credential, but request has
     * REALM, reject with .... 401 ???
     */
    rc = run_client_test("Unwanted REALM (short term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_SHORT_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 REALM,		    // realm
			 USERNAME,	    // username
			 NULL,		    // nonce
			 PASSWORD,	    // password
			 PJ_TRUE,	    // dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401),	    // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &long_term_check2  // more check
			 );
    if (rc != 0) {
	goto done;
    }


    /*
     * Long term credential.
     * draft-ietf-behave-rfc3489bis-15#section-10.2.2
     */

    /* If the message does not contain a MESSAGE-INTEGRITY attribute, the
     * server MUST generate an error response with an error code of 401
     * (Unauthorized).  This response MUST include a REALM value.  It is
     * RECOMMENDED that the REALM value be the domain name of the
     * provider of the STUN server.  The response MUST include a NONCE,
     * selected by the server.  The response SHOULD NOT contain a
     * USERNAME or MESSAGE-INTEGRITY attribute.
     */
    rc = run_client_test("Missing M-I (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 NULL,		    // client realm
			 NULL,		    // client username
			 NULL,		    // client nonce
			 NULL,		    // client password
			 PJ_FALSE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401), // expected code
			 REALM,		    // expected realm
			 NONCE,		    // expected nonce
			 &long_term_check1  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* If the message contains a MESSAGE-INTEGRITY attribute, but is
     * missing the USERNAME, REALM or NONCE attributes, the server MUST
     * generate an error response with an error code of 400 (Bad
     * Request).  This response SHOULD NOT include a USERNAME, NONCE,
     * REALM or MESSAGE-INTEGRITY attribute.
     */
    /* Missing USERNAME */
    rc = run_client_test("Missing USERNAME (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 REALM,		    // client realm
			 NULL,		    // client username
			 NONCE,		    // client nonce
			 PASSWORD,	    // client password
			 PJ_TRUE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(400), // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &long_term_check2  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* Missing REALM */
    rc = run_client_test("Missing REALM (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 NULL,		    // client realm
			 USERNAME,	    // client username
			 NONCE,		    // client nonce
			 PASSWORD,	    // client password
			 PJ_TRUE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(400), // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &long_term_check2  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* Missing NONCE */
    rc = run_client_test("Missing NONCE (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_NONE, // client auth
			 REALM,		    // client realm
			 USERNAME,	    // client username
			 NULL,		    // client nonce
			 PASSWORD,	    // client password
			 PJ_TRUE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(400), // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &long_term_check2  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* If the NONCE is no longer valid, the server MUST generate an error
     * response with an error code of 438 (Stale Nonce).  This response
     * MUST include a NONCE and REALM attribute and SHOULD NOT incude the
     * USERNAME or MESSAGE-INTEGRITY attribute.  Servers can invalidate
     * nonces in order to provide additional security.  See Section 4.3
     * of [RFC2617] for guidelines.    
     */
    // how??

    /* If the username in the USERNAME attribute is not valid, the server
     * MUST generate an error response with an error code of 401
     * (Unauthorized).  This response MUST include a REALM value.  It is
     * RECOMMENDED that the REALM value be the domain name of the
     * provider of the STUN server.  The response MUST include a NONCE,
     * selected by the server.  The response SHOULD NOT contain a
     * USERNAME or MESSAGE-INTEGRITY attribute.
     */
    rc = run_client_test("Invalid username (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_LONG_TERM, // client auth
			 REALM,		    // client realm
			 "anotheruser",	    // client username
			 "a nonce",	    // client nonce
			 "somepassword",    // client password
			 PJ_FALSE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401), // expected code
			 REALM,		    // expected realm
			 NONCE,		    // expected nonce
			 &long_term_check1  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /* Successful long term authentication */
    rc = run_client_test("Successful scenario (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_LONG_TERM, // client auth
			 REALM,		    // client realm
			 USERNAME,	    // client username
			 "anothernonce",    // client nonce
			 PASSWORD,	    // client password
			 PJ_FALSE,	    // client dummy MI
			 PJ_FALSE,	    // expected error
			 0,		    // expected code
			 NULL,		    // expected realm
			 NULL,		    // expected nonce
			 &long_term_check3  // more check
			 );
    if (rc != 0) {
	goto done;
    }

    /*
     * (our own) Extended tests for long term credential
     */

    /* If REALM doesn't match, server must respond with 401
     */
#if 0
    // STUN session now will just use the realm sent in the
    // response, so this test will fail because it will
    // authenticate successfully.

    rc = run_client_test("Invalid REALM (long term)",  // title
			 PJ_TRUE,	    // server responding
			 PJ_STUN_AUTH_LONG_TERM, // server auth
			 PJ_STUN_AUTH_LONG_TERM, // client auth
			 "anotherrealm",    // client realm
			 USERNAME,	    // client username
			 NONCE,		    // client nonce
			 PASSWORD,	    // client password
			 PJ_FALSE,	    // client dummy MI
			 PJ_TRUE,	    // expected error
			 PJ_STATUS_FROM_STUN_CODE(401), // expected code
			 REALM,		    // expected realm
			 NONCE,		    // expected nonce
			 &long_term_check1  // more check
			 );
    if (rc != 0) {
	goto done;
    }
#endif

    /* Invalid HMAC */

    /* Valid static short term, without NONCE */

    /* Valid static short term, WITH NONCE */

    /* Valid static long term (with NONCE */

    /* Valid dynamic short term (without NONCE) */

    /* Valid dynamic short term (with NONCE) */

    /* Valid dynamic long term (with NONCE) */


done:
    pj_timer_heap_destroy(stun_cfg.timer_heap);
    pj_pool_release(pool);
    return rc;
}
