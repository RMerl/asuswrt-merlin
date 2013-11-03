/*
 *
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 *
 * 7 Oct 1997 added checks for 0 data.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* this streams writes */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>

#include <atalk/dsi.h>
#include <atalk/util.h>
#include <atalk/logger.h>

/* initialize relevant things for dsi_write. this returns the amount
 * of data in the data buffer. the interface has been reworked to allow
 * for arbitrary buffers. */
size_t dsi_writeinit(DSI *dsi, void *buf, const size_t buflen _U_)
{
  size_t len, header;

  /* figure out how much data we have. do a couple checks for 0 
   * data */
  header = ntohl(dsi->header.dsi_data.dsi_doff);
  dsi->datasize = header ? ntohl(dsi->header.dsi_len) - header : 0;

  if (dsi->datasize > 0) {
      len = MIN(dsi->server_quantum - header, dsi->datasize);

      /* write last part of command buffer into buf */
      memmove(buf, dsi->commands + header, len);

      /* recalculate remaining data */
      dsi->datasize -= len;
  } else
    len = 0;

  LOG(log_maxdebug, logtype_dsi, "dsi_writeinit: len: %ju, remaining DSI datasize: %jd",
      (intmax_t)len, (intmax_t)dsi->datasize);

  return len;
}

/* fill up buf and then return. this should be called repeatedly
 * until all the data has been read. i block alarm processing 
 * during the transfer to avoid sending unnecessary tickles. */
size_t dsi_write(DSI *dsi, void *buf, const size_t buflen)
{
  size_t length;

  LOG(log_maxdebug, logtype_dsi, "dsi_write: remaining DSI datasize: %jd", (intmax_t)dsi->datasize);

  if ((length = MIN(buflen, dsi->datasize)) > 0) {
      if ((length = dsi_stream_read(dsi, buf, length)) > 0) {
          LOG(log_maxdebug, logtype_dsi, "dsi_write: received: %ju", (intmax_t)length);
          dsi->datasize -= length;
          return length;
      }
  }
  return 0;
}

/* flush any unread buffers. */
void dsi_writeflush(DSI *dsi)
{
  size_t length;

  while (dsi->datasize > 0) { 
    length = dsi_stream_read(dsi, dsi->data,
			     MIN(sizeof(dsi->data), dsi->datasize));
    if (length > 0)
      dsi->datasize -= length;
    else
      break;
  }
}
