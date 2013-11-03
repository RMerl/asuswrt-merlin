/*
 *
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <atalk/logger.h>
#include <atalk/util.h>

#include <atalk/dsi.h>
#include <atalk/server_child.h>

/*!
 * Start a DSI session, fork an afpd process
 *
 * @param childp    (w) after fork: parent return pointer to child, child returns NULL
 * @returns             0 on sucess, any other value denotes failure
 */
int dsi_getsession(DSI *dsi, server_child_t *serv_children, int tickleval, afp_child_t **childp)
{
  pid_t pid;
  int ipc_fds[2];  
  afp_child_t *child;

  if (socketpair(PF_UNIX, SOCK_STREAM, 0, ipc_fds) < 0) {
      LOG(log_error, logtype_dsi, "dsi_getsess: %s", strerror(errno));
      return -1;
  }

  if (setnonblock(ipc_fds[0], 1) != 0 || setnonblock(ipc_fds[1], 1) != 0) {
      LOG(log_error, logtype_dsi, "dsi_getsess: setnonblock: %s", strerror(errno));
      return -1;
  }

  switch (pid = dsi->proto_open(dsi)) { /* in libatalk/dsi/dsi_tcp.c */
  case -1:
    /* if we fail, just return. it might work later */
    LOG(log_error, logtype_dsi, "dsi_getsess: %s", strerror(errno));
    return -1;

  case 0: /* child. mostly handled below. */
    break;

  default: /* parent */
    /* using SIGKILL is hokey, but the child might not have
     * re-established its signal handler for SIGTERM yet. */
    close(ipc_fds[1]);
    if ((child = server_child_add(serv_children, pid, ipc_fds[0])) ==  NULL) {
      LOG(log_error, logtype_dsi, "dsi_getsess: %s", strerror(errno));
      close(ipc_fds[0]);
      dsi->header.dsi_flags = DSIFL_REPLY;
      dsi->header.dsi_data.dsi_code = DSIERR_SERVBUSY;
      dsi_send(dsi);
      dsi->header.dsi_data.dsi_code = DSIERR_OK;
      kill(pid, SIGKILL);
    }
    dsi->proto_close(dsi);
    *childp = child;
    return 0;
  }
  
  /* child: check number of open connections. this is one off the
   * actual count. */
  if ((serv_children->servch_count >= serv_children->servch_nsessions) &&
      (dsi->header.dsi_command == DSIFUNC_OPEN)) {
    LOG(log_info, logtype_dsi, "dsi_getsess: too many connections");
    dsi->header.dsi_flags = DSIFL_REPLY;
    dsi->header.dsi_data.dsi_code = DSIERR_TOOMANY;
    dsi_send(dsi);
    exit(EXITERR_CLNT);
  }

  /* get rid of some stuff */
  dsi->AFPobj->ipc_fd = ipc_fds[1];
  close(ipc_fds[0]);
  close(dsi->serversock);
  dsi->serversock = -1;
  server_child_free(serv_children); 

  switch (dsi->header.dsi_command) {
  case DSIFUNC_STAT: /* send off status and return */
    {
      /* OpenTransport 1.1.2 bug workaround: 
       *
       * OT code doesn't currently handle close sockets well. urk.
       * the workaround: wait for the client to close its
       * side. timeouts prevent indefinite resource use. 
       */
      
      static struct timeval timeout = {120, 0};
      fd_set readfds;
      
      dsi_getstatus(dsi);

      FD_ZERO(&readfds);
      FD_SET(dsi->socket, &readfds);
      free(dsi);
      select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);    
      exit(0);
    }
    break;
    
  case DSIFUNC_OPEN: /* setup session */
    /* set up the tickle timer */
    dsi->timer.it_interval.tv_sec = dsi->timer.it_value.tv_sec = tickleval;
    dsi->timer.it_interval.tv_usec = dsi->timer.it_value.tv_usec = 0;
    dsi_opensession(dsi);
    *childp = NULL;
    return 0;

  default: /* just close */
    LOG(log_info, logtype_dsi, "DSIUnknown %d", dsi->header.dsi_command);
    dsi->proto_close(dsi);
    exit(EXITERR_CLNT);
  }
}
