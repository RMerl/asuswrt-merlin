/* vi: set sw=4 ts=4: */
/*
 * Packet ops
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include <netinet/in.h>
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined _NEWLIB_VERSION
# include <netpacket/packet.h>
# include <net/ethernet.h>
#else
# include <asm/types.h>
# include <linux/if_packet.h>
# include <linux/if_ether.h>
#endif

#include "common.h"
#include "dhcpd.h"

int minpkt = 0;	// zzz

#define SECS	3	/* lame attempt to add secs field */

void FAST_FUNC udhcp_init_header(struct dhcp_packet *packet, char type)
{
	memset(packet, 0, sizeof(*packet));
	packet->op = BOOTREQUEST; /* if client to a server */
	switch (type) {
	case DHCPDISCOVER:
	case DHCPREQUEST:
		packet->secs = htons(SECS);
		break;
	case DHCPOFFER:
	case DHCPACK:
	case DHCPNAK:
		packet->op = BOOTREPLY; /* if server to client */
	}
	packet->htype = 1; /* ethernet */
	packet->hlen = 6;
	packet->cookie = htonl(DHCP_MAGIC);
	if (DHCP_END != 0)
		packet->options[0] = DHCP_END;
	udhcp_add_simple_option(packet, DHCP_MESSAGE_TYPE, type);
}

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 2
void FAST_FUNC udhcp_dump_packet(struct dhcp_packet *packet)
{
	char buf[sizeof(packet->chaddr)*2 + 1];

	if (dhcp_verbose < 2)
		return;

	bb_info_msg(
		//" op %x"
		//" htype %x"
		" hlen %x"
		//" hops %x"
		" xid %x"
		//" secs %x"
		//" flags %x"
		" ciaddr %x"
		" yiaddr %x"
		" siaddr %x"
		" giaddr %x"
		//" chaddr %s"
		//" sname %s"
		//" file %s"
		//" cookie %x"
		//" options %s"
		//, packet->op
		//, packet->htype
		, packet->hlen
		//, packet->hops
		, packet->xid
		//, packet->secs
		//, packet->flags
		, packet->ciaddr
		, packet->yiaddr
		, packet->siaddr_nip
		, packet->gateway_nip
		//, packet->chaddr[16]
		//, packet->sname[64]
		//, packet->file[128]
		//, packet->cookie
		//, packet->options[]
	);
	*bin2hex(buf, (void *) packet->chaddr, sizeof(packet->chaddr)) = '\0';
	bb_info_msg(" chaddr %s", buf);
}
#endif

/* Read a packet from socket fd, return -1 on read error, -2 on packet error */
int FAST_FUNC udhcp_recv_kernel_packet(struct dhcp_packet *packet, int fd)
{
	int bytes;
	unsigned char *vendor;

	memset(packet, 0, sizeof(*packet));
	bytes = safe_read(fd, packet, sizeof(*packet));
	if (bytes < 0) {
		log1("Packet read error, ignoring");
		return bytes; /* returns -1 */
	}

	if (packet->cookie != htonl(DHCP_MAGIC)) {
		bb_info_msg("Packet with bad magic, ignoring");
		return -2;
	}
	log1("Received a packet");
	udhcp_dump_packet(packet);

	if (packet->op == BOOTREQUEST) {
		vendor = udhcp_get_option(packet, DHCP_VENDOR);
		if (vendor) {
#if 0
			static const char broken_vendors[][8] = {
				"MSFT 98",
				""
			};
			int i;
			for (i = 0; broken_vendors[i][0]; i++) {
				if (vendor[OPT_LEN - OPT_DATA] == (uint8_t)strlen(broken_vendors[i])
				 && strncmp((char*)vendor, broken_vendors[i], vendor[OPT_LEN - OPT_DATA]) == 0
				) {
					log1("Broken client (%s), forcing broadcast replies",
						broken_vendors[i]);
					packet->flags |= htons(BROADCAST_FLAG);
				}
			}
#else
			if (vendor[OPT_LEN - OPT_DATA] == (uint8_t)(sizeof("MSFT 98")-1)
			 && memcmp(vendor, "MSFT 98", sizeof("MSFT 98")-1) == 0
			) {
				log1("Broken client (%s), forcing broadcast replies", "MSFT 98");
				packet->flags |= htons(BROADCAST_FLAG);
			}
#endif
		}
	}

	return bytes;
}

