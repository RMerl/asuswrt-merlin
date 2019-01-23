/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2017 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PROTO_H
#define PROTO_H

#include "common.h"
#include "buffer.h"

#pragma pack(1)

/*
 * Tunnel types
 */
#define DEV_TYPE_UNDEF 0
#define DEV_TYPE_NULL  1
#define DEV_TYPE_TUN   2    /* point-to-point IP tunnel */
#define DEV_TYPE_TAP   3    /* ethernet (802.3) tunnel */

/* TUN topologies */

#define TOP_UNDEF   0
#define TOP_NET30   1
#define TOP_P2P     2
#define TOP_SUBNET  3

/*
 * IP and Ethernet protocol structs.  For portability,
 * OpenVPN needs its own definitions of these structs, and
 * names have been adjusted to avoid collisions with
 * native structs.
 */

#define OPENVPN_ETH_ALEN 6            /* ethernet address length */
struct openvpn_ethhdr
{
    uint8_t dest[OPENVPN_ETH_ALEN];   /* destination ethernet addr */
    uint8_t source[OPENVPN_ETH_ALEN]; /* source ethernet addr   */

#define OPENVPN_ETH_P_IPV4   0x0800   /* IPv4 protocol */
#define OPENVPN_ETH_P_IPV6   0x86DD   /* IPv6 protocol */
#define OPENVPN_ETH_P_ARP    0x0806   /* ARP protocol */
    uint16_t proto;                   /* packet type ID field */
};

struct openvpn_arp {
#define ARP_MAC_ADDR_TYPE 0x0001
    uint16_t mac_addr_type;     /* 0x0001 */

    uint16_t proto_addr_type;   /* 0x0800 */
    uint8_t mac_addr_size;      /* 0x06 */
    uint8_t proto_addr_size;    /* 0x04 */

#define ARP_REQUEST 0x0001
#define ARP_REPLY   0x0002
    uint16_t arp_command;       /* 0x0001 for ARP request, 0x0002 for ARP reply */

    uint8_t mac_src[OPENVPN_ETH_ALEN];
    in_addr_t ip_src;
    uint8_t mac_dest[OPENVPN_ETH_ALEN];
    in_addr_t ip_dest;
};

struct openvpn_iphdr {
#define OPENVPN_IPH_GET_VER(v) (((v) >> 4) & 0x0F)
#define OPENVPN_IPH_GET_LEN(v) (((v) & 0x0F) << 2)
    uint8_t version_len;

    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;

#define OPENVPN_IP_OFFMASK 0x1fff
    uint16_t frag_off;

    uint8_t ttl;

#define OPENVPN_IPPROTO_IGMP 2  /* IGMP protocol */
#define OPENVPN_IPPROTO_TCP  6  /* TCP protocol */
#define OPENVPN_IPPROTO_UDP 17  /* UDP protocol */
    uint8_t protocol;

    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
    /*The options start here. */
};

/*
 * IPv6 header
 */
struct openvpn_ipv6hdr {
    uint8_t version_prio;
    uint8_t flow_lbl[3];
    uint16_t payload_len;
    uint8_t nexthdr;
    uint8_t hop_limit;

    struct  in6_addr saddr;
    struct  in6_addr daddr;
};


/*
 * UDP header
 */
struct openvpn_udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

/*
 * TCP header, per RFC 793.
 */
struct openvpn_tcphdr {
    uint16_t source;       /* source port */
    uint16_t dest;         /* destination port */
    uint32_t seq;          /* sequence number */
    uint32_t ack_seq;      /* acknowledgement number */

#define OPENVPN_TCPH_GET_DOFF(d) (((d) & 0xF0) >> 2)
    uint8_t doff_res;

#define OPENVPN_TCPH_FIN_MASK (1<<0)
#define OPENVPN_TCPH_SYN_MASK (1<<1)
#define OPENVPN_TCPH_RST_MASK (1<<2)
#define OPENVPN_TCPH_PSH_MASK (1<<3)
#define OPENVPN_TCPH_ACK_MASK (1<<4)
#define OPENVPN_TCPH_URG_MASK (1<<5)
#define OPENVPN_TCPH_ECE_MASK (1<<6)
#define OPENVPN_TCPH_CWR_MASK (1<<7)
    uint8_t flags;

    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
};

