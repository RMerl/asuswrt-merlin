/* $Id: nat_detect.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjnath/nat_detect.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/timer.h>
#include <pj/compat/socket.h>


static const char *nat_type_names[] =
{
    "Unknown",
    "ErrUnknown",
    "Open",
    "Blocked",
    "Symmetric UDP",
    "Full Cone",
    "Symmetric",
    "Restricted",
    "Port Restricted"
};


#define CHANGE_IP_FLAG		4
#define CHANGE_PORT_FLAG	2
#define CHANGE_IP_PORT_FLAG	(CHANGE_IP_FLAG | CHANGE_PORT_FLAG)
#define TEST_INTERVAL		50

enum test_type
{
    ST_TEST_1,
    ST_TEST_2,
    ST_TEST_3,
    ST_TEST_1B,
    ST_MAX
};

static const char *test_names[] =
{
    "Test I: Binding request",
    "Test II: Binding request with change address and port request",
    "Test III: Binding request with change port request",
    "Test IB: Binding request to alternate address"
};

enum timer_type
{
    TIMER_TEST	    = 1,
    TIMER_DESTROY   = 2
};

typedef struct nat_detect_session
{
	int inst_id;

    pj_pool_t		    *pool;
    pj_grp_lock_t	    *grp_lock;

    pj_timer_heap_t	    *timer_heap;
    pj_timer_entry	     timer;
    unsigned		     timer_executed;

    void		    *user_data;
    pj_stun_nat_detect_cb   *cb;
    pj_sock_t		     sock;
    pj_sockaddr_in	     local_addr;
    pj_ioqueue_key_t	    *key;
    pj_sockaddr_in	     server;
    pj_sockaddr_in	    *cur_server;
    pj_stun_session	    *stun_sess;

    pj_ioqueue_op_key_t	     read_op, write_op;
    pj_uint8_t		     rx_pkt[PJ_STUN_MAX_PKT_LEN];
    pj_ssize_t		     rx_pkt_len;
    pj_sockaddr_in	     src_addr;
    int			     src_addr_len;

    struct result
    {
	pj_bool_t	executed;
	pj_bool_t	complete;
	pj_status_t	status;
	pj_sockaddr_in	ma;
	pj_sockaddr_in	ca;
	pj_stun_tx_data	*tdata;
    } result[ST_MAX];

} nat_detect_session;


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);
static void on_request_complete(pj_stun_session *sess,
			        pj_status_t status,
				void *token,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len);
static pj_status_t on_send_msg(pj_stun_session *sess,
			       void *token,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

static pj_status_t send_test(nat_detect_session *sess,
			     enum test_type test_id,
			     const pj_sockaddr_in *alt_addr,
			     pj_uint32_t change_flag);
static void on_sess_timer(pj_timer_heap_t *th,
			     pj_timer_entry *te);
static void sess_destroy(nat_detect_session *sess);
static void sess_on_destroy(void *member);

/*
 * Get the NAT name from the specified NAT type.
 */
PJ_DEF(const char*) pj_stun_get_nat_name(pj_stun_nat_type type)
{
    PJ_ASSERT_RETURN(type >= 0 && type <= PJ_STUN_NAT_TYPE_PORT_RESTRICTED,
		     "*Invalid*");

    return nat_type_names[type];
}

static int test_executed(nat_detect_session *sess)
{
    unsigned i, count;
    for (i=0, count=0; i<PJ_ARRAY_SIZE(sess->result); ++i) {
	if (sess->result[i].executed)
	    ++count;
    }
    return count;
}

static int test_completed(nat_detect_session *sess)
{
    unsigned i, count;
    for (i=0, count=0; i<PJ_ARRAY_SIZE(sess->result); ++i) {
	if (sess->result[i].complete)
	    ++count;
    }
    return count;
}

