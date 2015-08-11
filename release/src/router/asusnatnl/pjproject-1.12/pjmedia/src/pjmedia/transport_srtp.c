/* $Id: transport_srtp.c 4387 2013-02-27 10:16:08Z ming $ */
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA 
 */

#include <pjmedia/transport_srtp.h>
#include <pjmedia/endpoint.h>
#include <pjlib-util/base64.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)

#include <srtp.h>

#define THIS_FILE   "transport_srtp.c"

/* Maximum size of packet */
#define MAX_RTP_BUFFER_LEN	    1500
#define MAX_RTCP_BUFFER_LEN	    1500
#define MAX_KEY_LEN		    128

/* Initial value of probation counter. When probation counter > 0, 
 * it means SRTP is in probation state, and it may restart when
 * srtp_unprotect() returns err_status_replay_*
 */
#define PROBATION_CNT_INIT	    100

#define DEACTIVATE_MEDIA(pool, m)   pjmedia_sdp_media_deactivate(pool, m)

static const pj_str_t ID_RTP_AVP  = { "RTP/AVP", 7 };
static const pj_str_t ID_RTP_SAVP = { "RTP/SAVP", 8 };
static const pj_str_t ID_INACTIVE = { "inactive", 8 };
static const pj_str_t ID_CRYPTO   = { "crypto", 6 };

typedef struct crypto_suite
{
    char		*name;
    cipher_type_id_t	 cipher_type;
    unsigned		 cipher_key_len;
    auth_type_id_t	 auth_type;
    unsigned		 auth_key_len;
    unsigned		 srtp_auth_tag_len;
    unsigned		 srtcp_auth_tag_len;
    sec_serv_t		 service;
} crypto_suite;

/* Crypto suites as defined on RFC 4568 */
static crypto_suite crypto_suites[] = {
    /* plain RTP/RTCP (no cipher & no auth) */
    {"NULL", NULL_CIPHER, 0, NULL_AUTH, 0, 0, 0, sec_serv_none},

    /* cipher AES_CM, auth HMAC_SHA1, auth tag len = 10 octets */
    {"AES_CM_128_HMAC_SHA1_80", AES_128_ICM, 30, HMAC_SHA1, 20, 10, 10, 
	sec_serv_conf_and_auth},

    /* cipher AES_CM, auth HMAC_SHA1, auth tag len = 4 octets */
    {"AES_CM_128_HMAC_SHA1_32", AES_128_ICM, 30, HMAC_SHA1, 20, 4, 10,
	sec_serv_conf_and_auth},

    /* 
     * F8_128_HMAC_SHA1_8 not supported by libsrtp?
     * {"F8_128_HMAC_SHA1_8", NULL_CIPHER, 0, NULL_AUTH, 0, 0, 0, sec_serv_none}
     */
};

typedef struct transport_srtp
{
    pjmedia_transport	 base;		    /**< Base transport interface.  */
    pj_pool_t		*pool;		    /**< Pool for transport SRTP.   */
    pj_lock_t		*mutex;		    /**< Mutex for libsrtp contexts.*/
    char		 rtp_tx_buffer[MAX_RTP_BUFFER_LEN];
    char		 rtcp_tx_buffer[MAX_RTCP_BUFFER_LEN];
    pjmedia_srtp_setting setting;
    unsigned		 media_option;

    /* SRTP policy */
    pj_bool_t		 session_inited;
    pj_bool_t		 offerer_side;
    pj_bool_t		 bypass_srtp;
    char		 tx_key[MAX_KEY_LEN];
    char		 rx_key[MAX_KEY_LEN];
    pjmedia_srtp_crypto  tx_policy;
    pjmedia_srtp_crypto  rx_policy;

    /* Temporary policy for negotiation */
    pjmedia_srtp_crypto  tx_policy_neg;
    pjmedia_srtp_crypto  rx_policy_neg;

    /* libSRTP contexts */
    srtp_t		 srtp_tx_ctx;
    srtp_t		 srtp_rx_ctx;

    /* Stream information */
    void		*user_data;
    void		(*rtp_cb)( void *user_data,
				   void *pkt,
				   pj_ssize_t size);
    void		(*rtcp_cb)(void *user_data,
				   void *pkt,
				   pj_ssize_t size);
        
    /* Transport information */
    pjmedia_transport	*member_tp; /**< Underlying transport.       */

    /* SRTP usage policy of peer. This field is updated when media is starting.
     * This is useful when SRTP is in optional mode and peer is using mandatory
     * mode, so when local is about to reinvite/update, it should offer 
     * RTP/SAVP instead of offering RTP/AVP.
     */
    pjmedia_srtp_use	 peer_use;

    /* When probation counter > 0, it means SRTP is in probation state, 
     * and it may restart when srtp_unprotect() returns err_status_replay_*
     */
    unsigned		 probation_cnt;
} transport_srtp;


/*
 * This callback is called by transport when incoming rtp is received
 */
static void srtp_rtp_cb( void *user_data, void *pkt, pj_ssize_t size);

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void srtp_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size);


/*
 * These are media transport operations.
 */
static pj_status_t transport_get_info (pjmedia_transport *tp,
				       pjmedia_transport_info *info);
static pj_status_t transport_attach   (pjmedia_transport *tp,
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       void*,
						       pj_ssize_t));
