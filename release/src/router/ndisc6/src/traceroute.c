/*
 * traceroute.c - TCP/IPv6 traceroute tool
 * $Id: traceroute.c 651 2010-05-01 08:08:53Z remi $
 */

/*************************************************************************
 *  Copyright © 2005-2007 Rémi Denis-Courmont.                           *
 *  This program is free software: you can redistribute and/or modify    *
 *  it under the terms of the GNU General Public License as published by *
 *  the Free Software Foundation, versions 2 or 3 of the license.        *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gettext.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* div() */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h> /* nanosleep() */
#include <assert.h>

#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <net/if.h> // IFNAMSIZ, if_nametoindex
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netdb.h>
#include <arpa/inet.h> /* inet_ntop() */
#include <fcntl.h>
#include <errno.h>
#include <locale.h> /* setlocale() */
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif
#ifdef __linux__
# include <linux/filter.h>
#endif

#include "gettime.h"
#include "traceroute.h"

#ifndef AI_IDN
# define AI_IDN 0
#endif

#ifndef IPV6_TCLASS
# if defined (__linux__)
#  define IPV6_TCLASS 67
# elif defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
    || defined (__NetBSD__)  || defined (__NetBSD_kernel__)
#  define IPV6_TCLASS 61
# else
#  warning Traffic class support missing! Define IPV6_TCLASS!
# endif
#endif

#ifndef IPV6_RECVHOPLIMIT
/* Using obsolete RFC 2292 instead of RFC 3542 */
# define IPV6_RECVHOPLIMIT IPV6_HOPLIMIT
#endif

#ifndef ICMP6_DST_UNREACH_BEYONDSCOPE
# define ICMP6_DST_UNREACH_BEYONDSCOPE 2
#endif

#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif
#ifndef SOL_ICMPV6
# define SOL_ICMPV6 IPPROTO_ICMPV6
#endif


/* All our evil global variables */
static const tracetype *type = NULL;
static int niflags = 0;
static int tclass = -1;
uint16_t sport;
static bool debug = false, dontroute = false, show_hlim = false;
bool ecn = false;
static char ifname[IFNAMSIZ] = "";

static const char *rt_segv[127];
static int rt_segc = 0;

/****************************************************************************/

static uint16_t getsourceport (void)
{
	uint16_t v = ~getpid ();
	if (v < 1025)
		v += 1025;
	return htons (v);
}


ssize_t send_payload (int fd, const void *payload, size_t length, int hlim)
{
	char cbuf[CMSG_SPACE (sizeof (int))];
	struct iovec iov =
	{
		.iov_base = (void *)payload,
		.iov_len = length
	};
	struct msghdr hdr =
	{
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cbuf,
		.msg_controllen = sizeof (cbuf)
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR (&hdr);
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_HOPLIMIT;
	cmsg->cmsg_len = CMSG_LEN (sizeof (hlim));

	memcpy (CMSG_DATA (cmsg), &hlim, sizeof (hlim));

	ssize_t rc = sendmsg (fd, &hdr, 0);
	if (rc == (ssize_t)length)
		return 0;

	if (rc != -1)
		errno = EMSGSIZE;
	return -1;
}


static ssize_t
recv_payload (int fd, void *buf, size_t len,
              struct sockaddr_in6 *addr, int *hlim)
{
	char cbuf[CMSG_SPACE (sizeof (int))];
	struct iovec iov =
	{
		.iov_base = buf,
		.iov_len = len
	};
	struct msghdr hdr =
	{
		.msg_name = addr,
		.msg_namelen = (addr != NULL) ? sizeof (*addr) : 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cbuf,
		.msg_controllen = sizeof (cbuf)
	};

	ssize_t val = recvmsg (fd, &hdr, 0);
	if (val == -1)
		return val;

	/* ensures the hop limit is 255 */
	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&hdr);
	     cmsg != NULL;
	     cmsg = CMSG_NXTHDR (&hdr, cmsg))
		if ((cmsg->cmsg_level == IPPROTO_IPV6)
		 && (cmsg->cmsg_type == IPV6_HOPLIMIT))
			memcpy (hlim, CMSG_DATA (cmsg), sizeof (hlim));

	return val;
}


static bool has_port (int protocol)
{
	switch (protocol)
	{
		case IPPROTO_UDP:
		case IPPROTO_TCP:
		//case IPPROTO_SCTP:
		//case IPPROTO_DCCP:
			return true;
	}
	return false;
}


/**
 * Stores difference between two struct timespec in a third struct timespec.
 * The destination CAN be one (or both) term of the difference,
 * and both terms can be identical (though this is rather pointless).
 */
static void
tsdiff (struct timespec *res,
        const struct timespec *from, const struct timespec *to)
{
	res->tv_sec = to->tv_sec - from->tv_sec;
	if (to->tv_nsec < from->tv_nsec)
	{
		res->tv_sec--;
		res->tv_nsec = 1000000000 + to->tv_nsec - from->tv_nsec;
	}
	else
		res->tv_nsec = to->tv_nsec - from->tv_nsec;
}


