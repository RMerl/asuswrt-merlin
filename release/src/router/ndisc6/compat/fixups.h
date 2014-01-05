/*
 * Various fixes for obsolete, or plain broken, C libraries.
 */

/***********************************************************************
 *  Copyright © 2006 Rémi Denis-Courmont.                              *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license, or (at  *
 *  your option) any later version.                                    *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *  See the GNU General Public License for more details.               *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, you can get it from:              *
 *  http://www.gnu.org/copyleft/gpl.html                               *
 ***********************************************************************/

#ifdef NDISC6_COMPAT_FIXUPS_H
# error How come you include this header twice?!
#endif

#ifndef HAVE_FDATASYNC
int fdatasync (int fd);
#endif

#ifndef HAVE_INET6_RTH_ADD
# include <sys/types.h>
# include <sys/socket.h>

/* Price of the totally broken OS goes to MacOS X, which declares these
 * functions, but does not define them! Bug inherited from FreeBSD 4 and
 * still not fixed (as of early 2008). */
# include <netinet/in.h>
# define inet6_rth_space working_inet6_rth_space
# define inet6_rth_init working_inet6_rth_init
# define inet6_rth_add working_inet6_rth_add

struct in6_addr;

socklen_t inet6_rth_space (int type, int segments);
void *inet6_rth_init (void *bp, socklen_t bp_len, int type, int segments);
int inet6_rth_add (void *bp, const struct in6_addr *addr);
#endif

#ifndef IPV6_RTHDR_TYPE_0
# define IPV6_RTHDR_TYPE_0 0
#endif

#ifndef HAVE_PPOLL
# include <signal.h>
struct pollfd;
struct timespec;

int ppoll (struct pollfd *restrict fds, int n,
           const struct timespec *restrict ts,
           const sigset_t *restrict sigmask);
#endif

