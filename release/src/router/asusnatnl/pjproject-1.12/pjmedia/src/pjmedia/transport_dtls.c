/* $Id: transport_dtls.c 4387 2013-02-27 10:16:08Z ming $ */
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

#include <pjmedia/transport_dtls.h>
#include <pjmedia/endpoint.h>
#include <pjlib-util/base64.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/ssl_sock.h>
#include <pj/math.h>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#define in_port_t u_short
#define ssize_t int
#else
#include <sys/socket.h>
#endif

/* 
 * Include OpenSSL headers 
 */
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>

#define THIS_FILE   "transport_dtls.c"

/* Workaround for ticket #985 */
#define DELAYED_CLOSE_TIMEOUT	200
#define HANDSHAKE_RETRANS_TIMEOUT	100

/* Maximum ciphers */
#define MAX_CIPHERS		100

#define COOKIE_SECRET_LENGTH 16
unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
int cookie_initialized=0;

/*
 * SSL/TLS state enumeration.
 */
enum ssl_state {
    SSL_STATE_NULL,
    SSL_STATE_HANDSHAKING,
    SSL_STATE_ESTABLISHED
};

/*
 * Internal timer types.
 */
enum timer_id
{
	TIMER_NONE,
	TIMER_HANDSHAKE_RETRANSMISSION,
	TIMER_HANDSHAKE_TIMEOUT,
    TIMER_CLOSE
};

/*
 * Structure of SSL socket read buffer.
 */
typedef struct read_data_t
{
    void		 *data;
    pj_size_t		  len;
} read_data_t;

/*
 * Get the offset of pointer to read-buffer of SSL socket from read-buffer
 * of active socket. Note that both SSL socket and active socket employ 
 * different but correlated read-buffers (as much as async_cnt for each),
 * and to make it easier/faster to find corresponding SSL socket's read-buffer
 * from known active socket's read-buffer, the pointer of corresponding 
 * SSL socket's read-buffer is stored right after the end of active socket's
 * read-buffer.
 */
#define OFFSET_OF_READ_DATA_PTR(dtls, rbuf) \
					(read_data_t**) \
					((pj_int8_t*)(rbuf) + \
					dtls->setting.read_buffer_size)

/*
 * Structure of SSL socket write data.
 */
typedef struct write_data_t {
    PJ_DECL_LIST_MEMBER(struct write_data_t);
    pj_size_t 	 	 record_len;
    pj_size_t 	 	 plain_data_len;
    pj_size_t 	 	 data_len;
    unsigned		 flags;
    union {
	char		 content[1];
	const char	*ptr;
    } data;
} write_data_t;

/*
 * Structure of SSL socket write buffer (circular buffer).
 */
typedef struct send_buf_t {
    char		*buf;
    pj_size_t		 max_len;    
    char		*start;
    pj_size_t		 len;
} send_buf_t;

typedef struct transport_dtls
{
    pjmedia_transport	 base;		    /**< Base transport interface.  */
	pj_pool_t		*pool;		    /**< Pool for transport DTLS.   */
	pj_lock_t		*mutex;		    /**< Mutex for libdtls contexts.*/
	//pj_lock_t		*write_mutex;		    /**< Mutex for libdtls contexts.*/
	pj_sem_t		*handshake_sem;
	char			dtls_snd_buffer[NATNL_DTLS_MAX_SEND_LEN]; // Encrypted data.
	char			dtls_rcv_buffer[TCP_OR_UDP_TOTAL_HEADER_SIZE+NATNL_PKT_MAX_LEN];  // Decrypted data.
    pjmedia_dtls_setting setting;
    unsigned		media_option;
	pj_sockaddr		rem_addr;

    /* DTLS policy */
    pj_bool_t		 session_inited;
    pj_bool_t		 offerer_side;
    pj_bool_t		 bypass_dtls;

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
    pjmedia_dtls_use	 peer_use;

	unsigned long	  last_err;

	pjmedia_dtls_cert	 *cert;

	pj_ssl_cert_info	  local_cert_info;
	pj_ssl_cert_info	  remote_cert_info;

	pj_status_t		  verify_status;

	SSL_CTX		 *ossl_ctx;
	SSL			 *ossl_ssl;
	BIO			 *ossl_rbio;
	BIO			 *ossl_wbio;

	enum ssl_state	  ssl_state;
	pj_timer_entry	  timer;
	pj_size_t		  read_size;

	write_data_t	  write_pending;/* list of pending write to OpenSSL */
	write_data_t	  write_pending_empty; /* cache for write_pending   */
	pj_bool_t		  flushing_write_pend; /* flag of flushing is ongoing*/
	send_buf_t		  send_buf;
	write_data_t	  send_pending;	/* list of pending write to network */

	pj_timer_heap_t *timer_heap;


	int				  delayed_send_size;
	int				  handshake_retrans_cnt;

	pj_sockaddr *turn_mapped_addr;  // Save turn mapped address for notifying upper stack.
} transport_dtls;

/*
 * This callback is called by transport when incoming rtp is received
 */
static void dtls_rtp_cb( void *user_data, void *pkt, pj_ssize_t size);

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void dtls_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size);

static pj_status_t set_cipher_list(transport_dtls *dtls);

#ifdef ENABLE_DELAYED_SEND
static write_data_t* alloc_send_data(transport_dtls *dtls, pj_size_t len);
static void free_send_data(transport_dtls *dtls, write_data_t *wdata);
static pj_status_t flush_delayed_send(transport_dtls *dtls);
#endif

static const pj_str_t ID_TNL_PLAIN  = { "RTP/AVP", 7 };
static const pj_str_t ID_TNL_DTLS = { "RTP/SAVP", 8 };

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



static pjmedia_transport_op transport_dtls_op = 
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
/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */

/**
 * Mapping from OpenSSL error codes to pjlib error space.
 */

#define PJ_SSL_ERRNO_START		(PJ_ERRNO_START_USER + \
					 PJ_ERRNO_SPACE_SIZE*6)

#define PJ_SSL_ERRNO_SPACE_SIZE		PJ_ERRNO_SPACE_SIZE

/* Expected maximum value of reason component in OpenSSL error code */
#define MAX_OSSL_ERR_REASON		1200



static pj_status_t STATUS_FROM_SSL_ERR(transport_dtls *dtls,
				       unsigned long err)
{
    pj_status_t status;

    /* General SSL error, dig more from OpenSSL error queue */
    if (err == SSL_ERROR_SSL)
	err = ERR_get_error();

    /* OpenSSL error range is much wider than PJLIB errno space, so
     * if it exceeds the space, only the error reason will be kept.
     * Note that the last native error will be kept as is and can be
     * retrieved via SSL socket info.
     */
    status = ERR_GET_LIB(err)*MAX_OSSL_ERR_REASON + ERR_GET_REASON(err);
    if (status > PJ_SSL_ERRNO_SPACE_SIZE)
	status = ERR_GET_REASON(err);

    status += PJ_SSL_ERRNO_START;
    dtls->last_err = err;
    return status;
}

static pj_status_t GET_SSL_STATUS(transport_dtls *dtls)
{
    return STATUS_FROM_SSL_ERR(dtls, ERR_get_error());
}


/*
 * Get error string of OpenSSL.
 */
static pj_str_t ssl_strerror(pj_status_t status, 
			     char *buf, pj_size_t bufsize)
{
    pj_str_t errstr;
    unsigned long ssl_err = status;

    if (ssl_err) {
	unsigned long l, r;
	ssl_err -= PJ_SSL_ERRNO_START;
	l = ssl_err / MAX_OSSL_ERR_REASON;
	r = ssl_err % MAX_OSSL_ERR_REASON;
	ssl_err = ERR_PACK(l, 0, r);
    }

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    {
	const char *tmp = NULL;
	    tmp = ERR_reason_error_string(ssl_err);
	if (tmp) {
	    pj_ansi_strncpy(buf, tmp, bufsize);
	    errstr = pj_str(buf);
	    return errstr;
	}
    }

#endif	/* PJ_HAS_ERROR_STRING */

    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown OpenSSL error %lu",
				   ssl_err);

    return errstr;
}

/* OpenSSL library initialization counter */
static int openssl_init_count;

/* OpenSSL available ciphers */
static unsigned openssl_cipher_num;
static struct openssl_ciphers_t {
	pj_ssl_cipher    id;
	const char	    *name;
} openssl_ciphers[MAX_CIPHERS];

/* OpenSSL application data index */
static int sslsock_idx;


/* Initialize OpenSSL */
static pj_status_t init_openssl(void)
{
	pj_status_t status;

	if (openssl_init_count)
		return PJ_SUCCESS;

	openssl_init_count = 1;

	/* Register error subsystem */
	status = pj_register_strerror(PJ_SSL_ERRNO_START, 
		PJ_SSL_ERRNO_SPACE_SIZE, 
		&ssl_strerror);
	pj_assert(status == PJ_SUCCESS);

	/* Init OpenSSL lib */
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();

	/* Init available ciphers */
	if (openssl_cipher_num == 0) {
		SSL_METHOD *meth = NULL;
		SSL_CTX *ctx;
		SSL *ssl;
		STACK_OF(SSL_CIPHER) *sk_cipher;
		unsigned i, n;

		meth = (SSL_METHOD*)SSLv23_server_method();
		if (!meth)
			meth = (SSL_METHOD*)TLSv1_server_method();
		if (!meth)
			meth = (SSL_METHOD*)SSLv3_server_method();
#ifndef OPENSSL_NO_SSL2
		if (!meth)
			meth = (SSL_METHOD*)SSLv2_server_method();
#endif
		meth = (SSL_METHOD*)DTLSv1_server_method();
		if (!meth)
		pj_assert(meth);

		ctx=SSL_CTX_new(meth);
		SSL_CTX_set_cipher_list(ctx, "ALL");

		ssl = SSL_new(ctx);
		sk_cipher = SSL_get_ciphers(ssl);

		n = sk_SSL_CIPHER_num(sk_cipher);
		if (n > PJ_ARRAY_SIZE(openssl_ciphers))
			n = PJ_ARRAY_SIZE(openssl_ciphers);

		for (i = 0; i < n; ++i) {
			SSL_CIPHER *c;
			c = sk_SSL_CIPHER_value(sk_cipher,i);
			openssl_ciphers[i].id = (pj_ssl_cipher)
				(pj_uint32_t)c->id & 0x00FFFFFF;
			openssl_ciphers[i].name = SSL_CIPHER_get_name(c);
		}

		SSL_free(ssl);
		SSL_CTX_free(ctx);

		openssl_cipher_num = n;
	}

	/* Create OpenSSL application data index for SSL socket */
	sslsock_idx = SSL_get_ex_new_index(0, "DTLS socket", NULL, NULL, NULL);

	return PJ_SUCCESS;
}

