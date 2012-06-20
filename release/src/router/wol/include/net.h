/*
 * wol - wake on lan client
 *
 * $Id: net.h,v 1.4 2004/02/05 18:20:07 wol Exp $
 *
 * Copyright (C) 2000,2001,2002,2004 Thomas Krennwallner <krennwallner@aon.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */


#ifndef _NET_H
#define _NET_H

#include <sys/types.h>


int udp_open (void);

int tcp_open (const char *ip_str, unsigned int port);

int net_close (int socket);

ssize_t tcp_send (int socket,
		  const void *buf,
		  size_t len);

ssize_t tcp_recv (int socket,
		  void *buf,
		  size_t len);

ssize_t udp_send (int socket,
		  const char *host_str,
		  unsigned short int port,
		  const void *buf, size_t len);


#endif /* _NET_H */
