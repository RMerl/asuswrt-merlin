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
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: linksupport.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _LINKSUPPORT_H_
#define _LINKSUPPORT_H_ 1

#include "config.h"

#if !defined(HAVE_INET_NTOP)
 const char *inet_ntop( int family, const void *addr,
	char *str, size_t len );
#endif
#if !defined(HAVE_INET_PTON)
 int inet_pton( int family, const char *strptr, void *addr );
#endif

/* PROTOTYPES */
int Link_setreuse( int sock );
int Link_setkeepalive( int sock );
int connect_timeout( int timeout,
	int sock, struct sockaddr *name, int namelen);
int getconnection ( char *xhostname,
	int timeout, int connection_type, struct sockaddr *bindto, char *unix_socket_path );
void Set_linger( int sock, int n );
int Link_listen( char *port_name );
int Unix_link_listen( char *unix_socket_path );
int Link_open(char *host, int timeout, struct sockaddr *bindto,
	char *unix_socket_path );
int Link_open_type(char *host, int timeout, int connection_type,
	struct sockaddr *bindto, char * unix_socket_path );
int Link_open_list( char *hostlist, char **result,
	int timeout, struct sockaddr *bindto, char *unix_socket_path );
void Link_close( int *sock );
int Link_send( char *host, int *sock, int timeout,
	char *sendstr, int count, int *ack );
int Link_copy( char *host, int *sock, int readtimeout, int writetimeout,
	char *src, int fd, double pcount);
int Link_dest_port_num( char *port );
int Link_line_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_line_peek(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_file_read(char *host, int *sock, int readtimeout, int writetimeout,
	  int fd, double *count, int *ack );
const char *Link_err_str (int n);
const char *Ack_err_str (int n);
int AF_Protocol(void);
const char *inet_ntop_sockaddr( struct sockaddr *addr,
	char *str, int len );

#endif