static void	   transport_detach   (pjmedia_transport *tp,
				       void *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				       const pj_sockaddr_t *addr,
				       unsigned addr_len,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_media_create(pjmedia_transport *tp,
				       pj_pool_t *sdp_pool,
				       unsigned options,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				       pj_pool_t *sdp_pool,
				       pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
				       pj_pool_t *pool,
				       const pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				       pjmedia_dir dir,
				       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);



static pjmedia_transport_op transport_srtp_op = 
{
    &transport_get_info,
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy
};

/* This function may also be used by other module, e.g: pjmedia/errno.c,
 * it should have C compatible declaration.
 */
PJ_BEGIN_DECL
    const char* get_libsrtp_errstr(int err);
PJ_END_DECL

const char* get_libsrtp_errstr(int err)
{
#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)
    static char *liberr[] = {
	"ok",				    /* err_status_ok            = 0  */
	"unspecified failure",		    /* err_status_fail          = 1  */
	"unsupported parameter",	    /* err_status_bad_param     = 2  */
	"couldn't allocate memory",	    /* err_status_alloc_fail    = 3  */
	"couldn't deallocate properly",	    /* err_status_dealloc_fail  = 4  */
	"couldn't initialize",		    /* err_status_init_fail     = 5  */
	"can't process as much data as requested", 
					    /* err_status_terminus      = 6  */
	"authentication failure",	    /* err_status_auth_fail     = 7  */
	"cipher failure",		    /* err_status_cipher_fail   = 8  */
	"replay check failed (bad index)",  /* err_status_replay_fail   = 9  */
	"replay check failed (index too old)", 
					    /* err_status_replay_old    = 10 */
	"algorithm failed test routine",    /* err_status_algo_fail     = 11 */
	"unsupported operation",	    /* err_status_no_such_op    = 12 */
	"no appropriate context found",	    /* err_status_no_ctx        = 13 */
	"unable to perform desired validation", 
					    /* err_status_cant_check    = 14 */
	"can't use key any more",	    /* err_status_key_expired   = 15 */
	"error in use of socket",	    /* err_status_socket_err    = 16 */
	"error in use POSIX signals",	    /* err_status_signal_err    = 17 */
	"nonce check failed",		    /* err_status_nonce_bad     = 18 */
	"couldn't read data",		    /* err_status_read_fail     = 19 */
	"couldn't write data",		    /* err_status_write_fail    = 20 */
	"error pasring data",		    /* err_status_parse_err     = 21 */
	"error encoding data",		    /* err_status_encode_err    = 22 */
	"error while using semaphores",	    /* err_status_semaphore_err = 23 */
	"error while using pfkey"	    /* err_status_pfkey_err     = 24 */
    };
    if (err >= 0 && err < (int)PJ_ARRAY_SIZE(liberr)) {
	return liberr[err];
    } else {
	static char msg[32];
	pj_ansi_snprintf(msg, sizeof(msg), "Unknown libsrtp error %d", err);
	return msg;
    }
#else
    static char msg[32];
    pj_ansi_snprintf(msg, sizeof(msg), "libsrtp error %d", err);
    return msg;
#endif
}

static pj_bool_t libsrtp_initialized;
static void pjmedia_srtp_deinit_lib(pjmedia_endpt *endpt);

PJ_DEF(pj_status_t) pjmedia_srtp_init_lib(pjmedia_endpt *endpt)
{
    if (libsrtp_initialized == PJ_FALSE) {
	err_status_t err;

	err = srtp_init();
	if (err != err_status_ok) { 
	    PJ_LOG(4, (THIS_FILE, "Failed to initialize libsrtp: %s", 
		       get_libsrtp_errstr(err)));
	    //return PJMEDIA_ERRNO_FROM_LIBSRTP(err);
	}

	if (pjmedia_endpt_atexit(endpt, pjmedia_srtp_deinit_lib) != PJ_SUCCESS)
	{
	    /* There will be memory leak when it fails to schedule libsrtp 
	     * deinitialization, however the memory leak could be harmless,
	     * since in modern OS's memory used by an application is released 
	     * when the application terminates.
	     */
	    PJ_LOG(4, (THIS_FILE, "Failed to register libsrtp deinit."));
	}

	libsrtp_initialized = PJ_TRUE;
    }
    
    return PJ_SUCCESS;
}

static void pjmedia_srtp_deinit_lib(pjmedia_endpt *endpt)
{
    err_status_t err;

    /* Note that currently this SRTP init/deinit is not equipped with
     * reference counter, it should be safe as normally there is only
     * one single instance of media endpoint and even if it isn't, the
     * pjmedia_transport_srtp_create() will invoke SRTP init (the only
     * drawback should be the delay described by #788).
     */

    PJ_UNUSED_ARG(endpt);

    err = srtp_deinit();
    if (err != err_status_ok) {
	PJ_LOG(4, (THIS_FILE, "Failed to deinitialize libsrtp: %s", 
		   get_libsrtp_errstr(err)));
    }

    libsrtp_initialized = PJ_FALSE;
}


static int get_crypto_idx(const pj_str_t* crypto_name)
{
    int i;
    int cs_cnt = sizeof(crypto_suites)/sizeof(crypto_suites[0]);
    
    /* treat unspecified crypto_name as crypto 'NULL' */
    if (crypto_name->slen == 0)
	return 0;

    for (i=0; i<cs_cnt; ++i) {
	if (!pj_stricmp2(crypto_name, crypto_suites[i].name))
	    return i;
    }

    return -1;
}


static int srtp_crypto_cmp(const pjmedia_srtp_crypto* c1,
			   const pjmedia_srtp_crypto* c2)
{
    int r;

    r = pj_strcmp(&c1->key, &c2->key);
    if (r != 0)
	return r;

    r = pj_stricmp(&c1->name, &c2->name);
    if (r != 0)
	return r;

    return (c1->flags != c2->flags);
}


static pj_bool_t srtp_crypto_empty(const pjmedia_srtp_crypto* c)
{
    return (c->name.slen==0 || c->key.slen==0);
}


PJ_DEF(void) pjmedia_srtp_setting_default(pjmedia_srtp_setting *opt)
{
    unsigned i;

    pj_assert(opt);

    pj_bzero(opt, sizeof(pjmedia_srtp_setting));
    opt->close_member_tp = PJ_TRUE;
    opt->use = PJMEDIA_SRTP_OPTIONAL;

    /* Copy default crypto-suites, but skip crypto 'NULL' */
    opt->crypto_count = sizeof(crypto_suites)/sizeof(crypto_suites[0]) - 1;
    for (i=0; i<opt->crypto_count; ++i)
	opt->crypto[i].name = pj_str(crypto_suites[i+1].name);
}


/*
 * Create an SRTP media transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_create(
				       pjmedia_endpt *endpt,
				       pjmedia_transport *tp,
				       const pjmedia_srtp_setting *opt,
				       pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    transport_srtp *srtp;
    pj_status_t status;
    unsigned i;

    PJ_ASSERT_RETURN(endpt && tp && p_tp, PJ_EINVAL);

    /* Check crypto availability */
    if (opt && opt->crypto_count == 0 && 
	opt->use == PJMEDIA_SRTP_MANDATORY)
	return PJMEDIA_SRTP_ESDPREQCRYPTO;

    /* Check crypto */
    if (opt && opt->use != PJMEDIA_SRTP_DISABLED) {
	for (i=0; i < opt->crypto_count; ++i) {
	    int cs_idx = get_crypto_idx(&opt->crypto[i].name);

	    /* check crypto name */
	    if (cs_idx == -1)
		return PJMEDIA_SRTP_ENOTSUPCRYPTO;

	    /* check key length */
	    if (opt->crypto[i].key.slen && 
		opt->crypto[i].key.slen < 
		(pj_ssize_t)crypto_suites[cs_idx].cipher_key_len)
		return PJMEDIA_SRTP_EINKEYLEN;
	}
    }

    /* Init libsrtp. */
    status = pjmedia_srtp_init_lib(endpt);
    if (status != PJ_SUCCESS)
	return status;

    pool = pjmedia_endpt_create_pool(endpt, "srtp%p", 1000, 1000);
    srtp = PJ_POOL_ZALLOC_T(pool, transport_srtp);

    srtp->pool = pool;
    srtp->session_inited = PJ_FALSE;
    srtp->bypass_srtp = PJ_FALSE;
    srtp->probation_cnt = PROBATION_CNT_INIT;

    if (opt) {
	srtp->setting = *opt;
	if (opt->use == PJMEDIA_SRTP_DISABLED)
	    srtp->setting.crypto_count = 0;

	for (i=0; i < srtp->setting.crypto_count; ++i) {
	    int cs_idx = get_crypto_idx(&opt->crypto[i].name);
	    pj_str_t tmp_key = opt->crypto[i].key;

	    /* re-set crypto */
	    srtp->setting.crypto[i].name = pj_str(crypto_suites[cs_idx].name);
	    /* cut key length */
	    if (tmp_key.slen)
		tmp_key.slen = crypto_suites[cs_idx].cipher_key_len;
	    pj_strdup(pool, &srtp->setting.crypto[i].key, &tmp_key);
	}
    } else {
	pjmedia_srtp_setting_default(&srtp->setting);
    }

    status = pj_lock_create_recursive_mutex(pool, pool->obj_name, &srtp->mutex);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    /* Initialize base pjmedia_transport */
    pj_memcpy(srtp->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
    if (tp)
	srtp->base.type = tp->type;
    else
	srtp->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;
    srtp->base.op = &transport_srtp_op;

    /* Set underlying transport */
    srtp->member_tp = tp;

    /* Initialize peer's SRTP usage mode. */
    srtp->peer_use = srtp->setting.use;

    /* Done */
    *p_tp = &srtp->base;

    return PJ_SUCCESS;
}


/*
 * Initialize and start SRTP session with the given parameters.
 */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_start(
			   pjmedia_transport *tp, 
			   const pjmedia_srtp_crypto *tx,
			   const pjmedia_srtp_crypto *rx)
{
    transport_srtp  *srtp = (transport_srtp*) tp;
    srtp_policy_t    tx_;
    srtp_policy_t    rx_;
    err_status_t     err;
    int		     cr_tx_idx = 0;
    int		     au_tx_idx = 0;
    int		     cr_rx_idx = 0;
    int		     au_rx_idx = 0;
    int		     crypto_suites_cnt;
    pj_status_t	     status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(tp && tx && rx, PJ_EINVAL);

    pj_lock_acquire(srtp->mutex);

    if (srtp->session_inited) {
	pjmedia_transport_srtp_stop(tp);
    }

    crypto_suites_cnt = sizeof(crypto_suites)/sizeof(crypto_suites[0]);

    /* Get encryption and authentication method */
    cr_tx_idx = au_tx_idx = get_crypto_idx(&tx->name);
    if (tx->flags & PJMEDIA_SRTP_NO_ENCRYPTION)
	cr_tx_idx = 0;
    if (tx->flags & PJMEDIA_SRTP_NO_AUTHENTICATION)
	au_tx_idx = 0;

    cr_rx_idx = au_rx_idx = get_crypto_idx(&rx->name);
    if (rx->flags & PJMEDIA_SRTP_NO_ENCRYPTION)
	cr_rx_idx = 0;
    if (rx->flags & PJMEDIA_SRTP_NO_AUTHENTICATION)
	au_rx_idx = 0;

    /* Check whether the crypto-suite requested is supported */
    if (cr_tx_idx == -1 || cr_rx_idx == -1 || au_tx_idx == -1 || 
	au_rx_idx == -1)
    {
	status = PJMEDIA_SRTP_ENOTSUPCRYPTO;
	goto on_return;
    }

    /* If all options points to 'NULL' method, just bypass SRTP */
    if (cr_tx_idx == 0 && cr_rx_idx == 0 && au_tx_idx == 0 && au_rx_idx == 0) {
	srtp->bypass_srtp = PJ_TRUE;
	goto on_return;
    }

    /* Check key length */
    if (tx->key.slen != (pj_ssize_t)crypto_suites[cr_tx_idx].cipher_key_len ||
        rx->key.slen != (pj_ssize_t)crypto_suites[cr_rx_idx].cipher_key_len)
    {
	status = PJMEDIA_SRTP_EINKEYLEN;
	goto on_return;
    }

    /* Init transmit direction */
    pj_bzero(&tx_, sizeof(srtp_policy_t));
    pj_memmove(srtp->tx_key, tx->key.ptr, tx->key.slen);
    if (cr_tx_idx && au_tx_idx)
	tx_.rtp.sec_serv    = sec_serv_conf_and_auth;
    else if (cr_tx_idx)
	tx_.rtp.sec_serv    = sec_serv_conf;
    else if (au_tx_idx)
	tx_.rtp.sec_serv    = sec_serv_auth;
    else
	tx_.rtp.sec_serv    = sec_serv_none;
    tx_.key		    = (uint8_t*)srtp->tx_key;
    tx_.ssrc.type	    = ssrc_any_outbound;
    tx_.ssrc.value	    = 0;
    tx_.rtp.cipher_type	    = crypto_suites[cr_tx_idx].cipher_type;
    tx_.rtp.cipher_key_len  = crypto_suites[cr_tx_idx].cipher_key_len;
    tx_.rtp.auth_type	    = crypto_suites[au_tx_idx].auth_type;
    tx_.rtp.auth_key_len    = crypto_suites[au_tx_idx].auth_key_len;
    tx_.rtp.auth_tag_len    = crypto_suites[au_tx_idx].srtp_auth_tag_len;
    tx_.rtcp		    = tx_.rtp;
    tx_.rtcp.auth_tag_len   = crypto_suites[au_tx_idx].srtcp_auth_tag_len;
    tx_.next		    = NULL;
    err = srtp_create(&srtp->srtp_tx_ctx, &tx_);
    if (err != err_status_ok) {
	status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
	goto on_return;
    }
    srtp->tx_policy = *tx;
    pj_strset(&srtp->tx_policy.key,  srtp->tx_key, tx->key.slen);
    srtp->tx_policy.name=pj_str(crypto_suites[get_crypto_idx(&tx->name)].name);


    /* Init receive direction */
    pj_bzero(&rx_, sizeof(srtp_policy_t));
    pj_memmove(srtp->rx_key, rx->key.ptr, rx->key.slen);
    if (cr_rx_idx && au_rx_idx)
	rx_.rtp.sec_serv    = sec_serv_conf_and_auth;
    else if (cr_rx_idx)
	rx_.rtp.sec_serv    = sec_serv_conf;
    else if (au_rx_idx)
	rx_.rtp.sec_serv    = sec_serv_auth;
    else
	rx_.rtp.sec_serv    = sec_serv_none;
    rx_.key		    = (uint8_t*)srtp->rx_key;
    rx_.ssrc.type	    = ssrc_any_inbound;
    rx_.ssrc.value	    = 0;
    rx_.rtp.sec_serv	    = crypto_suites[cr_rx_idx].service;
    rx_.rtp.cipher_type	    = crypto_suites[cr_rx_idx].cipher_type;
    rx_.rtp.cipher_key_len  = crypto_suites[cr_rx_idx].cipher_key_len;
    rx_.rtp.auth_type	    = crypto_suites[au_rx_idx].auth_type;
    rx_.rtp.auth_key_len    = crypto_suites[au_rx_idx].auth_key_len;
    rx_.rtp.auth_tag_len    = crypto_suites[au_rx_idx].srtp_auth_tag_len;
    rx_.rtcp		    = rx_.rtp;
    rx_.rtcp.auth_tag_len   = crypto_suites[au_rx_idx].srtcp_auth_tag_len;
    rx_.next		    = NULL;
    err = srtp_create(&srtp->srtp_rx_ctx, &rx_);
    if (err != err_status_ok) {
	srtp_dealloc(srtp->srtp_tx_ctx);
	status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
	goto on_return;
    }
    srtp->rx_policy = *rx;
    pj_strset(&srtp->rx_policy.key,  srtp->rx_key, rx->key.slen);
    srtp->rx_policy.name=pj_str(crypto_suites[get_crypto_idx(&rx->name)].name);

    /* Declare SRTP session initialized */
    srtp->session_inited = PJ_TRUE;

    /* Logging stuffs */
#if PJ_LOG_MAX_LEVEL >= 5
    {
	char b64[PJ_BASE256_TO_BASE64_LEN(MAX_KEY_LEN)];
	int b64_len;

	/* TX crypto and key */
	b64_len = sizeof(b64);
	status = pj_base64_encode((pj_uint8_t*)tx->key.ptr, tx->key.slen,
				  b64, &b64_len);
	if (status != PJ_SUCCESS)
	    b64_len = pj_ansi_sprintf(b64, "--key too long--");
	else
	    b64[b64_len] = '\0';
        
	PJ_LOG(5, (srtp->pool->obj_name, "TX: %s key=%s",
		   srtp->tx_policy.name.ptr, b64));
    if (srtp->tx_policy.flags) {
	    PJ_LOG(5,(srtp->pool->obj_name, "TX: disable%s%s",
		      (cr_tx_idx?"":" enc"),
		  (au_tx_idx?"":" auth")));
    }

	/* RX crypto and key */
	b64_len = sizeof(b64);
	status = pj_base64_encode((pj_uint8_t*)rx->key.ptr, rx->key.slen,
				  b64, &b64_len);
	if (status != PJ_SUCCESS)
	    b64_len = pj_ansi_sprintf(b64, "--key too long--");
	else
	    b64[b64_len] = '\0';

	PJ_LOG(5, (srtp->pool->obj_name, "RX: %s key=%s",
		   srtp->rx_policy.name.ptr, b64));
    if (srtp->rx_policy.flags) {
	    PJ_LOG(5,(srtp->pool->obj_name,"RX: disable%s%s",
		      (cr_rx_idx?"":" enc"),
		  (au_rx_idx?"":" auth")));
    }
    }
#endif

on_return:
    pj_lock_release(srtp->mutex);
    return status;
}

/*
 * Stop SRTP session.
 */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_stop(pjmedia_transport *srtp)
{
    transport_srtp *p_srtp = (transport_srtp*) srtp;
    err_status_t err;

    PJ_ASSERT_RETURN(srtp, PJ_EINVAL);

    pj_lock_acquire(p_srtp->mutex);

    if (!p_srtp->session_inited) {
	pj_lock_release(p_srtp->mutex);
	return PJ_SUCCESS;
    }

    err = srtp_dealloc(p_srtp->srtp_rx_ctx);
    if (err != err_status_ok) {
	PJ_LOG(4, (p_srtp->pool->obj_name, 
		   "Failed to dealloc RX SRTP context: %s",
		   get_libsrtp_errstr(err)));
    }
    err = srtp_dealloc(p_srtp->srtp_tx_ctx);
    if (err != err_status_ok) {
	PJ_LOG(4, (p_srtp->pool->obj_name, 
		   "Failed to dealloc TX SRTP context: %s",
		   get_libsrtp_errstr(err)));
    }

    p_srtp->session_inited = PJ_FALSE;
    pj_bzero(&p_srtp->rx_policy, sizeof(p_srtp->rx_policy));
    pj_bzero(&p_srtp->tx_policy, sizeof(p_srtp->tx_policy));

    pj_lock_release(p_srtp->mutex);

    return PJ_SUCCESS;
}

PJ_DEF(pjmedia_transport *) pjmedia_transport_srtp_get_member(
						pjmedia_transport *tp)
{
    transport_srtp *srtp = (transport_srtp*) tp;

    PJ_ASSERT_RETURN(tp, NULL);

    return srtp->member_tp;
}


static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    transport_srtp *srtp = (transport_srtp*) tp;
    pjmedia_srtp_info srtp_info;
    int spc_info_idx;

    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);
    PJ_ASSERT_RETURN(info->specific_info_cnt <
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT, PJ_ETOOMANY);
    PJ_ASSERT_RETURN(sizeof(pjmedia_srtp_info) <=
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE, PJ_ENOMEM);

    srtp_info.active = srtp->session_inited;
    srtp_info.rx_policy = srtp->rx_policy;
    srtp_info.tx_policy = srtp->tx_policy;
    srtp_info.use = srtp->setting.use;
    srtp_info.peer_use = srtp->peer_use;

    spc_info_idx = info->specific_info_cnt++;
    info->spc_info[spc_info_idx].type = PJMEDIA_TRANSPORT_TYPE_SRTP;
    info->spc_info[spc_info_idx].cbsize = sizeof(srtp_info);
    pj_memcpy(&info->spc_info[spc_info_idx].buffer, &srtp_info, 
	      sizeof(srtp_info));

    return pjmedia_transport_get_info(srtp->member_tp, info);
}

