/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tsocket
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/filesys.h"
#include "system/network.h"
#include "tsocket.h"
#include "tsocket_internal.h"

static int tsocket_bsd_error_from_errno(int ret,
					int sys_errno,
					bool *retry)
{
	*retry = false;

	if (ret >= 0) {
		return 0;
	}

	if (ret != -1) {
		return EIO;
	}

	if (sys_errno == 0) {
		return EIO;
	}

	if (sys_errno == EINTR) {
		*retry = true;
		return sys_errno;
	}

	if (sys_errno == EINPROGRESS) {
		*retry = true;
		return sys_errno;
	}

	if (sys_errno == EAGAIN) {
		*retry = true;
		return sys_errno;
	}

#ifdef EWOULDBLOCK
	if (sys_errno == EWOULDBLOCK) {
		*retry = true;
		return sys_errno;
	}
#endif

	return sys_errno;
}

static int tsocket_bsd_common_prepare_fd(int fd, bool high_fd)
{
	int i;
	int sys_errno = 0;
	int fds[3];
	int num_fds = 0;

	int result, flags;

	if (fd == -1) {
		return -1;
	}

	/* first make a fd >= 3 */
	if (high_fd) {
		while (fd < 3) {
			fds[num_fds++] = fd;
			fd = dup(fd);
			if (fd == -1) {
				sys_errno = errno;
				break;
			}
		}
		for (i=0; i<num_fds; i++) {
			close(fds[i]);
		}
		if (fd == -1) {
			errno = sys_errno;
			return fd;
		}
	}

	/* fd should be nonblocking. */

#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

	if ((flags = fcntl(fd, F_GETFL)) == -1) {
		goto fail;
	}

	flags |= FLAG_TO_SET;
	if (fcntl(fd, F_SETFL, flags) == -1) {
		goto fail;
	}

#undef FLAG_TO_SET

	/* fd should be closed on exec() */
#ifdef FD_CLOEXEC
	result = flags = fcntl(fd, F_GETFD, 0);
	if (flags >= 0) {
		flags |= FD_CLOEXEC;
		result = fcntl(fd, F_SETFD, flags);
	}
	if (result < 0) {
		goto fail;
	}
#endif
	return fd;

 fail:
	if (fd != -1) {
		sys_errno = errno;
		close(fd);
		errno = sys_errno;
	}
	return -1;
}

static ssize_t tsocket_bsd_pending(int fd)
{
	int ret, error;
	int value = 0;
	socklen_t len;

	ret = ioctl(fd, FIONREAD, &value);
	if (ret == -1) {
		return ret;
	}

	if (ret != 0) {
		/* this should not be reached */
		errno = EIO;
		return -1;
	}

	if (value != 0) {
		return value;
	}

	error = 0;
	len = sizeof(error);

	/*
	 * if no data is available check if the socket is in error state. For
	 * dgram sockets it's the way to return ICMP error messages of
	 * connected sockets to the caller.
	 */
	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == -1) {
		return ret;
	}
	if (error != 0) {
		errno = error;
		return -1;
	}
	return 0;
}

static const struct tsocket_address_ops tsocket_address_bsd_ops;

struct tsocket_address_bsd {
	socklen_t sa_socklen;
	union {
		struct sockaddr sa;
		struct sockaddr_in in;
#ifdef HAVE_IPV6
		struct sockaddr_in6 in6;
#endif
		struct sockaddr_un un;
		struct sockaddr_storage ss;
	} u;
};

int _tsocket_address_bsd_from_sockaddr(TALLOC_CTX *mem_ctx,
				       struct sockaddr *sa,
				       size_t sa_socklen,
				       struct tsocket_address **_addr,
				       const char *location)
{
	struct tsocket_address *addr;
	struct tsocket_address_bsd *bsda;

	if (sa_socklen < sizeof(sa->sa_family)) {
		errno = EINVAL;
		return -1;
	}

	switch (sa->sa_family) {
	case AF_UNIX:
		if (sa_socklen > sizeof(struct sockaddr_un)) {
			sa_socklen = sizeof(struct sockaddr_un);
		}
		break;
	case AF_INET:
		if (sa_socklen < sizeof(struct sockaddr_in)) {
			errno = EINVAL;
			return -1;
		}
		sa_socklen = sizeof(struct sockaddr_in);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		if (sa_socklen < sizeof(struct sockaddr_in6)) {
			errno = EINVAL;
			return -1;
		}
		sa_socklen = sizeof(struct sockaddr_in6);
		break;
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (sa_socklen > sizeof(struct sockaddr_storage)) {
		errno = EINVAL;
		return -1;
	}

	addr = tsocket_address_create(mem_ctx,
				      &tsocket_address_bsd_ops,
				      &bsda,
				      struct tsocket_address_bsd,
				      location);
	if (!addr) {
		errno = ENOMEM;
		return -1;
	}

	ZERO_STRUCTP(bsda);

	memcpy(&bsda->u.ss, sa, sa_socklen);

	bsda->sa_socklen = sa_socklen;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	bsda->u.sa.sa_len = bsda->sa_socklen;
#endif

	*_addr = addr;
	return 0;
}

ssize_t tsocket_address_bsd_sockaddr(const struct tsocket_address *addr,
				     struct sockaddr *sa,
				     size_t sa_socklen)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);

	if (!bsda) {
		errno = EINVAL;
		return -1;
	}

	if (sa_socklen < bsda->sa_socklen) {
		errno = EINVAL;
		return -1;
	}

	if (sa_socklen > bsda->sa_socklen) {
		memset(sa, 0, sa_socklen);
		sa_socklen = bsda->sa_socklen;
	}

	memcpy(sa, &bsda->u.ss, sa_socklen);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	sa->sa_len = sa_socklen;
#endif
	return sa_socklen;
}

bool tsocket_address_is_inet(const struct tsocket_address *addr, const char *fam)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);

	if (!bsda) {
		return false;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_INET:
		if (strcasecmp(fam, "ip") == 0) {
			return true;
		}

		if (strcasecmp(fam, "ipv4") == 0) {
			return true;
		}

		return false;
#ifdef HAVE_IPV6
	case AF_INET6:
		if (strcasecmp(fam, "ip") == 0) {
			return true;
		}

		if (strcasecmp(fam, "ipv6") == 0) {
			return true;
		}

		return false;
#endif
	}

	return false;
}

