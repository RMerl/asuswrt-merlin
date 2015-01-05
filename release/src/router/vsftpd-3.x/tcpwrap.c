/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * tcpwrap.c
 *
 * Routines to encapsulate the usage of tcp_wrappers.
 */

#include "tcpwrap.h"
#include "builddefs.h"
#include "utility.h"
#include "sysutil.h"

#ifdef VSF_BUILD_TCPWRAPPERS
  #include <tcpd.h>
#endif

#ifdef VSF_BUILD_TCPWRAPPERS

#include <sys/syslog.h>

int deny_severity = LOG_WARNING;
int allow_severity = LOG_INFO;

int
vsf_tcp_wrapper_ok(int remote_fd)
{
  struct request_info req;
  vsf_sysutil_openlog(0);
  request_init(&req, RQ_DAEMON, "vsftpd", RQ_FILE, remote_fd, 0);
  fromhost(&req);
  if (!hosts_access(&req))
  {
    vsf_sysutil_closelog();
    return 0;
  }
  vsf_sysutil_closelog();
  return 1;
}

#else /* VSF_BUILD_TCPWRAPPERS */

int
vsf_tcp_wrapper_ok(int remote_fd)
{
  (void) remote_fd;
  die("tcp_wrappers is set to YES but no tcp wrapper support compiled in");
  return 0;
}

#endif /* VSF_BUILD_TCPWRAPPERS */

