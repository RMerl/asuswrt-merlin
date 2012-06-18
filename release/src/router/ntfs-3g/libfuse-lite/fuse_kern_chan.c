/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_lowlevel.h"
#include "fuse_kernel.h"
#include "fuse_i.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

static int fuse_kern_chan_receive(struct fuse_chan **chp, char *buf,
                                  size_t size)
{
    struct fuse_chan *ch = *chp;
    int err;
    ssize_t res;
    struct fuse_session *se = fuse_chan_session(ch);
    assert(se != NULL);

 restart:
    res = read(fuse_chan_fd(ch), buf, size);
    err = errno;

    if (fuse_session_exited(se))
        return 0;
    if (res == -1) {
        /* ENOENT means the operation was interrupted, it's safe
           to restart */
        if (err == ENOENT)
            goto restart;

        if (err == ENODEV) {
            fuse_session_exit(se);
            return 0;
        }
        /* Errors occuring during normal operation: EINTR (read
           interrupted), EAGAIN (nonblocking I/O), ENODEV (filesystem
           umounted) */
        if (err != EINTR && err != EAGAIN)
            perror("fuse: reading device");
        return -err;
    }
    if ((size_t) res < sizeof(struct fuse_in_header)) {
        fprintf(stderr, "short read on fuse device\n");
        return -EIO;
    }
    return res;
}

static int fuse_kern_chan_send(struct fuse_chan *ch, const struct iovec iov[],
                               size_t count)
{
    if (iov) {
        ssize_t res = writev(fuse_chan_fd(ch), iov, count);
        int err = errno;

        if (res == -1) {
            struct fuse_session *se = fuse_chan_session(ch);

            assert(se != NULL);

            /* ENOENT means the operation was interrupted */
            if (!fuse_session_exited(se) && err != ENOENT)
                perror("fuse: writing device");
            return -err;
        }
    }
    return 0;
}

static void fuse_kern_chan_destroy(struct fuse_chan *ch)
{
    close(fuse_chan_fd(ch));
}

#define MIN_BUFSIZE 0x21000

struct fuse_chan *fuse_kern_chan_new(int fd)
{
    struct fuse_chan_ops op = {
        .receive = fuse_kern_chan_receive,
        .send = fuse_kern_chan_send,
        .destroy = fuse_kern_chan_destroy,
    };
    size_t bufsize = getpagesize() + 0x1000;
    bufsize = bufsize < MIN_BUFSIZE ? MIN_BUFSIZE : bufsize;
    return fuse_chan_new(&op, fd, bufsize, NULL);
}
