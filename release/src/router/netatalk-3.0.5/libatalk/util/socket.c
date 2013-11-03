/*
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

/*!
 * @file
 * Netatalk utility functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/standards.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/errchk.h>

static char ipv4mapprefix[] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};

/*!
 * @brief set or unset non-blocking IO on a fd
 *
 * @param     fd         (r) File descriptor
 * @param     cmd        (r) 0: disable non-blocking IO, ie block\n
 *                           <>0: enable non-blocking IO
 *
 * @returns   0 on success, -1 on failure
 */
int setnonblock(int fd, int cmd)
{
    int ofdflags;
    int fdflags;

    if ((fdflags = ofdflags = fcntl(fd, F_GETFL, 0)) == -1)
        return -1;

    if (cmd)
        fdflags |= O_NONBLOCK;
    else
        fdflags &= ~O_NONBLOCK;

    if (fdflags != ofdflags)
        if (fcntl(fd, F_SETFL, fdflags) == -1)
            return -1;

    return 0;
}

/*!
 * non-blocking drop-in replacement for read with timeout using select
 *
 * @param socket          (r)  socket, if in blocking mode, pass "setnonblocking" arg as 1
 * @param data            (rw) buffer for the read data
 * @param lenght          (r)  how many bytes to read
 * @param setnonblocking  (r)  when non-zero this func will enable and disable non blocking
 *                             io mode for the socket
 * @param timeout         (r)  number of seconds to try reading, 0 means no timeout
 *
 * @returns number of bytes actually read or -1 on timeout or error
 */
ssize_t readt(int socket, void *data, const size_t length, int setnonblocking, int timeout)
{
    size_t stored = 0;
    ssize_t len = 0;
    struct timeval now, end, tv;
    fd_set rfds;
    int ret;

    FD_ZERO(&rfds);

    if (setnonblocking) {
        if (setnonblock(socket, 1) != 0)
            return -1;
    }

    /* Calculate end time */
    if (timeout) {
        (void)gettimeofday(&now, NULL);
        end = now;
        end.tv_sec += timeout;
    }

    while (stored < length) {
        len = recv(socket, (char *) data + stored, length - stored, 0);
        if (len == -1) {
            switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                FD_SET(socket, &rfds);
                if (timeout) {
                    tv.tv_usec = 0;
                    tv.tv_sec  = timeout;
                }

                while ((ret = select(socket + 1, &rfds, NULL, NULL, timeout ? &tv : NULL)) < 1) {
                    switch (ret) {
                    case 0:
                        LOG(log_debug, logtype_dsi, "select timeout %d s", timeout);
                        errno = EAGAIN;
                        goto exit;

                    default: /* -1 */
                        switch (errno) {
                        case EINTR:
                            if (timeout) {
                                (void)gettimeofday(&now, NULL);
                                if (now.tv_sec > end.tv_sec
                                    ||
                                    (now.tv_sec == end.tv_sec && now.tv_usec >= end.tv_usec)) {
                                    LOG(log_debug, logtype_afpd, "select timeout %d s", timeout);
                                    goto exit;
                                }
                                if (now.tv_usec > end.tv_usec) {
                                    tv.tv_usec = 1000000 + end.tv_usec - now.tv_usec;
                                    tv.tv_sec  = end.tv_sec - now.tv_sec - 1;
                                } else {
                                    tv.tv_usec = end.tv_usec - now.tv_usec;
                                    tv.tv_sec  = end.tv_sec - now.tv_sec;
                                }
                            }
                            FD_SET(socket, &rfds);
                            continue;
                        case EBADF:
                            /* possibly entered disconnected state, don't spam log here */
                            LOG(log_debug, logtype_afpd, "select: %s", strerror(errno));
                            stored = -1;
                            goto exit;
                        default:
                            LOG(log_error, logtype_afpd, "select: %s", strerror(errno));
                            stored = -1;
                            goto exit;
                        }
                    }
                } /* while (select) */
                continue;
            } /* switch (errno) */
            LOG(log_error, logtype_afpd, "read: %s", strerror(errno));
            stored = -1;
            goto exit;
        } /* (len == -1) */
        else if (len > 0)
            stored += len;
        else
            break;
    } /* while (stored < length) */

exit:
    if (setnonblocking) {
        if (setnonblock(socket, 0) != 0)
            return -1;
    }

    if (len == -1 && stored == 0)
        /* last read or select got an error and we haven't got yet anything => return -1*/
        return -1;
    return stored;
}

