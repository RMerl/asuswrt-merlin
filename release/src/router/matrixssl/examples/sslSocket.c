/*
 *	socketLayer.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Sample SSL socket layer for MatrixSSL example exectuables
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
#include "sslSocket.h"

/******************************************************************************/
/*
	An EXAMPLE socket layer API for the MatrixSSL library.  
*/

/******************************************************************************/
/*
	Server side.  Set up a listen socket.  This code is not specific to SSL.
*/
SOCKET socketListen(short port, int *err)
{
	struct sockaddr_in	addr;
	SOCKET				fd;
	int					rc;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error creating listen socket\n");
		*err = getSocketError();
		return INVALID_SOCKET;
	}
/*
	Make sure the socket is not inherited by exec'd processes
	Set the REUSE flag to minimize the number of sockets in TIME_WAIT
*/
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	rc = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&rc, sizeof(rc));

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, 
			"Can't bind socket. Port in use or insufficient privilege\n");
		*err = getSocketError();
		return INVALID_SOCKET;
	}
	if (listen(fd, SOMAXCONN) < 0) {
		fprintf(stderr, "Error listening on socket\n");
		*err = getSocketError();
		return INVALID_SOCKET;
	}
	return fd;
}

/******************************************************************************/
/*
	Server side.  Accept a new socket connection off our listen socket.  
	This code is not specific to SSL.
*/
SOCKET socketAccept(SOCKET listenfd, int *err)
{
	struct sockaddr_in	addr;
	SOCKET				fd;
	int					len;
/*
	Wait(blocking)/poll(non-blocking) for an incoming connection
*/
	len = sizeof(addr);
	if ((fd = accept(listenfd, (struct sockaddr *)&addr, &len)) 
			== INVALID_SOCKET) {
		*err = getSocketError();
		if (*err != WOULD_BLOCK) {
			fprintf(stderr, "Error %d accepting new socket\n", *err);
		}
		return INVALID_SOCKET;
	}
/*
	fd is the newly accepted socket. Disable Nagle on this socket.
	Set blocking mode as default
*/
/*	fprintf(stdout, "Connection received from %d.%d.%d.%d\n", 
		addr.sin_addr.S_un.S_un_b.s_b1,
		addr.sin_addr.S_un.S_un_b.s_b2,
		addr.sin_addr.S_un.S_un_b.s_b3,
		addr.sin_addr.S_un.S_un_b.s_b4);
*/
	setSocketNodelay(fd);
	setSocketBlock(fd);
	return fd;
}

/******************************************************************************/
/*
	Client side. Open a socket connection to a remote ip and port.
	This code is not specific to SSL.
*/
SOCKET socketConnect(char *ip, short port, int *err)
{
	struct sockaddr_in	addr;
	SOCKET				fd;
	int					rc;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error creating socket\n");
		*err = getSocketError();
		return INVALID_SOCKET;
	}
/*
	Make sure the socket is not inherited by exec'd processes
	Set the REUSEADDR flag to minimize the number of sockets in TIME_WAIT
*/
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	rc = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&rc, sizeof(rc));
	setSocketNodelay(fd);
/*
	Turn on blocking mode for the connecting socket
*/
	setSocketBlock(fd);

	memset((char *) &addr, 0x0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);
	rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
#if WIN
	if (rc != 0) {
#else
	if (rc < 0) {
#endif
		*err = getSocketError();
		return INVALID_SOCKET;
	}
	return fd;
}

