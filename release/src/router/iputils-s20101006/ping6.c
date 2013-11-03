/*
 *
 *	Modified for AF_INET6 by Pedro Roque
 *
 *	<roque@di.fc.ul.pt>
 *
 *	Original copyright notice included bellow
 */

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 *			P I N G . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */
#include "ping_common.h"

#include <linux/filter.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <resolv.h>

#include "ping6_niquery.h"

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif

#ifndef SOL_ICMPV6
#define SOL_ICMPV6 IPPROTO_ICMPV6
#endif

/* RFC3542 */
#ifndef ICMP6_DST_UNREACH_BEYONDSCOPE
#define ICMP6_DST_UNREACH_BEYONDSCOPE ICMP6_DST_UNREACH_NOTNEIGHBOR
#endif

#ifndef IPV6_SRCRT_TYPE_0
#define IPV6_SRCRT_TYPE_0	0
#endif

#ifndef MLD_LISTENER_QUERY
#define MLD_LISTENER_QUERY	130
#define MLD_LISTENER_REPORT	131
#define MLD_LISTENER_REDUCTION	132
#endif

#define BIT_CLEAR(nr, addr) do { ((__u32 *)(addr))[(nr) >> 5] &= ~(1U << ((nr) & 31)); } while(0)
#define BIT_SET(nr, addr) do { ((__u32 *)(addr))[(nr) >> 5] |= (1U << ((nr) & 31)); } while(0)
#define BIT_TEST(nr, addr) do { (__u32 *)(addr))[(nr) >> 5] & (1U << ((nr) & 31)); } while(0)

#ifndef ICMP6_FILTER_WILLPASS
#define ICMP6_FILTER_WILLPASS(type, filterp) \
	(BIT_TEST((type), filterp) == 0)

#define ICMP6_FILTER_WILLBLOCK(type, filterp) \
	BIT_TEST((type), filterp)

#define ICMP6_FILTER_SETPASS(type, filterp) \
	BIT_CLEAR((type), filterp)

#define ICMP6_FILTER_SETBLOCK(type, filterp) \
	BIT_SET((type), filterp)

#define ICMP6_FILTER_SETPASSALL(filterp) \
	memset(filterp, 0, sizeof(struct icmp6_filter));

#define ICMP6_FILTER_SETBLOCKALL(filterp) \
	memset(filterp, 0xFF, sizeof(struct icmp6_filter));
#endif

#define	MAXPACKET	128000		/* max packet size */

#ifdef SO_TIMESTAMP
#define HAVE_SIN6_SCOPEID 1
#endif

#ifndef SCOPE_DELIMITER
# define SCOPE_DELIMITER '%'
#endif

__u32 flowlabel;
__u32 tclass;
struct cmsghdr *srcrt;

struct sockaddr_in6 whereto;	/* who to ping */
u_char outpack[MAXPACKET];
int maxpacket = sizeof(outpack);

static unsigned char cmsgbuf[4096];
static int cmsglen = 0;

static char * pr_addr(struct in6_addr *addr);
static char * pr_addr_n(struct in6_addr *addr);
static int pr_icmph(__u8 type, __u8 code, __u32 info);
static void usage(void) __attribute((noreturn));

struct sockaddr_in6 source;
char *device;
int pmtudisc=-1;

static int icmp_sock;

#include <openssl/md5.h>

/* Node Information query */
int ni_query = -1;
int ni_flag = 0;
void *ni_subject = NULL;
int ni_subject_len = 0;
int ni_subject_type = 0;
char *ni_group;

__u8 ni_nonce[8];

static struct in6_addr in6_anyaddr;
static __inline__ int ipv6_addr_any(struct in6_addr *addr)
{
	return (memcmp(addr, &in6_anyaddr, 16) == 0);
}

size_t inet6_srcrt_space(int type, int segments)
{
	if (type != 0 || segments > 24)
		return 0;

	return (sizeof(struct cmsghdr) + sizeof(struct ip6_rthdr0) +
		segments * sizeof(struct in6_addr));
}

extern struct cmsghdr *	inet6_srcrt_init(void *bp, int type)
{
	struct cmsghdr *cmsg;

	if (type)
		return NULL;

	memset(bp, 0, sizeof(struct cmsghdr) + sizeof(struct ip6_rthdr0));
	cmsg = (struct cmsghdr *) bp;

	cmsg->cmsg_len = sizeof(struct cmsghdr) + sizeof(struct ip6_rthdr0);
	cmsg->cmsg_level = SOL_IPV6;
	cmsg->cmsg_type = IPV6_RTHDR;

	return cmsg;
}

int inet6_srcrt_add(struct cmsghdr *cmsg, const struct in6_addr *addr)
{
	struct ip6_rthdr0 *hdr;

	hdr = (struct ip6_rthdr0 *) CMSG_DATA(cmsg);

	cmsg->cmsg_len += sizeof(struct in6_addr);
	hdr->ip6r0_len += sizeof(struct in6_addr) / 8;

	memcpy(&hdr->ip6r0_addr[hdr->ip6r0_segleft++], addr,
	       sizeof(struct in6_addr));

	return 0;
}

unsigned int if_name2index(const char *ifname)
{
	unsigned int i = if_nametoindex(ifname);
	if (!i) {
		fprintf(stderr, "ping: unknown iface %s\n", ifname);
		exit(2);
	}
	return i;
}

struct niquery_option {
	char *name;
	int namelen;
	int has_arg;
	int data;
	int (*handler)(int index, const char *arg);
};

#define NIQUERY_OPTION(_name, _has_arg, _data, _handler)	\
	{							\
		.name = _name,					\
		.namelen = sizeof(_name) - 1,			\
		.has_arg = _has_arg,				\
		.data = _data,					\
		.handler = _handler				\
	}

