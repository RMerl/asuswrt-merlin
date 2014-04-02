/* $Id: asyncsendto.c,v 1.2 2014/02/28 15:07:15 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "asyncsendto.h"

/* state diagram for a packet :
 *
 *                     |
 *                     V
 * -> ESCHEDULED -> ESENDNOW -> sent
 *                    ^  |
 *                    |  V
 *                EWAITREADY -> sent
 */
struct scheduled_send {
	LIST_ENTRY(scheduled_send) entries;
	struct timeval ts;
	enum {ESCHEDULED=1, EWAITREADY=2, ESENDNOW=3} state;
	int sockfd;
	const void * buf;
	size_t len;
	int flags;
	const struct sockaddr *dest_addr;
	socklen_t addrlen;
	char data[];
};

static LIST_HEAD(listhead, scheduled_send) send_list = { NULL };

/*
 * ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
 *                const struct sockaddr *dest_addr, socklen_t addrlen);
 */

/* delay = milli seconds */
ssize_t
sendto_schedule(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen,
                unsigned int delay)
{
	enum {ESCHEDULED, EWAITREADY, ESENDNOW} state;
	ssize_t n;
	struct timeval tv;
	struct scheduled_send * elt;

	if(delay == 0) {
		/* first try to send at once */
		n = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
		if(n >= 0)
			return n;
		else if(errno == EAGAIN || errno == EWOULDBLOCK) {
			/* use select() on this socket */
			state = EWAITREADY;
		} else if(errno == EINTR) {
			state = ESENDNOW;
		} else {
			/* uncatched error */
			return n;
		}
	} else {
		state = ESCHEDULED;
	}

	/* schedule */
	if(gettimeofday(&tv, 0) < 0) {
		return -1;
	}
	/* allocate enough space for structure + buffers */
	elt = malloc(sizeof(struct scheduled_send) + len + addrlen);
	if(elt == NULL) {
		syslog(LOG_ERR, "malloc failed to allocate %u bytes",
		       (unsigned)(sizeof(struct scheduled_send) + len + addrlen));
		return -1;
	}
	elt->state = state;
	/* time the packet should be sent */
	elt->ts.tv_sec = tv.tv_sec + (delay / 1000);
	elt->ts.tv_usec = tv.tv_usec + (delay % 1000) * 1000;
	if(elt->ts.tv_usec > 1000000) {
		elt->ts.tv_sec++;
		elt->ts.tv_usec -= 1000000;
	}
	elt->sockfd = sockfd;
	elt->flags = flags;
	memcpy(elt->data, dest_addr, addrlen);
	elt->dest_addr = (struct sockaddr *)elt->data;
	elt->addrlen = addrlen;
	memcpy(elt->data + addrlen, buf, len);
	elt->buf = (void *)(elt->data + addrlen);
	elt->len = len;
	/* insert */
	LIST_INSERT_HEAD( &send_list, elt, entries);
	return 0;
}


/* try to send at once, and queue the packet if needed */
ssize_t
sendto_or_schedule(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return sendto_schedule(sockfd, buf, len, flags, dest_addr, addrlen, 0);
}

/* get_next_scheduled_send() return number of scheduled send in list */
int get_next_scheduled_send(struct timeval * next_send)
{
	int n = 0;
	struct scheduled_send * elt;
	if(next_send == NULL)
		return -1;
	for(elt = send_list.lh_first; elt != NULL; elt = elt->entries.le_next) {
		if(n == 0 || (elt->ts.tv_sec < next_send->tv_sec) ||
		   (elt->ts.tv_sec == next_send->tv_sec && elt->ts.tv_usec < next_send->tv_usec)) {
			next_send->tv_sec = elt->ts.tv_sec;
			next_send->tv_usec = elt->ts.tv_usec;
		}
		n++;
	}
	return n;
}

/* update writefds for select() call
 * return the number of packets to try to send at once */
int get_sendto_fds(fd_set * writefds, int * max_fd, const struct timeval * now)
{
	int n = 0;
	struct scheduled_send * elt;
	for(elt = send_list.lh_first; elt != NULL; elt = elt->entries.le_next) {
		if(elt->state == EWAITREADY) {
			/* last sendto() call returned EAGAIN/EWOULDBLOCK */
			FD_SET(elt->sockfd, writefds);
			if(elt->sockfd > *max_fd)
				*max_fd = elt->sockfd;
			n++;
		} else if((elt->ts.tv_sec < now->tv_sec) ||
		          (elt->ts.tv_sec == now->tv_sec && elt->ts.tv_usec <= now->tv_usec)) {
			/* we waited long enough, now send ! */
			elt->state = ESENDNOW;
			n++;
		}
	}
	return n;
}

