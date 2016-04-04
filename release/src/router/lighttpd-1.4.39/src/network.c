#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "connections.h"
#include "plugin.h"
#include "joblist.h"
#include "configfile.h"

#include "network_backends.h"
#include "sys-mmap.h"
#include "sys-socket.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define DBE 0

#ifdef USE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/rand.h>
# ifndef OPENSSL_NO_DH
#  include <openssl/dh.h>
# endif
# include <openssl/bn.h>

# if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#  ifndef OPENSSL_NO_ECDH
# include <openssl/ecdh.h>
#  endif
# endif
#endif

#ifdef USE_OPENSSL
static void ssl_info_callback(const SSL *ssl, int where, int ret) {
	UNUSED(ret);

	if (0 != (where & SSL_CB_HANDSHAKE_START)) {
		connection *con = SSL_get_app_data(ssl);
		++con->renegotiations;
	}
}
#endif

static handler_t network_server_handle_fdevent(server *srv, void *context, int revents) {
	server_socket *srv_socket = (server_socket *)context;
	connection *con;
	int loops = 0;

	UNUSED(context);

	if (0 == (revents & FDEVENT_IN)) {
		log_error_write(srv, __FILE__, __LINE__, "sdd",
				"strange event for server socket",
				srv_socket->fd,
				revents);
		return HANDLER_ERROR;
	}

	/* accept()s at most 100 connections directly
	 *
	 * we jump out after 100 to give the waiting connections a chance */
	for (loops = 0; loops < 100 && NULL != (con = connection_accept(srv, srv_socket)); loops++) {
		handler_t r;

		connection_state_machine(srv, con);

		switch(r = plugins_call_handle_joblist(srv, con)) {
		case HANDLER_FINISHED:
		case HANDLER_GO_ON:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "d", r);
			break;
		}
	}
	return HANDLER_GO_ON;
}

#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
static int network_ssl_servername_callback(SSL *ssl, int *al, server *srv) {
	const char *servername;
	connection *con = (connection *) SSL_get_app_data(ssl);
	UNUSED(al);

	buffer_copy_string(con->uri.scheme, "https");

	if (NULL == (servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))) {
#if 0
		/* this "error" just means the client didn't support it */
		log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
				"failed to get TLS server name");
#endif
		return SSL_TLSEXT_ERR_NOACK;
	}
	buffer_copy_string(con->tlsext_server_name, servername);
	buffer_to_lower(con->tlsext_server_name);

	/* Sometimes this is still set, confusing COMP_HTTP_HOST */
	buffer_reset(con->uri.authority);

	config_cond_cache_reset(srv, con);
	config_setup_connection(srv, con);

	config_patch_connection(srv, con, COMP_SERVER_SOCKET);
	config_patch_connection(srv, con, COMP_HTTP_SCHEME);
	config_patch_connection(srv, con, COMP_HTTP_HOST);

	if (NULL == con->conf.ssl_pemfile_x509 || NULL == con->conf.ssl_pemfile_pkey) {
		/* x509/pkey available <=> pemfile was set <=> pemfile got patched: so this should never happen, unless you nest $SERVER["socket"] */
		log_error_write(srv, __FILE__, __LINE__, "ssb", "SSL:",
			"no certificate/private key for TLS server name", con->tlsext_server_name);
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	}

	/* first set certificate! setting private key checks whether certificate matches it */
	if (!SSL_use_certificate(ssl, con->conf.ssl_pemfile_x509)) {
		log_error_write(srv, __FILE__, __LINE__, "ssb:s", "SSL:",
			"failed to set certificate for TLS server name", con->tlsext_server_name,
			ERR_error_string(ERR_get_error(), NULL));
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	}

	if (!SSL_use_PrivateKey(ssl, con->conf.ssl_pemfile_pkey)) {
		log_error_write(srv, __FILE__, __LINE__, "ssb:s", "SSL:",
			"failed to set private key for TLS server name", con->tlsext_server_name,
			ERR_error_string(ERR_get_error(), NULL));
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	}

	if (con->conf.ssl_verifyclient) {
		if (NULL == con->conf.ssl_ca_file_cert_names) {
			log_error_write(srv, __FILE__, __LINE__, "ssb:s", "SSL:",
				"can't verify client without ssl.ca-file for TLS server name", con->tlsext_server_name,
				ERR_error_string(ERR_get_error(), NULL));
			return SSL_TLSEXT_ERR_ALERT_FATAL;
		}

		SSL_set_client_CA_list(ssl, SSL_dup_CA_list(con->conf.ssl_ca_file_cert_names));
		/* forcing verification here is really not that useful - a client could just connect without SNI */
		SSL_set_verify(
			ssl,
			SSL_VERIFY_PEER | (con->conf.ssl_verifyclient_enforce ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0),
			NULL
		);
		SSL_set_verify_depth(ssl, con->conf.ssl_verifyclient_depth);
	}

	return SSL_TLSEXT_ERR_OK;
}
#endif