static int niquery_option_name_handler(int index, const char *arg);
static int niquery_option_ipv6_handler(int index, const char *arg);
static int niquery_option_ipv6_flag_handler(int index, const char *arg);
static int niquery_option_ipv4_handler(int index, const char *arg);
static int niquery_option_ipv4_flag_handler(int index, const char *arg);
static int niquery_option_subject_addr_handler(int index, const char *arg);
static int niquery_option_subject_name_handler(int index, const char *arg);
char *ni_groupaddr(const char *name);

struct niquery_option niquery_options[] = {
	NIQUERY_OPTION("name",			0,	0,				niquery_option_name_handler),
	NIQUERY_OPTION("fqdn",			0,	0,				niquery_option_name_handler),
	NIQUERY_OPTION("ipv6",			0,	0,				niquery_option_ipv6_handler),
	NIQUERY_OPTION("ipv6-all",		0,	NI_IPV6ADDR_F_ALL,		niquery_option_ipv6_flag_handler),
	NIQUERY_OPTION("ipv6-compatible",	0,	NI_IPV6ADDR_F_COMPAT,		niquery_option_ipv6_flag_handler),
	NIQUERY_OPTION("ipv6-linklocal",	0,	NI_IPV6ADDR_F_LINKLOCAL,	niquery_option_ipv6_flag_handler),
	NIQUERY_OPTION("ipv6-sitelocal",	0,	NI_IPV6ADDR_F_SITELOCAL,	niquery_option_ipv6_flag_handler),
	NIQUERY_OPTION("ipv6-global",		0,	NI_IPV6ADDR_F_GLOBAL,		niquery_option_ipv6_flag_handler),
	NIQUERY_OPTION("ipv4",			0,	0,				niquery_option_ipv4_handler),
	NIQUERY_OPTION("ipv4-all",		0,	NI_IPV4ADDR_F_ALL,		niquery_option_ipv4_flag_handler),
	NIQUERY_OPTION("subject-ipv6",		1,	NI_SUBJ_IPV6,			niquery_option_subject_addr_handler),
	NIQUERY_OPTION("subject-ipv4",		1,	NI_SUBJ_IPV4,			niquery_option_subject_addr_handler),
	NIQUERY_OPTION("subject-name",		1,	0,				niquery_option_subject_name_handler),
	NIQUERY_OPTION("subject-fqdn",		1,	-1,				niquery_option_subject_name_handler),
	{},
};

static int niquery_set_qtype(int type)
{
	if (ni_query >= 0 && ni_query != type) {
		printf("Qtype conflict\n");
		return -1;
	}
	ni_query = type;
	return 0;
}

static int niquery_option_name_handler(int index, const char *arg)
{
	if (niquery_set_qtype(NI_QTYPE_NAME) < 0)
		return -1;
	return 0;
}

static int niquery_option_ipv6_handler(int index, const char *arg)
{
	if (niquery_set_qtype(NI_QTYPE_IPV6ADDR) < 0)
		return -1;
	return 0;
}

static int niquery_option_ipv6_flag_handler(int index, const char *arg)
{
	if (niquery_set_qtype(NI_QTYPE_IPV6ADDR) < 0)
		return -1;
	ni_flag |= niquery_options[index].data;
	return 0;
}

static int niquery_option_ipv4_handler(int index, const char *arg)
{
	if (niquery_set_qtype(NI_QTYPE_IPV4ADDR) < 0)
		return -1;
	return 0;
}

static int niquery_option_ipv4_flag_handler(int index, const char *arg)
{
	if (niquery_set_qtype(NI_QTYPE_IPV4ADDR) < 0)
		return -1;
	ni_flag |= niquery_options[index].data;
	return 0;
}

static int niquery_set_subject_type(int type)
{
	if (ni_subject_type && ni_subject_type != type) {
		printf("Subject type conflict\n");
		return -1;
	}
	ni_subject_type = type;
	return 0;
}

#define ARRAY_SIZE(array)	(sizeof(array) / sizeof(array[0]))
#define OFFSET_OF(type,elem)	((size_t)&((type *)0)->elem)

static int niquery_option_subject_addr_handler(int index, const char *arg)
{
	struct addrinfo hints, *ai0, *ai;
	int offset;
	int gai;

	if (niquery_set_subject_type(niquery_options[index].data) < 0)
		return -1;

	ni_subject_type = niquery_options[index].data;

	switch (niquery_options[index].data) {
	case NI_SUBJ_IPV6:
		ni_subject_len = sizeof(struct in6_addr);
		offset = OFFSET_OF(struct sockaddr_in6, sin6_addr);
		hints.ai_family = AF_INET6;
		break;
	case NI_SUBJ_IPV4:
		ni_subject_len = sizeof(struct in_addr);
		offset = OFFSET_OF(struct sockaddr_in, sin_addr);
		hints.ai_family = AF_INET;
		break;
	default:
		/* should not happen. */
		offset = -1;
	}

	hints.ai_socktype = SOCK_DGRAM;

	gai = getaddrinfo(arg, 0, &hints, &ai0);
	if (!gai) {
		fprintf(stderr, "Unknown host: %s\n", arg);
		return -1;
	}

	for (ai = ai0; ai; ai = ai->ai_next) {
		void *p = malloc(ni_subject_len);
		if (!p)
			continue;
		memcpy(p, (__u8 *)ai->ai_addr + offset, ni_subject_len);
		free(ni_subject);
		ni_subject = p;
		break;
	}
	freeaddrinfo(ai0);

	return 0;
}

static int niquery_count_dots(const char *arg)
{
	const char *p;
	int count = 0;
	for (p = arg; *p; p++) {
		if (*p == '.')
			count++;
	}
	return count;
}

