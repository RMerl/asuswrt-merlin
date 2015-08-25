/* $Id: ssl_sock.c 4392 2013-02-27 11:42:24Z ming $ */
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
#include <pjlib.h>


#define CERT_DIR		    "../build/"
#define CERT_CA_FILE		    CERT_DIR "cacert.pem"
#define CERT_FILE		    CERT_DIR "cacert.pem"
#define CERT_PRIVKEY_FILE	    CERT_DIR "privkey.pem"
#define CERT_PRIVKEY_PASS	    ""


#if INCLUDE_SSLSOCK_TEST

/* Global vars */
static int clients_num;

struct send_key {
    pj_ioqueue_op_key_t	op_key;
};


static int get_cipher_list(void) {
    pj_status_t status;
    pj_ssl_cipher ciphers[100];
    unsigned cipher_num;
    unsigned i;

    cipher_num = PJ_ARRAY_SIZE(ciphers);
    status = pj_ssl_cipher_get_availables(ciphers, &cipher_num);
    if (status != PJ_SUCCESS) {
	app_perror("...FAILED to get available ciphers", status);
	return status;
    }

    PJ_LOG(3, ("", "...Found %u ciphers:", cipher_num));
    for (i = 0; i < cipher_num; ++i) {
	const char* st;
	st = pj_ssl_cipher_name(ciphers[i]);
	if (st == NULL)
	    st = "[Unknown]";

	PJ_LOG(3, ("", "...%3u: 0x%08x=%s", i+1, ciphers[i], st));
    }

    return PJ_SUCCESS;
}


struct test_state
{
    pj_pool_t	   *pool;	    /* pool				    */
    pj_ioqueue_t   *ioqueue;	    /* ioqueue				    */
    pj_bool_t	    is_server;	    /* server role flag			    */
    pj_bool_t	    is_verbose;	    /* verbose flag, e.g: cert info	    */
    pj_bool_t	    echo;	    /* echo received data		    */
    pj_status_t	    err;	    /* error flag			    */
    unsigned	    sent;	    /* bytes sent			    */
    unsigned	    recv;	    /* bytes received			    */
    pj_uint8_t	    read_buf[256];  /* read buffer			    */
    pj_bool_t	    done;	    /* test done flag			    */
    char	   *send_str;	    /* data to send once connected	    */
    unsigned	    send_str_len;   /* send data length			    */
    pj_bool_t	    check_echo;	    /* flag to compare sent & echoed data   */
    const char	   *check_echo_ptr; /* pointer/cursor for comparing data    */
    struct send_key send_key;	    /* send op key			    */
};

static void dump_ssl_info(const pj_ssl_sock_info *si)
{
    const char *tmp_st;

    /* Print cipher name */
    tmp_st = pj_ssl_cipher_name(si->cipher);
    if (tmp_st == NULL)
	tmp_st = "[Unknown]";
    PJ_LOG(3, ("", ".....Cipher: %s", tmp_st));

    /* Print remote certificate info and verification result */
    if (si->remote_cert_info && si->remote_cert_info->subject.info.slen) 
    {
	char buf[2048];
	const char *verif_msgs[32];
	unsigned verif_msg_cnt;

	/* Dump remote TLS certificate info */
	PJ_LOG(3, ("", ".....Remote certificate info:"));
	pj_ssl_cert_info_dump(si->remote_cert_info, "  ", buf, sizeof(buf));
	PJ_LOG(3,("", "\n%s", buf));

	/* Dump remote TLS certificate verification result */
	verif_msg_cnt = PJ_ARRAY_SIZE(verif_msgs);
	pj_ssl_cert_get_verify_status_strings(si->verify_status,
					      verif_msgs, &verif_msg_cnt);
	PJ_LOG(3,("", ".....Remote certificate verification result: %s",
		  (verif_msg_cnt == 1? verif_msgs[0]:"")));
	if (verif_msg_cnt > 1) {
	    unsigned i;
	    for (i = 0; i < verif_msg_cnt; ++i)
		PJ_LOG(3,("", "..... - %s", verif_msgs[i]));
	}
    }
}


