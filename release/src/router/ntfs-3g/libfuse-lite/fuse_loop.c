/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_lowlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int fuse_session_loop(struct fuse_session *se)
{
    int res = 0;
    struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
    size_t bufsize = fuse_chan_bufsize(ch);
    char *buf = (char *) malloc(bufsize);
    if (!buf) {
        fprintf(stderr, "fuse: failed to allocate read buffer\n");
        return -1;
    }

    while (!fuse_session_exited(se)) {
        struct fuse_chan *tmpch = ch;
        res = fuse_chan_recv(&tmpch, buf, bufsize);
        if (res == -EINTR)
            continue;
        if (res <= 0)
            break;
        fuse_session_process(se, buf, res, tmpch);
    }

    free(buf);
    fuse_session_reset(se);
    return res < 0 ? -1 : 0;
}
