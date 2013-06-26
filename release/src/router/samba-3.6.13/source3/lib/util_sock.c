/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Tim Potter      2000-2001
   Copyright (C) Jeremy Allison  1992-2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "memcache.h"
#include "../lib/async_req/async_sock.h"
#include "../lib/util/select.h"
#include "interfaces.h"
#include "../lib/util/tevent_unix.h"
#include "../lib/util/tevent_ntstatus.h"

const char *client_name(int fd)
{
	return get_peer_name(fd,false);
}

const char *client_addr(int fd, char *addr, size_t addrlen)
{
	return get_peer_addr(fd,addr,addrlen);
}

#if 0
/* Not currently used. JRA. */
int client_socket_port(int fd)
{
	return get_socket_port(fd);
}
#endif

/****************************************************************************
 Accessor functions to make thread-safe code easier later...
****************************************************************************/

void set_smb_read_error(enum smb_read_errors *pre,
			enum smb_read_errors newerr)
{
	if (pre) {
		*pre = newerr;
	}
}

void cond_set_smb_read_error(enum smb_read_errors *pre,
			enum smb_read_errors newerr)
{
	if (pre && *pre == SMB_READ_OK) {
		*pre = newerr;
	}
}

/****************************************************************************
 Determine if a file descriptor is in fact a socket.
****************************************************************************/

bool is_a_socket(int fd)
{
	int v;
	socklen_t l;
	l = sizeof(int);
	return(getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *)&v, &l) == 0);
}

enum SOCK_OPT_TYPES {OPT_BOOL,OPT_INT,OPT_ON};

typedef struct smb_socket_option {
	const char *name;
	int level;
	int option;
	int value;
	int opttype;
} smb_socket_option;

static const smb_socket_option socket_options[] = {
  {"SO_KEEPALIVE", SOL_SOCKET, SO_KEEPALIVE, 0, OPT_BOOL},
  {"SO_REUSEADDR", SOL_SOCKET, SO_REUSEADDR, 0, OPT_BOOL},
  {"SO_BROADCAST", SOL_SOCKET, SO_BROADCAST, 0, OPT_BOOL},
#ifdef TCP_NODELAY
  {"TCP_NODELAY", IPPROTO_TCP, TCP_NODELAY, 0, OPT_BOOL},
#endif
#ifdef TCP_KEEPCNT
  {"TCP_KEEPCNT", IPPROTO_TCP, TCP_KEEPCNT, 0, OPT_INT},
#endif
#ifdef TCP_KEEPIDLE
  {"TCP_KEEPIDLE", IPPROTO_TCP, TCP_KEEPIDLE, 0, OPT_INT},
#endif
#ifdef TCP_KEEPINTVL
  {"TCP_KEEPINTVL", IPPROTO_TCP, TCP_KEEPINTVL, 0, OPT_INT},
#endif
#ifdef IPTOS_LOWDELAY
  {"IPTOS_LOWDELAY", IPPROTO_IP, IP_TOS, IPTOS_LOWDELAY, OPT_ON},
#endif
#ifdef IPTOS_THROUGHPUT
  {"IPTOS_THROUGHPUT", IPPROTO_IP, IP_TOS, IPTOS_THROUGHPUT, OPT_ON},
#endif
#ifdef SO_REUSEPORT
  {"SO_REUSEPORT", SOL_SOCKET, SO_REUSEPORT, 0, OPT_BOOL},
#endif
#ifdef SO_SNDBUF
  {"SO_SNDBUF", SOL_SOCKET, SO_SNDBUF, 0, OPT_INT},
#endif
#ifdef SO_RCVBUF
  {"SO_RCVBUF", SOL_SOCKET, SO_RCVBUF, 0, OPT_INT},
#endif
#ifdef SO_SNDLOWAT
  {"SO_SNDLOWAT", SOL_SOCKET, SO_SNDLOWAT, 0, OPT_INT},
#endif
#ifdef SO_RCVLOWAT
  {"SO_RCVLOWAT", SOL_SOCKET, SO_RCVLOWAT, 0, OPT_INT},
#endif
#ifdef SO_SNDTIMEO
  {"SO_SNDTIMEO", SOL_SOCKET, SO_SNDTIMEO, 0, OPT_INT},
#endif
#ifdef SO_RCVTIMEO
  {"SO_RCVTIMEO", SOL_SOCKET, SO_RCVTIMEO, 0, OPT_INT},
#endif
#ifdef TCP_FASTACK
  {"TCP_FASTACK", IPPROTO_TCP, TCP_FASTACK, 0, OPT_INT},
#endif
#ifdef TCP_QUICKACK
  {"TCP_QUICKACK", IPPROTO_TCP, TCP_QUICKACK, 0, OPT_BOOL},
#endif
#ifdef TCP_NODELAYACK
  {"TCP_NODELAYACK", IPPROTO_TCP, TCP_NODELAYACK, 0, OPT_BOOL},
#endif
#ifdef TCP_KEEPALIVE_THRESHOLD
  {"TCP_KEEPALIVE_THRESHOLD", IPPROTO_TCP, TCP_KEEPALIVE_THRESHOLD, 0, OPT_INT},
#endif
#ifdef TCP_KEEPALIVE_ABORT_THRESHOLD
  {"TCP_KEEPALIVE_ABORT_THRESHOLD", IPPROTO_TCP, TCP_KEEPALIVE_ABORT_THRESHOLD, 0, OPT_INT},
#endif
  {NULL,0,0,0,0}};

/****************************************************************************
 Print socket options.
****************************************************************************/

