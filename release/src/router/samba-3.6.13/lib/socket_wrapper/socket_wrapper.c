/*
 * Copyright (C) Jelmer Vernooij 2005,2008 <jelmer@samba.org>
 * Copyright (C) Stefan Metzmacher 2006-2009 <metze@samba.org>
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
   Socket wrapper library. Passes all socket communication over
   unix domain sockets if the environment variable SOCKET_WRAPPER_DIR
   is set.
*/

#ifdef _SAMBA_BUILD_

#define SOCKET_WRAPPER_NOT_REPLACE
#include "../replace/replace.h"
#include "system/network.h"
#include "system/filesys.h"
#include "system/time.h"

#else /* _SAMBA_BUILD_ */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <errno.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#endif

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

#define SWRAP_DLIST_ADD(list,item) do { \
	if (!(list)) { \
		(item)->prev	= NULL; \
		(item)->next	= NULL; \
		(list)		= (item); \
	} else { \
		(item)->prev	= NULL; \
		(item)->next	= (list); \
		(list)->prev	= (item); \
		(list)		= (item); \
	} \
} while (0)

#define SWRAP_DLIST_REMOVE(list,item) do { \
	if ((list) == (item)) { \
		(list)		= (item)->next; \
		if (list) { \
			(list)->prev	= NULL; \
		} \
	} else { \
		if ((item)->prev) { \
			(item)->prev->next	= (item)->next; \
		} \
		if ((item)->next) { \
			(item)->next->prev	= (item)->prev; \
		} \
	} \
	(item)->prev	= NULL; \
	(item)->next	= NULL; \
} while (0)

/* LD_PRELOAD doesn't work yet, so REWRITE_CALLS is all we support
 * for now */
#define REWRITE_CALLS 

#ifdef REWRITE_CALLS
#define real_accept accept
#define real_connect connect
#define real_bind bind
#define real_listen listen
#define real_getpeername getpeername
#define real_getsockname getsockname
#define real_getsockopt getsockopt
#define real_setsockopt setsockopt
#define real_recvfrom recvfrom
#define real_sendto sendto
#define real_sendmsg sendmsg
#define real_ioctl ioctl
#define real_recv recv
#define real_read read
#define real_send send
#define real_readv readv
#define real_writev writev
#define real_socket socket
#define real_close close
#endif

#ifdef HAVE_GETTIMEOFDAY_TZ
#define swrapGetTimeOfDay(tval) gettimeofday(tval,NULL)
#else
#define swrapGetTimeOfDay(tval)	gettimeofday(tval)
#endif

/* we need to use a very terse format here as IRIX 6.4 silently
   truncates names to 16 chars, so if we use a longer name then we
   can't tell which port a packet came from with recvfrom() 
   
   with this format we have 8 chars left for the directory name
*/
#define SOCKET_FORMAT "%c%02X%04X"
#define SOCKET_TYPE_CHAR_TCP		'T'
#define SOCKET_TYPE_CHAR_UDP		'U'
#define SOCKET_TYPE_CHAR_TCP_V6		'X'
#define SOCKET_TYPE_CHAR_UDP_V6		'Y'

#define MAX_WRAPPED_INTERFACES 16

#ifdef HAVE_IPV6
/*
 * FD00::5357:5FXX
 */
static const struct in6_addr *swrap_ipv6(void)
{
	static struct in6_addr v;
	static int initialized;
	int ret;

	if (initialized) {
		return &v;
	}
	initialized = 1;

	ret = inet_pton(AF_INET6, "FD00::5357:5F00", &v);
	if (ret <= 0) {
		abort();
	}

	return &v;
}
#endif

static struct sockaddr *sockaddr_dup(const void *data, socklen_t len)
{
	struct sockaddr *ret = (struct sockaddr *)malloc(len);
	memcpy(ret, data, len);
	return ret;
}

static void set_port(int family, int prt, struct sockaddr *addr)
{
	switch (family) {
	case AF_INET:
		((struct sockaddr_in *)addr)->sin_port = htons(prt);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		((struct sockaddr_in6 *)addr)->sin6_port = htons(prt);
		break;
#endif
	}
}

static size_t socket_length(int family)
{
	switch (family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
#endif
	}
	return 0;
}



struct socket_info
{
	int fd;

	int family;
	int type;
	int protocol;
	int bound;
	int bcast;
	int is_server;
	int connected;
	int defer_connect;

	char *path;
	char *tmp_path;

	struct sockaddr *myname;
	socklen_t myname_len;

	struct sockaddr *peername;
	socklen_t peername_len;

	struct {
		unsigned long pck_snd;
		unsigned long pck_rcv;
	} io;

	struct socket_info *prev, *next;
};

static struct socket_info *sockets;

const char *socket_wrapper_dir(void)
{
	const char *s = getenv("SOCKET_WRAPPER_DIR");
	if (s == NULL) {
		return NULL;
	}
	if (strncmp(s, "./", 2) == 0) {
		s += 2;
	}
	return s;
}

unsigned int socket_wrapper_default_iface(void)
{
	const char *s = getenv("SOCKET_WRAPPER_DEFAULT_IFACE");
	if (s) {
		unsigned int iface;
		if (sscanf(s, "%u", &iface) == 1) {
			if (iface >= 1 && iface <= MAX_WRAPPED_INTERFACES) {
				return iface;
			}
		}
	}

	return 1;/* 127.0.0.1 */
}

