/*
 * iplink.c		"ip link".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"


static void usage(void) __attribute__((noreturn));

void iplink_usage(void)
{
	fprintf(stderr, "Usage: ip link set DEVICE { up | down |\n");
	fprintf(stderr, "	                     arp { on | off } |\n");
	fprintf(stderr, "	                     dynamic { on | off } |\n");
	fprintf(stderr, "	                     multicast { on | off } |\n");
	fprintf(stderr, "	                     allmulticast { on | off } |\n");
	fprintf(stderr, "	                     promisc { on | off } |\n");
	fprintf(stderr, "	                     trailers { on | off } |\n");
	fprintf(stderr, "	                     txqueuelen PACKETS |\n");
	fprintf(stderr, "	                     name NEWNAME |\n");
	fprintf(stderr, "	                     address LLADDR | broadcast LLADDR |\n");
	fprintf(stderr, "	                     mtu MTU }\n");
	fprintf(stderr, "       ip link show [ DEVICE ]\n");
	exit(-1);
}

static void usage(void)
{
	iplink_usage();
}

static int on_off(char *msg)
{
	fprintf(stderr, "Error: argument of \"%s\" must be \"on\" or \"off\"\n", msg);
	return -1;
}

static int get_ctl_fd(void)
{
	int s_errno;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	s_errno = errno;
	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	fd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	errno = s_errno;
	perror("Cannot create control socket");
	return -1;
}

static int do_chflags(const char *dev, __u32 flags, __u32 mask)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	fd = get_ctl_fd();
	if (fd < 0)
		return -1;
	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		perror("SIOCGIFFLAGS");
		close(fd);
		return -1;
	}
	if ((ifr.ifr_flags^flags)&mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask&flags;
		err = ioctl(fd, SIOCSIFFLAGS, &ifr);
		if (err)
			perror("SIOCSIFFLAGS");
	}
	close(fd);
	return err;
}

static int do_changename(const char *dev, const char *newdev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	strncpy(ifr.ifr_newname, newdev, IFNAMSIZ);
	fd = get_ctl_fd();
	if (fd < 0)
		return -1;
	err = ioctl(fd, SIOCSIFNAME, &ifr);
	if (err) {
		perror("SIOCSIFNAME");
		close(fd);
		return -1;
	}
	close(fd);
	return err;
}

static int set_qlen(const char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ); 
	ifr.ifr_qlen = qlen; 
	if (ioctl(s, SIOCSIFTXQLEN, &ifr) < 0) {
		perror("SIOCSIFXQLEN");
		close(s);
		return -1;
	}
	close(s);

	return 0; 
}

static int set_mtu(const char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ); 
	ifr.ifr_mtu = mtu; 
	if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
		perror("SIOCSIFMTU");
		close(s);
		return -1;
	}
	close(s);

	return 0; 
}

static int get_address(const char *dev, int *htype)
{
	struct ifreq ifr;
	struct sockaddr_ll me;
	socklen_t alen;
	int s;

	s = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (s < 0) { 
		perror("socket(PF_PACKET)");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		close(s);
		return -1;
	}

	memset(&me, 0, sizeof(me));
	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifr.ifr_ifindex;
	me.sll_protocol = htons(ETH_P_LOOP);
	if (bind(s, (struct sockaddr*)&me, sizeof(me)) == -1) {
		perror("bind");
		close(s);
		return -1;
	}

	alen = sizeof(me);
	if (getsockname(s, (struct sockaddr*)&me, &alen) == -1) {
		perror("getsockname");
		close(s);
		return -1;
	}
	close(s);
	*htype = me.sll_hatype;
	return me.sll_halen;
}

static int parse_address(const char *dev, int hatype, int halen, 
		char *lla, struct ifreq *ifr)
{
	int alen;

	memset(ifr, 0, sizeof(*ifr));
	strncpy(ifr->ifr_name, dev, IFNAMSIZ);
	ifr->ifr_hwaddr.sa_family = hatype;
	alen = ll_addr_a2n(ifr->ifr_hwaddr.sa_data, 14, lla);
	if (alen < 0)
		return -1;
	if (alen != halen) {
		fprintf(stderr, "Wrong address (%s) length: expected %d bytes\n", lla, halen);
		return -1;
	}
	return 0; 
}

static int set_address(struct ifreq *ifr, int brd)
{
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;
	if (ioctl(s, brd?SIOCSIFHWBROADCAST:SIOCSIFHWADDR, ifr) < 0) {
		perror(brd?"SIOCSIFHWBROADCAST":"SIOCSIFHWADDR");
		close(s);
		return -1;
	}
	close(s);
	return 0; 
}


static int do_set(int argc, char **argv)
{
	char *dev = NULL;
	__u32 mask = 0;
	__u32 flags = 0;
	int qlen = -1;
	int mtu = -1;
	char *newaddr = NULL;
	char *newbrd = NULL;
	struct ifreq ifr0, ifr1;
	char *newname = NULL;
	int htype, halen;

	while (argc > 0) {
		if (strcmp(*argv, "up") == 0) {
			mask |= IFF_UP;
			flags |= IFF_UP;
		} else if (strcmp(*argv, "down") == 0) {
			mask |= IFF_UP;
			flags &= ~IFF_UP;
		} else if (strcmp(*argv, "name") == 0) {
			NEXT_ARG();
			newname = *argv;
		} else if (matches(*argv, "address") == 0) {
			NEXT_ARG();
			newaddr = *argv;
		} else if (matches(*argv, "broadcast") == 0 ||
			   strcmp(*argv, "brd") == 0) {
			NEXT_ARG();
			newbrd = *argv;
		} else if (matches(*argv, "txqueuelen") == 0 ||
			   strcmp(*argv, "qlen") == 0 ||
			   matches(*argv, "txqlen") == 0) {
			NEXT_ARG();
			if (qlen != -1)
				duparg("txqueuelen", *argv);
			if (get_integer(&qlen,  *argv, 0))
				invarg("Invalid \"txqueuelen\" value\n", *argv);
		} else if (strcmp(*argv, "mtu") == 0) {
			NEXT_ARG();
			if (mtu != -1)
				duparg("mtu", *argv);
			if (get_integer(&mtu, *argv, 0))
				invarg("Invalid \"mtu\" value\n", *argv);
		} else if (strcmp(*argv, "multicast") == 0) {
			NEXT_ARG();
			mask |= IFF_MULTICAST;
			if (strcmp(*argv, "on") == 0) {
				flags |= IFF_MULTICAST;
			} else if (strcmp(*argv, "off") == 0) {
				flags &= ~IFF_MULTICAST;
			} else
				return on_off("multicast");
		} else if (strcmp(*argv, "allmulticast") == 0) {
			NEXT_ARG();
			mask |= IFF_ALLMULTI;
			if (strcmp(*argv, "on") == 0) {
				flags |= IFF_ALLMULTI;
			} else if (strcmp(*argv, "off") == 0) {
				flags &= ~IFF_ALLMULTI;
			} else
				return on_off("allmulticast");
		} else if (strcmp(*argv, "promisc") == 0) {
			NEXT_ARG();
			mask |= IFF_PROMISC;
			if (strcmp(*argv, "on") == 0) {
				flags |= IFF_PROMISC;
			} else if (strcmp(*argv, "off") == 0) {
				flags &= ~IFF_PROMISC;
			} else
				return on_off("promisc");
		} else if (strcmp(*argv, "trailers") == 0) {
			NEXT_ARG();
			mask |= IFF_NOTRAILERS;
			if (strcmp(*argv, "off") == 0) {
				flags |= IFF_NOTRAILERS;
			} else if (strcmp(*argv, "on") == 0) {
				flags &= ~IFF_NOTRAILERS;
			} else
				return on_off("trailers");
		} else if (strcmp(*argv, "arp") == 0) {
			NEXT_ARG();
			mask |= IFF_NOARP;
			if (strcmp(*argv, "on") == 0) {
				flags &= ~IFF_NOARP;
			} else if (strcmp(*argv, "off") == 0) {
				flags |= IFF_NOARP;
			} else
				return on_off("noarp");
#ifdef IFF_DYNAMIC
		} else if (matches(*argv, "dynamic") == 0) {
			NEXT_ARG();
			mask |= IFF_DYNAMIC;
			if (strcmp(*argv, "on") == 0) {
				flags |= IFF_DYNAMIC;
			} else if (strcmp(*argv, "off") == 0) {
				flags &= ~IFF_DYNAMIC;
			} else
				return on_off("dynamic");
#endif
		} else {
                        if (strcmp(*argv, "dev") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (dev)
				duparg2("dev", *argv);
			dev = *argv;
		}
		argc--; argv++;
	}

	if (!dev) {
		fprintf(stderr, "Not enough of information: \"dev\" argument is required.\n");
		exit(-1);
	}

	if (newaddr || newbrd) {
		halen = get_address(dev, &htype);
		if (halen < 0)
			return -1;
		if (newaddr) {
			if (parse_address(dev, htype, halen, newaddr, &ifr0) < 0)
				return -1;
		}
		if (newbrd) {
			if (parse_address(dev, htype, halen, newbrd, &ifr1) < 0)
				return -1; 
		}
	}

	if (newname && strcmp(dev, newname)) {
		if (do_changename(dev, newname) < 0)
			return -1;
		dev = newname;
	}
	if (qlen != -1) { 
		if (set_qlen(dev, qlen) < 0)
			return -1; 
	}
	if (mtu != -1) { 
		if (set_mtu(dev, mtu) < 0)
			return -1; 
	}
	if (newaddr || newbrd) {
		if (newbrd) {
			if (set_address(&ifr1, 1) < 0)
				return -1; 
		}
		if (newaddr) {
			if (set_address(&ifr0, 0) < 0)
				return -1;
		}
	}
	if (mask)
		return do_chflags(dev, flags, mask);
	return 0;
}

int do_iplink(int argc, char **argv)
{
	if (argc > 0) {
		if (matches(*argv, "set") == 0)
			return do_set(argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return ipaddr_list_link(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return ipaddr_list_link(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip link help\".\n", *argv);
	exit(-1);
}