static void print_socket_options(int s)
{
	int value;
	socklen_t vlen = 4;
	const smb_socket_option *p = &socket_options[0];

	/* wrapped in if statement to prevent streams
	 * leak in SCO Openserver 5.0 */
	/* reported on samba-technical  --jerry */
	if ( DEBUGLEVEL >= 5 ) {
		DEBUG(5,("Socket options:\n"));
		for (; p->name != NULL; p++) {
			if (getsockopt(s, p->level, p->option,
						(void *)&value, &vlen) == -1) {
				DEBUGADD(5,("\tCould not test socket option %s.\n",
							p->name));
			} else {
				DEBUGADD(5,("\t%s = %d\n",
							p->name,value));
			}
		}
	}
 }

/****************************************************************************
 Set user socket options.
****************************************************************************/

void set_socket_options(int fd, const char *options)
{
	TALLOC_CTX *ctx = talloc_stackframe();
	char *tok;

	while (next_token_talloc(ctx, &options, &tok," \t,")) {
		int ret=0,i;
		int value = 1;
		char *p;
		bool got_value = false;

		if ((p = strchr_m(tok,'='))) {
			*p = 0;
			value = atoi(p+1);
			got_value = true;
		}

		for (i=0;socket_options[i].name;i++)
			if (strequal(socket_options[i].name,tok))
				break;

		if (!socket_options[i].name) {
			DEBUG(0,("Unknown socket option %s\n",tok));
			continue;
		}

		switch (socket_options[i].opttype) {
		case OPT_BOOL:
		case OPT_INT:
			ret = setsockopt(fd,socket_options[i].level,
					socket_options[i].option,
					(char *)&value,sizeof(int));
			break;

		case OPT_ON:
			if (got_value)
				DEBUG(0,("syntax error - %s "
					"does not take a value\n",tok));

			{
				int on = socket_options[i].value;
				ret = setsockopt(fd,socket_options[i].level,
					socket_options[i].option,
					(char *)&on,sizeof(int));
			}
			break;
		}

		if (ret != 0) {
			/* be aware that some systems like Solaris return
			 * EINVAL to a setsockopt() call when the client
			 * sent a RST previously - no need to worry */
			DEBUG(2,("Failed to set socket option %s (Error %s)\n",
				tok, strerror(errno) ));
		}
	}

	TALLOC_FREE(ctx);
	print_socket_options(fd);
}

/****************************************************************************
 Read from a socket.
****************************************************************************/

ssize_t read_udp_v4_socket(int fd,
			char *buf,
			size_t len,
			struct sockaddr_storage *psa)
{
	ssize_t ret;
	socklen_t socklen = sizeof(*psa);
	struct sockaddr_in *si = (struct sockaddr_in *)psa;

	memset((char *)psa,'\0',socklen);

	ret = (ssize_t)sys_recvfrom(fd,buf,len,0,
			(struct sockaddr *)psa,&socklen);
	if (ret <= 0) {
		/* Don't print a low debug error for a non-blocking socket. */
		if (errno == EAGAIN) {
			DEBUG(10,("read_udp_v4_socket: returned EAGAIN\n"));
		} else {
			DEBUG(2,("read_udp_v4_socket: failed. errno=%s\n",
				strerror(errno)));
		}
		return 0;
	}

	if (psa->ss_family != AF_INET) {
		DEBUG(2,("read_udp_v4_socket: invalid address family %d "
			"(not IPv4)\n", (int)psa->ss_family));
		return 0;
	}

	DEBUG(10,("read_udp_v4_socket: ip %s port %d read: %lu\n",
			inet_ntoa(si->sin_addr),
			si->sin_port,
			(unsigned long)ret));

	return ret;
}

/****************************************************************************
 Read data from a file descriptor with a timout in msec.
 mincount = if timeout, minimum to read before returning
 maxcount = number to be read.
 time_out = timeout in milliseconds
 NB. This can be called with a non-socket fd, don't change
 sys_read() to sys_recv() or other socket call.
****************************************************************************/

NTSTATUS read_fd_with_timeout(int fd, char *buf,
				  size_t mincnt, size_t maxcnt,
				  unsigned int time_out,
				  size_t *size_ret)
{
	int pollrtn;
	ssize_t readret;
	size_t nread = 0;

	/* just checking .... */
	if (maxcnt <= 0)
		return NT_STATUS_OK;

	/* Blocking read */
	if (time_out == 0) {
		if (mincnt == 0) {
			mincnt = maxcnt;
		}

		while (nread < mincnt) {
			readret = sys_read(fd, buf + nread, maxcnt - nread);

			if (readret == 0) {
				DEBUG(5,("read_fd_with_timeout: "
					"blocking read. EOF from client.\n"));
				return NT_STATUS_END_OF_FILE;
			}

			if (readret == -1) {
				return map_nt_error_from_unix(errno);
			}
			nread += readret;
		}
		goto done;
	}

	/* Most difficult - timeout read */
	/* If this is ever called on a disk file and
	   mincnt is greater then the filesize then
	   system performance will suffer severely as
	   select always returns true on disk files */

	for (nread=0; nread < mincnt; ) {
		int revents;

		pollrtn = poll_intr_one_fd(fd, POLLIN|POLLHUP, time_out,
					   &revents);

		/* Check if error */
		if (pollrtn == -1) {
			return map_nt_error_from_unix(errno);
		}

		/* Did we timeout ? */
		if ((pollrtn == 0) ||
		    ((revents & (POLLIN|POLLHUP|POLLERR)) == 0)) {
			DEBUG(10,("read_fd_with_timeout: timeout read. "
				"select timed out.\n"));
			return NT_STATUS_IO_TIMEOUT;
		}

		readret = sys_read(fd, buf+nread, maxcnt-nread);

		if (readret == 0) {
			/* we got EOF on the file descriptor */
			DEBUG(5,("read_fd_with_timeout: timeout read. "
				"EOF from client.\n"));
			return NT_STATUS_END_OF_FILE;
		}

		if (readret == -1) {
			return map_nt_error_from_unix(errno);
		}

		nread += readret;
	}

 done:
	/* Return the number we got */
	if (size_ret) {
		*size_ret = nread;
	}
	return NT_STATUS_OK;
}