/******************************************************************************/
/*
	Server side.  Accept an incomming SSL connection request.
	'conn' will be filled in with information about the accepted ssl connection

	return -1 on error, 0 on success, or WOULD_BLOCK for non-blocking sockets
*/
int sslAccept(sslConn_t **cpp, SOCKET fd, sslKeys_t *keys,
			  int (*certValidator)(sslCertInfo_t *t, void *arg), int flags)
{
	sslConn_t		*conn;
	unsigned char	buf[1024];
	int				status, rc;
/*
	Associate a new ssl session with this socket.  The session represents
	the state of the ssl protocol over this socket.  Session caching is
	handled automatically by this api.
*/
	conn = calloc(sizeof(sslConn_t), 1);
	conn->fd = fd;
	if (matrixSslNewSession(&conn->ssl, keys, NULL,
			SSL_FLAGS_SERVER | flags) < 0) {
		sslFreeConnection(&conn);
		return -1;
	}
/*
	MatrixSSL doesn't provide buffers for data internally.  Define them
	here to support buffered reading and writing for non-blocking sockets.
	Although it causes quite a bit more work, we support dynamically growing
	the buffers as needed.  Alternately, we could define 16K buffers here
	and not worry about growing them.
*/
	memset(&conn->inbuf, 0x0, sizeof(sslBuf_t));
	conn->insock.size = 1024;
	conn->insock.start = conn->insock.end = conn->insock.buf = 
		(unsigned char *)malloc(conn->insock.size);
	conn->outsock.size = 1024;
	conn->outsock.start = conn->outsock.end = conn->outsock.buf = 
		(unsigned char *)malloc(conn->outsock.size);
	conn->inbuf.size = 0;
	conn->inbuf.start = conn->inbuf.end = conn->inbuf.buf = NULL;
	*cpp = conn;

readMore:
	rc = sslRead(conn, buf, sizeof(buf), &status);
/*
	Reading handshake records should always return 0 bytes, we aren't
	expecting any data yet.
*/
	if (rc == 0) {
		if (status == SSLSOCKET_EOF || status == SSLSOCKET_CLOSE_NOTIFY) {
			sslFreeConnection(&conn);
			return -1;
		}
		if (matrixSslHandshakeIsComplete(conn->ssl) == 0) {
			goto readMore;
		}
	} else if (rc > 0) {
		socketAssert(0);
		return -1;
	} else {
		fprintf(stderr, "sslRead error in sslAccept\n");
		sslFreeConnection(&conn);
		return -1;
	}
	*cpp = conn;

	return 0;
}

/******************************************************************************/
/*
	Client side.  Make a socket connection and go through the SSL handshake
	phase in blocking mode.  The last parameter is an optional function
	callback for user-level certificate validation.  NULL if not needed.
*/
int sslConnect(sslConn_t **cpp, SOCKET fd, sslKeys_t *keys, 
			   sslSessionId_t *id, short cipherSuite, 
			   int (*certValidator)(sslCertInfo_t *t, void *arg))
{
	sslConn_t	*conn;

/*
	Create a new SSL session for the new socket and register the
	user certificate validator 
*/
	conn = calloc(sizeof(sslConn_t), 1);
	conn->fd = fd;
	if (matrixSslNewSession(&conn->ssl, keys, id, 0) < 0) {
		sslFreeConnection(&conn);
		return -1;
	}
	matrixSslSetCertValidator(conn->ssl, certValidator, keys);

	*cpp = sslDoHandshake(conn, cipherSuite);
	
	if (*cpp == NULL) {
		return -1;
	}
	return 0;
}

/******************************************************************************/
/*
	Construct the initial HELLO message to send to the server and initiate
	the SSL handshake.  Can be used in the re-handshake scenario as well.
*/
sslConn_t *sslDoHandshake(sslConn_t *conn, short cipherSuite)
{
	char	buf[1024];
	int		bytes, status, rc;

/*
	MatrixSSL doesn't provide buffers for data internally.  Define them
	here to support buffered reading and writing for non-blocking sockets.
	Although it causes quite a bit more work, we support dynamically growing
	the buffers as needed.  Alternately, we could define 16K buffers here
	and not worry about growing them.
*/
	conn->insock.size = 1024;
	conn->insock.start = conn->insock.end = conn->insock.buf = 
		(unsigned char *)malloc(conn->insock.size);
	conn->outsock.size = 1024;
	conn->outsock.start = conn->outsock.end = conn->outsock.buf = 
		(unsigned char *)malloc(conn->outsock.size);
	conn->inbuf.size = 0;
	conn->inbuf.start = conn->inbuf.end = conn->inbuf.buf = NULL;

	bytes = matrixSslEncodeClientHello(conn->ssl, &conn->outsock, cipherSuite);
	if (bytes < 0) {
		socketAssert(bytes < 0);
		goto error;
	}
/*
	Send the hello with a blocking write
*/
	if (psSocketWrite(conn->fd, &conn->outsock) < 0) {
		fprintf(stdout, "Error in socketWrite\n");
		goto error;
	}
	conn->outsock.start = conn->outsock.end = conn->outsock.buf;
/*
	Call sslRead to work through the handshake.  Not actually expecting
	data back, so the finished case is simply when the handshake is
	complete.
*/
readMore:
	rc = sslRead(conn, buf, sizeof(buf), &status);
/*
	Reading handshake records should always return 0 bytes, we aren't
	expecting any data yet.
*/
	if (rc == 0) {
		if (status == SSLSOCKET_EOF || status == SSLSOCKET_CLOSE_NOTIFY) {
			goto error;
		}
		if (matrixSslHandshakeIsComplete(conn->ssl) == 0) {
			goto readMore;
		}
	} else if (rc > 0) {
		fprintf(stderr, "sslRead got %d data in sslDoHandshake %s\n", rc, buf);
		goto readMore;
	} else {
		fprintf(stderr, "sslRead error in sslDoHandhake\n");
		goto error;
	}

	return conn;

error:
	sslFreeConnection(&conn);
	return NULL;
}

