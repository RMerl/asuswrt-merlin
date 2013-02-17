/*
 * Copyright (C) 1995, 1997-1999 Jeffrey A. Uphoff
 *
 * NSM for Linux.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arpa/inet.h>

#include "rpcmisc.h"
#include "statd.h"
#include "notlist.h"

extern void my_svc_exit (void);


/*
 * Services SM_SIMU_CRASH requests.
 */
void *
sm_simu_crash_1_svc (void *argp, struct svc_req *rqstp)
{
  struct sockaddr_in *sin = nfs_getrpccaller_in(rqstp->rq_xprt);
  static char *result = NULL;
  struct in_addr caller;

  if (sin->sin_family != AF_INET) {
    note(N_WARNING, "Call to statd from non-AF_INET address");
    goto failure;
  }

  caller = sin->sin_addr;
  if (caller.s_addr != htonl(INADDR_LOOPBACK)) {
    note(N_WARNING, "Call to statd from non-local host %s",
      inet_ntoa(caller));
    goto failure;
  }

  if (ntohs(sin->sin_port) >= 1024) {
    note(N_WARNING, "Call to statd-simu-crash from unprivileged port");
    goto failure;
  }

  note (N_WARNING, "*** SIMULATING CRASH! ***");
  my_svc_exit ();

  if (rtnl)
    nlist_kill (&rtnl);

 failure:
  return ((void *)&result);
}