static int niquery_option_subject_name_handler(int index, const char *arg)
{
	unsigned char *dnptrs[2], **dpp, **lastdnptr;
	int n;
	char *name, *p;
	unsigned char *buf;
	size_t buflen = strlen(arg) + 1;
	int fqdn = niquery_options[index].data;

	if (niquery_set_subject_type(NI_SUBJ_NAME) < 0)
		return -1;

	if (fqdn == 0) {
		/* guess if hostname is FQDN */
		fqdn = niquery_count_dots(arg) ? 1 : -1;
	}

	name = strdup(arg);
	buf = malloc(buflen + 1);
	if (!name || !buf) {
		free(name);
		free(buf);
		fprintf(stderr, "ping6: out of memory.\n");
		exit(1);
	}

	ni_group = ni_groupaddr(name);

	p = strchr(name, '%');
	if (p)
		*p = '\0';

	dpp = dnptrs;
	lastdnptr = &dnptrs[ARRAY_SIZE(dnptrs)];

	*dpp++ = (unsigned char *)buf;
	*dpp++ = NULL;

	n = dn_comp(name, (unsigned char *)buf, buflen, dnptrs, lastdnptr);
	if (n < 0) {
		fprintf(stderr, "ping6: Inappropriate subject name: %s\n", buf);
		free(name);
		free(buf);
		exit(1);
	}

	if (fqdn < 0)
		buf[n] = 0;

	free(ni_subject);
	ni_subject = buf;
	ni_subject_len = n + (fqdn < 0);

	free(name);
	return 0;
}

int niquery_option_handler(const char *opt_arg)
{
	struct niquery_option *p;
	int i;
	int ret = -1;
	for (i = 0, p = niquery_options; p->name; i++, p++) {
		if (strncmp(p->name, opt_arg, p->namelen))
			continue;
		if (!p->has_arg) {
			if (opt_arg[p->namelen] == '\0') {
				ret = p->handler(i, NULL);
				if (ret >= 0)
					break;
			}
		} else {
			if (opt_arg[p->namelen] == '=') {
				ret = p->handler(i, &opt_arg[p->namelen] + 1);
				if (ret >= 0)
					break;
			}
		}
	}
	return ret;
}

char *ni_groupaddr(const char *name)
{
	MD5_CTX ctxt;
	__u8 digest[16];
	static char nigroup_buf[INET6_ADDRSTRLEN + 1 + IFNAMSIZ];
	size_t len;
	char buf[64], *p = buf, *q;
	int i;

	if (!p) {
		fprintf(stderr, "ping6: memory allocation failure.\n");
		exit(1);
	}

	len = strcspn(name, ".%");
	if (len & ~0x3f) {
		fprintf(stderr, "ping6: label too long for subject: %s\n",
			name);
		exit(1);
	}

	q = strrchr(name, '%');
	if (q && strlen(q + 1) >= IFNAMSIZ) {
		fprintf(stderr, "ping6: scope too long: %s\n",
			q + 1);
		exit(1);
	}

	*p++ = len;
	for (i = 0; i < len; i++)
		*p++ = isupper(name[i]) ? tolower(name[i]) : name[i];

	MD5_Init(&ctxt);
	MD5_Update(&ctxt, buf, len + 1);
	MD5_Final(digest, &ctxt);

	sprintf(nigroup_buf, "ff02::2:%02x%02x:%02x%02x",
		digest[0], digest[1], digest[2], digest[3]);

	if (q)
		strcat(nigroup_buf, q);
	return nigroup_buf;
}