int _tsocket_address_inet_from_strings(TALLOC_CTX *mem_ctx,
				       const char *fam,
				       const char *addr,
				       uint16_t port,
				       struct tsocket_address **_addr,
				       const char *location)
{
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	char port_str[6];
	int ret;

	ZERO_STRUCT(hints);
	/*
	 * we use SOCKET_STREAM here to get just one result
	 * back from getaddrinfo().
	 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

	if (strcasecmp(fam, "ip") == 0) {
		hints.ai_family = AF_UNSPEC;
		if (!addr) {
#ifdef HAVE_IPV6
			addr = "::";
#else
			addr = "0.0.0.0";
#endif
		}
	} else if (strcasecmp(fam, "ipv4") == 0) {
		hints.ai_family = AF_INET;
		if (!addr) {
			addr = "0.0.0.0";
		}
#ifdef HAVE_IPV6
	} else if (strcasecmp(fam, "ipv6") == 0) {
		hints.ai_family = AF_INET6;
		if (!addr) {
			addr = "::";
		}
#endif
	} else {
		errno = EAFNOSUPPORT;
		return -1;
	}

	snprintf(port_str, sizeof(port_str) - 1, "%u", port);

	ret = getaddrinfo(addr, port_str, &hints, &result);
	if (ret != 0) {
		switch (ret) {
		case EAI_FAIL:
			errno = EINVAL;
			break;
		}
		ret = -1;
		goto done;
	}

	if (result->ai_socktype != SOCK_STREAM) {
		errno = EINVAL;
		ret = -1;
		goto done;
	}

	ret = _tsocket_address_bsd_from_sockaddr(mem_ctx,
						  result->ai_addr,
						  result->ai_addrlen,
						  _addr,
						  location);

done:
	if (result) {
		freeaddrinfo(result);
	}
	return ret;
}

char *tsocket_address_inet_addr_string(const struct tsocket_address *addr,
				       TALLOC_CTX *mem_ctx)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);
	char addr_str[INET6_ADDRSTRLEN+1];
	const char *str;

	if (!bsda) {
		errno = EINVAL;
		return NULL;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_INET:
		str = inet_ntop(bsda->u.in.sin_family,
				&bsda->u.in.sin_addr,
				addr_str, sizeof(addr_str));
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		str = inet_ntop(bsda->u.in6.sin6_family,
				&bsda->u.in6.sin6_addr,
				addr_str, sizeof(addr_str));
		break;
#endif
	default:
		errno = EINVAL;
		return NULL;
	}

	if (!str) {
		return NULL;
	}

	return talloc_strdup(mem_ctx, str);
}

uint16_t tsocket_address_inet_port(const struct tsocket_address *addr)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);
	uint16_t port = 0;

	if (!bsda) {
		errno = EINVAL;
		return 0;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_INET:
		port = ntohs(bsda->u.in.sin_port);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		port = ntohs(bsda->u.in6.sin6_port);
		break;
#endif
	default:
		errno = EINVAL;
		return 0;
	}

	return port;
}

int tsocket_address_inet_set_port(struct tsocket_address *addr,
				  uint16_t port)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);

	if (!bsda) {
		errno = EINVAL;
		return -1;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_INET:
		bsda->u.in.sin_port = htons(port);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		bsda->u.in6.sin6_port = htons(port);
		break;
#endif
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}

bool tsocket_address_is_unix(const struct tsocket_address *addr)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);

	if (!bsda) {
		return false;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_UNIX:
		return true;
	}

	return false;
}

int _tsocket_address_unix_from_path(TALLOC_CTX *mem_ctx,
				    const char *path,
				    struct tsocket_address **_addr,
				    const char *location)
{
	struct sockaddr_un un;
	void *p = &un;
	int ret;

	if (!path) {
		path = "";
	}

	if (strlen(path) > sizeof(un.sun_path)-1) {
		errno = ENAMETOOLONG;
		return -1;
	}

	ZERO_STRUCT(un);
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, path, sizeof(un.sun_path)-1);

	ret = _tsocket_address_bsd_from_sockaddr(mem_ctx,
						 (struct sockaddr *)p,
						 sizeof(un),
						 _addr,
						 location);

	return ret;
}

char *tsocket_address_unix_path(const struct tsocket_address *addr,
				TALLOC_CTX *mem_ctx)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);
	const char *str;

	if (!bsda) {
		errno = EINVAL;
		return NULL;
	}

	switch (bsda->u.sa.sa_family) {
	case AF_UNIX:
		str = bsda->u.un.sun_path;
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	return talloc_strdup(mem_ctx, str);
}

static char *tsocket_address_bsd_string(const struct tsocket_address *addr,
					TALLOC_CTX *mem_ctx)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);
	char *str;
	char *addr_str;
	const char *prefix = NULL;
	uint16_t port;

	switch (bsda->u.sa.sa_family) {
	case AF_UNIX:
		return talloc_asprintf(mem_ctx, "unix:%s",
				       bsda->u.un.sun_path);
	case AF_INET:
		prefix = "ipv4";
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		prefix = "ipv6";
		break;
#endif
	default:
		errno = EINVAL;
		return NULL;
	}

	addr_str = tsocket_address_inet_addr_string(addr, mem_ctx);
	if (!addr_str) {
		return NULL;
	}

	port = tsocket_address_inet_port(addr);

	str = talloc_asprintf(mem_ctx, "%s:%s:%u",
			      prefix, addr_str, port);
	talloc_free(addr_str);

	return str;
}

static struct tsocket_address *tsocket_address_bsd_copy(const struct tsocket_address *addr,
							 TALLOC_CTX *mem_ctx,
							 const char *location)
{
	struct tsocket_address_bsd *bsda = talloc_get_type(addr->private_data,
					   struct tsocket_address_bsd);
	struct tsocket_address *copy;
	int ret;

	ret = _tsocket_address_bsd_from_sockaddr(mem_ctx,
						 &bsda->u.sa,
						 bsda->sa_socklen,
						 &copy,
						 location);
	if (ret != 0) {
		return NULL;
	}

	return copy;
}

static const struct tsocket_address_ops tsocket_address_bsd_ops = {
	.name		= "bsd",
	.string		= tsocket_address_bsd_string,
	.copy		= tsocket_address_bsd_copy,
};

struct tdgram_bsd {
	int fd;

	void *event_ptr;
	struct tevent_fd *fde;
	bool optimize_recvfrom;

	void *readable_private;
	void (*readable_handler)(void *private_data);
	void *writeable_private;
	void (*writeable_handler)(void *private_data);
};

bool tdgram_bsd_optimize_recvfrom(struct tdgram_context *dgram,
				  bool on)
{
	struct tdgram_bsd *bsds =
		talloc_get_type(_tdgram_context_data(dgram),
		struct tdgram_bsd);
	bool old;

	if (bsds == NULL) {
		/* not a bsd socket */
		return false;
	}

	old = bsds->optimize_recvfrom;
	bsds->optimize_recvfrom = on;

	return old;
}