static ssize_t
parse (trace_parser_t func, const void *data, size_t len,
       int *hlim, int *attempt, uint16_t port)
{
	if (func == NULL)
		return -1;

	unsigned dummy = -1;
	ssize_t rc = func (data, len, hlim, &dummy, port);
	*attempt = dummy;
	if (rc < 0)
		return rc;

	return rc;
}


static const void *
skip_exthdrs (struct ip6_hdr *ip6, size_t *plen)
{
	const uint8_t *payload = (const uint8_t *)(ip6 + 1);
	size_t len = *plen;
	uint8_t nxt = ip6->ip6_nxt;

	for (;;)
	{
		uint16_t hlen;

		switch (nxt)
		{
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
				if (len < 2)
					return NULL;

				hlen = (1 + (uint16_t)payload[1]) << 3;
				break;

			case IPPROTO_FRAGMENT:
				hlen = 8;
				break;

			case IPPROTO_AH:
				if (len < 2)
					return NULL;

				hlen = (2 + (uint16_t)payload[1]) << 2;
				break;

			default: // THE END
				goto out;
		}

		if (len < hlen)
			return NULL; // too short;

		switch (nxt)
		{
			case IPPROTO_ROUTING:
			{
				/* Extract real destination */
				if (payload[3] > 0) // segments left
				{
					if (payload[2] != 0)
						return NULL; // unknown type
	
					/* Handle Routing Type 0 */
					if ((hlen & 8) != 8)
						return NULL; // != 8[16] -> invalid length

					memcpy (&ip6->ip6_dst,
					        payload + (16 * payload[3]) - 8, 16);
				}
				break;
			}

			case IPPROTO_FRAGMENT:
			{
				uint16_t offset;
				memcpy (&offset, payload + 2, 2);
				if (ntohs (offset) >> 3)
					return NULL; // non-first fragment
				break;
			}
		}

		nxt = payload[0];
		len -= hlen;
		payload += hlen;
	}

out:
	ip6->ip6_nxt = nxt;
	*plen = len;
	return payload;
}


#define TRACE_TIMEOUT     0
#define TRACE_OK          1 // TTL exceeded, echo reply, port unreachable...
#define TRACE_CLOSED      2
#define TRACE_OPEN        3
#define TRACE_NOROUTE 0x100 // !N network unreachable
#define TRACE_ADMIN   0x101 // !A administratively prohibited
#define TRACE_BSCOPE  0X102 // !S beyond scope of source address
#define TRACE_NOHOST  0x103 // !H address unreachable
//#define TRACE_TOOBIG  0x100 // !F-%u packet too big
//#define TRACE_NOSUP   0x400 // header field error
#define TRACE_NOPROTO 0x401 // !P unrecognized next header
//#define TRACE_NOOPT 0x402 // unrecognized option


typedef struct
{
	struct sockaddr_in6 addr;  // hop address
	struct timespec     sent;  // request date
	struct timespec     rcvd;  // reply date
	int                 rhlim; // received hop limit
	unsigned            result;// 0: no reply, 1: ok, 2: closed, 3: open
} tracetest_t;


static int
icmp_recv (int fd, tracetest_t *res, int *attempt, int *hlim,
           const struct sockaddr_in6 *dst)
{
	struct
	{
		struct icmp6_hdr hdr;
		struct ip6_hdr inhdr;
		uint8_t buf[1192];
	} pkt;
	res->rhlim = -1;

	ssize_t len = recv_payload (fd, &pkt, sizeof (pkt), &res->addr,
	                            &res->rhlim);

	if (len < (ssize_t)(sizeof (pkt.hdr) + sizeof (pkt.inhdr)))
		return 0; // too small

	len -= sizeof (pkt.hdr) + sizeof (pkt.inhdr);

#if 0
	/* "Extended" ICMP detection */
	const uint8_t *ext = NULL;
	size_t extlen = 0;
	switch (pkt.hdr.icmp6_type)
	{
		case ICMP6_DST_UNREACH:
		case ICMP6_TIME_EXCEEDED:
			if (pkt.hdr.icmp6_data8[0] != 0)
			{
				short origlen = pkt.hdr.icmp6_data8[0] << 6;
				if (origlen > len)
					return 0; // malformatted extended ICMP

				assert (origlen >= 40);
				ext = pkt.buf - 40 + origlen;
				extlen = len - origlen;
				len = origlen;

				if ((extlen < 1) || ((ext[0] >> 4) != 2))
					break; // no ext / unknown version
				if (extlen < 4)
					return 0; // malformatted v2 extension

				/* FIXME: compute checksum */
			}
	}
#endif

	const void *buf = skip_exthdrs (&pkt.inhdr, (size_t *)&len);

	if (memcmp (&pkt.inhdr.ip6_dst, &dst->sin6_addr, 16))
		return 0; // wrong destination

	if (pkt.inhdr.ip6_nxt != type->protocol)
		return 0; // wrong protocol

	len = parse (type->parse_err, buf, len, hlim, attempt, dst->sin6_port);
	if (len < 0)
		return 0;

	/* interesting ICMPv6 error */
	bool final = true;
	switch (pkt.hdr.icmp6_type)
	{
		case ICMP6_DST_UNREACH:
			switch (pkt.hdr.icmp6_code)
			{
				case ICMP6_DST_UNREACH_NOROUTE:
				case ICMP6_DST_UNREACH_ADMIN:
				case ICMP6_DST_UNREACH_BEYONDSCOPE:
				case ICMP6_DST_UNREACH_ADDR:
					res->result = 0x100 | pkt.hdr.icmp6_code;
					break;
				case ICMP6_DST_UNREACH_NOPORT:
					res->result = TRACE_OK;
					break;
				case 5: // ingress/egress policy failure
				case 6: // reject route
					res->result = TRACE_ADMIN;
					break;
			}
			break;

		case ICMP6_PARAM_PROB:
			switch (pkt.hdr.icmp6_code)
			{
				case ICMP6_PARAMPROB_NEXTHEADER:
					res->result = 0x400 | pkt.hdr.icmp6_code;
			}
			break;

		case ICMP6_TIME_EXCEEDED:
			if (pkt.hdr.icmp6_code == ICMP6_TIME_EXCEED_TRANSIT)
			{
				res->result = TRACE_OK;
				final = false; // intermediary reponse
				break;
			}
		default: // should not happen (ICMPv6 filter)
			return 0;
	}

#if 0
	/* "Extended" ICMP handling */
	if (extlen > 0)
	{
		ext += 4;
		extlen -= 4;
		while (extlen >= 4)
		{
			uint16_t objlen = (ext[0] << 8) | ext[1];
			if ((objlen & 3) || (objlen < 4) // malformatted object
			 || (objlen > extlen)) // incomplete object
				break;

			switch (ext[2])
			{
			}

			ext += objlen;
			extlen -= objlen;
		}
	}
#endif

	if (!final)
		return 1; // intermediary response

	// final response received
	return memcmp (&res->addr.sin6_addr, &dst->sin6_addr, 16) ? 2 : 3;
}