static int convert_un_in(const struct sockaddr_un *un, struct sockaddr *in, socklen_t *len)
{
	unsigned int iface;
	unsigned int prt;
	const char *p;
	char type;

	p = strrchr(un->sun_path, '/');
	if (p) p++; else p = un->sun_path;

	if (sscanf(p, SOCKET_FORMAT, &type, &iface, &prt) != 3) {
		errno = EINVAL;
		return -1;
	}

	if (iface == 0 || iface > MAX_WRAPPED_INTERFACES) {
		errno = EINVAL;
		return -1;
	}

	if (prt > 0xFFFF) {
		errno = EINVAL;
		return -1;
	}

	switch(type) {
	case SOCKET_TYPE_CHAR_TCP:
	case SOCKET_TYPE_CHAR_UDP: {
		struct sockaddr_in *in2 = (struct sockaddr_in *)(void *)in;

		if ((*len) < sizeof(*in2)) {
		    errno = EINVAL;
		    return -1;
		}

		memset(in2, 0, sizeof(*in2));
		in2->sin_family = AF_INET;
		in2->sin_addr.s_addr = htonl((127<<24) | iface);
		in2->sin_port = htons(prt);

		*len = sizeof(*in2);
		break;
	}
#ifdef HAVE_IPV6
	case SOCKET_TYPE_CHAR_TCP_V6:
	case SOCKET_TYPE_CHAR_UDP_V6: {
		struct sockaddr_in6 *in2 = (struct sockaddr_in6 *)(void *)in;

		if ((*len) < sizeof(*in2)) {
			errno = EINVAL;
			return -1;
		}

		memset(in2, 0, sizeof(*in2));
		in2->sin6_family = AF_INET6;
		in2->sin6_addr = *swrap_ipv6();
		in2->sin6_addr.s6_addr[15] = iface;
		in2->sin6_port = htons(prt);

		*len = sizeof(*in2);
		break;
	}
#endif
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}

static int convert_in_un_remote(struct socket_info *si, const struct sockaddr *inaddr, struct sockaddr_un *un,
				int *bcast)
{
	char type = '\0';
	unsigned int prt;
	unsigned int iface;
	int is_bcast = 0;

	if (bcast) *bcast = 0;

	switch (inaddr->sa_family) {
	case AF_INET: {
		const struct sockaddr_in *in = 
		    (const struct sockaddr_in *)(const void *)inaddr;
		unsigned int addr = ntohl(in->sin_addr.s_addr);
		char u_type = '\0';
		char b_type = '\0';
		char a_type = '\0';

		switch (si->type) {
		case SOCK_STREAM:
			u_type = SOCKET_TYPE_CHAR_TCP;
			break;
		case SOCK_DGRAM:
			u_type = SOCKET_TYPE_CHAR_UDP;
			a_type = SOCKET_TYPE_CHAR_UDP;
			b_type = SOCKET_TYPE_CHAR_UDP;
			break;
		}

		prt = ntohs(in->sin_port);
		if (a_type && addr == 0xFFFFFFFF) {
			/* 255.255.255.255 only udp */
			is_bcast = 2;
			type = a_type;
			iface = socket_wrapper_default_iface();
		} else if (b_type && addr == 0x7FFFFFFF) {
			/* 127.255.255.255 only udp */
			is_bcast = 1;
			type = b_type;
			iface = socket_wrapper_default_iface();
		} else if ((addr & 0xFFFFFF00) == 0x7F000000) {
			/* 127.0.0.X */
			is_bcast = 0;
			type = u_type;
			iface = (addr & 0x000000FF);
		} else {
			errno = ENETUNREACH;
			return -1;
		}
		if (bcast) *bcast = is_bcast;
		break;
	}
#ifdef HAVE_IPV6
	case AF_INET6: {
		const struct sockaddr_in6 *in = 
		    (const struct sockaddr_in6 *)(const void *)inaddr;
		struct in6_addr cmp1, cmp2;

		switch (si->type) {
		case SOCK_STREAM:
			type = SOCKET_TYPE_CHAR_TCP_V6;
			break;
		case SOCK_DGRAM:
			type = SOCKET_TYPE_CHAR_UDP_V6;
			break;
		}

		/* XXX no multicast/broadcast */

		prt = ntohs(in->sin6_port);

		cmp1 = *swrap_ipv6();
		cmp2 = in->sin6_addr;
		cmp2.s6_addr[15] = 0;
		if (IN6_ARE_ADDR_EQUAL(&cmp1, &cmp2)) {
			iface = in->sin6_addr.s6_addr[15];
		} else {
			errno = ENETUNREACH;
			return -1;
		}

		break;
	}
#endif
	default:
		errno = ENETUNREACH;
		return -1;
	}

	if (prt == 0) {
		errno = EINVAL;
		return -1;
	}

	if (is_bcast) {
		snprintf(un->sun_path, sizeof(un->sun_path), "%s/EINVAL", 
			 socket_wrapper_dir());
		/* the caller need to do more processing */
		return 0;
	}

	snprintf(un->sun_path, sizeof(un->sun_path), "%s/"SOCKET_FORMAT, 
		 socket_wrapper_dir(), type, iface, prt);

	return 0;
}

static int convert_in_un_alloc(struct socket_info *si, const struct sockaddr *inaddr, struct sockaddr_un *un,
			       int *bcast)
{
	char type = '\0';
	unsigned int prt;
	unsigned int iface;
	struct stat st;
	int is_bcast = 0;

	if (bcast) *bcast = 0;

	switch (si->family) {
	case AF_INET: {
		const struct sockaddr_in *in = 
		    (const struct sockaddr_in *)(const void *)inaddr;
		unsigned int addr = ntohl(in->sin_addr.s_addr);
		char u_type = '\0';
		char d_type = '\0';
		char b_type = '\0';
		char a_type = '\0';

		prt = ntohs(in->sin_port);

		switch (si->type) {
		case SOCK_STREAM:
			u_type = SOCKET_TYPE_CHAR_TCP;
			d_type = SOCKET_TYPE_CHAR_TCP;
			break;
		case SOCK_DGRAM:
			u_type = SOCKET_TYPE_CHAR_UDP;
			d_type = SOCKET_TYPE_CHAR_UDP;
			a_type = SOCKET_TYPE_CHAR_UDP;
			b_type = SOCKET_TYPE_CHAR_UDP;
			break;
		}

		if (addr == 0) {
			/* 0.0.0.0 */
		 	is_bcast = 0;
			type = d_type;
			iface = socket_wrapper_default_iface();
		} else if (a_type && addr == 0xFFFFFFFF) {
			/* 255.255.255.255 only udp */
			is_bcast = 2;
			type = a_type;
			iface = socket_wrapper_default_iface();
		} else if (b_type && addr == 0x7FFFFFFF) {
			/* 127.255.255.255 only udp */
			is_bcast = 1;
			type = b_type;
			iface = socket_wrapper_default_iface();
		} else if ((addr & 0xFFFFFF00) == 0x7F000000) {
			/* 127.0.0.X */
			is_bcast = 0;
			type = u_type;
			iface = (addr & 0x000000FF);
		} else {
			errno = EADDRNOTAVAIL;
			return -1;
		}
		break;
	}
#ifdef HAVE_IPV6
	case AF_INET6: {
		const struct sockaddr_in6 *in = 
		    (const struct sockaddr_in6 *)(const void *)inaddr;
		struct in6_addr cmp1, cmp2;

		switch (si->type) {
		case SOCK_STREAM:
			type = SOCKET_TYPE_CHAR_TCP_V6;
			break;
		case SOCK_DGRAM:
			type = SOCKET_TYPE_CHAR_UDP_V6;
			break;
		}

		/* XXX no multicast/broadcast */

		prt = ntohs(in->sin6_port);

		cmp1 = *swrap_ipv6();
		cmp2 = in->sin6_addr;
		cmp2.s6_addr[15] = 0;
		if (IN6_IS_ADDR_UNSPECIFIED(&in->sin6_addr)) {
			iface = socket_wrapper_default_iface();
		} else if (IN6_ARE_ADDR_EQUAL(&cmp1, &cmp2)) {
			iface = in->sin6_addr.s6_addr[15];
		} else {
			errno = EADDRNOTAVAIL;
			return -1;
		}

		break;
	}
#endif
	default:
		errno = EADDRNOTAVAIL;
		return -1;
	}


	if (bcast) *bcast = is_bcast;

	if (prt == 0) {
		/* handle auto-allocation of ephemeral ports */
		for (prt = 5001; prt < 10000; prt++) {
			snprintf(un->sun_path, sizeof(un->sun_path), "%s/"SOCKET_FORMAT, 
				 socket_wrapper_dir(), type, iface, prt);
			if (stat(un->sun_path, &st) == 0) continue;

			set_port(si->family, prt, si->myname);
			break;
		}
		if (prt == 10000) {
			errno = ENFILE;
			return -1;
		}
	}

	snprintf(un->sun_path, sizeof(un->sun_path), "%s/"SOCKET_FORMAT, 
		 socket_wrapper_dir(), type, iface, prt);
	return 0;
}

static struct socket_info *find_socket_info(int fd)
{
	struct socket_info *i;
	for (i = sockets; i; i = i->next) {
		if (i->fd == fd) 
			return i;
	}

	return NULL;
}

static int sockaddr_convert_to_un(struct socket_info *si, const struct sockaddr *in_addr, socklen_t in_len, 
				  struct sockaddr_un *out_addr, int alloc_sock, int *bcast)
{
	struct sockaddr *out = (struct sockaddr *)(void *)out_addr;
	if (!out_addr)
		return 0;

	out->sa_family = AF_UNIX;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	out->sa_len = sizeof(*out_addr);
#endif

	switch (in_addr->sa_family) {
	case AF_INET:
#ifdef HAVE_IPV6
	case AF_INET6:
#endif
		switch (si->type) {
		case SOCK_STREAM:
		case SOCK_DGRAM:
			break;
		default:
			errno = ESOCKTNOSUPPORT;
			return -1;
		}
		if (alloc_sock) {
			return convert_in_un_alloc(si, in_addr, out_addr, bcast);
		} else {
			return convert_in_un_remote(si, in_addr, out_addr, bcast);
		}
	default:
		break;
	}

	errno = EAFNOSUPPORT;
	return -1;
}

static int sockaddr_convert_from_un(const struct socket_info *si, 
				    const struct sockaddr_un *in_addr, 
				    socklen_t un_addrlen,
				    int family,
				    struct sockaddr *out_addr,
				    socklen_t *out_addrlen)
{
	int ret;

	if (out_addr == NULL || out_addrlen == NULL) 
		return 0;

	if (un_addrlen == 0) {
		*out_addrlen = 0;
		return 0;
	}

	switch (family) {
	case AF_INET:
#ifdef HAVE_IPV6
	case AF_INET6:
#endif
		switch (si->type) {
		case SOCK_STREAM:
		case SOCK_DGRAM:
			break;
		default:
			errno = ESOCKTNOSUPPORT;
			return -1;
		}
		ret = convert_un_in(in_addr, out_addr, out_addrlen);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
		out_addr->sa_len = *out_addrlen;
#endif
		return ret;
	default:
		break;
	}

	errno = EAFNOSUPPORT;
	return -1;
}

enum swrap_packet_type {
	SWRAP_CONNECT_SEND,
	SWRAP_CONNECT_UNREACH,
	SWRAP_CONNECT_RECV,
	SWRAP_CONNECT_ACK,
	SWRAP_ACCEPT_SEND,
	SWRAP_ACCEPT_RECV,
	SWRAP_ACCEPT_ACK,
	SWRAP_RECVFROM,
	SWRAP_SENDTO,
	SWRAP_SENDTO_UNREACH,
	SWRAP_PENDING_RST,
	SWRAP_RECV,
	SWRAP_RECV_RST,
	SWRAP_SEND,
	SWRAP_SEND_RST,
	SWRAP_CLOSE_SEND,
	SWRAP_CLOSE_RECV,
	SWRAP_CLOSE_ACK,
};

struct swrap_file_hdr {
	uint32_t	magic;
	uint16_t	version_major;
	uint16_t	version_minor;
	int32_t		timezone;
	uint32_t	sigfigs;
	uint32_t	frame_max_len;
#define SWRAP_FRAME_LENGTH_MAX 0xFFFF
	uint32_t	link_type;
};
#define SWRAP_FILE_HDR_SIZE 24

struct swrap_packet_frame {
	uint32_t seconds;
	uint32_t micro_seconds;
	uint32_t recorded_length;
	uint32_t full_length;
};
#define SWRAP_PACKET_FRAME_SIZE 16

union swrap_packet_ip {
	struct {
		uint8_t		ver_hdrlen;
		uint8_t		tos;
		uint16_t	packet_length;
		uint16_t	identification;
		uint8_t		flags;
		uint8_t		fragment;
		uint8_t		ttl;
		uint8_t		protocol;
		uint16_t	hdr_checksum;
		uint32_t	src_addr;
		uint32_t	dest_addr;
	} v4;
#define SWRAP_PACKET_IP_V4_SIZE 20
	struct {
		uint8_t		ver_prio;
		uint8_t		flow_label_high;
		uint16_t	flow_label_low;
		uint16_t	payload_length;
		uint8_t		next_header;
		uint8_t		hop_limit;
		uint8_t		src_addr[16];
		uint8_t		dest_addr[16];
	} v6;
#define SWRAP_PACKET_IP_V6_SIZE 40
};
#define SWRAP_PACKET_IP_SIZE 40

union swrap_packet_payload {
	struct {
		uint16_t	source_port;
		uint16_t	dest_port;
		uint32_t	seq_num;
		uint32_t	ack_num;
		uint8_t		hdr_length;
		uint8_t		control;
		uint16_t	window;
		uint16_t	checksum;
		uint16_t	urg;
	} tcp;
#define SWRAP_PACKET_PAYLOAD_TCP_SIZE 20
	struct {
		uint16_t	source_port;
		uint16_t	dest_port;
		uint16_t	length;
		uint16_t	checksum;
	} udp;
#define SWRAP_PACKET_PAYLOAD_UDP_SIZE 8
	struct {
		uint8_t		type;
		uint8_t		code;
		uint16_t	checksum;
		uint32_t	unused;
	} icmp4;
#define SWRAP_PACKET_PAYLOAD_ICMP4_SIZE 8
	struct {
		uint8_t		type;
		uint8_t		code;
		uint16_t	checksum;
		uint32_t	unused;
	} icmp6;
#define SWRAP_PACKET_PAYLOAD_ICMP6_SIZE 8
};
#define SWRAP_PACKET_PAYLOAD_SIZE 20

#define SWRAP_PACKET_MIN_ALLOC \
	(SWRAP_PACKET_FRAME_SIZE + \
	 SWRAP_PACKET_IP_SIZE + \
	 SWRAP_PACKET_PAYLOAD_SIZE)

static const char *socket_wrapper_pcap_file(void)
{
	static int initialized = 0;
	static const char *s = NULL;
	static const struct swrap_file_hdr h;
	static const struct swrap_packet_frame f;
	static const union swrap_packet_ip i;
	static const union swrap_packet_payload p;

	if (initialized == 1) {
		return s;
	}
	initialized = 1;

	/*
	 * TODO: don't use the structs use plain buffer offsets
	 *       and PUSH_U8(), PUSH_U16() and PUSH_U32()
	 * 
	 * for now make sure we disable PCAP support
	 * if the struct has alignment!
	 */
	if (sizeof(h) != SWRAP_FILE_HDR_SIZE) {
		return NULL;
	}
	if (sizeof(f) != SWRAP_PACKET_FRAME_SIZE) {
		return NULL;
	}
	if (sizeof(i) != SWRAP_PACKET_IP_SIZE) {
		return NULL;
	}
	if (sizeof(i.v4) != SWRAP_PACKET_IP_V4_SIZE) {
		return NULL;
	}
	if (sizeof(i.v6) != SWRAP_PACKET_IP_V6_SIZE) {
		return NULL;
	}
	if (sizeof(p) != SWRAP_PACKET_PAYLOAD_SIZE) {
		return NULL;
	}
	if (sizeof(p.tcp) != SWRAP_PACKET_PAYLOAD_TCP_SIZE) {
		return NULL;
	}
	if (sizeof(p.udp) != SWRAP_PACKET_PAYLOAD_UDP_SIZE) {
		return NULL;
	}
	if (sizeof(p.icmp4) != SWRAP_PACKET_PAYLOAD_ICMP4_SIZE) {
		return NULL;
	}
	if (sizeof(p.icmp6) != SWRAP_PACKET_PAYLOAD_ICMP6_SIZE) {
		return NULL;
	}

	s = getenv("SOCKET_WRAPPER_PCAP_FILE");
	if (s == NULL) {
		return NULL;
	}
	if (strncmp(s, "./", 2) == 0) {
		s += 2;
	}
	return s;
}

static uint8_t *swrap_packet_init(struct timeval *tval,
				  const struct sockaddr *src,
				  const struct sockaddr *dest,
				  int socket_type,
				  const uint8_t *payload,
				  size_t payload_len,
				  unsigned long tcp_seqno,
				  unsigned long tcp_ack,
				  unsigned char tcp_ctl,
				  int unreachable,
				  size_t *_packet_len)
{
	uint8_t *base;
	uint8_t *buf;
	struct swrap_packet_frame *frame;
	union swrap_packet_ip *ip;
	union swrap_packet_payload *pay;
	size_t packet_len;
	size_t alloc_len;
	size_t nonwire_len = sizeof(*frame);
	size_t wire_hdr_len = 0;
	size_t wire_len = 0;
	size_t ip_hdr_len = 0;
	size_t icmp_hdr_len = 0;
	size_t icmp_truncate_len = 0;
	uint8_t protocol = 0, icmp_protocol = 0;
	const struct sockaddr_in *src_in = NULL;
	const struct sockaddr_in *dest_in = NULL;
#ifdef HAVE_IPV6
	const struct sockaddr_in6 *src_in6 = NULL;
	const struct sockaddr_in6 *dest_in6 = NULL;
#endif
	uint16_t src_port;
	uint16_t dest_port;

	switch (src->sa_family) {
	case AF_INET:
		src_in = (const struct sockaddr_in *)src;
		dest_in = (const struct sockaddr_in *)dest;
		src_port = src_in->sin_port;
		dest_port = dest_in->sin_port;
		ip_hdr_len = sizeof(ip->v4);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		src_in6 = (const struct sockaddr_in6 *)src;
		dest_in6 = (const struct sockaddr_in6 *)dest;
		src_port = src_in6->sin6_port;
		dest_port = dest_in6->sin6_port;
		ip_hdr_len = sizeof(ip->v6);
		break;
#endif
	default:
		return NULL;
	}

	switch (socket_type) {
	case SOCK_STREAM:
		protocol = 0x06; /* TCP */
		wire_hdr_len = ip_hdr_len + sizeof(pay->tcp);
		wire_len = wire_hdr_len + payload_len;
		break;

	case SOCK_DGRAM:
		protocol = 0x11; /* UDP */
		wire_hdr_len = ip_hdr_len + sizeof(pay->udp);
		wire_len = wire_hdr_len + payload_len;
		break;

	default:
		return NULL;
	}

	if (unreachable) {
		icmp_protocol = protocol;
		switch (src->sa_family) {
		case AF_INET:
			protocol = 0x01; /* ICMPv4 */
			icmp_hdr_len = ip_hdr_len + sizeof(pay->icmp4);
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			protocol = 0x3A; /* ICMPv6 */
			icmp_hdr_len = ip_hdr_len + sizeof(pay->icmp6);
			break;
#endif
		}
		if (wire_len > 64 ) {
			icmp_truncate_len = wire_len - 64;
		}
		wire_hdr_len += icmp_hdr_len;
		wire_len += icmp_hdr_len;
	}

	packet_len = nonwire_len + wire_len;
	alloc_len = packet_len;
	if (alloc_len < SWRAP_PACKET_MIN_ALLOC) {
		alloc_len = SWRAP_PACKET_MIN_ALLOC;
	}

	base = (uint8_t *)malloc(alloc_len);
	if (!base) return NULL;

	buf = base;

	frame = (struct swrap_packet_frame *)buf;
	frame->seconds		= tval->tv_sec;
	frame->micro_seconds	= tval->tv_usec;
	frame->recorded_length	= wire_len - icmp_truncate_len;
	frame->full_length	= wire_len - icmp_truncate_len;
	buf += SWRAP_PACKET_FRAME_SIZE;

	ip = (union swrap_packet_ip *)buf;
	switch (src->sa_family) {
	case AF_INET:
		ip->v4.ver_hdrlen	= 0x45; /* version 4 and 5 * 32 bit words */
		ip->v4.tos		= 0x00;
		ip->v4.packet_length	= htons(wire_len - icmp_truncate_len);
		ip->v4.identification	= htons(0xFFFF);
		ip->v4.flags		= 0x40; /* BIT 1 set - means don't fraqment */
		ip->v4.fragment		= htons(0x0000);
		ip->v4.ttl		= 0xFF;
		ip->v4.protocol		= protocol;
		ip->v4.hdr_checksum	= htons(0x0000);
		ip->v4.src_addr		= src_in->sin_addr.s_addr;
		ip->v4.dest_addr	= dest_in->sin_addr.s_addr;
		buf += SWRAP_PACKET_IP_V4_SIZE;
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		ip->v6.ver_prio		= 0x60; /* version 4 and 5 * 32 bit words */
		ip->v6.flow_label_high	= 0x00;
		ip->v6.flow_label_low	= 0x0000;
		ip->v6.payload_length	= htons(wire_len - icmp_truncate_len); /* TODO */
		ip->v6.next_header	= protocol;
		memcpy(ip->v6.src_addr, src_in6->sin6_addr.s6_addr, 16);
		memcpy(ip->v6.dest_addr, dest_in6->sin6_addr.s6_addr, 16);
		buf += SWRAP_PACKET_IP_V6_SIZE;
		break;
#endif
	}

	if (unreachable) {
		pay = (union swrap_packet_payload *)buf;
		switch (src->sa_family) {
		case AF_INET:
			pay->icmp4.type		= 0x03; /* destination unreachable */
			pay->icmp4.code		= 0x01; /* host unreachable */
			pay->icmp4.checksum	= htons(0x0000);
			pay->icmp4.unused	= htonl(0x00000000);
			buf += SWRAP_PACKET_PAYLOAD_ICMP4_SIZE;

			/* set the ip header in the ICMP payload */
			ip = (union swrap_packet_ip *)buf;
			ip->v4.ver_hdrlen	= 0x45; /* version 4 and 5 * 32 bit words */
			ip->v4.tos		= 0x00;
			ip->v4.packet_length	= htons(wire_len - icmp_hdr_len);
			ip->v4.identification	= htons(0xFFFF);
			ip->v4.flags		= 0x40; /* BIT 1 set - means don't fraqment */
			ip->v4.fragment		= htons(0x0000);
			ip->v4.ttl		= 0xFF;
			ip->v4.protocol		= icmp_protocol;
			ip->v4.hdr_checksum	= htons(0x0000);
			ip->v4.src_addr		= dest_in->sin_addr.s_addr;
			ip->v4.dest_addr	= src_in->sin_addr.s_addr;
			buf += SWRAP_PACKET_IP_V4_SIZE;

			src_port = dest_in->sin_port;
			dest_port = src_in->sin_port;
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			pay->icmp6.type		= 0x01; /* destination unreachable */
			pay->icmp6.code		= 0x03; /* address unreachable */
			pay->icmp6.checksum	= htons(0x0000);
			pay->icmp6.unused	= htonl(0x00000000);
			buf += SWRAP_PACKET_PAYLOAD_ICMP6_SIZE;

			/* set the ip header in the ICMP payload */
			ip = (union swrap_packet_ip *)buf;
			ip->v6.ver_prio		= 0x60; /* version 4 and 5 * 32 bit words */
			ip->v6.flow_label_high	= 0x00;
			ip->v6.flow_label_low	= 0x0000;
			ip->v6.payload_length	= htons(wire_len - icmp_truncate_len); /* TODO */
			ip->v6.next_header	= protocol;
			memcpy(ip->v6.src_addr, dest_in6->sin6_addr.s6_addr, 16);
			memcpy(ip->v6.dest_addr, src_in6->sin6_addr.s6_addr, 16);
			buf += SWRAP_PACKET_IP_V6_SIZE;

			src_port = dest_in6->sin6_port;
			dest_port = src_in6->sin6_port;
			break;
#endif
		}
	}

	pay = (union swrap_packet_payload *)buf;

	switch (socket_type) {
	case SOCK_STREAM:
		pay->tcp.source_port	= src_port;
		pay->tcp.dest_port	= dest_port;
		pay->tcp.seq_num	= htonl(tcp_seqno);
		pay->tcp.ack_num	= htonl(tcp_ack);
		pay->tcp.hdr_length	= 0x50; /* 5 * 32 bit words */
		pay->tcp.control	= tcp_ctl;
		pay->tcp.window		= htons(0x7FFF);
		pay->tcp.checksum	= htons(0x0000);
		pay->tcp.urg		= htons(0x0000);
		buf += SWRAP_PACKET_PAYLOAD_TCP_SIZE;

		break;

	case SOCK_DGRAM:
		pay->udp.source_port	= src_port;
		pay->udp.dest_port	= dest_port;
		pay->udp.length		= htons(8 + payload_len);
		pay->udp.checksum	= htons(0x0000);
		buf += SWRAP_PACKET_PAYLOAD_UDP_SIZE;

		break;
	}

	if (payload && payload_len > 0) {
		memcpy(buf, payload, payload_len);
	}

	*_packet_len = packet_len - icmp_truncate_len;
	return base;
}

static int swrap_get_pcap_fd(const char *fname)
{
	static int fd = -1;

	if (fd != -1) return fd;

	fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_APPEND, 0644);
	if (fd != -1) {
		struct swrap_file_hdr file_hdr;
		file_hdr.magic		= 0xA1B2C3D4;
		file_hdr.version_major	= 0x0002;	
		file_hdr.version_minor	= 0x0004;
		file_hdr.timezone	= 0x00000000;
		file_hdr.sigfigs	= 0x00000000;
		file_hdr.frame_max_len	= SWRAP_FRAME_LENGTH_MAX;
		file_hdr.link_type	= 0x0065; /* 101 RAW IP */

		if (write(fd, &file_hdr, sizeof(file_hdr)) != sizeof(file_hdr)) {
			close(fd);
			fd = -1;
		}
		return fd;
	}

	fd = open(fname, O_WRONLY|O_APPEND, 0644);

	return fd;
}

