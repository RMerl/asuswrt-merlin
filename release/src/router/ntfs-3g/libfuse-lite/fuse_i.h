/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB
*/

#include "fuse.h"

struct fuse_session;
struct fuse_chan;
struct fuse_lowlevel_ops;
struct fuse_req;

struct fuse_cmd {
    char *buf;
    size_t buflen;
    struct fuse_chan *ch;
};

struct fuse_chan *fuse_kern_chan_new(int fd);

void fuse_kern_unmount(const char *mountpoint, int fd);
int fuse_kern_mount(const char *mountpoint, struct fuse_args *args);