int main(int argc, char *argv[])
{
	int ch, hold, packlen;
	u_char *packet;
	char *target;
	struct addrinfo hints, *ai;
	int gai;
	struct sockaddr_in6 firsthop;
	int socket_errno;
	struct icmp6_filter filter;
	int err;
#ifdef __linux__
	int csum_offset, sz_opt;
#endif
	static uint32_t scope_id = 0;

	icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	socket_errno = errno;

	uid = getuid();
	if (setuid(uid)) {
		perror("ping: setuid");
		exit(-1);
	}

	source.sin6_family = AF_INET6;
	memset(&firsthop, 0, sizeof(firsthop));
	firsthop.sin6_family = AF_INET6;

	preload = 1;
	while ((ch = getopt(argc, argv, COMMON_OPTSTR "F:N:")) != EOF) {
		switch(ch) {
		case 'F':
			sscanf(optarg, "%x", &flowlabel);
			options |= F_FLOWINFO;
			break;
		case 'Q':
			sscanf(optarg, "%x", &tclass);
			options |= F_TCLASS;
			break;
		case 'I':
			if (strchr(optarg, ':')) {
				char *p, *addr = strdup(optarg);

				if (!addr) {
					fprintf(stderr, "ping: out of memory\n");
					exit(2);
				}

				p = strchr(addr, SCOPE_DELIMITER);
				if (p) {
					*p = '\0';
					device = optarg + (p - addr) + 1;
				}

				if (inet_pton(AF_INET6, addr, (char*)&source.sin6_addr) <= 0) {
					fprintf(stderr, "ping: invalid source address %s\n", optarg);
					exit(2);
				}

				options |= F_STRICTSOURCE;

				free(addr);
			} else {
				device = optarg;
			}
			break;
		case 'M':
			if (strcmp(optarg, "do") == 0)
				pmtudisc = IPV6_PMTUDISC_DO;
			else if (strcmp(optarg, "dont") == 0)
				pmtudisc = IPV6_PMTUDISC_DONT;
			else if (strcmp(optarg, "want") == 0)
				pmtudisc = IPV6_PMTUDISC_WANT;
			else {
				fprintf(stderr, "ping: wrong value for -M: do, dont, want are valid ones.\n");
				exit(2);
			}
			break;
		case 'V':
			printf("ping6 utility, iputils-ss%s\n", SNAPSHOT);
			exit(0);
		case 'N':
			if (niquery_option_handler(optarg) < 0) {
				usage();
				break;
			}
			break;
		COMMON_OPTIONS
			common_options(ch);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	while (argc > 1) {
		struct in6_addr *addr;

		if (srcrt == NULL) {
			int space;

			space = inet6_srcrt_space(IPV6_SRCRT_TYPE_0, argc - 1);

			if (space == 0)	{
				fprintf(stderr, "srcrt_space failed\n");
				exit(2);
			}
			if (space + cmsglen > sizeof(cmsgbuf)) {
				fprintf(stderr, "no room for options\n");
				exit(2);
			}

			srcrt = (struct cmsghdr*)(cmsgbuf+cmsglen);
			cmsglen += CMSG_ALIGN(space);
			inet6_srcrt_init(srcrt, IPV6_SRCRT_TYPE_0);
		}

		target = *argv;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		gai = getaddrinfo(target, NULL, &hints, &ai);
		if (gai) {
			fprintf(stderr, "unknown host\n");
			exit(2);
		}
		addr = &((struct sockaddr_in6 *)(ai->ai_addr))->sin6_addr;
		inet6_srcrt_add(srcrt, addr);
		if (ipv6_addr_any(&firsthop.sin6_addr)) {
			memcpy(&firsthop.sin6_addr, addr, 16);
#ifdef HAVE_SIN6_SCOPEID
			firsthop.sin6_scope_id = ((struct sockaddr_in6 *)(ai->ai_addr))->sin6_scope_id;
			/* Verify scope_id is the same as previous nodes */
			if (firsthop.sin6_scope_id && scope_id && firsthop.sin6_scope_id != scope_id) {
				fprintf(stderr, "scope discrepancy among the nodes\n");
				exit(2);
			} else if (!scope_id) {
				scope_id = firsthop.sin6_scope_id;
			}
#endif
		}
		freeaddrinfo(ai);

		argv++;
		argc--;
	}

	if (ni_query >= 0) {
		int i;
		for (i = 0; i < 8; i++)
			ni_nonce[i] = rand();

		if (!ni_subject) {
			ni_subject = &whereto.sin6_addr;
			ni_subject_len = sizeof(whereto.sin6_addr);
			ni_subject_type = NI_SUBJ_IPV6;
		}
	}

	if (argc > 1)
		usage();
	else if (argc == 1) {
		target = *argv;
	} else {
		if (ni_query < 0 && ni_subject_type != NI_SUBJ_NAME)
			usage();
		target = ni_group;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai = getaddrinfo(target, NULL, &hints, &ai);
	if (gai) {
		fprintf(stderr, "unknown host\n");
		exit(2);
	}

	memcpy(&whereto, ai->ai_addr, sizeof(whereto));
	whereto.sin6_port = htons(IPPROTO_ICMPV6);

	if (memchr(target, ':', strlen(target)))
		options |= F_NUMERIC;

	freeaddrinfo(ai);

	if (ipv6_addr_any(&firsthop.sin6_addr)) {
		memcpy(&firsthop.sin6_addr, &whereto.sin6_addr, 16);
#ifdef HAVE_SIN6_SCOPEID
		firsthop.sin6_scope_id = whereto.sin6_scope_id;
		/* Verify scope_id is the same as intermediate nodes */
		if (firsthop.sin6_scope_id && scope_id && firsthop.sin6_scope_id != scope_id) {
			fprintf(stderr, "scope discrepancy among the nodes\n");
			exit(2);
		} else if (!scope_id) {
			scope_id = firsthop.sin6_scope_id;
		}
#endif
	}

	hostname = target;

	if (ipv6_addr_any(&source.sin6_addr)) {
		socklen_t alen;
		int probe_fd = socket(AF_INET6, SOCK_DGRAM, 0);

		if (probe_fd < 0) {
			perror("socket");
			exit(2);
		}
		if (device) {
#if defined(IPV6_RECVPKTINFO) || defined(HAVE_SIN6_SCOPEID)
			unsigned int iface = if_name2index(device);
#endif
#ifdef IPV6_RECVPKTINFO
			struct in6_pktinfo ipi;

			memset(&ipi, 0, sizeof(ipi));
			ipi.ipi6_ifindex = iface;
#endif

#ifdef HAVE_SIN6_SCOPEID
			if (IN6_IS_ADDR_LINKLOCAL(&firsthop.sin6_addr) ||
			    IN6_IS_ADDR_MC_LINKLOCAL(&firsthop.sin6_addr))
				firsthop.sin6_scope_id = iface;
#endif
			if (
#ifdef IPV6_RECVPKTINFO
			    setsockopt(probe_fd, IPPROTO_IPV6, IPV6_PKTINFO, &ipi, sizeof(ipi)) == -1 &&
#endif
			    setsockopt(probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device)+1) == -1) {
				perror("setsockopt(SO_BINDTODEVICE)");
			}
		}
		firsthop.sin6_port = htons(1025);
		if (connect(probe_fd, (struct sockaddr*)&firsthop, sizeof(firsthop)) == -1) {
			perror("connect");
			exit(2);
		}
		alen = sizeof(source);
		if (getsockname(probe_fd, (struct sockaddr*)&source, &alen) == -1) {
			perror("getsockname");
			exit(2);
		}
		source.sin6_port = 0;
		close(probe_fd);
	}
#ifdef HAVE_SIN6_SCOPEID
	else if (device && (IN6_IS_ADDR_LINKLOCAL(&source.sin6_addr) ||
			    IN6_IS_ADDR_MC_LINKLOCAL(&source.sin6_addr)))
		source.sin6_scope_id = if_name2index(device);
#endif

	if (icmp_sock < 0) {
		errno = socket_errno;
		perror("ping: icmp open socket");
		exit(2);
	}

	if (device) {
		struct cmsghdr *cmsg;
		struct in6_pktinfo *ipi;

		cmsg = (struct cmsghdr*)(cmsgbuf+cmsglen);
		cmsglen += CMSG_SPACE(sizeof(*ipi));
		cmsg->cmsg_len = CMSG_LEN(sizeof(*ipi));
		cmsg->cmsg_level = SOL_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;

		ipi = (struct in6_pktinfo*)CMSG_DATA(cmsg);
		memset(ipi, 0, sizeof(*ipi));
		ipi->ipi6_ifindex = if_name2index(device);
	}

	if ((whereto.sin6_addr.s6_addr16[0]&htons(0xff00)) == htons (0xff00)) {
		if (uid) {
			if (interval < 1000) {
				fprintf(stderr, "ping: multicast ping with too short interval.\n");
				exit(2);
			}
			if (pmtudisc >= 0 && pmtudisc != IPV6_PMTUDISC_DO) {
				fprintf(stderr, "ping: multicast ping does not fragment.\n");
				exit(2);
			}
		}
		if (pmtudisc < 0)
			pmtudisc = IPV6_PMTUDISC_DO;
	}

	if (pmtudisc >= 0) {
		if (setsockopt(icmp_sock, SOL_IPV6, IPV6_MTU_DISCOVER, &pmtudisc, sizeof(pmtudisc)) == -1) {
			perror("ping: IPV6_MTU_DISCOVER");
			exit(2);
		}
	}

	if ((options&F_STRICTSOURCE) &&
	    bind(icmp_sock, (struct sockaddr*)&source, sizeof(source)) == -1) {
		perror("ping: bind icmp socket");
		exit(2);
	}

	if (datalen >= sizeof(struct timeval) && (ni_query < 0)) {
		/* can we time transfer */
		timing = 1;
	}
	packlen = datalen + 8 + 4096 + 40 + 8; /* 4096 for rthdr */
	if (!(packet = (u_char *)malloc((u_int)packlen))) {
		fprintf(stderr, "ping: out of memory.\n");
		exit(2);
	}

	working_recverr = 1;
	hold = 1;
	if (setsockopt(icmp_sock, SOL_IPV6, IPV6_RECVERR, (char *)&hold, sizeof(hold))) {
		fprintf(stderr, "WARNING: your kernel is veeery old. No problems.\n");
		working_recverr = 0;
	}

	/* Estimate memory eaten by single packet. It is rough estimate.
	 * Actually, for small datalen's it depends on kernel side a lot. */
	hold = datalen+8;
	hold += ((hold+511)/512)*(40+16+64+160);
	sock_setbufs(icmp_sock, hold);

#ifdef __linux__
	csum_offset = 2;
	sz_opt = sizeof(int);

	err = setsockopt(icmp_sock, SOL_RAW, IPV6_CHECKSUM, &csum_offset, sz_opt);
	if (err < 0) {
		/* checksum should be enabled by default and setting this
		 * option might fail anyway.
		 */
		fprintf(stderr, "setsockopt(RAW_CHECKSUM) failed - try to continue.");
	}
#endif

	/*
	 *	select icmp echo reply as icmp type to receive
	 */

	ICMP6_FILTER_SETBLOCKALL(&filter);

	if (!working_recverr) {
		ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
		ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
		ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
		ICMP6_FILTER_SETPASS(ICMP6_PARAM_PROB, &filter);
	}

	if (ni_query >= 0)
		ICMP6_FILTER_SETPASS(ICMPV6_NI_REPLY, &filter);
	else
		ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);

	err = setsockopt(icmp_sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
			 sizeof(struct icmp6_filter));

	if (err < 0) {
		perror("setsockopt(ICMP6_FILTER)");
		exit(2);
	}

	if (options & F_NOLOOP) {
		int loop = 0;
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
							&loop, sizeof(loop)) == -1) {
			perror ("can't disable multicast loopback");
			exit(2);
		}
	}
	if (options & F_TTL) {
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			       &ttl, sizeof(ttl)) == -1) {
			perror ("can't set multicast hop limit");
			exit(2);
		}
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			       &ttl, sizeof(ttl)) == -1) {
			perror ("can't set unicast hop limit");
			exit(2);
		}
	}

	if (1) {
		int on = 1;
		if (
#ifdef IPV6_RECVHOPLIMIT
		    setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
			       &on, sizeof(on)) == -1 &&
		    setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_2292HOPLIMIT,
			       &on, sizeof(on)) == -1
