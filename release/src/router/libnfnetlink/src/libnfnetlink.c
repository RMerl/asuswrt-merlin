/* libnfnetlink.c: generic library for communication with netfilter
 *
 * (C) 2002-2006 by Harald Welte <laforge@gnumonks.org>
 * (C) 2006-2011 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * Based on some original ideas from Jay Schulist <jschlst@samba.org>
 *
 * Development of this code funded by Astaro AG (http://www.astaro.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * 2005-09-14 Pablo Neira Ayuso <pablo@netfilter.org>: 
 * 	Define structure nfnlhdr
 * 	Added __be64_to_cpu function
 *	Use NFA_TYPE macro to get the attribute type
 *
 * 2006-01-14 Harald Welte <laforge@netfilter.org>:
 * 	introduce nfnl_subsys_handle
 *
 * 2006-01-15 Pablo Neira Ayuso <pablo@netfilter.org>:
 * 	set missing subsys_id in nfnl_subsys_open
 * 	set missing nfnlh->local.nl_pid in nfnl_open
 *
 * 2006-01-26 Harald Welte <laforge@netfilter.org>:
 * 	remove bogus nfnlh->local.nl_pid from nfnl_open ;)
 * 	add 16bit attribute functions
 *
 * 2006-07-03 Pablo Neira Ayuso <pablo@netfilter.org>:
 * 	add iterator API
 * 	add replacements for nfnl_listen and nfnl_talk
 * 	fix error handling
 * 	add assertions
 * 	add documentation
 * 	minor cleanups
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <assert.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <linux/netlink.h>

#include <libnfnetlink/libnfnetlink.h>

#ifndef NETLINK_ADD_MEMBERSHIP
#define NETLINK_ADD_MEMBERSHIP 1
#endif

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif


#define nfnl_error(format, args...) \
	fprintf(stderr, "%s: " format "\n", __FUNCTION__, ## args)

#ifdef _NFNL_DEBUG
#define nfnl_debug_dump_packet nfnl_dump_packet
#else
#define nfnl_debug_dump_packet(a, b, ...)
#endif

struct nfnl_subsys_handle {
	struct nfnl_handle 	*nfnlh;
	u_int32_t		subscriptions;
	u_int8_t		subsys_id;
	u_int8_t		cb_count;
	struct nfnl_callback 	*cb;	/* array of callbacks */
};

#define		NFNL_MAX_SUBSYS			16 /* enough for now */

#define NFNL_F_SEQTRACK_ENABLED		(1 << 0)

struct nfnl_handle {
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	u_int32_t		subscriptions;
	u_int32_t		seq;
	u_int32_t		dump;
	u_int32_t		rcv_buffer_size;	/* for nfnl_catch */
	u_int32_t		flags;
	struct nlmsghdr 	*last_nlhdr;
	struct nfnl_subsys_handle subsys[NFNL_MAX_SUBSYS+1];
};

void nfnl_dump_packet(struct nlmsghdr *nlh, int received_len, char *desc)
{
	void *nlmsg_data = NLMSG_DATA(nlh);
	struct nfattr *nfa = NFM_NFA(NLMSG_DATA(nlh));
	int len = NFM_PAYLOAD(nlh);

	printf("%s called from %s\n", __FUNCTION__, desc);
	printf("  nlmsghdr = %p, received_len = %u\n", nlh, received_len);
	printf("  NLMSG_DATA(nlh) = %p (+%td bytes)\n", nlmsg_data,
	       (nlmsg_data - (void *)nlh));
	printf("  NFM_NFA(NLMSG_DATA(nlh)) = %p (+%td bytes)\n",
		nfa, ((void *)nfa - (void *)nlh));
	printf("  NFM_PAYLOAD(nlh) = %u\n", len);
	printf("  nlmsg_type = %u, nlmsg_len = %u, nlmsg_seq = %u "
		"nlmsg_flags = 0x%x\n", nlh->nlmsg_type, nlh->nlmsg_len,
		nlh->nlmsg_seq, nlh->nlmsg_flags);

	while (NFA_OK(nfa, len)) {
		printf("    nfa@%p: nfa_type=%u, nfa_len=%u\n",
			nfa, NFA_TYPE(nfa), nfa->nfa_len);
		nfa = NFA_NEXT(nfa,len);
	}
}

/**
 * nfnl_fd - returns the descriptor that identifies the socket
 * @nfnlh: nfnetlink handler
 *
 * Use this function if you need to interact with the socket. Common
 * scenarios are the use of poll()/select() to achieve multiplexation.
 */
int nfnl_fd(struct nfnl_handle *h)
{
	assert(h);
	return h->fd;
}

/**
 * nfnl_portid - returns the Netlink port ID of this socket
 * @h: nfnetlink handler
 */
unsigned int nfnl_portid(const struct nfnl_handle *h)
{
	assert(h);
	return h->local.nl_pid;
}

static int recalc_rebind_subscriptions(struct nfnl_handle *nfnlh)
{
	int i, err;
	u_int32_t new_subscriptions = nfnlh->subscriptions;

	for (i = 0; i < NFNL_MAX_SUBSYS; i++)
		new_subscriptions |= nfnlh->subsys[i].subscriptions;

	nfnlh->local.nl_groups = new_subscriptions;
	err = bind(nfnlh->fd, (struct sockaddr *)&nfnlh->local,
		   sizeof(nfnlh->local));
	if (err == -1)
		return -1;

	nfnlh->subscriptions = new_subscriptions;

	return 0;
}

/**
 * nfnl_open - open a nfnetlink handler
 *
 * This function creates a nfnetlink handler, this is required to establish
 * a communication between the userspace and the nfnetlink system.
 *
 * On success, a valid address that points to a nfnl_handle structure
 * is returned. On error, NULL is returned and errno is set approapiately.
 */
struct nfnl_handle *nfnl_open(void)
{
	struct nfnl_handle *nfnlh;
	unsigned int addr_len;

	nfnlh = malloc(sizeof(*nfnlh));
	if (!nfnlh)
		return NULL;