/*!
 * non-blocking drop-in replacement for read with timeout using select
 *
 * @param socket          (r)  socket, if in blocking mode, pass "setnonblocking" arg as 1
 * @param data            (rw) buffer for the read data
 * @param lenght          (r)  how many bytes to read
 * @param setnonblocking  (r)  when non-zero this func will enable and disable non blocking
 *                             io mode for the socket
 * @param timeout         (r)  number of seconds to try reading
 *
 * @returns number of bytes actually read or -1 on fatal error
 */
ssize_t writet(int socket, void *data, const size_t length, int setnonblocking, int timeout)
{
    size_t stored = 0;
    ssize_t len = 0;
    struct timeval now, end, tv;
    fd_set rfds;
    int ret;

    if (setnonblocking) {
        if (setnonblock(socket, 1) != 0)
            return -1;
    }

    /* Calculate end time */
    (void)gettimeofday(&now, NULL);
    end = now;
    end.tv_sec += timeout;

    while (stored < length) {
        len = write(socket, (char *) data + stored, length - stored);
        if (len == -1) {
            switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                FD_ZERO(&rfds);
                FD_SET(socket, &rfds);
                tv.tv_usec = 0;
                tv.tv_sec  = timeout;
                        
                while ((ret = select(socket + 1, &rfds, NULL, NULL, &tv)) < 1) {
                    switch (ret) {
                    case 0:
                        LOG(log_warning, logtype_afpd, "select timeout %d s", timeout);
                        goto exit;

                    default: /* -1 */
                        if (errno == EINTR) {
                            (void)gettimeofday(&now, NULL);
                            if (now.tv_sec >= end.tv_sec && now.tv_usec >= end.tv_usec) {
                                LOG(log_warning, logtype_afpd, "select timeout %d s", timeout);
                                goto exit;
                            }
                            if (now.tv_usec > end.tv_usec) {
                                tv.tv_usec = 1000000 + end.tv_usec - now.tv_usec;
                                tv.tv_sec  = end.tv_sec - now.tv_sec - 1;
                            } else {
                                tv.tv_usec = end.tv_usec - now.tv_usec;
                                tv.tv_sec  = end.tv_sec - now.tv_sec;
                            }
                            FD_ZERO(&rfds);
                            FD_SET(socket, &rfds);
                            continue;
                        }
                        LOG(log_error, logtype_afpd, "select: %s", strerror(errno));
                        stored = -1;
                        goto exit;
                    }
                } /* while (select) */
                continue;
            } /* switch (errno) */
            LOG(log_error, logtype_afpd, "read: %s", strerror(errno));
            stored = -1;
            goto exit;
        } /* (len == -1) */
        else if (len > 0)
            stored += len;
        else
            break;
    } /* while (stored < length) */

exit:
    if (setnonblocking) {
        if (setnonblock(socket, 0) != 0)
            return -1;
    }

    if (len == -1 && stored == 0)
        /* last read or select got an error and we haven't got yet anything => return -1*/
        return -1;
    return stored;
}

/*!
 * @brief convert an IPv4 or IPv6 address to a static string using inet_ntop
 *
 * IPv6 mapped IPv4 addresses are returned as IPv4 addreses eg
 * ::ffff:10.0.0.0 is returned as "10.0.0.0".
 *
 * @param  sa        (r) pointer to an struct sockaddr
 *
 * @returns pointer to a static string cotaining the converted address as string.\n
 *          On error pointers to "0.0.0.0" or "::0" are returned.
 */
const char *getip_string(const struct sockaddr *sa)
{
    static char ip4[INET_ADDRSTRLEN];
    static char ip6[INET6_ADDRSTRLEN];

    switch (sa->sa_family) {

    case AF_INET: {
        const struct sockaddr_in *sai4 = (const struct sockaddr_in *)sa;
        if ((inet_ntop(AF_INET, &(sai4->sin_addr), ip4, INET_ADDRSTRLEN)) == NULL)
            return "0.0.0.0";
        return ip4;
    }
    case AF_INET6: {
        const struct sockaddr_in6 *sai6 = (const struct sockaddr_in6 *)sa;
        if ((inet_ntop(AF_INET6, &(sai6->sin6_addr), ip6, INET6_ADDRSTRLEN)) == NULL)
            return "::0";

        /* Deal with IPv6 mapped IPv4 addresses*/
        if ((memcmp(sai6->sin6_addr.s6_addr, ipv4mapprefix, sizeof(ipv4mapprefix))) == 0)
            return (strrchr(ip6, ':') + 1);
        return ip6;
    }
    default:
        return "getip_string ERROR";
    }

    /* We never get here */
}