static pj_status_t transport_attach(pjmedia_transport *tp,
				    void *user_data,
				    const pj_sockaddr_t *rem_addr,
				    const pj_sockaddr_t *rem_rtcp,
				    unsigned addr_len,
				    void (*rtp_cb) (void*, void*,
						    pj_ssize_t),
				    void (*rtcp_cb)(void*, void*,
						    pj_ssize_t))
{
    transport_srtp *srtp = (transport_srtp*) tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Save the callbacks */
    pj_lock_acquire(srtp->mutex);
    srtp->rtp_cb = rtp_cb;
    srtp->rtcp_cb = rtcp_cb;
    srtp->user_data = user_data;
    pj_lock_release(srtp->mutex);

    /* Attach itself to transport */
    status = pjmedia_transport_attach(srtp->member_tp, srtp, rem_addr, 
				      rem_rtcp, addr_len, &srtp_rtp_cb,
				      &srtp_rtcp_cb);
    if (status != PJ_SUCCESS) {
	pj_lock_acquire(srtp->mutex);
	srtp->rtp_cb = NULL;
	srtp->rtcp_cb = NULL;
	srtp->user_data = NULL;
	pj_lock_release(srtp->mutex);
	return status;
    }

    return PJ_SUCCESS;
}

static void transport_detach(pjmedia_transport *tp, void *strm)
{
    transport_srtp *srtp = (transport_srtp*) tp;

    PJ_UNUSED_ARG(strm);
    PJ_ASSERT_ON_FAIL(tp, return);

    if (srtp->member_tp) {
	pjmedia_transport_detach(srtp->member_tp, srtp);
    }

    /* Clear up application infos from transport */
    pj_lock_acquire(srtp->mutex);
    srtp->rtp_cb = NULL;
    srtp->rtcp_cb = NULL;
    srtp->user_data = NULL;
    pj_lock_release(srtp->mutex);
}