/******************************************************************************/
/*
	An example socket sslRead implementation that handles the ssl handshake
	transparently.  Caller passes in allocated buf and length. 
	
	Return codes are as follows:

	-1 return code is an error.  If a socket level error, error code is
		contained in status parameter.  If using a non-blocking socket
		implementation the caller should check for non-fatal errors such as
		WOULD_BLOCK before closing the connection.  A zero value
		in status indicates an error with this routine.  

	A positive integer return code is the number of bytes successfully read
		into the supplied buffer.  User can call sslRead again on the updated
		buffer is there is more to be read.

	0 return code indicates the read was successful, but there was no data
		to be returned.  If status is set to zero, this is a case internal
		to the sslAccept and sslConnect functions that a handshake
		message has been exchanged.  If status is set to SOCKET_EOF
		the connection has been closed by the other side.

*/
int sslRead(sslConn_t *cp, char *buf, int len, int *status)
{
	int				bytes, rc, remaining;
	unsigned char	error, alertLevel, alertDescription, performRead;

	*status = 0;

	if (cp->ssl == NULL || len <= 0) {
		return -1;
	}
/*
	If inbuf is valid, then we have previously decoded data that must be
	returned, return as much as possible.  Once all buffered data is
	returned, free the inbuf.
*/
	if (cp->inbuf.buf) {
		if (cp->inbuf.start < cp->inbuf.end) {
			remaining = (int)(cp->inbuf.end - cp->inbuf.start);
			bytes = (int)min(len, remaining);
			memcpy(buf, cp->inbuf.start, bytes);
			cp->inbuf.start += bytes;
			return bytes;
		}
		free(cp->inbuf.buf);
		cp->inbuf.buf = NULL;
	}
/*
	Pack the buffered socket data (if any) so that start is at zero.
*/
	if (cp->insock.buf < cp->insock.start) {
		if (cp->insock.start == cp->insock.end) {
			cp->insock.start = cp->insock.end = cp->insock.buf;
		} else {
			memmove(cp->insock.buf, cp->insock.start, cp->insock.end - cp->insock.start);
			cp->insock.end -= (cp->insock.start - cp->insock.buf);
			cp->insock.start = cp->insock.buf;
		}
	}
/*
	Read up to as many bytes as there are remaining in the buffer.  We could
	Have encrypted data already cached in conn->insock, but might as well read more
	if we can.
*/
	performRead = 0;
readMore:
	if (cp->insock.end == cp->insock.start || performRead) {
		performRead = 1;
		bytes = recv(cp->fd, (char *)cp->insock.end, 
			(int)((cp->insock.buf + cp->insock.size) - cp->insock.end), MSG_NOSIGNAL);
		if (bytes == SOCKET_ERROR) {
			*status = getSocketError();
			return -1;
		}
		if (bytes == 0) {
			*status = SSLSOCKET_EOF;
			return 0;
		}
		cp->insock.end += bytes;
	}
/*
	Define a temporary sslBuf
*/
	cp->inbuf.start = cp->inbuf.end = cp->inbuf.buf = malloc(len);
	cp->inbuf.size = len;
/*
	Decode the data we just read from the socket
*/
decodeMore:
	error = 0;
	alertLevel = 0;
	alertDescription = 0;

	rc = matrixSslDecode(cp->ssl, &cp->insock, &cp->inbuf, &error, &alertLevel, 
		&alertDescription);
	switch (rc) {
/*
	Successfully decoded a record that did not return data or require a response.
*/
	case SSL_SUCCESS:
		return 0;
/*
	Successfully decoded an application data record, and placed in tmp buf
*/
	case SSL_PROCESS_DATA:
/*
		Copy as much as we can from the temp buffer into the caller's buffer
		and leave the remainder in conn->inbuf until the next call to read
		It is possible that len > data in buffer if the encoded record
		was longer than len, but the decoded record isn't!
*/
		rc = (int)(cp->inbuf.end - cp->inbuf.start);
		rc = min(rc, len);
		memcpy(buf, cp->inbuf.start, rc);
		cp->inbuf.start += rc;
		return rc;
/*
	We've decoded a record that requires a response into tmp
	If there is no data to be flushed in the out buffer, we can write out
	the contents of the tmp buffer.  Otherwise, we need to append the data 
	to the outgoing data buffer and flush it out.
*/
	case SSL_SEND_RESPONSE:
		bytes = send(cp->fd, (char *)cp->inbuf.start, 
			(int)(cp->inbuf.end - cp->inbuf.start), MSG_NOSIGNAL);
		if (bytes == SOCKET_ERROR) {
			*status = getSocketError();
			if (*status != WOULD_BLOCK) {
				fprintf(stdout, "Socket send error:  %d\n", *status);
				goto readError;
			}
			*status = 0;
		}
		cp->inbuf.start += bytes;
		if (cp->inbuf.start < cp->inbuf.end) {
/*
			This must be a non-blocking socket since it didn't all get sent
			out and there was no error.  We want to finish the send here
			simply because we are likely in the SSL handshake.
*/
			setSocketBlock(cp->fd);
			bytes = send(cp->fd, (char *)cp->inbuf.start, 
				(int)(cp->inbuf.end - cp->inbuf.start), MSG_NOSIGNAL);
			if (bytes == SOCKET_ERROR) {
				*status = getSocketError();
				goto readError;
			}
			cp->inbuf.start += bytes;
			socketAssert(cp->inbuf.start == cp->inbuf.end);
/*
			Can safely set back to non-blocking because we wouldn't
			have got here if this socket wasn't non-blocking to begin with.
*/
			setSocketNonblock(cp->fd);
		}
		cp->inbuf.start = cp->inbuf.end = cp->inbuf.buf;
		return 0;
/*
	There was an error decoding the data, or encoding the out buffer.
	There may be a response data in the out buffer, so try to send.
	We try a single hail-mary send of the data, and then close the socket.
	Since we're closing on error, we don't worry too much about a clean flush.
*/
	case SSL_ERROR:
		fprintf(stderr, "SSL: Closing on protocol error %d\n", error);
		if (cp->inbuf.start < cp->inbuf.end) {
			setSocketNonblock(cp->fd);
			bytes = send(cp->fd, (char *)cp->inbuf.start, 
				(int)(cp->inbuf.end - cp->inbuf.start), MSG_NOSIGNAL);
		}
		goto readError;
/*
	We've decoded an alert.  The level and description passed into
	matrixSslDecode are filled in with the specifics.
*/
	case SSL_ALERT:
		if (alertDescription == SSL_ALERT_CLOSE_NOTIFY) {
			*status = SSLSOCKET_CLOSE_NOTIFY;
			goto readZero;
		}
		fprintf(stderr, "SSL: Closing on client alert %d: %d\n",
			alertLevel, alertDescription);
		goto readError;
/*
	We have a partial record, we need to read more data off the socket.
	If we have a completely full conn->insock buffer, we'll need to grow it
	here so that we CAN read more data when called the next time.
*/
	case SSL_PARTIAL:
		if (cp->insock.start == cp->insock.buf && cp->insock.end == 
				(cp->insock.buf + cp->insock.size)) {
			if (cp->insock.size > SSL_MAX_BUF_SIZE) {
				goto readError;
			}
			cp->insock.size *= 2;
			cp->insock.start = cp->insock.buf = 
				(unsigned char *)realloc(cp->insock.buf, cp->insock.size);
			cp->insock.end = cp->insock.buf + (cp->insock.size / 2);
		}
		if (!performRead) {
			performRead = 1;
			free(cp->inbuf.buf);
			cp->inbuf.buf = NULL;
			goto readMore;
		} else {
			goto readZero;
		}
/*
	The out buffer is too small to fit the decoded or response
	data.  Increase the size of the buffer and call decode again
*/
	case SSL_FULL:
		cp->inbuf.size *= 2;
		if (cp->inbuf.buf != (unsigned char*)buf) {
			free(cp->inbuf.buf);
			cp->inbuf.buf = NULL;
		}
		cp->inbuf.start = cp->inbuf.end = cp->inbuf.buf = 
			(unsigned char *)malloc(cp->inbuf.size);
		goto decodeMore;
	}
/*
	We consolidated some of the returns here because we must ensure
	that conn->inbuf is cleared if pointing at caller's buffer, otherwise
	it will be freed later on.
*/
readZero:
	if (cp->inbuf.buf == (unsigned char*)buf) {
		cp->inbuf.buf = NULL;
	}
	return 0;
readError:
	if (cp->inbuf.buf == (unsigned char*)buf) {
		cp->inbuf.buf = NULL;
	}
	return -1;
}