static int network_server_init(server *srv, buffer *host_token, specific_config *s) {
	int val;
	socklen_t addr_len;
	server_socket *srv_socket;
	char *sp;
	unsigned int port = 0;
	const char *host;
	buffer *b;
	int is_unix_domain_socket = 0;
	int fd;
	
#ifdef __WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		    /* Tell the user that we could not find a usable */
		    /* WinSock DLL.                                  */
		    return -1;
	}
#endif

	srv_socket = calloc(1, sizeof(*srv_socket));
	srv_socket->fd = -1;
	srv_socket->fde_ndx = -1;

	srv_socket->srv_token = buffer_init();
	buffer_copy_buffer(srv_socket->srv_token, host_token);

	b = buffer_init();
	buffer_copy_buffer(b, host_token);

	/* ipv4:port
	 * [ipv6]:port
	 */
	if (NULL == (sp = strrchr(b->ptr, ':'))) {
		log_error_write(srv, __FILE__, __LINE__, "sb", "value of $SERVER[\"socket\"] has to be \"ip:port\".", b);

		goto error_free_socket;
	}

	host = b->ptr;

	/* check for [ and ] */
	if (b->ptr[0] == '[' && *(sp-1) == ']') {
		*(sp-1) = '\0';
		host++;

		s->use_ipv6 = 1;
	}

	*(sp++) = '\0';

	port = strtol(sp, NULL, 10);

	if (host[0] == '/') {
		/* host is a unix-domain-socket */
		is_unix_domain_socket = 1;
	} else if (port == 0 || port > 65535) {
		log_error_write(srv, __FILE__, __LINE__, "sd", "port out of range:", port);

		goto error_free_socket;
	}

	if (*host == '\0') host = NULL;

	if (is_unix_domain_socket) {
#ifdef HAVE_SYS_UN_H

		srv_socket->addr.plain.sa_family = AF_UNIX;

		if (-1 == (srv_socket->fd = socket(srv_socket->addr.plain.sa_family, SOCK_STREAM, 0))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "socket failed:", strerror(errno));
			goto error_free_socket;
		}
#else
		log_error_write(srv, __FILE__, __LINE__, "s",
				"ERROR: Unix Domain sockets are not supported.");
		goto error_free_socket;
#endif
	}

#ifdef HAVE_IPV6
	if (s->use_ipv6) {
		srv_socket->addr.plain.sa_family = AF_INET6;

		if (-1 == (srv_socket->fd = socket(srv_socket->addr.plain.sa_family, SOCK_STREAM, IPPROTO_TCP))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "socket failed:", strerror(errno));
			goto error_free_socket;
		}
	}
