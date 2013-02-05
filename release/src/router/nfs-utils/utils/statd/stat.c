/*
 * Copyright (C) 1995, 1997, 1999 Jeffrey A. Uphoff
 * Modified by Olaf Kirch, 1996.
 *
 * NSM for Linux.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netdb.h>
#include "statd.h"

/* 
 * Services SM_STAT requests.
 *
 * According the the X/Open spec's on this procedure: "Implementations
 * should not rely on this procedure being operative.  In many current
 * implementations of the NSM it will always return a 'STAT_FAIL'
 * status."  My implementation is operative; it returns 'STAT_SUCC'
 * whenever it can resolve the hostname that it's being asked to
 * monitor, and returns 'STAT_FAIL' otherwise.
 *
 * sm_inter.x says the 'state' returned should be
 *   "state number of site sm_name".  It is not clear how to get this.
 * X/Open says:
 *   STAT_SUCC
 *      The NSM will monitor the given host. "sm_stat_res.state" contains
 *      the state of the NSM.
 * Which implies that 'state' is the state number of the *local* NSM.
 * href=http://www.opengroup.org/onlinepubs/9629799/SM_STAT.htm
 *
 * We return the *local* state as
 *   1/ We have easy access to it.
 *   2/ It might be useful to a remote client who needs it and has no
 *      other way to get it.
 *   3/ That's what we always did in the past.
 */
struct sm_stat_res * 
sm_stat_1_svc (struct sm_name *argp, struct svc_req *rqstp)
{
  static sm_stat_res result;

  if (gethostbyname (argp->mon_name) == NULL) {
    note (N_WARNING, "gethostbyname error for %s", argp->mon_name);
    result.res_stat = STAT_FAIL;
    dprintf (N_DEBUG, "STAT_FAIL for %s", argp->mon_name);
  } else {
    result.res_stat = STAT_SUCC;
    dprintf (N_DEBUG, "STAT_SUCC for %s", argp->mon_name);
  }
  result.state = MY_STATE;
  return(&result);
}
