/*
 * rarpd.c	RARP daemon.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <syslog.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

int do_reload = 1;

int debug;
int verbose;
int ifidx;
int allow_offlink;
int only_ethers;
int all_ifaces;
int listen_arp;
char *ifname;
char *tftp_dir = "/etc/tftpboot";

extern int ether_ntohost(char *name, unsigned char *ea);
void usage(void) __attribute__((noreturn));

struct iflink
{
	struct iflink	*next;
	int	       	index;
	int		hatype;
	unsigned char	lladdr[16];
	char		name[IFNAMSIZ];
	struct ifaddr 	*ifa_list;
} *ifl_list;

struct ifaddr
{
	struct ifaddr 	*next;
	__u32		prefix;
	__u32		mask;
	__u32		local;
};

struct rarp_map
{
	struct rarp_map *next;

	int		ifindex;
	int		arp_type;
	int		lladdr_len;
	unsigned char	lladdr[16];
	__u32		ipaddr;
} *rarp_db;

void usage()
{
	fprintf(stderr, "Usage: rarpd [ -dveaA ] [ -b tftpdir ] [ interface]\n");
	exit(1);
}

void load_db(void)
{
}

void load_if(void)
{
	int fd;
	struct ifreq *ifrp, *ifend;
	struct iflink *ifl;
	struct ifaddr *ifa;
	struct ifconf ifc;
	struct ifreq ibuf[256];

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		return;
	}

	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < (int)sizeof(struct ifreq)) {
		syslog(LOG_ERR, "SIOCGIFCONF: %m");
		close(fd);
		return;
	}

	while ((ifl = ifl_list) != NULL) {
		while ((ifa = ifl->ifa_list) != NULL) {
			ifl->ifa_list = ifa->next;
			free(ifa);
		}
		ifl_list = ifl->next;
		free(ifl);
	}

	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);
	for (ifrp = ibuf; ifrp < ifend; ifrp++) {
		__u32 addr;
		__u32 mask;
		__u32 prefix;

		if (ifrp->ifr_addr.sa_family != AF_INET)
			continue;
		addr = ((struct sockaddr_in*)&ifrp->ifr_addr)->sin_addr.s_addr;
		if (addr == 0)
			continue;
		if (ioctl(fd, SIOCGIFINDEX, ifrp)) {
			syslog(LOG_ERR, "ioctl(SIOCGIFNAME): %m");
			continue;
		}
		if (ifidx && ifrp->ifr_ifindex != ifidx)
			continue;
		for (ifl = ifl_list; ifl; ifl = ifl->next)
			if (ifl->index == ifrp->ifr_ifindex)
				break;
		if (ifl == NULL) {
			char *p;
			int index = ifrp->ifr_ifindex;

			if (ioctl(fd, SIOCGIFHWADDR, ifrp)) {
				syslog(LOG_ERR, "ioctl(SIOCGIFHWADDR): %m");
				continue;
			}

			ifl = (struct iflink*)malloc(sizeof(*ifl));
			if (ifl == NULL)
				continue;
			memset(ifl, 0, sizeof(*ifl));
			ifl->next = ifl_list;
			ifl_list = ifl;
			ifl->index = index;
			ifl->hatype = ifrp->ifr_hwaddr.sa_family;
			memcpy(ifl->lladdr, ifrp->ifr_hwaddr.sa_data, 14);
			strncpy(ifl->name, ifrp->ifr_name, IFNAMSIZ);
			p = strchr(ifl->name, ':');
			if (p)
				*p = 0;
			if (verbose)
				syslog(LOG_INFO, "link %s", ifl->name);
		}
		if (ioctl(fd, SIOCGIFNETMASK, ifrp)) {
			syslog(LOG_ERR, "ioctl(SIOCGIFMASK): %m");
			continue;
		}
		mask = ((struct sockaddr_in*)&ifrp->ifr_netmask)->sin_addr.s_addr;
		if (ioctl(fd, SIOCGIFDSTADDR, ifrp)) {
			syslog(LOG_ERR, "ioctl(SIOCGIFDSTADDR): %m");
			continue;
		}
		prefix = ((struct sockaddr_in*)&ifrp->ifr_dstaddr)->sin_addr.s_addr;
		for (ifa = ifl->ifa_list; ifa; ifa = ifa->next) {
			if (ifa->local == addr &&
			    ifa->prefix == prefix &&
			    ifa->mask == mask)
				break;
		}
		if (ifa == NULL) {
			if (mask == 0 || prefix == 0)
				continue;
			ifa = (struct ifaddr*)malloc(sizeof(*ifa));
			memset(ifa, 0, sizeof(*ifa));
			ifa->local = addr;
			ifa->prefix = prefix;
			ifa->mask = mask;
			ifa->next = ifl->ifa_list;
			ifl->ifa_list = ifa;

			if (verbose) {
				int i;
				__u32 m = ~0U;
				for (i=32; i>=0; i--) {
					if (htonl(m) == mask)
						break;
					m <<= 1;
				}
				if (addr == prefix) {
					syslog(LOG_INFO, "  addr %s/%d on %s\n",
					       inet_ntoa(*(struct in_addr*)&addr), i, ifl->name);
				} else {
					char tmpa[64];
					sprintf(tmpa, "%s", inet_ntoa(*(struct in_addr*)&addr));
					syslog(LOG_INFO, "  addr %s %s/%d on %s\n", tmpa,
					       inet_ntoa(*(struct in_addr*)&prefix), i, ifl->name);
				}
			}
		}
	}
}

void configure(void)
{
	load_if();
	load_db();
}

int bootable(__u32 addr)
{
	struct dirent *dent;
	DIR *d;
	char name[9];

	sprintf(name, "%08X", (__u32)ntohl(addr));
	d = opendir(tftp_dir);
	if (d == NULL) {
		syslog(LOG_ERR, "opendir: %m");
		return 0;
	}
	while ((dent = readdir(d)) != NULL) {
		if (strncmp(dent->d_name, name, 8) == 0)
			break;
	}
	closedir(d);
	return dent != NULL;
}

struct ifaddr *select_ipaddr(int ifindex, __u32 *sel_addr, __u32 **alist)
{
	struct iflink *ifl;
	struct ifaddr *ifa;
	int retry = 0;
	int i;

retry:
	for (ifl=ifl_list; ifl; ifl=ifl->next)
		if (ifl->index == ifindex)
			break;
	if (ifl == NULL && !retry) {
		retry++;
		load_if();
		goto retry;
	}
	if (ifl == NULL)
		return NULL;

	for (i=0; alist[i]; i++) {
		__u32 addr = *(alist[i]);
		for (ifa=ifl->ifa_list; ifa; ifa=ifa->next) {
			if (!((ifa->prefix^addr)&ifa->mask)) {
				*sel_addr = addr;
				return ifa;
			}
		}
		if (ifa == NULL && retry==0) {
			retry++;
			load_if();
			goto retry;
		}
	}
	if (i==1 && allow_offlink) {
		*sel_addr = *(alist[0]);
		return ifl->ifa_list;
	}
	syslog(LOG_ERR, "Off-link request on %s", ifl->name);
	return NULL;
}

struct rarp_map *rarp_lookup(int ifindex, int hatype,
			     int halen, unsigned char *lladdr)
{
	struct rarp_map *r;

	for (r=rarp_db; r; r=r->next) {
		if (r->arp_type != hatype && r->arp_type != -1)
			continue;
		if (r->lladdr_len != halen)
			continue;
		if (r->ifindex != ifindex && r->ifindex != 0)
			continue;
		if (memcmp(r->lladdr, lladdr, halen) == 0)
			break;
	}

	if (r == NULL) {
		if (hatype == ARPHRD_ETHER && halen == 6) {
			struct ifaddr *ifa;
			struct hostent *hp;
			char ename[256];
			static struct rarp_map emap = {
				NULL,
				0,
				ARPHRD_ETHER,
				6,
			};

			if (ether_ntohost(ename, lladdr) != 0 ||
			    (hp = gethostbyname(ename)) == NULL) {
				if (verbose)
					syslog(LOG_INFO, "not found in /etc/ethers");
				return NULL;
			}
			if (hp->h_addrtype != AF_INET) {
				syslog(LOG_ERR, "no IP address");
				return NULL;
			}
			ifa = select_ipaddr(ifindex, &emap.ipaddr, (__u32 **)hp->h_addr_list);
			if (ifa) {
				memcpy(emap.lladdr, lladdr, 6);
				if (only_ethers || bootable(emap.ipaddr))
					return &emap;
				if (verbose)
					syslog(LOG_INFO, "not bootable");
			}
		}
	}
	return r;
}

static int load_arp_bpflet(int fd)
{
	static struct sock_filter insns[] = {
		BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 6),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ARPOP_RREQUEST, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, 1024),
		BPF_STMT(BPF_RET|BPF_K, 0),
	};
	static struct sock_fprog filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};

	return setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
}

int put_mylladdr(unsigned char **ptr_p, int ifindex, int alen)
{
	struct iflink *ifl;

	for (ifl=ifl_list; ifl; ifl = ifl->next)
		if (ifl->index == ifindex)
			break;

	if (ifl==NULL)
		return -1;

	memcpy(*ptr_p, ifl->lladdr, alen);
	*ptr_p += alen;
	return 0;
}

int put_myipaddr(unsigned char **ptr_p, int ifindex, __u32 hisipaddr)
{
	__u32 laddr = 0;
	struct iflink *ifl;
	struct ifaddr *ifa;

	for (ifl=ifl_list; ifl; ifl = ifl->next)
		if (ifl->index == ifindex)
			break;

	if (ifl==NULL)
		return -1;

	for (ifa=ifl->ifa_list; ifa; ifa=ifa->next) {
		if (!((ifa->prefix^hisipaddr)&ifa->mask)) {
			laddr = ifa->local;
			break;
		}
	}
	memcpy(*ptr_p, &laddr, 4);
	*ptr_p += 4;
	return 0;
}

void arp_advise(int ifindex, unsigned char *lladdr, int lllen, __u32 ipaddr)
{
	int fd;
	struct arpreq req;
	struct sockaddr_in *sin;
	struct iflink *ifl;

	for (ifl=ifl_list; ifl; ifl = ifl->next)
		if (ifl->index == ifindex)
			break;

	if (ifl == NULL)
		return;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&req, 0, sizeof(req));
	req.arp_flags = ATF_COM;
	sin = (struct sockaddr_in *)&req.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	req.arp_ha.sa_family = ifl->hatype;
	memcpy(req.arp_ha.sa_data, lladdr, lllen);
	memcpy(req.arp_dev, ifl->name, IFNAMSIZ);

	if (ioctl(fd, SIOCSARP, &req))
		syslog(LOG_ERR, "SIOCSARP: %m");
	close(fd);
}

void serve_it(int fd)
{
	unsigned char buf[1024];
	struct sockaddr_ll sll;
	socklen_t sll_len = sizeof(sll);
	struct arphdr *a = (struct arphdr*)buf;
	struct rarp_map *rmap;
	unsigned char *ptr;
	int n;

	n = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&sll, &sll_len);
	if (n<0) {
		if (errno != EINTR && errno != EAGAIN)
			syslog(LOG_ERR, "recvfrom: %m");
		return;
	}

	/* Do not accept packets for other hosts and our own ones */
	if (sll.sll_pkttype != PACKET_BROADCAST &&
	    sll.sll_pkttype != PACKET_MULTICAST &&
	    sll.sll_pkttype != PACKET_HOST)
		return;

	if (ifidx && sll.sll_ifindex != ifidx)
		return;

	if (n<sizeof(*a)) {
		syslog(LOG_ERR, "truncated arp packet; len=%d", n);
		return;
	}

	/* Accept only RARP requests */
	if (a->ar_op != htons(ARPOP_RREQUEST))
		return;

	if (verbose) {
		int i;
		char tmpbuf[16*3];
		char *ptr = tmpbuf;
		for (i=0; i<sll.sll_halen; i++) {
			if (i) {
				sprintf(ptr, ":%02x", sll.sll_addr[i]);
				ptr++;
			} else
				sprintf(ptr, "%02x", sll.sll_addr[i]);
			ptr += 2;
		}
		syslog(LOG_INFO, "RARP request from %s on if%d", tmpbuf, sll.sll_ifindex);
	}

	/* Sanity checks */

	/* 1. IP only -> pln==4 */
	if (a->ar_pln != 4) {
		syslog(LOG_ERR, "interesting rarp_req plen=%d", a->ar_pln);
		return;
	}
	/* 2. ARP protocol must be IP */
	if (a->ar_pro != htons(ETH_P_IP)) {
		syslog(LOG_ERR, "rarp protocol is not IP %04x", ntohs(a->ar_pro));
		return;
	}
	/* 3. ARP types must match */
	if (htons(sll.sll_hatype) != a->ar_hrd) {
		switch (sll.sll_hatype) {
		case ARPHRD_FDDI:
			if (a->ar_hrd == htons(ARPHRD_ETHER) ||
			    a->ar_hrd == htons(ARPHRD_IEEE802))
				break;
		default:
			syslog(LOG_ERR, "rarp htype mismatch");
			return;
		}
	}
	/* 3. LL address lengths must be equal */
	if (a->ar_hln != sll.sll_halen) {
		syslog(LOG_ERR, "rarp hlen mismatch");
		return;
	}
	/* 4. Check packet length */
	if (sizeof(*a) + 2*4 + 2*a->ar_hln > n) {
		syslog(LOG_ERR, "truncated rarp request; len=%d", n);
		return;
	}
	/* 5. Silly check: if this guy set different source
	      addresses in MAC header and in ARP, he is insane
	 */
	if (memcmp(sll.sll_addr, a+1, sll.sll_halen)) {
		syslog(LOG_ERR, "this guy set different his lladdrs in arp and header");
		return;
	}
	/* End of sanity checks */

	/* Lookup requested target in our database */
	rmap = rarp_lookup(sll.sll_ifindex, sll.sll_hatype,
			   sll.sll_halen, (unsigned char*)(a+1) + sll.sll_halen + 4);
	if (rmap == NULL)
		return;

	/* Prepare reply. It is almost ready, we only
	   replace ARP packet type, put our lladdr and
	   IP address to source fileds,
	   and fill target IP address.
	 */
	a->ar_op = htons(ARPOP_RREPLY);
	ptr = (unsigned char*)(a+1);
	if (put_mylladdr(&ptr, sll.sll_ifindex, rmap->lladdr_len))
		return;
	if (put_myipaddr(&ptr, sll.sll_ifindex, rmap->ipaddr))
		return;
	/* It is already filled */
	ptr += rmap->lladdr_len;
	memcpy(ptr, &rmap->ipaddr, 4);
	ptr += 4;

	/* Update our ARP cache. Probably, this guy
	   will not able to make ARP (if it is broken)
	 */
	arp_advise(sll.sll_ifindex, rmap->lladdr, rmap->lladdr_len, rmap->ipaddr);

	/* Sendto is blocking, but with 5sec timeout */
	alarm(5);
	sendto(fd, buf, ptr - buf, 0, (struct sockaddr*)&sll, sizeof(sll));
	alarm(0);
}

