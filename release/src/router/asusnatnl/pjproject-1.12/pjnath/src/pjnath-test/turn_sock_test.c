/* $Id: turn_sock_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "server.h"

#define SRV_DOMAIN	"pjsip.lab.domain"
#define KA_INTERVAL	50

struct test_result
{
    unsigned    state_called;
    unsigned    rx_data_cnt;
};

struct test_session
{
    pj_pool_t		*pool;
    pj_stun_config	*stun_cfg;
    pj_turn_sock	*turn_sock;
    pj_dns_resolver	*resolver;
    test_server		*test_srv;

    pj_bool_t		 destroy_called;
    int			 destroy_on_state;
    struct test_result	 result;
};

struct test_session_cfg
{
    struct {
	pj_bool_t	enable_dns_srv;
	int		destroy_on_state;
    } client;

    struct {
	pj_uint32_t	flags;
	pj_bool_t	respond_allocate;
	pj_bool_t	respond_refresh;
    } srv;
};

static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_sock *turn_sock, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state);

static void destroy_session(struct test_session *sess)
{
    if (sess->resolver) {
	pj_dns_resolver_destroy(sess->resolver, PJ_TRUE);
	sess->resolver = NULL;
    }

    if (sess->turn_sock) {
	if (!sess->destroy_called) {
	    sess->destroy_called = PJ_TRUE;
	    pj_turn_sock_destroy(sess->turn_sock);
	}
	sess->turn_sock = NULL;
    }

    if (sess->test_srv) {
	destroy_test_server(sess->test_srv);
	sess->test_srv = NULL;
    }

    if (sess->pool) {
	pj_pool_release(sess->pool);
    }
}



static int create_test_session(pj_stun_config  *stun_cfg,
			       const struct test_session_cfg *cfg,
			       struct test_session **p_sess)
{
    struct test_session *sess;
    pj_pool_t *pool;
    pj_turn_sock_cb turn_sock_cb;
    pj_turn_alloc_param alloc_param;
    pj_stun_auth_cred cred;
    pj_status_t status;

    /* Create client */
    pool = pj_pool_create(mem, "turnclient", 512, 512, NULL);
    sess = PJ_POOL_ZALLOC_T(pool, struct test_session);
    sess->pool = pool;
    sess->stun_cfg = stun_cfg;
    sess->destroy_on_state = cfg->client.destroy_on_state;

    pj_bzero(&turn_sock_cb, sizeof(turn_sock_cb));
    turn_sock_cb.on_rx_data = &turn_on_rx_data;
    turn_sock_cb.on_state = &turn_on_state;
    status = pj_turn_sock_create(sess->stun_cfg, pj_AF_INET(), PJ_TURN_TP_UDP,
				 &turn_sock_cb, 0, sess, &sess->turn_sock);
    if (status != PJ_SUCCESS) {
	destroy_session(sess);
	return -20;
    }

    /* Create test server */
    status = create_test_server(sess->stun_cfg, cfg->srv.flags,
				SRV_DOMAIN, &sess->test_srv);
    if (status != PJ_SUCCESS) {
	destroy_session(sess);
	return -30;
    }

    sess->test_srv->turn_respond_allocate = cfg->srv.respond_allocate;
    sess->test_srv->turn_respond_refresh = cfg->srv.respond_refresh;

    /* Create client resolver */
    status = pj_dns_resolver_create(mem, "resolver", 0, sess->stun_cfg->timer_heap,
				    sess->stun_cfg->ioqueue, &sess->resolver);
    if (status != PJ_SUCCESS) {
	destroy_session(sess);
	return -40;

    } else {
	pj_str_t dns_srv = pj_str("127.0.0.1");
	pj_uint16_t dns_srv_port = (pj_uint16_t) DNS_SERVER_PORT;
	status = pj_dns_resolver_set_ns(sess->resolver, 1, &dns_srv, &dns_srv_port);

	if (status != PJ_SUCCESS) {
	    destroy_session(sess);
	    return -50;
	}
    }

    /* Init TURN credential */
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    cred.data.static_cred.realm = pj_str(SRV_DOMAIN);
    cred.data.static_cred.username = pj_str(TURN_USERNAME);
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    cred.data.static_cred.data = pj_str(TURN_PASSWD);

    /* Init TURN allocate parameter */
    pj_turn_alloc_param_default(&alloc_param);
    alloc_param.ka_interval = KA_INTERVAL;

    /* Start the client */
    if (cfg->client.enable_dns_srv) {
	/* Use DNS SRV to resolve server, may fallback to DNS A */
	pj_str_t domain = pj_str(SRV_DOMAIN);
	status = pj_turn_sock_alloc(sess->turn_sock, &domain, TURN_SERVER_PORT,
				    sess->resolver, &cred, &alloc_param);

    } else {
	/* Explicitly specify server address */
	pj_str_t host = pj_str("127.0.0.1");
	status = pj_turn_sock_alloc(sess->turn_sock, &host, TURN_SERVER_PORT,
				    NULL, &cred, &alloc_param);

    }

    if (status != PJ_SUCCESS) {
	if (cfg->client.destroy_on_state >= PJ_TURN_STATE_READY) {
	    destroy_session(sess);
	    return -70;
	}
    }

    *p_sess = sess;
    return 0;
}