static int
proto_recv (int fd, tracetest_t *res, int *attempt, int *hlim,
            const struct sockaddr_in6 *dst)
{
	res->rhlim = -1;

	uint8_t buf[1240];
	ssize_t len = recv_payload (fd, buf, sizeof (buf), NULL,
	                            &res->rhlim);
	if (len < 0)
	{
		switch (errno)
		{
			case EAGAIN:
			case ECONNREFUSED:
#ifdef EPROTO
			case EPROTO:
#endif
				return 0;

			default:
				/* These are very bad errors (-> bugs) */
				fprintf (stderr, _("Receive error: %s\n"), strerror (errno));
				return 0;
		}
	}

	len = parse (type->parse_resp, buf, len, hlim, attempt, dst->sin6_port);
	if (len < 0)
		return 0;

	/* Route determination complete! */
	memcpy (&res->addr, dst, sizeof (res->addr));
	res->result = 1 + len;
	return 1; // response received
}


static int
probe (int protofd, int icmpfd, const struct sockaddr_in6 *dst,
       const struct timespec *deadline,
       tracetest_t *res, int *hlim, int *attempt)
{
	for (;;)
	{
		struct pollfd ufds[2];

		memset (ufds, 0, sizeof (ufds));
		ufds[0].fd = protofd;
		ufds[0].events = POLLIN;
		ufds[1].fd = icmpfd;
		ufds[1].events = POLLIN;

		struct timespec recvd;
		mono_gettime (&recvd);
		int val = ((deadline->tv_sec  - recvd.tv_sec ) * 1000)
		   + (int)((deadline->tv_nsec - recvd.tv_nsec) / 1000000);

		val = poll (ufds, 2, val > 0 ? val : 0);
		mono_gettime (&recvd);

		if (val < 0) /* interrupted by signal - well, not really */
			continue;
		if (val == 0)
		{
			*hlim = -1;
			break;
		}

		/* Receive final packet when host reached */
		if (ufds[0].revents)
		{
			if (proto_recv (protofd, res, attempt, hlim, dst) > 0)
			{
				res->rcvd = recvd;
				return 1;
			}
		}

		/* Receive ICMP errors along the way */
		if (ufds[1].revents)
		{
			val = icmp_recv (icmpfd, res, attempt, hlim, dst);
			if (val)
				res->rcvd = recvd;

			switch (val)
			{
				case 1:
					return 0; // TTL exceeded
				case 2:
					return -1; // unreachable
				case 3:
					return 1; // reached
			}
		}
	}
	return 0;
}


/* Performs reverse lookup; print hostname and address */
static void
printname (const struct sockaddr *addr, size_t addrlen)
{
	char buf[NI_MAXHOST];
	int flags = niflags;

	int c = getnameinfo (addr, addrlen, buf, sizeof (buf), NULL, 0, flags);
	if (c == 0)
	{
		printf (" %s ", buf);

		if (flags & NI_NUMERICHOST)
			return;
	}
	flags |= NI_NUMERICHOST;

	if (getnameinfo (addr, addrlen, buf, sizeof (buf), NULL, 0, flags))
		strcpy (buf, "???");

	if (c != 0)
		printf (" %s ", buf); // work around DNS resolution failure

	printf ("(%s) ", buf);
}


