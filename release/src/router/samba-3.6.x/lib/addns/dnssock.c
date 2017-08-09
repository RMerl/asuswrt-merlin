/*
  Linux DNS client library implementation

  Copyright (C) 2006 Krishna Ganugapati <krishnag@centeris.com>
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "dns.h"
#include <sys/time.h>
#include <unistd.h>
#include "system/select.h"

static int destroy_dns_connection(struct dns_connection *conn)
{
	return close(conn->s);
}

/********************************************************************
********************************************************************/

static DNS_ERROR dns_tcp_open( const char *nameserver,
			       TALLOC_CTX *mem_ctx,
			       struct dns_connection **result )
{
	uint32_t ulAddress;
	struct hostent *pHost;
	struct sockaddr_in s_in;
	struct dns_connection *conn;
	int res;

	if (!(conn = talloc(mem_ctx, struct dns_connection))) {
		return ERROR_DNS_NO_MEMORY;
	}

	if ( (ulAddress = inet_addr( nameserver )) == INADDR_NONE ) {
		if ( (pHost = gethostbyname( nameserver )) == NULL ) {
			TALLOC_FREE(conn);
			return ERROR_DNS_INVALID_NAME_SERVER;
		}
		memcpy( &ulAddress, pHost->h_addr, pHost->h_length );
	}

	conn->s = socket( PF_INET, SOCK_STREAM, 0 );
	if (conn->s == -1) {
		TALLOC_FREE(conn);
		return ERROR_DNS_CONNECTION_FAILED;
	}

	talloc_set_destructor(conn, destroy_dns_connection);

	s_in.sin_family = AF_INET;
	s_in.sin_addr.s_addr = ulAddress;
	s_in.sin_port = htons( DNS_TCP_PORT );

	res = connect(conn->s, (struct sockaddr*)&s_in, sizeof( s_in ));
	if (res == -1) {
		TALLOC_FREE(conn);
		return ERROR_DNS_CONNECTION_FAILED;
	}

	conn->hType = DNS_TCP;

	*result = conn;
	return ERROR_DNS_SUCCESS;
}

/********************************************************************
********************************************************************/

static DNS_ERROR dns_udp_open( const char *nameserver,
			       TALLOC_CTX *mem_ctx,
			       struct dns_connection **result )
{
	unsigned long ulAddress;
	struct hostent *pHost;
	struct sockaddr_in RecvAddr;
	struct dns_connection *conn;

	if (!(conn = talloc(NULL, struct dns_connection))) {
		return ERROR_DNS_NO_MEMORY;
	}

	if ( (ulAddress = inet_addr( nameserver )) == INADDR_NONE ) {
		if ( (pHost = gethostbyname( nameserver )) == NULL ) {
			TALLOC_FREE(conn);
			return ERROR_DNS_INVALID_NAME_SERVER;
		}
		memcpy( &ulAddress, pHost->h_addr, pHost->h_length );
	}

	/* Create a socket for sending data */

	conn->s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if (conn->s == -1) {
		TALLOC_FREE(conn);
		return ERROR_DNS_CONNECTION_FAILED;
	}

	talloc_set_destructor(conn, destroy_dns_connection);

	/* Set up the RecvAddr structure with the IP address of
	   the receiver (in this example case "123.456.789.1")
	   and the specified port number. */

	ZERO_STRUCT(RecvAddr);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons( DNS_UDP_PORT );
	RecvAddr.sin_addr.s_addr = ulAddress;

	conn->hType = DNS_UDP;
	memcpy( &conn->RecvAddr, &RecvAddr, sizeof( struct sockaddr_in ) );

	*result = conn;
	return ERROR_DNS_SUCCESS;
}

/********************************************************************
********************************************************************/

DNS_ERROR dns_open_connection( const char *nameserver, int32 dwType,
		    TALLOC_CTX *mem_ctx,
		    struct dns_connection **conn )
{
	switch ( dwType ) {
	case DNS_TCP:
		return dns_tcp_open( nameserver, mem_ctx, conn );
	case DNS_UDP:
		return dns_udp_open( nameserver, mem_ctx, conn );
	}

	return ERROR_DNS_INVALID_PARAMETER;
}

static DNS_ERROR write_all(int fd, uint8 *data, size_t len)
{
	size_t total = 0;

	while (total < len) {

		ssize_t ret = write(fd, data + total, len - total);

		if (ret <= 0) {
			/*
			 * EOF or error
			 */
			return ERROR_DNS_SOCKET_ERROR;
		}

		total += ret;
	}

	return ERROR_DNS_SUCCESS;
}

static DNS_ERROR dns_send_tcp(struct dns_connection *conn,
			      const struct dns_buffer *buf)
{
	uint16 len = htons(buf->offset);
	DNS_ERROR err;

	err = write_all(conn->s, (uint8 *)&len, sizeof(len));
	if (!ERR_DNS_IS_OK(err)) return err;

	return write_all(conn->s, buf->data, buf->offset);
}

