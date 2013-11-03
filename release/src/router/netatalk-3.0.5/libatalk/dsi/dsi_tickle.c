/*
 *
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>

#include <atalk/dsi.h>

/* server generated tickles. as this is only called by the tickle handler,
 * we don't need to block signals. */
int dsi_tickle(DSI *dsi)
{
  char block[DSI_BLOCKSIZ];
  uint16_t id;
  
  if ((dsi->flags & DSI_SLEEPING) || dsi->in_write)
      return 1;

  id = htons(dsi_serverID(dsi));

  memset(block, 0, sizeof(block));
  block[0] = DSIFL_REQUEST;
  block[1] = DSIFUNC_TICKLE;
  memcpy(block + 2, &id, sizeof(id));
  /* code = len = reserved = 0 */

  return dsi_stream_write(dsi, block, DSI_BLOCKSIZ, DSI_NOWAIT);
}