/****************************************************************************
 Read data from an fd, reading exactly N bytes.
 NB. This can be called with a non-socket fd, don't add dependencies
 on socket calls.
****************************************************************************/

NTSTATUS read_data(int fd, char *buffer, size_t N)
{
	return read_fd_with_timeout(fd, buffer, N, N, 0, NULL);
}

/****************************************************************************
 Write all data from an iov array
 NB. This can be called with a non-socket fd, don't add dependencies
 on socket calls.
****************************************************************************/

ssize_t write_data_iov(int fd, const struct iovec *orig_iov, int iovcnt)
{
	int i;
	size_t to_send;
	ssize_t thistime;
	size_t sent;
	struct iovec *iov_copy, *iov;

	to_send = 0;
	for (i=0; i<iovcnt; i++) {
		to_send += orig_iov[i].iov_len;
	}

	thistime = sys_writev(fd, orig_iov, iovcnt);
	if ((thistime <= 0) || (thistime == to_send)) {
		return thistime;
	}
	sent = thistime;

	/*
	 * We could not send everything in one call. Make a copy of iov that
	 * we can mess with. We keep a copy of the array start in iov_copy for
	 * the TALLOC_FREE, because we're going to modify iov later on,
	 * discarding elements.
	 */

	iov_copy = (struct iovec *)TALLOC_MEMDUP(
		talloc_tos(), orig_iov, sizeof(struct iovec) * iovcnt);

	if (iov_copy == NULL) {
		errno = ENOMEM;
		return -1;
	}
	iov = iov_copy;

	while (sent < to_send) {
		/*
		 * We have to discard "thistime" bytes from the beginning
		 * iov array, "thistime" contains the number of bytes sent
		 * via writev last.
		 */
		while (thistime > 0) {
			if (thistime < iov[0].iov_len) {
				char *new_base =
					(char *)iov[0].iov_base + thistime;
				iov[0].iov_base = (void *)new_base;
				iov[0].iov_len -= thistime;
				break;
			}
			thistime -= iov[0].iov_len;
			iov += 1;
			iovcnt -= 1;
		}

		thistime = sys_writev(fd, iov, iovcnt);
		if (thistime <= 0) {
			break;
		}
		sent += thistime;
	}

	TALLOC_FREE(iov_copy);
	return sent;
}

/****************************************************************************
 Write data to a fd.
 NB. This can be called with a non-socket fd, don't add dependencies
 on socket calls.
****************************************************************************/

ssize_t write_data(int fd, const char *buffer, size_t N)
{
	struct iovec iov;

	iov.iov_base = CONST_DISCARD(void *, buffer);
	iov.iov_len = N;
	return write_data_iov(fd, &iov, 1);
}

/****************************************************************************
 Send a keepalive packet (rfc1002).
****************************************************************************/

bool send_keepalive(int client)
{
	unsigned char buf[4];

	buf[0] = SMBkeepalive;
	buf[1] = buf[2] = buf[3] = 0;

	return(write_data(client,(char *)buf,4) == 4);
}

/****************************************************************************
 Read 4 bytes of a smb packet and return the smb length of the packet.
 Store the result in the buffer.
 This version of the function will return a length of zero on receiving
 a keepalive packet.
 Timeout is in milliseconds.
****************************************************************************/

NTSTATUS read_smb_length_return_keepalive(int fd, char *inbuf,
					  unsigned int timeout,
					  size_t *len)
{
	int msg_type;
	NTSTATUS status;

	status = read_fd_with_timeout(fd, inbuf, 4, 4, timeout, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*len = smb_len(inbuf);
	msg_type = CVAL(inbuf,0);

	if (msg_type == SMBkeepalive) {
		DEBUG(5,("Got keepalive packet\n"));
	}

	DEBUG(10,("got smb length of %lu\n",(unsigned long)(*len)));

	return NT_STATUS_OK;
}

/****************************************************************************
 Read an smb from a fd.
 The timeout is in milliseconds.
 This function will return on receipt of a session keepalive packet.
 maxlen is the max number of bytes to return, not including the 4 byte
 length. If zero it means buflen limit.
 Doesn't check the MAC on signed packets.
****************************************************************************/

NTSTATUS receive_smb_raw(int fd, char *buffer, size_t buflen, unsigned int timeout,
			 size_t maxlen, size_t *p_len)
{
	size_t len;
	NTSTATUS status;

	status = read_smb_length_return_keepalive(fd,buffer,timeout,&len);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("read_fd_with_timeout failed, read "
			  "error = %s.\n", nt_errstr(status)));
		return status;
	}

	if (len > buflen) {
		DEBUG(0,("Invalid packet length! (%lu bytes).\n",
					(unsigned long)len));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if(len > 0) {
		if (maxlen) {
			len = MIN(len,maxlen);
		}

		status = read_fd_with_timeout(
			fd, buffer+4, len, len, timeout, &len);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("read_fd_with_timeout failed, read error = "
				  "%s.\n", nt_errstr(status)));
			return status;
		}

		/* not all of samba3 properly checks for packet-termination
		 * of strings. This ensures that we don't run off into
		 * empty space. */
		SSVAL(buffer+4,len, 0);
	}

	*p_len = len;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a socket of the specified type, port, and address for incoming data.
****************************************************************************/

