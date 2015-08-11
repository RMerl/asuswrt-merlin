/*
 * Project: udptunnel
 * File: message.h
 *
 * Copyright (C) 2009 Daniel Meekins
 * Contact: dmeekins - gmail
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AA_MESSAGE_H
#define AA_MESSAGE_H

#ifndef WIN32
#include <inttypes.h>
#include <arpa/inet.h>
#endif /*WIN32*/

#include <common.h>
#include <socket.h>
#include <list.h>

#include <pjmedia/transport.h>

#define KEEP_ALIVE_SECS 20
#define KEEP_ALIVE_TIMEOUT_SECS (7*60+1) /* has 7 tries to send a keep alive */

#define DUMP_HEX 0

#define MAX_PACKET_LEN  3000

#define UDP_MSG_MAX_LEN NATNL_PKT_MAX_LEN 
#define UPNP_TCP_MSG_MAX_LEN NATNL_PKT_MAX_LEN
#define TURN_MSG_MAX_LEN NATNL_PKT_MAX_LEN

//#ifdef PJ_M_MIPS // DEAN
#define SND_THREAD_DELAY_CNT 100
#define RCV_THREAD_DELAY_CNT 100
//#else
//#define SND_THREAD_DELAY_CNT 15
//#define RCV_THREAD_DELAY_CNT 100
//#endif

/* Message types: max 8 bits */
#define MSG_TYPE_GOODBYE   0x01
#define MSG_TYPE_HELLO     0x02
#define MSG_TYPE_HELLOACK  0x03
#define MSG_TYPE_KEEPALIVE 0x04
#define MSG_TYPE_DATA0     0x05
#define MSG_TYPE_DATA1     0x06
#define MSG_TYPE_ACK0      0x07
#define MSG_TYPE_ACK1      0x08
#define MSG_TYPE_RESUME    0x09
#define MSG_TYPE_RESUME_DATA    0x0A
#define MSG_TYPE_READY_GOODBYE   0x0B

// !!!!!!!!!!! DEAN: notice !!!!!!!!!!!
// If the size of msg_hdr structure has been changed, 
// please remember to modify TUNNEL_HEADER_SIZE value that defines in pjlib/include/config_site.h
#ifndef WIN32
struct msg_hdr
{
	//uint32_t magic;
	uint16_t length;
	uint32_t pkt_id;
	uint16_t client_id;
	uint8_t type;
	uint8_t proto;
	uint8_t qos_priority;
	uint8_t reserved[9];
} __attribute__ ((__packed__));
#else
#pragma pack(push, 1)
struct msg_hdr
{
	//uint32_t magic;
	uint16_t length;
	uint32_t pkt_id;
	uint16_t client_id;
	uint8_t type;
	uint8_t proto;
	uint8_t qos_priority;
	uint8_t reserved[9];
};
#pragma pack(pop)
#endif /*WIN32*/

typedef struct msg_hdr msg_hdr_t;

int msg_send_msg(pjmedia_transport *tp, uint16_t client_id, uint32_t pkt_id, 
				 uint8_t type, char *data, int data_len, uint8_t proto, uint8_t qos_priority);
int msg_send_hello(pjmedia_transport *tp, char *host, char *port, uint16_t req_id, uint8_t sock_type, uint8_t qos_priority);

int msg_recv_msg(socket_t *sock, socket_t *from, char *data, int *data_len);

int retrieve_src_addr(socket_t *sock, socket_t *from);

/* Inline functions for working with the message header struct */
static _inline_ void msg_init_header(msg_hdr_t *hdr, uint32_t pkt_id, uint16_t client_id,
                                   uint8_t type, uint16_t len, uint8_t proto, uint8_t qos_priority)
{
//    hdr->magic = TUNNEL_HEADER_MAGIC;
	hdr->pkt_id = htonl(pkt_id);
	hdr->client_id = htons(client_id);
	hdr->type = type;
	hdr->length = htons(len);
	hdr->proto = proto;
	hdr->qos_priority = qos_priority;
	memset(hdr->reserved, 0, sizeof(hdr->reserved));
}

static _inline_ uint16_t msg_get_client_id(msg_hdr_t *h)
{
	return ntohs(h->client_id);
}

static _inline_ uint32_t msg_get_pkt_id(msg_hdr_t *h)
{
	return ntohl(h->pkt_id);
}

static _inline_ uint8_t msg_get_type(msg_hdr_t *h)
{
    return h->type;
}

static _inline_ uint16_t msg_get_length(msg_hdr_t *h)
{
	return ntohs(h->length);
}

static _inline_ uint16_t msg_get_proto(msg_hdr_t *h)
{
	return h->proto;
}

static _inline_ uint8_t msg_get_qos_priority(msg_hdr_t *h)
{
	return h->qos_priority;
}

#endif /* AA_MESSAGE_H */

