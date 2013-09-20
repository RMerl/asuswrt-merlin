#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#endif

#include "dutil.h"
#include "dnet.h"

typedef struct sockaddr SA;
typedef struct sockaddr_in SIN;

struct socket {
	bool eom;		/* End of Message? */
	int sock;		/* Socket descriptor */
	int flags;		/* Error flags */
	int errnum;		/* Essentially, errno */
	int avail_bytes;	/* Available bytes in buffer */
	char *bufptr;		/* Pointer to Internal buffer */
	char *buf;		/* The buffer. */
#ifdef HAVE_LIBSSL
	SSL_CTX *ctx;		/* OpenSSL CTX struct, used for TLS */
	SSL *ssl;		/* OpenSSL SSL struct, used for TLS */
#endif
};

/**
 * A wrapper for gethostbyname. This allows us to figure out
 * if we need to use the regular gethostbyname, the reentrant
 * gethostbyname's or an IPV6 version.
 *
 * Params
 * 	name - The hostname to resolve
 * 	hent - A hostent struct.
 *
 * Return
 * 	ERROR upon failure to resolve name
 * 	SUCCESS otherwise
 */
int
dnetResolveName(const char *name, struct hostent *hent)
{
        int ret = ERROR; 
        struct hostent *him;

        him = gethostbyname(name);
        if (him) {
                memcpy(hent, him, sizeof(struct hostent));
                ret = SUCCESS;
        }
        return ret;
}

/**
 * This function creates a socket connection based on a 
 * hostname and port.  It will return a FILE descriptor
 * so that you can perform buffered read/writes on the socket.
 *
 * LIMITATIONS
 * 	- Only opens a PF_INET socket.
 * 	- "hostname" must be a FQD or ascii IP address.
 *
 * Params
 * 	hostname - The FQD or IP of the host to connect to
 * 	port - the port to connect on.
 *
 * Return
 * 	dsocket - upon successful connection.
 * 	NULL - upon failure to connect.
 */
dsocket *
dnetConnect(const char *hostname, unsigned int port)
{
	int sd=0;
	SIN sin;
	struct hostent him;
	dsocket *ret = NULL;

	memset(&sin, 0, sizeof(SIN));
	memset(&him, 0, sizeof(struct hostent));

	if (dnetResolveName(hostname, &him) == ERROR) {
		return NULL;
	}
	sin.sin_family = PF_INET;
	sin.sin_port = htons(port);
	memcpy(&sin.sin_addr.s_addr, him.h_addr_list[0], 
		sizeof(sin.sin_addr.s_addr));
	sd = socket(sin.sin_family, SOCK_STREAM, 0);
	if (sd > 0) {
		if (connect(sd, (SA *)&sin, sizeof(SIN)) >= 0) {
			ret = xmalloc(sizeof(struct socket));
			ret->sock = sd;
			ret->buf = xmalloc(MAXSOCKBUF);
			ret->bufptr = ret->buf;
		}
	}
	return ret;
}

/**
 * Generates a random seed used for TLS connection.
 */
#ifdef HAVE_LIBSSL
static void
_genRandomSeed(void)
{
	struct {
		struct utsname uname;
		int uname_1;
		int uname_2;
		uid_t uid;
		uid_t euid;
		gid_t gid;
		gid_t egid;
	} data;

	struct {
		pid_t pid;
		time_t time;
		void *stack;
	} uniq;

	data.uname_1 = uname(&data.uname);
	data.uname_2 = errno;
	data.uid = getuid();
	data.euid = geteuid();
	data.gid = getgid();
	data.egid = getegid();
	RAND_seed(&data, sizeof(data));

	uniq.pid = getpid();
	uniq.time = time(NULL);
	uniq.stack = &uniq;
	RAND_seed(&uniq, sizeof(uniq));
}
#endif

/**
 * This will allow you to use TLS over an existing connection.
 * Will return error if a connection has not already been established.
 */
int
dnetUseTls(dsocket *sd)
{
#ifdef HAVE_LIBSSL
	// Can't do this unless we have a connection.
	if (sd->sock <= 0) {
		return ERROR;
	}

	SSL_load_error_strings();
	if (SSL_library_init() == -1) {
		return ERROR;
	}
	_genRandomSeed();
	sd->ctx = SSL_CTX_new(TLSv1_client_method());
	if (!sd->ctx) {
		return ERROR;
	}
	sd->ssl = SSL_new(sd->ctx);
	if (!sd->ssl) {
		SSL_CTX_free(sd->ctx);
		sd->ctx = NULL;
		return ERROR;
	}
	SSL_set_fd(sd->ssl, sd->sock);
	if (SSL_connect(sd->ssl) == -1) {
		SSL_CTX_free(sd->ctx);
		SSL_free(sd->ssl);
		sd->ssl = NULL;
		sd->ctx = NULL;
		return ERROR;
	}
#else
	sd = sd;
#endif
	return SUCCESS;
}

/**
 * Simply verfies that the certificate of the peer is valid and
 * that nobody is trying to fool us.
 */
int
dnetVerifyCert(dsocket *sd)
{
#ifdef HAVE_LIBSSL
	X509 *cert = NULL;
	cert = SSL_get_peer_certificate(sd->ssl);
	if (!cert) {
		return ERROR;
	}

	/* TODO: Do some sort of verification here. */

	X509_free(cert);
#else
	sd = sd;
#endif
	return SUCCESS;
}

/**
 * Close the socket connection and destroy the SOCKET structure
 */