#else
		    setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_HOPLIMIT,
			       &on, sizeof(on)) == -1
#endif
		   ){
			perror ("can't receive hop limit");
			exit(2);
		}
	}

	if (options & F_TCLASS) {
#ifdef IPV6_TCLASS
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_TCLASS,
			       &tclass, sizeof(tclass)) == -1) {
			perror ("setsockopt(IPV6_TCLASS)");
			exit(2);
		}
#else
		fprintf(stderr, "Traffic class is not supported.\n");
#endif
	}

	if (options&F_FLOWINFO) {
#ifdef IPV6_FLOWINFO_SEND
		int on = 1;
#endif
#ifdef IPV6_FLOWLABEL_MGR
		char freq_buf[CMSG_ALIGN(sizeof(struct in6_flowlabel_req)) + cmsglen];
		struct in6_flowlabel_req *freq = (struct in6_flowlabel_req *)freq_buf;
		int freq_len = sizeof(*freq);
		if (srcrt)
			freq_len = CMSG_ALIGN(sizeof(*freq)) + srcrt->cmsg_len;
		memset(freq, 0, sizeof(*freq));
		freq->flr_label = htonl(flowlabel&0xFFFFF);
		freq->flr_action = IPV6_FL_A_GET;
		freq->flr_flags = IPV6_FL_F_CREATE;
		freq->flr_share = IPV6_FL_S_EXCL;
		memcpy(&freq->flr_dst, &whereto.sin6_addr, 16);
		if (srcrt)
			memcpy(freq_buf + CMSG_ALIGN(sizeof(*freq)), srcrt, srcrt->cmsg_len);
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_FLOWLABEL_MGR,
			       freq, freq_len) == -1) {
			perror ("can't set flowlabel");
			exit(2);
		}
		flowlabel = freq->flr_label;
		if (srcrt) {
			cmsglen = (char*)srcrt - (char*)cmsgbuf;
			srcrt = NULL;
		}