static inline void
printrtt (const struct timespec *rtt)
{
	div_t d = div (rtt->tv_nsec / 1000, 1000);

	/*
	 * For some stupid reasons, div() returns a negative remainder when
	 * the numerator is negative, instead of following the mathematicians
	 * convention that the remainder always be positive.
	 */
	if (d.rem < 0)
	{
		d.quot--;
		d.rem += 1000;
	}
	d.quot += 1000 * rtt->tv_sec;

	printf (_(" %u.%03u ms "), (unsigned)(d.quot), (unsigned)d.rem);
}


static void
display (const tracetest_t *tab, unsigned min_ttl, unsigned max_ttl,
         unsigned retries)
{
	for (unsigned ttl = min_ttl; ttl <= max_ttl; ttl++)
	{
		struct sockaddr_in6 hop = { .sin6_family = AF_UNSPEC };
		const tracetest_t *line = tab + retries * (ttl - min_ttl);

		printf ("%2d ", ttl);

		for (unsigned col = 0; col < retries; col++)
		{
			const tracetest_t *test = line + col;
			if (test->result == TRACE_TIMEOUT)
			{
				fputs (" *", stdout);
				continue;
			}

			if ((col == 0) || memcmp (&hop, &test->addr, sizeof (hop)))
			{
				memcpy (&hop, &test->addr, sizeof (hop));
				printname ((struct sockaddr *)&hop, sizeof (hop));
			}

			struct timespec rtt;
			tsdiff (&rtt, &test->sent, &test->rcvd);
			printrtt (&rtt);

			if ((col == 0) || (test[-1].result != test->result))
			{
				const char *msg;

				switch (test->result)
				{
					case TRACE_CLOSED:
						msg = N_("[closed] ");
						break;

					case TRACE_OPEN:
						msg = N_("[open] ");
						break;

					default:
						msg = NULL;
				}

				if (msg != NULL)
					fputs (gettext (msg), stdout);
			}

			if (test->rhlim != -1)
				printf ("(%d) ", test->rhlim);

			const char *msg2;
			switch (test->result)
			{
				case TRACE_NOROUTE:
					msg2 = "!N ";
					break;

				case TRACE_ADMIN:
					msg2 = "!A ";
					break;

				case TRACE_BSCOPE:
					msg2 = "!S ";
					break;

				case TRACE_NOHOST:
					msg2 = "!H ";
					break;

				case TRACE_NOPROTO:
					msg2 = "!P ";
					break;

				default:
					msg2 = NULL;
			}

			if (msg2 != NULL)
				fputs (msg2, stdout);
		}

		fputc ('\n', stdout);
	}
}



static int
getaddrinfo_err (const char *host, const char *serv,
                 const struct addrinfo *hints, struct addrinfo **res)
{
	int val = getaddrinfo (host, serv, hints, res);
	if (val)
	{
		fprintf (stderr, _("%s%s%s%s: %s\n"), host ?: "", host ? " " : "",
		         serv ? _("port ") : "", serv ?: "", gai_strerror (val));
		return val;
	}
	return 0;
}


