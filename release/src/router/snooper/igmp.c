/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <endian.h>
#include <asm/byteorder.h>

#include "snooper.h"

#ifdef DEBUG_IGMP
#define log_igmp(fmt, args...) log_debug("%s::" fmt, "igmp", ##args)
#else
#define log_igmp(...) do {} while(0)
#endif

struct ipopt_ra {
	uint8_t ipopt_type;
	uint8_t ipopt_len;
	uint16_t ipopt_data;
} __attribute__((packed));

struct igmp3_grec {
	uint8_t grec_type;
	uint8_t grec_auxwords;
	uint16_t grec_nsrcs;
	struct in_addr grec_mca;
	struct in_addr grec_src[0];
} __attribute__((packed));

struct igmp3_report {
	uint8_t igmp_type;
	uint8_t igmp_resv1;
	uint16_t igmp_cksum;
	uint16_t igmp_resv2;
	uint16_t igmp_ngrec;
	struct igmp3_grec igmp_grec[0];
} __attribute__((packed));

struct igmp3_query {
	uint8_t igmp_type;
	uint8_t igmp_code;
	uint16_t igmp_cksum;
	struct in_addr igmp_group;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t igmp_qrv:3,
		igmp_suppress:1,
		igmp_resv:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t igmp_resv:4,
		igmp_suppress:1,
		igmp_qrv:3;
#else
#error "Please fix <bits/endian.h>"
#endif
	uint8_t igmp_qqic;
	uint16_t igmp_nsrcs;
	struct in_addr igmp_srcs[0];
} __attribute__((packed));

#define ROBUSTNESS           2
#define QUERY_INT            125
#define QUERY_RESPONSE_INT   10
#define GROUP_MEMBERSHIP_INT (ROBUSTNESS * QUERY_INT + QUERY_RESPONSE_INT)
#define QUERIER_PRESENT_INT  (ROBUSTNESS * QUERY_INT + QUERY_RESPONSE_INT / 2)
#define STARTUP_QUERY_INT    (QUERY_INT / 4)
#define STARTUP_QUERY_CNT    (ROBUSTNESS)
#define LASTMEMBER_QUERY_INT 1
#define LASTMEMBER_QUERY_CNT (ROBUSTNESS)
#define OLD_VER_PRESENT_INT  (ROBUSTNESS * QUERY_INT + QUERY_RESPONSE_INT)

#define IGMP3_MASK(value, nb) ((nb) >= 32 ? (value) : ((1 << (nb)) - 1) & (value))
#define IGMP3_EXP(thresh, nbmant, nbexp, value) \
	((value) < (thresh) ? (value) : \
        ((IGMP3_MASK(value, nbmant) | (1 << (nbmant))) << \
         (IGMP3_MASK((value) >> (nbmant), nbexp) + (nbexp))))

#define IGMP3_QQIC(value) IGMP3_EXP(0x80, 4, 3, value)
#define IGMP3_MRC(value) IGMP3_EXP(0x80, 4, 3, value)

#define IGMP3_MODE_IS_INCLUDE   1
#define IGMP3_MODE_IS_EXCLUDE   2
#define IGMP3_CHANGE_TO_INCLUDE 3
#define IGMP3_CHANGE_TO_EXCLUDE 4
#define IGMP3_ALLOW_NEW_SOURCES 5
#define IGMP3_BLOCK_OLD_SOURCES 6

#define IGMP_V3_MEMBERSHIP_REPORT 0x22

static struct {
	int enabled:1;
	int version;
	int startup;
	struct timer_entry timer;
} querier;

static int reserved_mcast(in_addr_t group)
{
	static const struct {
		in_addr_t addr;
		in_addr_t mask;
	} reserved[] = {
		{ __constant_htonl(0xe0000000), __constant_htonl(0xffffff00) },
		{ __constant_htonl(0xe000ff87), __constant_htonl(0xffffffff) },
		{ __constant_htonl(0xeffffffa), __constant_htonl(0xffffffff) }
	};
	unsigned char ea1[ETHER_ADDR_LEN];
	unsigned char ea2[ETHER_ADDR_LEN];
	unsigned int i;

	for (i = 0; i < sizeof(reserved)/sizeof(reserved[0]); i++) {
		ether_mtoe(reserved[i].addr, ea1);
		ether_mtoe(reserved[i].mask & group, ea2);
		if (memcmp(ea1, ea2, sizeof(ea1)) == 0)
			return 1;
	}

	return 0;
}