uint16_t FAST_FUNC udhcp_checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 * beginning at location "addr".
	 */
	int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		/* Make sure that the left-over byte is added correctly both
		 * with little and big endian hosts */
		uint16_t tmp = 0;
		*(uint8_t*)&tmp = *(uint8_t*)source;
		sum += tmp;
	}
	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/* Construct a ip/udp header for a packet, send packet */
int FAST_FUNC udhcp_send_raw_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_nip, int source_port,
		uint32_t dest_nip, int dest_port, const uint8_t *dest_arp,
		int ifindex)
{
	struct sockaddr_ll dest_sll;
	struct ip_udp_dhcp_packet packet;
	unsigned padding;
	int fd;
	int result = -1;
	const char *msg;

	fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0) {
		msg = "socket(%s)";
		goto ret_msg;
	}

	memset(&dest_sll, 0, sizeof(dest_sll));
	memset(&packet, 0, offsetof(struct ip_udp_dhcp_packet, data));
	packet.data = *dhcp_pkt; /* struct copy */

	dest_sll.sll_family = AF_PACKET;
	dest_sll.sll_protocol = htons(ETH_P_IP);
	dest_sll.sll_ifindex = ifindex;
	dest_sll.sll_halen = 6;
	memcpy(dest_sll.sll_addr, dest_arp, 6);

	if (bind(fd, (struct sockaddr *)&dest_sll, sizeof(dest_sll)) < 0) {
		msg = "bind(%s)";
		goto ret_close;
	}

	/* We were sending full-sized DHCP packets (zero padded),
	 * but some badly configured servers were seen dropping them.
	 * Apparently they drop all DHCP packets >576 *ethernet* octets big,
	 * whereas they may only drop packets >576 *IP* octets big
	 * (which for typical Ethernet II means 590 octets: 6+6+2 + 576).
	 *
	 * In order to work with those buggy servers,
	 * we truncate packets after end option byte.
	 */
	padding = minpkt ? DHCP_OPTIONS_BUFSIZE - 1 - udhcp_end_option(packet.data.options) : 0;

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_nip;
	packet.ip.daddr = dest_nip;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	/* size, excluding IP header: */
	packet.udp.len = htons(UPD_DHCP_SIZE - padding);
	/* for UDP checksumming, ip.len is set to UDP packet len */
	packet.ip.tot_len = packet.udp.len;
	packet.udp.check = udhcp_checksum(&packet, IP_UPD_DHCP_SIZE - padding);
	/* but for sending, it is set to IP packet len */
	packet.ip.tot_len = htons(IP_UPD_DHCP_SIZE - padding);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL * 2;
	packet.ip.check = udhcp_checksum(&packet.ip, sizeof(packet.ip));

	udhcp_dump_packet(dhcp_pkt);
	result = sendto(fd, &packet, IP_UPD_DHCP_SIZE - padding, /*flags:*/ 0,
			(struct sockaddr *) &dest_sll, sizeof(dest_sll));
	msg = "sendto";
 ret_close:
	close(fd);
	if (result < 0) {
 ret_msg:
		bb_perror_msg(msg, "PACKET");
	}
	return result;
}

/* Let the kernel do all the work for packet generation */
int FAST_FUNC udhcp_send_kernel_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_nip, int source_port,
		uint32_t dest_nip, int dest_port)
{
	struct sockaddr_in client;
	unsigned padding;
	int fd;
	int result = -1;
	const char *msg;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		msg = "socket(%s)";
		goto ret_msg;
	}
	setsockopt_reuseaddr(fd);

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_nip;
	if (bind(fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
		msg = "bind(%s)";
		goto ret_close;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_nip;
	if (connect(fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
		msg = "connect";
		goto ret_close;
	}

	udhcp_dump_packet(dhcp_pkt);

	padding = minpkt ? DHCP_OPTIONS_BUFSIZE - 1 - udhcp_end_option(dhcp_pkt->options) : 0;
	result = safe_write(fd, dhcp_pkt, DHCP_SIZE - padding);
	msg = "write";
 ret_close:
	close(fd);
	if (result < 0) {
 ret_msg:
		bb_perror_msg(msg, "UDP");
	}
	return result;
}
