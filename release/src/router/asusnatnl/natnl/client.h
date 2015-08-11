/*
 * Project: udptunnel
 * File: client.h
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

#ifndef AA_CLIENT_H
#define AA_CLIENT_H


#ifndef WIN32
#include <inttypes.h>
#include <sys/time.h>
#endif /*WIN32*/

#if PJ_ANDROID==1
#include <sys/select.h>
#endif

#include <common.h>
#include <socket.h>
#include <list.h>
#include <message.h>

#include <pjmedia/transport.h>
#include <pj/timer.h>

/* 2013-03-20 DEAN Added, for bandwidth control */
#include <pj/bandwidth.h>

#include <natnl_lib.h>

#define CLIENT_TIMEOUT 1 /* in seconds */
#define CLIENT_MAX_RESEND 10

#define CLIENT_WAIT_HELLO 1
#define CLIENT_WAIT_DATA0 2
#define CLIENT_WAIT_DATA1 3
#define CLIENT_WAIT_ACK0  4
#define CLIENT_WAIT_ACK1  5

#define ACK_CHECK_BOUND 50

#define KEEP_ALIVE_TIME 10

enum disconn_type {
	DISCONN_EMPTY_BUFFER,
	DISCONN_REMOTE_NOTIFY
};

enum client_status {
	CLIENT_STATUS_NONE,
	CLIENT_STATUS_WORKING,
	CLIENT_STATUS_SUSPENDED,
	CLIENT_STATUS_RESUMING,
	CLIENT_STATUS_DESTORYING,
	CLIENT_STATUS_DESTORYED,
};

enum client_role {
	CLIENT_ROLE_NONE,
	CLIENT_ROLE_CLIENT,
	CLIENT_ROLE_SERVER
};
typedef struct client
{
    uint16_t id; /* Must be first in struct */
    socket_t *sock; /* Socket for connection to TCP server */
    //socket_t *udp_sock; /* Socket to hold address from UDP client */
	uint8_t sock_type;
    pjmedia_transport *tp; /* Socket to hold address from UDP client */
	int connected;
	int to_disconn_tcp_srv_no_data;
	int to_disconn_remote_request;
    struct timeval keepalive;
	int len;

    /* For data going from UDP tunnel to TCP connection */
    char udp2tcp[MAX_PACKET_LEN];
    int udp2tcp_len;
	int udp2tcp_state;

	uint32_t udp2tcp_curr_pkt;
	int udp2tcp_buf_cnt;

    /* For data going from TCP connection to UDP tunnel */
	char tcp2udp[MAX_PACKET_LEN];
	int tcp2udp_len;
    int tcp2udp_state;
    struct timeval tcp2udp_timeout;
    int resend_count;

	uint32_t tcp2udp_curr_pkt;
	int tcp2udp_buf_cnt;

	int resend;

	// for ACK check
	int send_count;
	int recv_count;

	natnl_tunnel_type tunnel_type;
	int tcp_recv_bytes;
	int tcp_send_bytes;
	int udp_recv_bytes;
	int udp_send_bytes;

	enum client_status status;
	enum client_role role;

	socket_t src_sock;
	pj_timestamp         last_data;     // DEAN, to prevent wrong KA_TIMEOUT event

	int resume_start_pkt;

	pj_thread_t *resume_thread;

	int qos_priority;
	int qos_cnt;

	pj_mutex_t *lock;
	int inst_id;        // The instance id of SDK.
	int call_id;        // The identity of tunnel.
} client_t;

/* Call specific data */
typedef struct call_data
{
	pj_timer_entry      timer;

	fd_set              client_fds;
	list_t             *clients;
	list_t             *client_locks;

    /* for UDP_CLIENT */
    list_t             *conn_clients;

	//socket_t           *tcp_serv;
    list_t             *sock_servs;

    char *lhost, *lport, *phost, *pport, *rhost, *rport;

	/* 2013-03-20 DEAN Added, for bandwidth control */
	pj_band_t          *band;

	char udp_tmp_data[MAX_PACKET_LEN];
	int udp_tmp_len;

	int tnl_ports_cnt;
	natnl_tnl_port tnl_ports[MAX_TUNNEL_PORT_COUNT];

	int next_client_id;

} call_data;


#define CLIENT_ID(c) ((c)->id)
//#define CLIENT_UDP_SOCK(c) ((c)->udp_sock)
#define CLIENT_UDP_SOCK(c) (-1)
#define CLIENT_TCP_SOCK(c) ((c)->tcp_sock)

client_t *client_create(uint16_t id, socket_t *tcp_sock, pjmedia_transport *tp,
                        int connected);
