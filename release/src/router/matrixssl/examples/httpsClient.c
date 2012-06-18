/*
 *	httpClient.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Simple example program for MatrixSSL
 *	Sends a HTTPS request and echos the response back to the sender.
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#ifndef WINCE
	#include <time.h>
#endif

/******************************************************************************/

#include "sslSocket.h"

/******************************************************************************/

#define HTTPS_PORT	4433
#define HTTPS_IP	"127.0.0.1"

static char CAfile[] = "CAcertSrv.pem";


#define ITERATIONS	100 /* How many individual connections to make */
#define REQUESTS	10  /* How many requests per each connection */
#define REUSE		0  /* 0 if session resumption disabled */

#define ENFORCE_CERT_VALIDATION 1 /* 0 to allow connection without validation */


static const char request[] = "GET / HTTP/1.0\r\n"
		"User-Agent: MatrixSSL httpClient\r\n"
		"Accept: */*\r\n"
		"\r\n";

static const char requestAgain[] = "GET /again HTTP/1.0\r\n"
		"User-Agent: MatrixSSL httpClient\r\n"
		"Accept: */*\r\n"
		"\r\n";

static const char quitString[] = "GET /quit";

/*
	Callback that is registered to receive server certificate 
	information for custom validation
*/
static int certChecker(sslCertInfo_t *cert, void *arg);