#else
		fprintf(stderr, "Flow labels are not supported.\n");
		exit(2);
#endif

#ifdef IPV6_FLOWINFO_SEND
		whereto.sin6_flowinfo = flowlabel;
		if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_FLOWINFO_SEND,
			       &on, sizeof(on)) == -1) {
			perror ("can't send flowinfo");
			exit(2);
		}
#else
		fprintf(stderr, "Flowinfo is not supported.\n");
		exit(2);
#endif
	}

	printf("PING %s(%s) ", hostname, pr_addr(&whereto.sin6_addr));
	if (flowlabel)
		printf(", flow 0x%05x, ", (unsigned)ntohl(flowlabel));
	if (device || (options&F_STRICTSOURCE)) {
		printf("from %s %s: ",
		       pr_addr_n(&source.sin6_addr), device ? : "");
	}
	printf("%d data bytes\n", datalen);

	setup(icmp_sock);

	main_loop(icmp_sock, packet, packlen);
}

int receive_error_msg()
{
	int res;
	char cbuf[512];
	struct iovec  iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct sock_extended_err *e;
	struct icmp6_hdr icmph;
	struct sockaddr_in6 target;
	int net_errors = 0;
	int local_errors = 0;
	int saved_errno = errno;

	iov.iov_base = &icmph;
	iov.iov_len = sizeof(icmph);
	msg.msg_name = (void*)&target;
	msg.msg_namelen = sizeof(target);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	res = recvmsg(icmp_sock, &msg, MSG_ERRQUEUE|MSG_DONTWAIT);
	if (res < 0)
		goto out;

	e = NULL;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_IPV6) {
			if (cmsg->cmsg_type == IPV6_RECVERR)
				e = (struct sock_extended_err *)CMSG_DATA(cmsg);
		}
	}
	if (e == NULL)
		abort();

	if (e->ee_origin == SO_EE_ORIGIN_LOCAL) {
		local_errors++;
		if (options & F_QUIET)
			goto out;
		if (options & F_FLOOD)
			write(STDOUT_FILENO, "E", 1);
		else if (e->ee_errno != EMSGSIZE)
			fprintf(stderr, "ping: local error: %s\n", strerror(e->ee_errno));
		else
			fprintf(stderr, "ping: local error: Message too long, mtu=%u\n", e->ee_info);
		nerrors++;
	} else if (e->ee_origin == SO_EE_ORIGIN_ICMP6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)(e+1);

		if (res < sizeof(icmph) ||
		    memcmp(&target.sin6_addr, &whereto.sin6_addr, 16) ||
		    icmph.icmp6_type != ICMP6_ECHO_REQUEST ||
		    icmph.icmp6_id != ident) {
			/* Not our error, not an error at all. Clear. */
			saved_errno = 0;
			goto out;
		}

		net_errors++;
		nerrors++;
		if (options & F_QUIET)
			goto out;
		if (options & F_FLOOD) {
			write(STDOUT_FILENO, "\bE", 2);
		} else {
			print_timestamp();
			printf("From %s icmp_seq=%u ", pr_addr(&sin6->sin6_addr), ntohs(icmph.icmp6_seq));
			pr_icmph(e->ee_type, e->ee_code, e->ee_info);
			putchar('\n');
			fflush(stdout);
		}
	}

out:
	errno = saved_errno;
	return net_errors ? : -local_errors;
}

/*
 * pinger --
 * 	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
int build_echo(__u8 *_icmph)
{
	struct icmp6_hdr *icmph;
	int cc;

	icmph = (struct icmp6_hdr *)_icmph;
	icmph->icmp6_type = ICMP6_ECHO_REQUEST;
	icmph->icmp6_code = 0;
	icmph->icmp6_cksum = 0;
	icmph->icmp6_seq = htons(ntransmitted+1);
	icmph->icmp6_id = ident;

	if (timing)
		gettimeofday((struct timeval *)&outpack[8],
		    (struct timezone *)NULL);

	cc = datalen + 8;			/* skips ICMP portion */

	return cc;
}

int build_niquery(__u8 *_nih)
{
	struct ni_hdr *nih;
	int cc;

	nih = (struct ni_hdr *)_nih;
	nih->ni_cksum = 0;

	CLR(ntohs((*(__u16*)(nih->ni_nonce))) % mx_dup_ck);

	nih->ni_type = ICMPV6_NI_QUERY;
	cc = sizeof(*nih);
	datalen = 0;

	memcpy(nih->ni_nonce, ni_nonce, sizeof(nih->ni_nonce));
	*(__u16*)(nih->ni_nonce) = htons(ntransmitted + 1);

	nih->ni_code = ni_subject_type;
	nih->ni_qtype = htons(ni_query);
	nih->ni_flags = ni_flag;
	memcpy(nih + 1, ni_subject, ni_subject_len);
	cc += ni_subject_len;

	return cc;
}

int send_probe(void)
{
	int len, cc;

	CLR((ntransmitted+1) % mx_dup_ck);

	if (ni_query >= 0)
		len = build_niquery(outpack);
	else
		len = build_echo(outpack);

	if (cmsglen == 0) {
		cc = sendto(icmp_sock, (char *)outpack, len, confirm,
			    (struct sockaddr *) &whereto,
			    sizeof(struct sockaddr_in6));
	} else {
		struct msghdr mhdr;
		struct iovec iov;

		iov.iov_len  = len;
		iov.iov_base = outpack;

		mhdr.msg_name = &whereto;
		mhdr.msg_namelen = sizeof(struct sockaddr_in6);
		mhdr.msg_iov = &iov;
		mhdr.msg_iovlen = 1;
		mhdr.msg_control = cmsgbuf;
		mhdr.msg_controllen = cmsglen;

		cc = sendmsg(icmp_sock, &mhdr, confirm);
	}
	confirm = 0;

	return (cc == len ? 0 : cc);
}