/*!
 * @brief return port number from struct sockaddr
 *
 * @param  sa        (r) pointer to an struct sockaddr
 *
 * @returns port as unsigned int
 */
unsigned int getip_port(const struct sockaddr  *sa)
{
    if (sa->sa_family == AF_INET) { /* IPv4 */
        const struct sockaddr_in *sai4 = (const struct sockaddr_in *)sa;
        return ntohs(sai4->sin_port);
    } else {                       /* IPv6 */
        const struct sockaddr_in6 *sai6 = (const struct sockaddr_in6 *)sa;
        return ntohs(sai6->sin6_port);
    }

    /* We never get here */
}

/*!
 * @brief apply netmask to IP (v4 or v6)
 *
 * Modifies IP address in sa->sin[6]_addr-s[6]_addr. The caller is responsible
 * for passing a value for mask that is sensible to the passed address,
 * eg 0 <= mask <= 32 for IPv4 or 0<= mask <= 128 for IPv6. mask > 32 for
 * IPv4 is treated as mask = 32, mask > 128 is set to 128 for IPv6.
 *
 * @param  ai        (rw) pointer to an struct sockaddr
 * @parma  mask      (r) number of maskbits
 */
void apply_ip_mask(struct sockaddr *sa, int mask)
{

    switch (sa->sa_family) {
    case AF_INET: {
        if (mask >= 32)
            return;

        struct sockaddr_in *si = (struct sockaddr_in *)sa;
        uint32_t nmask = mask ? ~((1 << (32 - mask)) - 1) : 0;
        si->sin_addr.s_addr &= htonl(nmask);
        break;
    }
    case AF_INET6: {
        if (mask >= 128)
            return;

        int i, maskbytes, maskbits;
        struct sockaddr_in6 *si6 = (struct sockaddr_in6 *)sa;

        /* Deal with IPv6 mapped IPv4 addresses*/
        if ((memcmp(si6->sin6_addr.s6_addr, ipv4mapprefix, sizeof(ipv4mapprefix))) == 0) {
            mask += 96;
            if (mask >= 128)
                return;
        }

        maskbytes = (128 - mask) / 8; /* maskbytes really are those that will be 0'ed */
        maskbits = mask % 8;

        for (i = maskbytes - 1; i >= 0; i--)
            si6->sin6_addr.s6_addr[15 - i] = 0;
        if (maskbits)
            si6->sin6_addr.s6_addr[15 - maskbytes] &= ~((1 << (8 - maskbits)) - 1);
        break;
    }
    default:
        break;
    }
}

/*!
 * @brief compare IP addresses for equality
 *
 * @param  sa1       (r) pointer to an struct sockaddr
 * @param  sa2       (r) pointer to an struct sockaddr
 *
 * @returns Addresses are converted to strings and compared with strcmp and
 *          the result of strcmp is returned.
 *
 * @note IPv6 mapped IPv4 addresses are treated as IPv4 addresses.
 */
int compare_ip(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
    int ret;
    char *ip1;
    const char *ip2;

    ip1 = strdup(getip_string(sa1));
    ip2 = getip_string(sa2);

    ret = strcmp(ip1, ip2);

    free(ip1);

    return ret;
}

/*!
 * Tokenize IP(4/6) addresses with an optional port into address and port
 *
 * @param ipurl    (r) IP URL string
 * @param address  (w) IP address
 * @param port     (w) IP port
 *
 * @returns 0 on success, -1 on failure
 *
 * Tokenize IPv4, IPv4:port, IPv6, [IPv6] or [IPv6:port] URL into address and
 * port and return two allocated strings with the address and the port.
 *
 * If the function returns 0, then address point to a newly allocated
 * valid address string, port may either be NULL or point to a newly
 * allocated port number.
 *
 * If the function returns -1, then the contents of address and port are
 * undefined.
 */
