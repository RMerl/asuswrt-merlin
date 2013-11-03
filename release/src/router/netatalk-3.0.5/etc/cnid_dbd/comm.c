/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 *
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <atalk/standards.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <time.h>

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/compat.h>

#include "db_param.h"
#include "usockfd.h"
#include "comm.h"

/* Length of the space taken up by a padded control message of length len */
#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(len))
#endif


struct connection {
    time_t tm;                    /* When respawned last */
    int    fd;
};

static int   control_fd;
static int   cur_fd;
static struct connection *fd_table;
static int  fd_table_size;
static int  fds_in_use = 0;


static void invalidate_fd(int fd)
{
    int i;

    if (fd == control_fd)
        return;
    for (i = 0; i != fds_in_use; i++)
        if (fd_table[i].fd == fd)
            break;

    assert(i < fds_in_use);

    fds_in_use--;
    fd_table[i] = fd_table[fds_in_use];
    fd_table[fds_in_use].fd = -1;
    close(fd);
    return;
}


/*
 *  Check for client requests. We keep up to fd_table_size open descriptors in
 *  fd_table. If the table is full and we get a new descriptor via
 *  control_fd, we close a random decriptor in the table to make space. The
 *  affected client will automatically reconnect. For an EOF (descriptor is
 *  closed by the client, so a read here returns 0) comm_rcv will take care of
 *  things and clean up fd_table. The same happens for any read/write errors.
 */

static int check_fd(time_t timeout, const sigset_t *sigmask, time_t *now)
{
    int fd;
    fd_set readfds;
    struct timespec tv;
    int ret;
    int i;
    int maxfd = control_fd;
    time_t t;

    FD_ZERO(&readfds);
    FD_SET(control_fd, &readfds);

    for (i = 0; i != fds_in_use; i++) {
        FD_SET(fd_table[i].fd, &readfds);
        if (maxfd < fd_table[i].fd)
            maxfd = fd_table[i].fd;
    }

    tv.tv_nsec = 0;
    tv.tv_sec  = timeout;
    if ((ret = pselect(maxfd + 1, &readfds, NULL, NULL, &tv, sigmask)) < 0) {
        if (errno == EINTR)
            return 0;
        LOG(log_error, logtype_cnid, "error in select: %s",strerror(errno));
        return -1;
    }

    time(&t);
    if (now)
        *now = t;

    if (!ret)
        return 0;


    if (FD_ISSET(control_fd, &readfds)) {
        int    l = 0;

        fd = recv_fd(control_fd, 0);
        if (fd < 0) {
            return -1;
        }
        if (fds_in_use < fd_table_size) {
            fd_table[fds_in_use].fd = fd;
            fd_table[fds_in_use].tm = t;
            fds_in_use++;
        } else {
            time_t older = t;

            for (i = 0; i != fds_in_use; i++) {
                if (older <= fd_table[i].tm) {
                    older = fd_table[i].tm;
                    l = i;
                }
            }
            close(fd_table[l].fd);
            fd_table[l].fd = fd;
            fd_table[l].tm = t;
        }
        return 0;
    }

    for (i = 0; i != fds_in_use; i++) {
        if (FD_ISSET(fd_table[i].fd, &readfds)) {
            fd_table[i].tm = t;
            return fd_table[i].fd;
        }
    }
    /* We should never get here */
    return 0;
}

int comm_init(struct db_param *dbp, int ctrlfd, int clntfd)
{
    int i;

    fds_in_use = 0;
    fd_table_size = dbp->fd_table_size;

    if ((fd_table = malloc(fd_table_size * sizeof(struct connection))) == NULL) {
        LOG(log_error, logtype_cnid, "Out of memory");
        return -1;
    }
    for (i = 0; i != fd_table_size; i++)
        fd_table[i].fd = -1;
    /* from dup2 */
    control_fd = ctrlfd;
#if 0
    int b = 1;
    /* this one dump core in recvmsg, great */
    if ( setsockopt(control_fd, SOL_SOCKET, SO_PASSCRED, &b, sizeof (b)) < 0) {
        LOG(log_error, logtype_cnid, "setsockopt SO_PASSCRED %s",  strerror(errno));
        return -1;
    }
#endif
    /* push the first client fd */
    fd_table[fds_in_use].fd = clntfd;
    fds_in_use++;

    return 0;
}