/* Shutdown OpenSSL */
static void shutdown_openssl(void)
{
	PJ_UNUSED_ARG(openssl_init_count);
}

/* SSL password callback. */
static int password_cb(char *buf, int num, int rwflag, void *user_data)
{
    pjmedia_dtls_cert *cert = (pjmedia_dtls_cert*) user_data;

    PJ_UNUSED_ARG(rwflag);

    if(num < cert->privkey_pass.slen)
	return 0;
    
    pj_memcpy(buf, cert->privkey_pass.ptr, cert->privkey_pass.slen);
    return cert->privkey_pass.slen;
}


/* SSL password callback. */
static int verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    transport_dtls *dtls;
    SSL *ossl_ssl;
    int err;

    /* Get SSL instance */
    ossl_ssl = X509_STORE_CTX_get_ex_data(x509_ctx, 
				    SSL_get_ex_data_X509_STORE_CTX_idx());
    pj_assert(ossl_ssl);

    /* Get SSL socket instance */
    dtls = SSL_get_ex_data(ossl_ssl, sslsock_idx);
    pj_assert(dtls);

    /* Store verification status */
    err = X509_STORE_CTX_get_error(x509_ctx);
    switch (err) {
    case X509_V_OK:
	break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	dtls->verify_status |= PJ_SSL_CERT_EISSUER_NOT_FOUND;
	break;

    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
	dtls->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
	break;

    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_HAS_EXPIRED:
	dtls->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
	break;

    case X509_V_ERR_UNABLE_TO_GET_CRL:
    case X509_V_ERR_CRL_NOT_YET_VALID:
    case X509_V_ERR_CRL_HAS_EXPIRED:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
	dtls->verify_status |= PJ_SSL_CERT_ECRL_FAILURE;
	break;	

    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	dtls->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
	break;	

    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
    case X509_V_ERR_AKID_SKID_MISMATCH:
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
	dtls->verify_status |= PJ_SSL_CERT_EISSUER_MISMATCH;
	break;

    case X509_V_ERR_CERT_REVOKED:
	dtls->verify_status |= PJ_SSL_CERT_EREVOKED;
	break;	

    case X509_V_ERR_INVALID_PURPOSE:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_INVALID_CA:
	dtls->verify_status |= PJ_SSL_CERT_EINVALID_PURPOSE;
	break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG: /* not really used */
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
	dtls->verify_status |= PJ_SSL_CERT_ECHAIN_TOO_LONG;
	break;

    /* Unknown errors */
    case X509_V_ERR_OUT_OF_MEM:
    default:
	dtls->verify_status |= PJ_SSL_CERT_EUNKNOWN;
	break;
    }

    /* When verification is not requested just return ok here, however
     * application can still get the verification status.
     */

	if (PJ_FALSE == dtls->setting.verify_server)
	preverify_ok = 1;

    return preverify_ok;
}

