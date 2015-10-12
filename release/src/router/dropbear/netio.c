#include "netio.h"
#include "list.h"
#include "dbutil.h"
#include "session.h"
#include "debug.h"

struct dropbear_progress_connection {
	struct addrinfo *res;
	struct addrinfo *res_iter;

	char *remotehost, *remoteport; /* For error reporting */

	connect_callback cb;
	void *cb_data;

	struct Queue *writequeue; /* A queue of encrypted packets to send with TCP fastopen,
								or NULL. */

	int sock;

	char* errstring;
};

/* Deallocate a progress connection. Removes from the pending list if iter!=NULL.
Does not close sockets */
static void remove_connect(struct dropbear_progress_connection *c, m_list_elem *iter) {
	if (c->res) {
		freeaddrinfo(c->res);
	}
	m_free(c->remotehost);
	m_free(c->remoteport);
	m_free(c->errstring);
	m_free(c);

	if (iter) {
		list_remove(iter);
	}
}

static void cancel_callback(int result, int sock, void* UNUSED(data), const char* UNUSED(errstring)) {
	if (result == DROPBEAR_SUCCESS)
	{
		m_close(sock);
	}
}

void cancel_connect(struct dropbear_progress_connection *c) {
	c->cb = cancel_callback;
	c->cb_data = NULL;
}

static void connect_try_next(struct dropbear_progress_connection *c) {
	struct addrinfo *r;
	int res = 0;
	int fastopen = 0;
#ifdef DROPBEAR_CLIENT_TCP_FAST_OPEN
	struct msghdr message;
#endif

	for (r = c->res_iter; r; r = r->ai_next)
	{
		dropbear_assert(c->sock == -1);

		c->sock = socket(c->res_iter->ai_family, c->res_iter->ai_socktype, c->res_iter->ai_protocol);
		if (c->sock < 0) {
			continue;
		}

		ses.maxfd = MAX(ses.maxfd, c->sock);
		set_sock_nodelay(c->sock);
		setnonblocking(c->sock);

#ifdef DROPBEAR_CLIENT_TCP_FAST_OPEN
		fastopen = (c->writequeue != NULL);

		if (fastopen) {
			memset(&message, 0x0, sizeof(message));
			message.msg_name = r->ai_addr;
			message.msg_namelen = r->ai_addrlen;
			/* 6 is arbitrary, enough to hold initial packets */
			unsigned int iovlen = 6; /* Linux msg_iovlen is a size_t */
			struct iovec iov[6];
			packet_queue_to_iovec(c->writequeue, iov, &iovlen);
			message.msg_iov = iov;
			message.msg_iovlen = iovlen;
			res = sendmsg(c->sock, &message, MSG_FASTOPEN);
			/* Returns EINPROGRESS if FASTOPEN wasn't available */
			if (res < 0) {
				if (errno != EINPROGRESS) {
					m_free(c->errstring);
					c->errstring = m_strdup(strerror(errno));
					/* Not entirely sure which kind of errors are normal - 2.6.32 seems to 
					return EPIPE for any (nonblocking?) sendmsg(). just fall back */
					TRACE(("sendmsg tcp_fastopen failed, falling back. %s", strerror(errno)));
					/* No kernel MSG_FASTOPEN support. Fall back below */
					fastopen = 0;
					/* Set to NULL to avoid trying again */
					c->writequeue = NULL;
				}
			} else {
				packet_queue_consume(c->writequeue, res);
			}
		}
#endif

		/* Normal connect(), used as fallback for TCP fastopen too */
		if (!fastopen) {
			res = connect(c->sock, r->ai_addr, r->ai_addrlen);
		}

		if (res < 0 && errno != EINPROGRESS) {
			/* failure */
			m_free(c->errstring);
			c->errstring = m_strdup(strerror(errno));
			close(c->sock);
			c->sock = -1;
			continue;
		} else {
			/* new connection was successful, wait for it to complete */
			break;
		}
	}

	if (r) {
		c->res_iter = r->ai_next;
	} else {
		c->res_iter = NULL;
	}
}

