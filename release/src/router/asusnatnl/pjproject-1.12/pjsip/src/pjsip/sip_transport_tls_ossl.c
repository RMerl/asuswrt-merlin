/* $Id: sip_transport_tls_ossl.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/compat/socket.h>
#include <pj/sock_select.h>
#include <pj/string.h>


/* Only build when PJSIP_HAS_TLS_TRANSPORT is enabled */
#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0



/* 
 * Include OpenSSL headers 
 */
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/*
 * With VisualC++, it's not possible to dynamically link against some
 * libraries when some macros are defined (unlike "make" based build
 * system where this can be easily manipulated).
 *
 * So for VisualC++ IDE, include OpenSSL libraries in the linking by
 * using the #pragma lib construct below.
 */
#ifdef _MSC_VER
# ifdef _DEBUG
#  pragma comment( lib, "libeay32MTd")
#  pragma comment( lib, "ssleay32MTd")
#else
#  pragma comment( lib, "libeay32MT")
#  pragma comment( lib, "ssleay32MT")
# endif
#endif


#define THIS_FILE	"transport_tls_ossl.c"

#define MAX_ASYNC_CNT	16
#define POOL_LIS_INIT	512
#define POOL_LIS_INC	512
#define POOL_TP_INIT	512
#define POOL_TP_INC	512



/**
 * Get the number of descriptors in the set. This is defined in sock_select.c
 * This function will only return the number of sockets set from PJ_FD_SET
 * operation. When the set is modified by other means (such as by select()),
 * the count will not be reflected here.
 *
 * That's why don't export this function in the header file, to avoid
 * misunderstanding.
 *
 * @param fdsetp    The descriptor set.
 *
 * @return          Number of descriptors in the set.
 */
PJ_DECL(pj_size_t) PJ_FD_COUNT(const pj_fd_set_t *fdsetp);



struct tls_listener;
struct tls_transport;


/*
 * This structure is "descendant" of pj_ioqueue_op_key_t, and it is used to
 * track pending/asynchronous accept() operation. TLS transport may have
 * more than one pending accept() operations, depending on the value of
 * async_cnt.
 */
struct pending_accept
{
    pj_ioqueue_op_key_t	     op_key;
    struct tls_listener	    *listener;
    unsigned		     index;
    pj_pool_t		    *pool;
    pj_sock_t		     new_sock;
    int			     addr_len;
    pj_sockaddr_in	     local_addr;
    pj_sockaddr_in	     remote_addr;
};


/*
 * This is the TLS listener, which is a "descendant" of pjsip_tpfactory (the
 * SIP transport factory).
 */
struct tls_listener
{
    pjsip_tpfactory	     factory;
    pj_bool_t		     is_registered;
    pjsip_tls_setting	     setting;
    pjsip_endpoint	    *endpt;
    pjsip_tpmgr		    *tpmgr;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    unsigned		     async_cnt;
    struct pending_accept   *accept_op[MAX_ASYNC_CNT];

    SSL_CTX		    *ctx;
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
};


/*
 * This structure describes the TLS transport, and it's descendant of
 * pjsip_transport.
 */
struct tls_transport
{
    pjsip_transport	     base;
    pj_bool_t		     is_server;

    /* Do not save listener instance in the transport, because
     * listener might be destroyed during transport's lifetime.
     * See http://trac.pjsip.org/repos/ticket/491
    struct tls_listener	    *listener;
     */

    /* TLS settings, copied from listener */
    struct {
	pj_str_t	     server_name;
	pj_time_val	     timeout;
    } setting;

    pj_bool_t		     is_registered;
    pj_bool_t		     is_closing;
    pj_status_t		     close_reason;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    pj_bool_t		     has_pending_connect;

    /* SSL connection */
    SSL			    *ssl;
    pj_bool_t		     ssl_shutdown_called;

    /* Keep alive */
    pj_timer_entry	     ka_timer;
    pj_time_val		     last_activity;

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
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status);

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory);

/* This callback is called by transport manager to create transport */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **transport);

/* Common function to create and initialize transport */
static pj_status_t tls_create(struct tls_listener *listener,
			      pj_pool_t *pool,
			      pj_sock_t sock, pj_bool_t is_server,
			      const pj_sockaddr_in *local,
			      const pj_sockaddr_in *remote,
			      struct tls_transport **p_tls);


/****************************************************************************
 * SSL FUNCTIONS
 */

/* ssl_report_error() */
static void ssl_report_error(const char *sender, int level, 
			     pj_status_t status,
			     const char *format, ...)
{
    va_list marker;

    va_start(marker, format);

#if PJ_LOG_MAX_LEVEL > 0
    if (status != PJ_SUCCESS) {
	char err_format[PJ_ERR_MSG_SIZE + 512];
	int len;

	len = pj_ansi_snprintf(err_format, sizeof(err_format),
			       "%s: ", format);
	pj_strerror(status, err_format+len, sizeof(err_format)-len);
	
	pj_log(sender, level, err_format, marker);

    } else {
	unsigned long ssl_err;

	ssl_err = ERR_get_error();

	if (ssl_err == 0) {
	    pj_log(sender, level, format, marker);
	} else {
	    char err_format[512];
	    int len;

	    len = pj_ansi_snprintf(err_format, sizeof(err_format),
				   "%s: ", format);
	    ERR_error_string(ssl_err, err_format+len);
	    
	    pj_log(sender, level, err_format, marker);
	}
    }
#endif

    va_end(marker);
}


static void sockaddr_to_host_port( pj_pool_t *pool,
				   pjsip_host_port *host_port,
				   const pj_sockaddr_in *addr )
{
    enum { M = 48 };
    host_port->host.ptr = (char*)pj_pool_alloc(pool, M);
    host_port->host.slen = pj_ansi_snprintf( host_port->host.ptr, M, "%s", 
					    pj_inet_ntoa(addr->sin_addr));
    host_port->port = pj_ntohs(addr->sin_port);
}


/* SSL password callback. */
static int password_cb(char *buf, int num, int rwflag, void *user_data)
{
    struct tls_listener *lis = (struct tls_listener*) user_data;

    PJ_UNUSED_ARG(rwflag);

    if(num < lis->setting.password.slen+1)
	return 0;
    
    pj_memcpy(buf, lis->setting.password.ptr, lis->setting.password.slen);
    return lis->setting.password.slen;
}


/* OpenSSL library initialization counter */
static int openssl_init_count;

/* Initialize OpenSSL */
static pj_status_t init_openssl(void)
{
    if (++openssl_init_count != 1)
	return PJ_SUCCESS;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    return PJ_SUCCESS;
}

/* Shutdown OpenSSL */
static void shutdown_openssl(void)
{
    if (--openssl_init_count != 0)
	return;
}