void pr_echo_reply(__u8 *_icmph, int cc)
{
	struct icmp6_hdr *icmph = (struct icmp6_hdr *) _icmph;
	printf(" icmp_seq=%u", ntohs(icmph->icmp6_seq));
};

static void putchar_safe(char c)
{
	if (isprint(c))
		putchar(c);
	else
		printf("\\%03o", c);
}

void pr_niquery_reply_name(struct ni_hdr *nih, int len)
{
	__u8 *h = (__u8 *)(nih + 1);
	__u8 *p = h + 4;
	__u8 *end = (__u8 *)nih + len;
	int continued = 0;
	char buf[1024];
	int ret;

	len -= sizeof(struct ni_hdr) + 4;

	if (len < 0) {
		printf(" parse error (too short)");
		return;
	}
	while (p < end) {
		int fqdn = 1;
		int len;
		int i;

		memset(buf, 0xff, sizeof(buf));

		if (continued)
			putchar(',');

		ret = dn_expand(h, end, p, buf, sizeof(buf));
		if (ret < 0) {
			printf(" parse error (truncated)");
			break;
		}
		if (p + ret < end && *(p + ret) == '\0')
			fqdn = 0;
		len = strlen(buf);

		putchar(' ');
		for (i = 0; i < strlen(buf); i++)
			putchar_safe(buf[i]);
		if (fqdn)
			putchar('.');

		p += ret + !fqdn;

		continued = 1;
	}
}

void pr_niquery_reply_addr(struct ni_hdr *nih, int len)
{
	__u8 *h = (__u8 *)(nih + 1);
	__u8 *p = h + 4;
	__u8 *end = (__u8 *)nih + len;
	int af;
	int aflen;
	int continued = 0;
	int truncated;
	char buf[1024];

	switch (ntohs(nih->ni_qtype)) {
	case NI_QTYPE_IPV4ADDR:
		af = AF_INET;
		aflen = sizeof(struct in_addr);
		truncated = nih->ni_flags & NI_IPV6ADDR_F_TRUNCATE;
		break;
	case NI_QTYPE_IPV6ADDR:
		af = AF_INET6;
		aflen = sizeof(struct in6_addr);
		truncated = nih->ni_flags & NI_IPV4ADDR_F_TRUNCATE;
		break;
	default:
		/* should not happen */
		af = aflen = truncated = 0;
	}
	p = h;
	if (len < 0) {
		printf(" parse error (too short)");
		return;
	}

	while (p < end) {
		if (continued)
			putchar(',');

		if (p + sizeof(__u32) + aflen > end) {
			printf(" parse error (truncated)");
			break;
		}
		if (!inet_ntop(af, p + sizeof(__u32), buf, sizeof(buf)))
			printf(" unexpeced error in inet_ntop(%s)",
			       strerror(errno));
		else
			printf(" %s", buf);
		p += sizeof(__u32) + aflen;

		continued = 1;
	}
	if (truncated)
		printf(" (truncated)");
}

void pr_niquery_reply(__u8 *_nih, int len)
{
	struct ni_hdr *nih = (struct ni_hdr *)_nih;

	switch (nih->ni_code) {
	case NI_SUCCESS:
		switch (ntohs(nih->ni_qtype)) {
		case NI_QTYPE_NAME:
			pr_niquery_reply_name(nih, len);
			break;
		case NI_QTYPE_IPV4ADDR:
		case NI_QTYPE_IPV6ADDR:
			pr_niquery_reply_addr(nih, len);
			break;
		default:
			printf(" unknown qtype(0x%02x)", ntohs(nih->ni_qtype));
		}
		break;
	case NI_REFUSED:
		printf(" refused");
		break;
	case NI_UNKNOWN:
		printf(" unknown");
		break;
	default:
		printf(" unknown code(%02x)", ntohs(nih->ni_code));
	}
	putchar(';');
}

/*
 * parse_reply --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
int
parse_reply(struct msghdr *msg, int cc, void *addr, struct timeval *tv)
{
	struct sockaddr_in6 *from = addr;
	__u8 *buf = msg->msg_iov->iov_base;
	struct cmsghdr *c;
	struct icmp6_hdr *icmph;
	int hops = -1;

	for (c = CMSG_FIRSTHDR(msg); c; c = CMSG_NXTHDR(msg, c)) {
		if (c->cmsg_level != SOL_IPV6)
			continue;
		switch(c->cmsg_type) {
		case IPV6_HOPLIMIT:
#ifdef IPV6_2292HOPLIMIT
		case IPV6_2292HOPLIMIT:
#endif
			if (c->cmsg_len < CMSG_LEN(sizeof(int)))
				continue;
			hops = *(int*)CMSG_DATA(c);
		}
	}


	/* Now the ICMP part */

	icmph = (struct icmp6_hdr *) buf;
	if (cc < 8) {
		if (options & F_VERBOSE)
			fprintf(stderr, "ping: packet too short (%d bytes)\n", cc);
		return 1;
	}

	if (icmph->icmp6_type == ICMP6_ECHO_REPLY) {
		if (icmph->icmp6_id != ident)
			return 1;
		if (gather_statistics((__u8*)icmph, sizeof(*icmph), cc,
				      ntohs(icmph->icmp6_seq),
				      hops, 0, tv, pr_addr(&from->sin6_addr),
				      pr_echo_reply))
			return 0;
	} else if (icmph->icmp6_type == ICMPV6_NI_REPLY) {
		struct ni_hdr *nih = (struct ni_hdr *)icmph;
		__u16 seq = ntohs(*(__u16 *)nih->ni_nonce);
		if (memcmp(&nih->ni_nonce[2], &ni_nonce[2], sizeof(ni_nonce) - sizeof(__u16)))
			return 1;
		if (gather_statistics((__u8*)icmph, sizeof(*icmph), cc,
				      seq,
				      hops, 0, tv, pr_addr(&from->sin6_addr),
				      pr_niquery_reply))
			return 0;
	} else {
		int nexthdr;
		struct ip6_hdr *iph1 = (struct ip6_hdr*)(icmph+1);
		struct icmp6_hdr *icmph1 = (struct icmp6_hdr *)(iph1+1);

		/* We must not ever fall here. All the messages but
		 * echo reply are blocked by filter and error are
		 * received with IPV6_RECVERR. Ugly code is preserved
		 * however, just to remember what crap we avoided
		 * using RECVRERR. :-)
		 */

		if (cc < 8+sizeof(struct ip6_hdr)+8)
			return 1;

		if (memcmp(&iph1->ip6_dst, &whereto.sin6_addr, 16))
			return 1;

		nexthdr = iph1->ip6_nxt;

		if (nexthdr == 44) {
			nexthdr = *(__u8*)icmph1;
			icmph1++;
		}
		if (nexthdr == IPPROTO_ICMPV6) {
			if (icmph1->icmp6_type != ICMP6_ECHO_REQUEST ||
			    icmph1->icmp6_id != ident)
				return 1;
			acknowledge(ntohs(icmph1->icmp6_seq));
			if (working_recverr)
				return 0;
			nerrors++;
			if (options & F_FLOOD) {
				write(STDOUT_FILENO, "\bE", 2);
				return 0;
			}
			print_timestamp();
			printf("From %s: icmp_seq=%u ", pr_addr(&from->sin6_addr), ntohs(icmph1->icmp6_seq));
		} else {
			/* We've got something other than an ECHOREPLY */
			if (!(options & F_VERBOSE) || uid)
				return 1;
			print_timestamp();
			printf("From %s: ", pr_addr(&from->sin6_addr));
		}
		pr_icmph(icmph->icmp6_type, icmph->icmp6_code, ntohl(icmph->icmp6_mtu));
	}

	if (!(options & F_FLOOD)) {
		if (options & F_AUDIBLE)
			putchar('\a');
		putchar('\n');
		fflush(stdout);
	} else {
		putchar('\a');
		fflush(stdout);
	}
	return 0;
}


