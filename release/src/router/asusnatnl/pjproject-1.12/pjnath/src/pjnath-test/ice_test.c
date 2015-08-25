/* $Id: ice_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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

enum
{
    NO	= 0,
    YES	= 1,
    SRV	= 3,
};

#define NODELAY		0xFFFFFFFF
#define SRV_DOMAIN	"pjsip.lab.domain"

#define INDENT		"    "

/* Client flags */
enum
{
    WRONG_TURN	= 1,
    DEL_ON_ERR	= 2,
};


/* Test results */
struct test_result
{
    pj_status_t	init_status;	/* init successful?		*/
    pj_status_t	nego_status;	/* negotiation successful?	*/
    unsigned	rx_cnt[4];	/* Number of data received	*/
};


/* Test session configuration */
struct test_cfg
{
    pj_ice_sess_role role;	/* Role.			*/
    unsigned	comp_cnt;	/* Component count		*/
    unsigned    enable_host;	/* Enable host candidates	*/
    unsigned    enable_stun;	/* Enable srflx candidates	*/
    unsigned    enable_turn;	/* Enable turn candidates	*/
    unsigned	client_flag;	/* Client flags			*/

    unsigned    answer_delay;	/* Delay before sending SDP	*/
    unsigned    send_delay;	/* Delay before sending data	*/
    unsigned    destroy_delay;	/* Delay before destroy()	*/

    struct test_result expected;/* Expected result		*/

    pj_bool_t   nom_regular;	/* Use regular nomination?	*/
};

/* ICE endpoint state */
struct ice_ept
{
    struct test_cfg	 cfg;	/* Configuratino.		*/
    pj_ice_strans	*ice;	/* ICE stream transport		*/
    struct test_result	 result;/* Test result.			*/

    pj_str_t		 ufrag;	/* username fragment.		*/
    pj_str_t		 pass;	/* password			*/
};

/* The test session */
struct test_sess
{
    pj_pool_t		*pool;
    pj_stun_config	*stun_cfg;
    pj_dns_resolver	*resolver;

    test_server		*server;

    unsigned		 server_flag;
    struct ice_ept	 caller;
    struct ice_ept	 callee;
};


static void ice_on_rx_data(pj_ice_strans *ice_st,
			   unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len);
static void ice_on_ice_complete(pj_ice_strans *ice_st, 
			        pj_ice_strans_op op,
					pj_status_t status,
					pj_sockaddr *turn_mapped_addr);
static void destroy_sess(struct test_sess *sess, unsigned wait_msec);