static uint8_t *swrap_marshall_packet(struct socket_info *si,
				      const struct sockaddr *addr,
				      enum swrap_packet_type type,
				      const void *buf, size_t len,
				      size_t *packet_len)
{
	const struct sockaddr *src_addr;
	const struct sockaddr *dest_addr;
	unsigned long tcp_seqno = 0;
	unsigned long tcp_ack = 0;
	unsigned char tcp_ctl = 0;
	int unreachable = 0;

	struct timeval tv;

	switch (si->family) {
	case AF_INET:
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		break;
#endif
	default:
		return NULL;
	}

	switch (type) {
	case SWRAP_CONNECT_SEND:
		if (si->type != SOCK_STREAM) return NULL;

		src_addr = si->myname;
		dest_addr = addr;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x02; /* SYN */

		si->io.pck_snd += 1;

		break;

	case SWRAP_CONNECT_RECV:
		if (si->type != SOCK_STREAM) return NULL;

		dest_addr = si->myname;
		src_addr = addr;

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x12; /** SYN,ACK */

		si->io.pck_rcv += 1;

		break;

	case SWRAP_CONNECT_UNREACH:
		if (si->type != SOCK_STREAM) return NULL;

		dest_addr = si->myname;
		src_addr = addr;

		/* Unreachable: resend the data of SWRAP_CONNECT_SEND */
		tcp_seqno = si->io.pck_snd - 1;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x02; /* SYN */
		unreachable = 1;

		break;

	case SWRAP_CONNECT_ACK:
		if (si->type != SOCK_STREAM) return NULL;

		src_addr = si->myname;
		dest_addr = addr;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x10; /* ACK */

		break;

	case SWRAP_ACCEPT_SEND:
		if (si->type != SOCK_STREAM) return NULL;

		dest_addr = si->myname;
		src_addr = addr;

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x02; /* SYN */

		si->io.pck_rcv += 1;

		break;

	case SWRAP_ACCEPT_RECV:
		if (si->type != SOCK_STREAM) return NULL;

		src_addr = si->myname;
		dest_addr = addr;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x12; /* SYN,ACK */

		si->io.pck_snd += 1;

		break;

	case SWRAP_ACCEPT_ACK:
		if (si->type != SOCK_STREAM) return NULL;

		dest_addr = si->myname;
		src_addr = addr;

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x10; /* ACK */

		break;

	case SWRAP_SEND:
		src_addr = si->myname;
		dest_addr = si->peername;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x18; /* PSH,ACK */

		si->io.pck_snd += len;

		break;

	case SWRAP_SEND_RST:
		dest_addr = si->myname;
		src_addr = si->peername;

		if (si->type == SOCK_DGRAM) {
			return swrap_marshall_packet(si, si->peername,
					  SWRAP_SENDTO_UNREACH,
			      		  buf, len, packet_len);
		}

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x14; /** RST,ACK */

		break;

	case SWRAP_PENDING_RST:
		dest_addr = si->myname;
		src_addr = si->peername;

		if (si->type == SOCK_DGRAM) {
			return NULL;
		}

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x14; /* RST,ACK */

		break;

	case SWRAP_RECV:
		dest_addr = si->myname;
		src_addr = si->peername;

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x18; /* PSH,ACK */

		si->io.pck_rcv += len;

		break;

	case SWRAP_RECV_RST:
		dest_addr = si->myname;
		src_addr = si->peername;

		if (si->type == SOCK_DGRAM) {
			return NULL;
		}

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x14; /* RST,ACK */

		break;

	case SWRAP_SENDTO:
		src_addr = si->myname;
		dest_addr = addr;

		si->io.pck_snd += len;

		break;

	case SWRAP_SENDTO_UNREACH:
		dest_addr = si->myname;
		src_addr = addr;

		unreachable = 1;

		break;

	case SWRAP_RECVFROM:
		dest_addr = si->myname;
		src_addr = addr;

		si->io.pck_rcv += len;

		break;

	case SWRAP_CLOSE_SEND:
		if (si->type != SOCK_STREAM) return NULL;

		src_addr = si->myname;
		dest_addr = si->peername;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x11; /* FIN, ACK */

		si->io.pck_snd += 1;

		break;

	case SWRAP_CLOSE_RECV:
		if (si->type != SOCK_STREAM) return NULL;

		dest_addr = si->myname;
		src_addr = si->peername;

		tcp_seqno = si->io.pck_rcv;
		tcp_ack = si->io.pck_snd;
		tcp_ctl = 0x11; /* FIN,ACK */

		si->io.pck_rcv += 1;

		break;

	case SWRAP_CLOSE_ACK:
		if (si->type != SOCK_STREAM) return NULL;

		src_addr = si->myname;
		dest_addr = si->peername;

		tcp_seqno = si->io.pck_snd;
		tcp_ack = si->io.pck_rcv;
		tcp_ctl = 0x10; /* ACK */

		break;
	default:
		return NULL;
	}

	swrapGetTimeOfDay(&tv);

	return swrap_packet_init(&tv, src_addr, dest_addr, si->type,
				 (const uint8_t *)buf, len,
				 tcp_seqno, tcp_ack, tcp_ctl, unreachable,
				 packet_len);
}

