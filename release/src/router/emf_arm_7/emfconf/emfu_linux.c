/*
 * EMFL Command Line Utility Linux specific code
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfu_linux.c 441817 2013-12-08 08:50:30Z $
 */

#include <stdio.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <errno.h>
#include <bcmnvram.h>

#include "emfu_linux.h"
#include <emf_cfg.h>

#define MAX_DATA_SIZE sizeof(emf_cfg_request_t)

int
emf_cfg_request_send(emf_cfg_request_t *buffer, int length)
{
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct msghdr msg;
	struct iovec iov;
	int    sock_fd, ret;

	if ((buffer == NULL) || (length > MAX_DATA_SIZE))
	{
		fprintf(stderr, "Invalid parameters %p %d\n", buffer, length);
		return (FAILURE);
	}

	/* Create a netlink socket */
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_EMFC);

	if (sock_fd < 0)
	{
		fprintf(stderr, "Netlink socket create failed\n");
		return (FAILURE);
	}

	/* Associate a local address with the opened socket */
	memset(&src_addr, 0, sizeof(struct sockaddr_nl));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;
	if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		perror("");
		close(sock_fd);
		return FAILURE;
	}

	/* Fill the destination address, pid of 0 indicates kernel */
	memset(&dest_addr, 0, sizeof(struct sockaddr_nl));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	/* Allocate memory for sending configuration request */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_DATA_SIZE));

	if (nlh == NULL)
	{
		fprintf(stderr, "Out of memory allocating cfg buffer\n");
		close(sock_fd);
		return (FAILURE);
	}

	/* Fill the netlink message header. The configuration request
	 * contains netlink header followed by data.
	 */
	nlh->nlmsg_len = NLMSG_SPACE(MAX_DATA_SIZE);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	/* Fill the data part */
	memcpy(NLMSG_DATA(nlh), buffer, length);
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Send request to kernel module */
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret < 0)
	{
		perror("sendmsg:");
		free(nlh);
		close(sock_fd);
		return (ret);
	}

	/* Wait for the response */
	memset(nlh, 0, NLMSG_SPACE(MAX_DATA_SIZE));
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret < 0)
	{
		perror("recvmsg:");
		free(nlh);
		close(sock_fd);
		return (ret);
	}

	/* Copy data to user buffer */
	memcpy(buffer, NLMSG_DATA(nlh), length);

	free(nlh);

	close(sock_fd);

	return (ret);
}