static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    pj_status_t status;
    transport_srtp *srtp = (transport_srtp*) tp;
    int len = size;
    err_status_t err;

    if (srtp->bypass_srtp)
	return pjmedia_transport_send_rtp(srtp->member_tp, pkt, size);

    if (size > sizeof(srtp->rtp_tx_buffer))
	return PJ_ETOOBIG;

    pj_memcpy(srtp->rtp_tx_buffer, pkt, size);

    pj_lock_acquire(srtp->mutex);
    if (!srtp->session_inited) {
	pj_lock_release(srtp->mutex);
	return PJ_EINVALIDOP;
    }
    err = srtp_protect(srtp->srtp_tx_ctx, srtp->rtp_tx_buffer, &len);
    pj_lock_release(srtp->mutex);

    if (err == err_status_ok) {
	status = pjmedia_transport_send_rtp(srtp->member_tp, srtp->rtp_tx_buffer, len);
    } else {
	status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
    }
    
    return status;
}

static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}

static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				        const pj_sockaddr_t *addr,
				        unsigned addr_len,
				        const void *pkt,
				        pj_size_t size)
{
    pj_status_t status;
    transport_srtp *srtp = (transport_srtp*) tp;
    int len = size;
    err_status_t err;

    if (srtp->bypass_srtp) {
	return pjmedia_transport_send_rtcp2(srtp->member_tp, addr, addr_len, 
	                                    pkt, size);
    }

    if (size > sizeof(srtp->rtcp_tx_buffer))
	return PJ_ETOOBIG;

    pj_memcpy(srtp->rtcp_tx_buffer, pkt, size);

    pj_lock_acquire(srtp->mutex);
    if (!srtp->session_inited) {
	pj_lock_release(srtp->mutex);
	return PJ_EINVALIDOP;
    }
    err = srtp_protect_rtcp(srtp->srtp_tx_ctx, srtp->rtcp_tx_buffer, &len);
    pj_lock_release(srtp->mutex);

    if (err == err_status_ok) {
	status = pjmedia_transport_send_rtcp2(srtp->member_tp, addr, addr_len,
					      srtp->rtcp_tx_buffer, len);
    } else {
	status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
    }

    return status;
}