/* Create ICE stream transport */
static int create_ice_strans(struct test_sess *test_sess,
			     struct ice_ept *ept,
			     pj_ice_strans **p_ice)
{
    pj_ice_strans *ice;
    pj_ice_strans_cb ice_cb;
    pj_ice_strans_cfg ice_cfg;
    pj_sockaddr hostip;
    char serverip[PJ_INET6_ADDRSTRLEN];
    pj_status_t status;

    status = pj_gethostip(pj_AF_INET(), &hostip);
    if (status != PJ_SUCCESS)
	return -1030;

    pj_sockaddr_print(&hostip, serverip, sizeof(serverip), 0);

    /* Init callback structure */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_rx_data = &ice_on_rx_data;
    ice_cb.on_ice_complete = &ice_on_ice_complete;

    /* Init ICE stream transport configuration structure */
    pj_ice_strans_cfg_default(&ice_cfg);
    pj_memcpy(&ice_cfg.stun_cfg, test_sess->stun_cfg, sizeof(pj_stun_config));
    if ((ept->cfg.enable_stun & SRV)==SRV || (ept->cfg.enable_turn & SRV)==SRV)
	ice_cfg.resolver = test_sess->resolver;

    if (ept->cfg.enable_stun & YES) {
	if ((ept->cfg.enable_stun & SRV) == SRV) {
	    ice_cfg.stun.server = pj_str(SRV_DOMAIN);
	} else {
	    ice_cfg.stun.server = pj_str(serverip);
	}
	ice_cfg.stun.port = STUN_SERVER_PORT;
    }

    if (ept->cfg.enable_host == 0) {
	ice_cfg.stun.max_host_cands = 0;
    } else {
	//ice_cfg.stun.no_host_cands = PJ_FALSE;
	ice_cfg.stun.loop_addr = PJ_TRUE;
    }


    if (ept->cfg.enable_turn & YES) {
	if ((ept->cfg.enable_turn & SRV) == SRV) {
	    ice_cfg.turn.server = pj_str(SRV_DOMAIN);
	} else {
	    ice_cfg.turn.server = pj_str(serverip);
	}
	ice_cfg.turn.port = TURN_SERVER_PORT;
	ice_cfg.turn.conn_type = PJ_TURN_TP_UDP;
	ice_cfg.turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	ice_cfg.turn.auth_cred.data.static_cred.realm = pj_str(SRV_DOMAIN);
	if (ept->cfg.client_flag & WRONG_TURN)
	    ice_cfg.turn.auth_cred.data.static_cred.username = pj_str("xxx");
	else
	    ice_cfg.turn.auth_cred.data.static_cred.username = pj_str(TURN_USERNAME);
	ice_cfg.turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	ice_cfg.turn.auth_cred.data.static_cred.data = pj_str(TURN_PASSWD);
    }

    /* Create ICE stream transport */
    status = pj_ice_strans_create(NULL, &ice_cfg, ept->cfg.comp_cnt,
				  (void*)ept, &ice_cb,
				  &ice);
    if (status != PJ_SUCCESS) {
	app_perror(INDENT "err: pj_ice_strans_create()", status);
	return status;
    }

    pj_create_unique_string(test_sess->pool, &ept->ufrag);
    pj_create_unique_string(test_sess->pool, &ept->pass);

    /* Looks alright */
    *p_ice = ice;
    return PJ_SUCCESS;
}
			     
/* Create test session */
static int create_sess(pj_stun_config *stun_cfg,
		       unsigned server_flag,
		       struct test_cfg *caller_cfg,
		       struct test_cfg *callee_cfg,
		       struct test_sess **p_sess)
{
    pj_pool_t *pool;
    struct test_sess *sess;
    pj_str_t ns_ip;
    pj_uint16_t ns_port;
    unsigned flags;
    pj_status_t status;

    /* Create session structure */
    pool = pj_pool_create(mem, "testsess", 512, 512, NULL);
    sess = PJ_POOL_ZALLOC_T(pool, struct test_sess);
    sess->pool = pool;
    sess->stun_cfg = stun_cfg;

    pj_memcpy(&sess->caller.cfg, caller_cfg, sizeof(*caller_cfg));
    sess->caller.result.init_status = sess->caller.result.nego_status = PJ_EPENDING;

    pj_memcpy(&sess->callee.cfg, callee_cfg, sizeof(*callee_cfg));
    sess->callee.result.init_status = sess->callee.result.nego_status = PJ_EPENDING;

    /* Create server */
    flags = server_flag;
    status = create_test_server(stun_cfg, flags, SRV_DOMAIN, &sess->server);
    if (status != PJ_SUCCESS) {
	app_perror(INDENT "error: create_test_server()", status);
	destroy_sess(sess, 500);
	return -10;
    }
    sess->server->turn_respond_allocate = 
	sess->server->turn_respond_refresh = PJ_TRUE;

    /* Create resolver */
    status = pj_dns_resolver_create(mem, NULL, 0, stun_cfg->timer_heap,
				    stun_cfg->ioqueue, &sess->resolver);
    if (status != PJ_SUCCESS) {
	app_perror(INDENT "error: pj_dns_resolver_create()", status);
	destroy_sess(sess, 500);
	return -20;
    }

    ns_ip = pj_str("127.0.0.1");
    ns_port = (pj_uint16_t)DNS_SERVER_PORT;
    status = pj_dns_resolver_set_ns(sess->resolver, 1, &ns_ip, &ns_port);
    if (status != PJ_SUCCESS) {
	app_perror( INDENT "error: pj_dns_resolver_set_ns()", status);
	destroy_sess(sess, 500);
	return -21;
    }

    /* Create caller ICE stream transport */
    status = create_ice_strans(sess, &sess->caller, &sess->caller.ice);
    if (status != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -30;
    }