int open_socket_in(int type,
		uint16_t port,
		int dlevel,
		const struct sockaddr_storage *psock,
		bool rebind)
{
	struct sockaddr_storage sock;
	int res;
	socklen_t slen = sizeof(struct sockaddr_in);

	sock = *psock;

#if defined(HAVE_IPV6)
	if (sock.ss_family == AF_INET6) {
		((struct sockaddr_in6 *)&sock)->sin6_port = htons(port);
		slen = sizeof(struct sockaddr_in6);
	}
#endif
	if (sock.ss_family == AF_INET) {
		((struct sockaddr_in *)&sock)->sin_port = htons(port);
	}

	res = socket(sock.ss_family, type, 0 );
	if( res == -1 ) {
		if( DEBUGLVL(0) ) {
			dbgtext( "open_socket_in(): socket() call failed: " );
			dbgtext( "%s\n", strerror( errno ) );
		}
		return -1;
	}

	/* This block sets/clears the SO_REUSEADDR and possibly SO_REUSEPORT. */
	{
		int val = rebind ? 1 : 0;
		if( setsockopt(res,SOL_SOCKET,SO_REUSEADDR,
					(char *)&val,sizeof(val)) == -1 ) {
			if( DEBUGLVL( dlevel ) ) {
				dbgtext( "open_socket_in(): setsockopt: " );
				dbgtext( "SO_REUSEADDR = %s ",
						val?"true":"false" );
				dbgtext( "on port %d failed ", port );
				dbgtext( "with error = %s\n", strerror(errno) );
			}
		}
#ifdef SO_REUSEPORT
		if( setsockopt(res,SOL_SOCKET,SO_REUSEPORT,
					(char *)&val,sizeof(val)) == -1 ) {
			if( DEBUGLVL( dlevel ) ) {
				dbgtext( "open_socket_in(): setsockopt: ");
				dbgtext( "SO_REUSEPORT = %s ",
						val?"true":"false");
				dbgtext( "on port %d failed ", port);
				dbgtext( "with error = %s\n", strerror(errno));
			}
		}
#endif /* SO_REUSEPORT */
	}

#ifdef HAVE_IPV6
	/*
	 * As IPV6_V6ONLY is the default on some systems,
	 * we better try to be consistent and always use it.
	 *
	 * This also avoids using IPv4 via AF_INET6 sockets
	 * and makes sure %I never resolves to a '::ffff:192.168.0.1'
	 * string.
	 */
	if (sock.ss_family == AF_INET6) {
		int val = 1;
		int ret;

		ret = setsockopt(res, IPPROTO_IPV6, IPV6_V6ONLY,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			if(DEBUGLVL(0)) {
				dbgtext("open_socket_in(): IPV6_ONLY failed: ");
				dbgtext("%s\n", strerror(errno));
			}
			close(res);
			return -1;
		}
	}
#endif

	/* now we've got a socket - we need to bind it */
	if (bind(res, (struct sockaddr *)&sock, slen) == -1 ) {
		if( DEBUGLVL(dlevel) && (port == SMB_PORT1 ||
				port == SMB_PORT2 || port == NMB_PORT) ) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr),
					&sock);
			dbgtext( "bind failed on port %d ", port);
			dbgtext( "socket_addr = %s.\n", addr);
			dbgtext( "Error = %s\n", strerror(errno));
		}
		close(res);
		return -1;
	}

	DEBUG( 10, ( "bind succeeded on port %d\n", port ) );
	return( res );
 }

struct open_socket_out_state {
	int fd;
	struct event_context *ev;
	struct sockaddr_storage ss;
	socklen_t salen;
	uint16_t port;
	int wait_nsec;
};

static void open_socket_out_connected(struct tevent_req *subreq);

static int open_socket_out_state_destructor(struct open_socket_out_state *s)
{
	if (s->fd != -1) {
		close(s->fd);
	}
	return 0;
}

/****************************************************************************
 Create an outgoing socket. timeout is in milliseconds.
**************************************************************************/

struct tevent_req *open_socket_out_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					const struct sockaddr_storage *pss,
					uint16_t port,
					int timeout)
{
	char addr[INET6_ADDRSTRLEN];
	struct tevent_req *result, *subreq;
	struct open_socket_out_state *state;
	NTSTATUS status;

	result = tevent_req_create(mem_ctx, &state,
				   struct open_socket_out_state);
	if (result == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->ss = *pss;
	state->port = port;
	state->wait_nsec = 10000;
	state->salen = -1;

	state->fd = socket(state->ss.ss_family, SOCK_STREAM, 0);
	if (state->fd == -1) {
		status = map_nt_error_from_unix(errno);
		goto post_status;
	}
	talloc_set_destructor(state, open_socket_out_state_destructor);

	if (!tevent_req_set_endtime(
		    result, ev, timeval_current_ofs(0, timeout*1000))) {
		goto fail;
	}

#if defined(HAVE_IPV6)
	if (pss->ss_family == AF_INET6) {
		struct sockaddr_in6 *psa6;
		psa6 = (struct sockaddr_in6 *)&state->ss;
		psa6->sin6_port = htons(port);
		if (psa6->sin6_scope_id == 0
		    && IN6_IS_ADDR_LINKLOCAL(&psa6->sin6_addr)) {
			setup_linklocal_scope_id(
				(struct sockaddr *)&(state->ss));
		}
		state->salen = sizeof(struct sockaddr_in6);
	}
#endif
	if (pss->ss_family == AF_INET) {
		struct sockaddr_in *psa;
		psa = (struct sockaddr_in *)&state->ss;
		psa->sin_port = htons(port);
		state->salen = sizeof(struct sockaddr_in);
	}

	if (pss->ss_family == AF_UNIX) {
		state->salen = sizeof(struct sockaddr_un);
	}

	print_sockaddr(addr, sizeof(addr), &state->ss);
	DEBUG(3,("Connecting to %s at port %u\n", addr,	(unsigned int)port));

	subreq = async_connect_send(state, state->ev, state->fd,
				    (struct sockaddr *)&state->ss,
				    state->salen);
	if ((subreq == NULL)
	    || !tevent_req_set_endtime(
		    subreq, state->ev,
		    timeval_current_ofs(0, state->wait_nsec))) {
		goto fail;
	}
	tevent_req_set_callback(subreq, open_socket_out_connected, result);
	return result;

 post_status:
	tevent_req_nterror(result, status);
	return tevent_req_post(result, ev);
 fail:
	TALLOC_FREE(result);
	return NULL;
}

static void open_socket_out_connected(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct open_socket_out_state *state =
		tevent_req_data(req, struct open_socket_out_state);
	int ret;
	int sys_errno;

	ret = async_connect_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == 0) {
		tevent_req_done(req);
		return;
	}

	if (
#ifdef ETIMEDOUT
		(sys_errno == ETIMEDOUT) ||
#endif
		(sys_errno == EINPROGRESS) ||
		(sys_errno == EALREADY) ||
		(sys_errno == EAGAIN)) {

		/*
		 * retry
		 */

		if (state->wait_nsec < 250000) {
			state->wait_nsec *= 1.5;
		}

		subreq = async_connect_send(state, state->ev, state->fd,
					    (struct sockaddr *)&state->ss,
					    state->salen);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		if (!tevent_req_set_endtime(
			    subreq, state->ev,
			    timeval_current_ofs(0, state->wait_nsec))) {
			tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
			return;
		}
		tevent_req_set_callback(subreq, open_socket_out_connected, req);
		return;
	}

#ifdef EISCONN
	if (sys_errno == EISCONN) {
		tevent_req_done(req);
		return;
	}
#endif

	/* real error */
	tevent_req_nterror(req, map_nt_error_from_unix(sys_errno));
}