	memset(nfnlh, 0, sizeof(*nfnlh));
	nfnlh->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
	if (nfnlh->fd == -1)
		goto err_free;

	nfnlh->local.nl_family = AF_NETLINK;
	nfnlh->peer.nl_family = AF_NETLINK;

	addr_len = sizeof(nfnlh->local);
	getsockname(nfnlh->fd, (struct sockaddr *)&nfnlh->local, &addr_len);
	if (addr_len != sizeof(nfnlh->local)) {
		errno = EINVAL;
		goto err_close;
	}
	if (nfnlh->local.nl_family != AF_NETLINK) {
		errno = EINVAL;
		goto err_close;
	}
	nfnlh->seq = time(NULL);
	nfnlh->rcv_buffer_size = NFNL_BUFFSIZE;

	/* don't set pid here, only first socket of process has real pid !!! 
	 * binding to pid '0' will default */

	/* let us do the initial bind */
	if (recalc_rebind_subscriptions(nfnlh) < 0)
		goto err_close;

	/* use getsockname to get the netlink pid that the kernel assigned us */
	addr_len = sizeof(nfnlh->local);
	getsockname(nfnlh->fd, (struct sockaddr *)&nfnlh->local, &addr_len);
	if (addr_len != sizeof(nfnlh->local)) {
		errno = EINVAL;
		goto err_close;
	}
	/* sequence tracking enabled by default */
	nfnlh->flags |= NFNL_F_SEQTRACK_ENABLED;

	return nfnlh;

err_close:
	close(nfnlh->fd);
err_free:
	free(nfnlh);
	return NULL;
}

/**
 * nfnl_set_sequence_tracking - set netlink sequence tracking
 * @h: nfnetlink handler
 */
void nfnl_set_sequence_tracking(struct nfnl_handle *h)
{
	h->flags |= NFNL_F_SEQTRACK_ENABLED;
}

/**
 * nfnl_unset_sequence_tracking - set netlink sequence tracking
 * @h: nfnetlink handler
 */
void nfnl_unset_sequence_tracking(struct nfnl_handle *h)
{
	h->flags &= ~NFNL_F_SEQTRACK_ENABLED;
}

/**
 * nfnl_set_rcv_buffer_size - set the size of the receive buffer
 * @h: libnfnetlink handler
 * @size: buffer size
 *
 * This function sets the size of the receive buffer size, i.e. the size
 * of the buffer used by nfnl_recv. Default value is 4096 bytes.
 */
void nfnl_set_rcv_buffer_size(struct nfnl_handle *h, unsigned int size)
{
	h->rcv_buffer_size = size;
}

/**
 * nfnl_subsys_open - open a netlink subsystem
 * @nfnlh: libnfnetlink handle
 * @subsys_id: which nfnetlink subsystem we are interested in
 * @cb_count: number of callbacks that are used maximum.
 * @subscriptions: netlink groups we want to be subscribed to
 *
 * This function creates a subsystem handler that contains the set of 
 * callbacks that handle certain types of messages coming from a netfilter
 * subsystem. Initially the callback set is empty, you can register callbacks
 * via nfnl_callback_register().
 *
 * On error, NULL is returned and errno is set appropiately. On success,
 * a valid address that points to a nfnl_subsys_handle structure is returned.
 */
struct nfnl_subsys_handle *
nfnl_subsys_open(struct nfnl_handle *nfnlh, u_int8_t subsys_id,
		 u_int8_t cb_count, u_int32_t subscriptions)
{
	struct nfnl_subsys_handle *ssh;

	assert(nfnlh);

	if (subsys_id > NFNL_MAX_SUBSYS) { 
		errno = ENOENT;
		return NULL;
	}

	ssh = &nfnlh->subsys[subsys_id];
	if (ssh->cb) {
		errno = EBUSY;
		return NULL;
	}

	ssh->cb = calloc(cb_count, sizeof(*(ssh->cb)));
	if (!ssh->cb)
		return NULL;

	ssh->nfnlh = nfnlh;
	ssh->cb_count = cb_count;
	ssh->subscriptions = subscriptions;
	ssh->subsys_id = subsys_id;

	/* although now we have nfnl_join to subscribe to certain
	 * groups, just keep this to ensure compatibility */
	if (recalc_rebind_subscriptions(nfnlh) < 0) {
		free(ssh->cb);
		ssh->cb = NULL;
		return NULL;
	}
	
	return ssh;
}

/**
 * nfnl_subsys_close - close a nfnetlink subsys handler 
 * @ssh: nfnetlink subsystem handler
 *
 * Release all the callbacks registered in a subsystem handler.
 */
void nfnl_subsys_close(struct nfnl_subsys_handle *ssh)
{
	assert(ssh);

	ssh->subscriptions = 0;
	ssh->cb_count = 0;
	if (ssh->cb) {
		free(ssh->cb);
		ssh->cb = NULL;
	}
}

/**
 * nfnl_close - close a nfnetlink handler
 * @nfnlh: nfnetlink handler
 *
 * This function closes the nfnetlink handler. On success, 0 is returned.
 * On error, -1 is returned and errno is set appropiately.
 */
int nfnl_close(struct nfnl_handle *nfnlh)
{
	int i, ret;

	assert(nfnlh);

	for (i = 0; i < NFNL_MAX_SUBSYS; i++)
		nfnl_subsys_close(&nfnlh->subsys[i]);

	ret = close(nfnlh->fd);
	if (ret < 0)
		return ret;

	free(nfnlh);

	return 0;
}

/**
 * nfnl_join - join a nfnetlink multicast group
 * @nfnlh: nfnetlink handler
 * @group: group we want to join
 *
 * This function is used to join a certain multicast group. It must be
 * called once the nfnetlink handler has been created. If any doubt, 
 * just use it if you have to listen to nfnetlink events.
 *
 * On success, 0 is returned. On error, -1 is returned and errno is set
 * approapiately.
 */
int nfnl_join(const struct nfnl_handle *nfnlh, unsigned int group)
{
	assert(nfnlh);
	return setsockopt(nfnlh->fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
			  &group, sizeof(group));
}