static DNS_ERROR dns_send_udp(struct dns_connection *conn,
			      const struct dns_buffer *buf)
{
	ssize_t ret;

	ret = sendto(conn->s, buf->data, buf->offset, 0,
		     (struct sockaddr *)&conn->RecvAddr,
		     sizeof(conn->RecvAddr));

	if (ret != buf->offset) {
		return ERROR_DNS_SOCKET_ERROR;
	}

	return ERROR_DNS_SUCCESS;
}

DNS_ERROR dns_send(struct dns_connection *conn, const struct dns_buffer *buf)
{
	if (conn->hType == DNS_TCP) {
		return dns_send_tcp(conn, buf);
	}

	if (conn->hType == DNS_UDP) {
		return dns_send_udp(conn, buf);
	}

	return ERROR_DNS_INVALID_PARAMETER;
}

static DNS_ERROR read_all(int fd, uint8 *data, size_t len)
{
	size_t total = 0;

	while (total < len) {
		struct pollfd pfd;
		ssize_t ret;
		int fd_ready;

		ZERO_STRUCT(pfd);
		pfd.fd = fd;
		pfd.events = POLLIN|POLLHUP;

		fd_ready = poll(&pfd, 1, 10000);
		if ( fd_ready == 0 ) {
			/* read timeout */
			return ERROR_DNS_SOCKET_ERROR;
		}

		ret = read(fd, data + total, len - total);
		if (ret <= 0) {
			/* EOF or error */
			return ERROR_DNS_SOCKET_ERROR;
		}

		total += ret;
	}

	return ERROR_DNS_SUCCESS;
}

static DNS_ERROR dns_receive_tcp(TALLOC_CTX *mem_ctx,
				 struct dns_connection *conn,
				 struct dns_buffer **presult)
{
	struct dns_buffer *buf;
	DNS_ERROR err;
	uint16 len;

	if (!(buf = TALLOC_ZERO_P(mem_ctx, struct dns_buffer))) {
		return ERROR_DNS_NO_MEMORY;
	}

	err = read_all(conn->s, (uint8 *)&len, sizeof(len));
	if (!ERR_DNS_IS_OK(err)) {
		return err;
	}

	buf->size = ntohs(len);

	if (buf->size) {
		if (!(buf->data = TALLOC_ARRAY(buf, uint8, buf->size))) {
			TALLOC_FREE(buf);
			return ERROR_DNS_NO_MEMORY;
		}
	} else {
		buf->data = NULL;
	}

	err = read_all(conn->s, buf->data, buf->size);
	if (!ERR_DNS_IS_OK(err)) {
		TALLOC_FREE(buf);
		return err;
	}

	*presult = buf;
	return ERROR_DNS_SUCCESS;
}

static DNS_ERROR dns_receive_udp(TALLOC_CTX *mem_ctx,
				 struct dns_connection *conn,
				 struct dns_buffer **presult)
{
	struct dns_buffer *buf;
	ssize_t received;

	if (!(buf = TALLOC_ZERO_P(mem_ctx, struct dns_buffer))) {
		return ERROR_DNS_NO_MEMORY;
	}

	/*
	 * UDP based DNS can only be 512 bytes
	 */

	if (!(buf->data = TALLOC_ARRAY(buf, uint8, 512))) {
		TALLOC_FREE(buf);
		return ERROR_DNS_NO_MEMORY;
	}

	received = recv(conn->s, (void *)buf->data, 512, 0);

	if (received == -1) {
		TALLOC_FREE(buf);
		return ERROR_DNS_SOCKET_ERROR;
	}

	if (received > 512) {
		TALLOC_FREE(buf);
		return ERROR_DNS_BAD_RESPONSE;
	}

	buf->size = received;
	buf->offset = 0;

	*presult = buf;
	return ERROR_DNS_SUCCESS;
}

DNS_ERROR dns_receive(TALLOC_CTX *mem_ctx, struct dns_connection *conn,
		      struct dns_buffer **presult)
{
	if (conn->hType == DNS_TCP) {
		return dns_receive_tcp(mem_ctx, conn, presult);
	}

	if (conn->hType == DNS_UDP) {
		return dns_receive_udp(mem_ctx, conn, presult);
	}

	return ERROR_DNS_INVALID_PARAMETER;
}

DNS_ERROR dns_transaction(TALLOC_CTX *mem_ctx, struct dns_connection *conn,
			  const struct dns_request *req,
			  struct dns_request **resp)
{
	struct dns_buffer *buf = NULL;
	DNS_ERROR err;

	err = dns_marshall_request(conn, req, &buf);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_send(conn, buf);
	if (!ERR_DNS_IS_OK(err)) goto error;
	TALLOC_FREE(buf);

	err = dns_receive(mem_ctx, conn, &buf);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_unmarshall_request(mem_ctx, buf, resp);

 error:
	TALLOC_FREE(buf);
	return err;
}

DNS_ERROR dns_update_transaction(TALLOC_CTX *mem_ctx,
				 struct dns_connection *conn,
				 struct dns_update_request *up_req,
				 struct dns_update_request **up_resp)
{
	struct dns_request *resp;
	DNS_ERROR err;

	err = dns_transaction(mem_ctx, conn, dns_update2request(up_req),
			      &resp);

	if (!ERR_DNS_IS_OK(err)) return err;

	*up_resp = dns_request2update(resp);
	return ERROR_DNS_SUCCESS;
}