NTSTATUS open_socket_out_recv(struct tevent_req *req, int *pfd)
{
	struct open_socket_out_state *state =
		tevent_req_data(req, struct open_socket_out_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfd = state->fd;
	state->fd = -1;
	return NT_STATUS_OK;
}

/**
* @brief open a socket
*
* @param pss a struct sockaddr_storage defining the address to connect to
* @param port to connect to
* @param timeout in MILLISECONDS
* @param pfd file descriptor returned
*
* @return NTSTATUS code
*/
NTSTATUS open_socket_out(const struct sockaddr_storage *pss, uint16_t port,
			 int timeout, int *pfd)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}

	req = open_socket_out_send(frame, ev, pss, port, timeout);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll(req, ev)) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto fail;
	}
	status = open_socket_out_recv(req, pfd);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct open_socket_out_defer_state {
	struct event_context *ev;
	struct sockaddr_storage ss;
	uint16_t port;
	int timeout;
	int fd;
};

static void open_socket_out_defer_waited(struct tevent_req *subreq);
static void open_socket_out_defer_connected(struct tevent_req *subreq);

struct tevent_req *open_socket_out_defer_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      struct timeval wait_time,
					      const struct sockaddr_storage *pss,
					      uint16_t port,
					      int timeout)
{
	struct tevent_req *req, *subreq;
	struct open_socket_out_defer_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct open_socket_out_defer_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->ss = *pss;
	state->port = port;
	state->timeout = timeout;

	subreq = tevent_wakeup_send(
		state, ev,
		timeval_current_ofs(wait_time.tv_sec, wait_time.tv_usec));
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, open_socket_out_defer_waited, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void open_socket_out_defer_waited(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct open_socket_out_defer_state *state = tevent_req_data(
		req, struct open_socket_out_defer_state);
	bool ret;

	ret = tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);
	if (!ret) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	subreq = open_socket_out_send(state, state->ev, &state->ss,
				      state->port, state->timeout);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, open_socket_out_defer_connected, req);
}