/**
 * nfnl_send - send a nfnetlink message through netlink socket
 * @nfnlh: nfnetlink handler
 * @n: netlink message
 *
 * On success, the number of bytes is returned. On error, -1 is returned 
 * and errno is set appropiately.
 */
int nfnl_send(struct nfnl_handle *nfnlh, struct nlmsghdr *n)
{
	assert(nfnlh);
	assert(n);

	nfnl_debug_dump_packet(n, n->nlmsg_len+sizeof(*n), "nfnl_send");

	return sendto(nfnlh->fd, n, n->nlmsg_len, 0, 
		      (struct sockaddr *)&nfnlh->peer, sizeof(nfnlh->peer));
}

int nfnl_sendmsg(const struct nfnl_handle *nfnlh, const struct msghdr *msg,
		 unsigned int flags)
{
	assert(nfnlh);
	assert(msg);

	return sendmsg(nfnlh->fd, msg, flags);
}

int nfnl_sendiov(const struct nfnl_handle *nfnlh, const struct iovec *iov,
		 unsigned int num, unsigned int flags)
{
	struct msghdr msg;

	assert(nfnlh);

	msg.msg_name = (struct sockaddr *) &nfnlh->peer;
	msg.msg_namelen = sizeof(nfnlh->peer);
	msg.msg_iov = (struct iovec *) iov;
	msg.msg_iovlen = num;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	return nfnl_sendmsg(nfnlh, &msg, flags);
}

/**
 * nfnl_fill_hdr - fill in netlink and nfnetlink header
 * @nfnlh: nfnetlink handle
 * @nlh: netlink message to be filled in
 * @len: length of _payload_ bytes (not including nfgenmsg)
 * @family: AF_INET / ...
 * @res_id: resource id
 * @msg_type: nfnetlink message type (without subsystem)
 * @msg_flags: netlink message flags
 *
 * This function sets up appropiately the nfnetlink header. See that the
 * pointer to the netlink message passed must point to a memory region of
 * at least the size of struct nlmsghdr + struct nfgenmsg.
 */
void nfnl_fill_hdr(struct nfnl_subsys_handle *ssh,
		    struct nlmsghdr *nlh, unsigned int len, 
		    u_int8_t family,
		    u_int16_t res_id,
		    u_int16_t msg_type,
		    u_int16_t msg_flags)
{
	assert(ssh);
	assert(nlh);

	struct nfgenmsg *nfg = (void *)nlh + sizeof(*nlh);

	nlh->nlmsg_len = NLMSG_LENGTH(len+sizeof(*nfg));
	nlh->nlmsg_type = (ssh->subsys_id<<8)|msg_type;
	nlh->nlmsg_flags = msg_flags;
	nlh->nlmsg_pid = 0;

	if (ssh->nfnlh->flags & NFNL_F_SEQTRACK_ENABLED) {
		nlh->nlmsg_seq = ++ssh->nfnlh->seq;
		/* kernel uses sequence number zero for events */
		if (!ssh->nfnlh->seq)
			nlh->nlmsg_seq = ssh->nfnlh->seq = time(NULL);
	} else {
		/* unset sequence number, ignore it */
		nlh->nlmsg_seq = 0;
	}

	nfg->nfgen_family = family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(res_id);
}

struct nfattr *
nfnl_parse_hdr(const struct nfnl_handle *nfnlh,
		const struct nlmsghdr *nlh,
		struct nfgenmsg **genmsg)
{
	if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct nfgenmsg)))
		return NULL;

	if (nlh->nlmsg_len == NLMSG_LENGTH(sizeof(struct nfgenmsg))) {
		if (genmsg)
			*genmsg = (void *)nlh + sizeof(*nlh);
		return NULL;
	}

	if (genmsg)
		*genmsg = (void *)nlh + sizeof(*nlh);

	return (void *)nlh + NLMSG_LENGTH(sizeof(struct nfgenmsg));
}

/**
 * nfnl_recv - receive data from a nfnetlink subsystem
 * @h: nfnetlink handler
 * @buf: buffer where the data will be stored
 * @len: size of the buffer
 *
 * This function doesn't perform any sanity checking. So do no expect
 * that the data is well-formed. Such checkings are done by the parsing
 * functions.
 *
 * On success, 0 is returned. On error, -1 is returned and errno is set
 * appropiately.
 *
 * Note that ENOBUFS is returned in case that nfnetlink is exhausted. In
 * that case is possible that the information requested is incomplete.
 */
ssize_t 
nfnl_recv(const struct nfnl_handle *h, unsigned char *buf, size_t len)
{
	socklen_t addrlen;
	int status;
	struct sockaddr_nl peer;

	assert(h);
	assert(buf);
	assert(len > 0);
	
	if (len < sizeof(struct nlmsgerr)
	    || len < sizeof(struct nlmsghdr)) {
	    	errno = EBADMSG;
		return -1; 
	}

	addrlen = sizeof(h->peer);
	status = recvfrom(h->fd, buf, len, 0, (struct sockaddr *)&peer,	
			&addrlen);
	if (status <= 0)
		return status;

	if (addrlen != sizeof(peer)) {
		errno = EINVAL;
		return -1;
	}

	if (peer.nl_pid != 0) {
		errno = ENOMSG;
		return -1;
	}

	return status;
}
/**
 * nfnl_listen: listen for one or more netlink messages
 * @nfnhl: libnfnetlink handle
 * @handler: callback function to be called for every netlink message
 *          - the callback handler should normally return 0
 *          - but may return a negative error code which will cause
 *            nfnl_listen to return immediately with the same error code
 *          - or return a postivie error code which will cause 
 *            nfnl_listen to return after it has finished processing all
 *            the netlink messages in the current packet
 *          Thus a positive error code will terminate nfnl_listen "soon"
 *          without any loss of data, a negative error code will terminate
 *          nfnl_listen "very soon" and throw away data already read from
 *          the netlink socket.
 * @jarg: opaque argument passed on to callback
 *
 * This function is used to receive and process messages coming from an open
 * nfnetlink handler like events or information request via nfnl_send().
 *
 * On error, -1 is returned, unfortunately errno is not always set
 * appropiately. For that reason, the use of this function is DEPRECATED. 
 * Please, use nfnl_receive_process() instead.
 */