#endif

	if (srv_socket->fd == -1) {
		srv_socket->addr.plain.sa_family = AF_INET;
		if (-1 == (srv_socket->fd = socket(srv_socket->addr.plain.sa_family, SOCK_STREAM, IPPROTO_TCP))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "socket failed:", strerror(errno));
			goto error_free_socket;
		}
	}

	/* set FD_CLOEXEC now, fdevent_fcntl_set is called later; needed for pipe-logger forks */
	fd_close_on_exec(srv_socket->fd);

	/* */
	srv->cur_fds = srv_socket->fd;

	val = 1;
	if (setsockopt(srv_socket->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "socketsockopt(SO_REUSEADDR) failed:", strerror(errno));
		goto error_free_socket;
	}

	switch(srv_socket->addr.plain.sa_family) {
#ifdef HAVE_IPV6
	case AF_INET6:
		memset(&srv_socket->addr, 0, sizeof(struct sockaddr_in6));
		srv_socket->addr.ipv6.sin6_family = AF_INET6;
		if (host == NULL) {
			srv_socket->addr.ipv6.sin6_addr = in6addr_any;
			log_error_write(srv, __FILE__, __LINE__, "s", "warning: please use server.use-ipv6 only for hostnames, not without server.bind / empty address; your config will break if the kernel default for IPV6_V6ONLY changes");
		} else {
			struct addrinfo hints, *res;
			int r;

			if (s->set_v6only) {
				val = 1;
				if (-1 == setsockopt(srv_socket->fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "socketsockopt(IPV6_V6ONLY) failed:", strerror(errno));
					goto error_free_socket;
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "s", "warning: server.set-v6only will be removed soon, update your config to have different sockets for ipv4 and ipv6");
			}

			memset(&hints, 0, sizeof(hints));

			hints.ai_family   = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			if (0 != (r = getaddrinfo(host, NULL, &hints, &res))) {
				log_error_write(srv, __FILE__, __LINE__,
						"sssss", "getaddrinfo failed: ",
						gai_strerror(r), "'", host, "'");

				goto error_free_socket;
			}

			memcpy(&(srv_socket->addr), res->ai_addr, res->ai_addrlen);

			freeaddrinfo(res);
		}
		srv_socket->addr.ipv6.sin6_port = htons(port);
		addr_len = sizeof(struct sockaddr_in6);
		break;
#endif
	case AF_INET:
		memset(&srv_socket->addr, 0, sizeof(struct sockaddr_in));
		srv_socket->addr.ipv4.sin_family = AF_INET;
		if (host == NULL) {
			srv_socket->addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		} else {
			struct hostent *he;
			if (NULL == (he = gethostbyname(host))) {
				log_error_write(srv, __FILE__, __LINE__,
						"sds", "gethostbyname failed: ",
						h_errno, host);
				goto error_free_socket;
			}

			if (he->h_addrtype != AF_INET) {
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-type != AF_INET: ", he->h_addrtype);
				goto error_free_socket;
			}

			if (he->h_length != sizeof(struct in_addr)) {
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-length != sizeof(in_addr): ", he->h_length);
				goto error_free_socket;
			}

			memcpy(&(srv_socket->addr.ipv4.sin_addr.s_addr), he->h_addr_list[0], he->h_length);
		}
		srv_socket->addr.ipv4.sin_port = htons(port);

		addr_len = sizeof(struct sockaddr_in);

		break;
	case AF_UNIX:
		memset(&srv_socket->addr, 0, sizeof(struct sockaddr_un));
		srv_socket->addr.un.sun_family = AF_UNIX;
		{
			size_t hostlen = strlen(host) + 1;
			if (hostlen > sizeof(srv_socket->addr.un.sun_path)) {
				log_error_write(srv, __FILE__, __LINE__, "sS", "unix socket filename too long:", host);
				goto error_free_socket;
			}
			memcpy(srv_socket->addr.un.sun_path, host, hostlen);

#if defined(SUN_LEN)
			addr_len = SUN_LEN(&srv_socket->addr.un);
#else
			/* stevens says: */
			addr_len = hostlen + sizeof(srv_socket->addr.un.sun_family);
#endif
		}

		/* check if the socket exists and try to connect to it. */
		if (-1 != (fd = connect(srv_socket->fd, (struct sockaddr *) &(srv_socket->addr), addr_len))) {
			close(fd);

			log_error_write(srv, __FILE__, __LINE__, "ss",
				"server socket is still in use:",
				host);


			goto error_free_socket;
		}

		/* connect failed */
		switch(errno) {
		case ECONNREFUSED:
			unlink(host);
			break;
		case ENOENT:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sds",
				"testing socket failed:",
				host, strerror(errno));

			goto error_free_socket;
		}

		break;
	default:
		goto error_free_socket;
	}

	if (0 != bind(srv_socket->fd, (struct sockaddr *) &(srv_socket->addr), addr_len)) {
		switch(srv_socket->addr.plain.sa_family) {
		case AF_UNIX:
			log_error_write(srv, __FILE__, __LINE__, "sds",
					"can't bind to socket:",
					host, strerror(errno));
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssds",
					"can't bind to port:",
					host, port, strerror(errno));
			break;
		}
		goto error_free_socket;
	}

	if (-1 == listen(srv_socket->fd, 128 * 8)) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "listen failed: ", strerror(errno));
		goto error_free_socket;
	}

	if (s->ssl_enabled) {
#ifdef USE_OPENSSL
		if (NULL == (srv_socket->ssl_ctx = s->ssl_ctx)) {
			log_error_write(srv, __FILE__, __LINE__, "s", "ssl.pemfile has to be set");
			goto error_free_socket;
		}
#else

		log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
				"ssl requested but openssl support is not compiled in");

		goto error_free_socket;
#endif
#ifdef TCP_DEFER_ACCEPT
	} else if (s->defer_accept) {
		int v = s->defer_accept;
		if (-1 == setsockopt(srv_socket->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v, sizeof(v))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "can't set TCP_DEFER_ACCEPT: ", strerror(errno));
		}
#endif
	} else {
#ifdef SO_ACCEPTFILTER
		/* FreeBSD accf_http filter */
		struct accept_filter_arg afa;
		memset(&afa, 0, sizeof(afa));
		strcpy(afa.af_name, "httpready");
		if (setsockopt(srv_socket->fd, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa)) < 0) {
			if (errno != ENOENT) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "can't set accept-filter 'httpready': ", strerror(errno));
			}
		}
#endif
	}

	srv_socket->is_ssl = s->ssl_enabled;

	if (srv->srv_sockets.size == 0) {
		srv->srv_sockets.size = 4;
		srv->srv_sockets.used = 0;
		srv->srv_sockets.ptr = malloc(srv->srv_sockets.size * sizeof(server_socket*));
	} else if (srv->srv_sockets.used == srv->srv_sockets.size) {
		srv->srv_sockets.size += 4;
		srv->srv_sockets.ptr = realloc(srv->srv_sockets.ptr, srv->srv_sockets.size * sizeof(server_socket*));
	}

	srv->srv_sockets.ptr[srv->srv_sockets.used++] = srv_socket;

	buffer_free(b);

	return 0;