/* Create and initialize new SSL context */
static pj_status_t create_ctx( struct tls_listener *lis, SSL_CTX **p_ctx)
{
    struct pjsip_tls_setting *opt = &lis->setting;
    int method;
    char *lis_name = lis->factory.obj_name;
    SSL_METHOD *ssl_method;
    SSL_CTX *ctx;
    int mode, rc;
        
    *p_ctx = NULL;

    /* Make sure OpenSSL library has been initialized */
    init_openssl();

    /* Determine SSL method to use */
    method = opt->method;
    if (method == PJSIP_SSL_UNSPECIFIED_METHOD)
	method = PJSIP_SSL_DEFAULT_METHOD;

    switch (method) {
    case PJSIP_SSLV23_METHOD:
	ssl_method = SSLv23_method();
	break;
    case PJSIP_TLSV1_METHOD:
	ssl_method = TLSv1_method();
	break;
    case PJSIP_SSLV2_METHOD:
	ssl_method = SSLv2_method();
	break;
    case PJSIP_SSLV3_METHOD:
	ssl_method = SSLv3_method();
	break;
    default:
	ssl_report_error(lis_name, 4, PJSIP_TLS_EINVMETHOD,
			 "Error creating SSL context");
	return PJSIP_TLS_EINVMETHOD;
    }

    /* Create SSL context for the listener */
    ctx = SSL_CTX_new(ssl_method);
    if (ctx == NULL) {
	ssl_report_error(lis_name, 4, PJ_SUCCESS,
			 "Error creating SSL context");
	return PJSIP_TLS_ECTX;
    }


    /* Load CA list if one is specified. */
    if (opt->ca_list_file.slen) {

	/* Can only take a NULL terminated filename in the setting */
	pj_assert(opt->ca_list_file.ptr[opt->ca_list_file.slen] == '\0');

	rc = SSL_CTX_load_verify_locations(ctx, opt->ca_list_file.ptr, NULL);

	if (rc != 1) {
	    ssl_report_error(lis_name, 4, PJ_SUCCESS,
			     "Error loading/verifying CA list file '%.*s'",
			     (int)opt->ca_list_file.slen,
			     opt->ca_list_file.ptr);
	    SSL_CTX_free(ctx);
	    return PJSIP_TLS_ECACERT;
	}

	PJ_LOG(5,(lis_name, "TLS CA file successfully loaded from '%.*s'",
		  (int)opt->ca_list_file.slen,
		  opt->ca_list_file.ptr));
    }
    
    /* Set password callback */
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, lis);


    /* Load certificate if one is specified */
    if (opt->cert_file.slen) {

	/* Can only take a NULL terminated filename in the setting */
	pj_assert(opt->cert_file.ptr[opt->cert_file.slen] == '\0');

	/* Load certificate chain from file into ctx */
	rc = SSL_CTX_use_certificate_chain_file(ctx, opt->cert_file.ptr);

	if(rc != 1) {
	    ssl_report_error(lis_name, 4, PJ_SUCCESS,
			     "Error loading certificate chain file '%.*s'",
			     (int)opt->cert_file.slen,
			     opt->cert_file.ptr);
	    SSL_CTX_free(ctx);
	    return PJSIP_TLS_ECERTFILE;
	}

	PJ_LOG(5,(lis_name, "TLS certificate successfully loaded from '%.*s'",
		  (int)opt->cert_file.slen,
		  opt->cert_file.ptr));
    }


    /* Load private key if one is specified */
    if (opt->privkey_file.slen) {

	/* Can only take a NULL terminated filename in the setting */
	pj_assert(opt->privkey_file.ptr[opt->privkey_file.slen] == '\0');

	/* Adds the first private key found in file to ctx */
	rc = SSL_CTX_use_PrivateKey_file(ctx, opt->privkey_file.ptr, 
					 SSL_FILETYPE_PEM);

	if(rc != 1) {
	    ssl_report_error(lis_name, 4, PJ_SUCCESS,
			     "Error adding private key from '%.*s'",
			     (int)opt->privkey_file.slen,
			     opt->privkey_file.ptr);
	    SSL_CTX_free(ctx);
	    return PJSIP_TLS_EKEYFILE;
	}

	PJ_LOG(5,(lis_name, "TLS private key successfully loaded from '%.*s'",
		  (int)opt->privkey_file.slen,
		  opt->privkey_file.ptr));
    }


    /* SSL verification options */
    if (lis->setting.verify_client || lis->setting.verify_server) {
	mode = SSL_VERIFY_PEER;
	if (lis->setting.require_client_cert)
	    mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    } else {
	mode = SSL_VERIFY_NONE;
    }

    SSL_CTX_set_verify(ctx, mode, NULL);

    PJ_LOG(5,(lis_name, "TLS verification mode set to %d", mode));

    /* Optionally set cipher list if one is specified */
    if (opt->ciphers.slen) {
	/* Can only take a NULL terminated cipher list in the setting */
	pj_assert(opt->cert_file.ptr[opt->cert_file.slen] == '\0');

	rc = SSL_CTX_set_cipher_list(ctx, opt->ciphers.ptr);
	if (rc != 1) {
	    ssl_report_error(lis_name, 4, PJ_SUCCESS,
			     "Error setting cipher list '%.*s'",
			     (int)opt->ciphers.slen,
			     opt->ciphers.ptr);
	    SSL_CTX_free(ctx);
	    return PJSIP_TLS_ECIPHER;
	}

	PJ_LOG(5,(lis_name, "TLS ciphers set to '%.*s'",
		  (int)opt->ciphers.slen,
		  opt->ciphers.ptr));
    }

    /* Done! */

    *p_ctx = ctx;
    return PJ_SUCCESS;
}

/* Destroy SSL context */
static void destroy_ctx(SSL_CTX *ctx)
{
    SSL_CTX_free(ctx);

    /* Potentially shutdown OpenSSL library if this is the last
     * context exists.
     */
    shutdown_openssl();
}

/*
 * Perform SSL_connect upon completion of socket connect()
 */