static void open_socket_out_defer_connected(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct open_socket_out_defer_state *state = tevent_req_data(
		req, struct open_socket_out_defer_state);
	NTSTATUS status;

	status = open_socket_out_recv(subreq, &state->fd);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS open_socket_out_defer_recv(struct tevent_req *req, int *pfd)
{
	struct open_socket_out_defer_state *state = tevent_req_data(
		req, struct open_socket_out_defer_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfd = state->fd;
	state->fd = -1;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a connected UDP socket to host on port
**************************************************************************/

int open_udp_socket(const char *host, int port)
{
	struct sockaddr_storage ss;
	int res;

	if (!interpret_string_addr(&ss, host, 0)) {
		DEBUG(10,("open_udp_socket: can't resolve name %s\n",
			host));
		return -1;
	}

	res = socket(ss.ss_family, SOCK_DGRAM, 0);
	if (res == -1) {
		return -1;
	}

#if defined(HAVE_IPV6)
	if (ss.ss_family == AF_INET6) {
		struct sockaddr_in6 *psa6;
		psa6 = (struct sockaddr_in6 *)&ss;
		psa6->sin6_port = htons(port);
		if (psa6->sin6_scope_id == 0
				&& IN6_IS_ADDR_LINKLOCAL(&psa6->sin6_addr)) {
			setup_linklocal_scope_id(
				(struct sockaddr *)&ss);
		}
	}
#endif
        if (ss.ss_family == AF_INET) {
                struct sockaddr_in *psa;
                psa = (struct sockaddr_in *)&ss;
                psa->sin_port = htons(port);
        }

	if (sys_connect(res,(struct sockaddr *)&ss)) {
		close(res);
		return -1;
	}

	return res;
}

/*******************************************************************
 Return the IP addr of the remote end of a socket as a string.
 Optionally return the struct sockaddr_storage.
 ******************************************************************/

static const char *get_peer_addr_internal(int fd,
				char *addr_buf,
				size_t addr_buf_len,
				struct sockaddr *pss,
				socklen_t *plength)
{
	struct sockaddr_storage ss;
	socklen_t length = sizeof(ss);

	strlcpy(addr_buf,"0.0.0.0",addr_buf_len);

	if (fd == -1) {
		return addr_buf;
	}

	if (pss == NULL) {
		pss = (struct sockaddr *)&ss;
		plength = &length;
	}

	if (getpeername(fd, (struct sockaddr *)pss, plength) < 0) {
		int level = (errno == ENOTCONN) ? 2 : 0;
		DEBUG(level, ("getpeername failed. Error was %s\n",
			       strerror(errno)));
		return addr_buf;
	}

	print_sockaddr_len(addr_buf,
			addr_buf_len,
			pss,
			*plength);
	return addr_buf;
}

/*******************************************************************
 Matchname - determine if host name matches IP address. Used to
 confirm a hostname lookup to prevent spoof attacks.
******************************************************************/

static bool matchname(const char *remotehost,
		const struct sockaddr *pss,
		socklen_t len)
{
	struct addrinfo *res = NULL;
	struct addrinfo *ailist = NULL;
	char addr_buf[INET6_ADDRSTRLEN];
	bool ret = interpret_string_addr_internal(&ailist,
			remotehost,
			AI_ADDRCONFIG|AI_CANONNAME);

	if (!ret || ailist == NULL) {
		DEBUG(3,("matchname: getaddrinfo failed for "
			"name %s [%s]\n",
			remotehost,
			gai_strerror(ret) ));
		return false;
	}

	/*
	 * Make sure that getaddrinfo() returns the "correct" host name.
	 */

	if (ailist->ai_canonname == NULL ||
		(!strequal(remotehost, ailist->ai_canonname) &&
		 !strequal(remotehost, "localhost"))) {
		DEBUG(0,("matchname: host name/name mismatch: %s != %s\n",
			 remotehost,
			 ailist->ai_canonname ?
				 ailist->ai_canonname : "(NULL)"));
		freeaddrinfo(ailist);
		return false;
	}

	/* Look up the host address in the address list we just got. */
	for (res = ailist; res; res = res->ai_next) {
		if (!res->ai_addr) {
			continue;
		}
		if (sockaddr_equal((const struct sockaddr *)res->ai_addr,
					(struct sockaddr *)pss)) {
			freeaddrinfo(ailist);
			return true;
		}
	}

	/*
	 * The host name does not map to the original host address. Perhaps
	 * someone has compromised a name server. More likely someone botched
	 * it, but that could be dangerous, too.
	 */

	DEBUG(0,("matchname: host name/address mismatch: %s != %s\n",
		print_sockaddr_len(addr_buf,
			sizeof(addr_buf),
			pss,
			len),
		 ailist->ai_canonname ? ailist->ai_canonname : "(NULL)"));

	if (ailist) {
		freeaddrinfo(ailist);
	}
	return false;
}

/*******************************************************************
 Deal with the singleton cache.
******************************************************************/

struct name_addr_pair {
	struct sockaddr_storage ss;
	const char *name;
};

/*******************************************************************
 Lookup a name/addr pair. Returns memory allocated from memcache.
******************************************************************/

static bool lookup_nc(struct name_addr_pair *nc)
{
	DATA_BLOB tmp;

	ZERO_STRUCTP(nc);

	if (!memcache_lookup(
			NULL, SINGLETON_CACHE,
			data_blob_string_const_null("get_peer_name"),
			&tmp)) {
		return false;
	}

	memcpy(&nc->ss, tmp.data, sizeof(nc->ss));
	nc->name = (const char *)tmp.data + sizeof(nc->ss);
	return true;
}

/*******************************************************************
 Save a name/addr pair.
******************************************************************/

static void store_nc(const struct name_addr_pair *nc)
{
	DATA_BLOB tmp;
	size_t namelen = strlen(nc->name);

	tmp = data_blob(NULL, sizeof(nc->ss) + namelen + 1);
	if (!tmp.data) {
		return;
	}
	memcpy(tmp.data, &nc->ss, sizeof(nc->ss));
	memcpy(tmp.data+sizeof(nc->ss), nc->name, namelen+1);

	memcache_add(NULL, SINGLETON_CACHE,
			data_blob_string_const_null("get_peer_name"),
			tmp);
	data_blob_free(&tmp);
}

/*******************************************************************
 Return the DNS name of the remote end of a socket.
******************************************************************/

const char *get_peer_name(int fd, bool force_lookup)
{
	struct name_addr_pair nc;
	char addr_buf[INET6_ADDRSTRLEN];
	struct sockaddr_storage ss;
	socklen_t length = sizeof(ss);
	const char *p;
	int ret;
	char name_buf[MAX_DNS_NAME_LENGTH];
	char tmp_name[MAX_DNS_NAME_LENGTH];

	/* reverse lookups can be *very* expensive, and in many
	   situations won't work because many networks don't link dhcp
	   with dns. To avoid the delay we avoid the lookup if
	   possible */
	if (!lp_hostname_lookups() && (force_lookup == false)) {
		length = sizeof(nc.ss);
		nc.name = get_peer_addr_internal(fd, addr_buf, sizeof(addr_buf),
			(struct sockaddr *)&nc.ss, &length);
		store_nc(&nc);
		lookup_nc(&nc);
		return nc.name ? nc.name : "UNKNOWN";
	}

	lookup_nc(&nc);

	memset(&ss, '\0', sizeof(ss));
	p = get_peer_addr_internal(fd, addr_buf, sizeof(addr_buf), (struct sockaddr *)&ss, &length);

	/* it might be the same as the last one - save some DNS work */
	if (sockaddr_equal((struct sockaddr *)&ss, (struct sockaddr *)&nc.ss)) {
		return nc.name ? nc.name : "UNKNOWN";
	}

	/* Not the same. We need to lookup. */
	if (fd == -1) {
		return "UNKNOWN";
	}

	/* Look up the remote host name. */
	ret = sys_getnameinfo((struct sockaddr *)&ss,
			length,
			name_buf,
			sizeof(name_buf),
			NULL,
			0,
			0);

	if (ret) {
		DEBUG(1,("get_peer_name: getnameinfo failed "
			"for %s with error %s\n",
			p,
			gai_strerror(ret)));
		strlcpy(name_buf, p, sizeof(name_buf));
	} else {
		if (!matchname(name_buf, (struct sockaddr *)&ss, length)) {
			DEBUG(0,("Matchname failed on %s %s\n",name_buf,p));
			strlcpy(name_buf,"UNKNOWN",sizeof(name_buf));
		}
	}

	/* can't pass the same source and dest strings in when you
	   use --enable-developer or the clobber_region() call will
	   get you */

	strlcpy(tmp_name, name_buf, sizeof(tmp_name));
	alpha_strcpy(name_buf, tmp_name, "_-.", sizeof(name_buf));
	if (strstr(name_buf,"..")) {
		strlcpy(name_buf, "UNKNOWN", sizeof(name_buf));
	}

	nc.name = name_buf;
	nc.ss = ss;

	store_nc(&nc);
	lookup_nc(&nc);
	return nc.name ? nc.name : "UNKNOWN";
}

/*******************************************************************
 Return the IP addr of the remote end of a socket as a string.
 ******************************************************************/

const char *get_peer_addr(int fd, char *addr, size_t addr_len)
{
	return get_peer_addr_internal(fd, addr, addr_len, NULL, NULL);
}

/*******************************************************************
 Create protected unix domain socket.

 Some unixes cannot set permissions on a ux-dom-sock, so we
 have to make sure that the directory contains the protection
 permissions instead.
 ******************************************************************/

int create_pipe_sock(const char *socket_dir,
		     const char *socket_name,
		     mode_t dir_perms)
{
#ifdef HAVE_UNIXSOCKET
	struct sockaddr_un sunaddr;
	struct stat st;
	int sock;
	mode_t old_umask;
	char *path = NULL;

	old_umask = umask(0);

	/* Create the socket directory or reuse the existing one */

	if (lstat(socket_dir, &st) == -1) {
		if (errno == ENOENT) {
			/* Create directory */
			if (mkdir(socket_dir, dir_perms) == -1) {
				DEBUG(0, ("error creating socket directory "
					"%s: %s\n", socket_dir,
					strerror(errno)));
				goto out_umask;
			}
		} else {
			DEBUG(0, ("lstat failed on socket directory %s: %s\n",
				socket_dir, strerror(errno)));
			goto out_umask;
		}
	} else {
		/* Check ownership and permission on existing directory */
		if (!S_ISDIR(st.st_mode)) {
			DEBUG(0, ("socket directory %s isn't a directory\n",
				socket_dir));
			goto out_umask;
		}
		if ((st.st_uid != sec_initial_uid()) ||
				((st.st_mode & 0777) != dir_perms)) {
			DEBUG(0, ("invalid permissions on socket directory "
				"%s\n", socket_dir));
			goto out_umask;
		}
	}

	/* Create the socket file */

	sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock == -1) {
		DEBUG(0, ("create_pipe_sock: socket error %s\n",
			strerror(errno) ));
                goto out_close;
	}

	if (asprintf(&path, "%s/%s", socket_dir, socket_name) == -1) {
                goto out_close;
	}

	unlink(path);
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, path, sizeof(sunaddr.sun_path));

	if (bind(sock, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) == -1) {
		DEBUG(0, ("bind failed on pipe socket %s: %s\n", path,
			strerror(errno)));
		goto out_close;
	}

	if (listen(sock, 5) == -1) {
		DEBUG(0, ("listen failed on pipe socket %s: %s\n", path,
			strerror(errno)));
		goto out_close;
	}

	SAFE_FREE(path);

	umask(old_umask);
	return sock;

out_close:
	SAFE_FREE(path);
	if (sock != -1)
		close(sock);

out_umask:
	umask(old_umask);
	return -1;

#else
        DEBUG(0, ("create_pipe_sock: No Unix sockets on this system\n"));
        return -1;
#endif /* HAVE_UNIXSOCKET */
}

/****************************************************************************
 Get my own canonical name, including domain.
****************************************************************************/

const char *get_mydnsfullname(void)
{
	struct addrinfo *res = NULL;
	char my_hostname[HOST_NAME_MAX];
	bool ret;
	DATA_BLOB tmp;

	if (memcache_lookup(NULL, SINGLETON_CACHE,
			data_blob_string_const_null("get_mydnsfullname"),
			&tmp)) {
		SMB_ASSERT(tmp.length > 0);
		return (const char *)tmp.data;
	}

	/* get my host name */
	if (gethostname(my_hostname, sizeof(my_hostname)) == -1) {
		DEBUG(0,("get_mydnsfullname: gethostname failed\n"));
		return NULL;
	}

	/* Ensure null termination. */
	my_hostname[sizeof(my_hostname)-1] = '\0';

	ret = interpret_string_addr_internal(&res,
				my_hostname,
				AI_ADDRCONFIG|AI_CANONNAME);

	if (!ret || res == NULL) {
		DEBUG(3,("get_mydnsfullname: getaddrinfo failed for "
			"name %s [%s]\n",
			my_hostname,
			gai_strerror(ret) ));
		return NULL;
	}

	/*
	 * Make sure that getaddrinfo() returns the "correct" host name.
	 */

	if (res->ai_canonname == NULL) {
		DEBUG(3,("get_mydnsfullname: failed to get "
			"canonical name for %s\n",
			my_hostname));
		freeaddrinfo(res);
		return NULL;
	}

	/* This copies the data, so we must do a lookup
	 * afterwards to find the value to return.
	 */

	memcache_add(NULL, SINGLETON_CACHE,
			data_blob_string_const_null("get_mydnsfullname"),
			data_blob_string_const_null(res->ai_canonname));

	if (!memcache_lookup(NULL, SINGLETON_CACHE,
			data_blob_string_const_null("get_mydnsfullname"),
			&tmp)) {
		tmp = data_blob_talloc(talloc_tos(), res->ai_canonname,
				strlen(res->ai_canonname) + 1);
	}

	freeaddrinfo(res);

	return (const char *)tmp.data;
}

/************************************************************
 Is this my ip address ?
************************************************************/

static bool is_my_ipaddr(const char *ipaddr_str)
{
	struct sockaddr_storage ss;
	struct iface_struct *nics;
	int i, n;

	if (!interpret_string_addr(&ss, ipaddr_str, AI_NUMERICHOST)) {
		return false;
	}

	if (ismyaddr((struct sockaddr *)&ss)) {
		return true;
	}

	if (is_zero_addr(&ss) ||
		is_loopback_addr((struct sockaddr *)&ss)) {
		return false;
	}

	n = get_interfaces(talloc_tos(), &nics);
	for (i=0; i<n; i++) {
		if (sockaddr_equal((struct sockaddr *)&nics[i].ip, (struct sockaddr *)&ss)) {
			TALLOC_FREE(nics);
			return true;
		}
	}
	TALLOC_FREE(nics);
	return false;
}

/************************************************************
 Is this my name ?
************************************************************/

bool is_myname_or_ipaddr(const char *s)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *name = NULL;
	const char *dnsname;
	char *servername = NULL;

	if (!s) {
		return false;
	}

	/* Santize the string from '\\name' */
	name = talloc_strdup(ctx, s);
	if (!name) {
		return false;
	}

	servername = strrchr_m(name, '\\' );
	if (!servername) {
		servername = name;
	} else {
		servername++;
	}

	/* Optimize for the common case */
	if (strequal(servername, global_myname())) {
		return true;
	}

	/* Check for an alias */
	if (is_myname(servername)) {
		return true;
	}

	/* Check for loopback */
	if (strequal(servername, "127.0.0.1") ||
			strequal(servername, "::1")) {
		return true;
	}

	if (strequal(servername, "localhost")) {
		return true;
	}

	/* Maybe it's my dns name */
	dnsname = get_mydnsfullname();
	if (dnsname && strequal(servername, dnsname)) {
		return true;
	}

	/* Maybe its an IP address? */
	if (is_ipaddress(servername)) {
		return is_my_ipaddr(servername);
	}

	/* Handle possible CNAME records - convert to an IP addr. list. */
	{
		/* Use DNS to resolve the name, check all addresses. */
		struct addrinfo *p = NULL;
		struct addrinfo *res = NULL;

		if (!interpret_string_addr_internal(&res,
				servername,
				AI_ADDRCONFIG)) {
			return false;
		}

		for (p = res; p; p = p->ai_next) {
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_storage ss;

			ZERO_STRUCT(ss);
			memcpy(&ss, p->ai_addr, p->ai_addrlen);
			print_sockaddr(addr,
					sizeof(addr),
					&ss);
			if (is_my_ipaddr(addr)) {
				freeaddrinfo(res);
				return true;
			}
		}
		freeaddrinfo(res);
	}

	/* No match */
	return false;
}