static void swrap_dump_packet(struct socket_info *si,
			      const struct sockaddr *addr,
			      enum swrap_packet_type type,
			      const void *buf, size_t len)
{
	const char *file_name;
	uint8_t *packet;
	size_t packet_len = 0;
	int fd;

	file_name = socket_wrapper_pcap_file();
	if (!file_name) {
		return;
	}

	packet = swrap_marshall_packet(si, addr, type, buf, len, &packet_len);
	if (!packet) {
		return;
	}

	fd = swrap_get_pcap_fd(file_name);
	if (fd != -1) {
		if (write(fd, packet, packet_len) != packet_len) {
			free(packet);
			return;
		}
	}

	free(packet);
}

_PUBLIC_ int swrap_socket(int family, int type, int protocol)
{
	struct socket_info *si;
	int fd;
	int real_type = type;
#ifdef SOCK_CLOEXEC
	real_type &= ~SOCK_CLOEXEC;
#endif
#ifdef SOCK_NONBLOCK
	real_type &= ~SOCK_NONBLOCK;
#endif

	if (!socket_wrapper_dir()) {
		return real_socket(family, type, protocol);
	}

	switch (family) {
	case AF_INET:
#ifdef HAVE_IPV6
	case AF_INET6:
#endif
		break;
	case AF_UNIX:
		return real_socket(family, type, protocol);
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	switch (real_type) {
	case SOCK_STREAM:
		break;
	case SOCK_DGRAM:
		break;
	default:
		errno = EPROTONOSUPPORT;
		return -1;
	}

	switch (protocol) {
	case 0:
		break;
	case 6:
		if (real_type == SOCK_STREAM) {
			break;
		}
		/*fall through*/
	case 17:
		if (real_type == SOCK_DGRAM) {
			break;
		}
		/*fall through*/
	default:
		errno = EPROTONOSUPPORT;
		return -1;
	}

	/* We must call real_socket with type, from the caller, not the version we removed
	   SOCK_CLOEXEC and SOCK_NONBLOCK from */
	fd = real_socket(AF_UNIX, type, 0);

	if (fd == -1) return -1;

	si = (struct socket_info *)calloc(1, sizeof(struct socket_info));

	si->family = family;

	/* however, the rest of the socket_wrapper code expects just
	 * the type, not the flags */
	si->type = real_type;
	si->protocol = protocol;
	si->fd = fd;

	SWRAP_DLIST_ADD(sockets, si);

	return si->fd;
}

_PUBLIC_ int swrap_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	struct socket_info *parent_si, *child_si;
	int fd;
	struct sockaddr_un un_addr;
	socklen_t un_addrlen = sizeof(un_addr);
	struct sockaddr_un un_my_addr;
	socklen_t un_my_addrlen = sizeof(un_my_addr);
	struct sockaddr *my_addr;
	socklen_t my_addrlen, len;
	int ret;

	parent_si = find_socket_info(s);
	if (!parent_si) {
		return real_accept(s, addr, addrlen);
	}

	/* 
	 * assume out sockaddr have the same size as the in parent
	 * socket family
	 */
	my_addrlen = socket_length(parent_si->family);
	if (my_addrlen <= 0) {
		errno = EINVAL;
		return -1;
	}

	my_addr = (struct sockaddr *)malloc(my_addrlen);
	if (my_addr == NULL) {
		return -1;
	}

	memset(&un_addr, 0, sizeof(un_addr));
	memset(&un_my_addr, 0, sizeof(un_my_addr));

	ret = real_accept(s, (struct sockaddr *)(void *)&un_addr, &un_addrlen);
	if (ret == -1) {
		free(my_addr);
		return ret;
	}

	fd = ret;

	len = my_addrlen;
	ret = sockaddr_convert_from_un(parent_si, &un_addr, un_addrlen,
				       parent_si->family, my_addr, &len);
	if (ret == -1) {
		free(my_addr);
		close(fd);
		return ret;
	}

	child_si = (struct socket_info *)malloc(sizeof(struct socket_info));
	memset(child_si, 0, sizeof(*child_si));

	child_si->fd = fd;
	child_si->family = parent_si->family;
	child_si->type = parent_si->type;
	child_si->protocol = parent_si->protocol;
	child_si->bound = 1;
	child_si->is_server = 1;
	child_si->connected = 1;

	child_si->peername_len = len;
	child_si->peername = sockaddr_dup(my_addr, len);

	if (addr != NULL && addrlen != NULL) {
		size_t copy_len = MIN(*addrlen, len);
		if (copy_len > 0) {
			memcpy(addr, my_addr, copy_len);
		}
		*addrlen = len;
	}

	ret = real_getsockname(fd, (struct sockaddr *)(void *)&un_my_addr,
			       &un_my_addrlen);
	if (ret == -1) {
		free(child_si);
		close(fd);
		return ret;
	}

	len = my_addrlen;
	ret = sockaddr_convert_from_un(child_si, &un_my_addr, un_my_addrlen,
				       child_si->family, my_addr, &len);
	if (ret == -1) {
		free(child_si);
		free(my_addr);
		close(fd);
		return ret;
	}

	child_si->myname_len = len;
	child_si->myname = sockaddr_dup(my_addr, len);
	free(my_addr);

	SWRAP_DLIST_ADD(sockets, child_si);

	swrap_dump_packet(child_si, addr, SWRAP_ACCEPT_SEND, NULL, 0);
	swrap_dump_packet(child_si, addr, SWRAP_ACCEPT_RECV, NULL, 0);
	swrap_dump_packet(child_si, addr, SWRAP_ACCEPT_ACK, NULL, 0);

	return fd;
}

