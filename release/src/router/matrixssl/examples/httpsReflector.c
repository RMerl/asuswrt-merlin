/*
 *	httpReflector.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Simple example program for MatrixSSL
 *	Accepts a HTTPS request and echos the response back to the sender.
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
#include <string.h>

/******************************************************************************/

#include "sslSocket.h"

#define HTTPS_PORT	4433
static char keyfile[] = "privkeySrv.pem";
static char certfile[] = "certSrv.pem";

static const char responseHdr[] = "HTTP/1.0 200 OK\r\n"
		"Server: PeerSec Networks MatrixSSL\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-cache\r\n"
		"Content-type: text/plain\r\n"
		"\r\n"
		"PeerSec Networks\n"
		"Successful MatrixSSL request:\n";

static const char quitString[] = "GET /quit";
static const char againString[] = "GET /again";



/******************************************************************************/
/*
	Helper framework for testing matrixSslReadKeysMem
*/
#define USE_MEM_CERTS 0
#if USE_MEM_CERTS
#include <sys/stat.h>
static int32 getFileBin(char *fileName, unsigned char **bin, int32 *binLen);
#endif

/******************************************************************************/
/*
	This example application acts as an https server that accepts incoming
	client requests and reflects incoming data back to that client.  
*/
#if VXWORKS
int _httpsReflector(char *arg1)
#elif WINCE
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					LPWSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char **argv)
#endif
{
	sslConn_t		*cp;
	sslKeys_t		*keys;
	SOCKET			listenfd, fd;
	WSADATA			wsaData;
	unsigned char	buf[1024];
	unsigned char	*response, *c;
	int				responseHdrLen, acceptAgain, flags;
	int				bytes, status, quit, again, rc, err;
#if USE_MEM_CERTS
	unsigned char	*servBin, *servKeyBin, *caBin; 
	int				servBinLen, caBinLen, servKeyBinLen;
#endif

	cp = NULL;
/*
	Initialize Windows sockets (no-op on other platforms)
*/
	WSAStartup(MAKEWORD(1,1), &wsaData);
/*
	Initialize the MatrixSSL Library, and read in the public key (certificate)
	and private key.
*/
	if (matrixSslOpen() < 0) {
		fprintf(stderr, "matrixSslOpen failed, exiting...");
	}

#if USE_MEM_CERTS
/*
	Example of DER binary certs for matrixSslReadKeysMem
*/
	getFileBin("certSrv.der", &servBin, &servBinLen);
	getFileBin("privkeySrv.der", &servKeyBin, &servKeyBinLen);
	getFileBin("CAcertCln.der", &caBin, &caBinLen);

	matrixSslReadKeysMem(&keys, servBin, servBinLen,
		servKeyBin, servKeyBinLen, caBin, caBinLen); 

	free(servBin);
	free(servKeyBin);
	free(caBin);
#else 
/*
	Standard PEM files
*/
	if (matrixSslReadKeys(&keys, certfile, keyfile, NULL, NULL) < 0)  {
		fprintf(stderr, "Error reading or parsing %s or %s.\n", 
			certfile, keyfile);
		goto promptAndExit;
	}
#endif /* USE_MEM_CERTS */
	fprintf(stdout, 
		"Run httpsClient or type https://127.0.0.1:%d into your local Web browser.\n",
		HTTPS_PORT);
/*
	Create the listen socket
*/
	if ((listenfd = socketListen(HTTPS_PORT, &err)) == INVALID_SOCKET) {
		fprintf(stderr, "Cannot listen on port %d\n", HTTPS_PORT);
		goto promptAndExit;
	}
/*
	Set blocking or not on the listen socket
*/
	setSocketBlock(listenfd);
/*
	Loop control initalization
*/
	quit = 0;
	again = 0;
	flags = 0;

	acceptAgain = 1;
/*
	Main connection loop
*/
	while (!quit) {

		if (acceptAgain) {
/*
			sslAccept creates a new server session
*/
			/* TODO - deadlock on blocking socket accept.  Should disable blocking here */
			if ((fd = socketAccept(listenfd, &err)) == INVALID_SOCKET) {
				fprintf(stdout, "Error accepting connection: %d\n", err);
				continue;
			}
			if ((rc = sslAccept(&cp, fd, keys, NULL, flags)) != 0) {
				socketShutdown(fd);
				continue;
			}

			flags = 0;
			acceptAgain = 0;
		}
/*
		Read response
		< 0 return indicates an error.
		0 return indicates an EOF or CLOSE_NOTIFY in this situation
		> 0 indicates that some bytes were read.  Keep reading until we see
		the /r/n/r/n from the GET request.  We don't actually parse the request,
		we just echo it back.
*/
		c = buf;
readMore:
		if ((rc = sslRead(cp, c, sizeof(buf) - (int)(c - buf), &status)) > 0) {
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
				socketShutdown(cp->fd);
				sslFreeConnection(&cp);
				acceptAgain = 1;
				continue;
			}
			goto readMore;
		}
/*
		Done reading.  If the incoming data starts with the quitString,
		quit the application after this request
*/
		if (memcmp(buf, quitString, min(c - buf, 
				(int)strlen(quitString))) == 0) {
			quit++;
			fprintf(stdout, "Q");
		}
/*
		If the incoming data starts with the againString,
		we are getting a pipeline request on the same session.  Don't
		close and wait for new connection in this case.
*/
		if (memcmp(buf, againString,
				min(c - buf, (int)strlen(againString))) == 0) {
			again++;
			fprintf(stdout, "A");
		} else {
			fprintf(stdout, "R");
			again = 0;
		}
/*
		Copy the canned response header and decoded data from socket as the
		response (reflector)
*/
		responseHdrLen = (int)strlen(responseHdr);
		bytes = responseHdrLen + (int)(c - buf);
		response = malloc(bytes);
		memcpy(response, responseHdr, responseHdrLen);
		memcpy(response + responseHdrLen, buf, c - buf); 
/*
		Send response.
		< 0 return indicates an error.
		0 return indicates not all data was sent and we must retry
		> 0 indicates that all requested bytes were sent
*/
writeMore:
		rc = sslWrite(cp, response, bytes, &status);
		if (rc < 0) {
			free(response);
			fprintf(stdout, "Internal sslWrite error\n");
			socketShutdown(cp->fd);
			sslFreeConnection(&cp);
			continue;
		} else if (rc == 0) {
			goto writeMore;
		}
		free(response);
/*
		If we saw an /again request, loop up and process another pipelined
		HTTP request.  The /again request is supported in the httpsClient
		example code.
*/
		if (again) {
			continue;
		}
/*
		Send a closure alert for clean shutdown of remote SSL connection
		This is for good form, some implementations just close the socket
*/
		sslWriteClosureAlert(cp);
/*
		Close the socket and wait for next connection (new session)
*/
		socketShutdown(cp->fd);
		sslFreeConnection(&cp);
		acceptAgain = 1;
	}
/*
	Close listening socket, free remaining items
*/
	if (cp && cp->ssl) {
		socketShutdown(cp->fd);
		sslFreeConnection(&cp);
	}
	socketShutdown(listenfd);

	matrixSslFreeKeys(keys);
	matrixSslClose();
	WSACleanup();
promptAndExit:
	fprintf(stdout, "\n\nPress return to exit...\n");
	getchar();
	return 0;
}



#if USE_MEM_CERTS
static int32 getFileBin(char *fileName, unsigned char **bin,
				 int32 *binLen)
{
	FILE	*fp;
	struct	stat	fstat;
	size_t	tmp = 0;

	*binLen = 0;
	*bin = NULL;

	if (fileName == NULL) {
		return -1;
	}
	if ((stat(fileName, &fstat) != 0) || (fp = fopen(fileName, "rb")) == NULL) {
		return -7; /* FILE_NOT_FOUND */
	}

	*bin = malloc(fstat.st_size);
	if (*bin == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	while (((tmp = fread(*bin + *binLen, sizeof(char), 512, fp)) > 0) &&
			(*binLen < fstat.st_size)) { 
		*binLen += (int32)tmp;
	}
	fclose(fp);
	return 0;
}
#endif

/******************************************************************************/