static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
					   pjmedia_dir dir,
					   unsigned pct_lost)
{
    transport_srtp *srtp = (transport_srtp *) tp;
    
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    return pjmedia_transport_simulate_lost(srtp->member_tp, dir, pct_lost);
}

static pj_status_t transport_destroy  (pjmedia_transport *tp)
{
    transport_srtp *srtp = (transport_srtp *) tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    if (srtp->setting.close_member_tp && srtp->member_tp) {
	pjmedia_transport_close(srtp->member_tp);
    }

    status = pjmedia_transport_srtp_stop(tp);

    /* In case mutex is being acquired by other thread */
    pj_lock_acquire(srtp->mutex);
    pj_lock_release(srtp->mutex);

    pj_lock_destroy(srtp->mutex);
    pj_pool_release(srtp->pool);

    return status;
}

/*
 * This callback is called by transport when incoming rtp is received
 */
static void srtp_rtp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_srtp *srtp = (transport_srtp *) user_data;
    int len = size;
    err_status_t err;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
    void *cb_data = NULL;

    if (srtp->bypass_srtp) {
	srtp->rtp_cb(srtp->user_data, pkt, size);
	return;
    }

    if (size < 0) {
	return;
    }

    /* Make sure buffer is 32bit aligned */
    PJ_ASSERT_ON_FAIL( (((long)pkt) & 0x03)==0, return );

    if (srtp->probation_cnt > 0)
	--srtp->probation_cnt;

    pj_lock_acquire(srtp->mutex);

    if (!srtp->session_inited) {
	pj_lock_release(srtp->mutex);
	return;
    }
    err = srtp_unprotect(srtp->srtp_rx_ctx, (pj_uint8_t*)pkt, &len);
    if (srtp->probation_cnt > 0 && 
	(err == err_status_replay_old || err == err_status_replay_fail)) 
    {
	/* Handle such condition that stream is updated (RTP seq is reinited
	* & SRTP is restarted), but some old packets are still coming 
	* so SRTP is learning wrong RTP seq. While the newly inited RTP seq
	* comes, SRTP thinks the RTP seq is replayed, so srtp_unprotect() 
	 * will return err_status_replay_*. Restarting SRTP can resolve this.
	*/
	pjmedia_srtp_crypto tx, rx;
	pj_status_t status;

	tx = srtp->tx_policy;
	rx = srtp->rx_policy;
	status = pjmedia_transport_srtp_start((pjmedia_transport*)srtp,
					      &tx, &rx);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(5,(srtp->pool->obj_name, "Failed to restart SRTP, err=%s", 
		      get_libsrtp_errstr(err)));
	} else if (!srtp->bypass_srtp) {
	    err = srtp_unprotect(srtp->srtp_rx_ctx, (pj_uint8_t*)pkt, &len);
	}
    }

    if (err != err_status_ok) {
	PJ_LOG(5,(srtp->pool->obj_name, 
		  "Failed to unprotect SRTP, pkt size=%d, err=%s", 
		  size, get_libsrtp_errstr(err)));
    } else {
	cb = srtp->rtp_cb;
	cb_data = srtp->user_data;
    }

    pj_lock_release(srtp->mutex);

    if (cb) {
	(*cb)(cb_data, pkt, len);
    }
}

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void srtp_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_srtp *srtp = (transport_srtp *) user_data;
    int len = size;
    err_status_t err;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
    void *cb_data = NULL;

    if (srtp->bypass_srtp) {
	srtp->rtcp_cb(srtp->user_data, pkt, size);
	return;
    }

    if (size < 0) {
	return;
    }

    /* Make sure buffer is 32bit aligned */
    PJ_ASSERT_ON_FAIL( (((long)pkt) & 0x03)==0, return );

    pj_lock_acquire(srtp->mutex);

    if (!srtp->session_inited) {
	pj_lock_release(srtp->mutex);
	return;
    }
    err = srtp_unprotect_rtcp(srtp->srtp_rx_ctx, (pj_uint8_t*)pkt, &len);
    if (err != err_status_ok) {
	PJ_LOG(5,(srtp->pool->obj_name, 
		  "Failed to unprotect SRTCP, pkt size=%d, err=%s",
		  size, get_libsrtp_errstr(err)));
    } else {
	cb = srtp->rtcp_cb;
	cb_data = srtp->user_data;
    }

    pj_lock_release(srtp->mutex);

    if (cb) {
	(*cb)(cb_data, pkt, len);
    }
}

/* Generate crypto attribute, including crypto key.
 * If crypto-suite chosen is crypto NULL, just return PJ_SUCCESS,
 * and set buffer_len = 0.
 */
static pj_status_t generate_crypto_attr_value(pj_pool_t *pool,
					      char *buffer, int *buffer_len, 
					      pjmedia_srtp_crypto *crypto,
					      int tag)
{
    pj_status_t status;
    int cs_idx = get_crypto_idx(&crypto->name);
    char b64_key[PJ_BASE256_TO_BASE64_LEN(MAX_KEY_LEN)+1];
    int b64_key_len = sizeof(b64_key);

    if (cs_idx == -1)
	return PJMEDIA_SRTP_ENOTSUPCRYPTO;

    /* Crypto-suite NULL. */
    if (cs_idx == 0) {
	*buffer_len = 0;
	return PJ_SUCCESS;
    }

    /* Generate key if not specified. */
    if (crypto->key.slen == 0) {
	pj_bool_t key_ok;
	char key[MAX_KEY_LEN];
	err_status_t err;
	unsigned i;

	PJ_ASSERT_RETURN(MAX_KEY_LEN >= crypto_suites[cs_idx].cipher_key_len,
			 PJ_ETOOSMALL);

	do {
	    key_ok = PJ_TRUE;

	    err = crypto_get_random((unsigned char*)key, 
				     crypto_suites[cs_idx].cipher_key_len);
	    if (err != err_status_ok) {
		PJ_LOG(5,(THIS_FILE, "Failed generating random key: %s",
			  get_libsrtp_errstr(err)));
		return PJMEDIA_ERRNO_FROM_LIBSRTP(err);
	    }
	    for (i=0; i<crypto_suites[cs_idx].cipher_key_len && key_ok; ++i)
		if (key[i] == 0) key_ok = PJ_FALSE;

	} while (!key_ok);
	crypto->key.ptr = (char*)
			  pj_pool_zalloc(pool, 
					 crypto_suites[cs_idx].cipher_key_len);
	pj_memcpy(crypto->key.ptr, key, crypto_suites[cs_idx].cipher_key_len);
	crypto->key.slen = crypto_suites[cs_idx].cipher_key_len;
    }

    if (crypto->key.slen != (pj_ssize_t)crypto_suites[cs_idx].cipher_key_len)
	return PJMEDIA_SRTP_EINKEYLEN;

    /* Key transmitted via SDP should be base64 encoded. */
    status = pj_base64_encode((pj_uint8_t*)crypto->key.ptr, crypto->key.slen,
			      b64_key, &b64_key_len);
    if (status != PJ_SUCCESS) {
	PJ_LOG(5,(THIS_FILE, "Failed encoding plain key to base64"));
	return status;
    }

    b64_key[b64_key_len] = '\0';
    
    PJ_ASSERT_RETURN(*buffer_len >= (crypto->name.slen + \
		     b64_key_len + 16), PJ_ETOOSMALL);

    /* Print the crypto attribute value. */
    *buffer_len = pj_ansi_snprintf(buffer, *buffer_len, "%d %s inline:%s",
				   tag, 
				   crypto_suites[cs_idx].name,
				   b64_key);

    return PJ_SUCCESS;
}

