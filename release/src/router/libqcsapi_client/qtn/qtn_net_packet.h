#ifndef __QTN_NET_PACKET_H__
#define __QTN_NET_PACKET_H__

#include <qtn/qtn_global.h>

#ifndef ETHERTYPE_ARP
#define	ETHERTYPE_ARP	0x0806		/* ARP protocol */
#endif

#ifndef ETHERTYPE_AARP
#define ETHERTYPE_AARP	0x80f3		/* Appletalk AARP */
#endif

#ifndef ETHERTYPE_PAE
#define	ETHERTYPE_PAE	0x888e		/* EAPOL PAE/802.1x */
#endif

#ifndef ETHERTYPE_IP
#define	ETHERTYPE_IP	0x0800		/* IP protocol */
#endif

#ifndef ETHERTYPE_IPV6
#define	ETHERTYPE_IPV6	0x86DD		/* IPv6 protocol */
#endif

#ifndef ETHERTYPE_IPX
#define ETHERTYPE_IPX	0x8137		/* IPX over DIX */
#endif

#ifndef ETHERTYPE_802A
#define ETHERTYPE_802A	0x88B7
#endif

#ifndef ETHERTYPE_8021Q
#define	ETHERTYPE_8021Q	0x8100          /* 802.1Q VLAN header */
#endif

#ifndef ETHERTYPE_8021AD
#define ETHERTYPE_8021AD	0x88A8  /* 802.1AD VLAN S-TAG header */
#endif

#ifndef ETHERTYPE_WAKE_ON_LAN
#define ETHERTYPE_WAKE_ON_LAN	0X0842
#endif

union qtn_ipv4_addr {
	uint32_t ip32;
	uint16_t ip16[2];
	uint8_t ip8[4];
};

struct qtn_ipv4 {
	uint8_t	vers_ihl;
	uint8_t dscp;
	uint16_t length;
	uint16_t ident;
	uint16_t flags:3,
		 fragoffset:13;
	uint8_t ttl;
	uint8_t proto;
	uint16_t csum;
	union qtn_ipv4_addr srcip;
	union qtn_ipv4_addr dstip;
	uint32_t opt[0];
};

union qtn_ipv6_addr {
	uint64_t ip64[2];
	uint32_t ip32[4];
	uint16_t ip16[8];
	uint8_t ip8[16];
};

struct qtn_ipv6 {
	uint16_t vers_tclass_flowlabel[2];
	uint16_t length;
	uint8_t next_hdr;
	uint8_t hop_limit;
	union qtn_ipv6_addr srcip;
	union qtn_ipv6_addr dstip;
};

RUBY_INLINE uint8_t qtn_ipv6_tclass(const struct qtn_ipv6 *ipv6)
{
	return ((ipv6->vers_tclass_flowlabel[0]) >> 4) & 0xFF;
}

#define QTN_IP_PROTO_ICMP	1
#define QTN_IP_PROTO_IGMP	2
#define QTN_IP_PROTO_TCP	6
#define QTN_IP_PROTO_UDP	17
#define QTN_IP_PROTO_IPV6FRAG	44
#define QTN_IP_PROTO_ICMPV6	58
#define QTN_IP_PROTO_RAW	255

#define QTN_MAX_VLANS	4

struct qtn_8021q {
	uint16_t tpid;
	uint16_t tci;
};

struct qtn_udp {
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t length;
	uint16_t csum;
};

RUBY_INLINE void qtn_mcast_ipv4_to_mac(uint8_t *const mac_be,
		const uint8_t *const ipv4)
{
	mac_be[0] = 0x01;
	mac_be[1] = 0x00;
	mac_be[2] = 0x5E;
	mac_be[3] = ipv4[1] & 0x7F;
	mac_be[4] = ipv4[2];
	mac_be[5] = ipv4[3];
}

RUBY_INLINE void qtn_mcast_ipv6_to_mac(uint8_t *const mac_be,
		const uint8_t *const ipv6)
{
	mac_be[0] = 0x33;
	mac_be[1] = 0x33;
	mac_be[2] = ipv6[12];
	mac_be[3] = ipv6[13];
	mac_be[4] = ipv6[14];
	mac_be[5] = ipv6[15];
}

RUBY_INLINE void qtn_mcast_mac_to_ipv4(uint8_t *const ipv4,
		const uint8_t *const mac_be, const uint8_t ip_map)
{
	ipv4[0] = ((ip_map >> 1) & 0xF) | (0xE << 4);
	ipv4[1] = (mac_be[3] & 0x7F) | ((ip_map & 1) << 7);
	ipv4[2] = mac_be[4];
	ipv4[3] = mac_be[5];
}

RUBY_INLINE void qtn_mcast_to_mac(uint8_t *mac_be, const void *addr, uint16_t ether_type)
{
	if (ether_type == htons(ETHERTYPE_IP)) {
		qtn_mcast_ipv4_to_mac(mac_be, addr);
	} else if (ether_type == htons(ETHERTYPE_IPV6)) {
		qtn_mcast_ipv6_to_mac(mac_be, addr);
	} else {
		/* invalid address family */
	}
}

/*
 * IPV4 extra metadata per entry
 * Size derive from ipv4 address[27:23]
 * ipv4[0] is always 0xe followed by 5 bits that define the ipv4
 * table size so in this way we can differentiate between similar multicast mac addresses
 * the other 23 bits assemble the mac multicast id.
 */
RUBY_INLINE uint8_t qtn_mcast_ipv4_alias(const uint8_t *ipv4)
{
	return ((ipv4[1] >> 7) | (ipv4[0] & 0xF) << 1);
}

RUBY_INLINE uint8_t qtn_ether_type_is_vlan(const uint16_t type)
{
	return ((type == htons(ETHERTYPE_8021Q))
			|| (type == htons(ETHERTYPE_8021AD)));
}

#endif	// __QTN_NET_PACKET_H__