static int generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
	transport_dtls *dtls = NULL;
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	char buf[PJ_INET6_ADDRSTRLEN+10];

	/* Initialize a random secret */
	if (!cookie_initialized)
	{
		if (!RAND_bytes(cookie_secret, COOKIE_SECRET_LENGTH))
		{
			printf("error setting random cookie secret\n");
			return 0;
		}
		cookie_initialized = 1;
	}

	// Get associated dtls instance, for getting remote address.
	dtls = SSL_get_ex_data(ssl, sslsock_idx);
	if (dtls == NULL)
		return 0;

	PJ_LOG(4, (THIS_FILE, "generate_cookie() rem_addr=[%s]", 
		pj_sockaddr_print((pj_sockaddr_t *)&dtls->rem_addr, buf, sizeof(buf), 3)));

	/* Create buffer with peer's address and port */
	length = 0;
	switch (dtls->rem_addr.addr.sa_family) {
		case AF_INET:
			length += sizeof(pj_sockaddr_in);
			break;
		case AF_INET6:
			length += sizeof(pj_sockaddr_in6);
			break;
		default:
			OPENSSL_assert(0);
			break;
	}
	length += sizeof(pj_uint16_t); // port
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if (buffer == NULL)
	{
		printf("out of memory\n");
		return 0;
	}

	switch (dtls->rem_addr.addr.sa_family) {
		case AF_INET:
			memcpy(buffer,
				&dtls->rem_addr.ipv4.sin_port,
				sizeof(pj_uint16_t)); 
			memcpy(buffer + sizeof(dtls->rem_addr.ipv4.sin_port),
				&dtls->rem_addr.ipv4.sin_addr,
				sizeof(pj_sockaddr_in));
			break;
		case AF_INET6:
			memcpy(buffer,
				&dtls->rem_addr.ipv6.sin6_port,
				sizeof(pj_uint16_t));
			memcpy(buffer + sizeof(pj_uint16_t),
				&dtls->rem_addr.ipv6.sin6_addr,
				sizeof(pj_sockaddr_in6));
			break;
		default:
			OPENSSL_assert(0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
		(const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	memcpy(cookie, result, resultlength);
	*cookie_len = resultlength;

	return 1;
}

static int verify_cookie(SSL *ssl, unsigned char *cookie, unsigned int cookie_len)
{
	transport_dtls *dtls = NULL;
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	char buf[PJ_INET6_ADDRSTRLEN+10];
	unsigned int length = 0, resultlength;

	/* If secret isn't initialized yet, the cookie can't be valid */
	if (!cookie_initialized)
		return 0;

	// Get associated dtls instance, for getting remote address.
	dtls = SSL_get_ex_data(ssl, sslsock_idx);
	if (dtls == NULL)
		return 0;
	
	PJ_LOG(4, (THIS_FILE, "verify_cookie() rem_addr=[%s]", 
		pj_sockaddr_print((pj_sockaddr_t *)&dtls->rem_addr, buf, sizeof(buf), 3)));

	/* Create buffer with peer's address and port */
	length = 0;
	switch (dtls->rem_addr.addr.sa_family) {
		case AF_INET:
			length += sizeof(pj_sockaddr_in);
			break;
		case AF_INET6:
			length += sizeof(pj_sockaddr_in6);
			break;
		default:
			OPENSSL_assert(0);
			break;
	}
	length += sizeof(pj_uint16_t); // port
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if (buffer == NULL)
	{
		printf("out of memory\n");
		return 0;
	}

	switch (dtls->rem_addr.addr.sa_family) {
		case AF_INET:
			memcpy(buffer,
				&dtls->rem_addr.ipv4.sin_port,
				sizeof(pj_uint16_t));
			memcpy(buffer + sizeof(pj_uint16_t),
				&dtls->rem_addr.ipv4.sin_addr,
				sizeof(pj_sockaddr_in));
			break;
		case AF_INET6:
			memcpy(buffer,
				&dtls->rem_addr.ipv6.sin6_port,
				sizeof(pj_uint16_t));
			memcpy(buffer + sizeof(pj_uint16_t),
				&dtls->rem_addr.ipv6.sin6_addr,
				sizeof(pj_sockaddr_in6));
			break;
		default:
			OPENSSL_assert(0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
		(const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	if (cookie_len == resultlength && memcmp(result, cookie, resultlength) == 0)
		return 1;

	return 0;
	//return 1;
}

/* Create and initialize new SSL context and instance */
static pj_status_t create_ssl(transport_dtls *dtls)
{
    SSL_METHOD *ssl_method;
    SSL_CTX *ctx;
    pjmedia_dtls_cert *cert;
    int mode, rc;
    pj_status_t status;
        
    pj_assert(dtls);

    cert = dtls->cert;

    /* Make sure OpenSSL library has been initialized */
    init_openssl();

    /* Determine SSL method to use */
    switch (PJ_SSL_SOCK_PROTO_DTLS1) {
    case PJ_SSL_SOCK_PROTO_DEFAULT:
    case PJ_SSL_SOCK_PROTO_TLS1:
	ssl_method = (SSL_METHOD*)TLSv1_method();
	break;
#ifndef OPENSSL_NO_SSL2
    case PJ_SSL_SOCK_PROTO_SSL2:
	ssl_method = (SSL_METHOD*)SSLv2_method();
	break;
#endif
    case PJ_SSL_SOCK_PROTO_SSL3:
	ssl_method = (SSL_METHOD*)SSLv3_method();
	break;
    case PJ_SSL_SOCK_PROTO_SSL23:
	ssl_method = (SSL_METHOD*)SSLv23_method();
	break;
    case PJ_SSL_SOCK_PROTO_DTLS1:
	ssl_method = (SSL_METHOD*)DTLSv1_method();
	break;
    default:
	return PJ_EINVAL;
    }

    /* Create SSL context */
    ctx = SSL_CTX_new(ssl_method);
    if (ctx == NULL) {
	return GET_SSL_STATUS(dtls);
    }

    /* Apply credentials */
    if (cert) {
		if (dtls->setting.verify_server && !cert->CA_file.slen && cert->cert_file.slen) {
			pj_strcpy(&cert->CA_file, &cert->cert_file);
		}

	/* Load CA list if one is specified. */
	if (cert->CA_file.slen) {

	    rc = SSL_CTX_load_verify_locations(ctx, cert->CA_file.ptr, NULL);

	    if (rc != 1) {
		status = GET_SSL_STATUS(dtls);
		PJ_LOG(1,(dtls->pool->obj_name, "Error loading CA list file "
			  "'%s'", cert->CA_file.ptr));
		SSL_CTX_free(ctx);
		return status;
	    }
	}
    
	/* Set password callback */
	if (cert->privkey_pass.slen) {
	    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
	    SSL_CTX_set_default_passwd_cb_userdata(ctx, cert);
	}


	/* Load certificate if one is specified */
	if (cert->cert_file.slen) {

	    /* Load certificate chain from file into ctx */
	    rc = SSL_CTX_use_certificate_chain_file(ctx, cert->cert_file.ptr);

	    if(rc != 1) {
		status = GET_SSL_STATUS(dtls);
		PJ_LOG(1,(dtls->pool->obj_name, "Error loading certificate "
			  "chain file '%s'", cert->cert_file.ptr));
		SSL_CTX_free(ctx);
		return status;
	    }
	}


	/* Load private key if one is specified */
	if (cert->privkey_file.slen) {
	    /* Adds the first private key found in file to ctx */
	    rc = SSL_CTX_use_PrivateKey_file(ctx, cert->privkey_file.ptr, 
					     SSL_FILETYPE_PEM);

	    if(rc != 1) {
		status = GET_SSL_STATUS(dtls);
		PJ_LOG(1,(dtls->pool->obj_name, "Error adding private key "
			  "from '%s'", cert->privkey_file.ptr));
		SSL_CTX_free(ctx);
		return status;
	    }
	}
    }

    dtls->ossl_ctx = ctx;

	// dean : For DTLS, if OpenSSL 1.0.1k or later version is used, read_ahead must be set to "ON". Otherwise, handshake always fails.
	// please check http://rt.openssl.org/Ticket/Display.html?id=3657&user=guest&pass=guest
	SSL_CTX_set_read_ahead(dtls->ossl_ctx, 1);

	/* Create SSL instance */
    dtls->ossl_ssl = SSL_new(dtls->ossl_ctx);
    if (dtls->ossl_ssl == NULL) {
	return GET_SSL_STATUS(dtls);
    }

    /* Set SSL sock as application data of SSL instance */
    SSL_set_ex_data(dtls->ossl_ssl, sslsock_idx, dtls);

    /* SSL verification options */
    mode = SSL_VERIFY_PEER;
    if (!dtls->offerer_side && dtls->setting.require_client_cert)
	mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

	SSL_set_verify(dtls->ossl_ssl, mode, &verify_cb);
	SSL_CTX_set_cookie_generate_cb(dtls->ossl_ctx, generate_cookie);
	SSL_CTX_set_cookie_verify_cb(dtls->ossl_ctx, verify_cookie);
	SSL_set_options(dtls->ossl_ssl, SSL_OP_COOKIE_EXCHANGE);

    /* Set cipher list */
    status = set_cipher_list(dtls);
    if (status != PJ_SUCCESS)
	return status;

    /* Setup SSL BIOs */
    dtls->ossl_rbio = BIO_new(BIO_s_mem());
	//BIO_set_mem_eof_return(dtls->ossl_rbio, -1);
	dtls->ossl_wbio = BIO_new(BIO_s_mem());
	//BIO_set_mem_eof_return(dtls->ossl_wbio, -1);
    BIO_set_close(dtls->ossl_rbio, BIO_CLOSE);
    BIO_set_close(dtls->ossl_wbio, BIO_CLOSE);
	SSL_set_bio(dtls->ossl_ssl, dtls->ossl_rbio, dtls->ossl_wbio);

    return PJ_SUCCESS;
}


/* Destroy SSL context and instance */
static void destroy_ssl(transport_dtls *dtls)
{
    /* Destroy SSL instance */
    if (dtls->ossl_ssl) {
	SSL_shutdown(dtls->ossl_ssl);
	SSL_free(dtls->ossl_ssl); /* this will also close BIOs */
	dtls->ossl_ssl = NULL;
    }

    /* Destroy SSL context */
    if (dtls->ossl_ctx) {
	SSL_CTX_free(dtls->ossl_ctx);
	dtls->ossl_ctx = NULL;
    }

    /* Potentially shutdown OpenSSL library if this is the last
     * context exists.
     */
    shutdown_openssl();
}
/*
 *******************************************************************
 * API
 *******************************************************************
 */

/* Load credentials from files. */
PJ_DEF(pj_status_t) pjmedia_dtls_cert_load_from_files (pj_pool_t *pool,
						 const pj_str_t *CA_file,
						 const pj_str_t *cert_file,
						 const pj_str_t *privkey_file,
						 const pj_str_t *privkey_pass,
						 pjmedia_dtls_cert **p_cert)
{
    pjmedia_dtls_cert *cert;

    PJ_ASSERT_RETURN(pool && CA_file && cert_file && privkey_file, PJ_EINVAL);

    cert = PJ_POOL_ZALLOC_T(pool, pjmedia_dtls_cert);
    pj_strdup_with_null(pool, &cert->CA_file, CA_file);
    pj_strdup_with_null(pool, &cert->cert_file, cert_file);
    pj_strdup_with_null(pool, &cert->privkey_file, privkey_file);
    pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return PJ_SUCCESS;
}


/* Set SSL socket credentials. */
PJ_DECL(pj_status_t) pjmedia_dtls_set_certificate(
					    transport_dtls *dtls,
					    pj_pool_t *pool,
					    const pjmedia_dtls_cert *cert)
{
    pjmedia_dtls_cert *cert_;

    PJ_ASSERT_RETURN(dtls && pool && cert, PJ_EINVAL);

    cert_ = PJ_POOL_ZALLOC_T(pool, pjmedia_dtls_cert);
    pj_memcpy(cert_, cert, sizeof(cert));
    pj_strdup_with_null(pool, &cert_->CA_file, &cert->CA_file);
    pj_strdup_with_null(pool, &cert_->cert_file, &cert->cert_file);
    pj_strdup_with_null(pool, &cert_->privkey_file, &cert->privkey_file);
    pj_strdup_with_null(pool, &cert_->privkey_pass, &cert->privkey_pass);

    dtls->cert = cert_;

    return PJ_SUCCESS;
}


/* Get available ciphers. */
PJ_DEF(pj_status_t) pjmedia_dtls_cipher_get_availables(pj_ssl_cipher ciphers[],
					         unsigned *cipher_num)
{
    unsigned i;

    PJ_ASSERT_RETURN(ciphers && cipher_num, PJ_EINVAL);

    if (openssl_cipher_num == 0) {
	init_openssl();
	shutdown_openssl();
    }

    if (openssl_cipher_num == 0) {
		*cipher_num = 0;
		PJ_LOG(4, (THIS_FILE, "pj_ssl_cipher_get_availables() openssl_cipher not found."));
		return PJ_ENOTFOUND;
    }

    *cipher_num = PJ_MIN(*cipher_num, openssl_cipher_num);

    for (i = 0; i < *cipher_num; ++i)
	ciphers[i] = openssl_ciphers[i].id;

    return PJ_SUCCESS;
}


/* Get cipher name string */
PJ_DEF(const char*) pjmedia_dtls_cipher_name(pj_ssl_cipher cipher)
{
    unsigned i;

    if (openssl_cipher_num == 0) {
	init_openssl();
	shutdown_openssl();
    }

    for (i = 0; i < openssl_cipher_num; ++i) {
	if (cipher == openssl_ciphers[i].id)
	    return openssl_ciphers[i].name;
    }

    return NULL;
}

/* Check if the specified cipher is supported by SSL/TLS backend. */
PJ_DEF(pj_bool_t) pjmedia_dtls_cipher_is_supported(pj_ssl_cipher cipher)
{
    unsigned i;

    if (openssl_cipher_num == 0) {
	init_openssl();
	shutdown_openssl();
    }

    for (i = 0; i < openssl_cipher_num; ++i) {
	if (cipher == openssl_ciphers[i].id)
	    return PJ_TRUE;
    }

    return PJ_FALSE;
}


/* Generate cipher list with user preference order in OpenSSL format */
static pj_status_t set_cipher_list(transport_dtls *dtls)
{
	char buf[1024];
	pj_str_t cipher_list;
	STACK_OF(SSL_CIPHER) *sk_cipher;
	unsigned i;
	int j, ret;

	if (dtls->setting.ciphers_num == 0)
		return PJ_SUCCESS;

	pj_strset(&cipher_list, buf, 0);

	/* Set SSL with ALL available ciphers */
	SSL_set_cipher_list(dtls->ossl_ssl, "ALL");

	/* Generate user specified cipher list in OpenSSL format */
	sk_cipher = SSL_get_ciphers(dtls->ossl_ssl);
	for (i = 0; i < dtls->setting.ciphers_num; ++i) {
		for (j = 0; j < sk_SSL_CIPHER_num(sk_cipher); ++j) {
			SSL_CIPHER *c;
			c = sk_SSL_CIPHER_value(sk_cipher, j);
			if (dtls->setting.ciphers[i] == (pj_ssl_cipher)
				((pj_uint32_t)c->id & 0x00FFFFFF))
			{
				const char *c_name;

				c_name = SSL_CIPHER_get_name(c);

				/* Check buffer size */
				if (cipher_list.slen + pj_ansi_strlen(c_name) + 2 > sizeof(buf)) {
					pj_assert(!"Insufficient temporary buffer for cipher");
					return PJ_ETOOMANY;
				}

				/* Add colon separator */
				if (cipher_list.slen)
					pj_strcat2(&cipher_list, ":");

				/* Add the cipher */
				pj_strcat2(&cipher_list, c_name);
				break;
			}
		}
	}

	/* Put NULL termination in the generated cipher list */
	cipher_list.ptr[cipher_list.slen] = '\0';

	/* Finally, set chosen cipher list */
	ret = SSL_set_cipher_list(dtls->ossl_ssl, buf);
	if (ret < 1) {
		return GET_SSL_STATUS(dtls);
	}

	return PJ_SUCCESS;
}


/* Parse OpenSSL ASN1_TIME to pj_time_val and GMT info */
static pj_bool_t parse_ossl_asn1_time(pj_time_val *tv, pj_bool_t *gmt,
				      const ASN1_TIME *tm)
{
    unsigned long parts[7] = {0};
    char *p, *end;
    unsigned len;
    pj_bool_t utc;
    pj_parsed_time pt;
    int i;

    utc = tm->type == V_ASN1_UTCTIME;
    p = (char*)tm->data;
    len = tm->length;
    end = p + len - 1;

    /* GMT */
    *gmt = (*end == 'Z');

    /* parse parts */
    for (i = 0; i < 7 && p < end; ++i) {
	pj_str_t st;

	if (i==0 && !utc) {
	    /* 4 digits year part for non-UTC time format */
	    st.slen = 4;
	} else if (i==6) {
	    /* fraction of seconds */
	    if (*p == '.') ++p;
	    st.slen = end - p + 1;
	} else {
	    /* other parts always 2 digits length */
	    st.slen = 2;
	}
	st.ptr = p;

	parts[i] = pj_strtoul(&st);
	p += st.slen;
    }

    /* encode parts to pj_time_val */
    pt.year = parts[0];
    if (utc)
	pt.year += (pt.year < 50)? 2000:1900;
    pt.mon = parts[1] - 1;
    pt.day = parts[2];
    pt.hour = parts[3];
    pt.min = parts[4];
    pt.sec = parts[5];
    pt.msec = parts[6];

    pj_time_encode(&pt, tv);

    return PJ_TRUE;
}


/* Get Common Name field string from a general name string */
static void get_cn_from_gen_name(const pj_str_t *gen_name, pj_str_t *cn)
{
    pj_str_t CN_sign = {"/CN=", 4};
    char *p, *q;

    pj_bzero(cn, sizeof(cn));

    p = pj_strstr(gen_name, &CN_sign);
    if (!p)
	return;

    p += 4; /* shift pointer to value part */
    pj_strset(cn, p, gen_name->slen - (p - gen_name->ptr));
    q = pj_strchr(cn, '/');
    if (q)
	cn->slen = q - p;
}


/* Get certificate info from OpenSSL X509, in case the certificate info
 * hal already populated, this function will check if the contents need 
 * to be updated by inspecting the issuer and the serial number.
 */
static void get_cert_info(pj_pool_t *pool, pj_ssl_cert_info *ci, X509 *x)
{
    pj_bool_t update_needed;
    char buf[512];
    pj_uint8_t serial_no[64] = {0}; /* should be >= sizeof(ci->serial_no) */
    pj_uint8_t *p;
    unsigned len;
    GENERAL_NAMES *names = NULL;

    pj_assert(pool && ci && x);

    /* Get issuer */
    X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof(buf));

    /* Get serial no */
    p = (pj_uint8_t*) M_ASN1_STRING_data(X509_get_serialNumber(x));
    len = M_ASN1_STRING_length(X509_get_serialNumber(x));
    if (len > sizeof(ci->serial_no)) 
	len = sizeof(ci->serial_no);
    pj_memcpy(serial_no + sizeof(ci->serial_no) - len, p, len);

    /* Check if the contents need to be updated. */
    update_needed = pj_strcmp2(&ci->issuer.info, buf) || 
	            pj_memcmp(ci->serial_no, serial_no, sizeof(ci->serial_no));
    if (!update_needed)
	return;

    /* Update cert info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
    ci->version = X509_get_version(x) + 1;

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    get_cn_from_gen_name(&ci->issuer.info, &ci->issuer.cn);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, sizeof(ci->serial_no));

    /* Subject */
    pj_strdup2(pool, &ci->subject.info, 
	       X509_NAME_oneline(X509_get_subject_name(x),
				 buf, sizeof(buf)));
    get_cn_from_gen_name(&ci->subject.info, &ci->subject.cn);

    /* Validity */
    parse_ossl_asn1_time(&ci->validity.start, &ci->validity.gmt,
			 X509_get_notBefore(x));
    parse_ossl_asn1_time(&ci->validity.end, &ci->validity.gmt,
			 X509_get_notAfter(x));

    /* Subject Alternative Name extension */
    if (ci->version >= 3) {
	names = (GENERAL_NAMES*) X509_get_ext_d2i(x, NID_subject_alt_name,
						  NULL, NULL);
    }
    if (names) {
        unsigned i, cnt;

        cnt = sk_GENERAL_NAME_num(names);
	ci->subj_alt_name.entry = pj_pool_calloc(pool, cnt, 
					    sizeof(*ci->subj_alt_name.entry));

        for (i = 0; i < cnt; ++i) {
	    unsigned char *p = 0;
	    pj_ssl_cert_name_type type = PJ_SSL_CERT_NAME_UNKNOWN;
            const GENERAL_NAME *name;
	    
	    name = sk_GENERAL_NAME_value(names, i);

            switch (name->type) {
                case GEN_EMAIL:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_RFC822;
                    break;
                case GEN_DNS:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_DNS;
                    break;
                case GEN_URI:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_URI;
                    break;
                case GEN_IPADD:
		    p = ASN1_STRING_data(name->d.ip);
		    len = ASN1_STRING_length(name->d.ip);
		    type = PJ_SSL_CERT_NAME_IP;
                    break;
		default:
		    break;
            }

	    if (p && len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
		ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
		if (type == PJ_SSL_CERT_NAME_IP) {
		    int af = pj_AF_INET();
		    if (len == sizeof(pj_in6_addr)) af = pj_AF_INET6();
		    pj_inet_ntop2(af, p, buf, sizeof(buf));
		    pj_strdup2(pool, 
		          &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		          buf);
		} else {
		    pj_strdup2(pool, 
			  &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name, 
			  (char*)p);
		    OPENSSL_free(p);
		}
		ci->subj_alt_name.cnt++;
	    }
        }
    }
}


/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed.
 */
static void update_certs_info(transport_dtls *dtls)
{
    X509 *x;

    pj_assert(dtls->ssl_state == SSL_STATE_ESTABLISHED);

    /* Active local certificate */
    x = SSL_get_certificate(dtls->ossl_ssl);
    if (x) {
	get_cert_info(dtls->pool, &dtls->local_cert_info, x);
	/* Don't free local's X509! */
    } else {
	pj_bzero(&dtls->local_cert_info, sizeof(pj_ssl_cert_info));
    }

    /* Active remote certificate */
    x = SSL_get_peer_certificate(dtls->ossl_ssl);
    if (x) {
	get_cert_info(dtls->pool, &dtls->remote_cert_info, x);
	/* Free peer's X509 */
	X509_free(x);
    } else {
	pj_bzero(&dtls->remote_cert_info, sizeof(pj_ssl_cert_info));
    }
}

static void dump_bin(const char *buf, unsigned len)
{
	unsigned i;
	if (len > 64)
		len = 64;
	PJ_LOG(3,(THIS_FILE, "begin dump"));
	for (i=0; i<len; ++i) {
		int j;
		char bits[9];
		unsigned val = buf[i] & 0xFF;

		bits[8] = '\0';
		for (j=0; j<8; ++j) {
			if (val & (1 << (7-j)))
				bits[j] = '1';
			else
				bits[j] = '0';
		}

		PJ_LOG(3,(THIS_FILE, "%2d %s [%d]", i, bits, val));
	}
	PJ_LOG(3,(THIS_FILE, "end dump"));
}


/* Flush write BIO to network socket. Note that any access to write BIO
 * MUST be serialized, so mutex protection must cover any call to OpenSSL
 * API (that possibly generate data for write BIO) along with the call to
 * this function (flushing all data in write BIO generated by above 
 * OpenSSL API call).
 */
static pj_status_t flush_write_bio(transport_dtls *dtls, 
				   pj_size_t orig_len,
				   unsigned flags)
{
	pj_status_t status = PJ_SUCCESS;

	pj_lock_acquire(dtls->mutex);

	//PJ_LOG(1, (THIS_FILE, "flush_write_bio() orig_len=%d", orig_len));

    /* Check if there is data in write BIO, flush it if any */
    if (!BIO_pending(dtls->ossl_wbio)) {
	pj_lock_release(dtls->mutex);
	return PJ_SUCCESS;
    }

	do {
    /* Get data and its length */
    //len = BIO_get_mem_data(dtls->ossl_wbio, &data);
	dtls->delayed_send_size = BIO_read(dtls->ossl_wbio, dtls->dtls_snd_buffer, sizeof(dtls->dtls_snd_buffer));
    if (dtls->delayed_send_size == 0) {
	pj_lock_release(dtls->mutex);
	return PJ_SUCCESS;
	}

	// Active a handshake retransmission timer.
	if (dtls->ssl_state == SSL_STATE_HANDSHAKING) {
		pj_time_val delay = {0, HANDSHAKE_RETRANS_TIMEOUT};

		if (dtls->timer.id > 0) {
			pj_timer_heap_cancel(dtls->timer_heap, &dtls->timer);
			dtls->timer.id = TIMER_NONE;
		}

		dtls->timer.id = TIMER_HANDSHAKE_RETRANSMISSION;
		status = pj_timer_heap_schedule(dtls->timer_heap, 
			&dtls->timer,
			&delay);
		if (status != PJ_SUCCESS)
			dtls->timer.id = TIMER_NONE;
		else
			PJ_LOG(3, (THIS_FILE, "flush_write_bio() TIMER_HANDSHAKE_RETRANSMISSION scheduled."));
	}
	
	//dump_bin(data, len);

    /* Ticket #1573: Don't hold mutex while calling PJLIB socket send(). */
    pj_lock_release(dtls->mutex);

	((pj_uint8_t *)dtls->dtls_snd_buffer)[dtls->delayed_send_size] = flags;

	status = pjmedia_transport_send_rtp(dtls->member_tp, dtls->dtls_snd_buffer, dtls->delayed_send_size);

	//PJ_LOG(1, (THIS_FILE, "flush_write_bio() Send encrtyped data=%d, status=%d", dtls->delayed_send_size, status));

	pj_lock_acquire(dtls->mutex);
#if 0
	if (status == PJNATH_EICEINPROGRESS) {
		dtls->delayed_send_active = PJ_TRUE;
		pj_lock_release(dtls->mutex);
		return PJ_EPENDING;
	}
#endif
	} while (BIO_pending(dtls->ossl_wbio));

	pj_lock_release(dtls->mutex);

	return status;
}

/* Write plain data to SSL and flush write BIO. */
static pj_status_t ssl_write(transport_dtls *dtls, 
			     const void *data,
			     pj_ssize_t size,
			     unsigned flags)
{
    pj_status_t status;
	int nwritten;


	// DEAN, check if this is tunnel data. If true, update last_data time.
	unsigned tnl_data = ((pj_uint8_t *)data)[size];

    /* Write the plain data to SSL, after SSL encrypts it, write BIO will
     * contain the secured data to be sent via socket. Note that re-
     * negotitation may be on progress, so sending data should be delayed
     * until re-negotiation is completed.
     */
    pj_lock_acquire(dtls->mutex);
    nwritten = SSL_write(dtls->ossl_ssl, data, size);
    pj_lock_release(dtls->mutex);
    
    if (nwritten == size) {
	/* All data written, flush write BIO to network socket */
		status = flush_write_bio(dtls, size, tnl_data);
    } else if (nwritten <= 0) {
	/* SSL failed to process the data, it may just that re-negotiation
	 * is on progress.
	 */
	int err;
	err = SSL_get_error(dtls->ossl_ssl, nwritten);
	if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_NONE) {
	    /* Re-negotiation is on progress, flush re-negotiation data */
	    status = flush_write_bio(dtls, size, tnl_data);
	    if (status == PJ_SUCCESS || status == PJ_EPENDING)
		/* Just return PJ_EBUSY when re-negotiation is on progress */
		status = PJ_EBUSY;
	} else {
	    /* Some problem occured */
	    status = STATUS_FROM_SSL_ERR(dtls, err);
	}
    } else {
	/* nwritten < *size, shouldn't happen, unless write BIO cannot hold 
	 * the whole secured data, perhaps because of insufficient memory.
	 */
	status = PJ_ENOMEM;
    }

    return status;
}

/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return PJ_FALSE when SSL socket instance is destroyed by application.
 */
static pj_bool_t on_handshake_complete(transport_dtls *dtls, 
				       pj_status_t status)
{
    /* Cancel handshake timer */
    if (dtls->timer.id == TIMER_HANDSHAKE_RETRANSMISSION ||
		dtls->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
	pj_timer_heap_cancel(dtls->timer_heap, &dtls->timer);
	dtls->timer.id = TIMER_NONE;
    }

    /* Update certificates info on successful handshake */
    if (status == PJ_SUCCESS)
	update_certs_info(dtls);

	dtls->ssl_state = SSL_STATE_ESTABLISHED;
	pj_sem_post(dtls->handshake_sem);

    /* Accepting */
    if (!dtls->offerer_side) { //is_server side
		if (status != PJ_SUCCESS) {
			/* Handshake failed in accepting, destroy our self silently. */

	#if 0
			char errmsg[PJ_ERR_MSG_SIZE];
			char buf[PJ_INET6_ADDRSTRLEN+10];
			pj_strerror(status, errmsg, sizeof(errmsg));
			PJ_LOG(3,(dtls->pool->obj_name, "Handshake failed in accepting "
				  "%s: %s",
				  pj_sockaddr_print(&dtls->rem_addr, buf, sizeof(buf), 3),
				  errmsg));
	#endif
			/* Workaround for ticket #985 */
	#if defined(PJ_WIN32) && PJ_WIN32!=0
			if (dtls->timer_heap) {
			pj_time_val interval = {0, DELAYED_CLOSE_TIMEOUT};

			dtls->timer.id = TIMER_CLOSE;
			pj_time_val_normalize(&interval);
			if (pj_timer_heap_schedule(dtls->timer_heap, 
						   &dtls->timer, &interval) != 0)
			{
				dtls->timer.id = TIMER_NONE;
				pjmedia_transport_dtls_stop((pjmedia_transport *)dtls);
			}
			} else 
	#endif	/* PJ_WIN32 */
			{
				dtls->ssl_state = SSL_STATE_NULL;
				pjmedia_transport_dtls_stop((pjmedia_transport *)dtls);
			}
			// Call callback
			if (dtls->setting.cb.on_dtls_handshake_complete) {
				(*dtls->setting.cb.on_dtls_handshake_complete)((pjmedia_transport *)dtls->member_tp, 
					status, dtls->turn_mapped_addr);
			}
			return PJ_FALSE;
		}
    }

    /* Connecting */
    else {
		/* On failure, reset SSL socket state first, as app may try to 
		 * reconnect in the callback.
		 */
		if (status != PJ_SUCCESS) {
			/* Server disconnected us, possibly due to SSL nego failure */
			if (status == PJ_EEOF) {
			unsigned long err;
			err = ERR_get_error();
			if (err != SSL_ERROR_NONE)
				status = STATUS_FROM_SSL_ERR(dtls, err);
			}
			dtls->ssl_state = SSL_STATE_NULL;
			pjmedia_transport_dtls_stop((pjmedia_transport *)dtls);
		}
	}

	// Call callback
	if (dtls->setting.cb.on_dtls_handshake_complete) {
		(*dtls->setting.cb.on_dtls_handshake_complete)((pjmedia_transport *)dtls->member_tp, 
			status, dtls->turn_mapped_addr);
	}

    return PJ_TRUE;
}

#if ENABLE_DELAYED_SEND
static write_data_t* alloc_send_data(transport_dtls *dtls, pj_size_t len)
{
    send_buf_t *send_buf = &dtls->send_buf;
    pj_size_t avail_len, skipped_len = 0;
    char *reg1, *reg2;
    pj_size_t reg1_len, reg2_len;
    write_data_t *p;

    /* Check buffer availability */
    avail_len = send_buf->max_len - send_buf->len;
    if (avail_len < len)
	return NULL;

    /* If buffer empty, reset start pointer and return it */
    if (send_buf->len == 0) {
	send_buf->start = send_buf->buf;
	send_buf->len   = len;
	p = (write_data_t*)send_buf->start;
	goto init_send_data;
    }

    /* Free space may be wrapped/splitted into two regions, so let's
     * analyze them if any region can hold the write data.
     */
    reg1 = send_buf->start + send_buf->len;
    if (reg1 >= send_buf->buf + send_buf->max_len)
	reg1 -= send_buf->max_len;
    reg1_len = send_buf->max_len - send_buf->len;
    if (reg1 + reg1_len > send_buf->buf + send_buf->max_len) {
	reg1_len = send_buf->buf + send_buf->max_len - reg1;
	reg2 = send_buf->buf;
	reg2_len = send_buf->start - send_buf->buf;
    } else {
	reg2 = NULL;
	reg2_len = 0;
    }

    /* More buffer availability check, note that the write data must be in
     * a contigue buffer.
     */
    avail_len = PJ_MAX(reg1_len, reg2_len);
    if (avail_len < len)
	return NULL;

    /* Get the data slot */
    if (reg1_len >= len) {
	p = (write_data_t*)reg1;
    } else {
	p = (write_data_t*)reg2;
	skipped_len = reg1_len;
    }

    /* Update buffer length */
    send_buf->len += len + skipped_len;

init_send_data:
    /* Init the new send data */
    pj_bzero(p, sizeof(*p));
    pj_list_init(p);
    pj_list_push_back(&dtls->send_pending, p);

    return p;
}

static void free_send_data(transport_dtls *dtls, write_data_t *wdata)
{
    send_buf_t *buf = &dtls->send_buf;
    write_data_t *spl = &dtls->send_pending;

    pj_assert(!pj_list_empty(&dtls->send_pending));
    
    /* Free slot from the buffer */
    if (spl->next == wdata && spl->prev == wdata) {
	/* This is the only data, reset the buffer */
	buf->start = buf->buf;
	buf->len = 0;
    } else if (spl->next == wdata) {
	/* This is the first data, shift start pointer of the buffer and
	 * adjust the buffer length.
	 */
	buf->start = (char*)wdata->next;
	if (wdata->next > wdata) {
	    buf->len -= ((char*)wdata->next - buf->start);
	} else {
	    /* Overlapped */
	    unsigned right_len, left_len;
	    right_len = buf->buf + buf->max_len - (char*)wdata;
	    left_len  = (char*)wdata->next - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    } else if (spl->prev == wdata) {
	/* This is the last data, just adjust the buffer length */
	if (wdata->prev < wdata) {
	    unsigned jump_len;
	    jump_len = (char*)wdata -
		       ((char*)wdata->prev + wdata->prev->record_len);
	    buf->len -= (wdata->record_len + jump_len);
	} else {
	    /* Overlapped */
	    unsigned right_len, left_len;
	    right_len = buf->buf + buf->max_len -
			((char*)wdata->prev + wdata->prev->record_len);
	    left_len  = (char*)wdata + wdata->record_len - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    }
    /* For data in the middle buffer, just do nothing on the buffer. The slot
     * will be freed later when freeing the first/last data.
     */
    
    /* Remove the data from send pending list */
    pj_list_erase(wdata);
}

/* Flush delayed data sending in the write pending list. */
static pj_status_t flush_delayed_send(transport_dtls *dtls)
{
	/* Check for another ongoing flush */
	if (dtls->flushing_write_pend)
		return PJ_EBUSY;

	pj_lock_acquire(dtls->mutex);

	/* Again, check for another ongoing flush */
	if (dtls->flushing_write_pend) {
		pj_lock_release(dtls->mutex);
		return PJ_EBUSY;
	}

	/* Set ongoing flush flag */
	dtls->flushing_write_pend = PJ_TRUE;

	while (!pj_list_empty(&dtls->write_pending)) {
		write_data_t *wp;
		pj_status_t status;

		wp = dtls->write_pending.next;

		/* Ticket #1573: Don't hold mutex while calling socket send. */
		pj_lock_release(dtls->mutex);

		status = ssl_write(dtls, wp->data.ptr, 
			wp->plain_data_len, wp->flags);
		if (status != PJ_SUCCESS) {
			/* Reset ongoing flush flag first. */
			dtls->flushing_write_pend = PJ_FALSE;
			return status;
		}

		pj_lock_acquire(dtls->mutex);
		pj_list_erase(wp);
		pj_list_push_back(&dtls->write_pending_empty, wp);
	}

	/* Reset ongoing flush flag */
	dtls->flushing_write_pend = PJ_FALSE;

	pj_lock_release(dtls->mutex);

	return PJ_SUCCESS;
}

/* Sending is delayed, push back the sending data into pending list. */
static pj_status_t delay_send (transport_dtls *dtls,
							   const void *data,
							   pj_ssize_t size,
							   unsigned flags)
{
	write_data_t *wp;

	pj_lock_acquire(dtls->mutex);

	/* Init write pending instance */
	if (!pj_list_empty(&dtls->write_pending_empty)) {
		wp = dtls->write_pending_empty.next;
		pj_list_erase(wp);
	} else {
		wp = PJ_POOL_ZALLOC_T(dtls->pool, write_data_t);
	}

	wp->plain_data_len = size;
	wp->data.ptr = data;
	wp->flags = flags;

	pj_list_push_back(&dtls->write_pending, wp);

	pj_lock_release(dtls->mutex);

	/* Must return PJ_EPENDING */
	return PJ_EPENDING;
}
#endif

static void on_timer(pj_timer_heap_t *th, struct pj_timer_entry *te)
{
	transport_dtls *dtls = (transport_dtls*)te->user_data;
	int timer_id = te->id;
	pj_status_t status;
	pj_time_val delayed = {0, DELAYED_CLOSE_TIMEOUT};

	te->id = TIMER_NONE;

	PJ_UNUSED_ARG(th);

	switch (timer_id) {
	case TIMER_HANDSHAKE_RETRANSMISSION:
		if (dtls->ssl_state == SSL_STATE_HANDSHAKING &&
			dtls->delayed_send_size) {
				pj_time_val delay = {0, HANDSHAKE_RETRANS_TIMEOUT};

				if (dtls->timer.id > 0) {
					pj_timer_heap_cancel(dtls->timer_heap, &dtls->timer);
					dtls->timer.id = TIMER_NONE;
				}

				if (dtls->handshake_retrans_cnt >= 7) {
					dtls->handshake_retrans_cnt = 0;
					PJ_LOG(1, (THIS_FILE, "on_timer() TIMER_HANDSHAKE_RETRANSMISSION exceeds 7 times. Give up!!"));
				} else {
					dtls->timer.id = TIMER_HANDSHAKE_RETRANSMISSION;
					delay.msec <<= dtls->handshake_retrans_cnt;
					status = pj_timer_heap_schedule(dtls->timer_heap, 
						&dtls->timer,
						&delay);
					if (status != PJ_SUCCESS)
						dtls->timer.id = TIMER_NONE;
					else
						PJ_LOG(3, (THIS_FILE, "on_timer() TIMER_HANDSHAKE_RETRANSMISSION scheduled."));

					// Send handshake data out.
					pjmedia_transport_send_rtp(dtls->member_tp, dtls->dtls_snd_buffer, dtls->delayed_send_size);

					dtls->handshake_retrans_cnt++;
					PJ_LOG(1, (THIS_FILE, "on_timer() TIMER_HANDSHAKE_RETRANSMISSION sent."));
				}
		}

		break;
	case TIMER_HANDSHAKE_TIMEOUT:
		PJ_LOG(1,(dtls->pool->obj_name, "DTLS timeout after %d.%ds",
			dtls->setting.timeout.sec, dtls->setting.timeout.msec));

		on_handshake_complete(dtls, PJ_ETIMEDOUT);
		break;
	case TIMER_CLOSE:
		pjmedia_transport_dtls_stop((pjmedia_transport *)dtls);
		break;
	default:
		pj_assert(!"Unknown timer");
		break;
	}
}

/* Asynchronouse handshake */
static pj_status_t do_handshake(transport_dtls *dtls)
{
    pj_status_t status;
    int err;

    /* Perform SSL handshake */
    pj_lock_acquire(dtls->mutex);
    err = SSL_do_handshake(dtls->ossl_ssl);
	pj_lock_release(dtls->mutex);

    /* SSL_do_handshake() may put some pending data into SSL write BIO, 
     * flush it if any.
     */
    status = flush_write_bio(dtls, 0, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	return status;
    }

    if (err < 0) {
	err = SSL_get_error(dtls->ossl_ssl, err);
	if (err != SSL_ERROR_NONE && err != SSL_ERROR_WANT_READ) 
	{
		char buf[1024];
		pj_str_t err_str;
	    /* Handshake fails */
	    status = STATUS_FROM_SSL_ERR(dtls, err);
		err_str = ssl_strerror(status, buf, sizeof(buf));
	    return status;
	}
    }

    /* Check if handshake has been completed */
    if (SSL_is_init_finished(dtls->ossl_ssl)) {
	dtls->ssl_state = SSL_STATE_ESTABLISHED;
	return PJ_SUCCESS;
    }

    return PJ_EPENDING;
}

PJ_DEF(pj_status_t) pjmedia_dtls_do_handshake(pjmedia_transport *tp,
											  pj_sockaddr *turn_mapped_addr)
{
	transport_dtls *dtls = (transport_dtls *)tp;
	pj_status_t status;
	pj_bool_t ret;

	dtls->turn_mapped_addr = turn_mapped_addr;

	if (dtls->bypass_dtls) {
		status = PJ_SUCCESS;
		// Call callback
		if (dtls->setting.cb.on_dtls_handshake_complete) {
			(*dtls->setting.cb.on_dtls_handshake_complete)((pjmedia_transport *)dtls->member_tp, 
				status, dtls->turn_mapped_addr);
		}
	} else {
		// As server side
		if (!dtls->offerer_side) {
				if (dtls->ssl_state == SSL_STATE_NULL) {
					SSL_set_accept_state(dtls->ossl_ssl);  // tls server side.
					PJ_LOG(4, (THIS_FILE, "pjmedia_dtls_do_handshake SSL_set_accept_state."));
					dtls->ssl_state = SSL_STATE_HANDSHAKING;
				}
			return PJ_SUCCESS;
		}

		if (dtls->ssl_state == SSL_STATE_NULL) {
			SSL_set_connect_state(dtls->ossl_ssl);  // tls client side.
			PJ_LOG(4, (THIS_FILE, "pjmedia_dtls_do_handshake SSL_set_connect_state."));
			dtls->ssl_state = SSL_STATE_HANDSHAKING;
		}

		// DO Handshake first
		status = do_handshake(dtls);

		/* Not pending is either success or failed */
		if (status != PJ_EPENDING)
			ret = on_handshake_complete(dtls, status);
	}

	return status;
}

static pj_bool_t libdtls_initialized;
static void pjmedia_dtls_deinit_lib(pjmedia_endpt *endpt);

PJ_DEF(pj_status_t) pjmedia_dtls_init_lib(pjmedia_endpt *endpt)
{
    if (libdtls_initialized == PJ_FALSE) {
	pj_status_t status;

	status = init_openssl();
	if (status != PJ_SUCCESS) { 
	    PJ_LOG(4, (THIS_FILE, "Failed to initialize libdtls: %d", 
		       status));
	    return status;
	}

	if (pjmedia_endpt_atexit(endpt, pjmedia_dtls_deinit_lib) != PJ_SUCCESS)
	{
	    /* There will be memory leak when it fails to schedule libdtls 
	     * deinitialization, however the memory leak could be harmless,
	     * since in modern OS's memory used by an application is released 
	     * when the application terminates.
	     */
	    PJ_LOG(4, (THIS_FILE, "Failed to register libdtls deinit."));
	}

	libdtls_initialized = PJ_TRUE;
    }
    
    return PJ_SUCCESS;
}

static void pjmedia_dtls_deinit_lib(pjmedia_endpt *endpt)
{
    /* Note that currently this SRTP init/deinit is not equipped with
     * reference counter, it should be safe as normally there is only
     * one single instance of media endpoint and even if it isn't, the
     * pjmedia_transport_dtls_create() will invoke DTLS init (the only
     * drawback should be the delay described by #788).
     */

    PJ_UNUSED_ARG(endpt);

    shutdown_openssl();

    libdtls_initialized = PJ_FALSE;
}


PJ_DEF(void) pjmedia_dtls_setting_default(pjmedia_dtls_setting *opt)
{
	pj_assert(opt);

	pj_bzero(opt, sizeof(pjmedia_dtls_setting));
	opt->close_member_tp = PJ_TRUE;
	opt->use = PJMEDIA_DTLS_DISABLED;

	opt->read_buffer_size = TCP_OR_UDP_TOTAL_HEADER_SIZE+NATNL_PKT_MAX_LEN;
	opt->send_buffer_size = NATNL_DTLS_MAX_SEND_LEN;
	opt->verify_server = PJ_FALSE;
	opt->verify_client = PJ_FALSE;
	opt->verify_peer = PJ_FALSE;
	opt->timeout.sec = 3;
	opt->require_client_cert = PJ_FALSE;
}


/*
 * Create an SRTP media transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_dtls_create(
				       pjmedia_endpt *endpt,
				       pjmedia_transport *tp,
				       const pjmedia_dtls_setting *opt,
				       pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
	transport_dtls *dtls;
	pj_timer_heap_t *timer = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tp && p_tp, PJ_EINVAL);

    /* Init libdtls. */
    status = pjmedia_dtls_init_lib(endpt);
    if (status != PJ_SUCCESS)
	return status;

    pool = pjmedia_endpt_create_pool(endpt, "dtls%p", 1000, 1000);
    dtls = PJ_POOL_ZALLOC_T(pool, transport_dtls);

    dtls->pool = pool;
    dtls->session_inited = PJ_FALSE;
    dtls->bypass_dtls = PJ_FALSE;

	if (opt) {
		dtls->setting = *opt;
	} else {
		pjmedia_dtls_setting_default(&dtls->setting);
	}

	status = pj_lock_create_recursive_mutex(pool, pool->obj_name, &dtls->mutex);
	if (status != PJ_SUCCESS) {
		pj_pool_release(pool);
		return status;
	}

	/* Create semaphore */
	status = pj_sem_create(pool, "dtls_sem", 0, 65535, &dtls->handshake_sem);
	if (status != PJ_SUCCESS) {
		pj_pool_release(pool);
		return status;
	}

    /* Initialize base pjmedia_transport */
    pj_memcpy(dtls->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
    if (tp)
	dtls->base.type = tp->type;
    else
	dtls->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;
	dtls->base.op = &transport_dtls_op;

	pj_list_init(&dtls->write_pending);
	pj_list_init(&dtls->write_pending_empty);
	pj_list_init(&dtls->send_pending);
	pj_timer_entry_init(&dtls->timer, 0, dtls, &on_timer);

	status = pjmedia_dtls_set_certificate(dtls, dtls->pool, 
		&opt->cert);
	if (status != PJ_SUCCESS)
		return status;

	/* Init secure socket param */
	if (opt->ciphers_num > 0) {
		unsigned i;
		dtls->setting.ciphers = (pj_ssl_cipher*)
			pj_pool_calloc(pool, opt->ciphers_num, 
			sizeof(pj_ssl_cipher));
		for (i = 0; i < opt->ciphers_num; ++i)
			dtls->setting.ciphers[i] = opt->ciphers[i];
	}

	/* Prepare write/send state */
	pj_assert(dtls->send_buf.max_len == 0);
	dtls->send_buf.buf = (char*)
		pj_pool_alloc(dtls->pool, 
		dtls->setting.send_buffer_size);
	dtls->send_buf.max_len = dtls->setting.send_buffer_size;
	dtls->send_buf.start = dtls->send_buf.buf;
	dtls->send_buf.len = 0;
	dtls->read_size = dtls->setting.send_buffer_size;
	dtls->handshake_retrans_cnt = 0;

	if (!opt->timer_heap) {
		// Timer heap
		status = pj_timer_heap_create(pool, 4, &timer);
		if (status != PJ_SUCCESS) {
			return status;
		}
	} else
		timer = opt->timer_heap;

	dtls->timer_heap = timer;

	/* Create SSL context */
	status = create_ssl(dtls);
	if (status != PJ_SUCCESS)
		return status;

    /* Set underlying transport */
    dtls->member_tp = tp;

    /* Initialize peer's SRTP usage mode. */
    dtls->peer_use = dtls->setting.use;

    /* Done */
    *p_tp = &dtls->base;

    return PJ_SUCCESS;
}


/*
 * Initialize and start DTLS session with the given parameters.
 */
PJ_DEF(pj_status_t) pjmedia_transport_dtls_start(
			   pjmedia_transport *tp)
{
    transport_dtls  *dtls = (transport_dtls*) tp;
    pj_status_t	     status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    pj_lock_acquire(dtls->mutex);

    if (dtls->session_inited) {
	pjmedia_transport_dtls_stop(tp);
	}

	if (!dtls->bypass_dtls) {
		if (!dtls->offerer_side) {
				if (dtls->ssl_state == SSL_STATE_NULL) {
					SSL_set_accept_state(dtls->ossl_ssl);   // tls server side.

					PJ_LOG(4, (THIS_FILE, "pjmedia_transport_dtls_start SSL_set_accept_state."));
					// Try to do handshaking.
					dtls->ssl_state = SSL_STATE_HANDSHAKING;
				}
		} else {
			if (dtls->ssl_state == SSL_STATE_NULL) {
				SSL_set_connect_state(dtls->ossl_ssl);  // tls client side.

				PJ_LOG(4, (THIS_FILE, "pjmedia_transport_dtls_start SSL_set_connect_state."));
				// Try to do handshaking.
				dtls->ssl_state = SSL_STATE_HANDSHAKING;
			}
		}

	}

	/* Declare DTLS session initialized */
	dtls->session_inited = PJ_TRUE;

    pj_lock_release(dtls->mutex);
    return status;
}

/*
 * Stop SRTP session.
 */
PJ_DEF(pj_status_t) pjmedia_transport_dtls_stop(pjmedia_transport *dtls)
{
    transport_dtls *p_dtls = (transport_dtls*) dtls;
    //pj_status_t status;

    PJ_ASSERT_RETURN(dtls, PJ_EINVAL);

	pj_lock_acquire(p_dtls->mutex);

    if (!p_dtls->session_inited) {
	pj_lock_release(p_dtls->mutex);
	return PJ_SUCCESS;
    }

	destroy_ssl(p_dtls);

    p_dtls->session_inited = PJ_FALSE;

    pj_lock_release(p_dtls->mutex);

    return PJ_SUCCESS;
}

PJ_DEF(pjmedia_transport *) pjmedia_transport_dtls_get_member(
						pjmedia_transport *tp)
{
    transport_dtls *dtls = (transport_dtls*) tp;

    if(!tp)
		return NULL;

    return dtls->member_tp;
}


static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    transport_dtls *dtls = (transport_dtls*) tp;
    pjmedia_dtls_info dtls_info;
    int spc_info_idx;

    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);
    PJ_ASSERT_RETURN(info->specific_info_cnt <
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT, PJ_ETOOMANY);
    PJ_ASSERT_RETURN(sizeof(pjmedia_dtls_info) <=
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE, PJ_ENOMEM);

    dtls_info.active = dtls->session_inited;
    dtls_info.use = dtls->setting.use;
    dtls_info.peer_use = dtls->peer_use;

    spc_info_idx = info->specific_info_cnt++;
    info->spc_info[spc_info_idx].type = PJMEDIA_TRANSPORT_TYPE_DTLS;
    info->spc_info[spc_info_idx].cbsize = sizeof(dtls_info);
    pj_memcpy(&info->spc_info[spc_info_idx].buffer, &dtls_info, 
	      sizeof(dtls_info));

    return pjmedia_transport_get_info(dtls->member_tp, info);
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
	transport_dtls *dtls = (transport_dtls*) tp;
	char buf[PJ_INET6_ADDRSTRLEN+10];
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Save the callbacks */
    pj_lock_acquire(dtls->mutex);
    dtls->rtp_cb = rtp_cb;
    dtls->rtcp_cb = rtcp_cb;
	dtls->user_data = user_data;
	
	// Save remote address.
	pj_sockaddr_cp(&dtls->rem_addr, rem_addr);
	PJ_LOG(3, (THIS_FILE, "transport_attach rem_addr=[%s]", 
		pj_sockaddr_print((pj_sockaddr_t *)&dtls->rem_addr, buf, sizeof(buf), 3)));

    pj_lock_release(dtls->mutex);

    /* Attach itself to transport */
    status = pjmedia_transport_attach(dtls->member_tp, dtls, rem_addr, 
				      rem_rtcp, addr_len, &dtls_rtp_cb,
				      &dtls_rtcp_cb);
    if (status != PJ_SUCCESS) {
	pj_lock_acquire(dtls->mutex);
	dtls->rtp_cb = NULL;
	dtls->rtcp_cb = NULL;
	dtls->user_data = NULL;
	pj_lock_release(dtls->mutex);
	return status;
    }

    return PJ_SUCCESS;
}

static void transport_detach(pjmedia_transport *tp, void *strm)
{
    transport_dtls *dtls = (transport_dtls*) tp;

    PJ_UNUSED_ARG(strm);
    PJ_ASSERT_ON_FAIL(tp, return);

    if (dtls->member_tp) {
	pjmedia_transport_detach(dtls->member_tp, dtls);
    }

    /* Clear up application infos from transport */
    pj_lock_acquire(dtls->mutex);
    dtls->rtp_cb = NULL;
    dtls->rtcp_cb = NULL;
    dtls->user_data = NULL;
    pj_lock_release(dtls->mutex);
}

static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    pj_status_t status = PJ_SUCCESS;
    transport_dtls *dtls = (transport_dtls*) tp;
    //int len = size;
    //int err = PJ_SUCCESS;
    char *data = (char *)pkt;

    if (dtls->bypass_dtls)
		return pjmedia_transport_send_rtp(dtls->member_tp, pkt, size);

	if (dtls->ssl_state == SSL_STATE_ESTABLISHED)
	{
		// Ticket #1573: Don't hold mutex while calling PJLIB socket send().
#if ENABLE_DELAYED_SEND // We no need do do delay send. Because UDT will handle it.
		/* Flush delayed send first. Sending data might be delayed when 
		 * re-negotiation is on-progress.
		 */
		status = flush_delayed_send(dtls);
		if (status == PJ_EBUSY) {
		/* Re-negotiation or flushing is on progress, delay sending */
			//status = delay_send(dtls, data, size, 0); 
			status = PJ_EBUSY;
			PJ_LOG(5, (THIS_FILE, "transport_send_rtp() flush_delayed_send PJ_BUSY."));
			goto on_return;
		} else if (status != PJ_SUCCESS) {
			PJ_LOG(5, (THIS_FILE, "transport_send_rtp() flush_delayed_send failed status=[%d].", status));
			goto on_return;
		}
#endif
		/* Write data to SSL */
		status = ssl_write(dtls, data, size, 0);
		if (status == PJ_EBUSY) {
			/* Re-negotiation is on progress, delay sending */
			PJ_LOG(5, (THIS_FILE, "transport_send_rtp() ssl_write PJ_BUSY."));
#if ENABLE_DELAYED_SEND // We no need do do delay send. Because UDT will handle it.
			//status = delay_send(dtls, data, size, 0); 
#endif
			goto on_return;
		}

		if (size == sizeof(dtls->dtls_rcv_buffer))
			PJ_LOG(5, (THIS_FILE, "ssl_write. status=[%d], size_=[%d], [%x], msg_type=[%d], pkt_id=[%d]", 
			status, size, ((pj_uint16_t *)data)[0], ((pj_uint8_t*)data)[TCP_SESS_MSG_TYPE_INDEX], pj_ntohl(((pj_uint32_t*)&((pj_uint8_t*)data)[22])[0])));
		else
			PJ_LOG(5, (THIS_FILE, "ssl_write. status=[%d], size_=[%d], [%x]", status, size, ((pj_uint16_t *)data)[0]));
	} else {
		PJ_LOG(5, (THIS_FILE, "transport_send_rtp() SSL not ready dtls->ssl_state=[%d].", dtls->ssl_state));
#if ENABLE_DELAYED_SEND // We no need do do delay send. Because UDT will handle it.
		//status = delay_send(dtls, data, size, 0); 
#endif
		status = PJ_EBUSY;
	}
on_return:
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
    pj_status_t status = PJ_SUCCESS;
    transport_dtls *dtls = (transport_dtls*) tp;
    //int len = size;
    int err = PJ_SUCCESS;

    if (dtls->bypass_dtls) {
	return pjmedia_transport_send_rtcp2(dtls->member_tp, addr, addr_len, 
	                                    pkt, size);
    }

    //if (size > sizeof(dtls->dtls_tx_buffer))
	//return PJ_ETOOBIG;

    //pj_memcpy(dtls->dtls_tx_buffer, pkt, size);

    pj_lock_acquire(dtls->mutex);
    if (!dtls->session_inited) {
	pj_lock_release(dtls->mutex);
	return PJ_EINVALIDOP;
    }
    //err = srtp_protect_rtcp(dtls->srtp_tx_ctx, dtls->rtcp_tx_buffer, &len);
    pj_lock_release(dtls->mutex);

    if (err == PJ_SUCCESS) {
	//status = pjmedia_transport_send_rtcp2(dtls->member_tp, addr, addr_len,
	//				      dtls->dtls_tx_buffer, len);
    } else {
	//status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
    }

    return status;
}


static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
					   pjmedia_dir dir,
					   unsigned pct_lost)
{
    transport_dtls *dtls = (transport_dtls *) tp;
    
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    return pjmedia_transport_simulate_lost(dtls->member_tp, dir, pct_lost);
}

static pj_status_t transport_destroy  (pjmedia_transport *tp)
{
    transport_dtls *dtls = (transport_dtls *) tp;
    pj_status_t status;

	PJ_ASSERT_RETURN(tp, PJ_EINVAL);

	if (dtls->timer_heap) {
		if (dtls->timer.id > 0) {
			pj_timer_heap_cancel(dtls->timer_heap, &dtls->timer);
			dtls->timer.id = TIMER_NONE;
		}

		pj_timer_heap_destroy(dtls->timer_heap);
		dtls->timer_heap = NULL;
	}

    if (dtls->setting.close_member_tp && dtls->member_tp) {
	pjmedia_transport_close(dtls->member_tp);
    }

    status = pjmedia_transport_dtls_stop(tp);

	/* In case mutex is being acquired by other thread */

	pj_lock_acquire(dtls->mutex);
	pj_lock_release(dtls->mutex);

	pj_lock_destroy(dtls->mutex);
    pj_pool_release(dtls->pool);

    return status;
}

/*
 * This callback is called by transport when incoming rtp is received
 */
static void dtls_rtp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_dtls *dtls = (transport_dtls *) user_data;
    //int len = size;
    //pj_status_t err = PJ_SUCCESS;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
	void *cb_data = NULL;
	pj_size_t nwritten;
	pj_status_t status;
	void *data = pkt;

    if (dtls->bypass_dtls) {
	dtls->rtp_cb(dtls->user_data, pkt, size);
	return;
    }

    if (size < 0) {
	return;
    }

	if (dtls->ssl_state == SSL_STATE_HANDSHAKING) {
		/* Cancel handshake timer */
		if (dtls->timer.id == TIMER_HANDSHAKE_RETRANSMISSION ||
			dtls->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
				pj_timer_heap_cancel(dtls->timer_heap, &dtls->timer);
				dtls->timer.id = TIMER_NONE;
				PJ_LOG(3, (THIS_FILE, "dtls_rtp_cb() TIMER_HANDSHAKE_RETRANSMISSION cancelled."));
		}
	}

	/* Socket error or closed */
	if (data && size > 0) {
		/* Consume the whole data */
		nwritten = BIO_write(dtls->ossl_rbio, data, size);
		if (nwritten < size) {
			status = GET_SSL_STATUS(dtls);
			return;
		}
	}

	/* Check if SSL handshake hasn't finished yet */
	if (dtls->ssl_state == SSL_STATE_HANDSHAKING) {
		pj_bool_t ret = PJ_TRUE;

		status = do_handshake(dtls);
		PJ_LOG(4, (THIS_FILE, "@#@#@#@#@# do_handshake. status=%d", status));
		/* Not pending is either success or failed */
		if (status != PJ_EPENDING)
			ret = on_handshake_complete(dtls, status);

		return;
	}

	// Read decrypted data from BIO and send it to underlying media transport.
	do {
		int size_ = sizeof(dtls->dtls_rcv_buffer);

	    /* SSL_read() may write some data to BIO write when re-negotiation
	     * is on progress, so let's protect it with write mutex.
	     */
		pj_lock_acquire(dtls->mutex);
		size_ = SSL_read(dtls->ossl_ssl, (void *)dtls->dtls_rcv_buffer, size_);
		pj_lock_release(dtls->mutex);

		if (size_ > 0) {
			if (size_ == sizeof(dtls->dtls_rcv_buffer))
				PJ_LOG(5, (THIS_FILE, "SSL_read ok. size_=[%d], [%x], msg_type=[%d], pkt_id=[%d]", 
					size_, ((pj_uint16_t *)dtls->dtls_rcv_buffer)[0], 
					((pj_uint8_t*)dtls->dtls_rcv_buffer)[TCP_SESS_MSG_TYPE_INDEX], 
					pj_ntohl(((pj_uint32_t*)&((pj_uint8_t*)dtls->dtls_rcv_buffer)[22])[0])));
			else
				PJ_LOG(5, (THIS_FILE, "SSL_read ok. size_=[%d], [%x]", size_, ((pj_uint16_t *)dtls->dtls_rcv_buffer)[0]));
			cb = dtls->rtp_cb;
			cb_data = dtls->user_data;
			//pj_lock_release(dtls->mutex);

			if (cb) {
				(*cb)(cb_data, dtls->dtls_rcv_buffer, size_);
			}

		} else {
			int err;

			err = SSL_get_error(dtls->ossl_ssl, size_);
			PJ_LOG(5, (THIS_FILE, "SSL_read failed. size_=[%d], err=[%d]", err, size_));

			/* SSL might just return SSL_ERROR_WANT_READ in 
			 * re-negotiation.
			 */
			if (err != SSL_ERROR_NONE && err != SSL_ERROR_WANT_READ)
			{
				PJ_LOG(5, (THIS_FILE, "SSL_read failed. size_=[%d], err=[%d], lerr=[%d], wsalerr=[%d]", size_, err, pj_get_os_error(), pj_get_netos_error()));
				/* Reset SSL socket state, then return PJ_FALSE */
				status = STATUS_FROM_SSL_ERR(dtls, err);
				goto on_error;
			}

			status = do_handshake(dtls);
			if (status == PJ_SUCCESS) {
				/* Renegotiation completed */

				/* Update certificates */
				update_certs_info(dtls);
#ifdef ENABLE_DELAYED_SEND
				// Ticket #1573: Don't hold mutex while calling
				//               PJLIB socket send(). 
				//pj_lock_acquire(ssock->write_mutex);
				status = flush_delayed_send(dtls);
				//pj_lock_release(ssock->write_mutex);
#endif
				/* If flushing is ongoing, treat it as success */
				if (status == PJ_EBUSY)
					status = PJ_SUCCESS;

				if (status != PJ_SUCCESS && status != PJ_EPENDING) {
				PJ_PERROR(1,(dtls->pool->obj_name, status, 
						 "Failed to flush delayed send"));
				goto on_error;
				}
			} else if (status != PJ_EPENDING) {
				PJ_PERROR(1,(dtls->pool->obj_name, status, 
						 "Renegotiation failed"));
				goto on_error;
			}

			break;
		}
	} while(1);

	return;

on_error:
	if (dtls->ssl_state == SSL_STATE_HANDSHAKING)
	{
		on_handshake_complete(dtls, status);
		//pj_lock_release(dtls->mutex);
		return;
	}
	dtls->ssl_state = SSL_STATE_NULL;

	return;
}

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void dtls_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_dtls *dtls = (transport_dtls *) user_data;
    int len = size;
    pj_status_t err = PJ_SUCCESS;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
    void *cb_data = NULL;

    if (dtls->bypass_dtls) {
	dtls->rtcp_cb(dtls->user_data, pkt, size);
	return;
    }

    if (size < 0) {
	return;
    }

    pj_lock_acquire(dtls->mutex);

    if (!dtls->session_inited) {
	pj_lock_release(dtls->mutex);
	return;
    }
	// TODO DTLS decrypt data
    //err = srtp_unprotect_rtcp(dtls->srtp_rx_ctx, (pj_uint8_t*)pkt, &len);
    if (err != PJ_SUCCESS) {
	PJ_LOG(5,(dtls->pool->obj_name, 
		  "Failed to decrypt DTLS, pkt size=%d, err=%d",
		  size, GET_SSL_STATUS(dtls)));
    } else {
	cb = dtls->rtcp_cb;
	cb_data = dtls->user_data;
    }

    pj_lock_release(dtls->mutex);

    if (cb) {
	(*cb)(cb_data, pkt, len);
    }
}