/******************************************************************************/
/*
	Example sslWrite functionality.  Takes care of encoding the input buffer
	and sending it out on the connection.

	Return codes are as follows:

	-1 return code is an error.  If a socket level error, error code is
		contained in status.  If using a non-blocking socket
		implementation the caller should check for non-fatal errors such as
		WOULD_BLOCK before closing the connection.  A zero value
		in status indicates an error with this routine.

	A positive integer return value indicates the number of bytes succesfully
		written on the connection.  Should always match the len parameter.

	0 return code indicates the write must be called again with the same
		parameters.
*/
int sslWrite(sslConn_t *cp, char *buf, int len, int *status)
{
	int		rc;

	*status = 0;
/*
	Pack the buffered socket data (if any) so that start is at zero.
*/
	if (cp->outsock.buf < cp->outsock.start) {
		if (cp->outsock.start == cp->outsock.end) {
			cp->outsock.start = cp->outsock.end = cp->outsock.buf;
		} else {
			memmove(cp->outsock.buf, cp->outsock.start, cp->outsock.end - cp->outsock.start);
			cp->outsock.end -= (cp->outsock.start - cp->outsock.buf);
			cp->outsock.start = cp->outsock.buf;
		}
	}
/*
	If there is buffered output data, the caller must be trying to
	send the same amount of data as last time.  We don't support 
	sending additional data until the original buffered request has
	been completely sent.
*/
	if (cp->outBufferCount > 0 && len != cp->outBufferCount) {
		socketAssert(len != cp->outBufferCount);
		return -1;
	}
/*
	If we don't have buffered data, encode the caller's data
*/
	if (cp->outBufferCount == 0) {
retryEncode:
		rc = matrixSslEncode(cp->ssl, (unsigned char *)buf, len, &cp->outsock);
		switch (rc) {
		case SSL_ERROR:
			return -1;
		case SSL_FULL:
			if (cp->outsock.size > SSL_MAX_BUF_SIZE) {
				return -1;
			}
			cp->outsock.size *= 2;
			cp->outsock.buf = 
				(unsigned char *)realloc(cp->outsock.buf, cp->outsock.size);
			cp->outsock.end = cp->outsock.buf + (cp->outsock.end - cp->outsock.start);
			cp->outsock.start = cp->outsock.buf;
			goto retryEncode;
		}
	}
/*
	We've got data to send.
*/
	rc = send(cp->fd, (char *)cp->outsock.start, 
		(int)(cp->outsock.end - cp->outsock.start), MSG_NOSIGNAL);
	if (rc == SOCKET_ERROR) {
		*status = getSocketError();
		return -1;
	}
	cp->outsock.start += rc;
/*
	If we wrote it all return the length, otherwise remember the number of
	bytes passed in, and return 0 to be called again later.
*/
	if (cp->outsock.start == cp->outsock.end) {
		cp->outBufferCount = 0;
		return len;
	}
	cp->outBufferCount = len;
	return 0;
}