error_free_socket:
	if (srv_socket->fd != -1) {
		/* check if server fd are already registered */
		if (srv_socket->fde_ndx != -1) {
			fdevent_event_del(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd);
			fdevent_unregister(srv->ev, srv_socket->fd);
		}

		close(srv_socket->fd);
	}
	buffer_free(srv_socket->srv_token);
	free(srv_socket);

	buffer_free(b);

	return -1;
}

int network_close(server *srv) {
	size_t i;
	for (i = 0; i < srv->srv_sockets.used; i++) {
		server_socket *srv_socket = srv->srv_sockets.ptr[i];

		if (srv_socket->fd != -1) {
			/* check if server fd are already registered */
			if (srv_socket->fde_ndx != -1) {
				fdevent_event_del(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd);
				fdevent_unregister(srv->ev, srv_socket->fd);
			}

			close(srv_socket->fd);
		}

		buffer_free(srv_socket->srv_token);

		free(srv_socket);
	}

	free(srv->srv_sockets.ptr);

	return 0;
}

typedef enum {
	NETWORK_BACKEND_UNSET,
	NETWORK_BACKEND_WRITE,
	NETWORK_BACKEND_WRITEV,
	NETWORK_BACKEND_SENDFILE,
} network_backend_t;

#ifdef USE_OPENSSL
static X509* x509_load_pem_file(server *srv, const char *file) {
	BIO *in;
	X509 *x = NULL;

	in = BIO_new(BIO_s_file());
	if (NULL == in) {
		log_error_write(srv, __FILE__, __LINE__, "S", "SSL: BIO_new(BIO_s_file()) failed");
		goto error;
	}

	if (BIO_read_filename(in,file) <= 0) {
		log_error_write(srv, __FILE__, __LINE__, "SSS", "SSL: BIO_read_filename('", file,"') failed");
		goto error;
	}
	x = PEM_read_bio_X509(in, NULL, NULL, NULL);

	if (NULL == x) {
		log_error_write(srv, __FILE__, __LINE__, "SSS", "SSL: couldn't read X509 certificate from '", file,"'");
		goto error;
	}

	BIO_free(in);
	return x;

error:
	if (NULL != in) BIO_free(in);
	return NULL;
}

static EVP_PKEY* evp_pkey_load_pem_file(server *srv, const char *file) {
	BIO *in;
	EVP_PKEY *x = NULL;

	in=BIO_new(BIO_s_file());
	if (NULL == in) {
		log_error_write(srv, __FILE__, __LINE__, "s", "SSL: BIO_new(BIO_s_file()) failed");
		goto error;
	}

	if (BIO_read_filename(in,file) <= 0) {
		log_error_write(srv, __FILE__, __LINE__, "SSS", "SSL: BIO_read_filename('", file,"') failed");
		goto error;
	}
	x = PEM_read_bio_PrivateKey(in, NULL, NULL, NULL);

	if (NULL == x) {
		log_error_write(srv, __FILE__, __LINE__, "SSS", "SSL: couldn't read private key from '", file,"'");
		goto error;
	}

	BIO_free(in);
	return x;

error:
	if (NULL != in) BIO_free(in);
	return NULL;
}

static int network_openssl_load_pemfile(server *srv, size_t ndx) {
	specific_config *s = srv->config_storage[ndx];

#ifdef OPENSSL_NO_TLSEXT
	{
		data_config *dc = (data_config *)srv->config_context->data[ndx];
		if ((ndx > 0 && (COMP_SERVER_SOCKET != dc->comp || dc->cond != CONFIG_COND_EQ))
			|| !s->ssl_enabled) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
					"ssl.pemfile only works in SSL socket binding context as openssl version does not support TLS extensions");
			return -1;
		}
	}
#endif

	if (NULL == (s->ssl_pemfile_x509 = x509_load_pem_file(srv, s->ssl_pemfile->ptr))) return -1;
	if (NULL == (s->ssl_pemfile_pkey = evp_pkey_load_pem_file(srv, s->ssl_pemfile->ptr))) return -1;

	if (!X509_check_private_key(s->ssl_pemfile_x509, s->ssl_pemfile_pkey)) {
		log_error_write(srv, __FILE__, __LINE__, "sssb", "SSL:",
				"Private key does not match the certificate public key, reason:",
				ERR_error_string(ERR_get_error(), NULL),
				s->ssl_pemfile);
		return -1;
	}

	return 0;
}
#endif