static int autobind_start_init;
static int autobind_start;

/* using sendto() or connect() on an unbound socket would give the
   recipient no way to reply, as unlike UDP and TCP, a unix domain
   socket can't auto-assign emphemeral port numbers, so we need to
   assign it here.
   Note: this might change the family from ipv6 to ipv4
*/
static int swrap_auto_bind(struct socket_info *si, int family)
{
	struct sockaddr_un un_addr;
	int i;
	char type;
	int ret;
	int port;
	struct stat st;

	if (autobind_start_init != 1) {
		autobind_start_init = 1;
		autobind_start = getpid();
		autobind_start %= 50000;
		autobind_start += 10000;
	}

	un_addr.sun_family = AF_UNIX;

	switch (family) {
	case AF_INET: {
		struct sockaddr_in in;

		switch (si->type) {
		case SOCK_STREAM:
			type = SOCKET_TYPE_CHAR_TCP;
			break;
		case SOCK_DGRAM:
		    	type = SOCKET_TYPE_CHAR_UDP;
			break;
		default:
		    errno = ESOCKTNOSUPPORT;
		    return -1;
		}

		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		in.sin_addr.s_addr = htonl(127<<24 | 
					   socket_wrapper_default_iface());

		si->myname_len = sizeof(in);
		si->myname = sockaddr_dup(&in, si->myname_len);
		break;
	}
#ifdef HAVE_IPV6
	case AF_INET6: {
		struct sockaddr_in6 in6;

		if (si->family != family) {
			errno = ENETUNREACH;
			return -1;
		}

		switch (si->type) {
		case SOCK_STREAM:
			type = SOCKET_TYPE_CHAR_TCP_V6;
			break;
		case SOCK_DGRAM:
		    	type = SOCKET_TYPE_CHAR_UDP_V6;
			break;
		default:
			errno = ESOCKTNOSUPPORT;
			return -1;
		}

		memset(&in6, 0, sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_addr = *swrap_ipv6();
		in6.sin6_addr.s6_addr[15] = socket_wrapper_default_iface();
		si->myname_len = sizeof(in6);
		si->myname = sockaddr_dup(&in6, si->myname_len);
		break;
	}
#endif
	default:
		errno = ESOCKTNOSUPPORT;
		return -1;
	}

	if (autobind_start > 60000) {
		autobind_start = 10000;
	}

	for (i=0;i<1000;i++) {
		port = autobind_start + i;
		snprintf(un_addr.sun_path, sizeof(un_addr.sun_path), 
			 "%s/"SOCKET_FORMAT, socket_wrapper_dir(),
			 type, socket_wrapper_default_iface(), port);
		if (stat(un_addr.sun_path, &st) == 0) continue;

		ret = real_bind(si->fd, (struct sockaddr *)(void *)&un_addr,
				sizeof(un_addr));
		if (ret == -1) return ret;

		si->tmp_path = strdup(un_addr.sun_path);
		si->bound = 1;
		autobind_start = port + 1;
		break;
	}
	if (i == 1000) {
		errno = ENFILE;
		return -1;
	}

	si->family = family;
	set_port(si->family, port, si->myname);

	return 0;
}


_PUBLIC_ int swrap_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	int ret;
	struct sockaddr_un un_addr;
	struct socket_info *si = find_socket_info(s);
	int bcast = 0;

	if (!si) {
		return real_connect(s, serv_addr, addrlen);
	}

	if (si->bound == 0) {
		ret = swrap_auto_bind(si, serv_addr->sa_family);
		if (ret == -1) return -1;
	}

	if (si->family != serv_addr->sa_family) {
		errno = EINVAL;
		return -1;
	}

	ret = sockaddr_convert_to_un(si, serv_addr,
				     addrlen, &un_addr, 0, &bcast);
	if (ret == -1) return -1;

	if (bcast) {
		errno = ENETUNREACH;
		return -1;
	}

	if (si->type == SOCK_DGRAM) {
		si->defer_connect = 1;
		ret = 0;
	} else {
		swrap_dump_packet(si, serv_addr, SWRAP_CONNECT_SEND, NULL, 0);

		ret = real_connect(s, (struct sockaddr *)(void *)&un_addr,
				   sizeof(struct sockaddr_un));
	}

	/* to give better errors */
	if (ret == -1 && errno == ENOENT) {
		errno = EHOSTUNREACH;
	}

	if (ret == 0) {
		si->peername_len = addrlen;
		si->peername = sockaddr_dup(serv_addr, addrlen);
		si->connected = 1;

		swrap_dump_packet(si, serv_addr, SWRAP_CONNECT_RECV, NULL, 0);
		swrap_dump_packet(si, serv_addr, SWRAP_CONNECT_ACK, NULL, 0);
	} else {
		swrap_dump_packet(si, serv_addr, SWRAP_CONNECT_UNREACH, NULL, 0);
	}

	return ret;
}