    /* Create callee ICE stream transport */
    status = create_ice_strans(sess, &sess->callee, &sess->callee.ice);
    if (status != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -40;
    }

    *p_sess = sess;
    return 0;
}

/* Destroy test session */
static void destroy_sess(struct test_sess *sess, unsigned wait_msec)
{
    if (sess->caller.ice) {
	pj_ice_strans_destroy(sess->caller.ice);
	sess->caller.ice = NULL;
    }

    if (sess->callee.ice) {
	pj_ice_strans_destroy(sess->callee.ice);
	sess->callee.ice = NULL;
    }

    poll_events(sess->stun_cfg, wait_msec, PJ_FALSE);

    if (sess->resolver) {
	pj_dns_resolver_destroy(sess->resolver, PJ_FALSE);
	sess->resolver = NULL;
    }

    if (sess->server) {
	destroy_test_server(sess->server);
	sess->server = NULL;
    }

    if (sess->pool) {
	pj_pool_t *pool = sess->pool;
	sess->pool = NULL;
	pj_pool_release(pool);
    }
}

static void ice_on_rx_data(pj_ice_strans *ice_st,
			   unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len)
{
    struct ice_ept *ept;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    ept = (struct ice_ept*) pj_ice_strans_get_user_data(ice_st);
    ept->result.rx_cnt[comp_id]++;
}


static void ice_on_ice_complete(pj_ice_strans *ice_st, 
			        pj_ice_strans_op op,
					pj_status_t status,
					pj_sockaddr *turn_mapped_addr)
{
    struct ice_ept *ept;

    ept = (struct ice_ept*) pj_ice_strans_get_user_data(ice_st);
    switch (op) {
    case PJ_ICE_STRANS_OP_INIT:
	ept->result.init_status = status;
	if (status != PJ_SUCCESS && (ept->cfg.client_flag & DEL_ON_ERR)) {
	    pj_ice_strans_destroy(ice_st);
	    ept->ice = NULL;
	}
	break;
    case PJ_ICE_STRANS_OP_NEGOTIATION:
	ept->result.nego_status = status;
	break;
    default:
	pj_assert(!"Unknown op");
    }
}


/* Start ICE negotiation on the endpoint, based on parameter from
 * the other endpoint.
 */
static pj_status_t start_ice(struct ice_ept *ept, const struct ice_ept *remote)
{
    pj_ice_sess_cand rcand[32];
    unsigned i, rcand_cnt = 0;
    pj_status_t status;

    /* Enum remote candidates */
    for (i=0; i<remote->cfg.comp_cnt; ++i) {
	unsigned cnt = PJ_ARRAY_SIZE(rcand) - rcand_cnt;
	status = pj_ice_strans_enum_cands(remote->ice, i+1, &cnt, rcand+rcand_cnt);
	if (status != PJ_SUCCESS) {
	    app_perror(INDENT "err: pj_ice_strans_enum_cands()", status);
	    return status;
	}
	rcand_cnt += cnt;
    }

    status = pj_ice_strans_start_ice(ept->ice, &remote->ufrag, &remote->pass,
				     rcand_cnt, rcand);
    if (status != PJ_SUCCESS) {
	app_perror(INDENT "err: pj_ice_strans_start_ice()", status);
	return status;
    }

    return PJ_SUCCESS;
}