//client_t *client_get_client_by_tp(struct call_data *cd, pjmedia_transport *tp);
client_t *client_copy(client_t *dst, client_t *src, size_t len);
int client_cmp(client_t *c1, client_t *c2, size_t len);
void client_free(client_t **c);
void mutex_free(pj_mutex_t **c);
int client_connect_tcp(client_t *c);
void client_disconnect_tcp(client_t *c);
void client_disconnect_udp(client_t *c);
int check_and_send_tcp_keepalive(pjmedia_transport *tp, struct timeval curr_tv);
int check_timed_out(pjmedia_transport *tp, struct timeval curr_tv);

#ifdef UDP_SOCK
int client_recv_udp_msg(client_t *client, char *data, int data_len,
                        uint16_t *id, uint8_t *msg_type, uint16_t *len);
#endif

int client_send_lo_data(client_t *c);
int client_recv_lo_data(client_t *c, struct call_data *cd);
int client_send_tnl_data(client_t *c);
int client_recv_tnl_data(client_t *c, char *data, int data_len,
						uint32_t pkt_id, uint8_t msg_type);
int client_got_ack(client_t *c, uint8_t ack_type);
int client_send_hello(client_t *c, char *host, char *port,
                      uint16_t req_id);
int client_send_helloack(client_t *c, uint16_t req_id);
int client_got_helloack(client_t *c);
int client_send_goodbye(client_t *c);
int client_send_goodbye_by_id(pjmedia_transport *tp, uint16_t id, uint8_t sock_type); //DEAN added
#if 0
int client_check_and_send_keepalive(client_t *client, struct timeval curr_tv);
#endif
void client_udp_reset_keepalive(client_t *client);
int client_udp_timed_out(client_t *client, struct timeval curr_tv);
void client_set_status(client_t *client, enum client_status status);
int client_is_working(client_t *client);
int client_suspend_all(int inst_id, int call_id);

/* Function pointers to use when making a list_t of clients */
#define p_client_copy ((void* (*)(void *, const void *, size_t))&client_copy)
#define p_client_cmp ((int (*)(const void *, const void *, size_t))&client_cmp)
#define p_client_free ((void (*)(void **))&client_free)

/* Function pointers to use when making a list_t of clients */
//#define p_mutex_copy ((void* (*)(void *, const void *, size_t))&mutex_copy)
//#define p_mutex_cmp ((int (*)(const void *, const void *, size_t))&mutex_cmp)
#define p_mutex_free ((void (*)(void **))&mutex_free)

/* Function pointers to use when making a list_t of packets */
#define p_packet_copy ((void* (*)(void *, const void *, size_t))&packet_copy)
#define p_packet_cmp ((int (*)(const void *, const void *, size_t))&packet_cmp)
#define p_packet_free ((void (*)(void **))&packet_free)

/* Inline functions as wrappers for handling the file descriptors in the
 * client's sockets */

static _inline_ void client_add_fd_to_set(client_t *c, fd_set *set)
{
    if (c != NULL && c->sock != NULL)
        if(SOCK_FD(c->sock) >= 0 && !FD_ISSET(SOCK_FD(c->sock), set))
            FD_SET(SOCK_FD(c->sock), set);
}

#if 0
static _inline_ void client_add_udp_fd_to_set(client_t *c, fd_set *set)
{
    if(SOCK_FD(c->udp_sock) >= 0)
        FD_SET(SOCK_FD(c->udp_sock), set);
}
#endif

static _inline_ int client_tcp_fd_isset(client_t *c, fd_set *set)
{
    return SOCK_FD(c->sock) >= 0 ? FD_ISSET(SOCK_FD(c->sock), set) : 0;
}

#if 0
static _inline_ int client_udp_fd_isset(client_t *c, fd_set *set)
{
    return SOCK_FD(c->udp_sock) >= 0 ? FD_ISSET(SOCK_FD(c->udp_sock), set) : 0;
}
#endif

static _inline_ void client_remove_fd_from_set(client_t *c, fd_set *set)
{
    if(SOCK_FD(c->sock) >= 0)
        FD_CLR(SOCK_FD(c->sock), set);
}

#if 0
static _inline_ void client_remove_udp_fd_from_set(client_t *c, fd_set *set)
{
    if(SOCK_FD(c->udp_sock) >= 0)
        FD_CLR(SOCK_FD(c->udp_sock), set);
}
#endif

static _inline_ void client_mark_to_disconnect(client_t *c, enum disconn_type type)
{
	(type == DISCONN_EMPTY_BUFFER) ? 
		(c->to_disconn_tcp_srv_no_data = 1) : (c->to_disconn_remote_request = 1);
}

//DEAN Add arguments chk_buf to determine to check tcp2udp_q buffer or not.
static _inline_ int client_ready_to_disconnect(client_t *c)
{
	return ((c->to_disconn_remote_request /*|| 
		c->to_disconn_tcp_srv_no_data*/) /*&& c->tcp2udp_len == 0*/);
}

#endif /* CLIENT_H */