int network_init(server *srv) {
	buffer *b;
	size_t i, j;
	network_backend_t backend;

#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#ifndef OPENSSL_NO_ECDH
	EC_KEY *ecdh;
	int nid;
#endif
#endif

#ifdef USE_OPENSSL
# ifndef OPENSSL_NO_DH
	DH *dh;
# endif
	BIO *bio;

       /* 1024-bit MODP Group with 160-bit prime order subgroup (RFC5114)
	* -----BEGIN DH PARAMETERS-----
	* MIIBDAKBgQCxC4+WoIDgHd6S3l6uXVTsUsmfvPsGo8aaap3KUtI7YWBz4oZ1oj0Y
	* mDjvHi7mUsAT7LSuqQYRIySXXDzUm4O/rMvdfZDEvXCYSI6cIZpzck7/1vrlZEc4
	* +qMaT/VbzMChUa9fDci0vUW/N982XBpl5oz9p21NpwjfH7K8LkpDcQKBgQCk0cvV
	* w/00EmdlpELvuZkF+BBN0lisUH/WQGz/FCZtMSZv6h5cQVZLd35pD1UE8hMWAhe0
	* sBuIal6RVH+eJ0n01/vX07mpLuGQnQ0iY/gKdqaiTAh6CR9THb8KAWm2oorWYqTR
	* jnOvoy13nVkY0IvIhY9Nzvl8KiSFXm7rIrOy5QICAKA=
	* -----END DH PARAMETERS-----
	*/

	static const unsigned char dh1024_p[]={
		0xB1,0x0B,0x8F,0x96,0xA0,0x80,0xE0,0x1D,0xDE,0x92,0xDE,0x5E,
		0xAE,0x5D,0x54,0xEC,0x52,0xC9,0x9F,0xBC,0xFB,0x06,0xA3,0xC6,
		0x9A,0x6A,0x9D,0xCA,0x52,0xD2,0x3B,0x61,0x60,0x73,0xE2,0x86,
		0x75,0xA2,0x3D,0x18,0x98,0x38,0xEF,0x1E,0x2E,0xE6,0x52,0xC0,
		0x13,0xEC,0xB4,0xAE,0xA9,0x06,0x11,0x23,0x24,0x97,0x5C,0x3C,
		0xD4,0x9B,0x83,0xBF,0xAC,0xCB,0xDD,0x7D,0x90,0xC4,0xBD,0x70,
		0x98,0x48,0x8E,0x9C,0x21,0x9A,0x73,0x72,0x4E,0xFF,0xD6,0xFA,
		0xE5,0x64,0x47,0x38,0xFA,0xA3,0x1A,0x4F,0xF5,0x5B,0xCC,0xC0,
		0xA1,0x51,0xAF,0x5F,0x0D,0xC8,0xB4,0xBD,0x45,0xBF,0x37,0xDF,
		0x36,0x5C,0x1A,0x65,0xE6,0x8C,0xFD,0xA7,0x6D,0x4D,0xA7,0x08,
		0xDF,0x1F,0xB2,0xBC,0x2E,0x4A,0x43,0x71,
	};

	static const unsigned char dh1024_g[]={
		0xA4,0xD1,0xCB,0xD5,0xC3,0xFD,0x34,0x12,0x67,0x65,0xA4,0x42,
		0xEF,0xB9,0x99,0x05,0xF8,0x10,0x4D,0xD2,0x58,0xAC,0x50,0x7F,
		0xD6,0x40,0x6C,0xFF,0x14,0x26,0x6D,0x31,0x26,0x6F,0xEA,0x1E,
		0x5C,0x41,0x56,0x4B,0x77,0x7E,0x69,0x0F,0x55,0x04,0xF2,0x13,
		0x16,0x02,0x17,0xB4,0xB0,0x1B,0x88,0x6A,0x5E,0x91,0x54,0x7F,
		0x9E,0x27,0x49,0xF4,0xD7,0xFB,0xD7,0xD3,0xB9,0xA9,0x2E,0xE1,
		0x90,0x9D,0x0D,0x22,0x63,0xF8,0x0A,0x76,0xA6,0xA2,0x4C,0x08,
		0x7A,0x09,0x1F,0x53,0x1D,0xBF,0x0A,0x01,0x69,0xB6,0xA2,0x8A,
		0xD6,0x62,0xA4,0xD1,0x8E,0x73,0xAF,0xA3,0x2D,0x77,0x9D,0x59,
		0x18,0xD0,0x8B,0xC8,0x85,0x8F,0x4D,0xCE,0xF9,0x7C,0x2A,0x24,
		0x85,0x5E,0x6E,0xEB,0x22,0xB3,0xB2,0xE5,
	};
#endif

	struct nb_map {
		network_backend_t nb;
		const char *name;
	} network_backends[] = {
		/* lowest id wins */
#if defined USE_SENDFILE
		{ NETWORK_BACKEND_SENDFILE,   "sendfile" },
#endif
#if defined USE_LINUX_SENDFILE
		{ NETWORK_BACKEND_SENDFILE,   "linux-sendfile" },
#endif
#if defined USE_FREEBSD_SENDFILE
		{ NETWORK_BACKEND_SENDFILE,   "freebsd-sendfile" },
#endif
#if defined USE_SOLARIS_SENDFILEV
		{ NETWORK_BACKEND_SENDFILE,   "solaris-sendfilev" },
#endif
#if defined USE_WRITEV
		{ NETWORK_BACKEND_WRITEV,     "writev" },
#endif
		{ NETWORK_BACKEND_WRITE,      "write" },
		{ NETWORK_BACKEND_UNSET,       NULL }
	};
	
#ifdef USE_OPENSSL
	/* load SSL certificates */
	for (i = 0; i < srv->config_context->used; i++) {
		specific_config *s = srv->config_storage[i];
#ifndef SSL_OP_NO_COMPRESSION
# define SSL_OP_NO_COMPRESSION 0
#endif
		long ssloptions =
			SSL_OP_ALL | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_NO_COMPRESSION;

		//- 20160204 Sungmin add
		ssloptions &= ~SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
		//ssloptions |= SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
		
		Cdbg(DBE, "ssloptions = %d", ssloptions);
		
		if (buffer_string_is_empty(s->ssl_pemfile) && buffer_string_is_empty(s->ssl_ca_file)) continue;

		if (srv->ssl_is_init == 0) {
			SSL_load_error_strings();
			SSL_library_init();
			OpenSSL_add_all_algorithms();
			srv->ssl_is_init = 1;

			if (0 == RAND_status()) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						"not enough entropy in the pool");
				return -1;
			}
		}

		if (!buffer_string_is_empty(s->ssl_pemfile)) {
#ifdef OPENSSL_NO_TLSEXT
			data_config *dc = (data_config *)srv->config_context->data[i];
			if (COMP_HTTP_HOST == dc->comp) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						"can't use ssl.pemfile with $HTTP[\"host\"], openssl version does not support TLS extensions");
				return -1;
			}
