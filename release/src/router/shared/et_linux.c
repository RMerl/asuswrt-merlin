/*
 * Wireless network adapter utilities (linux-specific)
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wl_linux.c 435840 2013-11-12 09:30:26Z $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include <shutils.h>
#include <etutils.h>

static int
et_check(int s, struct ifreq *ifr)
{
	struct ethtool_drvinfo info;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	ifr->ifr_data = (caddr_t)&info;
	if (ioctl(s, SIOCETHTOOL, (caddr_t)ifr) < 0) {
		/* print a good diagnostic if not superuser */
		if (errno == EPERM)
			perror("not permit");
		return (-1);
	}

	if (!strncmp(info.driver, "et", 2))
		return (0);
	else if (!strncmp(info.driver, "bcm57", 5))
		return (0);

	return (-1);
}

static void
et_find(int s, struct ifreq *ifr)
{
	char proc_net_dev[] = "/proc/net/dev";
	FILE *fp = NULL;
	char buf[512], *c, *name;

	ifr->ifr_name[0] = '\0';

	/* eat first two lines */
	if (!(fp = fopen(proc_net_dev, "r")) ||
	    !fgets(buf, sizeof(buf), fp) ||
	    !fgets(buf, sizeof(buf), fp))
		goto done;

	while (fgets(buf, sizeof(buf), fp)) {
		c = buf;
		while (isspace(*c))
			c++;
		if (!(name = strsep(&c, ":")))
			continue;
		if (!strncmp(name, "aux", 3))
			continue;
		strncpy(ifr->ifr_name, name, IFNAMSIZ);
		ifr->ifr_name[IFNAMSIZ-1] = '\0';
		if (et_check(s, ifr) == 0)
			break;
		ifr->ifr_name[0] = '\0';
	}

done:
	if (fp)
		fclose(fp);
}

int
et_iovar(char *name, int cmd, void *buf, int len, bool set)
{
	struct ifreq ifr;
	et_var_t var;
	int s, ret = 0;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	/* get interface name if need */
	if (name) {
		strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name)-1);
		ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
	}
	else
		et_find(s, &ifr);

	if (!*ifr.ifr_name) {
		perror("et interface not found");
		ret = -1;
		goto err;
	}

	/* do it */
	var.cmd = cmd;
	var.buf = buf;
	var.len = len;
	var.set = set;

	ifr.ifr_data = (caddr_t)&var;
	if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
		perror("ioctl");
		ret = errno;
	}

err:
	close(s);
	return ret;
}

#define ET_CAP_BUF_LEN (8 * 1024)

bool
et_capable(char *ifname, char *cap)
{
	char var[32], *next;
	char caps[ET_CAP_BUF_LEN];

	if (!cap)
		return FALSE;

	if (et_iovar(ifname, IOV_CAP, (void *)caps, sizeof(caps), FALSE))
		return FALSE;

	foreach(var, caps, next) {
		if (strncmp(var, cap, sizeof(var)) == 0)
			return TRUE;
	}

	return FALSE;
}
