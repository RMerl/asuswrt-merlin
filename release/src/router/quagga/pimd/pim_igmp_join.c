/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "pim_igmp_join.h"

#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif

#ifndef MCAST_JOIN_SOURCE_GROUP
#define MCAST_JOIN_SOURCE_GROUP 46
struct group_source_req
{
  uint32_t gsr_interface;
  struct sockaddr_storage gsr_group;
  struct sockaddr_storage gsr_source;
};
#endif

int pim_igmp_join_source(int fd, int ifindex,
			 struct in_addr group_addr,
			 struct in_addr source_addr)
{
  struct group_source_req req;
  struct sockaddr_in *group_sa = (struct sockaddr_in *) &req.gsr_group;
  struct sockaddr_in *source_sa = (struct sockaddr_in *) &req.gsr_source;

  memset(group_sa, 0, sizeof(*group_sa));
  group_sa->sin_family = AF_INET;
  group_sa->sin_addr = group_addr;
  group_sa->sin_port = htons(0);

  memset(source_sa, 0, sizeof(*source_sa));
  source_sa->sin_family = AF_INET;
  source_sa->sin_addr = source_addr;
  source_sa->sin_port = htons(0);

  req.gsr_interface = ifindex;

  return setsockopt(fd, SOL_IP, MCAST_JOIN_SOURCE_GROUP,
		    &req, sizeof(req));

  return 0;
}