/* Check that the pair in both agents are matched */
static int check_pair(const struct ice_ept *ept1, const struct ice_ept *ept2,
		      int start_err)
{
    unsigned i, min_cnt, max_cnt;

    if (ept1->cfg.comp_cnt < ept2->cfg.comp_cnt) {
	min_cnt = ept1->cfg.comp_cnt;
	max_cnt = ept2->cfg.comp_cnt;
    } else {
	min_cnt = ept2->cfg.comp_cnt;
	max_cnt = ept1->cfg.comp_cnt;
    }

    /* Must have valid pair for common components */
    for (i=0; i<min_cnt; ++i) {
	const pj_ice_sess_check *c1;
	const pj_ice_sess_check *c2;

	c1 = pj_ice_strans_get_valid_pair(ept1->ice, i+1);
	if (c1 == NULL) {
	    PJ_LOG(3,("", INDENT "err: unable to get valid pair for ice1 "
			  "component %d", i+1));
	    return start_err - 2;
	}

	c2 = pj_ice_strans_get_valid_pair(ept2->ice, i+1);
	if (c2 == NULL) {
	    PJ_LOG(3,("", INDENT "err: unable to get valid pair for ice2 "
			  "component %d", i+1));
	    return start_err - 4;
	}

	if (pj_sockaddr_cmp(&c1->rcand->addr, &c2->lcand->addr) != 0) {
	    PJ_LOG(3,("", INDENT "err: candidate pair does not match "
			  "for component %d", i+1));
	    return start_err - 6;
	}
    }

    /* Extra components must not have valid pair */
    for (; i<max_cnt; ++i) {
	if (ept1->cfg.comp_cnt>i &&
	    pj_ice_strans_get_valid_pair(ept1->ice, i+1) != NULL) 
	{
	    PJ_LOG(3,("", INDENT "err: ice1 shouldn't have valid pair "
		          "for component %d", i+1));
	    return start_err - 8;
	}
	if (ept2->cfg.comp_cnt>i &&
	    pj_ice_strans_get_valid_pair(ept2->ice, i+1) != NULL) 
	{
	    PJ_LOG(3,("", INDENT "err: ice2 shouldn't have valid pair "
		          "for component %d", i+1));
	    return start_err - 9;
	}
    }

    return 0;
}


#define WAIT_UNTIL(timeout,expr, RC)  { \
				pj_time_val t0, t; \
				pj_gettimeofday(&t0); \
				RC = -1; \
				for (;;) { \
				    poll_events(stun_cfg, 10, PJ_FALSE); \
				    pj_gettimeofday(&t); \
				    if (expr) { \
					rc = PJ_SUCCESS; \
					break; \
				    } \
				    if (t.sec - t0.sec > (timeout)) break; \
				} \
			    }


static int perform_test(const char *title,
			pj_stun_config *stun_cfg,
			unsigned server_flag,
		        struct test_cfg *caller_cfg,
		        struct test_cfg *callee_cfg)
{
    pjlib_state pjlib_state;
    struct test_sess *sess;
    int rc;

    PJ_LOG(3,("", INDENT "%s", title));

    capture_pjlib_state(stun_cfg, &pjlib_state);

    rc = create_sess(stun_cfg, server_flag, caller_cfg, callee_cfg, &sess);
    if (rc != 0)
	return rc;

#define ALL_READY   (sess->caller.result.init_status!=PJ_EPENDING && \
		     sess->callee.result.init_status!=PJ_EPENDING)

    /* Wait until both ICE transports are initialized */
    WAIT_UNTIL(30, ALL_READY, rc);

    if (!ALL_READY) {
	PJ_LOG(3,("", INDENT "err: init timed-out"));
	destroy_sess(sess, 500);
	return -100;
    }

    if (sess->caller.result.init_status != sess->caller.cfg.expected.init_status) {
	app_perror(INDENT "err: caller init", sess->caller.result.init_status);
	destroy_sess(sess, 500);
	return -102;
    }
    if (sess->callee.result.init_status != sess->callee.cfg.expected.init_status) {
	app_perror(INDENT "err: callee init", sess->callee.result.init_status);
	destroy_sess(sess, 500);
	return -104;
    }

    /* Failure condition */
    if (sess->caller.result.init_status != PJ_SUCCESS ||
	sess->callee.result.init_status != PJ_SUCCESS)
    {
	rc = 0;
	goto on_return;
    }

    /* Init ICE on caller */
    rc = pj_ice_strans_init_ice(sess->caller.ice, sess->caller.cfg.role, 
				&sess->caller.ufrag, &sess->caller.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: caller pj_ice_strans_init_ice()", rc);
	destroy_sess(sess, 500);
	return -100;
    }

    /* Init ICE on callee */
    rc = pj_ice_strans_init_ice(sess->callee.ice, sess->callee.cfg.role, 
				&sess->callee.ufrag, &sess->callee.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: callee pj_ice_strans_init_ice()", rc);
	destroy_sess(sess, 500);
	return -110;
    }

    /* Start ICE on callee */
    rc = start_ice(&sess->callee, &sess->caller);
    if (rc != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -120;
    }

    /* Wait for callee's answer_delay */
    poll_events(stun_cfg, sess->callee.cfg.answer_delay, PJ_FALSE);

    /* Start ICE on caller */
    rc = start_ice(&sess->caller, &sess->callee);
    if (rc != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -130;
    }

    /* Wait until negotiation is complete on both endpoints */