static void tdgram_bsd_fde_handler(struct tevent_context *ev,
				   struct tevent_fd *fde,
				   uint16_t flags,
				   void *private_data)
{
	struct tdgram_bsd *bsds = talloc_get_type_abort(private_data,
				  struct tdgram_bsd);

	if (flags & TEVENT_FD_WRITE) {
		bsds->writeable_handler(bsds->writeable_private);
		return;
	}
	if (flags & TEVENT_FD_READ) {
		if (!bsds->readable_handler) {
			TEVENT_FD_NOT_READABLE(bsds->fde);
			return;
		}
		bsds->readable_handler(bsds->readable_private);
		return;
	}
}

static int tdgram_bsd_set_readable_handler(struct tdgram_bsd *bsds,
					   struct tevent_context *ev,
					   void (*handler)(void *private_data),
					   void *private_data)
{
	if (ev == NULL) {
		if (handler) {
			errno = EINVAL;
			return -1;
		}
		if (!bsds->readable_handler) {
			return 0;
		}
		bsds->readable_handler = NULL;
		bsds->readable_private = NULL;

		return 0;
	}

	/* read and write must use the same tevent_context */
	if (bsds->event_ptr != ev) {
		if (bsds->readable_handler || bsds->writeable_handler) {
			errno = EINVAL;
			return -1;
		}
		bsds->event_ptr = NULL;
		TALLOC_FREE(bsds->fde);
	}

	if (tevent_fd_get_flags(bsds->fde) == 0) {
		TALLOC_FREE(bsds->fde);

		bsds->fde = tevent_add_fd(ev, bsds,
					  bsds->fd, TEVENT_FD_READ,
					  tdgram_bsd_fde_handler,
					  bsds);
		if (!bsds->fde) {
			errno = ENOMEM;
			return -1;
		}

		/* cache the event context we're running on */
		bsds->event_ptr = ev;
	} else if (!bsds->readable_handler) {
		TEVENT_FD_READABLE(bsds->fde);
	}

	bsds->readable_handler = handler;
	bsds->readable_private = private_data;

	return 0;
}

static int tdgram_bsd_set_writeable_handler(struct tdgram_bsd *bsds,
					    struct tevent_context *ev,
					    void (*handler)(void *private_data),
					    void *private_data)
{
	if (ev == NULL) {
		if (handler) {
			errno = EINVAL;
			return -1;
		}
		if (!bsds->writeable_handler) {
			return 0;
		}
		bsds->writeable_handler = NULL;
		bsds->writeable_private = NULL;
		TEVENT_FD_NOT_WRITEABLE(bsds->fde);

		return 0;
	}

	/* read and write must use the same tevent_context */
	if (bsds->event_ptr != ev) {
		if (bsds->readable_handler || bsds->writeable_handler) {
			errno = EINVAL;
			return -1;
		}
		bsds->event_ptr = NULL;
		TALLOC_FREE(bsds->fde);
	}

	if (tevent_fd_get_flags(bsds->fde) == 0) {
		TALLOC_FREE(bsds->fde);

		bsds->fde = tevent_add_fd(ev, bsds,
					  bsds->fd, TEVENT_FD_WRITE,
					  tdgram_bsd_fde_handler,
					  bsds);
		if (!bsds->fde) {
			errno = ENOMEM;
			return -1;
		}

		/* cache the event context we're running on */
		bsds->event_ptr = ev;
	} else if (!bsds->writeable_handler) {
		TEVENT_FD_WRITEABLE(bsds->fde);
	}

	bsds->writeable_handler = handler;
	bsds->writeable_private = private_data;

	return 0;
}

struct tdgram_bsd_recvfrom_state {
	struct tdgram_context *dgram;

	uint8_t *buf;
	size_t len;
	struct tsocket_address *src;
};

static int tdgram_bsd_recvfrom_destructor(struct tdgram_bsd_recvfrom_state *state)
{
	struct tdgram_bsd *bsds = tdgram_context_data(state->dgram,
				  struct tdgram_bsd);

	tdgram_bsd_set_readable_handler(bsds, NULL, NULL, NULL);

	return 0;
}

static void tdgram_bsd_recvfrom_handler(void *private_data);

static struct tevent_req *tdgram_bsd_recvfrom_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tdgram_context *dgram)
{
	struct tevent_req *req;
	struct tdgram_bsd_recvfrom_state *state;
	struct tdgram_bsd *bsds = tdgram_context_data(dgram, struct tdgram_bsd);
	int ret;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_bsd_recvfrom_state);
	if (!req) {
		return NULL;
	}

	state->dgram	= dgram;
	state->buf	= NULL;
	state->len	= 0;
	state->src	= NULL;

	talloc_set_destructor(state, tdgram_bsd_recvfrom_destructor);

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}


	/*
	 * this is a fast path, not waiting for the
	 * socket to become explicit readable gains
	 * about 10%-20% performance in benchmark tests.
	 */
	if (bsds->optimize_recvfrom) {
		/*
		 * We only do the optimization on
		 * recvfrom if the caller asked for it.
		 *
		 * This is needed because in most cases
		 * we preferr to flush send buffers before
		 * receiving incoming requests.
		 */
		tdgram_bsd_recvfrom_handler(req);
		if (!tevent_req_is_in_progress(req)) {
			goto post;
		}
	}

	ret = tdgram_bsd_set_readable_handler(bsds, ev,
					      tdgram_bsd_recvfrom_handler,
					      req);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_bsd_recvfrom_handler(void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
				 struct tevent_req);
	struct tdgram_bsd_recvfrom_state *state = tevent_req_data(req,
					struct tdgram_bsd_recvfrom_state);
	struct tdgram_context *dgram = state->dgram;
	struct tdgram_bsd *bsds = tdgram_context_data(dgram, struct tdgram_bsd);
	struct tsocket_address_bsd *bsda;
	ssize_t ret;
	int err;
	bool retry;

	ret = tsocket_bsd_pending(bsds->fd);
	if (ret == 0) {
		/* retry later */
		return;
	}
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	state->buf = talloc_array(state, uint8_t, ret);
	if (tevent_req_nomem(state->buf, req)) {
		return;
	}
	state->len = ret;

	state->src = tsocket_address_create(state,
					    &tsocket_address_bsd_ops,
					    &bsda,
					    struct tsocket_address_bsd,
					    __location__ "bsd_recvfrom");
	if (tevent_req_nomem(state->src, req)) {
		return;
	}

	ZERO_STRUCTP(bsda);
	bsda->sa_socklen = sizeof(bsda->u.ss);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	bsda->u.sa.sa_len = bsda->sa_socklen;