static pj_status_t ssl_connect(struct tls_transport *tls)
{
    SSL *ssl = tls->ssl;
    int status;

    if (SSL_is_init_finished (ssl))
	return PJ_SUCCESS;

    if (SSL_get_fd(ssl) < 0)
	SSL_set_fd(ssl, (int)tls->sock);

    if (!SSL_in_connect_init(ssl))
	SSL_set_connect_state(ssl);

#ifdef SSL_set_tlsext_host_name
    if (tls->setting.server_name.slen) {
	char server_name[PJ_MAX_HOSTNAME];

	if (tls->setting.server_name.slen >= PJ_MAX_HOSTNAME)
	    return PJ_ENAMETOOLONG;

	pj_memcpy(server_name, tls->setting.server_name.ptr, 
		  tls->setting.server_name.slen);
	server_name[tls->setting.server_name.slen] = '\0';

	if (!SSL_set_tlsext_host_name(ssl, server_name)) {
	    PJ_LOG(4,(tls->base.obj_name, 
		      "SSL_set_tlsext_host_name() failed"));
	}
    }
#endif

    PJ_LOG(5,(tls->base.obj_name, "Starting SSL_connect() negotiation"));

    do {
	/* These handle sets are used to set up for whatever SSL_connect
	 * says it wants next. They're reset on each pass around the loop.
	 */
	pj_fd_set_t rd_set;
	pj_fd_set_t wr_set;
	
	PJ_FD_ZERO(&rd_set);
	PJ_FD_ZERO(&wr_set);

	status = SSL_connect (ssl);
	switch (SSL_get_error (ssl, status)) {
        case SSL_ERROR_NONE:
	    /* Success */
	    status = 0;
	    PJ_LOG(5,(tls->base.obj_name, 
		      "SSL_connect() negotiation completes successfully"));
	    break;

        case SSL_ERROR_WANT_WRITE:
	    /* Wait for more activity */
	    PJ_FD_SET(tls->sock, &wr_set);
	    status = 1;
	    break;
	    
        case SSL_ERROR_WANT_READ:
	    /* Wait for more activity */
	    PJ_FD_SET(tls->sock, &rd_set);
	    status = 1;
	    break;
	    
        case SSL_ERROR_ZERO_RETURN:
	    /* The peer has notified us that it is shutting down via
	     * the SSL "close_notify" message so we need to
	     * shutdown, too.
	     */
	    PJ_LOG(4,(THIS_FILE, "SSL connect() failed, remote has"
				 "shutdown connection."));
	    return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
	    
        case SSL_ERROR_SYSCALL:
	    /* On some platforms (e.g. MS Windows) OpenSSL does not
	     * store the last error in errno so explicitly do so.
	     *
	     * Explicitly check for EWOULDBLOCK since it doesn't get
	     * converted to an SSL_ERROR_WANT_{READ,WRITE} on some
	     * platforms. If SSL_connect failed outright, though, don't
	     * bother checking more. This can happen if the socket gets
	     * closed during the handshake.
	     */
	    if (pj_get_netos_error() == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) &&
		status == -1)
            {
		/* Although the SSL_ERROR_WANT_READ/WRITE isn't getting
		 * set correctly, the read/write state should be valid.
		 * Use that to decide what to do.
		 */
		status = 1;               /* Wait for more activity */
		if (SSL_want_write (ssl))
		    PJ_FD_SET(tls->sock, &wr_set);
		else if (SSL_want_read (ssl))
		    PJ_FD_SET(tls->sock, &rd_set);
		else
		    status = -1;	/* Doesn't want anything - bail out */
            }
	    else {
		status = -1;
	    }
	    break;

        default:
	    ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			     "SSL_connect() error");
	    status = -1;
	    break;
        }
	
	if (status == 1) {
	    pj_time_val timeout, *p_timeout;

	    /* Must have at least one handle to wait for at this point. */
	    pj_assert(PJ_FD_COUNT(&rd_set) == 1 || PJ_FD_COUNT(&wr_set) == 1);
	    
	    /* This will block the whole stack!!! */
	    PJ_TODO(SUPPORT_SSL_ASYNCHRONOUS_CONNECT);

	    if (tls->setting.timeout.sec == 0 &&
		tls->setting.timeout.msec == 0)
	    {
		p_timeout = NULL;
	    } else {
		timeout = tls->setting.timeout;
		p_timeout = &timeout;
	    }

	    /* Block indefinitely if timeout pointer is zero. */
	    status = pj_sock_select(tls->sock+1, &rd_set, &wr_set,
				    NULL, p_timeout);
	    	    
	    /* 0 is timeout, so we're done.
	     * -1 is error, so we're done.
	     * Could be both handles set (same handle in both masks) so set to 1.
	     */
	    if (status >= 1)
		status = 1;
	    else if (status == 0)
		return PJSIP_TLS_ETIMEDOUT;
	    else
		status = -1;
        }
	
    } while (status == 1 && !SSL_is_init_finished (ssl));
        
    return (status == -1 ? PJSIP_TLS_ECONNECT : PJ_SUCCESS);
}


/*
 * Perform SSL_accept() on the newly established incoming TLS connection.
 */
static pj_status_t ssl_accept(struct tls_transport *tls)
{
    SSL *ssl = tls->ssl;
    int rc;
    
    if (SSL_is_init_finished (ssl))
	return PJ_SUCCESS;
    
    if (!SSL_in_accept_init(ssl))
	SSL_set_accept_state(ssl);

    PJ_LOG(5,(tls->base.obj_name, "Starting SSL_accept() negotiation"));

    /* Repeat retrying SSL_accept() procedure until it completes either
     * successfully or with failure.
     */
    do {

	/* These handle sets are used to set up for whatever SSL_accept says
	 * it wants next. They're reset on each pass around the loop.
	 */
	pj_fd_set_t rd_set;
	pj_fd_set_t wr_set;
	
	PJ_FD_ZERO(&rd_set);
	PJ_FD_ZERO(&wr_set);

	rc = SSL_accept (ssl);

	switch (SSL_get_error (ssl, rc)) {
        case SSL_ERROR_NONE:
	    /* Success! */
	    rc = 0;
	    PJ_LOG(5,(tls->base.obj_name, 
		      "SSL_accept() negotiation completes successfully"));
	    break;
	    
        case SSL_ERROR_WANT_WRITE:
	    PJ_FD_SET(tls->sock, &wr_set);
	    rc = 1;               /* Wait for more activity */
	    break;
	    
        case SSL_ERROR_WANT_READ:
	    PJ_FD_SET(tls->sock, &rd_set);
	    rc = 1;               /* Need to read more data */
	    break;

        case SSL_ERROR_ZERO_RETURN:
	    /* The peer has notified us that it is shutting down via the SSL 
	     * "close_notify" message so we need to shutdown, too.
	     */
	    PJ_LOG(4,(tls->base.obj_name,  
		      "Incoming SSL connection closed prematurely by client"));
	    return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);

        case SSL_ERROR_SYSCALL:
	    /* Explicitly check for EWOULDBLOCK since it doesn't get converted
	     * to an SSL_ERROR_WANT_{READ,WRITE} on some platforms. 
	     * If SSL_accept failed outright, though, don't bother checking 
	     * more. This can happen if the socket gets closed during the 
	     * handshake.
	     */
	    if (pj_get_netos_error()==PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) 
		&& rc==-1)
            {
		/* Although the SSL_ERROR_WANT_READ/WRITE isn't getting set 
		 * correctly, the read/write state should be valid. Use that 
		 * to decide what to do.
		 */
		rc = 1;               /* Wait for more activity */
		if (SSL_want_write(ssl))
		    PJ_FD_SET(tls->sock, &wr_set);
		else if (SSL_want_read(ssl))
		    PJ_FD_SET(tls->sock, &rd_set);
		else {
		    /* Doesn't want anything - bail out */
		    return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
		}
            }
	    else {
		return PJSIP_TLS_EUNKNOWN;
	    }
	    break;
	    
        default:
	    ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			     "Error calling SSL_accept()");
	    return pj_get_netos_error() ? pj_get_netos_error() : 
		    PJSIP_TLS_EUNKNOWN;
        }
	
	if (rc == 1) {

	    pj_time_val timeout, *p_timeout;

	    /* Must have at least one handle to wait for at this point. */
	    pj_assert(PJ_FD_COUNT(&rd_set) == 1 || PJ_FD_COUNT(&wr_set) == 1);

	    if (tls->setting.timeout.sec == 0 &&
		tls->setting.timeout.msec == 0)
	    {
		p_timeout = NULL;
	    } else {
		timeout = tls->setting.timeout;
		p_timeout = &timeout;
	    }

	    rc = pj_sock_select(tls->sock+1, &rd_set, &wr_set, NULL, 
			        p_timeout);
	    
	    if (rc >= 1)
		rc = 1;
	    else if (rc == 0)
		return PJSIP_TLS_ETIMEDOUT;
	    else
		return pj_get_netos_error();
        }
	
    } while (rc == 1 && !SSL_is_init_finished(ssl));
    
    return (rc == -1 ? PJSIP_TLS_EUNKNOWN : PJ_SUCCESS);
}


