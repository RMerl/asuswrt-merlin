/* rtnl - rtnetlink utility functions
 *
 * (C) 2004 by Astaro AG, written by Harald Welte <hwelte@astaro.com>
 * 
 * Adapted to nfnetlink by Eric Leblond <eric@inl.fr>
 *
 * This software is free software and licensed under GNU GPLv2. 
 *
 */

/* rtnetlink - routing table netlink interface */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <linux/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "rtnl.h"

#define rtnl_log(x, ...)

static inline struct rtnl_handler *
find_handler(struct rtnl_handle *rtnl_handle, u_int16_t type)
{
	struct rtnl_handler *h;
	for (h = rtnl_handle->handlers; h; h = h->next) {
		if (h->nlmsg_type == type)
			return h;
	}
	return NULL;
}

static int call_handler(struct rtnl_handle *rtnl_handle,
			u_int16_t type,
			struct nlmsghdr *hdr)
{
	struct rtnl_handler *h = find_handler(rtnl_handle, type);

	if (!h) {
		rtnl_log(LOG_DEBUG, "no registered handler for type %u", type);
		return 0;
	}

	return (h->handlefn)(hdr, h->arg);
}

/* rtnl_handler_register - register handler for given nlmsg type
 * @hdlr:	handler structure
 */
int rtnl_handler_register(struct rtnl_handle *rtnl_handle,
			  struct rtnl_handler *hdlr)
{
	rtnl_log(LOG_DEBUG, "registering handler for type %u",
		 hdlr->nlmsg_type);
	hdlr->next = rtnl_handle->handlers;
	rtnl_handle->handlers = hdlr;
	return 1;
}

/* rtnl_handler_unregister - unregister handler for given nlmst type
 * @hdlr:	handler structure
 */
int rtnl_handler_unregister(struct rtnl_handle *rtnl_handle,
			    struct rtnl_handler *hdlr)
{
	struct rtnl_handler *h, *prev = NULL;

	rtnl_log(LOG_DEBUG, "unregistering handler for type %u",
		 hdlr->nlmsg_type);

	for (h = rtnl_handle->handlers; h; h = h->next) {
		if (h == hdlr) {
			if (prev)
				prev->next = h->next;
			else
				rtnl_handle->handlers = h->next;
			return 1;
		}
		prev = h;
	}
	return 0;
}

int rtnl_parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * max);

	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta,len);
	}
	if (len)
		return -1;
	return 0;
}

/* rtnl_dump_type - ask rtnetlink to dump a specific table
 * @type:	type of table to be dumped
 */
int rtnl_dump_type(struct rtnl_handle *rtnl_handle, unsigned int type)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct sockaddr_nl nladdr;

	memset(&nladdr, 0, sizeof(nladdr));
	memset(&req, 0, sizeof(req));
	nladdr.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = sizeof(req);
 	req.nlh.nlmsg_type = type;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = rtnl_handle->rtnl_dump = ++(rtnl_handle->rtnl_seq);
	req.g.rtgen_family = AF_INET;

	return sendto(rtnl_handle->rtnl_fd, &req, sizeof(req), 0,
		      (struct sockaddr*)&nladdr, sizeof(nladdr));
}

/* rtnl_receive - receive netlink packets from rtnetlink socket */
int rtnl_receive(struct rtnl_handle *rtnl_handle)
{
	int status;
	char buf[8192];
	struct sockaddr_nl nladdr;
	struct iovec iov = { buf, sizeof(buf) };
	struct nlmsghdr *h;

	struct msghdr msg = {
		.msg_name    = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov     = &iov,
		.msg_iovlen  = 1,
	};

	status = recvmsg(rtnl_handle->rtnl_fd, &msg, 0);
	if (status < 0) {
		if (errno == EINTR)
			return 0;
		rtnl_log(LOG_NOTICE, "OVERRUN on rtnl socket");
		return -1;
	}
	if (status == 0) {
		rtnl_log(LOG_ERROR, "EOF on rtnl socket");
		return -1;
	}
	if (msg.msg_namelen != sizeof(nladdr)) {
		rtnl_log(LOG_ERROR, "invalid address size");
		return -1;
	}

	h = (struct nlmsghdr *) buf;
	while (NLMSG_OK(h, status)) {
#if 0
		if (h->nlmsg_pid != rtnl_local.nl_pid ||
		    h->nlmsg_seq != rtnl_dump) {
			goto skip;
		}
#endif

		if (h->nlmsg_type == NLMSG_DONE) {
			rtnl_log(LOG_NOTICE, "NLMSG_DONE");
			return 0;
		}
		if (h->nlmsg_type == NLMSG_ERROR) { 
			struct nlmsgerr *err = NLMSG_DATA(h);
			if (h->nlmsg_len>=NLMSG_LENGTH(sizeof(struct nlmsgerr)))
				errno = -err->error;
			rtnl_log(LOG_ERROR, "NLMSG_ERROR, errnp=%d",
				 errno);
			return -1;
		}

		if (call_handler(rtnl_handle, h->nlmsg_type, h) == 0) 
			rtnl_log(LOG_NOTICE, "unhandled nlmsg_type %u",
				 h->nlmsg_type);
		h = NLMSG_NEXT(h, status);
	}
	return 1;
}

int rtnl_receive_multi(struct rtnl_handle *rtnl_handle)
{
	while (1) {
		if (rtnl_receive(rtnl_handle) <= 0)
			break;
	}
	return 1;
}

/* rtnl_open - constructor of rtnetlink module */
struct rtnl_handle *rtnl_open(void)
{
	socklen_t addrlen;
	struct rtnl_handle *h;

	h = calloc(1, sizeof(struct rtnl_handle));
	if (!h)
		return NULL;

	addrlen = sizeof(h->rtnl_local);

	h->rtnl_local.nl_pid = getpid();
	h->rtnl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (h->rtnl_fd < 0) {
		rtnl_log(LOG_ERROR, "unable to create rtnetlink socket");
		goto err;
	}

	memset(&h->rtnl_local, 0, sizeof(h->rtnl_local));
	h->rtnl_local.nl_family = AF_NETLINK;
	h->rtnl_local.nl_groups = RTMGRP_LINK;
	if (bind(h->rtnl_fd, (struct sockaddr *) &h->rtnl_local, addrlen) < 0) {
		rtnl_log(LOG_ERROR, "unable to bind rtnetlink socket");
		goto err_close;
	}

	if (getsockname(h->rtnl_fd, 
			(struct sockaddr *) &h->rtnl_local, 
			&addrlen) < 0) {
		rtnl_log(LOG_ERROR, "cannot gescockname(rtnl_socket)");
		goto err_close;
	}

	if (addrlen != sizeof(h->rtnl_local)) {
		rtnl_log(LOG_ERROR, "invalid address size %u", addr_len);
		goto err_close;
	}

	if (h->rtnl_local.nl_family != AF_NETLINK) {
		rtnl_log(LOG_ERROR, "invalid AF %u", h->rtnl_local.nl_family);
		goto err_close;
	}

	h->rtnl_seq = time(NULL);

	return h;

err_close:
	close(h->rtnl_fd);
err:
	free(h);
	return NULL;
}

/* rtnl_close - destructor of rtnetlink module */
void rtnl_close(struct rtnl_handle *rtnl_handle)
{
	close(rtnl_handle->rtnl_fd);
	free(rtnl_handle);
	return;
}