#endif

	ret = recvfrom(bsds->fd, state->buf, state->len, 0,
		       &bsda->u.sa, &bsda->sa_socklen);
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	/*
	 * Some systems (FreeBSD, see bug #7115) return too much
	 * bytes in tsocket_bsd_pending()/ioctl(fd, FIONREAD, ...),
	 * the return value includes some IP/UDP header bytes,
	 * while recvfrom() just returns the payload.
	 */
	state->buf = talloc_realloc(state, state->buf, uint8_t, ret);
	if (tevent_req_nomem(state->buf, req)) {
		return;
	}
	state->len = ret;

	tevent_req_done(req);
}

static ssize_t tdgram_bsd_recvfrom_recv(struct tevent_req *req,
					int *perrno,
					TALLOC_CTX *mem_ctx,
					uint8_t **buf,
					struct tsocket_address **src)
{
	struct tdgram_bsd_recvfrom_state *state = tevent_req_data(req,
					struct tdgram_bsd_recvfrom_state);
	ssize_t ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		*buf = talloc_move(mem_ctx, &state->buf);
		ret = state->len;
		if (src) {
			*src = talloc_move(mem_ctx, &state->src);
		}
	}

	tevent_req_received(req);
	return ret;
}

struct tdgram_bsd_sendto_state {
	struct tdgram_context *dgram;

	const uint8_t *buf;
	size_t len;
	const struct tsocket_address *dst;

	ssize_t ret;
};

static int tdgram_bsd_sendto_destructor(struct tdgram_bsd_sendto_state *state)
{
	struct tdgram_bsd *bsds = tdgram_context_data(state->dgram,
				  struct tdgram_bsd);

	tdgram_bsd_set_writeable_handler(bsds, NULL, NULL, NULL);

	return 0;
}

static void tdgram_bsd_sendto_handler(void *private_data);

static struct tevent_req *tdgram_bsd_sendto_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct tdgram_context *dgram,
						 const uint8_t *buf,
						 size_t len,
						 const struct tsocket_address *dst)
{
	struct tevent_req *req;
	struct tdgram_bsd_sendto_state *state;
	struct tdgram_bsd *bsds = tdgram_context_data(dgram, struct tdgram_bsd);
	int ret;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_bsd_sendto_state);
	if (!req) {
		return NULL;
	}

	state->dgram	= dgram;
	state->buf	= buf;
	state->len	= len;
	state->dst	= dst;
	state->ret	= -1;

	talloc_set_destructor(state, tdgram_bsd_sendto_destructor);

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	/*
	 * this is a fast path, not waiting for the
	 * socket to become explicit writeable gains
	 * about 10%-20% performance in benchmark tests.
	 */
	tdgram_bsd_sendto_handler(req);
	if (!tevent_req_is_in_progress(req)) {
		goto post;
	}

	ret = tdgram_bsd_set_writeable_handler(bsds, ev,
					       tdgram_bsd_sendto_handler,
					       req);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_bsd_sendto_handler(void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
				 struct tevent_req);
	struct tdgram_bsd_sendto_state *state = tevent_req_data(req,
					struct tdgram_bsd_sendto_state);
	struct tdgram_context *dgram = state->dgram;
	struct tdgram_bsd *bsds = tdgram_context_data(dgram, struct tdgram_bsd);
	struct sockaddr *sa = NULL;
	socklen_t sa_socklen = 0;
	ssize_t ret;
	int err;
	bool retry;

	if (state->dst) {
		struct tsocket_address_bsd *bsda =
			talloc_get_type(state->dst->private_data,
			struct tsocket_address_bsd);

		sa = &bsda->u.sa;
		sa_socklen = bsda->sa_socklen;
	}

	ret = sendto(bsds->fd, state->buf, state->len, 0, sa, sa_socklen);
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	state->ret = ret;

	tevent_req_done(req);
}

static ssize_t tdgram_bsd_sendto_recv(struct tevent_req *req, int *perrno)
{
	struct tdgram_bsd_sendto_state *state = tevent_req_data(req,
					struct tdgram_bsd_sendto_state);
	ssize_t ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tdgram_bsd_disconnect_state {
	uint8_t __dummy;
};

static struct tevent_req *tdgram_bsd_disconnect_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct tdgram_context *dgram)
{
	struct tdgram_bsd *bsds = tdgram_context_data(dgram, struct tdgram_bsd);
	struct tevent_req *req;
	struct tdgram_bsd_disconnect_state *state;
	int ret;
	int err;
	bool dummy;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_bsd_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	TALLOC_FREE(bsds->fde);
	ret = close(bsds->fd);
	bsds->fd = -1;
	err = tsocket_bsd_error_from_errno(ret, errno, &dummy);
	if (tevent_req_error(req, err)) {
		goto post;
	}

	tevent_req_done(req);
post:
	tevent_req_post(req, ev);
	return req;
}