void catch_signal(int sig, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
#ifdef SA_INTERRUPT
	sa.sa_flags = SA_INTERRUPT;
#endif
	sigaction(sig, &sa, NULL);
}

void sig_alarm(int signo)
{
}

void sig_hup(int signo)
{
	do_reload = 1;
}

int main(int argc, char **argv)
{
	struct pollfd pset[2];
	int psize;
	int opt;


	opterr = 0;
	while ((opt = getopt(argc, argv, "aAb:dvoe")) != EOF) {
		switch (opt) {
		case 'a':
			++all_ifaces;
			break;

		case 'A':
			++listen_arp;
			break;

		case 'd':
			++debug;
			break;

		case 'v':
			++verbose;
			break;

		case 'o':
			++allow_offlink;
			break;

		case 'e':
			++only_ethers;
			break;

		case 'b':
			tftp_dir = optarg;
			break;

		default:
			usage();
		}
	}
	if (argc > optind) {
		if (argc > optind+1)
			usage();
		ifname = argv[optind];
	}

	psize = 1;
	pset[0].fd = socket(PF_PACKET, SOCK_DGRAM, 0);

	if (ifname) {
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(pset[0].fd, SIOCGIFINDEX, &ifr)) {
			perror("ioctl(SIOCGIFINDEX)");
			usage();
		}
		ifidx = ifr.ifr_ifindex;
	}

	pset[1].fd = -1;
	if (listen_arp) {
		pset[1].fd = socket(PF_PACKET, SOCK_DGRAM, 0);
		if (pset[1].fd >= 0) {
			load_arp_bpflet(pset[1].fd);
			psize = 1;
		}
	}

	if (pset[1].fd >= 0) {
		struct sockaddr_ll sll;
		memset(&sll, 0, sizeof(sll));
		sll.sll_family = AF_PACKET;
		sll.sll_protocol = htons(ETH_P_ARP);
		sll.sll_ifindex = all_ifaces ? 0 : ifidx;
		if (bind(pset[1].fd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
			close(pset[1].fd);
			pset[1].fd = -1;
			psize = 1;
		}
	}
	if (pset[0].fd >= 0) {
		struct sockaddr_ll sll;
		memset(&sll, 0, sizeof(sll));
		sll.sll_family = AF_PACKET;
		sll.sll_protocol = htons(ETH_P_RARP);
		sll.sll_ifindex = all_ifaces ? 0 : ifidx;
		if (bind(pset[0].fd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
			close(pset[0].fd);
			pset[0].fd = -1;
		}
	}
	if (pset[0].fd < 0) {
		pset[0] = pset[1];
		psize--;
	}
	if (psize == 0) {
		fprintf(stderr, "failed to bind any socket. Aborting.\n");
		exit(1);
	}

	if (!debug) {
		int fd;
		pid_t pid = fork();

		if (pid > 0)
			exit(0);
		else if (pid == -1) {
			perror("rarpd: fork");
			exit(1);
		}

		chdir("/");
		fd = open("/dev/null", O_RDWR);
		if (fd >= 0) {
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);
			if (fd > 2)
				close(fd);
		}
		setsid();
	}

	openlog("rarpd", LOG_PID | LOG_CONS, LOG_DAEMON);
	catch_signal(SIGALRM, sig_alarm);
	catch_signal(SIGHUP, sig_hup);

	for (;;) {
		int i;

		if (do_reload) {
			configure();
			do_reload = 0;
		}

#define EVENTS (POLLIN|POLLPRI|POLLERR|POLLHUP)
		pset[0].events = EVENTS;
		pset[0].revents = 0;
		pset[1].events = EVENTS;
		pset[1].revents = 0;

		i = poll(pset, psize, -1);
		if (i <= 0) {
			if (errno != EINTR && i<0) {
				syslog(LOG_ERR, "poll returned some crap: %m\n");
				sleep(10);
			}
			continue;
		}
		for (i=0; i<psize; i++) {
			if (pset[i].revents&EVENTS)
				serve_it(pset[i].fd);
		}
	}
}