static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    struct test_session *sess;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);

    sess = (struct test_session*) pj_turn_sock_get_user_data(turn_sock);
    if (sess == NULL)
	return;

    sess->result.rx_data_cnt++;
}


static void turn_on_state(pj_turn_sock *turn_sock, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    struct test_session *sess;
    unsigned i, mask;

    PJ_UNUSED_ARG(old_state);

    sess = (struct test_session*) pj_turn_sock_get_user_data(turn_sock);
    if (sess == NULL)
	return;

    /* This state must not be called before */
    pj_assert((sess->result.state_called & (1<<new_state)) == 0);

    /* new_state must be greater than old_state */
    pj_assert(new_state > old_state);

    /* must not call any greater state before */
    mask = 0;
    for (i=new_state+1; i<31; ++i) mask |= (1 << i);

    pj_assert((sess->result.state_called & mask) == 0);

    sess->result.state_called |= (1 << new_state);

    if (new_state >= sess->destroy_on_state && !sess->destroy_called) {
	sess->destroy_called = PJ_TRUE;
	pj_turn_sock_destroy(turn_sock);
    }

    if (new_state >= PJ_TURN_STATE_DESTROYING) {
	pj_turn_sock_set_user_data(sess->turn_sock, NULL);
	sess->turn_sock = NULL;
    }
}


/////////////////////////////////////////////////////////////////////

static int state_progression_test(pj_stun_config  *stun_cfg)
{
    struct test_session_cfg test_cfg = 
    {
	{   /* Client cfg */
	    /* DNS SRV */   /* Destroy on state */
	    PJ_TRUE,	    0xFFFF
	},
	{   /* Server cfg */
	    0xFFFFFFFF,	    /* flags */
	    PJ_TRUE,	    /* respond to allocate  */
	    PJ_TRUE	    /* respond to refresh   */
	}
    };
    struct test_session *sess;
    unsigned i;
    int rc;

    PJ_LOG(3,("", "  state progression tests"));

    for (i=0; i<=1; ++i) {
	enum { TIMEOUT = 60 };
	pjlib_state pjlib_state;
	pj_turn_session_info info;
	struct test_result result;
	pj_time_val tstart;

	PJ_LOG(3,("", "   %s DNS SRV resolution",
	              (i==0? "without" : "with")));

	capture_pjlib_state(stun_cfg, &pjlib_state);

	test_cfg.client.enable_dns_srv = i;

	rc = create_test_session(stun_cfg, &test_cfg, &sess);
	if (rc != 0)
	    return rc;

	pj_bzero(&info, sizeof(info));

	/* Wait until state is READY */
	pj_gettimeofday(&tstart);
	while (sess->turn_sock) {
	    pj_time_val now;

	    poll_events(stun_cfg, 10, PJ_FALSE);
	    rc = pj_turn_sock_get_info(sess->turn_sock, &info);
	    if (rc!=PJ_SUCCESS)
		break;

	    if (info.state >= PJ_TURN_STATE_READY)
		break;

	    pj_gettimeofday(&now);
	    if (now.sec - tstart.sec > TIMEOUT) {
		PJ_LOG(3,("", "    timed-out"));
		break;
	    }
	}

	if (info.state != PJ_TURN_STATE_READY) {
	    PJ_LOG(3,("", "    error: state is not READY"));
	    destroy_session(sess);
	    return -130;
	}

	/* Deallocate */
	pj_turn_sock_destroy(sess->turn_sock);

	/* Wait for couple of seconds.
	 * We can't poll the session info since the session may have
	 * been destroyed
	 */
	poll_events(stun_cfg, 2000, PJ_FALSE);
	sess->turn_sock = NULL;
	pj_memcpy(&result, &sess->result, sizeof(result));
	destroy_session(sess);

	/* Check the result */
	if ((result.state_called & (1<<PJ_TURN_STATE_RESOLVING)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_RESOLVING is not called"));
	    return -140;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_RESOLVED)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_RESOLVED is not called"));
	    return -150;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_ALLOCATING)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_ALLOCATING is not called"));
	    return -155;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_READY)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_READY is not called"));
	    return -160;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_DEALLOCATING)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_DEALLOCATING is not called"));
	    return -170;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_DEALLOCATED)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_DEALLOCATED is not called"));
	    return -180;
	}

	if ((result.state_called & (1<<PJ_TURN_STATE_DESTROYING)) == 0) {
	    PJ_LOG(3,("", "    error: PJ_TURN_STATE_DESTROYING is not called"));
	    return -190;
	}

	poll_events(stun_cfg, 500, PJ_FALSE);
	rc = check_pjlib_state(stun_cfg, &pjlib_state);
	if (rc != 0) {
	    PJ_LOG(3,("", "    error: memory/timer-heap leak detected"));
	    return rc;
	}
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////