#endif
			if (network_openssl_load_pemfile(srv, i)) return -1;
		}


		if (!buffer_string_is_empty(s->ssl_ca_file)) {
			s->ssl_ca_file_cert_names = SSL_load_client_CA_file(s->ssl_ca_file->ptr);
			if (NULL == s->ssl_ca_file_cert_names) {
				log_error_write(srv, __FILE__, __LINE__, "ssb", "SSL:",
						ERR_error_string(ERR_get_error(), NULL), s->ssl_ca_file);
			}
		}

		if (buffer_string_is_empty(s->ssl_pemfile) || !s->ssl_enabled) continue;

		if (NULL == (s->ssl_ctx = SSL_CTX_new(SSLv23_server_method()))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
					ERR_error_string(ERR_get_error(), NULL));
			return -1;
		}

		/* completely useless identifier; required for client cert verification to work with sessions */
		if (0 == SSL_CTX_set_session_id_context(s->ssl_ctx, (const unsigned char*) CONST_STR_LEN("lighttpd"))) {
			log_error_write(srv, __FILE__, __LINE__, "ss:s", "SSL:",
				"failed to set session context",
				ERR_error_string(ERR_get_error(), NULL));
			return -1;
		}

		if (s->ssl_empty_fragments) {
#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
			ssloptions &= ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;
#else
			ssloptions &= ~0x00000800L; /* hardcode constant */
			log_error_write(srv, __FILE__, __LINE__, "ss", "WARNING: SSL:",
					"'insert empty fragments' not supported by the openssl version used to compile lighttpd with");
#endif
		}

		SSL_CTX_set_options(s->ssl_ctx, ssloptions);
		SSL_CTX_set_info_callback(s->ssl_ctx, ssl_info_callback);

		if (!s->ssl_use_sslv2) {
			/* disable SSLv2 */
			if (!(SSL_OP_NO_SSLv2 & SSL_CTX_set_options(s->ssl_ctx, SSL_OP_NO_SSLv2))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return -1;
			}
		}

		if (!s->ssl_use_sslv3) {
			/* disable SSLv3 */
			if (!(SSL_OP_NO_SSLv3 & SSL_CTX_set_options(s->ssl_ctx, SSL_OP_NO_SSLv3))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return -1;
			}
		}

		if (!buffer_string_is_empty(s->ssl_cipher_list)) {
			/* Disable support for low encryption ciphers */
			if (SSL_CTX_set_cipher_list(s->ssl_ctx, s->ssl_cipher_list->ptr) != 1) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return -1;
			}

			if (s->ssl_honor_cipher_order) {
				SSL_CTX_set_options(s->ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
			}
		}

#ifndef OPENSSL_NO_DH
		/* Support for Diffie-Hellman key exchange */
		if (!buffer_string_is_empty(s->ssl_dh_file)) {
			/* DH parameters from file */
			bio = BIO_new_file((char *) s->ssl_dh_file->ptr, "r");
			if (bio == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL: Unable to open file", s->ssl_dh_file->ptr);
				return -1;
			}
			dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
			BIO_free(bio);
			if (dh == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL: PEM_read_bio_DHparams failed", s->ssl_dh_file->ptr);
				return -1;
			}
		} else {
			/* Default DH parameters from RFC5114 */
			dh = DH_new();
			if (dh == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "s", "SSL: DH_new () failed");
				return -1;
			}
			dh->p = BN_bin2bn(dh1024_p,sizeof(dh1024_p), NULL);
			dh->g = BN_bin2bn(dh1024_g,sizeof(dh1024_g), NULL);
			dh->length = 160;
			if ((dh->p == NULL) || (dh->g == NULL)) {
				DH_free(dh);
				log_error_write(srv, __FILE__, __LINE__, "s", "SSL: BN_bin2bn () failed");
				return -1;
			}
		}
		SSL_CTX_set_tmp_dh(s->ssl_ctx,dh);
		SSL_CTX_set_options(s->ssl_ctx,SSL_OP_SINGLE_DH_USE);
		DH_free(dh);