int tokenize_ip_port(const char *ipurl, char **address, char **port)
{
    EC_INIT;
    char *p = NULL;
    char *s;

    AFP_ASSERT(ipurl && address && port);
    EC_NULL( p = strdup(ipurl));

    /* Either ipv4, ipv4:port, ipv6, [ipv6] or [ipv6]:port */

    if (!strchr(p, ':')) {
        /* IPv4 address without port */
        *address = p;
        p = NULL;  /* prevent free() */
        *port = NULL;
        EC_EXIT_STATUS(0);
    }

    /* Either ipv4:port, ipv6, [ipv6] or [ipv6]:port */

    if (strchr(p, '.')) {
        /* ipv4:port */
        *address = p;
        p = strchr(p, ':');
        *p = '\0';
        EC_NULL( *port = strdup(p + 1));
        p = NULL; /* prevent free() */
        EC_EXIT_STATUS(0);
    }

    /* Either ipv6, [ipv6] or [ipv6]:port */

    if (p[0] != '[') {
        /* ipv6 */
        *address = p;
        p = NULL;  /* prevent free() */
        *port = NULL;
        EC_EXIT_STATUS(0);
    }

    /* [ipv6] or [ipv6]:port */

    EC_NULL( *address = strdup(p + 1) );

    if ((s = strchr(*address, ']')) == NULL) {
        LOG(log_error, logtype_dsi, "tokenize_ip_port: malformed ipv6 address %s\n", ipurl);
        EC_FAIL;
    }
    *s = '\0';
    /* address now points to the ipv6 address without [] */

    if (s[1] == ':') {
        /* [ipv6]:port */
        EC_NULL( *port = strdup(s + 2) );
    } else {
        /* [ipv6] */
        *port = NULL;
    }

EC_CLEANUP:
    if (p)
        free(p);
    EC_EXIT;
}

/*!
 * Add a fd to a dynamic pollfd array that is allocated and grown as needed
 *
 * This uses an additional array of struct polldata which stores type information
 * (enum fdtype) and a pointer to anciliary user data.
 *
 * 1. Allocate the arrays with the size of "maxconns" if *fdsetp is NULL.
 * 2. Fill in both array elements and increase count of used elements
 * 
 * @param maxconns    (r)  maximum number of connections, determines array size
 * @param fdsetp      (rw) pointer to callers pointer to the pollfd array
 * @param polldatap   (rw) pointer to callers pointer to the polldata array
 * @param fdset_usedp (rw) pointer to an int with the number of used elements
 * @param fdset_sizep (rw) pointer to an int which stores the array sizes
 * @param fd          (r)  file descriptor to add to the arrays
 * @param fdtype      (r)  type of fd, currently IPC_FD or LISTEN_FD
 * @param data        (rw) pointer to data the caller want to associate with an fd
 */
void fdset_add_fd(int maxconns,
                  struct pollfd **fdsetp,
                  struct polldata **polldatap,
                  int *fdset_usedp,
                  int *fdset_sizep,
                  int fd,
                  enum fdtype fdtype,
                  void *data)
{
    struct pollfd *fdset = *fdsetp;
    struct polldata *polldata = *polldatap;
    int fdset_size = *fdset_sizep;

    LOG(log_debug, logtype_default, "fdset_add_fd: adding fd %i in slot %i", fd, *fdset_usedp);

    if (fdset == NULL) { /* 1 */
        /* Initialize with space for all possibly active fds */
        fdset = calloc(maxconns, sizeof(struct pollfd));
        if (! fdset)
            exit(EXITERR_SYS);

        polldata = calloc(maxconns, sizeof(struct polldata));
        if (! polldata)
            exit(EXITERR_SYS);

        fdset_size = maxconns;

        *fdset_sizep = fdset_size;
        *fdsetp = fdset;
        *polldatap = polldata;

        LOG(log_debug, logtype_default, "fdset_add_fd: initialized with space for %i conncections", 
            maxconns);
    }

    /* 2 */
    fdset[*fdset_usedp].fd = fd;
    fdset[*fdset_usedp].events = POLLIN;
    polldata[*fdset_usedp].fdtype = fdtype;
    polldata[*fdset_usedp].data = data;
    (*fdset_usedp)++;
}

/*!
 * Remove a fd from our pollfd array
 *
 * 1. Search fd
 * 2a Matched last (or only) in the set ? null it and return
 * 2b If we remove the last array elemnt, just decrease count
 * 3. If found move all following elements down by one
 * 4. Decrease count of used elements in array
 *
 * This currently doesn't shrink the allocated storage of the array.
 *
 * @param fdsetp      (rw) pointer to callers pointer to the pollfd array
 * @param polldatap   (rw) pointer to callers pointer to the polldata array
 * @param fdset_usedp (rw) pointer to an int with the number of used elements
 * @param fdset_sizep (rw) pointer to an int which stores the array sizes
 * @param fd          (r)  file descriptor to remove from the arrays
 */
