/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <arpa/inet.h>

#include <atalk/dsi.h>
#include <atalk/logger.h>

/* this assumes that the reply follows right after the command, saving
 * on a couple assignments. specifically, command, requestID, and
 * reserved field are assumed to already be set. */ 
int dsi_cmdreply(DSI *dsi, const int err)
{
    int ret;

    LOG(log_debug, logtype_dsi, "dsi_cmdreply(DSI ID: %u, len: %zd): START",
        dsi->clientID, dsi->datalen);

    dsi->header.dsi_flags = DSIFL_REPLY;
    dsi->header.dsi_len = htonl(dsi->datalen);
    dsi->header.dsi_data.dsi_code = htonl(err);

    ret = dsi_stream_send(dsi, dsi->data, dsi->datalen);

    LOG(log_debug, logtype_dsi, "dsi_cmdreply(DSI ID: %u, len: %zd): END",
        dsi->clientID, dsi->datalen);

    return ret;
}