#else
		if (!buffer_string_is_empty(s->ssl_dh_file)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "SSL: openssl compiled without DH support, can't load parameters from", s->ssl_dh_file->ptr);
		}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#ifndef OPENSSL_NO_ECDH
		/* Support for Elliptic-Curve Diffie-Hellman key exchange */
		if (!buffer_string_is_empty(s->ssl_ec_curve)) {
			/* OpenSSL only supports the "named curves" from RFC 4492, section 5.1.1. */
			nid = OBJ_sn2nid((char *) s->ssl_ec_curve->ptr);
			if (nid == 0) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL: Unknown curve name", s->ssl_ec_curve->ptr);
				return -1;
			}
		} else {
			/* Default curve */
			nid = OBJ_sn2nid("prime256v1");
		}
		ecdh = EC_KEY_new_by_curve_name(nid);
		if (ecdh == NULL) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "SSL: Unable to create curve", s->ssl_ec_curve->ptr);
			return -1;
		}
		SSL_CTX_set_tmp_ecdh(s->ssl_ctx,ecdh);
		SSL_CTX_set_options(s->ssl_ctx,SSL_OP_SINGLE_ECDH_USE);
		EC_KEY_free(ecdh);
#endif
#endif

		/* load all ssl.ca-files specified in the config into each SSL_CTX to be prepared for SNI */
		for (j = 0; j < srv->config_context->used; j++) {
			specific_config *s1 = srv->config_storage[j];

			if (!buffer_string_is_empty(s1->ssl_ca_file)) {
				if (1 != SSL_CTX_load_verify_locations(s->ssl_ctx, s1->ssl_ca_file->ptr, NULL)) {
					log_error_write(srv, __FILE__, __LINE__, "ssb", "SSL:",
							ERR_error_string(ERR_get_error(), NULL), s1->ssl_ca_file);
					return -1;
				}
			}
		}

		if (s->ssl_verifyclient) {
			if (NULL == s->ssl_ca_file_cert_names) {
				log_error_write(srv, __FILE__, __LINE__, "s",
					"SSL: You specified ssl.verifyclient.activate but no ca_file"
				);
				return -1;
			}
			SSL_CTX_set_client_CA_list(s->ssl_ctx, SSL_dup_CA_list(s->ssl_ca_file_cert_names));
			SSL_CTX_set_verify(
				s->ssl_ctx,
				SSL_VERIFY_PEER | (s->ssl_verifyclient_enforce ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0),
				NULL
			);
			SSL_CTX_set_verify_depth(s->ssl_ctx, s->ssl_verifyclient_depth);
		}

		if (SSL_CTX_use_certificate(s->ssl_ctx, s->ssl_pemfile_x509) < 0) {
			log_error_write(srv, __FILE__, __LINE__, "ssb", "SSL:",
					ERR_error_string(ERR_get_error(), NULL), s->ssl_pemfile);
			return -1;
		}

		if (SSL_CTX_use_PrivateKey(s->ssl_ctx, s->ssl_pemfile_pkey) < 0) {
			log_error_write(srv, __FILE__, __LINE__, "ssb", "SSL:",
					ERR_error_string(ERR_get_error(), NULL), s->ssl_pemfile);
			return -1;
		}

		if (SSL_CTX_check_private_key(s->ssl_ctx) != 1) {
			log_error_write(srv, __FILE__, __LINE__, "sssb", "SSL:",
					"Private key does not match the certificate public key, reason:",
					ERR_error_string(ERR_get_error(), NULL),
					s->ssl_pemfile);
			return -1;
		}
		SSL_CTX_set_default_read_ahead(s->ssl_ctx, 1);
		SSL_CTX_set_mode(s->ssl_ctx, SSL_CTX_get_mode(s->ssl_ctx) | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

# ifndef OPENSSL_NO_TLSEXT
		if (!SSL_CTX_set_tlsext_servername_callback(s->ssl_ctx, network_ssl_servername_callback) ||
		    !SSL_CTX_set_tlsext_servername_arg(s->ssl_ctx, srv)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
					"failed to initialize TLS servername callback, openssl library does not support TLS servername extension");
			return -1;
		}
# endif
	}
#endif
	
	b = buffer_init();

	buffer_copy_buffer(b, srv->srvconf.bindhost);
	buffer_append_string_len(b, CONST_STR_LEN(":"));
	buffer_append_int(b, srv->srvconf.port);

	if (0 != network_server_init(srv, b, srv->config_storage[0])) {
		buffer_free(b);
		return -1;
	}
	buffer_free(b);