#if 0
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
	pj_status_t err;
	unsigned i;

	PJ_ASSERT_RETURN(MAX_KEY_LEN >= crypto_suites[cs_idx].cipher_key_len,
			 PJ_ETOOSMALL);

	do {
	    key_ok = PJ_TRUE;

	    err = crypto_get_random((unsigned char*)key, 
				     crypto_suites[cs_idx].cipher_key_len);
	    if (err != err_status_ok) {
		PJ_LOG(5,(THIS_FILE, "Failed generating random key: %s",
			  ssl_strerror(err)));
		//return PJMEDIA_ERRNO_FROM_LIBSRTP(err);
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
#endif

static pj_status_t transport_media_create(pjmedia_transport *tp,
				          pj_pool_t *sdp_pool,
					  unsigned options,
				          const pjmedia_sdp_session *sdp_remote,
					  unsigned media_index)
{
    struct transport_dtls *dtls = (struct transport_dtls*) tp;
    unsigned member_tp_option;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    dtls->media_option = options;
    member_tp_option = options | PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

    dtls->offerer_side = sdp_remote == NULL;

    /* Validations */
    if (dtls->offerer_side) {
	
	if (dtls->setting.use == PJMEDIA_DTLS_DISABLED)
	    goto BYPASS_DTLS;

    } else {

	pjmedia_sdp_media *m_rem;

	m_rem = sdp_remote->media[media_index];

	/* Nothing to do on inactive media stream */
	//if (pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL))
	//    goto BYPASS_DTLS;

	/* Validate remote media transport based on SRTP usage option.
	 */
	switch (dtls->setting.use) {
	    case PJMEDIA_DTLS_DISABLED:
		goto BYPASS_DTLS;
	    case PJMEDIA_DTLS_MANDATORY:
		break;
	}

    }
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_DTLS:
    dtls->bypass_dtls = PJ_TRUE;
    //member_tp_option &= ~PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

PROPAGATE_MEDIA_CREATE:
	// DEAN assign app's lock object.
	dtls->member_tp->app_lock = dtls->base.app_lock;
	dtls->member_tp->call_id = dtls->base.call_id;
    return pjmedia_transport_media_create(dtls->member_tp, sdp_pool, 
					  member_tp_option, sdp_remote,
					  media_index);
}

static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
					pj_pool_t *sdp_pool,
					pjmedia_sdp_session *sdp_local,
					const pjmedia_sdp_session *sdp_remote,
					unsigned media_index)
{
    struct transport_dtls *dtls = (struct transport_dtls*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    enum { MAXLEN = 512 };
    char buffer[MAXLEN];
    int buffer_len;
    pj_status_t status;
    pjmedia_sdp_attr *attr;
    pj_str_t attr_value;
    unsigned i, j;

    PJ_ASSERT_RETURN(tp && sdp_pool && sdp_local, PJ_EINVAL);
    
    dtls->offerer_side = (sdp_remote == NULL);

    m_rem = sdp_remote ? sdp_remote->media[media_index] : NULL;
    m_loc = sdp_local->media[media_index];

    /* If the media is inactive, do nothing. */
    /* No, we still need to process SRTP offer/answer even if the media is
     * marked as inactive, because the transport is still alive in this
     * case (e.g. for keep-alive). See:
     *   http://trac.pjsip.org/repos/ticket/1079
     */
    /*
    if (pjmedia_sdp_media_find_attr(m_loc, &ID_INACTIVE, NULL) || 
	(m_rem && pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL)))
	goto BYPASS_DTLS;
    */

    /* Check remote media transport & set local media transport 
     * based on SRTP usage option.
     */
    if (dtls->offerer_side) {

		/* Generate transport */
		switch (dtls->setting.use) {
			case PJMEDIA_DTLS_DISABLED:
				m_loc->desc.transport = ID_TNL_PLAIN;
				goto BYPASS_DTLS;
			case PJMEDIA_DTLS_MANDATORY:
				m_loc->desc.transport = ID_TNL_DTLS;
				break;
	}
    } else {
		/* Answerer side */

		pj_assert(sdp_remote && m_rem);

		/* Generate transport */
		if (pj_stricmp(&m_rem->desc.transport, &ID_TNL_PLAIN) == 0) {
			m_loc->desc.transport = ID_TNL_PLAIN;
			goto BYPASS_DTLS;
		} else {
			m_loc->desc.transport = ID_TNL_DTLS;
			dtls->bypass_dtls = PJ_FALSE;
		}

    }
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_DTLS:
    dtls->bypass_dtls = PJ_TRUE;

PROPAGATE_MEDIA_CREATE:
    return pjmedia_transport_encode_sdp(dtls->member_tp, sdp_pool, 
					sdp_local, sdp_remote, media_index);
}



static pj_status_t transport_media_start(pjmedia_transport *tp,
				         pj_pool_t *pool,
				         const pjmedia_sdp_session *sdp_local,
				         const pjmedia_sdp_session *sdp_remote,
				         unsigned media_index)
{
    struct transport_dtls *dtls = (struct transport_dtls*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && pool && sdp_local && sdp_remote, PJ_EINVAL);

    m_rem = sdp_remote->media[media_index];
    m_loc = sdp_local->media[media_index];

	status = pjmedia_transport_dtls_start(tp);
	if (status != PJ_SUCCESS)
		return status;

    return pjmedia_transport_media_start(dtls->member_tp, pool, 
					 sdp_local, sdp_remote,
				         media_index);
}

static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct transport_dtls *dtls = (struct transport_dtls*) tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    status = pjmedia_transport_media_stop(dtls->member_tp);
    if (status != PJ_SUCCESS)
	PJ_LOG(4, (dtls->pool->obj_name, 
		   "DTLS failed stop underlying media transport."));

    return pjmedia_transport_dtls_stop(tp);
}

/* Utility */
PJ_DEF(pj_status_t) pjmedia_transport_dtls_decrypt_pkt(pjmedia_transport *tp,
						       pj_bool_t is_rtp,
						       void *pkt,
						       int *pkt_len)
{
    transport_dtls *dtls = (transport_dtls *)tp;
    pj_status_t err = PJ_SUCCESS;

    if (dtls->bypass_dtls)
	return PJ_SUCCESS;

    PJ_ASSERT_RETURN(tp && pkt && (*pkt_len>0), PJ_EINVAL);
    PJ_ASSERT_RETURN(dtls->session_inited, PJ_EINVALIDOP);

    /* Make sure buffer is 32bit aligned */
    PJ_ASSERT_ON_FAIL( (((long)pkt) & 0x03)==0, return PJ_EINVAL);

    pj_lock_acquire(dtls->mutex);

    if (!dtls->session_inited) {
	pj_lock_release(dtls->mutex);
	return PJ_EINVALIDOP;
    }

	// TODO dtls decrypt

    if (err != PJ_SUCCESS) {
	PJ_LOG(5,(dtls->pool->obj_name, 
		  "Failed to decrypt DTLS, pkt size=%d, err=%d", 
		  *pkt_len, GET_SSL_STATUS(dtls)));
    }

    pj_lock_release(dtls->mutex);

    return (err==PJ_SUCCESS) ? PJ_SUCCESS : err;
}