/******************************************************************************/
/*
	Example ssl client that connects to a server and sends https messages
*/
#if VXWORKS
int _httpsClient(char *arg1)
#elif WINCE
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					LPWSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char **argv)
#endif
{
	sslSessionId_t		*sessionId;
	sslConn_t			*conn;
	sslKeys_t			*keys;
	WSADATA				wsaData;
	SOCKET				fd;
	short				cipherSuite;
	unsigned char		*ip, *c, *requestBuf;
	unsigned char		buf[1024];
	int					iterations, requests, connectAgain, status;
	int					quit, rc, bytes, i, j, err;
	time_t				t0, t1;
#if REUSE
	int					anonStatus;
#endif
#if VXWORKS
	int					argc;
	char				**argv;
	parseCmdLineArgs(arg1, &argc, &argv);
#endif /* VXWORKS */

#if WINCE
	int					argc;
	char				**argv;
	char				args[256];

/*
 *	parseCmdLineArgs expects an ASCII string and CE is unicoded, so convert
 *	the command line.  args will get hacked up, so you can't pass in a
 *	static string.
 */
	WideCharToMultiByte(CP_ACP, 0, lpCmdLine, -1, args, 256, NULL, NULL);

/*
 *	Parse the command line into an argv array.  This allocs memory, so
 *	we have to free argv when we're done.
 */
	parseCmdLineArgs(args, &argc, &argv);
#endif /* WINCE */

	conn = NULL;
/*
	First (optional) argument is ip address to connect to (port is hardcoded)
	Second (optional) argument is number of iterations to perform
	Third (optional) argument is number of keepalive HTTP requests
	Fourth (optional) argument is cipher suite number to use (0 for any)
*/
	ip = HTTPS_IP;
	iterations = ITERATIONS;
	requests = REQUESTS;
	cipherSuite = 0x0000;
	if (argc > 1) {
		ip = argv[1];
		if (argc > 2) {
			iterations = atoi(argv[2]);
			socketAssert(iterations > 0);
			if (argc > 3) {
				requests = atoi(argv[3]);
				socketAssert(requests > 0);
				if (argc > 4) {
					cipherSuite = (short)atoi(argv[4]);
				}
			}
		}
	}
/*
	Initialize Windows sockets (no-op on other platforms)
*/
	WSAStartup(MAKEWORD(1,1), &wsaData);
/*
	Initialize the MatrixSSL Library, and read in the certificate file
	used to validate the server.
*/
	if (matrixSslOpen() < 0) {
		fprintf(stderr, "matrixSslOpen failed, exiting...");
	}
	sessionId = NULL;
	if (matrixSslReadKeys(&keys, NULL, NULL, NULL, CAfile) < 0) {
		goto promptAndExit;
	}
/*
	Intialize loop control variables
*/
	quit = 0;
	connectAgain = 1;
	i = 1;
/*
	Just reuse the requestBuf and malloc to largest possible message size
*/
	requestBuf = malloc(sizeof(requestAgain));
	t0 = time(0);
/*
	Main ITERATIONS loop
*/
	while (!quit && (i < iterations)) {
/*
		sslConnect uses port and ip address to connect to SSL server.
		Generates a new session
*/
		if (connectAgain) {
			if ((fd = socketConnect(ip, HTTPS_PORT, &err)) == INVALID_SOCKET) {
				fprintf(stdout, "Error connecting to server %s:%d\n", ip, HTTPS_PORT);
				matrixSslFreeKeys(keys);
				goto promptAndExit;
			}
			if (sslConnect(&conn, fd, keys, sessionId, cipherSuite, certChecker) < 0) {
				quit = 1;
				socketShutdown(fd);
				fprintf(stderr, "Error connecting to %s:%d\n", ip, HTTPS_PORT);
				continue;
			}
			i++;
			connectAgain = 0;
			j = 1;
		}
		if (conn == NULL) {
			quit++;
			continue;
		}
/*
		Copy the HTTP request header into the buffer, based of whether or
		not we want httpReflector to keep the socket open or not
*/
		if (j == requests) {
			bytes = (int)strlen(request);
			memcpy(requestBuf, request, bytes);
		} else {
			bytes = (int)strlen(requestAgain);
			memcpy(requestBuf, requestAgain, bytes);
		}
/*
		Send request.  
		< 0 return indicates an error.
		0 return indicates not all data was sent and we must retry
		> 0 indicates that all requested bytes were sent
*/
writeMore:
		rc = sslWrite(conn, requestBuf, bytes, &status);
		if (rc < 0) {
			fprintf(stdout, "Internal sslWrite error\n");
			socketShutdown(conn->fd);
			sslFreeConnection(&conn);
			continue;
		} else if (rc == 0) {
			goto writeMore;
		}
/*
		Read response
		< 0 return indicates an error.
		0 return indicates an EOF or CLOSE_NOTIFY in this situation
		> 0 indicates that some bytes were read.  Keep reading until we see
		the /r/n/r/n from the response header.  There may be data following
		this header, but we don't try too hard to read it for this example.
*/
		c = buf;
readMore:
		if ((rc = sslRead(conn, c, sizeof(buf) - (int)(c - buf), &status)) > 0) {
			c += rc;
			if (c - buf < 4 || memcmp(c - 4, "\r\n\r\n", 4) != 0) {
				goto readMore;
			}
		} else {
			if (rc < 0) {
				fprintf(stdout, "sslRead error.  dropping connection.\n");
			}
			if (rc < 0 || status == SSLSOCKET_EOF ||
					status == SSLSOCKET_CLOSE_NOTIFY) {
				socketShutdown(conn->fd);
				sslFreeConnection(&conn);
				continue;
			}
			goto readMore;
		}
/*
		Determine if we want to do a pipelined HTTP request/response
*/
		if (j++ < requests) {
			fprintf(stdout, "R");
			fflush(stdout);
			continue;
		} else {
			fprintf(stdout, "C");
			fflush(stdout);
		}
/*
		Reuse the session.  Comment out these two lines to test the entire
		public key renegotiation each iteration
*/
#if REUSE
		matrixSslFreeSessionId(sessionId);
		matrixSslGetSessionId(conn->ssl, &sessionId);
/*
		This example shows how a user might want to limit a client to
		resuming handshakes only with authenticated servers.  In this
		example, the client will force any non-authenticated (anonymous)
		server to go through a complete handshake each time.  This is
		strictly an example of one policy decision an implementation 
		might wish to make.
*/
		matrixSslGetAnonStatus(conn->ssl, &anonStatus);
		if (anonStatus) {
			matrixSslFreeSessionId(sessionId);
			sessionId = NULL;
		}
#endif
/*
		Send a closure alert for clean shutdown of remote SSL connection
		This is for good form, some implementations just close the socket
*/
		sslWriteClosureAlert(conn);
/*
		Session done.  Connect again if more iterations remaining
*/
		socketShutdown(conn->fd);
		sslFreeConnection(&conn);
		connectAgain = 1;
	}

	t1 = time(0);
	free(requestBuf);
	matrixSslFreeSessionId(sessionId);
	if (conn && conn->ssl) {
		socketShutdown(conn->fd);
		sslFreeConnection(&conn);
	}
	fprintf(stdout, "\n%d connections in %d seconds (%f c/s)\n", 
		i, (int)(t1 - t0), (double)i / (t1 - t0));
	fprintf(stdout, "\n%d requests in %d seconds (%f r/s)\n", 
		i * requests, (int)(t1 - t0), 
		(double)(i * requests) / (t1 - t0));
/*
	Close listening socket, free remaining items
*/
	matrixSslFreeKeys(keys);
	matrixSslClose();
	WSACleanup();
promptAndExit:
	fprintf(stdout, "Press return to exit...\n");
	getchar();

#if WINCE || VXWORKS
	if (argv) {
		free((void*) argv);
	}
#endif /* WINCE */
	return 0;
}

/******************************************************************************/
/*
	Stub for a user-level certificate validator.  Just using
	the default validation value here.
*/
static int certChecker(sslCertInfo_t *cert, void *arg)
{
	sslCertInfo_t	*next;
	sslKeys_t		*keys;
/*
	Make sure we are checking the last cert in the chain
*/
	next = cert;
	keys = arg;
	while (next->next != NULL) {
		next = next->next;
	}
#if ENFORCE_CERT_VALIDATION
/*
	This case passes the true RSA authentication status through
*/
	return next->verified;
#else
/*
	This case passes an authenticated server through, but flags a
	non-authenticated server correctly.  The user can call the
	matrixSslGetAnonStatus later to see the status of this connection.
*/
	if (next->verified != 1) {
		return SSL_ALLOW_ANON_CONNECTION;
	}
	return next->verified;
#endif /* ENFORCE_CERT_VALIDATION */
}		

/******************************************************************************/