struct getaddrinfo_state {
	const char *node;
	const char *service;
	const struct addrinfo *hints;
	struct addrinfo *res;
	int ret;
};

static void getaddrinfo_do(void *private_data);
static void getaddrinfo_done(struct tevent_req *subreq);

struct tevent_req *getaddrinfo_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct fncall_context *ctx,
				    const char *node,
				    const char *service,
				    const struct addrinfo *hints)
{
	struct tevent_req *req, *subreq;
	struct getaddrinfo_state *state;

	req = tevent_req_create(mem_ctx, &state, struct getaddrinfo_state);
	if (req == NULL) {
		return NULL;
	}

	state->node = node;
	state->service = service;
	state->hints = hints;

	subreq = fncall_send(state, ev, ctx, getaddrinfo_do, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, getaddrinfo_done, req);
	return req;
}

static void getaddrinfo_do(void *private_data)
{
	struct getaddrinfo_state *state =
		(struct getaddrinfo_state *)private_data;

	state->ret = getaddrinfo(state->node, state->service, state->hints,
				 &state->res);
}

static void getaddrinfo_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	int ret, err;

	ret = fncall_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, err);
		return;
	}
	tevent_req_done(req);
}

int getaddrinfo_recv(struct tevent_req *req, struct addrinfo **res)
{
	struct getaddrinfo_state *state = tevent_req_data(
		req, struct getaddrinfo_state);
	int err;

	if (tevent_req_is_unix_error(req, &err)) {
		switch(err) {
		case ENOMEM:
			return EAI_MEMORY;
		default:
			return EAI_FAIL;
		}
	}
	if (state->ret == 0) {
		*res = state->res;
	}
	return state->ret;
}

int poll_one_fd(int fd, int events, int timeout, int *revents)
{
	struct pollfd *fds;
	int ret;
	int saved_errno;

	fds = TALLOC_ZERO_ARRAY(talloc_tos(), struct pollfd, 2);
	if (fds == NULL) {
		errno = ENOMEM;
		return -1;
	}
	fds[0].fd = fd;
	fds[0].events = events;

	ret = sys_poll(fds, 1, timeout);

	/*
	 * Assign whatever poll did, even in the ret<=0 case.
	 */
	*revents = fds[0].revents;
	saved_errno = errno;
	TALLOC_FREE(fds);
	errno = saved_errno;

	return ret;
}

int poll_intr_one_fd(int fd, int events, int timeout, int *revents)
{
	struct pollfd pfd;
	int ret;

	pfd.fd = fd;
	pfd.events = events;

	ret = sys_poll_intr(&pfd, 1, timeout);
	if (ret <= 0) {
		*revents = 0;
		return ret;
	}
	*revents = pfd.revents;
	return 1;
}