void fdset_del_fd(struct pollfd **fdsetp,
                  struct polldata **polldatap,
                  int *fdset_usedp,
                  int *fdset_sizep _U_,
                  int fd)
{
    struct pollfd *fdset = *fdsetp;
    struct polldata *polldata = *polldatap;

    if (*fdset_usedp < 1)
        return;

    for (int i = 0; i < *fdset_usedp; i++) {
        if (fdset[i].fd == fd) { /* 1 */
            if ((i + 1) == *fdset_usedp) { /* 2a */
                fdset[i].fd = -1;
                memset(&polldata[i], 0, sizeof(struct polldata));
            } else if (i < (*fdset_usedp - 1)) { /* 2b */
                memmove(&fdset[i],
                        &fdset[i+1],
                        (*fdset_usedp - i - 1) * sizeof(struct pollfd)); /* 3 */
                memmove(&polldata[i],
                        &polldata[i+1],
                        (*fdset_usedp - i - 1) * sizeof(struct polldata)); /* 3 */
            }
            (*fdset_usedp)--;
            break;
        }
    }
}

/* Length of the space taken up by a padded control message of length len */
#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(len))
#endif

/*
 * Receive a fd on a suitable socket
 * @args fd          (r) PF_UNIX socket to receive on
 * @args nonblocking (r) 0: fd is in blocking mode - 1: fd is nonblocking, poll for 1 sec
 * @returns fd on success, -1 on error
 */
int recv_fd(int fd, int nonblocking)
{
    int ret;
    struct msghdr msgh;
    struct iovec iov[1];
    struct cmsghdr *cmsgp = NULL;
    char buf[CMSG_SPACE(sizeof(int))];
    char dbuf[80];
    struct pollfd pollfds[1];

    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;

    memset(&msgh,0,sizeof(msgh));
    memset(buf,0,sizeof(buf));

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;

    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;

    iov[0].iov_base = dbuf;
    iov[0].iov_len = sizeof(dbuf);

    msgh.msg_control = buf;
    msgh.msg_controllen = sizeof(buf);

    if (nonblocking) {
        do {
            ret = poll(pollfds, 1, 2000); /* poll 2 seconds, evtl. multipe times (EINTR) */
        } while ( ret == -1 && errno == EINTR );
        if (ret != 1)
            return -1;
        ret = recvmsg(fd, &msgh, 0);
    } else {
        do  {
            ret = recvmsg(fd, &msgh, 0);
        } while ( ret == -1 && errno == EINTR );
    }

    if ( ret == -1 ) {
        return -1;
    }

    for ( cmsgp = CMSG_FIRSTHDR(&msgh); cmsgp != NULL; cmsgp = CMSG_NXTHDR(&msgh,cmsgp) ) {
        if ( cmsgp->cmsg_level == SOL_SOCKET && cmsgp->cmsg_type == SCM_RIGHTS ) {
            return *(int *) CMSG_DATA(cmsgp);
        }
    }

    if ( ret == sizeof (int) )
        errno = *(int *)dbuf; /* Rcvd errno */
    else
        errno = ENOENT;    /* Default errno */

    return -1;
}

/*
 * Send a fd across a suitable socket
 */
int send_fd(int socket, int fd)
{
    int ret;
    struct msghdr msgh;
    struct iovec iov[1];
    struct cmsghdr *cmsgp = NULL;
    char *buf;
    size_t size;
    int er=0;

    size = CMSG_SPACE(sizeof fd);
    buf = malloc(size);
    if (!buf) {
        LOG(log_error, logtype_cnid, "error in sendmsg: %s", strerror(errno));
        return -1;
    }

    memset(&msgh,0,sizeof (msgh));
    memset(buf,0, size);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;

    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;

    iov[0].iov_base = &er;
    iov[0].iov_len = sizeof(er);

    msgh.msg_control = buf;
    msgh.msg_controllen = size;

    cmsgp = CMSG_FIRSTHDR(&msgh);
    cmsgp->cmsg_level = SOL_SOCKET;
    cmsgp->cmsg_type = SCM_RIGHTS;
    cmsgp->cmsg_len = CMSG_LEN(sizeof(fd));

    *((int *)CMSG_DATA(cmsgp)) = fd;
    msgh.msg_controllen = cmsgp->cmsg_len;

    do  {
        ret = sendmsg(socket,&msgh, 0);
    } while ( ret == -1 && errno == EINTR );
    if (ret == -1) {
        LOG(log_error, logtype_cnid, "error in sendmsg: %s", strerror(errno));
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}