static int inet_cksum(void *data, int len)
{
	register int nleft = len;
	register uint16_t *ptr = data;
	register int32_t sum = 0;
	uint16_t answer = 0;

	while (nleft > 1) {
		sum += *ptr++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(uint8_t *) (&answer) = *(uint8_t *) ptr ;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;

	return answer;
}

#ifdef DEBUG_IGMP
static void log_packet(char *type, in_addr_t group, unsigned char *shost, struct ip *iph, struct igmp *igmp, int loopback)
{
	unsigned char dhost[ETHER_ADDR_LEN];
#ifndef DEBUG_IGMP_MORE
	if ((igmp->igmp_type != IGMP_MEMBERSHIP_QUERY) &&
	    (!IN_MULTICAST(ntohl(group)) || reserved_mcast(group)))
		return;
#endif
	ether_mtoe(iph->ip_dst.s_addr, dhost);
	log_igmp("%-7s [" FMT_EA "] " FMT_IP " => [" FMT_EA "] " FMT_IP " <" FMT_IP "> %s", type,
	    ARG_EA(shost), ARG_IP(&iph->ip_src.s_addr),
	    ARG_EA(dhost), ARG_IP(&iph->ip_dst.s_addr),
	    ARG_IP(&group), loopback ? "*" : "");
}
#else
#define log_packet(...) do {} while (0);
#endif

int build_query(unsigned char *packet, int size, in_addr_t group, in_addr_t dst)
{
	unsigned char *ptr = packet, *end = packet + size;
	struct ip *iph;
	struct ipopt_ra *raopt;
	union {
		struct igmp *igmp;
		struct igmp3_query *query3;
	} igmp;
	int igmp_len, len;

	memset(packet, 0, size);

	if (ptr > end - sizeof(*iph))
		return -1;
	iph = (struct ip *) ptr;
	iph->ip_v = IPVERSION;
/* 	iph->ip_hl = 0; */
	iph->ip_tos = 0xc0;
/* 	iph->ip_len = 0; */
	iph->ip_id = random();
	iph->ip_off = 0;
	iph->ip_ttl = 1;
	iph->ip_p = IPPROTO_IGMP;
/* 	iph->ip_sum = 0; */
	iph->ip_src.s_addr = ifaddr;
	iph->ip_dst.s_addr = dst;
	ptr += sizeof(*iph);

	if (ptr > end - sizeof(*raopt))
		return -1;
	raopt = (struct ipopt_ra *) ptr;
	raopt->ipopt_type = IPOPT_RA;
	raopt->ipopt_len = sizeof(*raopt);
	ptr += sizeof(*raopt);

	iph->ip_hl = (ptr - (unsigned char *) iph + 3) >> 2;
	ptr = (unsigned char *) iph + (iph->ip_hl << 2);

	igmp_len = (querier.version < 3) ? sizeof(*igmp.igmp) : sizeof(*igmp.query3);
	if (ptr > end - igmp_len)
		return -1;
	igmp.igmp = (struct igmp *) ptr;
	igmp.igmp->igmp_type = IGMP_MEMBERSHIP_QUERY;
	igmp.igmp->igmp_code = (querier.version < 2) ? 0 :
	    (group ? LASTMEMBER_QUERY_INT : QUERY_RESPONSE_INT) * 10;
	igmp.igmp->igmp_group.s_addr = group;
	if (querier.version == 3) {
		igmp.query3->igmp_qrv = ROBUSTNESS;
		igmp.query3->igmp_qqic = QUERY_INT;
	}
	igmp.igmp->igmp_cksum = inet_cksum(igmp.igmp, igmp_len);
	ptr += igmp_len;

	len = ptr - (unsigned char *) iph;
	iph->ip_len = htons(len);
	iph->ip_sum = inet_cksum(iph, len);

	return ptr - packet;
}

static int accept_join(in_addr_t group, in_addr_t host, unsigned char *shost, int loopback)
{
	unsigned char ea[ETHER_ADDR_LEN];
	int port;

	if (loopback || !IN_MULTICAST(ntohl(group)) || reserved_mcast(group))
		return 0;

	port = get_port(shost);
	if (port < 0)
		return 0;
	ether_mtoe(group, ea);
	add_member(ea, host, port, GROUP_MEMBERSHIP_INT * TIMER_HZ);

	return 1;
}

static int accept_leave(in_addr_t group, in_addr_t host, unsigned char *shost, int loopback)
{
	unsigned char ea[ETHER_ADDR_LEN];
	int port;

	if (loopback || !IN_MULTICAST(ntohl(group)) || reserved_mcast(group))
		return 0;

	ether_mtoe(group, ea);

	if (querier.enabled) {
		send_query(group);
		expire_members(ea, LASTMEMBER_QUERY_INT * TIMER_HZ);
	}

	port = get_port(shost);
	if (port < 0)
		return 0;
	del_member(ea, host, port);

	return 1;
}

int accept_query(in_addr_t group, in_addr_t host, unsigned char *shost, int timeout, int loopback)
{
	unsigned char ea[ETHER_ADDR_LEN];
	int port;

	if (loopback < 0 || (group && !IN_MULTICAST(ntohl(group))))
		return 0;

	if (host && (ifaddr ? host <= ifaddr : 1)) {
		querier.enabled = 0;
		mod_timer(&querier.timer, now() + QUERIER_PRESENT_INT * TIMER_HZ);
	}

	if (!querier.enabled && group && !reserved_mcast(group)) {
		ether_mtoe(group, ea);
		expire_members(ea, timeout);
	}

	if (loopback)
		return 1;

	port = get_port(shost);
	if (port < 0)
		return 0;
	add_router(host, port, QUERIER_PRESENT_INT * TIMER_HZ);

	return 1;
}

int accept_igmp(unsigned char *packet, int size, unsigned char *shost, int loopback)
{
	unsigned char *ptr = packet, *end = packet + size;
	struct ip *iph;
	union {
		struct igmp *igmp;
		struct igmp3_report *report3;
		struct igmp3_query *query3;
	} igmp;
	struct igmp3_grec *grec;
	unsigned int igmp_len, ngrec, nsrcs, timeout;

	iph = (struct ip *) ptr;
	if (ptr > end - sizeof(*iph) ||
	    iph->ip_p != IPPROTO_IGMP)
		return 0;
	ptr += iph->ip_hl << 2;
	end = MIN(end, (unsigned char *) iph + ntohs(iph->ip_len));

	igmp.igmp = (struct igmp *) ptr;
	igmp_len = end - ptr;
	if (igmp_len < sizeof(*igmp.igmp))
		return 0;
	switch (igmp.igmp->igmp_type) {
	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		if (igmp_len != sizeof(*igmp.igmp))
			return 0;
		log_packet((igmp.igmp->igmp_type == IGMP_V2_MEMBERSHIP_REPORT) ? "report2" : "report1",
		    igmp.igmp->igmp_group.s_addr, shost, iph, igmp.igmp, loopback);
		accept_join(igmp.igmp->igmp_group.s_addr, iph->ip_src.s_addr, shost, loopback);
		break;
	case IGMP_V2_LEAVE_GROUP:
		if (igmp_len != sizeof(*igmp.igmp))
			return 0;
		log_packet("leave2",
		    igmp.igmp->igmp_group.s_addr, shost, iph, igmp.igmp, loopback);
		accept_leave(igmp.igmp->igmp_group.s_addr, iph->ip_src.s_addr, shost, loopback);
		break;
	case IGMP_V3_MEMBERSHIP_REPORT:
		if (igmp_len < sizeof(*igmp.report3))
			return 0;
		ptr = (unsigned char *) igmp.report3->igmp_grec;
		ngrec = ntohs(igmp.report3->igmp_ngrec);
		while (ngrec-- > 0) {
			grec = (struct igmp3_grec *) ptr;
			if (ptr > end - sizeof(*grec))
				break;
			nsrcs = ntohs(grec->grec_nsrcs);
			switch (grec->grec_type) {
			case IGMP3_ALLOW_NEW_SOURCES:
				if (nsrcs == 0)
					break;
				/* else fall through */
			case IGMP3_MODE_IS_INCLUDE:
			case IGMP3_CHANGE_TO_INCLUDE:
				if (nsrcs == 0) {
					log_packet("leave3",
					    grec->grec_mca.s_addr, shost, iph, igmp.igmp, loopback);
					accept_leave(grec->grec_mca.s_addr, iph->ip_src.s_addr, shost, loopback);
					break;
				} /* else fall through */
			case IGMP3_MODE_IS_EXCLUDE:
			case IGMP3_CHANGE_TO_EXCLUDE:
				log_packet("report3",
				    grec->grec_mca.s_addr, shost, iph, igmp.igmp, loopback);
				accept_join(grec->grec_mca.s_addr, iph->ip_src.s_addr, shost, loopback);
				break;
			case IGMP3_BLOCK_OLD_SOURCES:
				break;
			}
			ptr = (unsigned char *) &grec->grec_src[nsrcs] + grec->grec_auxwords * 4;
		}
		break;
	case IGMP_MEMBERSHIP_QUERY:
		if (igmp_len == sizeof(*igmp.igmp))
			timeout = igmp.igmp->igmp_code ?
			    igmp.igmp->igmp_code * TIMER_HZ / 10 :
			    QUERY_RESPONSE_INT * TIMER_HZ;
		else if (igmp_len >= sizeof(*igmp.query3))
			timeout = IGMP3_MRC(igmp.igmp->igmp_code) * TIMER_HZ / 10;
		else
			return 0;
		log_packet((igmp_len >= sizeof(*igmp.query3)) ? "query3" :
		    igmp.igmp->igmp_code ? "query2" : "query1",
		    igmp.igmp->igmp_group.s_addr, shost, iph, igmp.igmp, loopback);
		accept_query(igmp.igmp->igmp_group.s_addr, iph->ip_src.s_addr, shost, timeout, loopback);
		break;
	default:
		return 0;
	}

	return 1;
}

static void query_timer(struct timer_entry *timer, void *data)
{
	if (!querier.enabled) {
		querier.enabled = 1;
		querier.startup = 0;
	}

	if (querier.startup > 0)
		querier.startup--;
	send_query(INADDR_ANY);
	mod_timer(timer, now() + ((querier.startup > 0) ?
	    STARTUP_QUERY_INT * TIMER_HZ : QUERY_INT * TIMER_HZ));
}

int init_querier(int enabled)
{
	querier.enabled = !!enabled;
	querier.version = 2;
	querier.startup = STARTUP_QUERY_CNT;
	set_timer(&querier.timer, query_timer, NULL);
	if (querier.enabled)
		mod_timer(&querier.timer, now() + 1);

	listen_query(htonl(INADDR_ALLHOSTS_GROUP), -1);

	return querier.enabled;
}