/* Parse crypto attribute line */
static pj_status_t parse_attr_crypto(pj_pool_t *pool,
				     const pjmedia_sdp_attr *attr,
				     pjmedia_srtp_crypto *crypto,
				     int *tag)
{
    pj_str_t input;
    char *token;
    int token_len;
    pj_str_t tmp;
    pj_status_t status;
    int itmp;

    pj_bzero(crypto, sizeof(*crypto));
    pj_strdup_with_null(pool, &input, &attr->value);

    /* Tag */
    token = strtok(input.ptr, " ");
    if (!token) {
	PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting tag"));
	return PJMEDIA_SDP_EINATTR;
    }
    token_len = pj_ansi_strlen(token);

    /* Tag must not use leading zeroes. */
    if (token_len > 1 && *token == '0')
	return PJMEDIA_SDP_EINATTR;

    /* Tag must be decimal, i.e: contains only digit '0'-'9'. */
    for (itmp = 0; itmp < token_len; ++itmp)
	if (!pj_isdigit(token[itmp]))
	    return PJMEDIA_SDP_EINATTR;

    /* Get tag value. */
    *tag = atoi(token);

    /* Crypto-suite */
    token = strtok(NULL, " ");
    if (!token) {
	PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting crypto suite"));
	return PJMEDIA_SDP_EINATTR;
    }
    crypto->name = pj_str(token);

    /* Key method */
    token = strtok(NULL, ":");
    if (!token) {
	PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting key method"));
	return PJMEDIA_SDP_EINATTR;
    }
    if (pj_ansi_stricmp(token, "inline")) {
	PJ_LOG(4,(THIS_FILE, "Attribute crypto key method '%s' not supported!",
	          token));
	return PJMEDIA_SDP_EINATTR;
    }

    /* Key */
    token = strtok(NULL, "| ");
    if (!token) {
	PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting key"));
	return PJMEDIA_SDP_EINATTR;
    }
    tmp = pj_str(token);
    if (PJ_BASE64_TO_BASE256_LEN(tmp.slen) > MAX_KEY_LEN) {
	PJ_LOG(4,(THIS_FILE, "Key too long"));
	return PJMEDIA_SRTP_EINKEYLEN;
    }

    /* Decode key */
    crypto->key.ptr = (char*) pj_pool_zalloc(pool, MAX_KEY_LEN);
    itmp = MAX_KEY_LEN;
    status = pj_base64_decode(&tmp, (pj_uint8_t*)crypto->key.ptr, 
			      &itmp);
    if (status != PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "Failed decoding crypto key from base64"));
	return status;
    }
    crypto->key.slen = itmp;

    return PJ_SUCCESS;
}

static pj_status_t transport_media_create(pjmedia_transport *tp,
				          pj_pool_t *sdp_pool,
					  unsigned options,
				          const pjmedia_sdp_session *sdp_remote,
					  unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*) tp;
    unsigned member_tp_option;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);
    
    pj_bzero(&srtp->rx_policy_neg, sizeof(srtp->rx_policy_neg));
    pj_bzero(&srtp->tx_policy_neg, sizeof(srtp->tx_policy_neg));

    srtp->media_option = options;
    member_tp_option = options | PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

    srtp->offerer_side = sdp_remote == NULL;

    /* Validations */
    if (srtp->offerer_side) {

	if (srtp->setting.use == PJMEDIA_SRTP_DISABLED)
	    goto BYPASS_SRTP;

    } else {

	pjmedia_sdp_media *m_rem;

	m_rem = sdp_remote->media[media_index];

	/* Nothing to do on inactive media stream */
	if (pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL))
	    goto BYPASS_SRTP;

	/* Validate remote media transport based on SRTP usage option.
	 */
	switch (srtp->setting.use) {
	    case PJMEDIA_SRTP_DISABLED:
		if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP) == 0)
		    return PJMEDIA_SRTP_ESDPINTRANSPORT;
		goto BYPASS_SRTP;
	    case PJMEDIA_SRTP_OPTIONAL:
		break;
	    case PJMEDIA_SRTP_MANDATORY:
		if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP) != 0)
		    return PJMEDIA_SRTP_ESDPINTRANSPORT;
		break;
	}

    }
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_SRTP:
    srtp->bypass_srtp = PJ_TRUE;
    member_tp_option &= ~PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

PROPAGATE_MEDIA_CREATE:
	// DEAN assign app's lock object.
	srtp->member_tp->app_lock = srtp->base.app_lock;
	srtp->member_tp->call_id = srtp->base.call_id;
    return pjmedia_transport_media_create(srtp->member_tp, sdp_pool, 
					  member_tp_option, sdp_remote,
					  media_index);
}

