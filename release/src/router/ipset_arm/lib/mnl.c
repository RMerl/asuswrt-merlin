/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>				/* assert */
#include <errno.h>				/* errno */
#include <stdlib.h>				/* calloc, free */
#include <time.h>				/* time */
#include <arpa/inet.h>				/* hto* */

#include <libipset/linux_ip_set.h>		/* enum ipset_cmd */
#include <libipset/debug.h>			/* D() */
#include <libipset/session.h>			/* ipset_session_handle */
#include <libipset/ui.h>			/* IPSET_ENV_EXIST */
#include <libipset/utils.h>			/* UNUSED */
#include <libipset/mnl.h>			/* prototypes */

#ifndef NFNL_SUBSYS_IPSET
#define NFNL_SUBSYS_IPSET	6
#endif

/* Internal data structure for the kernel-userspace communication parameters */
struct ipset_handle {
	struct mnl_socket *h;		/* the mnl socket */
	unsigned int seq;		/* netlink message sequence number */
	unsigned int portid;		/* the socket port identifier */
	mnl_cb_t *cb_ctl;		/* control block callbacks */
	void *data;			/* data pointer */
};

/* Netlink flags of the commands */
static const uint16_t cmdflags[] = {
	[IPSET_CMD_CREATE-1]	= NLM_F_REQUEST|NLM_F_ACK|
					NLM_F_CREATE|NLM_F_EXCL,
	[IPSET_CMD_DESTROY-1]	= NLM_F_REQUEST|NLM_F_ACK,
	[IPSET_CMD_FLUSH-1]	= NLM_F_REQUEST|NLM_F_ACK,
	[IPSET_CMD_RENAME-1]	= NLM_F_REQUEST|NLM_F_ACK,
	[IPSET_CMD_SWAP-1]	= NLM_F_REQUEST|NLM_F_ACK,
	[IPSET_CMD_LIST-1]	= NLM_F_REQUEST|NLM_F_ACK|NLM_F_DUMP,
	[IPSET_CMD_SAVE-1]	= NLM_F_REQUEST|NLM_F_ACK|NLM_F_DUMP,
	[IPSET_CMD_ADD-1]	= NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL,
	[IPSET_CMD_DEL-1]	= NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL,
	[IPSET_CMD_TEST-1]	= NLM_F_REQUEST|NLM_F_ACK,
	[IPSET_CMD_HEADER-1]	= NLM_F_REQUEST,
	[IPSET_CMD_TYPE-1]	= NLM_F_REQUEST,
	[IPSET_CMD_PROTOCOL-1]	= NLM_F_REQUEST,
};

/**
 * ipset_get_nlmsg_type - get ipset netlink message type
 * @nlh: pointer to the netlink message header
 *
 * Returns the ipset netlink message type, i.e. the ipset command.
 */
int
ipset_get_nlmsg_type(const struct nlmsghdr *nlh)
{
	return nlh->nlmsg_type & ~(NFNL_SUBSYS_IPSET << 8);
}

static void
ipset_mnl_fill_hdr(struct ipset_handle *handle, enum ipset_cmd cmd,
		   void *buffer, size_t len UNUSED, uint8_t envflags)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	assert(handle);
	assert(buffer);
	assert(cmd > IPSET_CMD_NONE && cmd < IPSET_MSG_MAX);

	nlh = mnl_nlmsg_put_header(buffer);
	nlh->nlmsg_type = cmd | (NFNL_SUBSYS_IPSET << 8);
	nlh->nlmsg_flags = cmdflags[cmd - 1];
	if (envflags & IPSET_ENV_EXIST)
		nlh->nlmsg_flags &=  ~NLM_F_EXCL;

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(struct nfgenmsg));
	nfg->nfgen_family = AF_INET;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(0);
}

static int
ipset_mnl_query(struct ipset_handle *handle, void *buffer, size_t len)
{
	struct nlmsghdr *nlh = buffer;
	int ret;

	assert(handle);
	assert(buffer);

	nlh->nlmsg_seq = ++handle->seq;
#ifdef IPSET_DEBUG
	ipset_debug_msg("sent", nlh, nlh->nlmsg_len);
#endif
	if (mnl_socket_sendto(handle->h, nlh, nlh->nlmsg_len) < 0)
		return -ECOMM;

	ret = mnl_socket_recvfrom(handle->h, buffer, len);
#ifdef IPSET_DEBUG
	ipset_debug_msg("received", buffer, ret);
#endif
	while (ret > 0) {
		ret = mnl_cb_run2(buffer, ret,
				  handle->seq, handle->portid,
				  handle->cb_ctl[NLMSG_MIN_TYPE],
				  handle->data,
				  handle->cb_ctl, NLMSG_MIN_TYPE);
		D("nfln_cb_run2, ret: %d, errno %d", ret, errno);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(handle->h, buffer, len);
		D("message received, ret: %d", ret);
	}
	return ret > 0 ? 0 : ret;
}

static struct ipset_handle *
ipset_mnl_init(mnl_cb_t *cb_ctl, void *data)
{
	struct ipset_handle *handle;

	assert(cb_ctl);
	assert(data);

	handle = calloc(1, sizeof(*handle));
	if (!handle)
		return NULL;

	handle->h = mnl_socket_open(NETLINK_NETFILTER);
	if (!handle->h)
		goto free_handle;

	if (mnl_socket_bind(handle->h, 0, MNL_SOCKET_AUTOPID) < 0)
		goto close_nl;

	handle->portid = mnl_socket_get_portid(handle->h);
	handle->cb_ctl = cb_ctl;
	handle->data = data;
	handle->seq = time(NULL);

	return handle;

close_nl:
	mnl_socket_close(handle->h);
free_handle:
	free(handle);

	return NULL;
}

static int
ipset_mnl_fini(struct ipset_handle *handle)
{
	assert(handle);

	if (handle->h)
		mnl_socket_close(handle->h);

	free(handle);
	return 0;
}

const struct ipset_transport ipset_mnl_transport = {
	.init	= ipset_mnl_init,
	.fini	= ipset_mnl_fini,
	.fill_hdr = ipset_mnl_fill_hdr,
	.query	= ipset_mnl_query,
};