_PUBLIC_ int swrap_bind(int s, const struct sockaddr *myaddr, socklen_t addrlen)
{
	int ret;
	struct sockaddr_un un_addr;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_bind(s, myaddr, addrlen);
	}

	si->myname_len = addrlen;
	si->myname = sockaddr_dup(myaddr, addrlen);

	ret = sockaddr_convert_to_un(si, myaddr, addrlen, &un_addr, 1, &si->bcast);
	if (ret == -1) return -1;

	unlink(un_addr.sun_path);

	ret = real_bind(s, (struct sockaddr *)(void *)&un_addr,
			sizeof(struct sockaddr_un));

	if (ret == 0) {
		si->bound = 1;
	}

	return ret;
}

_PUBLIC_ int swrap_listen(int s, int backlog)
{
	int ret;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_listen(s, backlog);
	}

	ret = real_listen(s, backlog);

	return ret;
}

_PUBLIC_ int swrap_getpeername(int s, struct sockaddr *name, socklen_t *addrlen)
{
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_getpeername(s, name, addrlen);
	}

	if (!si->peername)
	{
		errno = ENOTCONN;
		return -1;
	}

	memcpy(name, si->peername, si->peername_len);
	*addrlen = si->peername_len;

	return 0;
}

_PUBLIC_ int swrap_getsockname(int s, struct sockaddr *name, socklen_t *addrlen)
{
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_getsockname(s, name, addrlen);
	}

	memcpy(name, si->myname, si->myname_len);
	*addrlen = si->myname_len;

	return 0;
}

_PUBLIC_ int swrap_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_getsockopt(s, level, optname, optval, optlen);
	}

	if (level == SOL_SOCKET) {
		return real_getsockopt(s, level, optname, optval, optlen);
	} 

	errno = ENOPROTOOPT;
	return -1;
}

_PUBLIC_ int swrap_setsockopt(int s, int  level,  int  optname,  const  void  *optval, socklen_t optlen)
{
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_setsockopt(s, level, optname, optval, optlen);
	}

	if (level == SOL_SOCKET) {
		return real_setsockopt(s, level, optname, optval, optlen);
	}

	switch (si->family) {
	case AF_INET:
		return 0;
#ifdef HAVE_IPV6
	case AF_INET6:
		return 0;
#endif
	default:
		errno = ENOPROTOOPT;
		return -1;
	}
}

_PUBLIC_ int swrap_ioctl(int s, int r, void *p)
{
	int ret;
	struct socket_info *si = find_socket_info(s);
	int value;

	if (!si) {
		return real_ioctl(s, r, p);
	}

	ret = real_ioctl(s, r, p);

	switch (r) {
	case FIONREAD:
		value = *((int *)p);
		if (ret == -1 && errno != EAGAIN && errno != ENOBUFS) {
			swrap_dump_packet(si, NULL, SWRAP_PENDING_RST, NULL, 0);
		} else if (value == 0) { /* END OF FILE */
			swrap_dump_packet(si, NULL, SWRAP_PENDING_RST, NULL, 0);
		}
		break;
	}

	return ret;
}

