/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

#include <atalk/dsi.h>
#include <atalk/util.h>
#include <atalk/logger.h>

/* OpenSession. set up the connection */
void dsi_opensession(DSI *dsi)
{
  uint32_t i = 0; /* this serves double duty. it must be 4-bytes long */
  int offs;

  if (setnonblock(dsi->socket, 1) < 0) {
      LOG(log_error, logtype_dsi, "dsi_opensession: setnonblock: %s", strerror(errno));
      AFP_PANIC("setnonblock error");
  }

  /* parse options */
  while (i < dsi->cmdlen) {
    switch (dsi->commands[i++]) {
    case DSIOPT_ATTNQUANT:
      memcpy(&dsi->attn_quantum, dsi->commands + i + 1, dsi->commands[i]);
      dsi->attn_quantum = ntohl(dsi->attn_quantum);

    case DSIOPT_SERVQUANT: /* just ignore these */
    default:
      i += dsi->commands[i] + 1; /* forward past length tag + length */
      break;
    }
  }

  /* let the client know the server quantum. we don't use the
   * max server quantum due to a bug in appleshare client 3.8.6. */
  dsi->header.dsi_flags = DSIFL_REPLY;
  dsi->header.dsi_data.dsi_code = 0;
  /* dsi->header.dsi_command = DSIFUNC_OPEN;*/

  dsi->cmdlen = 2 * (2 + sizeof(i)); /* length of data. dsi_send uses it. */

  /* DSI Option Server Request Quantum */
  dsi->commands[0] = DSIOPT_SERVQUANT;
  dsi->commands[1] = sizeof(i);
  i = htonl(( dsi->server_quantum < DSI_SERVQUANT_MIN || 
	      dsi->server_quantum > DSI_SERVQUANT_MAX ) ? 
	    DSI_SERVQUANT_DEF : dsi->server_quantum);
  memcpy(dsi->commands + 2, &i, sizeof(i));

  /* AFP replaycache size option */
  offs = 2 + sizeof(i);
  dsi->commands[offs] = DSIOPT_REPLCSIZE;
  dsi->commands[offs+1] = sizeof(i);
  i = htonl(REPLAYCACHE_SIZE);
  memcpy(dsi->commands + offs + 2, &i, sizeof(i));
  dsi_send(dsi);
}