static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
					pj_pool_t *sdp_pool,
					pjmedia_sdp_session *sdp_local,
					const pjmedia_sdp_session *sdp_remote,
					unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    enum { MAXLEN = 512 };
    char buffer[MAXLEN];
    int buffer_len;
    pj_status_t status;
    pjmedia_sdp_attr *attr;
    pj_str_t attr_value;
    unsigned i, j;

    PJ_ASSERT_RETURN(tp && sdp_pool && sdp_local, PJ_EINVAL);
    
    pj_bzero(&srtp->rx_policy_neg, sizeof(srtp->rx_policy_neg));
    pj_bzero(&srtp->tx_policy_neg, sizeof(srtp->tx_policy_neg));

    srtp->offerer_side = sdp_remote == NULL;

    m_rem = sdp_remote ? sdp_remote->media[media_index] : NULL;
    m_loc = sdp_local->media[media_index];

    /* Bypass if media transport is not RTP/AVP or RTP/SAVP */
    if (pj_stricmp(&m_loc->desc.transport, &ID_RTP_AVP)  != 0 && 
	pj_stricmp(&m_loc->desc.transport, &ID_RTP_SAVP) != 0)
	goto BYPASS_SRTP;

    /* If the media is inactive, do nothing. */
    /* No, we still need to process SRTP offer/answer even if the media is
     * marked as inactive, because the transport is still alive in this
     * case (e.g. for keep-alive). See:
     *   http://trac.pjsip.org/repos/ticket/1079
     */
    /*
    if (pjmedia_sdp_media_find_attr(m_loc, &ID_INACTIVE, NULL) || 
	(m_rem && pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL)))
	goto BYPASS_SRTP;
    */

    /* Check remote media transport & set local media transport 
     * based on SRTP usage option.
     */
    if (srtp->offerer_side) {

	/* Generate transport */
	switch (srtp->setting.use) {
	    case PJMEDIA_SRTP_DISABLED:
		goto BYPASS_SRTP;
	    case PJMEDIA_SRTP_OPTIONAL:
		m_loc->desc.transport = 
				(srtp->peer_use == PJMEDIA_SRTP_MANDATORY)?
				ID_RTP_SAVP : ID_RTP_AVP;
		break;
	    case PJMEDIA_SRTP_MANDATORY:
		m_loc->desc.transport = ID_RTP_SAVP;
		break;
	}

	/* Generate crypto attribute if not yet */
	if (pjmedia_sdp_media_find_attr(m_loc, &ID_CRYPTO, NULL) == NULL) {
	    for (i=0; i<srtp->setting.crypto_count; ++i) {
		/* Offer crypto-suites based on setting. */
		buffer_len = MAXLEN;
		status = generate_crypto_attr_value(srtp->pool, buffer, &buffer_len,
						    &srtp->setting.crypto[i],
						    i+1);
		if (status != PJ_SUCCESS)
		    return status;

		/* If buffer_len==0, just skip the crypto attribute. */
		if (buffer_len) {
		    pj_strset(&attr_value, buffer, buffer_len);
		    attr = pjmedia_sdp_attr_create(srtp->pool, ID_CRYPTO.ptr, 
						   &attr_value);
		    m_loc->attr[m_loc->attr_count++] = attr;
		}
	    }
	}

    } else {
	/* Answerer side */

	pj_assert(sdp_remote && m_rem);

	/* Generate transport */
	switch (srtp->setting.use) {
	    case PJMEDIA_SRTP_DISABLED:
		if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP) == 0)
		    return PJMEDIA_SRTP_ESDPINTRANSPORT;
		goto BYPASS_SRTP;
	    case PJMEDIA_SRTP_OPTIONAL:
		m_loc->desc.transport = m_rem->desc.transport;
		break;
	    case PJMEDIA_SRTP_MANDATORY:
		if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP) != 0)
		    return PJMEDIA_SRTP_ESDPINTRANSPORT;
		m_loc->desc.transport = ID_RTP_SAVP;
		break;
	}

	/* Generate crypto attribute if not yet */
	if (pjmedia_sdp_media_find_attr(m_loc, &ID_CRYPTO, NULL) == NULL) {

	    pjmedia_srtp_crypto tmp_rx_crypto;
	    pj_bool_t has_crypto_attr = PJ_FALSE;
	    int matched_idx = -1;
	    int chosen_tag = 0;
	    int tags[64]; /* assume no more than 64 crypto attrs in a media */
	    unsigned cr_attr_count = 0;

	    /* Find supported crypto-suite, get the tag, and assign policy_local */
	    for (i=0; i<m_rem->attr_count; ++i) {
		if (pj_stricmp(&m_rem->attr[i]->name, &ID_CRYPTO) != 0)
		    continue;

		has_crypto_attr = PJ_TRUE;

		status = parse_attr_crypto(srtp->pool, m_rem->attr[i], 
					   &tmp_rx_crypto, &tags[cr_attr_count]);
		if (status != PJ_SUCCESS)
		    return status;
    	 
		/* Check duplicated tag */
		for (j=0; j<cr_attr_count; ++j) {
		    if (tags[j] == tags[cr_attr_count]) {
			DEACTIVATE_MEDIA(sdp_pool, m_loc);
			return PJMEDIA_SRTP_ESDPDUPCRYPTOTAG;
		    }
		}

		if (matched_idx == -1) {
		    /* lets see if the crypto-suite offered is supported */
		    for (j=0; j<srtp->setting.crypto_count; ++j)
			if (pj_stricmp(&tmp_rx_crypto.name, 
				       &srtp->setting.crypto[j].name) == 0)
			{
			    int cs_idx = get_crypto_idx(&tmp_rx_crypto.name);

			    /* Force to use test key */
			    /* bad keys for snom: */
			    //char *hex_test_key = "58b29c5c8f42308120ce857e439f2d"
			    //		     "7810a8b10ad0b1446be5470faea496";
			    //char *hex_test_key = "20a26aac7ba062d356ff52b61e3993"
			    //		     "ccb78078f12c64db94b9c294927fd0";
			    //pj_str_t *test_key = &srtp->setting.crypto[j].key;
			    //char  *raw_test_key = pj_pool_zalloc(srtp->pool, 64);
			    //hex_string_to_octet_string(
			    //		raw_test_key,
			    //		hex_test_key,
			    //		strlen(hex_test_key));
			    //pj_strset(test_key, raw_test_key, 
			    //	  crypto_suites[cs_idx].cipher_key_len);
			    /* EO Force to use test key */

			    if (tmp_rx_crypto.key.slen != 
				(int)crypto_suites[cs_idx].cipher_key_len)
				return PJMEDIA_SRTP_EINKEYLEN;

			    srtp->rx_policy_neg = tmp_rx_crypto;
			    chosen_tag = tags[cr_attr_count];
			    matched_idx = j;
    			    break;
			}
		}
		cr_attr_count++;
	    }

	    /* Check crypto negotiation result */
	    switch (srtp->setting.use) {
		case PJMEDIA_SRTP_DISABLED:
		    pj_assert(!"Should never reach here");
		    break;

		case PJMEDIA_SRTP_OPTIONAL:
		    /* bypass SRTP when no crypto-attr and remote uses RTP/AVP */
		    if (!has_crypto_attr && 
			pj_stricmp(&m_rem->desc.transport, &ID_RTP_AVP) == 0)
			goto BYPASS_SRTP;
		    /* bypass SRTP when nothing match and remote uses RTP/AVP */
		    else if (matched_idx == -1 && 
			pj_stricmp(&m_rem->desc.transport, &ID_RTP_AVP) == 0)
			goto BYPASS_SRTP;
		    break;

		case PJMEDIA_SRTP_MANDATORY:
		    /* Do nothing, intentional */
		    break;
	    }

	    /* No crypto attr */
	    if (!has_crypto_attr) {
		DEACTIVATE_MEDIA(sdp_pool, m_loc);
		return PJMEDIA_SRTP_ESDPREQCRYPTO;
	    }

	    /* No crypto match */
	    if (matched_idx == -1) {
		DEACTIVATE_MEDIA(sdp_pool, m_loc);
		return PJMEDIA_SRTP_ENOTSUPCRYPTO;
	    }

	    /* we have to generate crypto answer, 
	     * with srtp->tx_policy_neg matched the offer
	     * and rem_tag contains matched offer tag.
	     */
	    buffer_len = MAXLEN;
	    status = generate_crypto_attr_value(srtp->pool, buffer, &buffer_len,
						&srtp->setting.crypto[matched_idx],
						chosen_tag);
	    if (status != PJ_SUCCESS)
		return status;

	    srtp->tx_policy_neg = srtp->setting.crypto[matched_idx];
	    
	    /* If buffer_len==0, just skip the crypto attribute. */
	    if (buffer_len) {
		pj_strset(&attr_value, buffer, buffer_len);
		attr = pjmedia_sdp_attr_create(sdp_pool, ID_CRYPTO.ptr, 
					       &attr_value);
		m_loc->attr[m_loc->attr_count++] = attr;
	    }

	    /* At this point, we get valid rx_policy_neg & tx_policy_neg. */
	}
	    
    }
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_SRTP:
    /* Do not update this flag here as actually the media session hasn't been
     * updated.
     */
    //srtp->bypass_srtp = PJ_TRUE;

PROPAGATE_MEDIA_CREATE:
    return pjmedia_transport_encode_sdp(srtp->member_tp, sdp_pool, 
					sdp_local, sdp_remote, media_index);
}