/* ------------
   nbe of clients
*/
int comm_nbe(void)
{
    return fds_in_use;
}

/* ------------ */
int comm_rcv(struct cnid_dbd_rqst *rqst, time_t timeout, const sigset_t *sigmask, time_t *now)
{
    char *nametmp;
    int b;

    if ((cur_fd = check_fd(timeout, sigmask, now)) < 0)
        return -1;

    if (!cur_fd)
        return 0;

    LOG(log_maxdebug, logtype_cnid, "comm_rcv: got data on fd %u", cur_fd);

    if (setnonblock(cur_fd, 1) != 0) {
        LOG(log_error, logtype_cnid, "comm_rcv: setnonblock: %s", strerror(errno));
        return -1;
    }

    nametmp = (char *)rqst->name;
    if ((b = readt(cur_fd, rqst, sizeof(struct cnid_dbd_rqst), 1, CNID_DBD_TIMEOUT))
        != sizeof(struct cnid_dbd_rqst)) {
        if (b)
            LOG(log_error, logtype_cnid, "error reading message header: %s", strerror(errno));
        invalidate_fd(cur_fd);
        rqst->name = nametmp;
        return 0;
    }
    rqst->name = nametmp;
    if (rqst->namelen && readt(cur_fd, (char *)rqst->name, rqst->namelen, 1, CNID_DBD_TIMEOUT)
        != rqst->namelen) {
        LOG(log_error, logtype_cnid, "error reading message name: %s", strerror(errno));
        invalidate_fd(cur_fd);
        return 0;
    }
    /* We set this to make life easier for logging. None of the other stuff
       needs zero terminated strings. */
    ((char *)(rqst->name))[rqst->namelen] = '\0';

    LOG(log_maxdebug, logtype_cnid, "comm_rcv: got %u bytes", b + rqst->namelen);

    return 1;
}

/* ------------ */
#define USE_WRITEV
int comm_snd(struct cnid_dbd_rply *rply)
{
#ifdef USE_WRITEV
    struct iovec iov[2];
    size_t towrite;
#endif

    if (!rply->namelen) {
        if (write(cur_fd, rply, sizeof(struct cnid_dbd_rply)) != sizeof(struct cnid_dbd_rply)) {
            LOG(log_error, logtype_cnid, "error writing message header: %s", strerror(errno));
            invalidate_fd(cur_fd);
            return 0;
        }
        return 1;
    }
#ifdef USE_WRITEV

    iov[0].iov_base = rply;
    iov[0].iov_len = sizeof(struct cnid_dbd_rply);
    iov[1].iov_base = rply->name;
    iov[1].iov_len = rply->namelen;
    towrite = sizeof(struct cnid_dbd_rply) +rply->namelen;

    if (writev(cur_fd, iov, 2) != towrite) {
        LOG(log_error, logtype_cnid, "error writing message : %s", strerror(errno));
        invalidate_fd(cur_fd);
        return 0;
    }
#else
    if (write(cur_fd, rply, sizeof(struct cnid_dbd_rply)) != sizeof(struct cnid_dbd_rply)) {
        LOG(log_error, logtype_cnid, "error writing message header: %s", strerror(errno));
        invalidate_fd(cur_fd);
        return 0;
    }
    if (write(cur_fd, rply->name, rply->namelen) != rply->namelen) {
        LOG(log_error, logtype_cnid, "error writing message name: %s", strerror(errno));
        invalidate_fd(cur_fd);
        return 0;
    }
#endif
    return 1;
}