int nfnl_listen(struct nfnl_handle *nfnlh,
		int (*handler)(struct sockaddr_nl *, struct nlmsghdr *n,
			       void *), void *jarg)
{
	struct sockaddr_nl nladdr;
	char buf[NFNL_BUFFSIZE] __attribute__ ((aligned));
	struct iovec iov;
	int remain;
	struct nlmsghdr *h;
	struct nlmsgerr *msgerr;
	int quit=0;

	struct msghdr msg = {
		.msg_name    = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov     = &iov,
		.msg_iovlen  = 1,
	};

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	while (! quit) {
		remain = recvmsg(nfnlh->fd, &msg, 0);
		if (remain < 0) {
			if (errno == EINTR)
				continue;
			/* Bad file descriptor */
			else if (errno == EBADF)
				break;
			else if (errno == EAGAIN)
				break;
			nfnl_error("recvmsg overrun: %s", strerror(errno));
			continue;
		}
		if (remain == 0) {
			nfnl_error("EOF on netlink");
			return -1;
		}
		if (msg.msg_namelen != sizeof(nladdr)) {
			nfnl_error("Bad sender address len (%d)",
				   msg.msg_namelen);
			return -1;
		}

		for (h = (struct nlmsghdr *)buf; remain >= sizeof(*h);) {
			int err;
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);

			if (l < 0 || len > remain) {
				if (msg.msg_flags & MSG_TRUNC) {
					nfnl_error("MSG_TRUNC");
					return -1;
				}
				nfnl_error("Malformed msg (len=%d)", len);
				return -1;
			}

			/* end of messages reached, let's return */
			if (h->nlmsg_type == NLMSG_DONE)
				return 0;

			/* Break the loop if success is explicitely
			 * reported via NLM_F_ACK flag set */
			if (h->nlmsg_type == NLMSG_ERROR) {
				msgerr = NLMSG_DATA(h);
				return msgerr->error;
			}

			err = handler(&nladdr, h, jarg);
			if (err < 0)
				return err;
			quit |= err;
		
			/* FIXME: why not _NEXT macros, etc.? */
			//h = NLMSG_NEXT(h, remain);
			remain -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr *)((char *)h + NLMSG_ALIGN(len));
		}
		if (msg.msg_flags & MSG_TRUNC) {
			nfnl_error("MSG_TRUNC");
			continue;
		}
		if (remain) {
			nfnl_error("remnant size %d", remain);
			return -1;
		}
	}

	return quit;
}

/**
 * nfnl_talk - send a request and then receive and process messages returned
 * @nfnlh: nfnetelink handler
 * @n: netlink message that contains the request
 * @peer: peer PID
 * @groups: netlink groups
 * @junk: callback called if out-of-sequence messages were received
 * @jarg: data for the junk callback
 *
 * This function is used to request an action that does not returns any
 * information. On error, a negative value is returned, errno could be
 * set appropiately. For that reason, the use of this function is DEPRECATED.
 * Please, use nfnl_query() instead.
 */
int nfnl_talk(struct nfnl_handle *nfnlh, struct nlmsghdr *n, pid_t peer,
	      unsigned groups, struct nlmsghdr *answer,
	      int (*junk)(struct sockaddr_nl *, struct nlmsghdr *n, void *),
	      void *jarg)
{
	char buf[NFNL_BUFFSIZE] __attribute__ ((aligned));
	struct sockaddr_nl nladdr;
	struct nlmsghdr *h;
	unsigned int seq;
	int status;
	struct iovec iov = {
		n, n->nlmsg_len
	};
	struct msghdr msg = {
		.msg_name    = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov     = &iov,
		.msg_iovlen  = 1,
	};

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = peer;
	nladdr.nl_groups = groups;

	n->nlmsg_seq = seq = ++nfnlh->seq;
	/* FIXME: why ? */
	if (!answer)
		n->nlmsg_flags |= NLM_F_ACK;

	status = sendmsg(nfnlh->fd, &msg, 0);
	if (status < 0) {
		nfnl_error("sendmsg(netlink) %s", strerror(errno));
		return -1;
	}
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	while (1) {
		status = recvmsg(nfnlh->fd, &msg, 0);
		if (status < 0) {
			if (errno == EINTR)
				continue;
			nfnl_error("recvmsg over-run");
			continue;
		}
		if (status == 0) {
			nfnl_error("EOF on netlink");
			return -1;
		}
		if (msg.msg_namelen != sizeof(nladdr)) {
			nfnl_error("Bad sender address len %d",
				   msg.msg_namelen);
			return -1;
		}

		for (h = (struct nlmsghdr *)buf; status >= sizeof(*h); ) {
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);
			int err;

			if (l < 0 || len > status) {
				if (msg.msg_flags & MSG_TRUNC) {
					nfnl_error("Truncated message\n");
					return -1;
				}
				nfnl_error("Malformed message: len=%d\n", len);
				return -1; /* FIXME: libnetlink exits here */
			}

			if (h->nlmsg_pid != nfnlh->local.nl_pid ||
			    h->nlmsg_seq != seq) {
				if (junk) {
					err = junk(&nladdr, h, jarg);
					if (err < 0)
						return err;
				}
				goto cont;
			}

			if (h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = NLMSG_DATA(h);
				if (l < sizeof(struct nlmsgerr))
					nfnl_error("ERROR truncated\n");
				else {
					errno = -err->error;
					if (errno == 0) {
						if (answer)
							memcpy(answer, h, h->nlmsg_len);
						return 0;
					}
					perror("NFNETLINK answers");
				}
				return err->error;
			}
			if (answer) {
				memcpy(answer, h, h->nlmsg_len);
				return 0;
			}

			nfnl_error("Unexpected reply!\n");
cont:
			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr *)((char *)h + NLMSG_ALIGN(len));
		}
		if (msg.msg_flags & MSG_TRUNC) {
			nfnl_error("Messages truncated\n");
			continue;
		}
		if (status)
			nfnl_error("Remnant of size %d\n", status);
	}
}

