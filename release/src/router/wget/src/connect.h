/* Declarations for connect.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef CONNECT_H
#define CONNECT_H

#include "host.h"       /* for definition of ip_address */

/* Function declarations */

/* Returned by connect_to_host when host name cannot be resolved.  */
enum {
  E_HOST = -100
};
int connect_to_host (const char *, int);
int connect_to_ip (const ip_address *, int, const char *);

int bind_local (const ip_address *, int *);
int accept_connection (int);

enum {
  ENDPOINT_LOCAL,
  ENDPOINT_PEER
};
bool socket_ip_address (int, ip_address *, int);
int  socket_family (int sock, int endpoint);

bool retryable_socket_connect_error (int);

/* Flags for select_fd's WAIT_FOR argument. */
enum {
  WAIT_FOR_READ = 1,
  WAIT_FOR_WRITE = 2
};
int select_fd (int, double, int);
bool test_socket_open (int);

struct transport_implementation {
  int (*reader) (int, char *, int, void *);
  int (*writer) (int, char *, int, void *);
  int (*poller) (int, double, int, void *);
  int (*peeker) (int, char *, int, void *);
  const char *(*errstr) (int, void *);
  void (*closer) (int, void *);
};

void fd_register_transport (int, struct transport_implementation *, void *);
void *fd_transport_context (int);
int fd_read (int, char *, int, double);
int fd_write (int, char *, int, double);
int fd_peek (int, char *, int, double);
const char *fd_errstr (int);
void fd_close (int);

#endif /* CONNECT_H */
