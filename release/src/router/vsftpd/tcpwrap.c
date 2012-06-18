/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
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
  request_init(&req, RQ_DAEMON, "vsftpd", RQ_FILE, remote_fd, 0);
  fromhost(&req);
  if (!hosts_access(&req))
  {
    return 0;
  }
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