#define ALL_DONE    (sess->caller.result.nego_status!=PJ_EPENDING && \
		     sess->callee.result.nego_status!=PJ_EPENDING)
    WAIT_UNTIL(30, ALL_DONE, rc);

    if (!ALL_DONE) {
	PJ_LOG(3,("", INDENT "err: negotiation timed-out"));
	destroy_sess(sess, 500);
	return -140;
    }

    if (sess->caller.result.nego_status != sess->caller.cfg.expected.nego_status) {
	app_perror(INDENT "err: caller negotiation failed", sess->caller.result.nego_status);
	destroy_sess(sess, 500);
	return -150;
    }

    if (sess->callee.result.nego_status != sess->callee.cfg.expected.nego_status) {
	app_perror(INDENT "err: callee negotiation failed", sess->callee.result.nego_status);
	destroy_sess(sess, 500);
	return -160;
    }

    /* Verify that both agents have agreed on the same pair */
    rc = check_pair(&sess->caller, &sess->callee, -170);
    if (rc != 0) {
	destroy_sess(sess, 500);
	return rc;
    }
    rc = check_pair(&sess->callee, &sess->caller, -180);
    if (rc != 0) {
	destroy_sess(sess, 500);
	return rc;
    }

    /* Looks like everything is okay */

    /* Destroy ICE stream transports first to let it de-allocate
     * TURN relay (otherwise there'll be timer/memory leak, unless
     * we wait for long time in the last poll_events() below).
     */
    if (sess->caller.ice) {
	pj_ice_strans_destroy(sess->caller.ice);
	sess->caller.ice = NULL;
    }

    if (sess->callee.ice) {
	pj_ice_strans_destroy(sess->callee.ice);
	sess->callee.ice = NULL;
    }

on_return:
    /* Wait.. */
    poll_events(stun_cfg, 500, PJ_FALSE);

    /* Now destroy everything */
    destroy_sess(sess, 500);

    /* Flush events */
    poll_events(stun_cfg, 100, PJ_FALSE);

    rc = check_pjlib_state(stun_cfg, &pjlib_state);
    if (rc != 0) {
	return rc;
    }

    return 0;
}

#define ROLE1	PJ_ICE_SESS_ROLE_CONTROLLED
#define ROLE2	PJ_ICE_SESS_ROLE_CONTROLLING