static pj_bool_t ssl_on_connect_complete(pj_ssl_sock_t *ssock,
					 pj_status_t status)
{
    struct test_state *st = (struct test_state*) 
		    	    pj_ssl_sock_get_user_data(ssock);
    void *read_buf[1];
    pj_ssl_sock_info info;
    char buf1[64], buf2[64];

    if (status != PJ_SUCCESS) {
	app_perror("...ERROR ssl_on_connect_complete()", status);
	goto on_return;
    }

    status = pj_ssl_sock_get_info(ssock, &info);
    if (status != PJ_SUCCESS) {
	app_perror("...ERROR pj_ssl_sock_get_info()", status);
	goto on_return;
    }

    pj_sockaddr_print((pj_sockaddr_t*)&info.local_addr, buf1, sizeof(buf1), 1);
    pj_sockaddr_print((pj_sockaddr_t*)&info.remote_addr, buf2, sizeof(buf2), 1);
    PJ_LOG(3, ("", "...Connected %s -> %s!", buf1, buf2));

    if (st->is_verbose)
	dump_ssl_info(&info);

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(ssock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
    if (status != PJ_SUCCESS) {
	app_perror("...ERROR pj_ssl_sock_start_read2()", status);
	goto on_return;
    }

    /* Start sending data */
    while (st->sent < st->send_str_len) {
	pj_ssize_t size;

	size = st->send_str_len - st->sent;
	status = pj_ssl_sock_send(ssock, (pj_ioqueue_op_key_t*)&st->send_key, 
				  st->send_str + st->sent, &size, 0);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    app_perror("...ERROR pj_ssl_sock_send()", status);
	    goto on_return;
	}

	if (status == PJ_SUCCESS)
	    st->sent += size;
	else
	    break;
    }

on_return:
    st->err = status;

    if (st->err != PJ_SUCCESS) {
	pj_ssl_sock_close(ssock);
	clients_num--;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}


static pj_bool_t ssl_on_accept_complete(pj_ssl_sock_t *ssock,
					pj_ssl_sock_t *newsock,
					const pj_sockaddr_t *src_addr,
					int src_addr_len)
{
    struct test_state *parent_st = (struct test_state*) 
				   pj_ssl_sock_get_user_data(ssock);
    struct test_state *st;
    void *read_buf[1];
    pj_ssl_sock_info info;
    char buf[64];
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr_len);

    /* Duplicate parent test state to newly accepted test state */
    st = (struct test_state*)pj_pool_zalloc(parent_st->pool, sizeof(struct test_state));
    *st = *parent_st;
    pj_ssl_sock_set_user_data(newsock, st);

    status = pj_ssl_sock_get_info(newsock, &info);
    if (status != PJ_SUCCESS) {
	app_perror("...ERROR pj_ssl_sock_get_info()", status);
	goto on_return;
    }

    pj_sockaddr_print(src_addr, buf, sizeof(buf), 1);
    PJ_LOG(3, ("", "...Accepted connection from %s", buf));

    if (st->is_verbose)
	dump_ssl_info(&info);

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(newsock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
    if (status != PJ_SUCCESS) {
	app_perror("...ERROR pj_ssl_sock_start_read2()", status);
	goto on_return;
    }

    /* Start sending data */
    while (st->sent < st->send_str_len) {
	pj_ssize_t size;

	size = st->send_str_len - st->sent;
	status = pj_ssl_sock_send(newsock, (pj_ioqueue_op_key_t*)&st->send_key, 
				  st->send_str + st->sent, &size, 0);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    app_perror("...ERROR pj_ssl_sock_send()", status);
	    goto on_return;
	}

	if (status == PJ_SUCCESS)
	    st->sent += size;
	else
	    break;
    }

on_return:
    st->err = status;

    if (st->err != PJ_SUCCESS) {
	pj_ssl_sock_close(newsock);
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t ssl_on_data_read(pj_ssl_sock_t *ssock,
				  void *data,
				  pj_size_t size,
				  pj_status_t status,
				  pj_size_t *remainder)
{
    struct test_state *st = (struct test_state*) 
			     pj_ssl_sock_get_user_data(ssock);

    PJ_UNUSED_ARG(remainder);
    PJ_UNUSED_ARG(data);

    if (size > 0) {
	pj_size_t consumed;

	/* Set random remainder */
	*remainder = pj_rand() % 100;

	/* Apply zero remainder if:
	 * - remainder is less than size, or
	 * - connection closed/error
	 * - echo/check_eco set
	 */
	if (*remainder > size || status != PJ_SUCCESS || st->echo || st->check_echo)
	    *remainder = 0;

	consumed = size - *remainder;
	st->recv += consumed;

	//printf("%.*s", consumed, (char*)data);

	pj_memmove(data, (char*)data + consumed, *remainder);

	/* Echo data when specified to */
	if (st->echo) {
	    pj_ssize_t size_ = consumed;
	    status = pj_ssl_sock_send(ssock, (pj_ioqueue_op_key_t*)&st->send_key, data, &size_, 0);
	    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
		app_perror("...ERROR pj_ssl_sock_send()", status);
		goto on_return;
	    }

	    if (status == PJ_SUCCESS)
		st->sent += size_;
	}

	/* Verify echoed data when specified to */
	if (st->check_echo) {
	    if (!st->check_echo_ptr)
		st->check_echo_ptr = st->send_str;

	    if (pj_memcmp(st->check_echo_ptr, data, consumed)) {
		status = PJ_EINVAL;
		app_perror("...ERROR echoed data not exact", status);
		goto on_return;
	    }
	    st->check_echo_ptr += consumed;

	    /* Echo received completely */
	    if (st->send_str_len == st->recv) {
		pj_ssl_sock_info info;
		char buf[64];

		status = pj_ssl_sock_get_info(ssock, &info);
		if (status != PJ_SUCCESS) {
		    app_perror("...ERROR pj_ssl_sock_get_info()", status);
		    goto on_return;
		}

		pj_sockaddr_print((pj_sockaddr_t*)&info.local_addr, buf, sizeof(buf), 1);
		PJ_LOG(3, ("", "...%s successfully recv %d bytes echo", buf, st->recv));
		st->done = PJ_TRUE;
	    }
	}
    }

    if (status != PJ_SUCCESS) {
	if (status == PJ_EEOF) {
	    status = PJ_SUCCESS;
	    st->done = PJ_TRUE;
	} else {
	    app_perror("...ERROR ssl_on_data_read()", status);
	}
    }

on_return:
    st->err = status;

    if (st->err != PJ_SUCCESS || st->done) {
	pj_ssl_sock_close(ssock);
	if (!st->is_server)
	    clients_num--;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t ssl_on_data_sent(pj_ssl_sock_t *ssock,
				  pj_ioqueue_op_key_t *op_key,
				  pj_ssize_t sent)
{
    struct test_state *st = (struct test_state*)
			     pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(op_key);

    if (sent < 0) {
	st->err = -sent;
    } else {
	st->sent += sent;

	/* Send more if any */
	while (st->sent < st->send_str_len) {
	    pj_ssize_t size;
	    pj_status_t status;

	    size = st->send_str_len - st->sent;
	    status = pj_ssl_sock_send(ssock, (pj_ioqueue_op_key_t*)&st->send_key, 
				      st->send_str + st->sent, &size, 0);
	    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
		app_perror("...ERROR pj_ssl_sock_send()", status);
		st->err = status;
		break;
	    }

	    if (status == PJ_SUCCESS)
		st->sent += size;
	    else
		break;
	}
    }

    if (st->err != PJ_SUCCESS) {
	pj_ssl_sock_close(ssock);
	if (!st->is_server)
	    clients_num--;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

#define HTTP_REQ		"GET / HTTP/1.0\r\n\r\n";
#define HTTP_SERVER_ADDR	"trac.pjsip.org"
#define HTTP_SERVER_PORT	443

static int https_client_test(unsigned ms_timeout)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock = NULL;
    pj_ssl_sock_param param;
    pj_status_t status;
    struct test_state state = {0};
    pj_sockaddr local_addr, rem_addr;
    pj_str_t tmp_st;

    pool = pj_pool_create(mem, "https_get", 256, 256, NULL);

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    state.pool = pool;
    state.send_str = HTTP_REQ;
    state.send_str_len = pj_ansi_strlen(state.send_str);
    state.is_verbose = PJ_TRUE;

    pj_ssl_sock_param_default(&param);
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.user_data = &state;
    param.server_name = pj_str((char*)HTTP_SERVER_ADDR);
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    param.proto = PJ_SSL_SOCK_PROTO_SSL23;
    pj_time_val_normalize(&param.timeout);

    status = pj_ssl_sock_create(pool, &param, &ssock);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    pj_sockaddr_init(PJ_AF_INET, &local_addr, pj_strset2(&tmp_st, "0.0.0.0"), 0);
    pj_sockaddr_init(PJ_AF_INET, &rem_addr, pj_strset2(&tmp_st, HTTP_SERVER_ADDR), HTTP_SERVER_PORT);
    status = pj_ssl_sock_start_connect(ssock, pool, &local_addr, &rem_addr, sizeof(rem_addr));
    if (status == PJ_SUCCESS) {
	ssl_on_connect_complete(ssock, PJ_SUCCESS);
    } else if (status == PJ_EPENDING) {
	status = PJ_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received */
    while (state.err == PJ_SUCCESS && !state.done) {
#ifdef PJ_SYMBIAN
	pj_symbianos_poll(-1, 1000);
#else
	pj_time_val delay = {0, 100};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer, &delay);
#endif
    }

    if (state.err) {
	status = state.err;
	goto on_return;
    }

    PJ_LOG(3, ("", "...Done!"));
    PJ_LOG(3, ("", ".....Sent/recv: %d/%d bytes", state.sent, state.recv));

on_return:
    if (ssock && !state.err && !state.done) 
	pj_ssl_sock_close(ssock);
    if (ioqueue)
	pj_ioqueue_destroy(ioqueue);
    if (timer)
	pj_timer_heap_destroy(timer);
    if (pool)
	pj_pool_release(pool);

    return status;
}


static int echo_test(pj_ssl_sock_proto srv_proto, pj_ssl_sock_proto cli_proto,
		     pj_ssl_cipher srv_cipher, pj_ssl_cipher cli_cipher,
		     pj_bool_t req_client_cert, pj_bool_t client_provide_cert)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t *ssock_cli = NULL;
    pj_ssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    pj_sockaddr addr, listen_addr;
    pj_ssl_cipher ciphers[1];
    pj_ssl_cert_t *cert = NULL;
    pj_status_t status;

    pool = pj_pool_create(mem, "ssl_echo", 256, 256, NULL);

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.ciphers = ciphers;

    /* Init default bind address */
    {
	pj_str_t tmp_st;
	pj_sockaddr_init(PJ_AF_INET, &addr, pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* === SERVER === */
    param.proto = srv_proto;
    param.user_data = &state_serv;
    param.ciphers_num = (srv_cipher == -1)? 0 : 1;
    param.require_client_cert = req_client_cert;
    ciphers[0] = srv_cipher;

    state_serv.pool = pool;
    state_serv.echo = PJ_TRUE;
    state_serv.is_server = PJ_TRUE;
    state_serv.is_verbose = PJ_TRUE;

    status = pj_ssl_sock_create(pool, &param, &ssock_serv);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Set server cert */
    {
	pj_str_t tmp1, tmp2, tmp3, tmp4;

	status = pj_ssl_cert_load_from_files(pool, 
					     pj_strset2(&tmp1, (char*)CERT_CA_FILE), 
					     pj_strset2(&tmp2, (char*)CERT_FILE), 
					     pj_strset2(&tmp3, (char*)CERT_PRIVKEY_FILE), 
					     pj_strset2(&tmp4, (char*)CERT_PRIVKEY_PASS), 
					     &cert);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}

	status = pj_ssl_sock_set_certificate(ssock_serv, pool, cert);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    status = pj_ssl_sock_start_accept(ssock_serv, pool, &addr, pj_sockaddr_get_len(&addr));
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Get listener address */
    {
	pj_ssl_sock_info info;

	pj_ssl_sock_get_info(ssock_serv, &info);
	pj_sockaddr_cp(&listen_addr, &info.local_addr);
    }

    /* === CLIENT === */
    param.proto = cli_proto;
    param.user_data = &state_cli;
    param.ciphers_num = (cli_cipher == -1)? 0 : 1;
    ciphers[0] = cli_cipher;

    state_cli.pool = pool;
    state_cli.check_echo = PJ_TRUE;
    state_cli.is_verbose = PJ_TRUE;

    {
	pj_time_val now;

	pj_gettimeofday(&now);
	pj_srand((unsigned)now.sec);
	state_cli.send_str_len = (pj_rand() % 5 + 1) * 1024 + pj_rand() % 1024;
    }
    state_cli.send_str = (char*)pj_pool_alloc(pool, state_cli.send_str_len);
    {
	unsigned i;
	for (i = 0; i < state_cli.send_str_len; ++i)
	    state_cli.send_str[i] = (char)(pj_rand() % 256);
    }

    status = pj_ssl_sock_create(pool, &param, &ssock_cli);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Set cert for client */
    {

	if (!client_provide_cert) {
	    pj_str_t tmp1, tmp2;

	    pj_strset2(&tmp1, (char*)CERT_CA_FILE);
	    pj_strset2(&tmp2, NULL);
	    status = pj_ssl_cert_load_from_files(pool, 
						 &tmp1, &tmp2, &tmp2, &tmp2,
						 &cert);
	    if (status != PJ_SUCCESS) {
		goto on_return;
	    }
	}

	status = pj_ssl_sock_set_certificate(ssock_cli, pool, cert);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    status = pj_ssl_sock_start_connect(ssock_cli, pool, &addr, &listen_addr, pj_sockaddr_get_len(&addr));
    if (status == PJ_SUCCESS) {
	ssl_on_connect_complete(ssock_cli, PJ_SUCCESS);
    } else if (status == PJ_EPENDING) {
	status = PJ_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while (!state_serv.err && !state_cli.err && !state_serv.done && !state_cli.done)
    {
#ifdef PJ_SYMBIAN
	pj_symbianos_poll(-1, 1000);
#else
	pj_time_val delay = {0, 100};
	pj_ioqueue_poll(ioqueue, &delay);
#endif
    }

    /* Clean up sockets */
    {
	pj_time_val delay = {0, 100};
	while (pj_ioqueue_poll(ioqueue, &delay) > 0);
    }

    if (state_serv.err || state_cli.err) {
	if (state_serv.err != PJ_SUCCESS)
	    status = state_serv.err;
	else
	    status = state_cli.err;

	goto on_return;
    }

    PJ_LOG(3, ("", "...Done!"));
    PJ_LOG(3, ("", ".....Sent/recv: %d/%d bytes", state_cli.sent, state_cli.recv));

on_return:
    if (ssock_serv)
	pj_ssl_sock_close(ssock_serv);
    if (ssock_cli && !state_cli.err && !state_cli.done) 
	pj_ssl_sock_close(ssock_cli);
    if (ioqueue)
	pj_ioqueue_destroy(ioqueue);
    if (pool)
	pj_pool_release(pool);

    return status;
}


static pj_bool_t asock_on_data_read(pj_activesock_t *asock,
				    void *data,
				    pj_size_t size,
				    pj_status_t status,
				    pj_size_t *remainder)
{
    struct test_state *st = (struct test_state*)
			     pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(data);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(remainder);

    if (status != PJ_SUCCESS) {
	if (status == PJ_EEOF) {
	    status = PJ_SUCCESS;
	    st->done = PJ_TRUE;
	} else {
	    app_perror("...ERROR asock_on_data_read()", status);
	}
    }

    st->err = status;

    if (st->err != PJ_SUCCESS || st->done) {
	pj_activesock_close(asock);
	if (!st->is_server)
	    clients_num--;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}


static pj_bool_t asock_on_connect_complete(pj_activesock_t *asock,
					   pj_status_t status)
{
    struct test_state *st = (struct test_state*)
			     pj_activesock_get_user_data(asock);

    if (status == PJ_SUCCESS) {
	void *read_buf[1];

	/* Start reading data */
	read_buf[0] = st->read_buf;
	status = pj_activesock_start_read2(asock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
	if (status != PJ_SUCCESS) {
	    app_perror("...ERROR pj_ssl_sock_start_read2()", status);
	}
    }

    st->err = status;

    if (st->err != PJ_SUCCESS) {
	pj_activesock_close(asock);
	if (!st->is_server)
	    clients_num--;
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t asock_on_accept_complete(pj_activesock_t *asock,
					  pj_sock_t newsock,
					  const pj_sockaddr_t *src_addr,
					  int src_addr_len)
{
    struct test_state *st;
    void *read_buf[1];
    pj_activesock_t *new_asock;
    pj_activesock_cb asock_cb = { 0 };
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    st = (struct test_state*) pj_activesock_get_user_data(asock);

    asock_cb.on_data_read = &asock_on_data_read;
    status = pj_activesock_create(st->pool, newsock, pj_SOCK_STREAM(), NULL, 
				  st->ioqueue, &asock_cb, st, &new_asock);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = pj_activesock_start_read2(new_asock, st->pool, 
				       sizeof(st->read_buf), 
				       (void**)read_buf, 0);
    if (status != PJ_SUCCESS) {
	app_perror("...ERROR pj_ssl_sock_start_read2()", status);
    }

on_return:
    st->err = status;

    if (st->err != PJ_SUCCESS)
	pj_activesock_close(new_asock);

    return PJ_TRUE;
}


/* Raw TCP socket try to connect to SSL socket server, once
 * connection established, it will just do nothing, SSL socket
 * server should be able to close the connection after specified
 * timeout period (set ms_timeout to 0 to disable timer).
 */
static int client_non_ssl(unsigned ms_timeout)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_activesock_t *asock_cli = NULL;
    pj_activesock_cb asock_cb = { 0 };
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_ssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    pj_sockaddr listen_addr;
    pj_ssl_cert_t *cert = NULL;
    pj_status_t status;

    pool = pj_pool_create(mem, "ssl_accept_raw_tcp", 256, 256, NULL);

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Set cert */
    {
	pj_str_t tmp1, tmp2, tmp3, tmp4;
	status = pj_ssl_cert_load_from_files(pool, 
					     pj_strset2(&tmp1, (char*)CERT_CA_FILE), 
					     pj_strset2(&tmp2, (char*)CERT_FILE), 
					     pj_strset2(&tmp3, (char*)CERT_PRIVKEY_FILE), 
					     pj_strset2(&tmp4, (char*)CERT_PRIVKEY_PASS), 
					     &cert);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    pj_time_val_normalize(&param.timeout);

    /* SERVER */
    param.user_data = &state_serv;
    state_serv.pool = pool;
    state_serv.is_server = PJ_TRUE;
    state_serv.is_verbose = PJ_TRUE;

    status = pj_ssl_sock_create(pool, &param, &ssock_serv);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_ssl_sock_set_certificate(ssock_serv, pool, cert);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Init bind address */
    {
	pj_str_t tmp_st;
	pj_sockaddr_init(PJ_AF_INET, &listen_addr, pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = pj_ssl_sock_start_accept(ssock_serv, pool, &listen_addr, pj_sockaddr_get_len(&listen_addr));
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Update listener address */
    {
	pj_ssl_sock_info info;

	pj_ssl_sock_get_info(ssock_serv, &info);
	pj_sockaddr_cp(&listen_addr, &info.local_addr);
    }

    /* CLIENT */
    state_cli.pool = pool;
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    asock_cb.on_connect_complete = &asock_on_connect_complete;
    asock_cb.on_data_read = &asock_on_data_read;
    status = pj_activesock_create(pool, sock, pj_SOCK_STREAM(), NULL, 
				  ioqueue, &asock_cb, &state_cli, &asock_cli);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_activesock_start_connect(asock_cli, pool, (pj_sockaddr_t*)&listen_addr, 
					 pj_sockaddr_get_len(&listen_addr));
    if (status == PJ_SUCCESS) {
	asock_on_connect_complete(asock_cli, PJ_SUCCESS);
    } else if (status == PJ_EPENDING) {
	status = PJ_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while (!state_serv.err && !state_cli.err && !state_serv.done && !state_cli.done)
    {
#ifdef PJ_SYMBIAN
	pj_symbianos_poll(-1, 1000);
#else
	pj_time_val delay = {0, 100};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer, &delay);
#endif
    }

    if (state_serv.err || state_cli.err) {
	if (state_serv.err != PJ_SUCCESS)
	    status = state_serv.err;
	else
	    status = state_cli.err;

	goto on_return;
    }

    PJ_LOG(3, ("", "...Done!"));

on_return:
    if (ssock_serv)
	pj_ssl_sock_close(ssock_serv);
    if (asock_cli && !state_cli.err && !state_cli.done)
	pj_activesock_close(asock_cli);
    if (timer)
	pj_timer_heap_destroy(timer);
    if (ioqueue)
	pj_ioqueue_destroy(ioqueue);
    if (pool)
	pj_pool_release(pool);

    return status;
}


/* SSL socket try to connect to raw TCP socket server, once
 * connection established, SSL socket will try to perform SSL
 * handshake. SSL client socket should be able to close the
 * connection after specified timeout period (set ms_timeout to 
 * 0 to disable timer).
 */
static int server_non_ssl(unsigned ms_timeout)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_activesock_t *asock_serv = NULL;
    pj_ssl_sock_t *ssock_cli = NULL;
    pj_activesock_cb asock_cb = { 0 };
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_ssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    pj_sockaddr addr, listen_addr;
    pj_status_t status;

    pool = pj_pool_create(mem, "ssl_connect_raw_tcp", 256, 256, NULL);

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* SERVER */
    state_serv.pool = pool;
    state_serv.ioqueue = ioqueue;

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Init bind address */
    {
	pj_str_t tmp_st;
	pj_sockaddr_init(PJ_AF_INET, &listen_addr, pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = pj_sock_bind(sock, (pj_sockaddr_t*)&listen_addr, 
			  pj_sockaddr_get_len((pj_sockaddr_t*)&listen_addr));
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_sock_listen(sock, PJ_SOMAXCONN);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    asock_cb.on_accept_complete = &asock_on_accept_complete;
    status = pj_activesock_create(pool, sock, pj_SOCK_STREAM(), NULL, 
				  ioqueue, &asock_cb, &state_serv, &asock_serv);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_activesock_start_accept(asock_serv, pool);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Update listener address */
    {
	int addr_len;

	addr_len = sizeof(listen_addr);
	pj_sock_getsockname(sock, (pj_sockaddr_t*)&listen_addr, &addr_len);
    }

    /* CLIENT */
    pj_ssl_sock_param_default(&param);
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    pj_time_val_normalize(&param.timeout);
    param.user_data = &state_cli;

    state_cli.pool = pool;
    state_cli.is_server = PJ_FALSE;
    state_cli.is_verbose = PJ_TRUE;

    status = pj_ssl_sock_create(pool, &param, &ssock_cli);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Init default bind address */
    {
	pj_str_t tmp_st;
	pj_sockaddr_init(PJ_AF_INET, &addr, pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = pj_ssl_sock_start_connect(ssock_cli, pool, 
				       (pj_sockaddr_t*)&addr, 
				       (pj_sockaddr_t*)&listen_addr, 
				       pj_sockaddr_get_len(&listen_addr));
    if (status != PJ_EPENDING) {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while ((!state_serv.err && !state_serv.done) || (!state_cli.err && !state_cli.done))
    {
#ifdef PJ_SYMBIAN
	pj_symbianos_poll(-1, 1000);
#else
	pj_time_val delay = {0, 100};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer, &delay);
#endif
    }

    if (state_serv.err || state_cli.err) {
	if (state_cli.err != PJ_SUCCESS)
	    status = state_cli.err;
	else
	    status = state_serv.err;

	goto on_return;
    }

    PJ_LOG(3, ("", "...Done!"));

on_return:
    if (asock_serv)
	pj_activesock_close(asock_serv);
    if (ssock_cli && !state_cli.err && !state_cli.done)
	pj_ssl_sock_close(ssock_cli);
    if (timer)
	pj_timer_heap_destroy(timer);
    if (ioqueue)
	pj_ioqueue_destroy(ioqueue);
    if (pool)
	pj_pool_release(pool);

    return status;
}


/* Test will perform multiple clients trying to connect to single server.
 * Once SSL connection established, echo test will be performed.
 */
static int perf_test(unsigned clients, unsigned ms_handshake_timeout)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t **ssock_cli = NULL;
    pj_ssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state *state_cli = NULL;
    pj_sockaddr addr, listen_addr;
    pj_ssl_cert_t *cert = NULL;
    pj_status_t status;
    unsigned i, cli_err = 0, tot_sent = 0, tot_recv = 0;
    pj_time_val start;

    pool = pj_pool_create(mem, "ssl_perf", 256, 256, NULL);

    status = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_timer_heap_create(pool, PJ_IOQUEUE_MAX_HANDLES, &timer);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Set cert */
    {
	pj_str_t tmp1, tmp2, tmp3, tmp4;

	status = pj_ssl_cert_load_from_files(pool, 
					     pj_strset2(&tmp1, (char*)CERT_CA_FILE), 
					     pj_strset2(&tmp2, (char*)CERT_FILE), 
					     pj_strset2(&tmp3, (char*)CERT_PRIVKEY_FILE), 
					     pj_strset2(&tmp4, (char*)CERT_PRIVKEY_PASS), 
					     &cert);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_handshake_timeout;
    pj_time_val_normalize(&param.timeout);

    /* Init default bind address */
    {
	pj_str_t tmp_st;
	pj_sockaddr_init(PJ_AF_INET, &addr, pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* SERVER */
    param.user_data = &state_serv;

    state_serv.pool = pool;
    state_serv.echo = PJ_TRUE;
    state_serv.is_server = PJ_TRUE;

    status = pj_ssl_sock_create(pool, &param, &ssock_serv);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_ssl_sock_set_certificate(ssock_serv, pool, cert);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    status = pj_ssl_sock_start_accept(ssock_serv, pool, &addr, pj_sockaddr_get_len(&addr));
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Get listening address for clients to connect to */
    {
	pj_ssl_sock_info info;
	char buf[64];

	pj_ssl_sock_get_info(ssock_serv, &info);
	pj_sockaddr_cp(&listen_addr, &info.local_addr);

	pj_sockaddr_print((pj_sockaddr_t*)&listen_addr, buf, sizeof(buf), 1);
	PJ_LOG(3, ("", "...Listener ready at %s", buf));
    }


    /* CLIENTS */
    clients_num = clients;
    param.timeout.sec = 0;
    param.timeout.msec = 0;

    /* Init random seed */
    {
	pj_time_val now;

	pj_gettimeofday(&now);
	pj_srand((unsigned)now.sec);
    }

    /* Allocate SSL socket pointers and test state */
    ssock_cli = (pj_ssl_sock_t**)pj_pool_calloc(pool, clients, sizeof(pj_ssl_sock_t*));
    state_cli = (struct test_state*)pj_pool_calloc(pool, clients, sizeof(struct test_state));

    /* Get start timestamp */
    pj_gettimeofday(&start);

    /* Setup clients */
    for (i = 0; i < clients; ++i) {
	param.user_data = &state_cli[i];

	state_cli[i].pool = pool;
	state_cli[i].check_echo = PJ_TRUE;
	state_cli[i].send_str_len = (pj_rand() % 5 + 1) * 1024 + pj_rand() % 1024;
	state_cli[i].send_str = (char*)pj_pool_alloc(pool, state_cli[i].send_str_len);
	{
	    unsigned j;
	    for (j = 0; j < state_cli[i].send_str_len; ++j)
		state_cli[i].send_str[j] = (char)(pj_rand() % 256);
	}

	status = pj_ssl_sock_create(pool, &param, &ssock_cli[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...ERROR pj_ssl_sock_create()", status);
	    cli_err++;
	    clients_num--;
	    continue;
	}

	status = pj_ssl_sock_start_connect(ssock_cli[i], pool, &addr, &listen_addr, pj_sockaddr_get_len(&addr));
	if (status == PJ_SUCCESS) {
	    ssl_on_connect_complete(ssock_cli[i], PJ_SUCCESS);
	} else if (status == PJ_EPENDING) {
	    status = PJ_SUCCESS;
	} else {
	    app_perror("...ERROR pj_ssl_sock_create()", status);
	    pj_ssl_sock_close(ssock_cli[i]);
	    ssock_cli[i] = NULL;
	    clients_num--;
	    cli_err++;
	    continue;
	}

	/* Give chance to server to accept this client */
	{
	    unsigned n = 5;

#ifdef PJ_SYMBIAN
	    while(n && pj_symbianos_poll(-1, 1000))
		n--;
#else
	    pj_time_val delay = {0, 100};
	    while(n && pj_ioqueue_poll(ioqueue, &delay) > 0)
		n--;
#endif
	}
    }

    /* Wait until everything has been sent/received or error */
    while (clients_num)
    {
#ifdef PJ_SYMBIAN
	pj_symbianos_poll(-1, 1000);
#else
	pj_time_val delay = {0, 100};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer, &delay);
#endif
    }

    /* Clean up sockets */
    {
	pj_time_val delay = {0, 500};
	while (pj_ioqueue_poll(ioqueue, &delay) > 0);
    }

    if (state_serv.err != PJ_SUCCESS) {
	status = state_serv.err;
	goto on_return;
    }

    PJ_LOG(3, ("", "...Done!"));

    /* SSL setup and data transfer duration */
    {
	pj_time_val stop;
	
	pj_gettimeofday(&stop);
	PJ_TIME_VAL_SUB(stop, start);

	PJ_LOG(3, ("", ".....Setup & data transfer duration: %d.%03ds", stop.sec, stop.msec));
    }

    /* Check clients status */
    for (i = 0; i < clients; ++i) {
	if (state_cli[i].err != PJ_SUCCESS)
	    cli_err++;

	tot_sent += state_cli[1].sent;
	tot_recv += state_cli[1].recv;
    }

    PJ_LOG(3, ("", ".....Clients: %d (%d errors)", clients, cli_err));
    PJ_LOG(3, ("", ".....Total sent/recv: %d/%d bytes", tot_sent, tot_recv));

on_return:
    if (ssock_serv) 
	pj_ssl_sock_close(ssock_serv);

    for (i = 0; i < clients; ++i) {
	if (ssock_cli[i] && !state_cli[i].err && !state_cli[i].done)
	    pj_ssl_sock_close(ssock_cli[i]);
    }
    if (ioqueue)
	pj_ioqueue_destroy(ioqueue);
    if (pool)
	pj_pool_release(pool);

    return status;
}

#if 0 && (!defined(PJ_SYMBIAN) || PJ_SYMBIAN==0)
pj_status_t pj_ssl_sock_ossl_test_send_buf(pj_pool_t *pool);
static int ossl_test_send_buf()
{
    pj_pool_t *pool;
    pj_status_t status;

    pool = pj_pool_create(mem, "send_buf", 256, 256, NULL);
    status = pj_ssl_sock_ossl_test_send_buf(pool);
    pj_pool_release(pool);
    return status;
}
#else
static int ossl_test_send_buf()
{
    return 0;
}
#endif

int ssl_sock_test(void)
{
    int ret;

    PJ_LOG(3,("", "..test ossl send buf"));
    ret = ossl_test_send_buf();
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..get cipher list test"));
    ret = get_cipher_list();
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..https client test"));
    ret = https_client_test(30000);
    // Ignore test result as internet connection may not be available.
    //if (ret != 0)
	//return ret;

#ifndef PJ_SYMBIAN
   
    /* On Symbian platforms, SSL socket is implemented using CSecureSocket, 
     * and it hasn't supported server mode, so exclude the following tests,
     * which require SSL server, for now.
     */

    PJ_LOG(3,("", "..echo test w/ TLSv1 and PJ_TLS_RSA_WITH_DES_CBC_SHA cipher"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_TLS1, PJ_SSL_SOCK_PROTO_TLS1, 
		    PJ_TLS_RSA_WITH_DES_CBC_SHA, PJ_TLS_RSA_WITH_DES_CBC_SHA, 
		    PJ_FALSE, PJ_FALSE);
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..echo test w/ SSLv23 and PJ_TLS_RSA_WITH_AES_256_CBC_SHA cipher"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_SSL23, PJ_SSL_SOCK_PROTO_SSL23, 
		    PJ_TLS_RSA_WITH_AES_256_CBC_SHA, PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
		    PJ_FALSE, PJ_FALSE);
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..echo test w/ incompatible proto"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_TLS1, PJ_SSL_SOCK_PROTO_SSL3, 
		    PJ_TLS_RSA_WITH_DES_CBC_SHA, PJ_TLS_RSA_WITH_DES_CBC_SHA,
		    PJ_FALSE, PJ_FALSE);
    if (ret == 0)
	return PJ_EBUG;

    PJ_LOG(3,("", "..echo test w/ incompatible ciphers"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_DEFAULT, PJ_SSL_SOCK_PROTO_DEFAULT, 
		    PJ_TLS_RSA_WITH_DES_CBC_SHA, PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
		    PJ_FALSE, PJ_FALSE);
    if (ret == 0)
	return PJ_EBUG;

    PJ_LOG(3,("", "..echo test w/ client cert required but not provided"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_DEFAULT, PJ_SSL_SOCK_PROTO_DEFAULT, 
		    PJ_TLS_RSA_WITH_AES_256_CBC_SHA, PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
		    PJ_TRUE, PJ_FALSE);
    if (ret == 0)
	return PJ_EBUG;

    PJ_LOG(3,("", "..echo test w/ client cert required and provided"));
    ret = echo_test(PJ_SSL_SOCK_PROTO_DEFAULT, PJ_SSL_SOCK_PROTO_DEFAULT, 
		    PJ_TLS_RSA_WITH_AES_256_CBC_SHA, PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
		    PJ_TRUE, PJ_TRUE);
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..performance test"));
    ret = perf_test(PJ_IOQUEUE_MAX_HANDLES/2 - 1, 0);
    if (ret != 0)
	return ret;

    PJ_LOG(3,("", "..client non-SSL (handshake timeout 5 secs)"));
    ret = client_non_ssl(5000);
    /* PJ_TIMEDOUT won't be returned as accepted socket is deleted silently */
    if (ret != 0)
	return ret;

#endif

    PJ_LOG(3,("", "..server non-SSL (handshake timeout 5 secs)"));
    ret = server_non_ssl(5000);
    if (ret != PJ_ETIMEDOUT)
	return ret;

    return 0;
}

#else	/* INCLUDE_SSLSOCK_TEST */
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_ssl_sock_test;
#endif	/* INCLUDE_SSLSOCK_TEST */