static int destroy_test(pj_stun_config  *stun_cfg,
			pj_bool_t with_dns_srv,
			pj_bool_t in_callback)
{
    struct test_session_cfg test_cfg = 
    {
	{   /* Client cfg */
	    /* DNS SRV */   /* Destroy on state */
	    PJ_TRUE,	    0xFFFF
	},
	{   /* Server cfg */
	    0xFFFFFFFF,	    /* flags */
	    PJ_TRUE,	    /* respond to allocate  */
	    PJ_TRUE	    /* respond to refresh   */
	}
    };
    struct test_session *sess;
    int target_state;
    int rc;

    PJ_LOG(3,("", "  destroy test %s %s",
	          (in_callback? "in callback" : ""),
		  (with_dns_srv? "with DNS srv" : "")
		  ));

    test_cfg.client.enable_dns_srv = with_dns_srv;

    for (target_state=PJ_TURN_STATE_RESOLVING; target_state<=PJ_TURN_STATE_READY; ++target_state) {
	enum { TIMEOUT = 60 };
	pjlib_state pjlib_state;
	pj_turn_session_info info;
	pj_time_val tstart;

	capture_pjlib_state(stun_cfg, &pjlib_state);

	PJ_LOG(3,("", "   %s", pj_turn_state_name((pj_turn_state_t)target_state)));

	if (in_callback)
	    test_cfg.client.destroy_on_state = target_state;

	rc = create_test_session(stun_cfg, &test_cfg, &sess);
	if (rc != 0)
	    return rc;

	if (in_callback) {
	    pj_gettimeofday(&tstart);
	    rc = 0;
	    while (sess->turn_sock) {
		pj_time_val now;

		poll_events(stun_cfg, 100, PJ_FALSE);

		pj_gettimeofday(&now);
		if (now.sec - tstart.sec > TIMEOUT) {
		    rc = -7;
		    break;
		}
	    }

	} else {
	    pj_gettimeofday(&tstart);
	    rc = 0;
	    while (sess->turn_sock) {
		pj_time_val now;

		poll_events(stun_cfg, 1, PJ_FALSE);

		pj_turn_sock_get_info(sess->turn_sock, &info);
		
		if (info.state >= target_state) {
		    pj_turn_sock_destroy(sess->turn_sock);
		    break;
		}

		pj_gettimeofday(&now);
		if (now.sec - tstart.sec > TIMEOUT) {
		    rc = -8;
		    break;
		}
	    }
	}


	if (rc != 0) {
	    PJ_LOG(3,("", "    error: timeout"));
	    return rc;
	}

	poll_events(stun_cfg, 1000, PJ_FALSE);
	destroy_session(sess);

	rc = check_pjlib_state(stun_cfg, &pjlib_state);
	if (rc != 0) {
	    PJ_LOG(3,("", "    error: memory/timer-heap leak detected"));
	    return rc;
	}
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////

int turn_sock_test(void)
{
    pj_pool_t *pool;
    pj_stun_config stun_cfg;
    int i, rc = 0;

    pool = pj_pool_create(mem, "turntest", 512, 512, NULL);
    rc = create_stun_config(pool, &stun_cfg);
    if (rc != PJ_SUCCESS) {
	pj_pool_release(pool);
	return -2;
    }

    rc = state_progression_test(&stun_cfg);
    if (rc != 0) 
	goto on_return;

    for (i=0; i<=1; ++i) {
	int j;
	for (j=0; j<=1; ++j) {
	    rc = destroy_test(&stun_cfg, i, j);
	    if (rc != 0)
		goto on_return;
	}
    }

on_return:
    destroy_stun_config(&stun_cfg);
    pj_pool_release(pool);
    return rc;
}