static ssize_t swrap_sendmsg_before(struct socket_info *si,
				    struct msghdr *msg,
				    struct iovec *tmp_iov,
				    struct sockaddr_un *tmp_un,
				    const struct sockaddr_un **to_un,
				    const struct sockaddr **to,
				    int *bcast)
{
	size_t i, len = 0;
	ssize_t ret;

	if (to_un) {
		*to_un = NULL;
	}
	if (to) {
		*to = NULL;
	}
	if (bcast) {
		*bcast = 0;
	}

	switch (si->type) {
	case SOCK_STREAM:
		if (!si->connected) {
			errno = ENOTCONN;
			return -1;
		}

		if (msg->msg_iovlen == 0) {
			break;
		}

		/*
		 * cut down to 1500 byte packets for stream sockets,
		 * which makes it easier to format PCAP capture files
		 * (as the caller will simply continue from here)
		 */

		for (i=0; i < msg->msg_iovlen; i++) {
			size_t nlen;
			nlen = len + msg->msg_iov[i].iov_len;
			if (nlen > 1500) {
				break;
			}
		}
		msg->msg_iovlen = i;
		if (msg->msg_iovlen == 0) {
			*tmp_iov = msg->msg_iov[0];
			tmp_iov->iov_len = MIN(tmp_iov->iov_len, 1500);
			msg->msg_iov = tmp_iov;
			msg->msg_iovlen = 1;
		}
		break;

	case SOCK_DGRAM:
		if (si->connected) {
			if (msg->msg_name) {
				errno = EISCONN;
				return -1;
			}
		} else {
			const struct sockaddr *msg_name;
			msg_name = (const struct sockaddr *)msg->msg_name;

			if (msg_name == NULL) {
				errno = ENOTCONN;
				return -1;
			}


			ret = sockaddr_convert_to_un(si, msg_name, msg->msg_namelen,
						     tmp_un, 0, bcast);
			if (ret == -1) return -1;

			if (to_un) {
				*to_un = tmp_un;
			}
			if (to) {
				*to = msg_name;
			}
			msg->msg_name = tmp_un;
			msg->msg_namelen = sizeof(*tmp_un);
		}

		if (si->bound == 0) {
			ret = swrap_auto_bind(si, si->family);
			if (ret == -1) return -1;
		}

		if (!si->defer_connect) {
			break;
		}

		ret = sockaddr_convert_to_un(si, si->peername, si->peername_len,
					     tmp_un, 0, NULL);
		if (ret == -1) return -1;

		ret = real_connect(si->fd, (struct sockaddr *)(void *)tmp_un,
				   sizeof(*tmp_un));

		/* to give better errors */
		if (ret == -1 && errno == ENOENT) {
			errno = EHOSTUNREACH;
		}

		if (ret == -1) {
			return ret;
		}

		si->defer_connect = 0;
		break;
	default:
		errno = EHOSTUNREACH;
		return -1;
	}

	return 0;
}

static void swrap_sendmsg_after(struct socket_info *si,
				struct msghdr *msg,
				const struct sockaddr *to,
				ssize_t ret)
{
	int saved_errno = errno;
	size_t i, len = 0;
	uint8_t *buf;
	off_t ofs = 0;
	size_t avail = 0;
	size_t remain;

	/* to give better errors */
	if (ret == -1 && saved_errno == ENOENT) {
		saved_errno = EHOSTUNREACH;
	}

	for (i=0; i < msg->msg_iovlen; i++) {
		avail += msg->msg_iov[i].iov_len;
	}

	if (ret == -1) {
		remain = MIN(80, avail);
	} else {
		remain = ret;
	}

	/* we capture it as one single packet */
	buf = (uint8_t *)malloc(remain);
	if (!buf) {
		/* we just not capture the packet */
		errno = saved_errno;
		return;
	}

	for (i=0; i < msg->msg_iovlen; i++) {
		size_t this_time = MIN(remain, msg->msg_iov[i].iov_len);
		memcpy(buf + ofs,
		       msg->msg_iov[i].iov_base,
		       this_time);
		ofs += this_time;
		remain -= this_time;
	}
	len = ofs;

	switch (si->type) {
	case SOCK_STREAM:
		if (ret == -1) {
			swrap_dump_packet(si, NULL, SWRAP_SEND, buf, len);
			swrap_dump_packet(si, NULL, SWRAP_SEND_RST, NULL, 0);
		} else {
			swrap_dump_packet(si, NULL, SWRAP_SEND, buf, len);
		}
		break;

	case SOCK_DGRAM:
		if (si->connected) {
			to = si->peername;
		}
		if (ret == -1) {
			swrap_dump_packet(si, to, SWRAP_SENDTO, buf, len);
			swrap_dump_packet(si, to, SWRAP_SENDTO_UNREACH, buf, len);
		} else {
			swrap_dump_packet(si, to, SWRAP_SENDTO, buf, len);
		}
		break;
	}

	free(buf);
	errno = saved_errno;
}

_PUBLIC_ ssize_t swrap_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	struct sockaddr_un un_addr;
	socklen_t un_addrlen = sizeof(un_addr);
	int ret;
	struct socket_info *si = find_socket_info(s);
	struct sockaddr_storage ss;
	socklen_t ss_len = sizeof(ss);

	if (!si) {
		return real_recvfrom(s, buf, len, flags, from, fromlen);
	}

	if (!from) {
		from = (struct sockaddr *)(void *)&ss;
		fromlen = &ss_len;
	}

	if (si->type == SOCK_STREAM) {
		/* cut down to 1500 byte packets for stream sockets,
		 * which makes it easier to format PCAP capture files
		 * (as the caller will simply continue from here) */
		len = MIN(len, 1500);
	}

	/* irix 6.4 forgets to null terminate the sun_path string :-( */
	memset(&un_addr, 0, sizeof(un_addr));
	ret = real_recvfrom(s, buf, len, flags,
			    (struct sockaddr *)(void *)&un_addr, &un_addrlen);
	if (ret == -1) 
		return ret;

	if (sockaddr_convert_from_un(si, &un_addr, un_addrlen,
				     si->family, from, fromlen) == -1) {
		return -1;
	}

	swrap_dump_packet(si, from, SWRAP_RECVFROM, buf, ret);

	return ret;
}


_PUBLIC_ ssize_t swrap_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	struct msghdr msg;
	struct iovec tmp;
	struct sockaddr_un un_addr;
	const struct sockaddr_un *to_un = NULL;
	ssize_t ret;
	struct socket_info *si = find_socket_info(s);
	int bcast = 0;

	if (!si) {
		return real_sendto(s, buf, len, flags, to, tolen);
	}

	tmp.iov_base = discard_const_p(char, buf);
	tmp.iov_len = len;

	ZERO_STRUCT(msg);
	msg.msg_name = discard_const_p(struct sockaddr, to); /* optional address */
	msg.msg_namelen = tolen;       /* size of address */
	msg.msg_iov = &tmp;            /* scatter/gather array */
	msg.msg_iovlen = 1;            /* # elements in msg_iov */
#if 0 /* not available on solaris */
	msg.msg_control = NULL;        /* ancillary data, see below */
	msg.msg_controllen = 0;        /* ancillary data buffer len */
	msg.msg_flags = 0;             /* flags on received message */
#endif

	ret = swrap_sendmsg_before(si, &msg, &tmp, &un_addr, &to_un, &to, &bcast);
	if (ret == -1) return -1;

	buf = msg.msg_iov[0].iov_base;
	len = msg.msg_iov[0].iov_len;

	if (bcast) {
		struct stat st;
		unsigned int iface;
		unsigned int prt = ntohs(((const struct sockaddr_in *)to)->sin_port);
		char type;

		type = SOCKET_TYPE_CHAR_UDP;

		for(iface=0; iface <= MAX_WRAPPED_INTERFACES; iface++) {
			snprintf(un_addr.sun_path, sizeof(un_addr.sun_path), "%s/"SOCKET_FORMAT,
				 socket_wrapper_dir(), type, iface, prt);
			if (stat(un_addr.sun_path, &st) != 0) continue;

			/* ignore the any errors in broadcast sends */
			real_sendto(s, buf, len, flags,
				    (struct sockaddr *)(void *)&un_addr,
				    sizeof(un_addr));
		}

		swrap_dump_packet(si, to, SWRAP_SENDTO, buf, len);

		return len;
	}

	ret = real_sendto(s, buf, len, flags, msg.msg_name, msg.msg_namelen);

	swrap_sendmsg_after(si, &msg, to, ret);

	return ret;
}

_PUBLIC_ ssize_t swrap_recv(int s, void *buf, size_t len, int flags)
{
	int ret;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_recv(s, buf, len, flags);
	}

	if (si->type == SOCK_STREAM) {
		/* cut down to 1500 byte packets for stream sockets,
		 * which makes it easier to format PCAP capture files
		 * (as the caller will simply continue from here) */
		len = MIN(len, 1500);
	}

	ret = real_recv(s, buf, len, flags);
	if (ret == -1 && errno != EAGAIN && errno != ENOBUFS) {
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret == 0) { /* END OF FILE */
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret > 0) {
		swrap_dump_packet(si, NULL, SWRAP_RECV, buf, ret);
	}

	return ret;
}

_PUBLIC_ ssize_t swrap_read(int s, void *buf, size_t len)
{
	int ret;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_read(s, buf, len);
	}

	if (si->type == SOCK_STREAM) {
		/* cut down to 1500 byte packets for stream sockets,
		 * which makes it easier to format PCAP capture files
		 * (as the caller will simply continue from here) */
		len = MIN(len, 1500);
	}

	ret = real_read(s, buf, len);
	if (ret == -1 && errno != EAGAIN && errno != ENOBUFS) {
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret == 0) { /* END OF FILE */
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret > 0) {
		swrap_dump_packet(si, NULL, SWRAP_RECV, buf, ret);
	}

	return ret;
}