/* Send outgoing data with SSL connection */
static pj_status_t ssl_write_bytes(struct tls_transport *tls,
				   const void *data,
				   int size,
				   const char *data_name)
{
    int sent = 0;

    do {
	const int fragment_sent = SSL_write(tls->ssl,
					    ((pj_uint8_t*)data) + sent, 
					    size - sent);
    
	switch( SSL_get_error(tls->ssl, fragment_sent)) {
	case SSL_ERROR_NONE:
	    sent += fragment_sent;
	    break;
	    
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	    /* For now, we can't handle situation where WANT_READ/WANT_WRITE
	     * is raised after some data has been sent, since we don't have
	     * mechanism to keep track of how many bytes have been sent
	     * inside the individual tdata.
	     */
	    pj_assert(sent == 0);
	    PJ_TODO(PARTIAL_SSL_SENT);
	    return PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK);
	    
	case SSL_ERROR_ZERO_RETURN:
	    /* The peer has notified us that it is shutting down via the SSL
	     * "close_notify" message. Tell the transport manager that it
	     * shouldn't use this transport any more and return ENOTCONN
	     * to caller.
	     */
	    
	    /* It is safe to call this multiple times. */
	    pjsip_transport_shutdown(&tls->base);

	    return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
	    
	case SSL_ERROR_SYSCALL:
	    if (fragment_sent == 0) {
		/* An EOF occured but the SSL "close_notify" message was not
		 * sent. Shutdown the transport and return ENOTCONN.
		 */

		/* It is safe to call this multiple times. */
		pjsip_transport_shutdown(&tls->base);

		return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
	    }
	    
	    /* Other error */
	    return pj_get_netos_error();

	default:
	    ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			     "Error sending %s with SSL_write()",
			     data_name);
	    return pj_get_netos_error() ? pj_get_netos_error() 
		    : PJSIP_TLS_ESEND;
	}
    
    } while (sent < size);

    return PJ_SUCCESS;

}

/* Send outgoing tdata with SSL connection */
static pj_status_t ssl_write(struct tls_transport *tls,
			     pjsip_tx_data *tdata)
{
    return ssl_write_bytes(tls, tdata->buf.start, 
			   tdata->buf.cur - tdata->buf.start,
			   pjsip_tx_data_get_info(tdata));
}


/* Read data from SSL connection */
static pj_status_t ssl_read(struct tls_transport *tls)
{
    pjsip_rx_data *rdata = &tls->rdata;

    int bytes_read, max_size;

    max_size = sizeof(rdata->pkt_info.packet) - rdata->pkt_info.len;
    bytes_read = SSL_read(tls->ssl, 
			  rdata->pkt_info.packet+rdata->pkt_info.len,
                          max_size);

    switch (SSL_get_error(tls->ssl, bytes_read)) {
    case SSL_ERROR_NONE:
	/* Data successfully read */
	rdata->pkt_info.len += bytes_read;
	return PJ_SUCCESS;
	
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
	return PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK);
	
    case SSL_ERROR_ZERO_RETURN:
	/* The peer has notified us that it is shutting down via the SSL
	 * "close_notify" message.
	 */
	pjsip_transport_shutdown(&tls->base);
	return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
	
    case SSL_ERROR_SYSCALL:
	if (bytes_read == 0) {
	    /* An EOF occured but the SSL "close_notify" message was not
	     * sent.
	     */
	    pjsip_transport_shutdown(&tls->base);
	    return PJ_STATUS_FROM_OS(OSERR_ENOTCONN);
	}
	
	/* Other error */
	return pj_get_netos_error();
	
    default:
	ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			 "Error reading data with SSL_read()");
	return pj_get_netos_error() ? pj_get_netos_error() 
		: PJSIP_TLS_EREAD;
    }

    /* Should not reach here */
}


/****************************************************************************
 * The TLS listener/transport factory.
 */

/*
 * This is the public API to create, initialize, register, and start the
 * TLS listener.
 */
PJ_DEF(pj_status_t) pjsip_tls_transport_start( pjsip_endpoint *endpt,
					       const pjsip_tls_setting *opt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory)
{
    pj_pool_t *pool;
    struct tls_listener *listener;
    pj_ioqueue_callback listener_cb;
    pj_sockaddr_in *listener_addr;
    int addr_len;
    unsigned i;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && async_cnt, PJ_EINVAL);

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
    listener->sock = PJ_INVALID_SOCKET;
    
    /* Create object name */
    pj_ansi_snprintf(listener->factory.obj_name, 
		     sizeof(listener->factory.obj_name),
		     "tls%p",  listener);
    
    /* Create duplicate of TLS settings */
    if (opt)
	pjsip_tls_setting_copy(pool, &listener->setting, opt);
    else
	pjsip_tls_setting_default(&listener->setting);

    /* Initialize SSL context to be used by this listener */
    status = create_ctx(listener, &listener->ctx);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_lock_create_recursive_mutex(pool, "tlslis", 
					    &listener->factory.lock);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create and bind socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &listener->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    listener_addr = (pj_sockaddr_in*)&listener->factory.local_addr;
    if (local) {
	pj_memcpy(listener_addr, local, sizeof(pj_sockaddr_in));
    } else {
	pj_sockaddr_in_init(listener_addr, NULL, 0);
    }

    status = pj_sock_bind(listener->sock, listener_addr, 
			  sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Retrieve the bound address */
    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(listener->sock, listener_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

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

	    listener_addr->sin_addr = hostip.ipv4.sin_addr;
	}

	/* Save the address name */
	sockaddr_to_host_port(listener->factory.pool, 
			      &listener->factory.addr_name, listener_addr);
    }

    /* If port is zero, get the bound port */
    if (listener->factory.addr_name.port == 0) {
	listener->factory.addr_name.port = pj_ntohs(listener_addr->sin_port);
    }

    /* Start listening to the address */
    status = pj_sock_listen(listener->sock, PJSIP_TLS_TRANSPORT_BACKLOG);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Register socket to ioqeuue */
    pj_bzero(&listener_cb, sizeof(listener_cb));
    listener_cb.on_accept_complete = &on_accept_complete;
    status = pj_ioqueue_register_sock(pool, pjsip_endpt_get_ioqueue(endpt),
				      listener->sock, listener,
				      &listener_cb, &listener->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport = lis_create_transport;
    listener->factory.destroy = lis_destroy;
    listener->is_registered = PJ_TRUE;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS) {
	listener->is_registered = PJ_FALSE;
	goto on_error;
    }


    /* Start pending accept() operations */
    if (async_cnt > MAX_ASYNC_CNT) async_cnt = MAX_ASYNC_CNT;
    listener->async_cnt = async_cnt;

    for (i=0; i<async_cnt; ++i) {
	pj_pool_t *pool;

	pool = pjsip_endpt_create_pool(endpt, "tlss%p", POOL_TP_INIT, 
				       POOL_TP_INIT);
	if (!pool) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}

	listener->accept_op[i] = PJ_POOL_ZALLOC_T(pool, struct pending_accept);
	pj_ioqueue_op_key_init(&listener->accept_op[i]->op_key, 
				sizeof(listener->accept_op[i]->op_key));
	listener->accept_op[i]->pool = pool;
	listener->accept_op[i]->listener = listener;
	listener->accept_op[i]->index = i;

	on_accept_complete(listener->key, &listener->accept_op[i]->op_key,
			   listener->sock, PJ_EPENDING);
    }

    PJ_LOG(4,(listener->factory.obj_name, 
	     "SIP TLS listener ready for incoming connections at %.*s:%d",
	     (int)listener->factory.addr_name.host.slen,
	     listener->factory.addr_name.host.ptr,
	     listener->factory.addr_name.port));

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
    unsigned i;

    if (listener->is_registered) {
	pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
	listener->is_registered = PJ_FALSE;
    }

    if (listener->key) {
	pj_ioqueue_unregister(listener->key);
	listener->key = NULL;
	listener->sock = PJ_INVALID_SOCKET;
    }

    if (listener->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(listener->sock);
	listener->sock = PJ_INVALID_SOCKET;
    }

    if (listener->factory.lock) {
	pj_lock_destroy(listener->factory.lock);
	listener->factory.lock = NULL;
    }

    for (i=0; i<PJ_ARRAY_SIZE(listener->accept_op); ++i) {
	if (listener->accept_op[i] && listener->accept_op[i]->pool) {
	    pj_pool_t *pool = listener->accept_op[i]->pool;
	    listener->accept_op[i]->pool = NULL;
	    pj_pool_release(pool);
	}
    }

    if (listener->ctx) {
	destroy_ctx(listener->ctx);
	listener->ctx = NULL;
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
				void (*callback)(pjsip_transport *transport,
						 void *token, 
						 pj_ssize_t sent_bytes));