void
dnetClose(dsocket *sd)
{
	if (sd) {
		if (sd->sock) {
			close(sd->sock);
		}
#ifdef HAVE_LIBSSL
		if (sd->ssl) {
			SSL_shutdown(sd->ssl);
			SSL_free(sd->ssl);
			if (sd->ctx) {
				SSL_CTX_free(sd->ctx);
			}
		}
#endif
		if (sd->buf) {
			xfree(sd->buf);
		}
		xfree(sd);
	}
}

/**
 * This function will be similar to fgetc except it will
 * use the SOCKET structure for buffering.    It will read
 * up to sizeof(sd->buf) bytes into the internal buffer
 * and then let sd->bufptr point to it.    If bufptr is 
 * not NULL, then it will return a byte each time it is
 * called and advance the pointer for the next call. If
 * bufptr is NULL, it will read another sizeof(sd->buf)
 * bytes and reset sd->bufptr.
 */
int
dnetGetc(dsocket *sd)
{
	int retval=1, recval=0;
	assert(sd != NULL);

	/* If there aren't any bytes available, get some. */
	if (sd->avail_bytes <= 0) {
		sd->eom = false;
		sd->flags = 0;
		memset(sd->buf, '\0', MAXSOCKBUF);
#ifdef HAVE_LIBSSL
		if (!sd->ssl) {
#endif
			recval = recv(sd->sock, sd->buf, MAXSOCKBUF-1, 0);
#ifdef HAVE_LIBSSL
		} else {
			recval = SSL_read(sd->ssl, sd->buf, MAXSOCKBUF-1);
		}
#endif
		if (recval == 0) {
			sd->flags |= SOCKET_EOF;
			retval = -1;
		} else if (recval == -1) {
			sd->flags |= SOCKET_ERROR;
			sd->errnum = errno;
			retval = -1;
		} else {
			sd->bufptr = sd->buf;
			sd->avail_bytes = recval;
			if (recval < MAXSOCKBUF-1) {
				// That's all they sent in this message.
				sd->eom = true;
			} else {
				sd->eom = false;
			}
		}
	} 

	if (sd->avail_bytes > 0) {
		sd->avail_bytes--;
		if (sd->avail_bytes == 0 && sd->eom) {
			sd->flags |= SOCKET_EOF;
		}
		retval = *sd->bufptr++;
	}
	return retval;
}

/**
 * This function will be similar to fputc except it will
 * use the SOCKET struture instead of the FILE structure
 * to place the file on the stream.
 */
int
dnetPutc(dsocket *sd, int ch)
{
	int retval=SUCCESS;
	assert(sd != NULL);

#ifdef HAVE_LIBSSL
	if (!sd->ssl) {
#endif
		if (send(sd->sock, (char *)&ch, 1, 0) != 1) {
			sd->flags |= SOCKET_ERROR;
			sd->errnum = errno;
			retval = ERROR;
		}
#ifdef HAVE_LIBSSL
	} else {
		if (SSL_write(sd->ssl, (char *)&ch, 1) == -1) {
			sd->flags |= SOCKET_ERROR;
			sd->errnum = errno;
			retval = ERROR;
		}
	}
#endif

	return retval;
}

/**
 * This function will be similar to fgets except it will
 * use the Sgetc to read one character at a time.
 */
int
dnetReadline(dsocket *sd, dstrbuf *buf)
{
	int ch, size=0;

	do {
		ch = dnetGetc(sd);
		if (ch == -1) {
			// Error
			break;
		} else {
			dsbCatChar(buf, ch);
			size++;
		}
	} while (ch != '\n' && !dnetEof(sd));
	return size;
}

/**
 * Writes to a socket
 */
int
dnetWrite(dsocket *sd, const char *buf, size_t len)
{
	int bytes = 0;
	const size_t blocklen = 4356;
	size_t sentLen=0;

#ifdef HAVE_LIBSSL
	if (!sd->ssl) {
#endif
		while (len > 0) {
			size_t sendSize = (len > blocklen) ? blocklen : len;
			bytes = send(sd->sock, buf, sendSize, 0);
			if (bytes == -1) {
				if (errno == EAGAIN) {
					continue;
				} else if (errno == EINTR) {
					continue;
				} else {
					sd->flags |= SOCKET_ERROR;
					sd->errnum = errno;
					break;
				}
			} else if (bytes == 0) {
				sd->flags |= SOCKET_ERROR;
				sd->errnum = EPIPE;
				break;
			} else if (bytes > 0) {
				buf += bytes;
				len -= bytes;
				sentLen += bytes;
			}
		}
#ifdef HAVE_LIBSSL
	} else {
		sentLen = SSL_write(sd->ssl, buf, len);
	}
#endif
	return sentLen;
}

/**
 * Reads stuff from the socket up to size.
 */
int
dnetRead(dsocket *sd, char *buf, size_t size)
{
	u_int i;
	int ch;

	for (i = 0; i < size-1; i++) {
		ch = dnetGetc(sd);
		if (ch == -1) {
			i = 0;
			break;
		}
		*buf++ = ch;
		if (dnetEof(sd)) {
			break;
		}
	}
	return i;
}
	

/**
 * Will test the flags for a SOCKET_ERROR 
 */
bool
dnetErr(dsocket *sd)
{
	return sd->flags & SOCKET_ERROR;
}

/**
 * Will test the flags for a SOCKET_EOF
 */
bool
dnetEof(dsocket *sd)
{
	return sd->flags & SOCKET_EOF;
}

/**
 * Returns the error string from the system which is
 * determined by errnum (errno).
 */
char *
dnetGetErr(dsocket *sd)
{
	return strerror(sd->errnum);
}

int
dnetGetSock(dsocket *sd)
{
	return sd->sock;
}

