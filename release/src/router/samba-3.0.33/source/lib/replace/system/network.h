#ifndef _system_network_h
#define _system_network_h
/* 
   Unix SMB/CIFS implementation.

   networking system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_UNIXSOCKET
#include <sys/un.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/*
 * The next three defines are needed to access the IPTOS_* options
 * on some systems.
 */

#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINET_IN_IP_H
#include <netinet/in_ip.h>
#endif

#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef SOCKET_WRAPPER
#ifndef SOCKET_WRAPPER_NOT_REPLACE
#define SOCKET_WRAPPER_REPLACE
#endif
#include "lib/socket_wrapper/socket_wrapper.h"
#endif

#ifdef REPLACE_INET_NTOA
char *rep_inet_ntoa(struct in_addr ip);
#define inet_ntoa rep_inet_ntoa
#endif

/*
 * glibc on linux doesn't seem to have MSG_WAITALL
 * defined. I think the kernel has it though..
 */
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

/*
 * Some older systems seem not to have MAXHOSTNAMELEN
 * defined.
 */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 254
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#endif