/* Called by transport manager to shutdown */
static pj_status_t tls_shutdown(pjsip_transport *transport);

/* Called by transport manager to destroy transport */
static pj_status_t tls_destroy_transport(pjsip_transport *transport);

/* Utility to destroy transport */
static pj_status_t tls_destroy(pjsip_transport *transport,
			       pj_status_t reason);

/* Callback from ioqueue on incoming packet */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

/* Callback from ioqueue when packet is sent */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent);

/* Callback from ioqueue when connect completes */
static void on_connect_complete(pj_ioqueue_key_t *key, 
                                pj_status_t status);

/* TLS keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e);

/*
 * Common function to create TLS transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tls_create( struct tls_listener *listener,
			       pj_pool_t *pool,
			       pj_sock_t sock, pj_bool_t is_server,
			       const pj_sockaddr_in *local,
			       const pj_sockaddr_in *remote,
			       struct tls_transport **p_tls)
{
    struct tls_transport *tls;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback tls_callback;
    int rc;
    pj_status_t status;
    

    PJ_ASSERT_RETURN(sock != PJ_INVALID_SOCKET, PJ_EINVAL);


    if (pool == NULL) {
	pool = pjsip_endpt_create_pool(listener->endpt, "tls",
				       POOL_TP_INIT, POOL_TP_INC);
	PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);
    }    

    /*
     * Create and initialize basic transport structure.
     */
    tls = PJ_POOL_ZALLOC_T(pool, struct tls_transport);
    tls->sock = sock;
    tls->is_server = is_server;
    /*tls->listener = listener;*/
    pj_list_init(&tls->delayed_list);
    tls->base.pool = pool;
    tls->setting.timeout = listener->setting.timeout;
    pj_strdup(pool, &tls->setting.server_name, 
    	      &listener->setting.server_name);

    pj_ansi_snprintf(tls->base.obj_name, PJ_MAX_OBJ_NAME, 
		     (is_server ? "tlss%p" :"tlsc%p"), tls);

    /* Initialize transport reference counter to 1 */
    status = pj_atomic_create(pool, 0, &tls->base.ref_cnt);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    status = pj_lock_create_recursive_mutex(pool, "tls", &tls->base.lock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tls->base.key.type = PJSIP_TRANSPORT_TLS;
    pj_memcpy(&tls->base.key.rem_addr, remote, sizeof(pj_sockaddr_in));
    tls->base.type_name = "tls";
    tls->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TLS);

    tls->base.info = (char*) pj_pool_alloc(pool, 64);
    pj_ansi_snprintf(tls->base.info, 64, "TLS to %s:%d",
		     pj_inet_ntoa(remote->sin_addr), 
		     (int)pj_ntohs(remote->sin_port));

    tls->base.addr_len = sizeof(pj_sockaddr_in);
    pj_memcpy(&tls->base.local_addr, local, sizeof(pj_sockaddr_in));
    sockaddr_to_host_port(pool, &tls->base.local_name, local);
    sockaddr_to_host_port(pool, &tls->base.remote_name, remote);

    tls->base.endpt = listener->endpt;
    tls->base.tpmgr = listener->tpmgr;
    tls->base.send_msg = &tls_send_msg;
    tls->base.do_shutdown = &tls_shutdown;
    tls->base.destroy = &tls_destroy_transport;

    /* Create SSL connection object */
    tls->ssl = SSL_new(listener->ctx);
    if (tls->ssl == NULL) {
	ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			 "Error creating SSL connection object");
	status = PJSIP_TLS_ESSLCONN;
	goto on_error;
    }

    /* Associate network socket with SSL connection object */
    rc = SSL_set_fd(tls->ssl, (int)sock);
    if (rc != 1) {
	ssl_report_error(tls->base.obj_name, 4, PJ_SUCCESS,
			 "Error calling SSL_set_fd");
	status = PJSIP_TLS_ESSLCONN;
	goto on_error;
    }

    /* Register socket to ioqueue */
    pj_bzero(&tls_callback, sizeof(pj_ioqueue_callback));
    tls_callback.on_read_complete = &on_read_complete;
    tls_callback.on_write_complete = &on_write_complete;
    tls_callback.on_connect_complete = &on_connect_complete;

    ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    status = pj_ioqueue_register_sock(pool, ioqueue, sock, 
				      tls, &tls_callback, &tls->key);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tls->base);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tls->is_registered = PJ_TRUE;

    /* Initialize keep-alive timer */
    tls->ka_timer.user_data = (void*) tls;
    tls->ka_timer.cb = &tls_keep_alive_timer;


    /* Done setting up basic transport. */
    *p_tls = tls;

    PJ_LOG(4,(tls->base.obj_name, "TLS %s transport created",
	      (tls->is_server ? "server" : "client")));

    return PJ_SUCCESS;

on_error:
    tls_destroy(&tls->base, status);
    return status;
}


/* Flush all delayed transmision once the socket is connected. 
 * Return non-zero if pending transmission list is empty after
 * the function returns.
 */
