/*
 * Copyright (c) 2012, Frank Lahm <franklahm@googlemail.com>
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <atalk/dsi.h>

DSI *dsi_init(AFPObj *obj, const char *hostname, const char *address, const char *port)
{
    DSI		*dsi;

    if ((dsi = (DSI *)calloc(1, sizeof(DSI))) == NULL)
        return NULL;

    dsi->attn_quantum = DSI_DEFQUANT;
    dsi->server_quantum = obj->options.server_quantum;
    dsi->dsireadbuf = obj->options.dsireadbuf;

    /* currently the only transport protocol that exists for dsi */
    if (dsi_tcp_init(dsi, hostname, address, port) != 0) {
        free(dsi);
        dsi = NULL;
    }

    return dsi;
}