/******************************************************************************/
/*
	Send a close alert
*/
void sslWriteClosureAlert(sslConn_t *cp)
{
	if (cp != NULL) {
		cp->outsock.start = cp->outsock.end = cp->outsock.buf;
			matrixSslEncodeClosureAlert(cp->ssl, &cp->outsock);
		setSocketNonblock(cp->fd);
		send(cp->fd, cp->outsock.start,
			(int)(cp->outsock.end - cp->outsock.start), MSG_NOSIGNAL);
	}
}

/******************************************************************************/
/*
	Server initiated rehandshake.  Builds and sends the HELLO_REQUEST message
*/
void sslRehandshake(sslConn_t *cp)
{
	matrixSslEncodeHelloRequest(cp->ssl, &cp->outsock);
	psSocketWrite(cp->fd, &cp->outsock);
	cp->outsock.start = cp->outsock.end = cp->outsock.buf;
}

/******************************************************************************/
/*
	Close a seesion that was opened with sslAccept or sslConnect and
	free the insock and outsock buffers
*/
void sslFreeConnection(sslConn_t **cpp)
{
	sslConn_t	*conn;

	conn = *cpp;
	matrixSslDeleteSession(conn->ssl);
	conn->ssl = NULL;
	if (conn->insock.buf) {
		free(conn->insock.buf);
		conn->insock.buf = NULL;
	}
	if (conn->outsock.buf) {
		free(conn->outsock.buf);
		conn->outsock.buf = NULL;
	}
	if (conn->inbuf.buf) {
		free(conn->inbuf.buf);
		conn->inbuf.buf = NULL;
	}
	free(conn);
	*cpp = NULL;
}