static pj_bool_t tls_flush_pending_tx(struct tls_transport *tls)
{
    pj_bool_t empty;

    pj_lock_acquire(tls->base.lock);
    while (!pj_list_empty(&tls->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pjsip_tx_data *tdata;
	pj_ioqueue_op_key_t *op_key;
	pj_ssize_t size;
	pj_status_t status;

	pending_tx = tls->delayed_list.next;

	tdata = pending_tx->tdata_op_key->tdata;
	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	/* send the txdata */
	status = ssl_write(tls, tdata);

	/* On EWOULDBLOCK, suspend further transmissions */
	if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
	    break;
	}

	/* tdata has been transmitted (successfully or with failure).
	 * In any case, remove it from pending transmission list.
	 */
	pj_list_erase(pending_tx);

	/* Notify callback */
	if (status == PJ_SUCCESS)
	    size = tdata->buf.cur - tdata->buf.start;
	else
	    size = -status;

	on_write_complete(tls->key, op_key, size);

    }

    empty = pj_list_empty(&tls->delayed_list);

    pj_lock_release(tls->base.lock);

    return empty;
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
    ++tls->is_closing;

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

	on_write_complete(tls->key, op_key, -reason);
    }

    if (tls->rdata.tp_info.pool) {
	pj_pool_release(tls->rdata.tp_info.pool);
	tls->rdata.tp_info.pool = NULL;
    }

    if (tls->key) {
	pj_ioqueue_unregister(tls->key);
	tls->key = NULL;
	tls->sock = PJ_INVALID_SOCKET;
    }

    if (tls->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tls->sock);
	tls->sock = PJ_INVALID_SOCKET;
    }

    if (tls->base.lock) {
	pj_lock_destroy(tls->base.lock);
	tls->base.lock = NULL;
    }

    if (tls->base.ref_cnt) {
	pj_atomic_destroy(tls->base.ref_cnt);
	tls->base.ref_cnt = NULL;
    }

    if (tls->ssl) {
	SSL_free(tls->ssl);
	tls->ssl = NULL;
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
    pj_status_t status;

    /* Init rdata */
    pool = pjsip_endpt_create_pool(tls->base.endpt,
				   "rtd%p",
				   PJSIP_POOL_RDATA_LEN,
				   PJSIP_POOL_RDATA_INC);
    if (!pool) {
	ssl_report_error(tls->base.obj_name, 4, PJ_ENOMEM, 
			 "Unable to create pool for listener rxdata");
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

    /* Here's the real trick with OpenSSL.
     * Since asynchronous socket operation with OpenSSL uses select() like
     * mechanism, it's not really compatible with PJLIB's ioqueue. So to
     * make them "talk" together, we simulate select() by using MSG_PEEK
     * when we call pj_ioqueue_recv().
     */
    size = 1;
    status = pj_ioqueue_recv(tls->key, &tls->rdata.tp_info.op_key.op_key,
			     tls->rdata.pkt_info.packet, &size,
			     PJ_IOQUEUE_ALWAYS_ASYNC | PJ_MSG_PEEK);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	ssl_report_error(tls->base.obj_name, 4, status,
			 "ioqueue_recv() error");
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
					pjsip_transport **p_transport)
{
    struct tls_listener *listener;
    struct tls_transport *tls;
    pj_sock_t sock;
    pj_sockaddr_in local_addr;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(factory && mgr && endpt && rem_addr &&
		     addr_len && p_transport, PJ_EINVAL);

    /* Check that address is a sockaddr_in */
    PJ_ASSERT_RETURN(rem_addr->addr.sa_family == pj_AF_INET() &&
		     addr_len == sizeof(pj_sockaddr_in), PJ_EINVAL);


    listener = (struct tls_listener*)factory;

    
    /* Create socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Bind to any port */
    status = pj_sock_bind_in(sock, 0, 0);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Get the local port */
    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(sock, &local_addr, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Initially set the address from the listener's address */
    local_addr.sin_addr.s_addr = 
	((pj_sockaddr_in*)&listener->factory.local_addr)->sin_addr.s_addr;

    /* Create the transport descriptor */
    status = tls_create(listener, NULL, sock, PJ_FALSE, &local_addr, 
			(pj_sockaddr_in*)rem_addr, &tls);
    if (status != PJ_SUCCESS)
	return status;


    /* Start asynchronous connect() operation */
    tls->has_pending_connect = PJ_TRUE;
    status = pj_ioqueue_connect(tls->key, rem_addr, sizeof(pj_sockaddr_in));
    if (status == PJ_SUCCESS) {

	/* Immediate socket connect() ! */
	tls->has_pending_connect = PJ_FALSE;

	/* Perform SSL_connect() */
	status = ssl_connect(tls);
	if (status != PJ_SUCCESS) {
	    tls_destroy(&tls->base, status);
	    return status;
	}

    } else if (status != PJ_EPENDING) {
	tls_destroy(&tls->base, status);
	return status;
    }

    /* Update (again) local address, just in case local address currently
     * set is different now that asynchronous connect() is started.
     */
    addr_len = sizeof(pj_sockaddr_in);
    if (pj_sock_getsockname(tls->sock, &local_addr, &addr_len)==PJ_SUCCESS) {
	pj_sockaddr_in *tp_addr = (pj_sockaddr_in*)&tls->base.local_addr;

	/* Some systems (like old Win32 perhaps) may not set local address
	 * properly before socket is fully connected.
	 */
	if (tp_addr->sin_addr.s_addr != local_addr.sin_addr.s_addr &&
	    local_addr.sin_addr.s_addr != 0) 
	{
	    tp_addr->sin_addr.s_addr = local_addr.sin_addr.s_addr;
	    tp_addr->sin_port = local_addr.sin_port;
	    sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
				  &local_addr);
	}
    }

    if (tls->has_pending_connect) {
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
 * This callback is called by ioqueue when pending accept() operation has
 * completed.
 */
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status)
{
    struct tls_listener *listener;
    struct tls_transport *tls;
    struct pending_accept *accept_op;
    int err_cnt = 0;

    listener = (struct tls_listener*) pj_ioqueue_get_user_data(key);
    accept_op = (struct pending_accept*) op_key;

    /*
     * Loop while there is immediate connection or when there is error.
     */
    do {
	if (status == PJ_EPENDING) {
	    /*
	     * This can only happen when this function is called during
	     * initialization to kick off asynchronous accept().
	     */

	} else if (status != PJ_SUCCESS) {

	    /*
	     * Error in accept().
	     */
	    ssl_report_error(listener->factory.obj_name, 4, status,
			     "Error in asynchronous accept() completion");

	    /*
	     * Prevent endless accept() error loop by limiting the
	     * number of consecutive errors. Once the number of errors
	     * is equal to maximum, we treat this as permanent error, and
	     * we stop the accept() operation.
	     */
	    ++err_cnt;
	    if (err_cnt >= 10) {
		PJ_LOG(1, (listener->factory.obj_name, 
			   "Too many errors, listener stopping"));
	    }

	} else {
	    pj_pool_t *pool;
	    struct pending_accept *new_op;

	    if (sock == PJ_INVALID_SOCKET) {
		sock = accept_op->new_sock;
	    }

	    if (sock == PJ_INVALID_SOCKET) {
		pj_assert(!"Should not happen. status should be error");
		goto next_accept;
	    }

	    PJ_LOG(4,(listener->factory.obj_name, 
		      "TLS listener %.*s:%d: got incoming TCP connection "
		      "from %s:%d, sock=%d",
		      (int)listener->factory.addr_name.host.slen,
		      listener->factory.addr_name.host.ptr,
		      listener->factory.addr_name.port,
		      pj_inet_ntoa(accept_op->remote_addr.sin_addr),
		      pj_ntohs(accept_op->remote_addr.sin_port),
		      sock));

	    /* Create new accept_opt */
	    pool = pjsip_endpt_create_pool(listener->endpt, "tlss%p", 
					   POOL_TP_INIT, POOL_TP_INC);
	    new_op = PJ_POOL_ZALLOC_T(pool, struct pending_accept);
	    new_op->pool = pool;
	    new_op->listener = listener;
	    new_op->index = accept_op->index;
	    pj_ioqueue_op_key_init(&new_op->op_key, sizeof(new_op->op_key));
	    listener->accept_op[accept_op->index] = new_op;

	    /* 
	     * Incoming connections!
	     * Create TLS transport for the new socket.
	     */
	    status = tls_create( listener, accept_op->pool, sock, PJ_TRUE,
				 &accept_op->local_addr, 
				 &accept_op->remote_addr, &tls);
	    if (status == PJ_SUCCESS) {
		/* Complete SSL_accept() */
		status = ssl_accept(tls);
	    }

	    if (status == PJ_SUCCESS) {
		/* Start asynchronous read from the socket */
		status = tls_start_read(tls);
	    }

	    if (status != PJ_SUCCESS) {
		ssl_report_error(tls->base.obj_name, 4, status,
				 "Error creating incoming TLS transport");
		pjsip_transport_shutdown(&tls->base);

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

	    accept_op = new_op;
	}

next_accept:
	/*
	 * Start the next asynchronous accept() operation.
	 */
	accept_op->addr_len = sizeof(pj_sockaddr_in);
	accept_op->new_sock = PJ_INVALID_SOCKET;

	status = pj_ioqueue_accept(listener->key, 
				   &accept_op->op_key,
				   &accept_op->new_sock,
				   &accept_op->local_addr,
				   &accept_op->remote_addr,
				   &accept_op->addr_len);

	/*
	 * Loop while we have immediate connection or when there is error.
	 */

    } while (status != PJ_EPENDING);
}


/* 
 * Callback from ioqueue when packet is sent.
 */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent)
{
    struct tls_transport *tls;
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    tls = (struct tls_transport*) pj_ioqueue_get_user_data(key);
    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
	/*
	 * Notify sip_transport.c that packet has been sent.
	 */
	if (bytes_sent == 0)
	    bytes_sent = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

	tdata_op_key->callback(&tls->base, tdata_op_key->token, bytes_sent);
    }

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	ssl_report_error(tls->base.obj_name, 4, -bytes_sent, 
		         "TLS send() error");

	status = (bytes_sent == 0) ? PJ_STATUS_FROM_OS(OSERR_ENOTCONN) :
				     -bytes_sent;
	if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
	pjsip_transport_shutdown(&tls->base);
    } else {
	/* Mark last activity */
	pj_gettimeofday(&tls->last_activity);
    }
}


