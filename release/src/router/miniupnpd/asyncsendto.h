/* $Id: asyncsendto.h,v 1.1 2014/02/25 10:55:24 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef ASYNCSENDTO_H_INCLUDED
#define ASYNCSENDTO_H_INCLUDED

/* sendto_schedule() : see sendto(2)
 * schedule sendto() call after delay (milliseconds) */
ssize_t
sendto_schedule(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen,
                unsigned int delay);

/* sendto_schedule() : see sendto(2)
 * try sendto() at once and schedule if EINTR/EAGAIN/EWOULDBLOCK */
ssize_t
sendto_or_schedule(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen);

/* get_next_scheduled_send()
 * return number of scheduled sendto
 * set next_send to timestamp to send next packet */
int get_next_scheduled_send(struct timeval * next_send);

/* execute sendto() for needed packets */
int try_sendto(fd_set * writefds);

/* set writefds before select() */
int get_sendto_fds(fd_set * writefds, int * max_fd, const struct timeval * now);

/* empty the list */
void finalize_sendto(void);

#endif