/**
 * nfnl_addattr_l - Add variable length attribute to nlmsghdr
 * @n: netlink message header to which attribute is to be added
 * @maxlen: maximum length of netlink message header
 * @type: type of new attribute
 * @data: content of new attribute
 * @len: attribute length
 */
int nfnl_addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data,
		   int alen)
{
	int len = NFA_LENGTH(alen);
	struct nfattr *nfa;

	assert(n);
	assert(maxlen > 0);
	assert(type >= 0);

	if ((NLMSG_ALIGN(n->nlmsg_len) + len) > maxlen) {
		errno = ENOSPC;
		return -1;
	}

	nfa = NLMSG_TAIL(n);
	nfa->nfa_type = type;
	nfa->nfa_len = len;
	memcpy(NFA_DATA(nfa), data, alen);
	n->nlmsg_len = (NLMSG_ALIGN(n->nlmsg_len) + NFA_ALIGN(len));
	return 0;
}

/**
 * nfnl_nfa_addattr_l - Add variable length attribute to struct nfattr 
 *
 * @nfa: struct nfattr
 * @maxlen: maximal length of nfattr buffer
 * @type: type for new attribute
 * @data: content of new attribute
 * @alen: length of new attribute
 *
 */
int nfnl_nfa_addattr_l(struct nfattr *nfa, int maxlen, int type, 
		       const void *data, int alen)
{
	struct nfattr *subnfa;
	int len = NFA_LENGTH(alen);

	assert(nfa);
	assert(maxlen > 0);
	assert(type >= 0);

	if (NFA_ALIGN(nfa->nfa_len) + len > maxlen) {
		errno = ENOSPC;
		return -1;
	}

	subnfa = (struct nfattr *)(((char *)nfa) + NFA_ALIGN(nfa->nfa_len));
	subnfa->nfa_type = type;
	subnfa->nfa_len = len;
	memcpy(NFA_DATA(subnfa), data, alen);
	nfa->nfa_len = NFA_ALIGN(nfa->nfa_len) + len;

	return 0;
}

/**
 * nfnl_addattr8 - Add u_int8_t attribute to nlmsghdr
 *
 * @n: netlink message header to which attribute is to be added
 * @maxlen: maximum length of netlink message header
 * @type: type of new attribute
 * @data: content of new attribute
 */
int nfnl_addattr8(struct nlmsghdr *n, int maxlen, int type, u_int8_t data)
{
	assert(n);
	assert(maxlen > 0);
	assert(type >= 0);

	return nfnl_addattr_l(n, maxlen, type, &data, sizeof(data));
}

/**
 * nfnl_nfa_addattr16 - Add u_int16_t attribute to struct nfattr 
 *
 * @nfa: struct nfattr
 * @maxlen: maximal length of nfattr buffer
 * @type: type for new attribute
 * @data: content of new attribute
 *
 */
int nfnl_nfa_addattr16(struct nfattr *nfa, int maxlen, int type, 
		       u_int16_t data)
{
	assert(nfa);
	assert(maxlen > 0);
	assert(type >= 0);

	return nfnl_nfa_addattr_l(nfa, maxlen, type, &data, sizeof(data));
}

/**
 * nfnl_addattr16 - Add u_int16_t attribute to nlmsghdr
 *
 * @n: netlink message header to which attribute is to be added
 * @maxlen: maximum length of netlink message header
 * @type: type of new attribute
 * @data: content of new attribute
 *
 */
int nfnl_addattr16(struct nlmsghdr *n, int maxlen, int type,
		   u_int16_t data)
{
	assert(n);
	assert(maxlen > 0);
	assert(type >= 0);

	return nfnl_addattr_l(n, maxlen, type, &data, sizeof(data));
}

/**
 * nfnl_nfa_addattr32 - Add u_int32_t attribute to struct nfattr 
 *
 * @nfa: struct nfattr
 * @maxlen: maximal length of nfattr buffer
 * @type: type for new attribute
 * @data: content of new attribute
 *
 */
int nfnl_nfa_addattr32(struct nfattr *nfa, int maxlen, int type, 
		       u_int32_t data)
{
	assert(nfa);
	assert(maxlen > 0);
	assert(type >= 0);

	return nfnl_nfa_addattr_l(nfa, maxlen, type, &data, sizeof(data));
}

/**
 * nfnl_addattr32 - Add u_int32_t attribute to nlmsghdr
 *
 * @n: netlink message header to which attribute is to be added
 * @maxlen: maximum length of netlink message header
 * @type: type of new attribute
 * @data: content of new attribute
 *
 */
int nfnl_addattr32(struct nlmsghdr *n, int maxlen, int type,
		   u_int32_t data)
{
	assert(n);
	assert(maxlen > 0);
	assert(type >= 0);

	return nfnl_addattr_l(n, maxlen, type, &data, sizeof(data));
}

/**
 * nfnl_parse_attr - Parse a list of nfattrs into a pointer array
 *
 * @tb: pointer array, will be filled in (output)
 * @max: size of pointer array
 * @nfa: pointer to list of nfattrs
 * @len: length of 'nfa'
 *
 * The returned value is equal to the number of remaining bytes of the netlink
 * message that cannot be parsed.
 */
int nfnl_parse_attr(struct nfattr *tb[], int max, struct nfattr *nfa, int len)
{
	assert(tb);
	assert(max > 0);
	assert(nfa);

	memset(tb, 0, sizeof(struct nfattr *) * max);

	while (NFA_OK(nfa, len)) {
		if (NFA_TYPE(nfa) <= max)
			tb[NFA_TYPE(nfa)-1] = nfa;
                nfa = NFA_NEXT(nfa,len);
	}

	return len;
}

/**
 * nfnl_build_nfa_iovec - Build two iovec's from tag, length and value
 *
 * @iov: pointer to array of two 'struct iovec' (caller-allocated)
 * @nfa: pointer to 'struct nfattr' (caller-allocated)
 * @type: type (tag) of attribute
 * @len: length of value
 * @val: pointer to buffer containing 'value'
 *
 */ 