static int tdgram_bsd_disconnect_recv(struct tevent_req *req,
				      int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tdgram_context_ops tdgram_bsd_ops = {
	.name			= "bsd",

	.recvfrom_send		= tdgram_bsd_recvfrom_send,
	.recvfrom_recv		= tdgram_bsd_recvfrom_recv,

	.sendto_send		= tdgram_bsd_sendto_send,
	.sendto_recv		= tdgram_bsd_sendto_recv,

	.disconnect_send	= tdgram_bsd_disconnect_send,
	.disconnect_recv	= tdgram_bsd_disconnect_recv,
};

static int tdgram_bsd_destructor(struct tdgram_bsd *bsds)
{
	TALLOC_FREE(bsds->fde);
	if (bsds->fd != -1) {
		close(bsds->fd);
		bsds->fd = -1;
	}
	return 0;
}

static int tdgram_bsd_dgram_socket(const struct tsocket_address *local,
				   const struct tsocket_address *remote,
				   bool broadcast,
				   TALLOC_CTX *mem_ctx,
				   struct tdgram_context **_dgram,
				   const char *location)
{
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	struct tsocket_address_bsd *rbsda = NULL;
	struct tdgram_context *dgram;
	struct tdgram_bsd *bsds;
	int fd;
	int ret;
	bool do_bind = false;
	bool do_reuseaddr = false;
	bool do_ipv6only = false;
	bool is_inet = false;
	int sa_fam = lbsda->u.sa.sa_family;

	if (remote) {
		rbsda = talloc_get_type_abort(remote->private_data,
			struct tsocket_address_bsd);
	}

	switch (lbsda->u.sa.sa_family) {
	case AF_UNIX:
		if (broadcast) {
			errno = EINVAL;
			return -1;
		}
		if (lbsda->u.un.sun_path[0] != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		break;
	case AF_INET:
		if (lbsda->u.in.sin_port != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		if (lbsda->u.in.sin_addr.s_addr != INADDR_ANY) {
			do_bind = true;
		}
		is_inet = true;
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		if (lbsda->u.in6.sin6_port != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		if (memcmp(&in6addr_any,
			   &lbsda->u.in6.sin6_addr,
			   sizeof(in6addr_any)) != 0) {
			do_bind = true;
		}
		is_inet = true;
		do_ipv6only = true;
		break;
#endif
	default:
		errno = EINVAL;
		return -1;
	}

	if (!do_bind && is_inet && rbsda) {
		sa_fam = rbsda->u.sa.sa_family;
		switch (sa_fam) {
		case AF_INET:
			do_ipv6only = false;
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			do_ipv6only = true;
			break;
#endif
		}
	}

	fd = socket(sa_fam, SOCK_DGRAM, 0);
	if (fd < 0) {
		return -1;
	}

	fd = tsocket_bsd_common_prepare_fd(fd, true);
	if (fd < 0) {
		return -1;
	}

	dgram = tdgram_context_create(mem_ctx,
				      &tdgram_bsd_ops,
				      &bsds,
				      struct tdgram_bsd,
				      location);
	if (!dgram) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	ZERO_STRUCTP(bsds);
	bsds->fd = fd;
	talloc_set_destructor(bsds, tdgram_bsd_destructor);

#ifdef HAVE_IPV6
	if (do_ipv6only) {
		int val = 1;

		ret = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			int saved_errno = errno;
			talloc_free(dgram);
			errno = saved_errno;
			return -1;
		}
	}
#endif

	if (broadcast) {
		int val = 1;

		ret = setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			int saved_errno = errno;
			talloc_free(dgram);
			errno = saved_errno;
			return -1;
		}
	}

	if (do_reuseaddr) {
		int val = 1;

		ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			int saved_errno = errno;
			talloc_free(dgram);
			errno = saved_errno;
			return -1;
		}
	}

	if (do_bind) {
		ret = bind(fd, &lbsda->u.sa, lbsda->sa_socklen);
		if (ret == -1) {
			int saved_errno = errno;
			talloc_free(dgram);
			errno = saved_errno;
			return -1;
		}
	}

	if (rbsda) {
		if (rbsda->u.sa.sa_family != sa_fam) {
			talloc_free(dgram);
			errno = EINVAL;
			return -1;
		}

		ret = connect(fd, &rbsda->u.sa, rbsda->sa_socklen);
		if (ret == -1) {
			int saved_errno = errno;
			talloc_free(dgram);
			errno = saved_errno;
			return -1;
		}
	}

	*_dgram = dgram;
	return 0;
}

int _tdgram_inet_udp_socket(const struct tsocket_address *local,
			    const struct tsocket_address *remote,
			    TALLOC_CTX *mem_ctx,
			    struct tdgram_context **dgram,
			    const char *location)
{
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	int ret;

	switch (lbsda->u.sa.sa_family) {
	case AF_INET:
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		break;
#endif
	default:
		errno = EINVAL;
		return -1;
	}

	ret = tdgram_bsd_dgram_socket(local, remote, false,
				      mem_ctx, dgram, location);

	return ret;
}

int _tdgram_unix_socket(const struct tsocket_address *local,
			const struct tsocket_address *remote,
			TALLOC_CTX *mem_ctx,
			struct tdgram_context **dgram,
			const char *location)
{
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	int ret;

	switch (lbsda->u.sa.sa_family) {
	case AF_UNIX:
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	ret = tdgram_bsd_dgram_socket(local, remote, false,
				      mem_ctx, dgram, location);

	return ret;
}

struct tstream_bsd {
	int fd;

	void *event_ptr;
	struct tevent_fd *fde;
	bool optimize_readv;

	void *readable_private;
	void (*readable_handler)(void *private_data);
	void *writeable_private;
	void (*writeable_handler)(void *private_data);
};

bool tstream_bsd_optimize_readv(struct tstream_context *stream,
				bool on)
{
	struct tstream_bsd *bsds =
		talloc_get_type(_tstream_context_data(stream),
		struct tstream_bsd);
	bool old;

	if (bsds == NULL) {
		/* not a bsd socket */
		return false;
	}

	old = bsds->optimize_readv;
	bsds->optimize_readv = on;

	return old;
}

static void tstream_bsd_fde_handler(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    uint16_t flags,
				    void *private_data)
{
	struct tstream_bsd *bsds = talloc_get_type_abort(private_data,
				   struct tstream_bsd);

	if (flags & TEVENT_FD_WRITE) {
		bsds->writeable_handler(bsds->writeable_private);
		return;
	}
	if (flags & TEVENT_FD_READ) {
		if (!bsds->readable_handler) {
			if (bsds->writeable_handler) {
				bsds->writeable_handler(bsds->writeable_private);
				return;
			}
			TEVENT_FD_NOT_READABLE(bsds->fde);
			return;
		}
		bsds->readable_handler(bsds->readable_private);
		return;
	}
}

static int tstream_bsd_set_readable_handler(struct tstream_bsd *bsds,
					    struct tevent_context *ev,
					    void (*handler)(void *private_data),
					    void *private_data)
{
	if (ev == NULL) {
		if (handler) {
			errno = EINVAL;
			return -1;
		}
		if (!bsds->readable_handler) {
			return 0;
		}
		bsds->readable_handler = NULL;
		bsds->readable_private = NULL;

		return 0;
	}

	/* read and write must use the same tevent_context */
	if (bsds->event_ptr != ev) {
		if (bsds->readable_handler || bsds->writeable_handler) {
			errno = EINVAL;
			return -1;
		}
		bsds->event_ptr = NULL;
		TALLOC_FREE(bsds->fde);
	}

	if (tevent_fd_get_flags(bsds->fde) == 0) {
		TALLOC_FREE(bsds->fde);

		bsds->fde = tevent_add_fd(ev, bsds,
					  bsds->fd, TEVENT_FD_READ,
					  tstream_bsd_fde_handler,
					  bsds);
		if (!bsds->fde) {
			errno = ENOMEM;
			return -1;
		}

		/* cache the event context we're running on */
		bsds->event_ptr = ev;
	} else if (!bsds->readable_handler) {
		TEVENT_FD_READABLE(bsds->fde);
	}

	bsds->readable_handler = handler;
	bsds->readable_private = private_data;

	return 0;
}

static int tstream_bsd_set_writeable_handler(struct tstream_bsd *bsds,
					     struct tevent_context *ev,
					     void (*handler)(void *private_data),
					     void *private_data)
{
	if (ev == NULL) {
		if (handler) {
			errno = EINVAL;
			return -1;
		}
		if (!bsds->writeable_handler) {
			return 0;
		}
		bsds->writeable_handler = NULL;
		bsds->writeable_private = NULL;
		TEVENT_FD_NOT_WRITEABLE(bsds->fde);

		return 0;
	}

	/* read and write must use the same tevent_context */
	if (bsds->event_ptr != ev) {
		if (bsds->readable_handler || bsds->writeable_handler) {
			errno = EINVAL;
			return -1;
		}
		bsds->event_ptr = NULL;
		TALLOC_FREE(bsds->fde);
	}

	if (tevent_fd_get_flags(bsds->fde) == 0) {
		TALLOC_FREE(bsds->fde);

		bsds->fde = tevent_add_fd(ev, bsds,
					  bsds->fd,
					  TEVENT_FD_READ | TEVENT_FD_WRITE,
					  tstream_bsd_fde_handler,
					  bsds);
		if (!bsds->fde) {
			errno = ENOMEM;
			return -1;
		}

		/* cache the event context we're running on */
		bsds->event_ptr = ev;
	} else if (!bsds->writeable_handler) {
		uint16_t flags = tevent_fd_get_flags(bsds->fde);
		flags |= TEVENT_FD_READ | TEVENT_FD_WRITE;
		tevent_fd_set_flags(bsds->fde, flags);
	}

	bsds->writeable_handler = handler;
	bsds->writeable_private = private_data;

	return 0;
}

static ssize_t tstream_bsd_pending_bytes(struct tstream_context *stream)
{
	struct tstream_bsd *bsds = tstream_context_data(stream,
				   struct tstream_bsd);
	ssize_t ret;

	if (bsds->fd == -1) {
		errno = ENOTCONN;
		return -1;
	}

	ret = tsocket_bsd_pending(bsds->fd);

	return ret;
}

struct tstream_bsd_readv_state {
	struct tstream_context *stream;

	struct iovec *vector;
	size_t count;

	int ret;
};

static int tstream_bsd_readv_destructor(struct tstream_bsd_readv_state *state)
{
	struct tstream_bsd *bsds = tstream_context_data(state->stream,
				   struct tstream_bsd);

	tstream_bsd_set_readable_handler(bsds, NULL, NULL, NULL);

	return 0;
}

static void tstream_bsd_readv_handler(void *private_data);

static struct tevent_req *tstream_bsd_readv_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					struct iovec *vector,
					size_t count)
{
	struct tevent_req *req;
	struct tstream_bsd_readv_state *state;
	struct tstream_bsd *bsds = tstream_context_data(stream, struct tstream_bsd);
	int ret;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_bsd_readv_state);
	if (!req) {
		return NULL;
	}

	state->stream	= stream;
	/* we make a copy of the vector so that we can modify it */
	state->vector	= talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		goto post;
	}
	memcpy(state->vector, vector, sizeof(struct iovec)*count);
	state->count	= count;
	state->ret	= 0;

	talloc_set_destructor(state, tstream_bsd_readv_destructor);

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	/*
	 * this is a fast path, not waiting for the
	 * socket to become explicit readable gains
	 * about 10%-20% performance in benchmark tests.
	 */
	if (bsds->optimize_readv) {
		/*
		 * We only do the optimization on
		 * readv if the caller asked for it.
		 *
		 * This is needed because in most cases
		 * we preferr to flush send buffers before
		 * receiving incoming requests.
		 */
		tstream_bsd_readv_handler(req);
		if (!tevent_req_is_in_progress(req)) {
			goto post;
		}
	}

	ret = tstream_bsd_set_readable_handler(bsds, ev,
					      tstream_bsd_readv_handler,
					      req);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_bsd_readv_handler(void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
				 struct tevent_req);
	struct tstream_bsd_readv_state *state = tevent_req_data(req,
					struct tstream_bsd_readv_state);
	struct tstream_context *stream = state->stream;
	struct tstream_bsd *bsds = tstream_context_data(stream, struct tstream_bsd);
	int ret;
	int err;
	bool retry;

	ret = readv(bsds->fd, state->vector, state->count);
	if (ret == 0) {
		/* propagate end of file */
		tevent_req_error(req, EPIPE);
		return;
	}
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	state->ret += ret;

	while (ret > 0) {
		if (ret < state->vector[0].iov_len) {
			uint8_t *base;
			base = (uint8_t *)state->vector[0].iov_base;
			base += ret;
			state->vector[0].iov_base = (void *)base;
			state->vector[0].iov_len -= ret;
			break;
		}
		ret -= state->vector[0].iov_len;
		state->vector += 1;
		state->count -= 1;
	}

	/*
	 * there're maybe some empty vectors at the end
	 * which we need to skip, otherwise we would get
	 * ret == 0 from the readv() call and return EPIPE
	 */
	while (state->count > 0) {
		if (state->vector[0].iov_len > 0) {
			break;
		}
		state->vector += 1;
		state->count -= 1;
	}

	if (state->count > 0) {
		/* we have more to read */
		return;
	}

	tevent_req_done(req);
}