static pj_status_t transport_media_start(pjmedia_transport *tp,
				         pj_pool_t *pool,
				         const pjmedia_sdp_session *sdp_local,
				         const pjmedia_sdp_session *sdp_remote,
				         unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    pj_status_t status;
    unsigned i;

    PJ_ASSERT_RETURN(tp && pool && sdp_local && sdp_remote, PJ_EINVAL);

    m_rem = sdp_remote->media[media_index];
    m_loc = sdp_local->media[media_index];

    if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP) == 0)
	srtp->peer_use = PJMEDIA_SRTP_MANDATORY;
    else
	srtp->peer_use = PJMEDIA_SRTP_OPTIONAL;

    /* For answerer side, this function will just have to start SRTP */

    /* Check remote media transport & set local media transport 
     * based on SRTP usage option.
     */
    if (srtp->offerer_side) {
	if (srtp->setting.use == PJMEDIA_SRTP_DISABLED) {
	    if (pjmedia_sdp_media_find_attr(m_rem, &ID_CRYPTO, NULL)) {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SRTP_ESDPINCRYPTO;
	    }
	    goto BYPASS_SRTP;
	} else if (srtp->setting.use == PJMEDIA_SRTP_OPTIONAL) {
	    // Regardless the answer's transport type (RTP/AVP or RTP/SAVP),
	    // the answer must be processed through in optional mode.
	    // Please note that at this point transport type is ensured to be 
	    // RTP/AVP or RTP/SAVP, see transport_media_create()
	    //if (pj_stricmp(&m_rem->desc.transport, &m_loc->desc.transport)) {
		//DEACTIVATE_MEDIA(pool, m_loc);
		//return PJMEDIA_SDP_EINPROTO;
	    //}
	} else if (srtp->setting.use == PJMEDIA_SRTP_MANDATORY) {
	    if (pj_stricmp(&m_rem->desc.transport, &ID_RTP_SAVP)) {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SDP_EINPROTO;
	    }
	}
    }
    
    if (srtp->offerer_side) {
	/* find supported crypto-suite, get the tag, and assign policy_local */
	pjmedia_srtp_crypto tmp_tx_crypto;
	pj_bool_t has_crypto_attr = PJ_FALSE;
	int rem_tag;

	for (i=0; i<m_rem->attr_count; ++i) {
	    if (pj_stricmp(&m_rem->attr[i]->name, &ID_CRYPTO) != 0)
		continue;

	    /* more than one crypto attribute in media answer */
	    if (has_crypto_attr) {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SRTP_ESDPAMBIGUEANS;
	    }

	    has_crypto_attr = PJ_TRUE;

	    status = parse_attr_crypto(srtp->pool, m_rem->attr[i], 
				       &tmp_tx_crypto, &rem_tag);
	    if (status != PJ_SUCCESS)
		return status;


	    /* our offer tag is always ordered by setting */
	    if (rem_tag < 1 || rem_tag > (int)srtp->setting.crypto_count) {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SRTP_ESDPINCRYPTOTAG;
	    }

	    /* match the crypto name */
	    if (pj_stricmp(&tmp_tx_crypto.name, 
		&srtp->setting.crypto[rem_tag-1].name) != 0)
	    {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SRTP_ECRYPTONOTMATCH;
	    }

	    srtp->tx_policy_neg = srtp->setting.crypto[rem_tag-1];
	    srtp->rx_policy_neg = tmp_tx_crypto;
	}

	if (srtp->setting.use == PJMEDIA_SRTP_DISABLED) {
	    /* should never reach here */
	    goto BYPASS_SRTP;
	} else if (srtp->setting.use == PJMEDIA_SRTP_OPTIONAL) {
	    if (!has_crypto_attr)
		goto BYPASS_SRTP;
	} else if (srtp->setting.use == PJMEDIA_SRTP_MANDATORY) {
	    if (!has_crypto_attr) {
		DEACTIVATE_MEDIA(pool, m_loc);
		return PJMEDIA_SRTP_ESDPREQCRYPTO;
	    }
	}

	/* At this point, we get valid rx_policy_neg & tx_policy_neg. */
    }

    /* Make sure we have the SRTP policies */
    if (srtp_crypto_empty(&srtp->tx_policy_neg) || 
	srtp_crypto_empty(&srtp->rx_policy_neg))
    {
	goto BYPASS_SRTP;
    }

    /* Reset probation counts */
    srtp->probation_cnt = PROBATION_CNT_INIT;

    /* Got policy_local & policy_remote, let's initalize the SRTP */

    /* Ticket #1075: media_start() is called whenever media description
     * gets updated, e.g: call hold, however we should restart SRTP only
     * when the SRTP policy settings are updated.
     */
    if (srtp_crypto_cmp(&srtp->tx_policy_neg, &srtp->tx_policy) ||
	srtp_crypto_cmp(&srtp->rx_policy_neg, &srtp->rx_policy))
    {
	status = pjmedia_transport_srtp_start(tp,
					      &srtp->tx_policy_neg,
					      &srtp->rx_policy_neg);
	if (status != PJ_SUCCESS)
	    return status;
    }

    srtp->bypass_srtp = PJ_FALSE;

    goto PROPAGATE_MEDIA_START;

BYPASS_SRTP:
    srtp->bypass_srtp = PJ_TRUE;
    srtp->peer_use = PJMEDIA_SRTP_DISABLED;
    if (srtp->session_inited) {
	pjmedia_transport_srtp_stop(tp);
    }

PROPAGATE_MEDIA_START:
    return pjmedia_transport_media_start(srtp->member_tp, pool, 
					 sdp_local, sdp_remote,
				         media_index);
}

static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct transport_srtp *srtp = (struct transport_srtp*) tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    status = pjmedia_transport_media_stop(srtp->member_tp);
    if (status != PJ_SUCCESS)
	PJ_LOG(4, (srtp->pool->obj_name, 
		   "SRTP failed stop underlying media transport."));

    return pjmedia_transport_srtp_stop(tp);
}

/* Utility */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_decrypt_pkt(pjmedia_transport *tp,
						       pj_bool_t is_rtp,
						       void *pkt,
						       int *pkt_len)
{
    transport_srtp *srtp = (transport_srtp *)tp;
    err_status_t err;

    if (srtp->bypass_srtp)
	return PJ_SUCCESS;

    PJ_ASSERT_RETURN(tp && pkt && (*pkt_len>0), PJ_EINVAL);
    PJ_ASSERT_RETURN(srtp->session_inited, PJ_EINVALIDOP);

    /* Make sure buffer is 32bit aligned */
    PJ_ASSERT_ON_FAIL( (((long)pkt) & 0x03)==0, return PJ_EINVAL);

    pj_lock_acquire(srtp->mutex);

    if (!srtp->session_inited) {
	pj_lock_release(srtp->mutex);
	return PJ_EINVALIDOP;
    }

    if (is_rtp)
	err = srtp_unprotect(srtp->srtp_rx_ctx, pkt, pkt_len);
    else
	err = srtp_unprotect_rtcp(srtp->srtp_rx_ctx, pkt, pkt_len);
    
    if (err != err_status_ok) {
	PJ_LOG(5,(srtp->pool->obj_name, 
		  "Failed to unprotect SRTP, pkt size=%d, err=%s", 
		  *pkt_len, get_libsrtp_errstr(err)));
    }

    pj_lock_release(srtp->mutex);

    return (err==err_status_ok) ? PJ_SUCCESS : PJMEDIA_ERRNO_FROM_LIBSRTP(err);
}

#endif