#ifdef USE_OPENSSL
	srv->network_ssl_backend_write = network_write_chunkqueue_openssl;
#endif

	/* get a usefull default */
	backend = network_backends[0].nb;

	/* match name against known types */
	if (!buffer_string_is_empty(srv->srvconf.network_backend)) {
		for (i = 0; network_backends[i].name; i++) {
			/**/
			if (buffer_is_equal_string(srv->srvconf.network_backend, network_backends[i].name, strlen(network_backends[i].name))) {
				backend = network_backends[i].nb;
				break;
			}
		}
		if (NULL == network_backends[i].name) {
			/* we don't know it */

			log_error_write(srv, __FILE__, __LINE__, "sb",
					"server.network-backend has a unknown value:",
					srv->srvconf.network_backend);

			return -1;
		}
	}

	switch(backend) {
	case NETWORK_BACKEND_WRITE:
		srv->network_backend_write = network_write_chunkqueue_write;
		break;
#if defined(USE_WRITEV)
	case NETWORK_BACKEND_WRITEV:
		srv->network_backend_write = network_write_chunkqueue_writev;
		break;
#endif
#if defined(USE_SENDFILE)
	case NETWORK_BACKEND_SENDFILE:		
		srv->network_backend_write = network_write_chunkqueue_sendfile;
		break;
#endif
	default:
		return -1;
	}

	/* check for $SERVER["socket"] */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		specific_config *s = srv->config_storage[i];

		/* not our stage */
		if (COMP_SERVER_SOCKET != dc->comp) continue;

		if (dc->cond != CONFIG_COND_EQ) continue;

		/* check if we already know this socket,
		 * if yes, don't init it */
		for (j = 0; j < srv->srv_sockets.used; j++) {
			if (buffer_is_equal(srv->srv_sockets.ptr[j]->srv_token, dc->string)) {
				break;
			}
		}

		if (j == srv->srv_sockets.used) {
			if (0 != network_server_init(srv, dc->string, s)) return -1;
		}
	}

	return 0;
}

int network_register_fdevents(server *srv) {
	size_t i;

	if (-1 == fdevent_reset(srv->ev)) {
		return -1;
	}

	/* register fdevents after reset */
	for (i = 0; i < srv->srv_sockets.used; i++) {
		server_socket *srv_socket = srv->srv_sockets.ptr[i];

		fdevent_register(srv->ev, srv_socket->fd, network_server_handle_fdevent, srv_socket);
		fdevent_event_set(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd, FDEVENT_IN);
	}
	return 0;
}

int network_write_chunkqueue(server *srv, connection *con, chunkqueue *cq, off_t max_bytes) {
	int ret = -1;
	off_t written = 0;
#ifdef TCP_CORK
	int corked = 0;
#endif
	server_socket *srv_socket = con->srv_socket;
	
	if (con->conf.global_kbytes_per_second) {
		off_t limit = con->conf.global_kbytes_per_second * 1024 - *(con->conf.global_bytes_per_second_cnt_ptr);
		if (limit <= 0) {
			/* we reached the global traffic limit */

			con->traffic_limit_reached = 1;
			joblist_append(srv, con);

			return 1;
		} else {
			if (max_bytes > limit) max_bytes = limit;
		}
	}

	if (con->conf.kbytes_per_second) {
		off_t limit = con->conf.kbytes_per_second * 1024 - con->bytes_written_cur_second;
		if (limit <= 0) {
			/* we reached the traffic limit */

			con->traffic_limit_reached = 1;
			joblist_append(srv, con);

			return 1;
		} else {
			if (max_bytes > limit) max_bytes = limit;
		}
	}

	written = cq->bytes_out;

#ifdef TCP_CORK
	/* Linux: put a cork into the socket as we want to combine the write() calls
	 * but only if we really have multiple chunks
	 */
	if (cq->first && cq->first->next) {
		corked = 1;
		setsockopt(con->fd, IPPROTO_TCP, TCP_CORK, &corked, sizeof(corked));
	}
#endif

	if (srv_socket->is_ssl) {
#ifdef USE_OPENSSL		
		ret = srv->network_ssl_backend_write(srv, con, con->ssl, cq, max_bytes);
#endif
	} else {
		ret = srv->network_backend_write(srv, con, con->fd, cq, max_bytes);
	}

	if (ret >= 0) {
		chunkqueue_remove_finished_chunks(cq);
		ret = chunkqueue_is_empty(cq) ? 0 : 1;
	}

#ifdef TCP_CORK
	if (corked) {
		corked = 0;
		setsockopt(con->fd, IPPROTO_TCP, TCP_CORK, &corked, sizeof(corked));
	}
#endif

	written = cq->bytes_out - written;
	con->bytes_written += written;
	con->bytes_written_cur_second += written;

	*(con->conf.global_bytes_per_second_cnt_ptr) += written;

	return ret;
}