void nfnl_build_nfa_iovec(struct iovec *iov, struct nfattr *nfa, 
			  u_int16_t type, u_int32_t len, unsigned char *val)
{
	assert(iov);
	assert(nfa);

        /* Set the attribut values */ 
        nfa->nfa_len = sizeof(struct nfattr) + len;
        nfa->nfa_type = type;

	iov[0].iov_base = nfa;
	iov[0].iov_len = sizeof(*nfa);
	iov[1].iov_base = val;
	iov[1].iov_len = NFA_ALIGN(len);
}

#ifndef SO_RCVBUFFORCE
#define SO_RCVBUFFORCE	(33)
#endif

/**
 * nfnl_rcvbufsiz - set the socket buffer size
 * @h: nfnetlink handler
 * @size: size of the buffer we want to set
 *
 * This function sets the new size of the socket buffer. Use this setting
 * to increase the socket buffer size if your system is reporting ENOBUFS
 * errors.
 *
 * This function returns the new size of the socket buffer.
 */
unsigned int nfnl_rcvbufsiz(const struct nfnl_handle *h, unsigned int size)
{
	int status;
	socklen_t socklen = sizeof(size);
	unsigned int read_size = 0;

	assert(h);

	/* first we try the FORCE option, which is introduced in kernel
	 * 2.6.14 to give "root" the ability to override the system wide
	 * maximum */
	status = setsockopt(h->fd, SOL_SOCKET, SO_RCVBUFFORCE, &size, socklen);
	if (status < 0) {
		/* if this didn't work, we try at least to get the system
		 * wide maximum (or whatever the user requested) */
		setsockopt(h->fd, SOL_SOCKET, SO_RCVBUF, &size, socklen);
	}
	getsockopt(h->fd, SOL_SOCKET, SO_RCVBUF, &read_size, &socklen);

	return read_size;
}

/**
 * nfnl_get_msg_first - get the first message of a multipart netlink message
 * @h: nfnetlink handle
 * @buf: data received that we want to process
 * @len: size of the data received
 *
 * This function returns a pointer to the first netlink message contained
 * in the chunk of data received from certain nfnetlink subsystem.
 *
 * On success, a valid address that points to the netlink message is returned.
 * On error, NULL is returned.
 */
struct nlmsghdr *nfnl_get_msg_first(struct nfnl_handle *h,
				    const unsigned char *buf,
				    size_t len)
{
	struct nlmsghdr *nlh;

	assert(h);
	assert(buf);
	assert(len > 0);

	/* first message in buffer */
	nlh = (struct nlmsghdr *)buf;
	if (!NLMSG_OK(nlh, len))
		return NULL;
	h->last_nlhdr = nlh;

	return nlh;
}

struct nlmsghdr *nfnl_get_msg_next(struct nfnl_handle *h,
				   const unsigned char *buf,
				   size_t len)
{
	struct nlmsghdr *nlh;
	size_t remain_len;

	assert(h);
	assert(buf);
	assert(len > 0);

	/* if last header in handle not inside this buffer, 
	 * drop reference to last header */
	if (!h->last_nlhdr ||
	    (unsigned char *)h->last_nlhdr >= (buf + len)  ||
	    (unsigned char *)h->last_nlhdr < buf) {
		h->last_nlhdr = NULL;
		return NULL;
	}

	/* n-th part of multipart message */
	if (h->last_nlhdr->nlmsg_type == NLMSG_DONE ||
	    h->last_nlhdr->nlmsg_flags & NLM_F_MULTI) {
		/* if last part in multipart message or no
		 * multipart message at all, return */
		h->last_nlhdr = NULL;
		return NULL;
	}

	remain_len = (len - ((unsigned char *)h->last_nlhdr - buf));
	nlh = NLMSG_NEXT(h->last_nlhdr, remain_len);

	if (!NLMSG_OK(nlh, remain_len)) {
		h->last_nlhdr = NULL;
		return NULL;
	}

	h->last_nlhdr = nlh;

	return nlh;
}

/**
 * nfnl_callback_register - register a callback for a certain message type
 * @ssh: nfnetlink subsys handler
 * @type: subsys call
 * @cb: nfnetlink callback to be registered
 *
 * On success, 0 is returned. On error, -1 is returned and errno is set
 * appropiately.
 */
int nfnl_callback_register(struct nfnl_subsys_handle *ssh,
			   u_int8_t type, struct nfnl_callback *cb)
{
	assert(ssh);
	assert(cb);

	if (type >= ssh->cb_count) {
		errno = EINVAL;
		return -1;
	}

	memcpy(&ssh->cb[type], cb, sizeof(*cb));

	return 0;
}

/**
 * nfnl_callback_unregister - unregister a certain callback
 * @ssh: nfnetlink subsys handler
 * @type: subsys call
 *
 * On sucess, 0 is returned. On error, -1 is returned and errno is
 * set appropiately.
 */
int nfnl_callback_unregister(struct nfnl_subsys_handle *ssh, u_int8_t type)
{
	assert(ssh);

	if (type >= ssh->cb_count) {
		errno = EINVAL;
		return -1;
	}

	ssh->cb[type].call = NULL;

	return 0;
}

int nfnl_check_attributes(const struct nfnl_handle *h,
			 const struct nlmsghdr *nlh,
			 struct nfattr *nfa[])
{
	assert(h);
	assert(nlh);
	assert(nfa);

	int min_len;
	u_int8_t type = NFNL_MSG_TYPE(nlh->nlmsg_type);
	u_int8_t subsys_id = NFNL_SUBSYS_ID(nlh->nlmsg_type);
	const struct nfnl_subsys_handle *ssh;
	struct nfnl_callback *cb;

	if (subsys_id > NFNL_MAX_SUBSYS)
		return -EINVAL;

	ssh = &h->subsys[subsys_id];
 	cb = &ssh->cb[type];

#if 1
	/* checks need to be enabled as soon as this is called from
	 * somebody else than __nfnl_handle_msg */
	if (type >= ssh->cb_count)
		return -EINVAL;

	min_len = NLMSG_SPACE(sizeof(struct nfgenmsg));
	if (nlh->nlmsg_len < min_len)
		return -EINVAL;
#endif
	memset(nfa, 0, sizeof(struct nfattr *) * cb->attr_count);