/******************************************************************************/
/*
	free the insock and outsock buffers
*/
void sslFreeConnectionBuffers(sslConn_t **cpp)
{
	sslConn_t	*conn;

	conn = *cpp;
	if (conn->insock.buf) {
		free(conn->insock.buf);
		conn->insock.buf = NULL;
	}
	if (conn->outsock.buf) {
		free(conn->outsock.buf);
		conn->outsock.buf = NULL;
	}
	if (conn->inbuf.buf) {
		free(conn->inbuf.buf);
		conn->inbuf.buf = NULL;
	}
}

/******************************************************************************/
/*
	Set the socket to non blocking mode and perform a few extra tricks
	to make sure the socket closes down cross platform
*/
void socketShutdown(SOCKET sock)
{
	char	buf[32];

	if (sock != INVALID_SOCKET) {
		setSocketNonblock(sock);
		if (shutdown(sock, 1) >= 0) {
			while (recv(sock, buf, sizeof(buf), 0) > 0);
		}
		closesocket(sock);
	}
}

/******************************************************************************/
/*
	Perform a blocking write of data to a socket
*/
int psSocketWrite(SOCKET sock, sslBuf_t *out)
{
	unsigned char	*s;
	int				bytes;

	s = out->start;
	while (out->start < out->end) {
		bytes = send(sock, out->start, (int)(out->end - out->start), MSG_NOSIGNAL);
		if (bytes == SOCKET_ERROR) {
			return -1;
		}
		out->start += bytes;
	}
	return (int)(out->start - s);
}

int psSocketRead(SOCKET sock, sslBuf_t **out, int *status)
{
	sslBuf_t	*local;
	char		*c;
	int			bytes;

	local = *out;
	c = local->start;

	bytes = recv(sock, c, (int)((local->buf + local->size) - local->end), MSG_NOSIGNAL);
	if (bytes == SOCKET_ERROR) {
		*status = getSocketError();
		return -1;
	}
	if (bytes == 0) {
		*status = SSLSOCKET_EOF;
		return 0;
	}
	local->end += bytes;
	return bytes;
}

/******************************************************************************/
/*
	Turn on socket blocking mode (and set CLOEXEC on LINUX for kicks).
*/
void setSocketBlock(SOCKET sock)
{
#if _WIN32
	int		block = 0;
	ioctlsocket(sock, FIONBIO, &block);
#elif LINUX
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) & ~O_NONBLOCK);
	fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
}

/******************************************************************************/
/*
	Turn off socket blocking mode.
*/
void setSocketNonblock(SOCKET sock)
{
#if _WIN32
	int		block = 1;
	ioctlsocket(sock, FIONBIO, &block);
#elif LINUX
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
#endif
}

/******************************************************************************/
/*
	Disable the Nagle algorithm for less latency in RPC
	http://www.faqs.org/rfcs/rfc896.html
	http://www.w3.org/Protocols/HTTP/Performance/Nagle/
*/
void setSocketNodelay(SOCKET sock)
{
#if _WIN32
	BOOL	tmp = TRUE;
#else
	int		tmp = 1;
#endif /* WIN32 */
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&tmp, sizeof(tmp));
}