_PUBLIC_ ssize_t swrap_send(int s, const void *buf, size_t len, int flags)
{
	struct msghdr msg;
	struct iovec tmp;
	struct sockaddr_un un_addr;
	ssize_t ret;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_send(s, buf, len, flags);
	}

	tmp.iov_base = discard_const_p(char, buf);
	tmp.iov_len = len;

	ZERO_STRUCT(msg);
	msg.msg_name = NULL;           /* optional address */
	msg.msg_namelen = 0;           /* size of address */
	msg.msg_iov = &tmp;            /* scatter/gather array */
	msg.msg_iovlen = 1;            /* # elements in msg_iov */
#if 0 /* not available on solaris */
	msg.msg_control = NULL;        /* ancillary data, see below */
	msg.msg_controllen = 0;        /* ancillary data buffer len */
	msg.msg_flags = 0;             /* flags on received message */
#endif

	ret = swrap_sendmsg_before(si, &msg, &tmp, &un_addr, NULL, NULL, NULL);
	if (ret == -1) return -1;

	buf = msg.msg_iov[0].iov_base;
	len = msg.msg_iov[0].iov_len;

	ret = real_send(s, buf, len, flags);

	swrap_sendmsg_after(si, &msg, NULL, ret);

	return ret;
}

_PUBLIC_ ssize_t swrap_sendmsg(int s, const struct msghdr *omsg, int flags)
{
	struct msghdr msg;
	struct iovec tmp;
	struct sockaddr_un un_addr;
	const struct sockaddr_un *to_un = NULL;
	const struct sockaddr *to = NULL;
	ssize_t ret;
	struct socket_info *si = find_socket_info(s);
	int bcast = 0;

	if (!si) {
		return real_sendmsg(s, omsg, flags);
	}

	tmp.iov_base = NULL;
	tmp.iov_len = 0;

	msg = *omsg;
#if 0
	msg.msg_name = omsg->msg_name;             /* optional address */
	msg.msg_namelen = omsg->msg_namelen;       /* size of address */
	msg.msg_iov = omsg->msg_iov;               /* scatter/gather array */
	msg.msg_iovlen = omsg->msg_iovlen;         /* # elements in msg_iov */
	/* the following is not available on solaris */
	msg.msg_control = omsg->msg_control;       /* ancillary data, see below */
	msg.msg_controllen = omsg->msg_controllen; /* ancillary data buffer len */
	msg.msg_flags = omsg->msg_flags;           /* flags on received message */
#endif

	ret = swrap_sendmsg_before(si, &msg, &tmp, &un_addr, &to_un, &to, &bcast);
	if (ret == -1) return -1;

	if (bcast) {
		struct stat st;
		unsigned int iface;
		unsigned int prt = ntohs(((const struct sockaddr_in *)to)->sin_port);
		char type;
		size_t i, len = 0;
		uint8_t *buf;
		off_t ofs = 0;
		size_t avail = 0;
		size_t remain;

		for (i=0; i < msg.msg_iovlen; i++) {
			avail += msg.msg_iov[i].iov_len;
		}

		len = avail;
		remain = avail;

		/* we capture it as one single packet */
		buf = (uint8_t *)malloc(remain);
		if (!buf) {
			return -1;
		}

		for (i=0; i < msg.msg_iovlen; i++) {
			size_t this_time = MIN(remain, msg.msg_iov[i].iov_len);
			memcpy(buf + ofs,
			       msg.msg_iov[i].iov_base,
			       this_time);
			ofs += this_time;
			remain -= this_time;
		}

		type = SOCKET_TYPE_CHAR_UDP;

		for(iface=0; iface <= MAX_WRAPPED_INTERFACES; iface++) {
			snprintf(un_addr.sun_path, sizeof(un_addr.sun_path), "%s/"SOCKET_FORMAT,
				 socket_wrapper_dir(), type, iface, prt);
			if (stat(un_addr.sun_path, &st) != 0) continue;

			msg.msg_name = &un_addr;           /* optional address */
			msg.msg_namelen = sizeof(un_addr); /* size of address */

			/* ignore the any errors in broadcast sends */
			real_sendmsg(s, &msg, flags);
		}

		swrap_dump_packet(si, to, SWRAP_SENDTO, buf, len);
		free(buf);

		return len;
	}

	ret = real_sendmsg(s, &msg, flags);

	swrap_sendmsg_after(si, &msg, to, ret);

	return ret;
}

int swrap_readv(int s, const struct iovec *vector, size_t count)
{
	int ret;
	struct socket_info *si = find_socket_info(s);
	struct iovec v;

	if (!si) {
		return real_readv(s, vector, count);
	}

	if (!si->connected) {
		errno = ENOTCONN;
		return -1;
	}

	if (si->type == SOCK_STREAM && count > 0) {
		/* cut down to 1500 byte packets for stream sockets,
		 * which makes it easier to format PCAP capture files
		 * (as the caller will simply continue from here) */
		size_t i, len = 0;

		for (i=0; i < count; i++) {
			size_t nlen;
			nlen = len + vector[i].iov_len;
			if (nlen > 1500) {
				break;
			}
		}
		count = i;
		if (count == 0) {
			v = vector[0];
			v.iov_len = MIN(v.iov_len, 1500);
			vector = &v;
			count = 1;
		}
	}

	ret = real_readv(s, vector, count);
	if (ret == -1 && errno != EAGAIN && errno != ENOBUFS) {
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret == 0) { /* END OF FILE */
		swrap_dump_packet(si, NULL, SWRAP_RECV_RST, NULL, 0);
	} else if (ret > 0) {
		uint8_t *buf;
		off_t ofs = 0;
		size_t i;
		size_t remain = ret;

		/* we capture it as one single packet */
		buf = (uint8_t *)malloc(ret);
		if (!buf) {
			/* we just not capture the packet */
			errno = 0;
			return ret;
		}

		for (i=0; i < count; i++) {
			size_t this_time = MIN(remain, vector[i].iov_len);
			memcpy(buf + ofs,
			       vector[i].iov_base,
			       this_time);
			ofs += this_time;
			remain -= this_time;
		}

		swrap_dump_packet(si, NULL, SWRAP_RECV, buf, ret);
		free(buf);
	}

	return ret;
}

int swrap_writev(int s, const struct iovec *vector, size_t count)
{
	struct msghdr msg;
	struct iovec tmp;
	struct sockaddr_un un_addr;
	ssize_t ret;
	struct socket_info *si = find_socket_info(s);

	if (!si) {
		return real_writev(s, vector, count);
	}

	tmp.iov_base = NULL;
	tmp.iov_len = 0;

	ZERO_STRUCT(msg);
	msg.msg_name = NULL;           /* optional address */
	msg.msg_namelen = 0;           /* size of address */
	msg.msg_iov = discard_const_p(struct iovec, vector); /* scatter/gather array */
	msg.msg_iovlen = count;        /* # elements in msg_iov */
#if 0 /* not available on solaris */
	msg.msg_control = NULL;        /* ancillary data, see below */
	msg.msg_controllen = 0;        /* ancillary data buffer len */
	msg.msg_flags = 0;             /* flags on received message */
#endif

	ret = swrap_sendmsg_before(si, &msg, &tmp, &un_addr, NULL, NULL, NULL);
	if (ret == -1) return -1;

	ret = real_writev(s, msg.msg_iov, msg.msg_iovlen);

	swrap_sendmsg_after(si, &msg, NULL, ret);

	return ret;
}

_PUBLIC_ int swrap_close(int fd)
{
	struct socket_info *si = find_socket_info(fd);
	int ret;

	if (!si) {
		return real_close(fd);
	}

	SWRAP_DLIST_REMOVE(sockets, si);

	if (si->myname && si->peername) {
		swrap_dump_packet(si, NULL, SWRAP_CLOSE_SEND, NULL, 0);
	}

	ret = real_close(fd);

	if (si->myname && si->peername) {
		swrap_dump_packet(si, NULL, SWRAP_CLOSE_RECV, NULL, 0);
		swrap_dump_packet(si, NULL, SWRAP_CLOSE_ACK, NULL, 0);
	}

	if (si->path) free(si->path);
	if (si->myname) free(si->myname);
	if (si->peername) free(si->peername);
	if (si->tmp_path) {
		unlink(si->tmp_path);
		free(si->tmp_path);
	}
	free(si);

	return ret;
}