	if (nlh->nlmsg_len > min_len) {
		struct nfattr *attr = NFM_NFA(NLMSG_DATA(nlh));
		int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(min_len);

		while (NFA_OK(attr, attrlen)) {
			unsigned int flavor = NFA_TYPE(attr);
			if (flavor) {
				if (flavor > cb->attr_count) {
					/* we have received an attribute from
					 * the kernel which we don't understand
					 * yet. We have to silently ignore this
					 * for the sake of future compatibility */
					attr = NFA_NEXT(attr, attrlen);
					continue;
				}
				nfa[flavor - 1] = attr;
			}
			attr = NFA_NEXT(attr, attrlen);
		}
	}

	return 0;
}

static int __nfnl_handle_msg(struct nfnl_handle *h, struct nlmsghdr *nlh,
			     int len)
{
	struct nfnl_subsys_handle *ssh;
	u_int8_t type = NFNL_MSG_TYPE(nlh->nlmsg_type);
	u_int8_t subsys_id = NFNL_SUBSYS_ID(nlh->nlmsg_type);
	int err = 0;

	if (subsys_id > NFNL_MAX_SUBSYS)
		return -1;

	ssh = &h->subsys[subsys_id];

	if (nlh->nlmsg_len < NLMSG_LENGTH(NLMSG_ALIGN(sizeof(struct nfgenmsg))))
		return -1;

	if (type >= ssh->cb_count)
		return -1;

	if (ssh->cb[type].attr_count) {
		struct nfattr *nfa[ssh->cb[type].attr_count];

		err = nfnl_check_attributes(h, nlh, nfa);
		if (err < 0)
			return err;
		if (ssh->cb[type].call)
			return ssh->cb[type].call(nlh, nfa, ssh->cb[type].data);
	}
	return 0;
}

int nfnl_handle_packet(struct nfnl_handle *h, char *buf, int len)
{

	while (len >= NLMSG_SPACE(0)) {
		u_int32_t rlen;
		struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

		if (nlh->nlmsg_len < sizeof(struct nlmsghdr)
		    || len < nlh->nlmsg_len)
			return -1;

		rlen = NLMSG_ALIGN(nlh->nlmsg_len);
		if (rlen > len)
			rlen = len;

		if (__nfnl_handle_msg(h, nlh, rlen) < 0)
			return -1;

		len -= rlen;
		buf += rlen;
	}
	return 0;
}

static int nfnl_is_error(struct nfnl_handle *h, struct nlmsghdr *nlh)
{
	/* This message is an ACK or a DONE */
	if (nlh->nlmsg_type == NLMSG_ERROR ||
	    (nlh->nlmsg_type == NLMSG_DONE &&
	     nlh->nlmsg_flags & NLM_F_MULTI)) {
		if (nlh->nlmsg_len < NLMSG_ALIGN(sizeof(struct nlmsgerr))) {
			errno = EBADMSG;
			return 1;
		}
		errno = -(*((int *)NLMSG_DATA(nlh)));
		return 1;
	}
	return 0;
}

/* On error, -1 is returned and errno is set appropiately. On success, 
 * 0 is returned if there is no more data to process, >0 if there is
 * more data to process */
static int nfnl_step(struct nfnl_handle *h, struct nlmsghdr *nlh)
{
	struct nfnl_subsys_handle *ssh;
	u_int8_t type = NFNL_MSG_TYPE(nlh->nlmsg_type);
	u_int8_t subsys_id = NFNL_SUBSYS_ID(nlh->nlmsg_type);

	/* Is this an error message? */
	if (nfnl_is_error(h, nlh)) {
		/* This is an ACK */
		if (errno == 0)
			return 0;
		/* This an error message */
		return -1;
	}
	
	/* nfnetlink sanity checks: check for nfgenmsg size */
	if (nlh->nlmsg_len < NLMSG_SPACE(sizeof(struct nfgenmsg))) {
		errno = ENOSPC;
		return -1;
	}

	if (subsys_id > NFNL_MAX_SUBSYS) {
		errno = ENOENT;
		return -1;
	}

	ssh = &h->subsys[subsys_id];
	if (!ssh) {
		errno = ENOENT;
		return -1;
	}

	if (type >= ssh->cb_count) {
		errno = ENOENT;
		return -1;
	}

	if (ssh->cb[type].attr_count) {
		int err;
		struct nfattr *tb[ssh->cb[type].attr_count];
		struct nfattr *attr = NFM_NFA(NLMSG_DATA(nlh));
		int min_len = NLMSG_SPACE(sizeof(struct nfgenmsg));
		int len = nlh->nlmsg_len - NLMSG_ALIGN(min_len);

		err = nfnl_parse_attr(tb, ssh->cb[type].attr_count, attr, len);
		if (err == -1)
			return -1;

		if (ssh->cb[type].call) {
			/*
			 * On error, the callback returns NFNL_CB_FAILURE and
			 * errno must be explicitely set. On success, 
			 * NFNL_CB_STOP is returned and we're done, otherwise 
			 * NFNL_CB_CONTINUE means that we want to continue 
			 * data processing.
			 */
			return ssh->cb[type].call(nlh,
						  tb,
						  ssh->cb[type].data);
		}
	}
	/* no callback set, continue data processing */
	return 1;
}

/**
 * nfnl_process - process data coming from a nfnetlink system
 * @h: nfnetlink handler
 * @buf: buffer that contains the netlink message
 * @len: size of the data contained in the buffer (not the buffer size)
 *
 * This function processes all the nfnetlink messages contained inside a
 * buffer. It performs the appropiate sanity checks and passes the message
 * to a certain handler that is registered via register_callback().
 *
 * On success, NFNL_CB_STOP is returned if the data processing has finished.
 * If a value NFNL_CB_CONTINUE is returned, then there is more data to
 * process. On error, NFNL_CB_CONTINUE is returned and errno is set to the 
 * appropiate value.
 *
 * In case that the callback returns NFNL_CB_FAILURE, errno may be set by
 * the library client. If your callback decides not to process data anymore
 * for any reason, then it must return NFNL_CB_STOP. Otherwise, if the 
 * callback continues the processing NFNL_CB_CONTINUE is returned.
 */