/******************************************************************************/
/*
	Set a breakpoint in this function to catch asserts.
	This function is called whenever an assert is triggered.  Useful because
	VisualStudio often won't show the right line of code if DebugBreak() is 
	called directly, and abort() may not be desireable on LINUX.
*/
void breakpoint()
{
	static int preventInline = 0;
#if _WIN32
	DebugBreak();
#elif LINUX
	abort();
#endif
}


/******************************************************************************/
/*
 	Parse an ASCII command line string.  Assumes a NULL terminated space 
 	separated list of command line arguments.  Uses this info to create an argv
 	array.
 
 	Notes:
 		handles double quotes
 		args gets hacked up!  can't pass in static string!
 		not thread safe, so should be called b4 any thread creation
 		we currently hardcode argv[0] cause none of our apps need it
 */

#if WINCE || VXWORKS

void parseCmdLineArgs(char *args, int *pargc, char ***pargv)
{
	char			**argv;
	char			*ptr;
	int				size, i;

/*
 *	Figure out the number of elements in our argv array.  
 *	We know we need an argv array of at least 3, since we have the
 *	program name, an argument, and a NULL in the array.
 */
	for (size = 3, ptr = args; ptr && *ptr != '\0'; ptr++) {
		if (isspace(*ptr)) {
			size++;
			while (isspace(*ptr)) {
				ptr++;
			}
			if (*ptr == '\0') {
				break;
			}
		}
	}
/*
 *	This is called from main, so don't use psMalloc here or
 *	all the stats will be wrong.
 */
	argv = (char**) malloc(size * sizeof(char*));
	*pargv = argv;

	for (i = 1, ptr = args; ptr && *ptr != '\0'; i++) {
		while (isspace(*ptr)) {
			ptr++;
		}
		if (*ptr == '\0')  {
			break;
		}
/*
 *		Handle double quoted arguments.  Treat everything within
 *		the double quote as one arg.
 */
		if (*ptr == '"') {
			ptr++;
			argv[i] = ptr;
			while ((*ptr != '\0') && (*ptr != '"')) {
				ptr++;
			}
		} else {
			argv[i] = ptr;
			while (*ptr != '\0' && !isspace(*ptr)) {
				ptr++;
			}
		}
		if (*ptr != '\0') {
			*ptr = '\0';
			ptr++;
		}
	}
	argv[i] = NULL;
	*pargc = i ;

	argv[0] = "PeerSec";
	for (ptr = argv[0]; *ptr; ptr++) {
		if (*ptr == '\\') {
			*ptr = '/';
		}
	}
}
#endif /* WINCE || VXWORKS */

#ifdef WINCE

/******************************************************************************/
/*
 	The following functions implement a unixlike time() function for WINCE.

	NOTE: this code is copied from the os layer in win.c to expose it for use
	in example applications.
 */

static FILETIME YearToFileTime(WORD wYear)
{	
	SYSTEMTIME sbase;
	FILETIME fbase;

	sbase.wYear         = wYear;
	sbase.wMonth        = 1;
	sbase.wDayOfWeek    = 1; //assumed
	sbase.wDay          = 1;
	sbase.wHour         = 0;
	sbase.wMinute       = 0;
	sbase.wSecond       = 0;
	sbase.wMilliseconds = 0;

	SystemTimeToFileTime( &sbase, &fbase );

	return fbase;
}

time_t time() {

	__int64 time1, time2, iTimeDiff;
	FILETIME fileTime1, fileTime2;
	SYSTEMTIME  sysTime;

/*
	Get 1970's filetime.
*/
	fileTime1 = YearToFileTime(1970);

/*
	Get the current filetime time.
*/
	GetSystemTime(&sysTime);
	SystemTimeToFileTime(&sysTime, &fileTime2);


/* 
	Stuff the 2 FILETIMEs into their own __int64s.
*/	
	time1 = fileTime1.dwHighDateTime;
	time1 <<= 32;				
	time1 |= fileTime1.dwLowDateTime;

	time2 = fileTime2.dwHighDateTime;
	time2 <<= 32;				
	time2 |= fileTime2.dwLowDateTime;

/*
	Get the difference of the two64-bit ints.

	This is he number of 100-nanosecond intervals since Jan. 1970.  So
	we divide by 10000 to get seconds.
 */
	iTimeDiff = (time2 - time1) / 10000000;
	return (int)iTimeDiff;
}
#endif /* WINCE */

/******************************************************************************/





