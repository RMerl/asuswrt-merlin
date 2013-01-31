/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2011 Denys Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#ifndef UDHCP_D6_COMMON_H
#define UDHCP_D6_COMMON_H 1

#include <netinet/ip6.h>

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN


/*** DHCPv6 packet ***/

/* DHCPv6 protocol. See RFC 3315 */
#define D6_MSG_SOLICIT              1
#define D6_MSG_ADVERTISE            2
#define D6_MSG_REQUEST              3
#define D6_MSG_CONFIRM              4
#define D6_MSG_RENEW                5
#define D6_MSG_REBIND               6
#define D6_MSG_REPLY                7
#define D6_MSG_RELEASE              8
#define D6_MSG_DECLINE              9
#define D6_MSG_RECONFIGURE         10
#define D6_MSG_INFORMATION_REQUEST 11
#define D6_MSG_RELAY_FORW          12
#define D6_MSG_RELAY_REPL          13

struct d6_packet {
	union {
		uint8_t d6_msg_type;
		uint32_t d6_xid32;
	} d6_u;
	uint8_t d6_options[576 - sizeof(struct iphdr) - sizeof(struct udphdr) - 4
			+ CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS];
} PACKED;
#define d6_msg_type d6_u.d6_msg_type
#define d6_xid32    d6_u.d6_xid32

struct ip6_udp_d6_packet {
	struct ip6_hdr ip6;
	struct udphdr udp;
	struct d6_packet data;
} PACKED;

struct udp_d6_packet {
	struct udphdr udp;
	struct d6_packet data;
} PACKED;

/*** Options ***/

struct d6_option {
	uint8_t code_hi;
	uint8_t code;
	uint8_t len_hi;
	uint8_t len;
	uint8_t data[1];
} PACKED;

#define D6_OPT_CLIENTID       1
#define D6_OPT_SERVERID       2
#define D6_OPT_IA_NA          3
#define D6_OPT_IA_TA          4
#define D6_OPT_IAADDR         5
#define D6_OPT_ORO            6
#define D6_OPT_PREFERENCE     7
#define D6_OPT_ELAPSED_TIME   8
#define D6_OPT_RELAY_MSG      9
#define D6_OPT_AUTH          11
#define D6_OPT_UNICAST       12
#define D6_OPT_STATUS_CODE   13
#define D6_OPT_RAPID_COMMIT  14
#define D6_OPT_USER_CLASS    15
#define D6_OPT_VENDOR_CLASS  16
#define D6_OPT_VENDOR_OPTS   17
#define D6_OPT_INTERFACE_ID  18
#define D6_OPT_RECONF_MSG    19
#define D6_OPT_RECONF_ACCEPT 20

#define D6_OPT_IA_PD         25
#define D6_OPT_IAPREFIX      26

/*** Other shared functions ***/

struct client6_data_t {
	struct d6_option *server_id;
	struct d6_option *ia_na;
	char **env_ptr;
	unsigned env_idx;
};

#define client6_data (*(struct client6_data_t*)(&bb_common_bufsiz1[COMMON_BUFSIZE - sizeof(struct client6_data_t)]))

int FAST_FUNC d6_listen_socket(int port, const char *inf);

int FAST_FUNC d6_recv_kernel_packet(
		struct in6_addr *peer_ipv6,
		struct d6_packet *packet, int fd
);

int FAST_FUNC d6_send_raw_packet(
		struct d6_packet *d6_pkt, unsigned d6_pkt_size,
		struct in6_addr *src_ipv6, int source_port,
		struct in6_addr *dst_ipv6, int dest_port, const uint8_t *dest_arp,
		int ifindex
);

int FAST_FUNC d6_send_kernel_packet(
		struct d6_packet *d6_pkt, unsigned d6_pkt_size,
		struct in6_addr *src_ipv6, int source_port,
		struct in6_addr *dst_ipv6, int dest_port
);

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 2
void FAST_FUNC d6_dump_packet(struct d6_packet *packet);
#else
# define d6_dump_packet(packet) ((void)0)
#endif


POP_SAVED_FUNCTION_VISIBILITY

#endif