int ice_test(void)
{
    pj_pool_t *pool;
    pj_stun_config stun_cfg;
    unsigned i;
    int rc;
    struct sess_cfg_t {
	const char	*title;
	unsigned	 server_flag;
	struct test_cfg	 ua1;
	struct test_cfg	 ua2;
    } sess_cfg[] = 
    {
	/*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	{
	    "hosts candidates only",
	    0xFFFF,
	    {ROLE1, 1,	    YES,    NO,	    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    YES,    NO,	    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "host and srflxes",
	    0xFFFF,
	    {ROLE1, 1,	    YES,    YES,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    YES,    YES,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "host vs relay",
	    0xFFFF,
	    {ROLE1, 1,	    YES,    NO,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    NO,     NO,    YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "relay vs host",
	    0xFFFF,
	    {ROLE1, 1,	    NO,	    NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	   YES,     NO,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "relay vs relay",
	    0xFFFF,
	    {ROLE1, 1,	    NO,	    NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    NO,     NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "all candidates",
	    0xFFFF,
	    {ROLE1, 1,	   YES,	   YES,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	   YES,    YES,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	},
    };

    pool = pj_pool_create(mem, NULL, 512, 512, NULL);
    rc = create_stun_config(pool, &stun_cfg);
    if (rc != PJ_SUCCESS) {
	pj_pool_release(pool);
	return -7;
    }

    /* Simple test first with host candidate */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "Basic with host candidates",
	    0x0,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	YES,     NO,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	YES,     NO,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;
	rc = perform_test("Basic with host candidates, 2 components", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Simple test first with srflx candidate */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "Basic with srflx candidates",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	YES,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	YES,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;

	rc = perform_test("Basic with srflx candidates, 2 components", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Simple test with relay candidate */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "Basic with relay candidates",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	 NO,     NO,	  YES,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	 NO,     NO,	  YES,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;

	rc = perform_test("Basic with relay candidates, 2 components", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Failure test with STUN resolution */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "STUN resolution failure",
	    0x0,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	2,	 NO,    YES,	    NO,	    0,	    0,	    0,	    0, {PJNATH_ESTUNTIMEDOUT, -1}},
	    {ROLE2,	2,	 NO,    YES,	    NO,	    0,	    0,	    0,	    0, {PJNATH_ESTUNTIMEDOUT, -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("STUN resolution failure with destroy on callback", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Failure test with TURN resolution */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "TURN allocation failure",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	2,	 NO,    NO,	YES, WRONG_TURN,    0,	    0,	    0, {PJ_STATUS_FROM_STUN_CODE(401), -1}},
	    {ROLE2,	2,	 NO,    NO,	YES, WRONG_TURN,    0,	    0,	    0, {PJ_STATUS_FROM_STUN_CODE(401), -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("TURN allocation failure with destroy on callback", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* STUN failure, testing TURN deallocation */
    if (1) {
	struct sess_cfg_t cfg = 
	{
	    "STUN failure, testing TURN deallocation",
	    0xFFFF & (~(CREATE_STUN_SERVER)),
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	2,	 YES,    YES,	YES,	0,    0,	    0,	    0, {PJNATH_ESTUNTIMEDOUT, -1}},
	    {ROLE2,	2,	 YES,    YES,	YES,	0,    0,	    0,	    0, {PJNATH_ESTUNTIMEDOUT, -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("STUN failure, testing TURN deallocation (cb)", 
			  &stun_cfg, cfg.server_flag, 
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    rc = 0;
    /* Iterate each test item */
    for (i=0; i<PJ_ARRAY_SIZE(sess_cfg); ++i) {
	struct sess_cfg_t *cfg = &sess_cfg[i];
	unsigned delay[] = { 50, 2000 };
	unsigned d;

	PJ_LOG(3,("", "  %s", cfg->title));

	/* For each test item, test with various answer delay */
	for (d=0; d<PJ_ARRAY_SIZE(delay); ++d) {
	    struct role_t {
		pj_ice_sess_role	ua1;
		pj_ice_sess_role	ua2;
	    } role[] = 
	    {
		{ ROLE1, ROLE2},
		{ ROLE2, ROLE1},
		{ ROLE1, ROLE1},
		{ ROLE2, ROLE2}
	    };
	    unsigned j;

	    cfg->ua1.answer_delay = delay[d];
	    cfg->ua2.answer_delay = delay[d];

	    /* For each test item, test with role conflict scenarios */
	    for (j=0; j<PJ_ARRAY_SIZE(role); ++j) {
		unsigned k1;

		cfg->ua1.role = role[j].ua1;
		cfg->ua2.role = role[j].ua2;

		/* For each test item, test with different number of components */
		for (k1=1; k1<=2; ++k1) {
		    unsigned k2;

		    cfg->ua1.comp_cnt = k1;

		    for (k2=1; k2<=2; ++k2) {
			char title[120];

			sprintf(title, 
				"%s/%s, %dms answer delay, %d vs %d components", 
				pj_ice_sess_role_name(role[j].ua1),
				pj_ice_sess_role_name(role[j].ua2),
				delay[d], k1, k2);

			cfg->ua2.comp_cnt = k2;
			rc = perform_test(title, &stun_cfg, cfg->server_flag, 
					  &cfg->ua1, &cfg->ua2);
			if (rc != 0)
			    goto on_return;
		    }
		}
	    }
	}
    }

on_return:
    destroy_stun_config(&stun_cfg);
    pj_pool_release(pool);
    return rc;
}