/* Connect via TCP to a host. */
struct dropbear_progress_connection *connect_remote(const char* remotehost, const char* remoteport,
	connect_callback cb, void* cb_data)
{
	struct dropbear_progress_connection *c = NULL;
	int err;
	struct addrinfo hints;

	c = m_malloc(sizeof(*c));
	c->remotehost = m_strdup(remotehost);
	c->remoteport = m_strdup(remoteport);
	c->sock = -1;
	c->cb = cb;
	c->cb_data = cb_data;

	list_append(&ses.conn_pending, c);

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;

	err = getaddrinfo(remotehost, remoteport, &hints, &c->res);
	if (err) {
		int len;
		len = 100 + strlen(gai_strerror(err));
		c->errstring = (char*)m_malloc(len);
		snprintf(c->errstring, len, "Error resolving '%s' port '%s'. %s", 
				remotehost, remoteport, gai_strerror(err));
		TRACE(("Error resolving: %s", gai_strerror(err)))
	} else {
		c->res_iter = c->res;
	}

	return c;
}

void remove_connect_pending() {
	while (ses.conn_pending.first) {
		struct dropbear_progress_connection *c = ses.conn_pending.first->item;
		remove_connect(c, ses.conn_pending.first);
	}
}


void set_connect_fds(fd_set *writefd) {
	m_list_elem *iter;
	TRACE(("enter set_connect_fds"))
	iter = ses.conn_pending.first;
	while (iter) {
		m_list_elem *next_iter = iter->next;
		struct dropbear_progress_connection *c = iter->item;
		/* Set one going */
		while (c->res_iter && c->sock < 0) {
			connect_try_next(c);
		}
		if (c->sock >= 0) {
			FD_SET(c->sock, writefd);
		} else {
			/* Final failure */
			if (!c->errstring) {
				c->errstring = m_strdup("unexpected failure");
			}
			c->cb(DROPBEAR_FAILURE, -1, c->cb_data, c->errstring);
			remove_connect(c, iter);
		}
		iter = next_iter;
	}
}

void handle_connect_fds(fd_set *writefd) {
	m_list_elem *iter;
	TRACE(("enter handle_connect_fds"))
	for (iter = ses.conn_pending.first; iter; iter = iter->next) {
		int val;
		socklen_t vallen = sizeof(val);
		struct dropbear_progress_connection *c = iter->item;

		if (c->sock < 0 || !FD_ISSET(c->sock, writefd)) {
			continue;
		}

		TRACE(("handling %s port %s socket %d", c->remotehost, c->remoteport, c->sock));

		if (getsockopt(c->sock, SOL_SOCKET, SO_ERROR, &val, &vallen) != 0) {
			TRACE(("handle_connect_fds getsockopt(%d) SO_ERROR failed: %s", c->sock, strerror(errno)))
			/* This isn't expected to happen - Unix has surprises though, continue gracefully. */
			m_close(c->sock);
			c->sock = -1;
		} else if (val != 0) {
			/* Connect failed */
			TRACE(("connect to %s port %s failed.", c->remotehost, c->remoteport))
			m_close(c->sock);
			c->sock = -1;

			m_free(c->errstring);
			c->errstring = m_strdup(strerror(val));
		} else {
			/* New connection has been established */
			c->cb(DROPBEAR_SUCCESS, c->sock, c->cb_data, NULL);
			remove_connect(c, iter);
			TRACE(("leave handle_connect_fds - success"))
			/* Must return here - remove_connect() invalidates iter */
			return; 
		}
	}
	TRACE(("leave handle_connect_fds - end iter"))
}

void connect_set_writequeue(struct dropbear_progress_connection *c, struct Queue *writequeue) {
	c->writequeue = writequeue;
}

void packet_queue_to_iovec(struct Queue *queue, struct iovec *iov, unsigned int *iov_count) {
	struct Link *l;
	unsigned int i;
	int len;
	buffer *writebuf;

	#ifndef IOV_MAX
	#define IOV_MAX UIO_MAXIOV
	#endif

	*iov_count = MIN(MIN(queue->count, IOV_MAX), *iov_count);

	for (l = queue->head, i = 0; i < *iov_count; l = l->link, i++)
	{
		writebuf = (buffer*)l->item;
		len = writebuf->len - 1 - writebuf->pos;
		dropbear_assert(len > 0);
		TRACE2(("write_packet writev #%d  type %d len %d/%d", i, writebuf->data[writebuf->len-1],
				len, writebuf->len-1))
		iov[i].iov_base = buf_getptr(writebuf, len);
		iov[i].iov_len = len;
	}
}