/* Add tdata to pending list */
static void add_pending_tx(struct tls_transport *tls,
			   pjsip_tx_data *tdata)
{
    struct delayed_tdata *delayed_tdata;

    delayed_tdata = PJ_POOL_ALLOC_T(tdata->pool, struct delayed_tdata);
    delayed_tdata->tdata_op_key = &tdata->op_key;
    pj_list_push_back(&tls->delayed_list, delayed_tdata);
}


/* 
 * This callback is called by transport manager to send SIP message 
 */
static pj_status_t tls_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				void (*callback)(pjsip_transport *transport,
						 void *token, 
						 pj_ssize_t sent_bytes))
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
	    /*
	     * connect() is still in progress. Put the transmit data to
	     * the delayed list.
	     */
	    add_pending_tx(tls, tdata);
	    status = PJ_EPENDING;

	    /* Prevent ssl_write() to be called below */
	    delayed = PJ_TRUE;
	}

	pj_lock_release(tls->base.lock);
    } 
    
    if (!delayed) {

	pj_bool_t no_pending_tx;

	/* Make sure that we've flushed pending tx first so that
	 * stream is in order.
	 */
	no_pending_tx = tls_flush_pending_tx(tls);

	/* Send data immediately with SSL_write() if we don't have
	 * pending data in our list.
	 */
	if (no_pending_tx) {

	    status = ssl_write(tls, tdata);

	    /* On EWOULDBLOCK, put this tdata in the list */
	    if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
		add_pending_tx(tls, tdata);
		status = PJ_EPENDING;
	    }

	    if (status != PJ_EPENDING) {
		/* Not pending (could be immediate success or error) */
		tdata->op_key.tdata = NULL;

		/* Shutdown transport on closure/errors */
		if (status != PJ_SUCCESS) {
		    size = -status;

		    ssl_report_error(tls->base.obj_name, 4, status,
				     "TLS send() error");

		    if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
		    pjsip_transport_shutdown(&tls->base);
		}
	    }

	} else {
	    /* We have pending data in our list, so queue the txdata
	     * in the pending tx list.
	     */
	    add_pending_tx(tls, tdata);
	    status = PJ_EPENDING;
	}
    }

    return status;
}


/* 
 * This callback is called by transport manager to shutdown transport.
 * This normally is only used by UDP transport.
 */
static pj_status_t tls_shutdown(pjsip_transport *transport)
{
    struct tls_transport *tls = (struct tls_transport*)transport;

    /* Shutdown SSL */
    if (!tls->ssl_shutdown_called) {
	/* shutdown SSL */
	SSL_shutdown(tls->ssl);
	tls->ssl_shutdown_called = PJ_TRUE;

	/* Stop keep-alive timer. */
	if (tls->ka_timer.id) {
	    pjsip_endpt_cancel_timer(tls->base.endpt, &tls->ka_timer);
	    tls->ka_timer.id = PJ_FALSE;
	}

	PJ_LOG(4,(transport->obj_name, "TLS transport shutdown"));
    }

    return PJ_SUCCESS;
}