static int tstream_bsd_readv_recv(struct tevent_req *req,
				  int *perrno)
{
	struct tstream_bsd_readv_state *state = tevent_req_data(req,
					struct tstream_bsd_readv_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_bsd_writev_state {
	struct tstream_context *stream;

	struct iovec *vector;
	size_t count;

	int ret;
};

static int tstream_bsd_writev_destructor(struct tstream_bsd_writev_state *state)
{
	struct tstream_bsd *bsds = tstream_context_data(state->stream,
				  struct tstream_bsd);

	tstream_bsd_set_writeable_handler(bsds, NULL, NULL, NULL);

	return 0;
}

static void tstream_bsd_writev_handler(void *private_data);

static struct tevent_req *tstream_bsd_writev_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct tstream_context *stream,
						 const struct iovec *vector,
						 size_t count)
{
	struct tevent_req *req;
	struct tstream_bsd_writev_state *state;
	struct tstream_bsd *bsds = tstream_context_data(stream, struct tstream_bsd);
	int ret;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_bsd_writev_state);
	if (!req) {
		return NULL;
	}

	state->stream	= stream;
	/* we make a copy of the vector so that we can modify it */
	state->vector	= talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		goto post;
	}
	memcpy(state->vector, vector, sizeof(struct iovec)*count);
	state->count	= count;
	state->ret	= 0;

	talloc_set_destructor(state, tstream_bsd_writev_destructor);

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	/*
	 * this is a fast path, not waiting for the
	 * socket to become explicit writeable gains
	 * about 10%-20% performance in benchmark tests.
	 */
	tstream_bsd_writev_handler(req);
	if (!tevent_req_is_in_progress(req)) {
		goto post;
	}

	ret = tstream_bsd_set_writeable_handler(bsds, ev,
					       tstream_bsd_writev_handler,
					       req);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_bsd_writev_handler(void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
				 struct tevent_req);
	struct tstream_bsd_writev_state *state = tevent_req_data(req,
					struct tstream_bsd_writev_state);
	struct tstream_context *stream = state->stream;
	struct tstream_bsd *bsds = tstream_context_data(stream, struct tstream_bsd);
	ssize_t ret;
	int err;
	bool retry;

	ret = writev(bsds->fd, state->vector, state->count);
	if (ret == 0) {
		/* propagate end of file */
		tevent_req_error(req, EPIPE);
		return;
	}
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	state->ret += ret;

	while (ret > 0) {
		if (ret < state->vector[0].iov_len) {
			uint8_t *base;
			base = (uint8_t *)state->vector[0].iov_base;
			base += ret;
			state->vector[0].iov_base = (void *)base;
			state->vector[0].iov_len -= ret;
			break;
		}
		ret -= state->vector[0].iov_len;
		state->vector += 1;
		state->count -= 1;
	}

	/*
	 * there're maybe some empty vectors at the end
	 * which we need to skip, otherwise we would get
	 * ret == 0 from the writev() call and return EPIPE
	 */
	while (state->count > 0) {
		if (state->vector[0].iov_len > 0) {
			break;
		}
		state->vector += 1;
		state->count -= 1;
	}

	if (state->count > 0) {
		/* we have more to read */
		return;
	}

	tevent_req_done(req);
}

