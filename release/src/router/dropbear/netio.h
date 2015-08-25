#ifndef DROPBEAR_NETIO_H
#define DROPBEAR_NETIO_H

#include "includes.h"
#include "buffer.h"
#include "queue.h"

enum dropbear_prio {
	DROPBEAR_PRIO_DEFAULT = 10,
	DROPBEAR_PRIO_LOWDELAY = 11,
	DROPBEAR_PRIO_BULK = 12,
};

void set_sock_nodelay(int sock);
void set_sock_priority(int sock, enum dropbear_prio prio);

void get_socket_address(int fd, char **local_host, char **local_port,
		char **remote_host, char **remote_port, int host_lookup);
void getaddrstring(struct sockaddr_storage* addr, 
		char **ret_host, char **ret_port, int host_lookup);
int dropbear_listen(const char* address, const char* port,
		int *socks, unsigned int sockcount, char **errstring, int *maxfd);

struct dropbear_progress_connection;

/* result is DROPBEAR_SUCCESS or DROPBEAR_FAILURE.
errstring is only set on DROPBEAR_FAILURE, returns failure message for the last attempted socket */
typedef void(*connect_callback)(int result, int sock, void* data, const char* errstring);

/* Always returns a progress connection, if it fails it will call the callback at a later point */
struct dropbear_progress_connection * connect_remote (const char* remotehost, const char* remoteport,
	connect_callback cb, void *cb_data);

/* Sets up for select() */
void set_connect_fds(fd_set *writefd);
/* Handles ready sockets after select() */
void handle_connect_fds(fd_set *writefd);
/* Cleanup */
void remove_connect_pending();

/* Doesn't actually stop the connect, but adds a dummy callback instead */
void cancel_connect(struct dropbear_progress_connection *c);

void connect_set_writequeue(struct dropbear_progress_connection *c, struct Queue *writequeue);

/* TODO: writev #ifdef guard */
/* Fills out iov which contains iov_count slots, returning the number filled in iov_count */
void packet_queue_to_iovec(struct Queue *queue, struct iovec *iov, unsigned int *iov_count);
void packet_queue_consume(struct Queue *queue, ssize_t written);

#ifdef DROPBEAR_SERVER_TCP_FAST_OPEN
/* Try for any Linux builds, will fall back if the kernel doesn't support it */
void set_listen_fast_open(int sock);
/* Define values which may be supported by the kernel even if the libc is too old */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN 23
#endif
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN 0x20000000
#endif
#endif

#endif