/* executed sendto() when needed */
int try_sendto(fd_set * writefds)
{
	ssize_t n;
	struct scheduled_send * elt;
	struct scheduled_send * next;
	for(elt = send_list.lh_first; elt != NULL; elt = next) {
		next = elt->entries.le_next;
		if((elt->state == ESENDNOW) ||
		   (elt->state == EWAITREADY && FD_ISSET(elt->sockfd, writefds))) {
			syslog(LOG_DEBUG, "try_sendto(): %d bytes on socket %d",
			       (int)elt->len, elt->sockfd);
			n = sendto(elt->sockfd, elt->buf, elt->len, elt->flags,
			           elt->dest_addr, elt->addrlen);
			if(n < 0) {
				if(errno == EINTR) {
					/* retry at once */
					elt->state = ESENDNOW;
					continue;
				} else if(errno == EAGAIN || errno == EWOULDBLOCK) {
					/* retry once the socket is ready for writing */
					elt->state = EWAITREADY;
					continue;
				}
				/* uncatched error */
				/* remove from the list */
				LIST_REMOVE(elt, entries);
				free(elt);
				return n;
			} else {
				/* remove from the list */
				LIST_REMOVE(elt, entries);
				free(elt);
			}
		}
	}
	return 0;
}

/* maximum execution time for finalize_sendto() in milliseconds */
#define FINALIZE_SENDTO_DELAY	(500)

/* empty the list */
void finalize_sendto(void)
{
	ssize_t n;
	struct scheduled_send * elt;
	struct scheduled_send * next;
	fd_set writefds;
	struct timeval deadline;
	struct timeval now;
	struct timeval timeout;
	int max_fd;

	if(gettimeofday(&deadline, NULL) < 0) {
		syslog(LOG_ERR, "gettimeofday: %m");
		return;
	}
	deadline.tv_usec += FINALIZE_SENDTO_DELAY*1000;
	if(deadline.tv_usec > 1000000) {
		deadline.tv_sec++;
		deadline.tv_usec -= 1000000;
	}
	while(send_list.lh_first) {
		FD_ZERO(&writefds);
		max_fd = -1;
		for(elt = send_list.lh_first; elt != NULL; elt = next) {
			next = elt->entries.le_next;
			syslog(LOG_DEBUG, "finalize_sendto(): %d bytes on socket %d",
			       (int)elt->len, elt->sockfd);
			n = sendto(elt->sockfd, elt->buf, elt->len, elt->flags,
			           elt->dest_addr, elt->addrlen);
			if(n < 0) {
				if(errno==EAGAIN || errno==EWOULDBLOCK) {
					FD_SET(elt->sockfd, &writefds);
					if(elt->sockfd > max_fd)
						max_fd = elt->sockfd;
					continue;
				}
				syslog(LOG_WARNING, "finalize_sendto(): socket=%d sendto: %m", elt->sockfd);
			}
			/* remove from the list */
			LIST_REMOVE(elt, entries);
			free(elt);
		}
		/* check deadline */
		if(gettimeofday(&now, NULL) < 0) {
			syslog(LOG_ERR, "gettimeofday: %m");
			return;
		}
		if(now.tv_sec > deadline.tv_sec ||
		   (now.tv_sec == deadline.tv_sec && now.tv_usec > deadline.tv_usec)) {
			/* deadline ! */
			while((elt = send_list.lh_first) != NULL) {
				LIST_REMOVE(elt, entries);
				free(elt);
			}
			return;
		}
		/* compute timeout value */
		timeout.tv_sec = deadline.tv_sec - now.tv_sec;
		timeout.tv_usec = deadline.tv_usec - now.tv_usec;
		if(timeout.tv_usec < 0) {
			timeout.tv_sec--;
			timeout.tv_usec += 1000000;
		}
		if(max_fd >= 0) {
			if(select(max_fd + 1, NULL, &writefds, NULL, &timeout) < 0) {
				syslog(LOG_ERR, "select: %m");
				return;
			}
		}
	}
}