#define OPENVPN_TCPOPT_EOL     0
#define OPENVPN_TCPOPT_NOP     1
#define OPENVPN_TCPOPT_MAXSEG  2
#define OPENVPN_TCPOLEN_MAXSEG 4

struct ip_tcp_udp_hdr {
    struct openvpn_iphdr ip;
    union {
        struct openvpn_tcphdr tcp;
        struct openvpn_udphdr udp;
    } u;
};

#pragma pack()

/*
 * The following macro is used to update an
 * internet checksum.  "acc" is a 32-bit
 * accumulation of all the changes to the
 * checksum (adding in old 16-bit words and
 * subtracting out new words), and "cksum"
 * is the checksum value to be updated.
 */
#define ADJUST_CHECKSUM(acc, cksum) { \
        int _acc = acc; \
        _acc += (cksum); \
        if (_acc < 0) { \
            _acc = -_acc; \
            _acc = (_acc >> 16) + (_acc & 0xffff); \
            _acc += _acc >> 16; \
            (cksum) = (uint16_t) ~_acc; \
        } else { \
            _acc = (_acc >> 16) + (_acc & 0xffff); \
            _acc += _acc >> 16; \
            (cksum) = (uint16_t) _acc; \
        } \
}

#define ADD_CHECKSUM_32(acc, u32) { \
        acc += (u32) & 0xffff; \
        acc += (u32) >> 16;    \
}

#define SUB_CHECKSUM_32(acc, u32) { \
        acc -= (u32) & 0xffff; \
        acc -= (u32) >> 16;    \
}

/*
 * We are in a "liberal" position with respect to MSS,
 * i.e. we assume that MSS can be calculated from MTU
 * by subtracting out only the IP and TCP header sizes
 * without options.
 *
 * (RFC 879, section 7).
 */
#define MTU_TO_MSS(mtu) (mtu - sizeof(struct openvpn_iphdr) \
                         - sizeof(struct openvpn_tcphdr))

/*
 * This returns an ip protocol version of packet inside tun
 * and offset of IP header (via parameter).
 */
inline static int
get_tun_ip_ver(int tunnel_type, struct buffer *buf, int *ip_hdr_offset)
{
    int ip_ver = -1;

    /* for tun get ip version from ip header */
    if (tunnel_type == DEV_TYPE_TUN)
    {
        *ip_hdr_offset = 0;
        if (likely(BLEN(buf) >= (int) sizeof(struct openvpn_iphdr)))
        {
            ip_ver = OPENVPN_IPH_GET_VER(*BPTR(buf));
        }
    }
    else if (tunnel_type == DEV_TYPE_TAP)
    {
        *ip_hdr_offset = (int)(sizeof(struct openvpn_ethhdr));
        /* for tap get ip version from eth header */
        if (likely(BLEN(buf) >= *ip_hdr_offset))
        {
            const struct openvpn_ethhdr *eh = (const struct openvpn_ethhdr *) BPTR(buf);
            uint16_t proto = ntohs(eh->proto);
            if (proto == OPENVPN_ETH_P_IPV6)
            {
                ip_ver = 6;
            }
            else if (proto == OPENVPN_ETH_P_IPV4)
            {
                ip_ver = 4;
            }
        }
    }

    return ip_ver;
}

/*
 * If raw tunnel packet is IPv4 or IPv6, return true and increment
 * buffer offset to start of IP header.
 */
bool is_ipv4(int tunnel_type, struct buffer *buf);

bool is_ipv6(int tunnel_type, struct buffer *buf);

#ifdef PACKET_TRUNCATION_CHECK
void ipv4_packet_size_verify(const uint8_t *data,
                             const int size,
                             const int tunnel_type,
                             const char
                             *prefix,
                             counter_type *errors);

#endif

#endif /* ifndef PROTO_H */