/* 
 * Callback from ioqueue that an incoming data is received from the socket.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read_unused)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    pjsip_rx_data_op_key *rdata_op_key = (pjsip_rx_data_op_key*) op_key;
    pjsip_rx_data *rdata = rdata_op_key->rdata;
    struct tls_transport *tls = 
	(struct tls_transport*)rdata->tp_info.transport;
    int i;
    pj_status_t status;

    /* Don't do anything if transport is closing. */
    if (tls->is_closing) {
	tls->is_closing++;
	return;
    }


    /* Recall that we use MSG_PEEK when calling ioqueue_recv(), so
     * when this callback is called, data has not actually been read
     * from socket buffer.
     */

    for (i=0;; ++i) {
	pj_uint32_t flags;

	/* Read data from SSL connection */
	status = ssl_read(tls);

	if (status == PJ_SUCCESS) {
	    /*
	     * We have packet!
	     */
	    pj_size_t size_eaten;

	    /* Mark last activity */
	    pj_gettimeofday(&tls->last_activity);

	    /* Init pkt_info part. */
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
	    if (size_eaten>0 && size_eaten<(pj_size_t)rdata->pkt_info.len) {
		pj_memmove(rdata->pkt_info.packet,
			   rdata->pkt_info.packet + size_eaten,
			   rdata->pkt_info.len - size_eaten);
	    }
	    
	    rdata->pkt_info.len -= size_eaten;

	} else if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {

	    /* Ignore EWOULDBLOCK error (?) */

	} else {

	    /* For other errors, treat as transport being closed */
	    ssl_report_error(tls->base.obj_name, 4, status,
			     "Error reading SSL stream");
	    
	    /* We can not destroy the transport since high level objects may
	     * still keep reference to this transport. So we can only 
	     * instruct transport manager to gracefully start the shutdown
	     * procedure for this transport.
	     */
	    if (tls->close_reason==PJ_SUCCESS) 
		tls->close_reason = status;
	    pjsip_transport_shutdown(&tls->base);

	    return;
	}

	/* Reset pool. */
	pj_pool_reset(rdata->tp_info.pool);

	/* If we have pending data in SSL buffer, read it. */
	if (SSL_pending(tls->ssl)) {

	    /* Check that we have enough space in buffer */
	    if (rdata->pkt_info.len >= PJSIP_MAX_PKT_LEN-1) {
		PJ_LOG(4,(tls->base.obj_name, 
			  "Incoming packet dropped from tls:%.*s:%d "
			  "because it's too big (%d bytes)",
			  (int)tls->base.remote_name.host.slen,
			  tls->base.remote_name.host.ptr,
			  tls->base.remote_name.port,
			  rdata->pkt_info.len));
		rdata->pkt_info.len = 0;
	    }

	    continue;
	}

	/* If we've reached maximum number of packets received on a single
	 * poll, force the next reading to be asynchronous.
	 */
	if (i >= MAX_IMMEDIATE_PACKET) {
	    /* Receive quota reached. Force ioqueue_recv() to 
	     * return PJ_EPENDING 
	     */
	    flags = PJ_IOQUEUE_ALWAYS_ASYNC;
	} else {
	    /* This doesn't work too well when IOCP is used, as it seems that
	     * IOCP refuses to do MSG_PEEK when WSARecv() is called without
	     * OVERLAP structure. With OpenSSL it gives this error:
	     * error:1408F10B:SSL routines:SSL3_GET_RECORD:wrong version 
	     * number
	     *
	     * flags = 0 
	     */
	    flags = PJ_IOQUEUE_ALWAYS_ASYNC;
	}

	/* Read next packet from the network. Remember, we need to use
	 * MSG_PEEK or otherwise the packet will be eaten by us!
	 */
	bytes_read_unused = 1;
	status = pj_ioqueue_recv(key, op_key, 
				 rdata->pkt_info.packet+rdata->pkt_info.len,
				 &bytes_read_unused, flags | PJ_MSG_PEEK);

	if (status == PJ_SUCCESS) {

	    /* Continue loop. */
	    pj_assert(i < MAX_IMMEDIATE_PACKET);

	} else if (status == PJ_EPENDING) {
	    break;

	} else {
	    /* Socket error */

	    /* We can not destroy the transport since high level objects may
	     * still keep reference to this transport. So we can only 
	     * instruct transport manager to gracefully start the shutdown
	     * procedure for this transport.
	     */
	    if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
	    pjsip_transport_shutdown(&tls->base);

	    return;
	}
    }
}


/* 
 * Callback from ioqueue when asynchronous connect() operation completes.
 */
static void on_connect_complete(pj_ioqueue_key_t *key, 
                                pj_status_t status)
{
    struct tls_transport *tls;
    pj_sockaddr_in addr;
    int addrlen;

    tls = (struct tls_transport*) pj_ioqueue_get_user_data(key);

    /* Check connect() status */
    if (status != PJ_SUCCESS) {

	/* Mark that pending connect() operation has completed. */
	tls->has_pending_connect = PJ_FALSE;

	ssl_report_error(tls->base.obj_name, 4, status,
			 "Error connecting to %.*s:%d",
			 (int)tls->base.remote_name.host.slen,
			 tls->base.remote_name.host.ptr,
			 tls->base.remote_name.port);

	/* Cancel all delayed transmits */
	while (!pj_list_empty(&tls->delayed_list)) {
	    struct delayed_tdata *pending_tx;
	    pj_ioqueue_op_key_t *op_key;

	    pending_tx = tls->delayed_list.next;
	    pj_list_erase(pending_tx);

	    op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	    on_write_complete(tls->key, op_key, -status);
	}

	/* We can not destroy the transport since high level objects may
	 * still keep reference to this transport. So we can only 
	 * instruct transport manager to gracefully start the shutdown
	 * procedure for this transport.
	 */
	if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
	pjsip_transport_shutdown(&tls->base);
	return;
    }

    PJ_LOG(4,(tls->base.obj_name, 
	      "TCP transport %.*s:%d is connected to %.*s:%d",
	      (int)tls->base.local_name.host.slen,
	      tls->base.local_name.host.ptr,
	      tls->base.local_name.port,
	      (int)tls->base.remote_name.host.slen,
	      tls->base.remote_name.host.ptr,
	      tls->base.remote_name.port));


    /* Update (again) local address, just in case local address currently
     * set is different now that the socket is connected (could happen
     * on some systems, like old Win32 probably?).
     */
    addrlen = sizeof(pj_sockaddr_in);
    if (pj_sock_getsockname(tls->sock, &addr, &addrlen)==PJ_SUCCESS) {
	pj_sockaddr_in *tp_addr = (pj_sockaddr_in*)&tls->base.local_addr;

	if (tp_addr->sin_addr.s_addr != addr.sin_addr.s_addr) {
	    tp_addr->sin_addr.s_addr = addr.sin_addr.s_addr;
	    tp_addr->sin_port = addr.sin_port;
	    sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
				  tp_addr);
	}
    }

    /* Perform SSL_connect() */
    status = ssl_connect(tls);
    if (status != PJ_SUCCESS) {

	/* Cancel all delayed transmits */
	while (!pj_list_empty(&tls->delayed_list)) {
	    struct delayed_tdata *pending_tx;
	    pj_ioqueue_op_key_t *op_key;

	    pending_tx = tls->delayed_list.next;
	    pj_list_erase(pending_tx);

	    op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	    on_write_complete(tls->key, op_key, -status);
	}

	if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
	pjsip_transport_shutdown(&tls->base);
	return;
    }

    /* Mark that pending connect() operation has completed. */
    tls->has_pending_connect = PJ_FALSE;

    /* Start pending read */
    status = tls_start_read(tls);
    if (status != PJ_SUCCESS) {

	/* Cancel all delayed transmits */
	while (!pj_list_empty(&tls->delayed_list)) {
	    struct delayed_tdata *pending_tx;
	    pj_ioqueue_op_key_t *op_key;

	    pending_tx = tls->delayed_list.next;
	    pj_list_erase(pending_tx);

	    op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	    on_write_complete(tls->key, op_key, -status);
	}


	/* We can not destroy the transport since high level objects may
	 * still keep reference to this transport. So we can only 
	 * instruct transport manager to gracefully start the shutdown
	 * procedure for this transport.
	 */
	if (tls->close_reason==PJ_SUCCESS) tls->close_reason = status;
	pjsip_transport_shutdown(&tls->base);
	return;
    }

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

}


/* Transport keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
    struct tls_transport *tls = (struct tls_transport*) e->user_data;
    const pj_str_t ka_data = PJSIP_TLS_KEEP_ALIVE_DATA;
    pj_time_val delay;
    pj_time_val now;
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
	      (int)ka_data.slen, (int)tls->base.remote_name.host.slen,
	      tls->base.remote_name.host.ptr,
	      tls->base.remote_name.port));

    /* Send the data */
    status = ssl_write_bytes(tls, ka_data.ptr, (int)ka_data.slen, 
			     "keep-alive");
    if (status != PJ_SUCCESS && 
	status != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK))
    {
	ssl_report_error(tls->base.obj_name, 1, status,
			 "Error sending keep-alive packet");
	return;
    }

    /* Register next keep-alive */
    delay.sec = PJSIP_TLS_KEEP_ALIVE_INTERVAL;
    delay.msec = 0;

    pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
			       &delay);
    tls->ka_timer.id = PJ_TRUE;
}

#endif	/* PJSIP_HAS_TLS_TRANSPORT */