void packet_queue_consume(struct Queue *queue, ssize_t written) {
	buffer *writebuf;
	int len;
	while (written > 0) {
		writebuf = (buffer*)examine(queue);
		len = writebuf->len - 1 - writebuf->pos;
		if (len > written) {
			/* partial buffer write */
			buf_incrpos(writebuf, written);
			written = 0;
		} else {
			written -= len;
			dequeue(queue);
			buf_free(writebuf);
		}
	}
}

void set_sock_nodelay(int sock) {
	int val;

	/* disable nagle */
	val = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&val, sizeof(val));
}

#ifdef DROPBEAR_SERVER_TCP_FAST_OPEN
void set_listen_fast_open(int sock) {
	int qlen = MAX(MAX_UNAUTH_PER_IP, 5);
	if (setsockopt(sock, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen)) != 0) {
		TRACE(("set_listen_fast_open failed for socket %d: %s", sock, strerror(errno)))
	}
}

#endif

void set_sock_priority(int sock, enum dropbear_prio prio) {

	int rc;
#ifdef IPTOS_LOWDELAY
	int iptos_val = 0;
#endif
#ifdef SO_PRIORITY
	int so_prio_val = 0;
#endif


	/* Don't log ENOTSOCK errors so that this can harmlessly be called
	 * on a client '-J' proxy pipe */

	/* set the TOS bit for either ipv4 or ipv6 */
#ifdef IPTOS_LOWDELAY
	if (prio == DROPBEAR_PRIO_LOWDELAY) {
		iptos_val = IPTOS_LOWDELAY;
	} else if (prio == DROPBEAR_PRIO_BULK) {
		iptos_val = IPTOS_THROUGHPUT;
	}
#if defined(IPPROTO_IPV6) && defined(IPV6_TCLASS)
	rc = setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, (void*)&iptos_val, sizeof(iptos_val));
	if (rc < 0 && errno != ENOTSOCK) {
		TRACE(("Couldn't set IPV6_TCLASS (%s)", strerror(errno)));
	}
#endif
	rc = setsockopt(sock, IPPROTO_IP, IP_TOS, (void*)&iptos_val, sizeof(iptos_val));
	if (rc < 0 && errno != ENOTSOCK) {
		TRACE(("Couldn't set IP_TOS (%s)", strerror(errno)));
	}
#endif

#ifdef SO_PRIORITY
	if (prio == DROPBEAR_PRIO_LOWDELAY) {
		so_prio_val = TC_PRIO_INTERACTIVE;
	} else if (prio == DROPBEAR_PRIO_BULK) {
		so_prio_val = TC_PRIO_BULK;
	}
	/* linux specific, sets QoS class. see tc-prio(8) */
	rc = setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (void*) &so_prio_val, sizeof(so_prio_val));
	if (rc < 0 && errno != ENOTSOCK)
		dropbear_log(LOG_WARNING, "Couldn't set SO_PRIORITY (%s)",
				strerror(errno));
#endif

}

/* Listen on address:port. 
 * Special cases are address of "" listening on everything,
 * and address of NULL listening on localhost only.
 * Returns the number of sockets bound on success, or -1 on failure. On
 * failure, if errstring wasn't NULL, it'll be a newly malloced error
 * string.*/
int dropbear_listen(const char* address, const char* port,
		int *socks, unsigned int sockcount, char **errstring, int *maxfd) {

	struct addrinfo hints, *res = NULL, *res0 = NULL;
	int err;
	unsigned int nsock;
	struct linger linger;
	int val;
	int sock;

	TRACE(("enter dropbear_listen"))
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* TODO: let them flag v4 only etc */
	hints.ai_socktype = SOCK_STREAM;

	/* for calling getaddrinfo:
	 address == NULL and !AI_PASSIVE: local loopback
	 address == NULL and AI_PASSIVE: all interfaces
	 address != NULL: whatever the address says */
	if (!address) {
		TRACE(("dropbear_listen: local loopback"))
	} else {
		if (address[0] == '\0') {
			TRACE(("dropbear_listen: all interfaces"))
			address = NULL;
		}
		hints.ai_flags = AI_PASSIVE;
	}
	err = getaddrinfo(address, port, &hints, &res0);

	if (err) {
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 20 + strlen(gai_strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error resolving: %s", gai_strerror(err));
		}
		if (res0) {
			freeaddrinfo(res0);
			res0 = NULL;
		}
		TRACE(("leave dropbear_listen: failed resolving"))
		return -1;
	}


	nsock = 0;
	for (res = res0; res != NULL && nsock < sockcount;
			res = res->ai_next) {

		/* Get a socket */
		socks[nsock] = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol);

		sock = socks[nsock]; /* For clarity */

		if (sock < 0) {
			err = errno;
			TRACE(("socket() failed"))
			continue;
		}

		/* Various useful socket options */
		val = 1;
		/* set to reuse, quick timeout */
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &val, sizeof(val));
		linger.l_onoff = 1;
		linger.l_linger = 5;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&linger, sizeof(linger));

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
		if (res->ai_family == AF_INET6) {
			int on = 1;
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, 
						&on, sizeof(on)) == -1) {
				dropbear_log(LOG_WARNING, "Couldn't set IPV6_V6ONLY");
			}
		}