int nfnl_process(struct nfnl_handle *h, const unsigned char *buf, size_t len)
{
	int ret = 0;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

	assert(h);
	assert(buf);
	assert(len > 0);

	/* check for out of sequence message */
	if (nlh->nlmsg_seq && nlh->nlmsg_seq != h->seq) {
		errno = EILSEQ;
		return -1;
	}
	while (len >= NLMSG_SPACE(0) && NLMSG_OK(nlh, len)) {

		ret = nfnl_step(h, nlh);
		if (ret <= NFNL_CB_STOP)
			break;

		nlh = NLMSG_NEXT(nlh, len);
	}
	return ret;
}

/*
 * New parsing functions based on iterators
 */

struct nfnl_iterator {
	struct nlmsghdr *nlh;
	unsigned int	len;
};

/**
 * nfnl_iterator_create: create an nfnetlink iterator
 * @h: nfnetlink handler
 * @buf: buffer that contains data received from a nfnetlink system
 * @len: size of the data contained in the buffer (not the buffer size)
 *
 * This function creates an iterator that can be used to parse nfnetlink
 * message one by one. The iterator gives more control to the programmer
 * in the messages processing.
 *
 * On success, a valid address is returned. On error, NULL is returned
 * and errno is set to the appropiate value.
 */
struct nfnl_iterator *
nfnl_iterator_create(const struct nfnl_handle *h,
		     const char *buf,
		     size_t len)
{
	struct nlmsghdr *nlh;
	struct nfnl_iterator *it;

	assert(h);
	assert(buf);
	assert(len > 0);

	it = malloc(sizeof(struct nfnl_iterator));
	if (!it) {
		errno = ENOMEM;
		return NULL;
	}

	/* first message in buffer */
	nlh = (struct nlmsghdr *)buf;
	if (len < NLMSG_SPACE(0) || !NLMSG_OK(nlh, len)) {
		free(it);
		errno = EBADMSG;
		return NULL;
	}
	it->nlh = nlh;
	it->len = len;

	return it;
}

/**
 * nfnl_iterator_destroy - destroy a nfnetlink iterator
 * @it: nfnetlink iterator
 *
 * This function destroys a certain iterator. Nothing is returned.
 */
void nfnl_iterator_destroy(struct nfnl_iterator *it)
{
	assert(it);
	free(it);
}

/**
 * nfnl_iterator_process - process a nfnetlink message
 * @h: nfnetlink handler
 * @it: nfnetlink iterator that contains the current message to be proccesed
 *
 * This function process just the current message selected by the iterator.
 * On success, a value greater or equal to zero is returned. On error,
 * -1 is returned and errno is appropiately set.
 */
int nfnl_iterator_process(struct nfnl_handle *h, struct nfnl_iterator *it)
{
	assert(h);
	assert(it->nlh);

        /* check for out of sequence message */
	if (it->nlh->nlmsg_seq && it->nlh->nlmsg_seq != h->seq) {
		errno = EILSEQ;
		return -1;
	}	
	if (it->len < NLMSG_SPACE(0) || !NLMSG_OK(it->nlh, it->len)) {
		errno = EBADMSG;
		return -1;
	}
	return nfnl_step(h, it->nlh);
}

/**
 * nfnl_iterator_next - get the next message hold by the iterator
 * @h: nfnetlink handler
 * @it: nfnetlink iterator that contains the current message processed
 *
 * This function update the current message to be processed pointer.
 * It returns NFNL_CB_CONTINUE if there is still more messages to be 
 * processed, otherwise NFNL_CB_STOP is returned.
 */
int nfnl_iterator_next(const struct nfnl_handle *h, struct nfnl_iterator *it)
{
	assert(h);
	assert(it);

	it->nlh = NLMSG_NEXT(it->nlh, it->len);
	if (!it->nlh)
		return 0;
	return 1;
}

/**
 * nfnl_catch - get responses from the nfnetlink system and process them
 * @h: nfnetlink handler
*
 * This function handles the data received from the nfnetlink system.
 * For example, events generated by one of the subsystems. The message
 * is passed to the callback registered via callback_register(). Note that
 * this a replacement of nfnl_listen and its use is recommended.
 * 
 * On success, 0 is returned. On error, a -1 is returned. If you do not
 * want to listen to events anymore, then your callback must return 
 * NFNL_CB_STOP.
 *
 * Note that ENOBUFS is returned in case that nfnetlink is exhausted. In
 * that case is possible that the information requested is incomplete.
 */
int nfnl_catch(struct nfnl_handle *h)
{
	int ret;

	assert(h);

	while (1) {
		unsigned char buf[h->rcv_buffer_size]
			__attribute__ ((aligned));

		ret = nfnl_recv(h, buf, sizeof(buf));
		if (ret == -1) {
			/* interrupted syscall must retry */
			if (errno == EINTR)
				continue;
			break;
		}

		ret = nfnl_process(h, buf, ret);
		if (ret <= NFNL_CB_STOP)
			break; 
	}

	return ret;
}

/**
 * nfnl_query - request/response communication challenge
 * @h: nfnetlink handler
 * @nlh: nfnetlink message to be sent
 *
 * This function sends a nfnetlink message to a certain subsystem and
 * receives the response messages associated, such messages are passed to
 * the callback registered via register_callback(). Note that this function
 * is a replacement for nfnl_talk, its use is recommended.
 *
 * On success, 0 is returned. On error, a negative is returned. If your
 * does not want to listen to events anymore, then your callback must 
 * return NFNL_CB_STOP.
 *
 * Note that ENOBUFS is returned in case that nfnetlink is exhausted. In
 * that case is possible that the information requested is incomplete.
 */
int nfnl_query(struct nfnl_handle *h, struct nlmsghdr *nlh)
{
	assert(h);
	assert(nlh);

	if (nfnl_send(h, nlh) == -1)
		return -1;

	return nfnl_catch(h);
}
