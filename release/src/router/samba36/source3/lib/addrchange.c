/*
 * Samba Unix/Linux SMB client library
 * Copyright (C) Volker Lendecke 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "lib/addrchange.h"
#include "../lib/util/tevent_ntstatus.h"

#if HAVE_LINUX_RTNETLINK_H

#include "asm/types.h"
#include "linux/netlink.h"
#include "linux/rtnetlink.h"
#include "lib/async_req/async_sock.h"

struct addrchange_context {
	int sock;
};

static int addrchange_context_destructor(struct addrchange_context *c);

NTSTATUS addrchange_context_create(TALLOC_CTX *mem_ctx,
				   struct addrchange_context **pctx)
{
	struct addrchange_context *ctx;
	struct sockaddr_nl addr;
	NTSTATUS status;
	int res;

	ctx = talloc(mem_ctx, struct addrchange_context);
	if (ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ctx->sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (ctx->sock == -1) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}
	talloc_set_destructor(ctx, addrchange_context_destructor);

	/*
	 * We're interested in address changes
	 */
	ZERO_STRUCT(addr);
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_IFADDR;

	res = bind(ctx->sock, (struct sockaddr *)(void *)&addr, sizeof(addr));
	if (res == -1) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	*pctx = ctx;
	return NT_STATUS_OK;
fail:
	TALLOC_FREE(ctx);
	return status;
}

static int addrchange_context_destructor(struct addrchange_context *c)
{
	if (c->sock != -1) {
		close(c->sock);
		c->sock = -1;
	}
	return 0;
}

struct addrchange_state {
	struct tevent_context *ev;
	struct addrchange_context *ctx;
	uint8_t buf[8192];
	struct sockaddr_storage fromaddr;
	socklen_t fromaddr_len;

	enum addrchange_type type;
	struct sockaddr_storage addr;
};

static void addrchange_done(struct tevent_req *subreq);

struct tevent_req *addrchange_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   struct addrchange_context *ctx)
{
	struct tevent_req *req, *subreq;
	struct addrchange_state *state;

	req = tevent_req_create(mem_ctx, &state, struct addrchange_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->ctx = ctx;

	state->fromaddr_len = sizeof(state->fromaddr);
	subreq = recvfrom_send(state, state->ev, state->ctx->sock,
			       state->buf, sizeof(state->buf), 0,
			       &state->fromaddr, &state->fromaddr_len);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, state->ev);
	}
	tevent_req_set_callback(subreq, addrchange_done, req);
	return req;
}

static void addrchange_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct addrchange_state *state = tevent_req_data(
		req, struct addrchange_state);
	struct sockaddr_nl *addr;
	struct nlmsghdr *h;
	struct ifaddrmsg *ifa;
	struct rtattr *rta;
	ssize_t received;
	int len;
	int err;
	bool found;

	received = recvfrom_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (received == -1) {
		DEBUG(10, ("recvfrom returned %s\n", strerror(errno)));
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	if ((state->fromaddr_len != sizeof(struct sockaddr_nl))
	    || (state->fromaddr.ss_family != AF_NETLINK)) {
		DEBUG(10, ("Got message from wrong addr\n"));
		goto retry;
	}

	addr = (struct sockaddr_nl *)(void *)&state->addr;
	if (addr->nl_pid != 0) {
		DEBUG(10, ("Got msg from pid %d, not from the kernel\n",
			   (int)addr->nl_pid));
		goto retry;
	}

	if (received < sizeof(struct nlmsghdr)) {
		DEBUG(10, ("received %d, expected at least %d\n",
			   (int)received, (int)sizeof(struct nlmsghdr)));
		goto retry;
	}

	h = (struct nlmsghdr *)state->buf;
	if (h->nlmsg_len < sizeof(struct nlmsghdr)) {
		DEBUG(10, ("nlmsg_len=%d, expected at least %d\n",
			   (int)h->nlmsg_len, (int)sizeof(struct nlmsghdr)));
		goto retry;
	}
	if (h->nlmsg_len > received) {
		DEBUG(10, ("nlmsg_len=%d, expected at most %d\n",
			   (int)h->nlmsg_len, (int)received));
		goto retry;
	}
	switch (h->nlmsg_type) {
	case RTM_NEWADDR:
		state->type = ADDRCHANGE_ADD;
		break;
	case RTM_DELADDR:
		state->type = ADDRCHANGE_DEL;
		break;
	default:
		DEBUG(10, ("Got unexpected type %d - ignoring\n", h->nlmsg_type));
		goto retry;
	}

	if (h->nlmsg_len < sizeof(struct nlmsghdr)+sizeof(struct ifaddrmsg)) {
		DEBUG(10, ("nlmsg_len=%d, expected at least %d\n",
			   (int)h->nlmsg_len,
			   (int)(sizeof(struct nlmsghdr)
				 +sizeof(struct ifaddrmsg))));
		tevent_req_nterror(req, NT_STATUS_UNEXPECTED_IO_ERROR);
		return;
	}

	ifa = (struct ifaddrmsg *)NLMSG_DATA(h);

	state->addr.ss_family = ifa->ifa_family;

	rta = IFA_RTA(ifa);
	len = h->nlmsg_len - sizeof(struct nlmsghdr) + sizeof(struct ifaddrmsg);

	found = false;

	for (rta = IFA_RTA(ifa); RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {

		if ((rta->rta_type != IFA_LOCAL)
		    && (rta->rta_type != IFA_ADDRESS)) {
			continue;
		}

		switch (ifa->ifa_family) {
		case AF_INET: {
			struct sockaddr_in *v4_addr;
			v4_addr = (struct sockaddr_in *)(void *)&state->addr;

			if (RTA_PAYLOAD(rta) != sizeof(uint32_t)) {
				continue;
			}
			v4_addr->sin_addr.s_addr = *(uint32_t *)RTA_DATA(rta);
			found = true;
			break;
		}
		case AF_INET6: {
			struct sockaddr_in6 *v6_addr;
			v6_addr = (struct sockaddr_in6 *)(void *)&state->addr;

			if (RTA_PAYLOAD(rta) !=
			    sizeof(v6_addr->sin6_addr.s6_addr)) {
				continue;
			}
			memcpy(v6_addr->sin6_addr.s6_addr, RTA_DATA(rta),
			       sizeof(v6_addr->sin6_addr.s6_addr));
			found = true;
			break;
		}
		}
	}

	if (!found) {
		tevent_req_nterror(req, NT_STATUS_INVALID_ADDRESS);
		return;
	}

	tevent_req_done(req);
	return;

retry:
	state->fromaddr_len = sizeof(state->fromaddr);
	subreq = recvfrom_send(state, state->ev, state->ctx->sock,
			       state->buf, sizeof(state->buf), 0,
			       &state->fromaddr, &state->fromaddr_len);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, addrchange_done, req);
}

NTSTATUS addrchange_recv(struct tevent_req *req, enum addrchange_type *type,
			 struct sockaddr_storage *addr)
{
	struct addrchange_state *state = tevent_req_data(
		req, struct addrchange_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*type = state->type;
	*addr = state->addr;
	return NT_STATUS_OK;
}

#else

NTSTATUS addrchange_context_create(TALLOC_CTX *mem_ctx,
				   struct addrchange_context **pctx)
{
	return NT_STATUS_NOT_SUPPORTED;
}

struct tevent_req *addrchange_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   struct addrchange_context *ctx)
{
	return NULL;
}

NTSTATUS addrchange_recv(struct tevent_req *req, enum addrchange_type *type,
			 struct sockaddr_storage *addr)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

#endif