static int
connect_proto (int fd, struct sockaddr_in6 *dst,
               const char *dsthost, const char *dstport,
               const char *srchost, const char *srcport)
{
	struct addrinfo hints, *res;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = type->gai_socktype;
	hints.ai_flags = AI_CANONNAME | AI_IDN;

	if ((srchost != NULL) || (srcport != NULL))
	{
		hints.ai_flags |= AI_PASSIVE;

		if (getaddrinfo_err (srchost, srcport, &hints, &res))
			return -1;

		if (bind (fd, res->ai_addr, res->ai_addrlen))
		{
			perror (srchost);
			goto error;
		}

		if (srcport != NULL)
			sport = ((const struct sockaddr_in6 *)res->ai_addr)->sin6_port;
		freeaddrinfo (res);

		hints.ai_flags &= ~AI_PASSIVE;
	}

	if (srcport == NULL)
		sport = getsourceport ();

	if (getaddrinfo_err (dsthost, dstport, &hints, &res))
		return -1;

	if (res->ai_addrlen > sizeof (*dst))
		goto error;

	if (connect (fd, res->ai_addr, res->ai_addrlen))
	{
		perror (dsthost);
		goto error;
	}

	char buf[INET6_ADDRSTRLEN];
	if (inet_ntop (AF_INET6,
	               &((const struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
	               buf, sizeof (buf)) == NULL)
		strcpy (buf, "??");

	printf (_("traceroute to %s (%s) "), res->ai_canonname, buf);

	if ((getsockname (fd, (struct sockaddr *)dst,
	                  &(socklen_t){ sizeof (*dst) }) == 0)
	 && inet_ntop (AF_INET6, &dst->sin6_addr, buf, sizeof (buf)))
		printf (_("from %s, "), buf);

	memcpy (dst, res->ai_addr, res->ai_addrlen);
	if (has_port (type->protocol))
		printf (_("port %u, from port %u, "), ntohs (dst->sin6_port),
		        ntohs (sport));

	freeaddrinfo (res);
	return 0;

error:
	freeaddrinfo (res);
	return -1;
}


static void setup_socket (int fd)
{
	if (debug)
		setsockopt (fd, SOL_SOCKET, SO_DEBUG, &(int){ 1 }, sizeof (int));
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

	if (show_hlim)
		setsockopt (fd, SOL_IPV6, IPV6_RECVHOPLIMIT, &(int){1}, sizeof (int));

	int val = fcntl (fd, F_GETFL);
	if (val == -1)
		val = 0;
	fcntl (fd, F_SETFL, O_NONBLOCK | val);
	fcntl (fd, F_GETFD, FD_CLOEXEC);
}


static int setsock_rth (int fd, int type, const char **segv, int segc)
{
	uint8_t hdr[inet6_rth_space (type, segc)];
	inet6_rth_init (hdr, sizeof (hdr), type, segc);

	struct addrinfo hints;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_IDN;

	for (int i = 0; i < segc; i++)
	{
		struct addrinfo *res;

		if (getaddrinfo_err (segv[i], NULL, &hints, &res))
			return -1;

		const struct sockaddr_in6 *a = (const void *)res->ai_addr;
		if (inet6_rth_add (hdr, &a->sin6_addr))
			return -1;
	}

#ifdef IPV6_RTHDR
	return setsockopt (fd, SOL_IPV6, IPV6_RTHDR, hdr, sizeof (hdr));
#else
	errno = ENOSYS;
	return -1;
#endif
}


/* Requests raw sockets ahead of use so we can drop root quicker */
static struct
{
	int protocol;
	int fd;
	int errnum;
} protofd[] =
{
	{ IPPROTO_ICMPV6, -1, EPERM },
	{ IPPROTO_ICMPV6, -1, EPERM },
	{ IPPROTO_UDP,    -1, EPERM },
	{ IPPROTO_UDPLITE,-1, EPERM },
	{ IPPROTO_TCP,    -1, EPERM }
};


static int prepare_sockets (void)
{
	for (unsigned i = 0; i < sizeof (protofd) / sizeof (protofd[0]); i++)
	{
		protofd[i].fd = socket (AF_INET6, SOCK_RAW, protofd[i].protocol);
		if (protofd[i].fd == -1)
			protofd[i].errnum = errno;
		else
		if (protofd[i].fd <= 2)
			return -1;
	}
	return 0;
}


static int get_socket (int protocol)
{
	errno = EPROTONOSUPPORT;
	for (unsigned i = 0; i < sizeof (protofd) / sizeof (protofd[0]); i++)
		if (protofd[i].protocol == protocol)
		{
			int fd = protofd[i].fd;
			if (fd != -1)
			{
				protofd[i].fd = -1;
				return fd;
			}
			errno = protofd[i].errnum;
		}

	return -1;
}


static void drop_sockets (void)
{
	for (unsigned i = 0; i < sizeof (protofd) / sizeof (protofd[0]); i++)
		if (protofd[i].fd != -1)
			close (protofd[i].fd);
}


static int
traceroute (const char *dsthost, const char *dstport,
            const char *srchost, const char *srcport,
            unsigned timeout, unsigned delay, unsigned retries,
            size_t packet_len, int min_ttl, int max_ttl)
{
	/* Creates ICMPv6 socket to collect error packets */
	int icmpfd = get_socket (IPPROTO_ICMPV6);
	if (icmpfd == -1)
	{
		perror (_("Raw IPv6 socket"));
		return -1;
	}

	/* Creates protocol-specific socket */
	int protofd = get_socket (type->protocol);
	if (protofd == -1)
	{
		perror (_("Raw IPv6 socket"));
		close (icmpfd);
		return -1;
	}

	drop_sockets ();

#ifdef IPV6_PKTINFO
	/* Set outgoing interface */
	if (*ifname)
	{
		struct in6_pktinfo nfo;

		memset (&nfo, 0, sizeof (nfo));
		nfo.ipi6_ifindex = if_nametoindex (ifname);
		if (nfo.ipi6_ifindex == 0)
		{
			fprintf (stderr, _("%s: %s\n"), ifname, strerror (ENXIO));
			goto error;
		}

		if (setsockopt (protofd, SOL_IPV6, IPV6_PKTINFO, &nfo, sizeof (nfo)))
		{
			perror (ifname);
			goto error;
		}
	}
#endif

	setup_socket (icmpfd);
	setup_socket (protofd);

	/* Set ICMPv6 filter */
	{
		struct icmp6_filter f;

		ICMP6_FILTER_SETBLOCKALL (&f);
		ICMP6_FILTER_SETPASS (ICMP6_DST_UNREACH, &f);
		ICMP6_FILTER_SETPASS (ICMP6_TIME_EXCEEDED, &f);
		ICMP6_FILTER_SETPASS (ICMP6_PARAM_PROB, &f);
		setsockopt (icmpfd, SOL_ICMPV6, ICMP6_FILTER, &f, sizeof (f));
	}

	/* Defines protocol-specific checksum offset */
	if ((type->checksum_offset != -1)
	 && setsockopt (protofd, SOL_IPV6, IPV6_CHECKSUM, &type->checksum_offset,
	                sizeof (int)))
	{
		perror ("setsockopt(IPV6_CHECKSUM)");
		goto error;
	}

	/* Set ICMPv6 filter for echo replies */
	if (type->protocol == IPPROTO_ICMPV6)
	{
		// NOTE: we assume any ICMP probes type uses ICMP echo
		struct icmp6_filter f;

		ICMP6_FILTER_SETBLOCKALL (&f);
		ICMP6_FILTER_SETPASS (ICMP6_ECHO_REPLY, &f);
		setsockopt (protofd, SOL_ICMPV6, ICMP6_FILTER, &f, sizeof (f));
	}

#ifdef IPV6_TCLASS
	/* Defines traffic class */
	setsockopt (protofd, SOL_IPV6, IPV6_TCLASS, &tclass, sizeof (tclass));
#endif

	if (dontroute)
		setsockopt (protofd, SOL_SOCKET, SO_DONTROUTE, &(int){ 1 },
		            sizeof (int));

	/* Defines Type 0 Routing Header */
	if (rt_segc > 0)
		setsock_rth (protofd, IPV6_RTHDR_TYPE_0, rt_segv, rt_segc);

	/* Defines destination */
	struct sockaddr_in6 dst;
	memset (&dst, 0, sizeof (dst));
	if (connect_proto (protofd, &dst, dsthost, dstport, srchost, srcport))
		goto error;
	printf (ngettext ("%u hop max, ", "%u hops max, ", max_ttl), max_ttl);

#ifdef SO_ATTACH_FILTER
	{
		/* Static socket filter for the unconnected ICMPv6 socket.
		* We are only interested if the inner IPv6 packet has the
		* right IPv6 destination */
		struct sock_filter f[10], *pc = f;
		struct sock_fprog sfp = {
			.len = sizeof (f) / sizeof (f[0]),
			.filter = f,
		};

		for (unsigned i = 0; i < 4; i++)
		{
			/* A = icmp->ip6_dst.s6_addr32[i]; */
			pc->code = BPF_LD + BPF_W + BPF_ABS;
			pc->jt = pc->jf = 0;
			pc->k = 8 + 24 + (i * 4);
			pc++;

			/* if (A != dst.sin6_addr.s6_addr32[i]) goto drop; */
			pc->code = BPF_JMP + BPF_JEQ + BPF_K;
			pc->jt = 0;
			pc->jf = f + (sizeof (f) / sizeof (f[0])) - (pc + 2);
			pc->k = ntohl (dst.sin6_addr.s6_addr32[i]);
			pc++;
		}

		/* return ~0U; */
		pc->code = BPF_RET + BPF_K;
		pc->jt = pc->jf = 0;
		pc->k = ~0U;
		pc++;

		/* drop: return 0; */
		pc->code = BPF_RET + BPF_K;
		pc->jt = pc->jf = pc->k = 0;

		assert ((++pc - f) == (sizeof (f) / sizeof (f[0])));
		setsockopt (icmpfd, SOL_SOCKET, SO_ATTACH_FILTER, &sfp, sizeof (sfp));
	}
#endif

	/* Adjusts packets length */
	size_t overhead = sizeof (struct ip6_hdr);
	if (rt_segc > 0)
		overhead += inet6_rth_space (IPV6_RTHDR_TYPE_0, rt_segc);
	if (packet_len < overhead)
		packet_len = overhead;

	printf (ngettext ("%zu byte packets\n", "%zu bytes packets\n", packet_len),
            packet_len);
	packet_len -= overhead;

	struct timespec delay_ts;
	if (delay)
	{
		div_t d = div (delay, 1000);
		delay_ts.tv_sec = d.quot;
		delay_ts.tv_nsec = d.rem * 1000000;
	}

	/* Performs traceroute */
	int val = 0;
	if (max_ttl >= min_ttl)
	{
		tracetest_t tab[(1 + max_ttl - min_ttl) * retries];
		memset (tab, 0, sizeof (tab));

		for (unsigned step = 1, progress = 0;
		     step < (1 + max_ttl - min_ttl) + retries;
		     step++)
		{
			unsigned pending = 0;

			if (isatty (1))
			{
				unsigned total = retries * (max_ttl - min_ttl + 1);
				printf (_(" %3u%% completed..."), 100 * progress / total);
				fputc ('\r', stdout);
			}

			if (delay && (step > 1))
				mono_nanosleep (&delay_ts);

			/* Sends requests */
			for (unsigned i = 0; i < retries; i++)
			{
				int attempt = (retries - 1) - i;
				int hlim = min_ttl + step + i - retries;

				if ((hlim > max_ttl) || (hlim < min_ttl))
					continue;

				tracetest_t *t = tab + (hlim - min_ttl) * retries + attempt;
				assert (t >= tab);
				assert (t < tab + (sizeof (tab) / sizeof (tab[0])));

				if (type->send_probe (protofd, hlim, attempt, packet_len,
				                      dst.sin6_port))
				{
					fprintf (stderr, _("Cannot send data: %s\n"),
					         strerror (errno));
					return -1;
				}

				mono_gettime (&t->sent);
				pending++;
			}

			struct timespec deadline;
			mono_gettime (&deadline);
			deadline.tv_sec += timeout;

			/* Receives replies */
			while (pending > 0)
			{
				tracetest_t results;
				int hlim = -1;
				int attempt = -1;
				int res = probe (protofd, icmpfd, &dst, &deadline,
				                 &results, &hlim, &attempt);

				if (hlim == -1) /* timeout! */
				{
					progress += pending;
					break;
				}

				if ((hlim > max_ttl) || (hlim < min_ttl))
					continue;

				if (attempt == -1)
					attempt = min_ttl + step - (hlim + 1);

				if ((attempt < 0) || ((unsigned)attempt >= retries))
					continue;

				tracetest_t *t = tab + (hlim - min_ttl) * retries + attempt;
				assert (t >= tab);
				assert (t < tab + (sizeof (tab) / sizeof (tab[0])));

				if (t->result == TRACE_TIMEOUT /* no result yet */)
				{
					struct timespec buf = t->sent;
					memcpy (t, &results, sizeof (*t));
					t->sent = buf;
					pending--;

					if (isatty (1))
					{
						unsigned total = retries * (max_ttl - min_ttl + 1);
						printf (_(" %3u%% completed..."),
						        100 * ++progress / total);
						fputc ('\r', stdout);
					}
				}

				if (res && (val <= 0))
				{
					val = res > 0 ? 1 : -1; // sign <-> reachability
					max_ttl = hlim;
				}
			}

			if (isatty (1))
			{
				// white spaces to erase "xxx% completed..."
				fputs (_("                  "), stdout);
				fputc ('\r', stdout);
			}

			if (step >= retries)
			{
				int hl = min_ttl + step - retries;
				display (tab + retries *(hl - min_ttl), hl, hl, retries);
			}
		}
	}

	/* Cleans up */
	close (protofd);
	close (icmpfd);
	return val > 0 ? 0 : -2;

error:
	close (protofd);
	close (icmpfd);
	return -1;
}


static int
quick_usage (const char *path)
{
	fprintf (stderr, _("Try \"%s -h\" for more information.\n"), path);
	return 2;
}


static int
usage (const char *path)
{
	printf (_(
"Usage: %s [options] <IPv6 hostname/address> [%s]\n"
"Print IPv6 network route to a host\n"), path, _("packet length"));

	puts (_("\n"
"  -A  send TCP ACK probes\n"
"  -d  enable socket debugging\n"
"  -E  set TCP Explicit Congestion Notification bits in TCP packets\n"
"  -f  specify the initial hop limit (default: 1)\n"
"  -g  insert a route segment within a \"Type 0\" routing header\n"
"  -h  display this help and exit\n"
"  -I  use ICMPv6 Echo Request packets as probes\n"
"  -i  force outgoing network interface\n"
"  -l  display incoming packets hop limit\n"
"  -m  set the maximum hop limit (default: 30)\n"
"  -N  perform reverse name lookups on the addresses of every hop\n"
"  -n  don't perform reverse name lookup on addresses\n"
"  -p  override destination port\n"
"  -q  override the number of probes per hop (default: 3)\n"
"  -r  do not route packets\n"
"  -S  send TCP SYN probes\n"
"  -s  specify the source IPv6 address of probe packets\n"
"  -t  set traffic class of probe packets\n"
"  -U  send UDP probes (default)\n"
"  -V  display program version and exit\n"
/*"  -v, --verbose  display all kind of ICMPv6 errors\n"*/
"  -w  override the timeout for response in seconds (default: 5)\n"
"  -z  specify a time to wait (in ms) between each probes (default: 0)\n"
	));

	return 0;
}


static int
version (void)
{
	printf (_(
"traceroute6: TCP & UDP IPv6 traceroute tool %s (%s)\n"), VERSION, "$Rev: 651 $");
	printf (_(" built %s on %s\n"), __DATE__, PACKAGE_BUILD_HOSTNAME);
	printf (_("Configured with: %s\n"), PACKAGE_CONFIGURE_INVOCATION);
	puts (_("Written by Remi Denis-Courmont\n"));

	printf (_("Copyright (C) %u-%u Remi Denis-Courmont\n"), 2005, 2007);
	puts (_("This is free software; see the source for copying conditions.\n"
	        "There is NO warranty; not even for MERCHANTABILITY or\n"
	        "FITNESS FOR A PARTICULAR PURPOSE.\n"));
	return 0;
}


static unsigned
parse_hlim (const char *str)
{
	char *end;
	unsigned long u = strtoul (str, &end, 0);
	if ((u > 255) || *end)
	{
		fprintf (stderr, _("%s: invalid hop limit\n"), str);
		return (unsigned)(-1);
	}
	return (unsigned)u;
}


static size_t
parse_plen (const char *str)
{
	char *end;
	unsigned long u = strtoul (str, &end, 0);
	if ((u > 65575) || *end)
	{
		fprintf (stderr, _("%s: invalid packet length\n"), str);
		return (size_t)(-1);
	}
	return (size_t)u;
}


static const struct option opts[] = 
{
	{ "ack",      no_argument,       NULL, 'A' },
	{ "debug",    no_argument,       NULL, 'd' },
	{ "ecn",      no_argument,       NULL, 'E' },
	// -F is a stub
	{ "first",    required_argument, NULL, 'f' },
	{ "segment",  required_argument, NULL, 'g' },
	{ "help",     no_argument,       NULL, 'h' },
	{ "icmp",     no_argument,       NULL, 'I' },
	{ "iface",    required_argument, NULL, 'i' },
	{ "hlim",     no_argument,       NULL, 'l' },
	{ "max",      required_argument, NULL, 'm' },
	// -N is not really a stub, should have a long name
	{ "numeric",  no_argument,       NULL, 'n' },
	{ "port",     required_argument, NULL, 'p' },
	{ "retry",    required_argument, NULL, 'q' },
	{ "noroute",  no_argument,       NULL, 'r' },
	{ "syn",      no_argument,       NULL, 'S' },
	{ "source",   required_argument, NULL, 's' },
	{ "tclass",   required_argument, NULL, 't' },
	{ "udp",      no_argument,       NULL, 'U' },
	{ "version",  no_argument,       NULL, 'V' },
	/*{ "verbose",  no_argument,       NULL, 'v' },*/
	{ "wait",     required_argument, NULL, 'w' },
	// -x is a stub
	{ "delay",    required_argument, NULL, 'z' },
	{ NULL,       0,                 NULL, 0   }
};


static const char optstr[] = "AdEFf:g:hIi:Llm:Nnp:q:rSs:t:UVw:xz:" "P:";

int
main (int argc, char *argv[])
{
	if (prepare_sockets () || setuid (getuid ()))
		return 1;

	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	const char *dsthost, *srchost = NULL, *dstport = "33434", *srcport = NULL;
	size_t plen = 60;
	unsigned retries = 3, wait = 5, delay = 0, minhlim = 1, maxhlim = 30;
	int val;

	while ((val = getopt_long (argc, argv, optstr, opts, NULL)) != EOF)
	{
		switch (val)
		{
			case 'A':
				type = &ack_type;
				break;

			case 'd':
				debug = true;
				break;

			case 'E':
				ecn = true;
				break;

			case 'F': // stub (don't fragment)
				break;

			case 'f':
				if ((minhlim = parse_hlim (optarg)) == (unsigned)(-1))
					return 1;
				break;

			case 'g':
				if (rt_segc >= 127)
				{
					fprintf (stderr,
					         "%s: Too many route segments specified.\n",
					         optarg);
					return 1;
				}
				rt_segv[rt_segc++] = optarg;
				break;

			case 'h':
				return usage (argv[0]);

			case 'I':
				type = &echo_type;
				break;

			case 'i':
				strncpy (ifname, optarg, IFNAMSIZ - 1);
				ifname[IFNAMSIZ - 1] = '\0';
				break;

			/* We should really have a generic option and
			 * use getprotobyname() instead. Semantics of -L
			 * will likely change in future versions!! */
			case 'L':
				type = &udplite_type;
				break;

			case 'l':
				show_hlim = true;
				break;

			case 'm':
				if ((maxhlim = parse_hlim (optarg)) == (unsigned)(-1))
					return 1;
				break;

			case 'N':
				/*
				 * FIXME: should we differenciate private addresses as
				 * tcptraceroute does?
				 */
				niflags &= ~NI_NUMERICHOST;
				break;

			case 'n':
				niflags |= NI_NUMERICHOST | NI_NUMERICSERV;
				break;

			case 'P':
				srcport = optarg;
				break;

			case 'p':
				dstport = optarg;
				break;

			case 'q':
			{
				char *end;
				unsigned long l = strtoul (optarg, &end, 0);
				if (*end || l > 255)
					return quick_usage (argv[0]);
				retries = l;
				break;
			}

			case 'r':
				dontroute = true;
				break;

			case 'S':
				type = &syn_type;
				break;

			case 's':
				srchost = optarg;
				break;

			case 't':
			{
				char *end;
				unsigned long l = strtoul (optarg, &end, 0);
				if (*end || l > 255)
					return quick_usage (argv[0]);
				tclass = l;
				break;
			}

			case 'U':
				type = &udp_type;
				break;

			case 'V':
				return version ();

			case 'w':
			{
				char *end;
				unsigned long l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				wait = (unsigned)l;
				break;
			}

			case 'x': // stub: no IPv6 checksums
				break;

			case 'z':
			{
				char *end;
				unsigned long l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				delay = (unsigned)l;
				break;
			}

			case '?':
			default:
				return quick_usage (argv[0]);
		}
	}

	if (type == NULL)
		type = &udp_type;

	if (optind >= argc)
		return quick_usage (argv[0]);

	dsthost = argv[optind++];

	if (optind < argc)
	{
		plen = parse_plen (argv[optind++]);
		if (plen == (size_t)(-1))
			return 1;
	}

	if (optind < argc)
		return quick_usage (argv[0]);

	setvbuf (stdout, NULL, _IONBF, 0);
	return -traceroute (dsthost, dstport, srchost, srcport, wait, delay,
	                    retries, plen, minhlim, maxhlim);
}