PJ_DEF(pj_status_t) get_local_interface(const pj_sockaddr_in *server,
				       pj_in_addr *local_addr)
{
    pj_sock_t sock;
    pj_sockaddr_in tmp;
    int addr_len;
    pj_status_t status;

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sock_bind_in(sock, 0, 0);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    status = pj_sock_connect(sock, server, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(sock, &tmp, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    local_addr->s_addr = tmp.sin_addr.s_addr;
    
    pj_sock_close(sock);
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_detect_nat_type(int inst_id,
						const pj_sockaddr_in *server,
					    pj_stun_config *stun_cfg,
					    void *user_data,
					    pj_stun_nat_detect_cb *cb)
{
    pj_pool_t *pool;
    nat_detect_session *sess;
    pj_stun_session_cb sess_cb;
    pj_ioqueue_callback ioqueue_cb;
    int addr_len;
    pj_status_t status;

    PJ_ASSERT_RETURN(server && stun_cfg, PJ_EINVAL);
    PJ_ASSERT_RETURN(stun_cfg->pf && stun_cfg->ioqueue && stun_cfg->timer_heap,
		     PJ_EINVAL);

    /*
     * Init NAT detection session.
     */
    pool = pj_pool_create(stun_cfg->pf, "natck%p", PJNATH_POOL_LEN_NATCK, 
			  PJNATH_POOL_INC_NATCK, NULL);
    if (!pool)
	return PJ_ENOMEM;

    sess = PJ_POOL_ZALLOC_T(pool, nat_detect_session);
    sess->pool = pool;
    sess->user_data = user_data;
    sess->cb = cb;
	sess->inst_id = inst_id;

    status = pj_grp_lock_create(pool, NULL, &sess->grp_lock);
    if (status != PJ_SUCCESS) {
	/* Group lock not created yet, just destroy pool and return */
	pj_pool_release(pool);
	return status;
    }

    pj_grp_lock_add_ref(sess->grp_lock);
    pj_grp_lock_add_handler(sess->grp_lock, pool, sess, &sess_on_destroy);

    pj_memcpy(&sess->server, server, sizeof(pj_sockaddr_in));

    /*
     * Init timer to self-destroy.
     */
    sess->timer_heap = stun_cfg->timer_heap;
    sess->timer.cb = &on_sess_timer;
    sess->timer.user_data = sess;


    /*
     * Initialize socket.
     */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sess->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Bind to any.
     */
    pj_bzero(&sess->local_addr, sizeof(pj_sockaddr_in));
    sess->local_addr.sin_family = pj_AF_INET();
    status = pj_sock_bind(sess->sock, &sess->local_addr, 
			  sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Get local/bound address.
     */
    addr_len = sizeof(sess->local_addr);
    status = pj_sock_getsockname(sess->sock, &sess->local_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Find out which interface is used to send to the server.
     */
    status = get_local_interface(server, &sess->local_addr.sin_addr);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(4,(sess->pool->obj_name, "Local address is %s:%d",
	      pj_inet_ntoa(sess->local_addr.sin_addr), 
	      pj_ntohs(sess->local_addr.sin_port)));

    PJ_LOG(4,(sess->pool->obj_name, "Server set to %s:%d",
	      pj_inet_ntoa(server->sin_addr), 
	      pj_ntohs(server->sin_port)));

    /*
     * Register socket to ioqueue to receive asynchronous input
     * notification.
     */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;

    status = pj_ioqueue_register_sock2(sess->pool, stun_cfg->ioqueue, 
				       sess->sock, sess->grp_lock, sess,
				       &ioqueue_cb, &sess->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Create STUN session.
     */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &on_request_complete;
    sess_cb.on_send_msg = &on_send_msg;
    status = pj_stun_session_create(stun_cfg, pool->obj_name, &sess_cb,
				    PJ_FALSE, sess->grp_lock, &sess->stun_sess);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_stun_session_set_user_data(sess->stun_sess, sess);

    /*
     * Kick-off ioqueue reading.
     */
    pj_ioqueue_op_key_init(&sess->read_op, sizeof(sess->read_op));
    pj_ioqueue_op_key_init(&sess->write_op, sizeof(sess->write_op));
    on_read_complete(sess->key, &sess->read_op, 0);

    /*
     * Start TEST_1
     */
    sess->timer.id = TIMER_TEST;
    on_sess_timer(stun_cfg->timer_heap, &sess->timer);

    return PJ_SUCCESS;

on_error:
    sess_destroy(sess);
    return status;
}


static void sess_destroy(nat_detect_session *sess)
{
    if (sess->stun_sess) { 
	pj_stun_session_destroy(sess->stun_sess);
	sess->stun_sess = NULL;
    }

    if (sess->key) {
	pj_ioqueue_unregister(sess->key);
	sess->key = NULL;
	sess->sock = PJ_INVALID_SOCKET;
    } else if (sess->sock && sess->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(sess->sock);
	sess->sock = PJ_INVALID_SOCKET;
    }

    if (sess->grp_lock) {
	pj_grp_lock_dec_ref(sess->grp_lock);
    }
}

static void sess_on_destroy(void *member)
{
    nat_detect_session *sess = (nat_detect_session*)member;
    if (sess->pool) {
	pj_pool_release(sess->pool);
    }
}

static void end_session(nat_detect_session *sess,
			pj_status_t status,
			pj_stun_nat_type nat_type)
{
    pj_stun_nat_detect_result result;
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_time_val delay;

    if (sess->timer.id != 0) {
	pj_timer_heap_cancel(sess->timer_heap, &sess->timer);
	sess->timer.id = 0;
    }

    pj_bzero(&result, sizeof(result));
    errmsg[0] = '\0';
    result.status_text = errmsg;

    result.status = status;
    pj_strerror(status, errmsg, sizeof(errmsg));
    result.nat_type = nat_type;
    result.nat_type_name = nat_type_names[result.nat_type];

    /* 
      DEAN modified
      We can pass the mapped-address as user_data parameter, 
      because the sess->user_data was assigned as NULL when 
      pj_stun_detect_nat_type was called.
     
      For checking whether the network is the type of Internet<->RT<->RT<->Device,
      the mapped-address can be compared with the external address
      returned by upper router.
    */ 
    #if 0
    if (sess->cb)
	(*sess->cb)(sess->user_data, &result);
    #else
    if (sess->cb)
	(*sess->cb)(sess->inst_id, &sess->local_addr, &sess->result[ST_TEST_1].ma, &result);
    #endif

    delay.sec = 0;
    delay.msec = 0;

    sess->timer.id = TIMER_DESTROY;
    pj_timer_heap_schedule(sess->timer_heap, &sess->timer, &delay);
}


/*
 * Callback upon receiving packet from network.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    nat_detect_session *sess;
    pj_status_t status;

    sess = (nat_detect_session *) pj_ioqueue_get_user_data(key);
    pj_assert(sess != NULL);

    pj_grp_lock_acquire(sess->grp_lock);

    /* Ignore packet when STUN session has been destroyed */
    if (!sess->stun_sess)
	goto on_return;

    if (bytes_read < 0) {
	if (-bytes_read != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) &&
	    -bytes_read != PJ_STATUS_FROM_OS(OSERR_EINPROGRESS) && 
	    -bytes_read != PJ_STATUS_FROM_OS(OSERR_ECONNRESET)) 
	{
	    /* Permanent error */
	    end_session(sess, (pj_status_t)-bytes_read, 
			PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
	    goto on_return;
	}

    } else if (bytes_read > 0) {
	pj_stun_session_on_rx_pkt(sess->stun_sess, sess->rx_pkt, bytes_read,
				  PJ_STUN_IS_DATAGRAM|PJ_STUN_CHECK_PACKET, 
				  NULL, NULL, 
				  &sess->src_addr, sess->src_addr_len);
    }


    sess->rx_pkt_len = sizeof(sess->rx_pkt);
    sess->src_addr_len = sizeof(sess->src_addr);
    status = pj_ioqueue_recvfrom(key, op_key, sess->rx_pkt, &sess->rx_pkt_len,
				 PJ_IOQUEUE_ALWAYS_ASYNC, 
				 &sess->src_addr, &sess->src_addr_len);

    if (status != PJ_EPENDING) {
	pj_assert(status != PJ_SUCCESS);
	end_session(sess, status, PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
    }

on_return:
    pj_grp_lock_release(sess->grp_lock);
}


/*
 * Callback to send outgoing packet from STUN session.
 */
static pj_status_t on_send_msg(pj_stun_session *stun_sess,
			       void *token,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len)
{
    nat_detect_session *sess;
    pj_ssize_t pkt_len;
    pj_status_t status;

    PJ_UNUSED_ARG(token);

    sess = (nat_detect_session*) pj_stun_session_get_user_data(stun_sess);

    pkt_len = pkt_size;
    status = pj_ioqueue_sendto(sess->key, &sess->write_op, pkt, &pkt_len, 0,
			       dst_addr, addr_len);

    return status;

}

/*
 * Callback upon request completion.
 */
static void on_request_complete(pj_stun_session *stun_sess,
			        pj_status_t status,
				void *token,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len)
{
    nat_detect_session *sess;
    pj_stun_sockaddr_attr *mattr = NULL;
    pj_stun_changed_addr_attr *ca = NULL;
    pj_uint32_t *tsx_id;
    int cmp;
    unsigned test_id;

    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(tdata);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    sess = (nat_detect_session*) pj_stun_session_get_user_data(stun_sess);

    pj_grp_lock_acquire(sess->grp_lock);

    /* Find errors in the response */
    if (status == PJ_SUCCESS) {

	/* Check error message */
	if (PJ_STUN_IS_ERROR_RESPONSE(response->hdr.type)) {
	    pj_stun_errcode_attr *eattr;
	    int err_code;

	    eattr = (pj_stun_errcode_attr*)
		    pj_stun_msg_find_attr(response, PJ_STUN_ATTR_ERROR_CODE, 0);

	    if (eattr != NULL)
		err_code = eattr->err_code;
	    else
		err_code = PJ_STUN_SC_SERVER_ERROR;

	    status = PJ_STATUS_FROM_STUN_CODE(err_code);


	} else {

	    /* Get MAPPED-ADDRESS or XOR-MAPPED-ADDRESS */
	    mattr = (pj_stun_sockaddr_attr*)
		    pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR, 0);
	    if (mattr == NULL) {
		mattr = (pj_stun_sockaddr_attr*)
			pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MAPPED_ADDR, 0);
	    }

	    if (mattr == NULL) {
		status = PJNATH_ESTUNNOMAPPEDADDR;
	    }

	    /* Get CHANGED-ADDRESS attribute */
	    ca = (pj_stun_changed_addr_attr*)
		 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_CHANGED_ADDR, 0);

	    if (ca == NULL) {
		status = PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_SERVER_ERROR);
	    }

	}
    }

    /* Save the result */
    tsx_id = (pj_uint32_t*) tdata->msg->hdr.tsx_id;
    test_id = tsx_id[2];

    if (test_id >= ST_MAX) {
	PJ_LOG(4,(sess->pool->obj_name, "Invalid transaction ID %u in response",
		  test_id));
	end_session(sess, PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_SERVER_ERROR),
		    PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
	goto on_return;
    }

    PJ_LOG(5,(sess->pool->obj_name, "Completed %s, status=%d",
	      test_names[test_id], status));

    sess->result[test_id].complete = PJ_TRUE;
    sess->result[test_id].status = status;
    if (status == PJ_SUCCESS) {
	pj_memcpy(&sess->result[test_id].ma, &mattr->sockaddr.ipv4,
		  sizeof(pj_sockaddr_in));
	pj_memcpy(&sess->result[test_id].ca, &ca->sockaddr.ipv4,
		  sizeof(pj_sockaddr_in));
    }

	// save mapped address
	if (sess->result[ST_TEST_1].complete && 
		sess->result[ST_TEST_1].status == PJ_SUCCESS)
	{
		pj_stun_nat_detect_result re = {0};
		re.status = -1;
		(*sess->cb)(sess->inst_id, &sess->local_addr, &sess->result[ST_TEST_1].ma, &re);
	}

    /* Send Test 1B only when Test 2 completes. Must not send Test 1B
     * before Test 2 completes to avoid creating mapping on the NAT.
     */
    if (!sess->result[ST_TEST_1B].executed && 
	sess->result[ST_TEST_2].complete &&
	sess->result[ST_TEST_2].status != PJ_SUCCESS &&
	sess->result[ST_TEST_1].complete &&
	sess->result[ST_TEST_1].status == PJ_SUCCESS) 
    {
	cmp = pj_memcmp(&sess->local_addr, &sess->result[ST_TEST_1].ma,
			sizeof(pj_sockaddr_in));
	if (cmp != 0)
	    send_test(sess, ST_TEST_1B, &sess->result[ST_TEST_1].ca, 0);
    }

    if (test_completed(sess)<3 || test_completed(sess)!=test_executed(sess))
	goto on_return;

    /* Handle the test result according to RFC 3489 page 22:


                        +--------+
                        |  Test  |
                        |   1    |
                        +--------+
                             |
                             |
                             V
                            /\              /\
                         N /  \ Y          /  \ Y             +--------+
          UDP     <-------/Resp\--------->/ IP \------------->|  Test  |
          Blocked         \ ?  /          \Same/              |   2    |
                           \  /            \? /               +--------+
                            \/              \/                    |
                                             | N                  |
                                             |                    V
                                             V                    /\
                                         +--------+  Sym.      N /  \
                                         |  Test  |  UDP    <---/Resp\
                                         |   2    |  Firewall   \ ?  /
                                         +--------+              \  /
                                             |                    \/
                                             V                     |Y
                  /\                         /\                    |
   Symmetric  N  /  \       +--------+   N  /  \                   V
      NAT  <--- / IP \<-----|  Test  |<--- /Resp\               Open
                \Same/      |   1B   |     \ ?  /               Internet
                 \? /       +--------+      \  /
                  \/                         \/
                  |                           |Y
                  |                           |
                  |                           V
                  |                           Full
                  |                           Cone
                  V              /\
              +--------+        /  \ Y
              |  Test  |------>/Resp\---->Restricted
              |   3    |       \ ?  /
              +--------+        \  /
                                 \/
                                  |N
                                  |       Port
                                  +------>Restricted

                 Figure 2: Flow for type discovery process
     */

    switch (sess->result[ST_TEST_1].status) {
    case PJNATH_ESTUNTIMEDOUT:
	/*
	 * Test 1 has timed-out. Conclude with NAT_TYPE_BLOCKED. 
	 */
	end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_BLOCKED);
	break;
    case PJ_SUCCESS:
	/*
	 * Test 1 is successful. Further tests are needed to detect
	 * NAT type. Compare the MAPPED-ADDRESS with the local address.
	 */
	cmp = pj_memcmp(&sess->local_addr, &sess->result[ST_TEST_1].ma,
			sizeof(pj_sockaddr_in));
	if (cmp==0) {
	    /*
	     * MAPPED-ADDRESS and local address is equal. Need one more
	     * test to determine NAT type.
	     */
	    switch (sess->result[ST_TEST_2].status) {
	    case PJ_SUCCESS:
		/*
		 * Test 2 is also successful. We're in the open.
		 */
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_OPEN);
		break;
	    case PJNATH_ESTUNTIMEDOUT:
		/*
		 * Test 2 has timed out. We're behind somekind of UDP
		 * firewall.
		 */
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_SYMMETRIC_UDP);
		break;
	    default:
		/*
		 * We've got other error with Test 2.
		 */
		end_session(sess, sess->result[ST_TEST_2].status, 
			    PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
		break;
	    }
	} else {
	    /*
	     * MAPPED-ADDRESS is different than local address.
	     * We're behind NAT.
	     */
	    switch (sess->result[ST_TEST_2].status) {
	    case PJ_SUCCESS:
		/*
		 * Test 2 is successful. We're behind a full-cone NAT.
		 */
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_FULL_CONE);
		break;
	    case PJNATH_ESTUNTIMEDOUT:
		/*
		 * Test 2 has timed-out Check result of test 1B..
		 */
		switch (sess->result[ST_TEST_1B].status) {
		case PJ_SUCCESS:
		    /*
		     * Compare the MAPPED-ADDRESS of test 1B with the
		     * MAPPED-ADDRESS returned in test 1..
		     */
		    cmp = pj_memcmp(&sess->result[ST_TEST_1].ma,
				    &sess->result[ST_TEST_1B].ma,
				    sizeof(pj_sockaddr_in));
		    if (cmp != 0) {
			/*
			 * MAPPED-ADDRESS is different, we're behind a
			 * symmetric NAT.
			 */
			end_session(sess, PJ_SUCCESS,
				    PJ_STUN_NAT_TYPE_SYMMETRIC);
		    } else {
			/*
			 * MAPPED-ADDRESS is equal. We're behind a restricted
			 * or port-restricted NAT, depending on the result of
			 * test 3.
			 */
			switch (sess->result[ST_TEST_3].status) {
			case PJ_SUCCESS:
			    /*
			     * Test 3 is successful, we're behind a restricted
			     * NAT.
			     */
			    end_session(sess, PJ_SUCCESS,
					PJ_STUN_NAT_TYPE_RESTRICTED);
			    break;
			case PJNATH_ESTUNTIMEDOUT:
			    /*
			     * Test 3 failed, we're behind a port restricted
			     * NAT.
			     */
			    end_session(sess, PJ_SUCCESS,
					PJ_STUN_NAT_TYPE_PORT_RESTRICTED);
			    break;
			default:
			    /*
			     * Got other error with test 3.
			     */
			    end_session(sess, sess->result[ST_TEST_3].status,
					PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
			    break;
			}
		    }
		    break;
		case PJNATH_ESTUNTIMEDOUT:
		    /*
		     * Strangely test 1B has failed. Maybe connectivity was
		     * lost? Or perhaps port 3489 (the usual port number in
		     * CHANGED-ADDRESS) is blocked?
		     */
		    switch (sess->result[ST_TEST_3].status) {
		    case PJ_SUCCESS:
			/* Although test 1B failed, test 3 was successful.
			 * It could be that port 3489 is blocked, while the
			 * NAT itself looks to be a Restricted one.
			 */
			end_session(sess, PJ_SUCCESS, 
				    PJ_STUN_NAT_TYPE_RESTRICTED);
			break;
		    default:
			/* Can't distinguish between Symmetric and Port
			 * Restricted, so set the type to Unknown
			 */
			end_session(sess, PJ_SUCCESS, 
				    PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
			break;
		    }
		    break;
		default:
		    /*
		     * Got other error with test 1B.
		     */
		    end_session(sess, sess->result[ST_TEST_1B].status,
				PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
		    break;
		}
		break;
	    default:
		/*
		 * We've got other error with Test 2.
		 */
		end_session(sess, sess->result[ST_TEST_2].status, 
			    PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
		break;
	    }
	}
	break;
    default:
	/*
	 * We've got other error with Test 1.
	 */
	end_session(sess, sess->result[ST_TEST_1].status, 
		    PJ_STUN_NAT_TYPE_ERR_UNKNOWN);
	break;
    }

on_return:
    pj_grp_lock_release(sess->grp_lock);
}


/* Perform test */
static pj_status_t send_test(nat_detect_session *sess,
			     enum test_type test_id,
			     const pj_sockaddr_in *alt_addr,
			     pj_uint32_t change_flag)
{
    pj_uint32_t magic, tsx_id[3];
    pj_status_t status;

    sess->result[test_id].executed = PJ_TRUE;

    /* Randomize tsx id */
    do {
	magic = pj_rand();
    } while (magic == PJ_STUN_MAGIC);

    tsx_id[0] = pj_rand();
    tsx_id[1] = pj_rand();
    tsx_id[2] = test_id;

    /* Create BIND request */
    status = pj_stun_session_create_req(sess->stun_sess, 
					PJ_STUN_BINDING_REQUEST, magic,
					(pj_uint8_t*)tsx_id, 
					&sess->result[test_id].tdata);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Add CHANGE-REQUEST attribute */
    status = pj_stun_msg_add_uint_attr(sess->pool, 
				       sess->result[test_id].tdata->msg,
				       PJ_STUN_ATTR_CHANGE_REQUEST,
				       change_flag);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Configure alternate address */
    if (alt_addr)
	sess->cur_server = (pj_sockaddr_in*) alt_addr;
    else
	sess->cur_server = &sess->server;

    PJ_LOG(5,(sess->pool->obj_name, 
              "Performing %s to %s:%d", 
	      test_names[test_id],
	      pj_inet_ntoa(sess->cur_server->sin_addr),
	      pj_ntohs(sess->cur_server->sin_port)));

    /* Send the request */
    status = pj_stun_session_send_msg(sess->stun_sess, NULL, PJ_TRUE,
				      PJ_TRUE, sess->cur_server, 
				      sizeof(pj_sockaddr_in),
				      sess->result[test_id].tdata);
    if (status != PJ_SUCCESS)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    sess->result[test_id].complete = PJ_TRUE;
    sess->result[test_id].status = status;

    return status;
}


/* Timer callback */
static void on_sess_timer(pj_timer_heap_t *th,
			     pj_timer_entry *te)
{
    nat_detect_session *sess;

    sess = (nat_detect_session*) te->user_data;

    if (te->id == TIMER_DESTROY) {
	pj_grp_lock_acquire(sess->grp_lock);
	pj_ioqueue_unregister(sess->key);
	sess->key = NULL;
	sess->sock = PJ_INVALID_SOCKET;
	te->id = 0;
	pj_grp_lock_release(sess->grp_lock);

	sess_destroy(sess);

    } else if (te->id == TIMER_TEST) {

	pj_bool_t next_timer;

	pj_grp_lock_acquire(sess->grp_lock);

	next_timer = PJ_FALSE;

	if (sess->timer_executed == 0) {
	    send_test(sess, ST_TEST_1, NULL, 0);
	    next_timer = PJ_TRUE;
	} else if (sess->timer_executed == 1) {
	    send_test(sess, ST_TEST_2, NULL, CHANGE_IP_PORT_FLAG);
	    next_timer = PJ_TRUE;
	} else if (sess->timer_executed == 2) {
	    send_test(sess, ST_TEST_3, NULL, CHANGE_PORT_FLAG);
	} else {
	    pj_assert(!"Shouldn't have timer at this state");
	}

	++sess->timer_executed;

	if (next_timer) {
	    pj_time_val delay = {0, TEST_INTERVAL};
	    pj_timer_heap_schedule(th, te, &delay);
	} else {
	    te->id = 0;
	}

	pj_grp_lock_release(sess->grp_lock);

    } else {
	pj_assert(!"Invalid timer ID");
    }
}