static int tstream_bsd_writev_recv(struct tevent_req *req, int *perrno)
{
	struct tstream_bsd_writev_state *state = tevent_req_data(req,
					struct tstream_bsd_writev_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_bsd_disconnect_state {
	void *__dummy;
};

static struct tevent_req *tstream_bsd_disconnect_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct tstream_context *stream)
{
	struct tstream_bsd *bsds = tstream_context_data(stream, struct tstream_bsd);
	struct tevent_req *req;
	struct tstream_bsd_disconnect_state *state;
	int ret;
	int err;
	bool dummy;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_bsd_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	if (bsds->fd == -1) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	TALLOC_FREE(bsds->fde);
	ret = close(bsds->fd);
	bsds->fd = -1;
	err = tsocket_bsd_error_from_errno(ret, errno, &dummy);
	if (tevent_req_error(req, err)) {
		goto post;
	}

	tevent_req_done(req);
post:
	tevent_req_post(req, ev);
	return req;
}

static int tstream_bsd_disconnect_recv(struct tevent_req *req,
				      int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tstream_context_ops tstream_bsd_ops = {
	.name			= "bsd",

	.pending_bytes		= tstream_bsd_pending_bytes,

	.readv_send		= tstream_bsd_readv_send,
	.readv_recv		= tstream_bsd_readv_recv,

	.writev_send		= tstream_bsd_writev_send,
	.writev_recv		= tstream_bsd_writev_recv,

	.disconnect_send	= tstream_bsd_disconnect_send,
	.disconnect_recv	= tstream_bsd_disconnect_recv,
};

static int tstream_bsd_destructor(struct tstream_bsd *bsds)
{
	TALLOC_FREE(bsds->fde);
	if (bsds->fd != -1) {
		close(bsds->fd);
		bsds->fd = -1;
	}
	return 0;
}

int _tstream_bsd_existing_socket(TALLOC_CTX *mem_ctx,
				 int fd,
				 struct tstream_context **_stream,
				 const char *location)
{
	struct tstream_context *stream;
	struct tstream_bsd *bsds;

	stream = tstream_context_create(mem_ctx,
					&tstream_bsd_ops,
					&bsds,
					struct tstream_bsd,
					location);
	if (!stream) {
		return -1;
	}
	ZERO_STRUCTP(bsds);
	bsds->fd = fd;
	talloc_set_destructor(bsds, tstream_bsd_destructor);

	*_stream = stream;
	return 0;
}

struct tstream_bsd_connect_state {
	int fd;
	struct tevent_fd *fde;
	struct tstream_conext *stream;
	struct tsocket_address *local;
};

static int tstream_bsd_connect_destructor(struct tstream_bsd_connect_state *state)
{
	TALLOC_FREE(state->fde);
	if (state->fd != -1) {
		close(state->fd);
		state->fd = -1;
	}

	return 0;
}

static void tstream_bsd_connect_fde_handler(struct tevent_context *ev,
					    struct tevent_fd *fde,
					    uint16_t flags,
					    void *private_data);

static struct tevent_req *tstream_bsd_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					int sys_errno,
					const struct tsocket_address *local,
					const struct tsocket_address *remote)
{
	struct tevent_req *req;
	struct tstream_bsd_connect_state *state;
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	struct tsocket_address_bsd *lrbsda = NULL;
	struct tsocket_address_bsd *rbsda =
		talloc_get_type_abort(remote->private_data,
		struct tsocket_address_bsd);
	int ret;
	int err;
	bool retry;
	bool do_bind = false;
	bool do_reuseaddr = false;
	bool do_ipv6only = false;
	bool is_inet = false;
	int sa_fam = lbsda->u.sa.sa_family;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_bsd_connect_state);
	if (!req) {
		return NULL;
	}
	state->fd = -1;
	state->fde = NULL;

	talloc_set_destructor(state, tstream_bsd_connect_destructor);

	/* give the wrappers a chance to report an error */
	if (sys_errno != 0) {
		tevent_req_error(req, sys_errno);
		goto post;
	}

	switch (lbsda->u.sa.sa_family) {
	case AF_UNIX:
		if (lbsda->u.un.sun_path[0] != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		break;
	case AF_INET:
		if (lbsda->u.in.sin_port != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		if (lbsda->u.in.sin_addr.s_addr != INADDR_ANY) {
			do_bind = true;
		}
		is_inet = true;
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		if (lbsda->u.in6.sin6_port != 0) {
			do_reuseaddr = true;
			do_bind = true;
		}
		if (memcmp(&in6addr_any,
			   &lbsda->u.in6.sin6_addr,
			   sizeof(in6addr_any)) != 0) {
			do_bind = true;
		}
		is_inet = true;
		do_ipv6only = true;
		break;
#endif
	default:
		tevent_req_error(req, EINVAL);
		goto post;
	}

	if (!do_bind && is_inet) {
		sa_fam = rbsda->u.sa.sa_family;
		switch (sa_fam) {
		case AF_INET:
			do_ipv6only = false;
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			do_ipv6only = true;
			break;
#endif
		}
	}

	if (is_inet) {
		state->local = tsocket_address_create(state,
						      &tsocket_address_bsd_ops,
						      &lrbsda,
						      struct tsocket_address_bsd,
						      __location__ "bsd_connect");
		if (tevent_req_nomem(state->local, req)) {
			goto post;
		}

		ZERO_STRUCTP(lrbsda);
		lrbsda->sa_socklen = sizeof(lrbsda->u.ss);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
		lrbsda->u.sa.sa_len = lrbsda->sa_socklen;
#endif
	}

	state->fd = socket(sa_fam, SOCK_STREAM, 0);
	if (state->fd == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	state->fd = tsocket_bsd_common_prepare_fd(state->fd, true);
	if (state->fd == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

#ifdef HAVE_IPV6
	if (do_ipv6only) {
		int val = 1;

		ret = setsockopt(state->fd, IPPROTO_IPV6, IPV6_V6ONLY,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			tevent_req_error(req, errno);
			goto post;
		}
	}
#endif

	if (do_reuseaddr) {
		int val = 1;

		ret = setsockopt(state->fd, SOL_SOCKET, SO_REUSEADDR,
				 (const void *)&val, sizeof(val));
		if (ret == -1) {
			tevent_req_error(req, errno);
			goto post;
		}
	}

	if (do_bind) {
		ret = bind(state->fd, &lbsda->u.sa, lbsda->sa_socklen);
		if (ret == -1) {
			tevent_req_error(req, errno);
			goto post;
		}
	}

	if (rbsda->u.sa.sa_family != sa_fam) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	ret = connect(state->fd, &rbsda->u.sa, rbsda->sa_socklen);
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		goto async;
	}
	if (tevent_req_error(req, err)) {
		goto post;
	}

	if (!state->local) {
		tevent_req_done(req);
		goto post;
	}

	ret = getsockname(state->fd, &lrbsda->u.sa, &lrbsda->sa_socklen);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	tevent_req_done(req);
	goto post;

 async:
	state->fde = tevent_add_fd(ev, state,
				   state->fd,
				   TEVENT_FD_READ | TEVENT_FD_WRITE,
				   tstream_bsd_connect_fde_handler,
				   req);
	if (tevent_req_nomem(state->fde, req)) {
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_bsd_connect_fde_handler(struct tevent_context *ev,
					    struct tevent_fd *fde,
					    uint16_t flags,
					    void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
				 struct tevent_req);
	struct tstream_bsd_connect_state *state = tevent_req_data(req,
					struct tstream_bsd_connect_state);
	struct tsocket_address_bsd *lrbsda = NULL;
	int ret;
	int error=0;
	socklen_t len = sizeof(error);
	int err;
	bool retry;

	ret = getsockopt(state->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0) {
		if (error != 0) {
			errno = error;
			ret = -1;
		}
	}
	err = tsocket_bsd_error_from_errno(ret, errno, &retry);
	if (retry) {
		/* retry later */
		return;
	}
	if (tevent_req_error(req, err)) {
		return;
	}

	if (!state->local) {
		tevent_req_done(req);
		return;
	}

	lrbsda = talloc_get_type_abort(state->local->private_data,
				       struct tsocket_address_bsd);

	ret = getsockname(state->fd, &lrbsda->u.sa, &lrbsda->sa_socklen);
	if (ret == -1) {
		tevent_req_error(req, errno);
		return;
	}

	tevent_req_done(req);
}

static int tstream_bsd_connect_recv(struct tevent_req *req,
				    int *perrno,
				    TALLOC_CTX *mem_ctx,
				    struct tstream_context **stream,
				    struct tsocket_address **local,
				    const char *location)
{
	struct tstream_bsd_connect_state *state = tevent_req_data(req,
					struct tstream_bsd_connect_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = _tstream_bsd_existing_socket(mem_ctx,
						   state->fd,
						   stream,
						   location);
		if (ret == -1) {
			*perrno = errno;
			goto done;
		}
		TALLOC_FREE(state->fde);
		state->fd = -1;

		if (local) {
			*local = talloc_move(mem_ctx, &state->local);
		}
	}

done:
	tevent_req_received(req);
	return ret;
}

struct tevent_req * tstream_inet_tcp_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct tsocket_address *local,
					const struct tsocket_address *remote)
{
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	struct tevent_req *req;
	int sys_errno = 0;

	switch (lbsda->u.sa.sa_family) {
	case AF_INET:
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		break;
#endif
	default:
		sys_errno = EINVAL;
		break;
	}

	req = tstream_bsd_connect_send(mem_ctx, ev, sys_errno, local, remote);

	return req;
}

int _tstream_inet_tcp_connect_recv(struct tevent_req *req,
				   int *perrno,
				   TALLOC_CTX *mem_ctx,
				   struct tstream_context **stream,
				   struct tsocket_address **local,
				   const char *location)
{
	return tstream_bsd_connect_recv(req, perrno,
					mem_ctx, stream, local,
					location);
}

struct tevent_req * tstream_unix_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct tsocket_address *local,
					const struct tsocket_address *remote)
{
	struct tsocket_address_bsd *lbsda =
		talloc_get_type_abort(local->private_data,
		struct tsocket_address_bsd);
	struct tevent_req *req;
	int sys_errno = 0;

	switch (lbsda->u.sa.sa_family) {
	case AF_UNIX:
		break;
	default:
		sys_errno = EINVAL;
		break;
	}

	req = tstream_bsd_connect_send(mem_ctx, ev, sys_errno, local, remote);

	return req;
}

int _tstream_unix_connect_recv(struct tevent_req *req,
				      int *perrno,
				      TALLOC_CTX *mem_ctx,
				      struct tstream_context **stream,
				      const char *location)
{
	return tstream_bsd_connect_recv(req, perrno,
					mem_ctx, stream, NULL,
					location);
}

int _tstream_unix_socketpair(TALLOC_CTX *mem_ctx1,
			     struct tstream_context **_stream1,
			     TALLOC_CTX *mem_ctx2,
			     struct tstream_context **_stream2,
			     const char *location)
{
	int ret;
	int fds[2];
	int fd1;
	int fd2;
	struct tstream_context *stream1 = NULL;
	struct tstream_context *stream2 = NULL;

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	if (ret == -1) {
		return -1;
	}
	fd1 = fds[0];
	fd2 = fds[1];

	fd1 = tsocket_bsd_common_prepare_fd(fd1, true);
	if (fd1 == -1) {
		int sys_errno = errno;
		close(fd2);
		errno = sys_errno;
		return -1;
	}

	fd2 = tsocket_bsd_common_prepare_fd(fd2, true);
	if (fd2 == -1) {
		int sys_errno = errno;
		close(fd1);
		errno = sys_errno;
		return -1;
	}

	ret = _tstream_bsd_existing_socket(mem_ctx1,
					   fd1,
					   &stream1,
					   location);
	if (ret == -1) {
		int sys_errno = errno;
		close(fd1);
		close(fd2);
		errno = sys_errno;
		return -1;
	}

	ret = _tstream_bsd_existing_socket(mem_ctx2,
					   fd2,
					   &stream2,
					   location);
	if (ret == -1) {
		int sys_errno = errno;
		talloc_free(stream1);
		close(fd2);
		errno = sys_errno;
		return -1;
	}

	*_stream1 = stream1;
	*_stream2 = stream2;
	return 0;
}

