/*
 * Project: udptunnel
 * File: socket.h
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

#ifndef SOCKET_H
#define SOCKET_H

#ifndef WIN32
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /*WIN32*/

#include <natnl_lib.h>
#include <string.h>
#include <common.h>
#include <pjlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BACKLOG 10
#define ADDRSTRLEN (INET6_ADDRSTRLEN + 9)

#define SOCK_TYPE_TCP 1
#define SOCK_TYPE_UDP 2
#define SOCK_IPV4     3
#define SOCK_IPV6     4

#define SIN(sa) ((struct sockaddr_in *)sa)
#define SIN6(sa) ((struct sockaddr_in6 *)sa)
#define PADDR(a) ((struct sockaddr *)a)

typedef struct socket_s {         // dean : Rename it from socket tot socket_s due to name conflicts with libusrsctp.
    int fd;                       /* Socket file descriptor to send/recv on */
    int type;                     /* SOCK_STREAM or SOCK_DGRAM */
    struct sockaddr_storage addr; /* IP and port */
	socklen_t addr_len;           /* Length of sockaddr type */
	uint16_t lport;               /* The port which local client should be connected. */
	uint16_t rport;               /* The port which remote client should connect to. */
	char rip[MAX_IP_LEN];         /* The remote device ip address string */
	uint8_t qos_priority;         /* The QoS priority. */
	uint8_t disable_flow_control; /* The flag for disable flow control. */
	uint16_t speed_limit;         /* The speed limit in KBytes/s unit. */
	int inst_id;                  /* The instance id of SDK. */
	int call_id;                  /* The identity of tunnel. */
	int client_id;                /* The identity of session. */
	char im_dest_deviceid[128];   /* The device_id of destination for sending message. */
	int im_timeout_sec;			  /* The timeout value in second unit for sending message. */
	int parent_client_id;         /* The parent client id. Used for RTSP udp session. */
} socket_t;

#define SOCK_FD(s) ((s)->fd)
#define SOCK_LEN(s) ((s)->addr_len)
#define SOCK_PADDR(s) ((struct sockaddr *)&(s)->addr)
#define SOCK_TYPE(s) ((s)->type)

// DEAN modified
/* Add rport parameter */
int sock_create(socket_t **sock, char *host, char *port, char *rip, char *rport, int ipver, int sock_type,
                      int is_serv, int conn, uint8_t qos_priority, uint8_t disable_flow_control, 
					  uint16_t speed_limit, int inst_id, int call_id);
socket_t *sock_copy(socket_t *sock);
int sock_connect(socket_t *sock, int is_serv);
socket_t *sock_accept(socket_t *serv_sock);
void sock_close(socket_t *s);
void sock_free(socket_t **s);
int sock_addr_equal(socket_t *s1, socket_t *s2);
int sock_ipaddr_cmp(socket_t *s1, socket_t *s2);
int sock_port_cmp(socket_t *s1, socket_t *s2);
int sock_isaddrany(socket_t *s);
char *sock_get_str(socket_t *s, char *buf, int len);
char *sock_get_addrstr(socket_t *s, char *buf, int len);
uint16_t sock_get_port(socket_t *s);
int sock_recv(socket_t *sock, socket_t *from, char *data, int len);
int sock_recv_whole_data(socket_t *sock, socket_t *from, char *data, int len);
int sock_send(socket_t *to, char *data, int len);
int isipaddr(char *ip, int ipver);

// DEAN added
void socket_t_free(socket_t **s);
/* Function pointers to use when making a list_t of tcp server sockets */
#define p_socket_t_copy ((void* (*)(void *, const void *, size_t))&sock_copy)
#define p_socket_t_free ((void (*)(void **))&socket_t_free)

static int _inline_ sock_get_ipver(socket_t *s)
{
    return (s->addr.ss_family == AF_INET) ? SOCK_IPV4 :
        ((s->addr.ss_family == AF_INET6) ? SOCK_IPV6 : 0);
}

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_H */

