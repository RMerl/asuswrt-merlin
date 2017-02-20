/*

	Minimal CyaSSL/OpenSSL Helper
	Copyright (C) 2006-2009 Jonathan Zarate
	Copyright (C) 2010 Fedor Kozhevnikov

	Licensed under GNU GPL v2 or later

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#define _dprintf(args...)	while (0) {}

const char *allowedCiphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA:!SRP-RSA-3DES-EDE-CBC-SHA";

typedef struct {
	SSL* ssl;
	int sd;
} mssl_cookie_t;

static SSL_CTX* ctx;

static inline void mssl_print_err(SSL* ssl)
{
	ERR_print_errors_fp(stderr);
}

static inline void mssl_cleanup(int err)
{
	if (err) mssl_print_err(NULL);
	SSL_CTX_free(ctx);
	ctx = NULL;
}

static ssize_t mssl_read(void *cookie, char *buf, size_t len)
{
	_dprintf("%s()\n", __FUNCTION__);

	mssl_cookie_t *kuki = cookie;
	int total = 0;
	int n, err;

	do {
		n = SSL_read(kuki->ssl, &(buf[total]), len - total);
		_dprintf("SSL_read(max=%d) returned %d\n", len - total, n);

		err = SSL_get_error(kuki->ssl, n);
		switch (err) {
		case SSL_ERROR_NONE:
			total += n;
			break;
		case SSL_ERROR_ZERO_RETURN:
			total += n;
			goto OUT;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			break;
		default:
			_dprintf("%s(): SSL error %d\n", __FUNCTION__, err);
			mssl_print_err(kuki->ssl);
			if (total == 0) total = -1;
			goto OUT;
		}
	} while ((len - total > 0) && SSL_pending(kuki->ssl));

OUT:
	_dprintf("%s() returns %d\n", __FUNCTION__, total);
	return total;
}

static ssize_t mssl_write(void *cookie, const char *buf, size_t len)
{
	_dprintf("%s()\n", __FUNCTION__);

	mssl_cookie_t *kuki = cookie;
	int total = 0;
	int n, err;

	while (total < len) {
		n = SSL_write(kuki->ssl, &(buf[total]), len - total);
		_dprintf("SSL_write(max=%d) returned %d\n", len - total, n);
		err = SSL_get_error(kuki->ssl, n);
		switch (err) {
		case SSL_ERROR_NONE:
			total += n;
			break;
		case SSL_ERROR_ZERO_RETURN:
			total += n;
			goto OUT;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			break;
		default:
			_dprintf("%s(): SSL error %d\n", __FUNCTION__, err);
			mssl_print_err(kuki->ssl);
			if (total == 0) total = -1;
			goto OUT;
		}
	}

OUT:
	_dprintf("%s() returns %d\n", __FUNCTION__, total);
	return total;
}

static int mssl_seek(void *cookie, __offmax_t *pos, int whence)
{
	_dprintf("%s()\n", __FUNCTION__);
	errno = EIO;
	return -1;
}

static int mssl_close(void *cookie)
{
	_dprintf("%s()\n", __FUNCTION__);

	mssl_cookie_t *kuki = cookie;
	if (!kuki) return 0;

	if (kuki->ssl) {
		SSL_shutdown(kuki->ssl);
		SSL_free(kuki->ssl);
	}

	free(kuki);
	return 0;
}

static const cookie_io_functions_t mssl = {
	mssl_read, mssl_write, mssl_seek, mssl_close
};

static FILE *_ssl_fopen(int sd, int client)
{
	int r = 0;
	int err;
	mssl_cookie_t *kuki;
	FILE *f;

	_dprintf("%s()\n", __FUNCTION__);
	//fprintf(stderr,"[ssl_fopen] ssl_fopen start!\n"); // tmp test

	if ((kuki = calloc(1, sizeof(*kuki))) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	kuki->sd = sd;

	// Create new SSL object
	if ((kuki->ssl = SSL_new(ctx)) == NULL) {
		fprintf(stderr,"[ssl_fopen] SSL_new failed!\n"); // tmp test
		_dprintf("%s: SSL_new failed\n", __FUNCTION__);
		goto ERROR;
	}

	// SSL structure for client authenticate after SSL_new()
	SSL_set_verify(kuki->ssl, SSL_VERIFY_NONE, NULL);
	SSL_set_mode(kuki->ssl, SSL_MODE_AUTO_RETRY);

	// Bind the socket to SSL structure
	// kuki->ssl : SSL structure
	// kuki->sd  : socket_fd

	// Setup EC support
#ifdef NID_X9_62_prime256v1
	EC_KEY *ecdh = NULL;
	if (ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) {
		SSL_CTX_set_tmp_ecdh(ctx, ecdh);
		EC_KEY_free(ecdh);
	}
#endif

	// Setup available ciphers
	if (SSL_CTX_set_cipher_list(ctx, allowedCiphers) != 1) {
		goto ERROR;
	}

	// Enforce our desired cipher order, disable obsolete protocols
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 |
		SSL_OP_NO_SSLv3 |
		SSL_OP_CIPHER_SERVER_PREFERENCE |
		SSL_OP_SAFARI_ECDHE_ECDSA_BUG);

	r = SSL_set_fd(kuki->ssl, kuki->sd);
	//fprintf(stderr,"[ssl_fopen] set_fd=%d\n", r); // tmp test

	if (!client){
		// Do the SSL Handshake
		r = SSL_accept(kuki->ssl);
	}
	else{
		// Connect to the server, SSL layer
		r = SSL_connect(kuki->ssl);
	}

	//fprintf(stderr,"[ssl_fopen] client=%d, r=%d\n", client, r); // tmp test
	// r = 0 show unknown CA, but we don't have any CA, so ignore.
	if (r < 0){
		// Check error in connect or accept
		err = SSL_get_error(kuki->ssl, r);
		fprintf(stderr,"[ssl_fopen] SSL error #%d in client=%d\n", err, client); // tmp test
		mssl_print_err(kuki->ssl);
		goto ERROR;
	}
	
	_dprintf("SSL connection using %s cipher\n", SSL_get_cipher(kuki->ssl));

	if ((f = fopencookie(kuki, "r+", mssl)) == NULL) {
		_dprintf("%s: fopencookie failed\n", __FUNCTION__);
		goto ERROR;
	}

	//fprintf(stderr,"[ssl_fopen] SUCCESS!\n", r); // tmp test
	_dprintf("%s() success\n", __FUNCTION__);
	return f;

ERROR:
	fprintf(stderr,"[ssl_fopen] ERROR!\n"); // tmp test
	mssl_close(kuki);
	return NULL;
}

FILE *ssl_server_fopen(int sd)
{
	_dprintf("%s()\n", __FUNCTION__);
	return _ssl_fopen(sd, 0);
}

FILE *ssl_client_fopen(int sd)
{
	_dprintf("%s()\n", __FUNCTION__);
	return _ssl_fopen(sd, 1);
}

int mssl_init(char *cert, char *priv)
{
	_dprintf("%s()\n", __FUNCTION__);

	int server = (cert != NULL);
	//fprintf(stderr,"[ssl_init] server=%d\n", server); // tmp test

	// Register error strings for libcrypto and libssl functions
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	// Create the new CTX with the method 
	// If server=1, use TLSv1_server_method() or SSLv23_server_method()
	// else 	use TLSv1_client_method() or SSLv23_client_method()
	ctx = SSL_CTX_new(server ? SSLv23_server_method() : SSLv23_client_method());

	if (!ctx) {
		fprintf(stderr,"[ssl_init] SSL_CTX_new() failed\n"); // tmp test
		_dprintf("SSL_CTX_new() failed\n");
		mssl_print_err(NULL);
		return 0;
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

	if (server) {
		// Set the certificate to be used
		_dprintf("SSL_CTX_use_certificate_chain_file(%s)\n", cert);
		if (SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) {
			_dprintf("SSL_CTX_use_certificate_chain_file() failed\n");
			mssl_cleanup(1);
			return 0;
		}
		// Indicate the key file to be used
		_dprintf("SSL_CTX_use_PrivateKey_file(%s)\n", priv);
		if (SSL_CTX_use_PrivateKey_file(ctx, priv, SSL_FILETYPE_PEM) <= 0) {
			_dprintf("SSL_CTX_use_PrivateKey_file() failed\n");
			mssl_cleanup(1);
			return 0;
		}
		// Make sure the key and certificate file match
		if (!SSL_CTX_check_private_key(ctx)) {
			_dprintf("Private key does not match the certificate public key\n");
			mssl_cleanup(0);
			return 0;
		}
	}

	fprintf(stderr,"[ssl_init] success!!\n"); // tmp test
	_dprintf("%s() success\n", __FUNCTION__);
	return 1;
}
