/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <atalk/dsi.h>
#include <atalk/util.h>
#include <atalk/logger.h>

/* streaming i/o for afp_read. this is all from the perspective of the
 * client. it basically does the reverse of dsi_write. on first entry,
 * it will send off the header plus whatever is in its command
 * buffer. it returns the amount of stuff still to be read
 * (constrained by the buffer size). */
ssize_t dsi_readinit(DSI *dsi, void *buf, const size_t buflen, const size_t size, const int err)
{
    LOG(log_maxdebug, logtype_dsi, "dsi_readinit: sending %zd bytes from buffer, total size: %zd",
        buflen, size);

    dsi->flags |= DSI_NOREPLY; /* we will handle our own replies */
    dsi->header.dsi_flags = DSIFL_REPLY;
    dsi->header.dsi_len = htonl(size);
    dsi->header.dsi_data.dsi_code = htonl(err);

    dsi->in_write++;
    if (dsi_stream_send(dsi, buf, buflen)) {
        dsi->datasize = size - buflen;
        LOG(log_maxdebug, logtype_dsi, "dsi_readinit: remaining data for sendfile: %zd", dsi->datasize);
        return MIN(dsi->datasize, buflen);
    }

    return -1; /* error */
}

void dsi_readdone(DSI *dsi)
{
    dsi->in_write--;
}

/* send off the data */
ssize_t dsi_read(DSI *dsi, void *buf, const size_t buflen)
{
    size_t len;

    len  = dsi_stream_write(dsi, buf, buflen, 0);

    if (len == buflen) {
        dsi->datasize -= len;
        return MIN(dsi->datasize, buflen);
    }

    return -1;
}