#endif

		set_sock_nodelay(sock);

		if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
			err = errno;
			close(sock);
			TRACE(("bind(%s) failed", port))
			continue;
		}

		if (listen(sock, DROPBEAR_LISTEN_BACKLOG) < 0) {
			err = errno;
			close(sock);
			TRACE(("listen() failed"))
			continue;
		}

		*maxfd = MAX(*maxfd, sock);

		nsock++;
	}

	if (res0) {
		freeaddrinfo(res0);
		res0 = NULL;
	}

	if (nsock == 0) {
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 20 + strlen(strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error listening: %s", strerror(err));
		}
		TRACE(("leave dropbear_listen: failure, %s", strerror(err)))
		return -1;
	}

	TRACE(("leave dropbear_listen: success, %d socks bound", nsock))
	return nsock;
}

void get_socket_address(int fd, char **local_host, char **local_port,
						char **remote_host, char **remote_port, int host_lookup)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	
	if (local_host || local_port) {
		addrlen = sizeof(addr);
		if (getsockname(fd, (struct sockaddr*)&addr, &addrlen) < 0) {
			dropbear_exit("Failed socket address: %s", strerror(errno));
		}
		getaddrstring(&addr, local_host, local_port, host_lookup);		
	}
	if (remote_host || remote_port) {
		addrlen = sizeof(addr);
		if (getpeername(fd, (struct sockaddr*)&addr, &addrlen) < 0) {
			dropbear_exit("Failed socket address: %s", strerror(errno));
		}
		getaddrstring(&addr, remote_host, remote_port, host_lookup);		
	}
}

/* Return a string representation of the socket address passed. The return
 * value is allocated with malloc() */
void getaddrstring(struct sockaddr_storage* addr, 
			char **ret_host, char **ret_port,
			int host_lookup) {

	char host[NI_MAXHOST+1], serv[NI_MAXSERV+1];
	unsigned int len;
	int ret;
	
	int flags = NI_NUMERICSERV | NI_NUMERICHOST;

#ifndef DO_HOST_LOOKUP
	host_lookup = 0;
#endif
	
	if (host_lookup) {
		flags = NI_NUMERICSERV;
	}

	len = sizeof(struct sockaddr_storage);
	/* Some platforms such as Solaris 8 require that len is the length
	 * of the specific structure. Some older linux systems (glibc 2.1.3
	 * such as debian potato) have sockaddr_storage.__ss_family instead
	 * but we'll ignore them */
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
	if (addr->ss_family == AF_INET) {
		len = sizeof(struct sockaddr_in);
	}
#ifdef AF_INET6
	if (addr->ss_family == AF_INET6) {
		len = sizeof(struct sockaddr_in6);
	}
#endif
#endif

	ret = getnameinfo((struct sockaddr*)addr, len, host, sizeof(host)-1, 
			serv, sizeof(serv)-1, flags);

	if (ret != 0) {
		if (host_lookup) {
			/* On some systems (Darwin does it) we get EINTR from getnameinfo
			 * somehow. Eew. So we'll just return the IP, since that doesn't seem
			 * to exhibit that behaviour. */
			getaddrstring(addr, ret_host, ret_port, 0);
			return;
		} else {
			/* if we can't do a numeric lookup, something's gone terribly wrong */
			dropbear_exit("Failed lookup: %s", gai_strerror(ret));
		}
	}

	if (ret_host) {
		*ret_host = m_strdup(host);
	}
	if (ret_port) {
		*ret_port = m_strdup(serv);
	}
}