int pr_icmph(__u8 type, __u8 code, __u32 info)
{
	switch(type) {
	case ICMP6_DST_UNREACH:
		printf("Destination unreachable: ");
		switch (code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			printf("No route");
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			printf("Administratively prohibited");
			break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			printf("Beyond scope of source address");
			break;
		case ICMP6_DST_UNREACH_ADDR:
			printf("Address unreachable");
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			printf("Port unreachable");
			break;
		default:
			printf("Unknown code %d", code);
			break;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		printf("Packet too big: mtu=%u", info);
		if (code)
			printf(", code=%d", code);
		break;
	case ICMP6_TIME_EXCEEDED:
		printf("Time exceeded: ");
		if (code == ICMP6_TIME_EXCEED_TRANSIT)
			printf("Hop limit");
		else if (code == ICMP6_TIME_EXCEED_REASSEMBLY)
			printf("Defragmentation failure");
		else
			printf("code %d", code);
		break;
	case ICMP6_PARAM_PROB:
		printf("Parameter problem: ");
		if (code == ICMP6_PARAMPROB_HEADER)
			printf("Wrong header field ");
		else if (code == ICMP6_PARAMPROB_NEXTHEADER)
			printf("Unknown header ");
		else if (code == ICMP6_PARAMPROB_OPTION)
			printf("Unknown option ");
		else
			printf("code %d ", code);
		printf ("at %u", info);
		break;
	case ICMP6_ECHO_REQUEST:
		printf("Echo request");
		break;
	case ICMP6_ECHO_REPLY:
		printf("Echo reply");
		break;
	case MLD_LISTENER_QUERY:
		printf("MLD Query");
		break;
	case MLD_LISTENER_REPORT:
		printf("MLD Report");
		break;
	case MLD_LISTENER_REDUCTION:
		printf("MLD Reduction");
		break;
	default:
		printf("unknown icmp type");

	}
	return 0;
}

#include <linux/filter.h>

void install_filter(void)
{
	static int once;
	static struct sock_filter insns[] = {
		BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 4),  /* Load icmp echo ident */
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0xAAAA, 0, 1),  /* Ours? */
		BPF_STMT(BPF_RET|BPF_K, ~0U),  /* Yes, it passes. */
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 0),  /* Load icmp type */
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ICMP6_ECHO_REPLY, 1, 0), /* Echo? */
		BPF_STMT(BPF_RET|BPF_K, ~0U), /* No. It passes. This must not happen. */
		BPF_STMT(BPF_RET|BPF_K, 0), /* Echo with wrong ident. Reject. */
	};
	static struct sock_fprog filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};

	if (once)
		return;
	once = 1;

	/* Patch bpflet for current identifier. */
	insns[1] = (struct sock_filter)BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, htons(ident), 0, 1);

	if (setsockopt(icmp_sock, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter)))
		perror("WARNING: failed to install socket filter\n");
}


/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
char * pr_addr(struct in6_addr *addr)
{
	struct hostent *hp = NULL;

	if (!(options&F_NUMERIC))
		hp = gethostbyaddr((__u8*)addr, sizeof(struct in6_addr), AF_INET6);

	return hp ? hp->h_name : pr_addr_n(addr);
}

char * pr_addr_n(struct in6_addr *addr)
{
	static char str[64];
	inet_ntop(AF_INET6, addr, str, sizeof(str));
	return str;
}

void usage(void)
{
	fprintf(stderr,
"Usage: ping6 [-LUdfnqrvVaAD] [-c count] [-i interval] [-w deadline]\n"
"             [-p pattern] [-s packetsize] [-t ttl] [-I interface]\n"
"             [-M pmtudisc-hint] [-S sndbuf] [-F flowlabel] [-Q tclass]\n"
"             [[-N nodeinfo-option] ...]\n"
"             [hop1 ...] destination\n");
	exit(2);
}
