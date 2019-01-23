/* Declarations for host.c
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2015 Free Software
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

#ifndef HOST_H
#define HOST_H

#ifdef WINDOWS
# include <winsock2.h>
#else
# ifdef __VMS
#  include "vms_ip.h"
# else /* def __VMS */
#  include <netdb.h>
# endif /* def __VMS [else] */
# include <sys/socket.h>
# include <netinet/in.h>
#ifndef __BEOS__
# include <arpa/inet.h>
#endif
#endif

struct url;
struct address_list;

/* This struct defines an IP address, tagged with family type.  */

typedef struct {
  /* Address family, one of AF_INET or AF_INET6. */
  int family;

  /* The actual data, in the form of struct in_addr or in6_addr: */
  union {
    struct in_addr d4;      /* IPv4 address */
#ifdef ENABLE_IPV6
    struct in6_addr d6;     /* IPv6 address */
#endif
  } data;

  /* Under IPv6 getaddrinfo also returns scope_id.  Since it's
     IPv6-specific it strictly belongs in the above union, but we put
     it here for simplicity.  */
#if defined ENABLE_IPV6 && defined HAVE_SOCKADDR_IN6_SCOPE_ID
  int ipv6_scope;
#endif
} ip_address;

/* IP_INADDR_DATA macro returns a void pointer that can be interpreted
   as a pointer to struct in_addr in IPv4 context or a pointer to
   struct in6_addr in IPv4 context.  This pointer can be passed to
   functions that work on either, such as inet_ntop.  */
#define IP_INADDR_DATA(x) ((void *) &(x)->data)

enum {
  LH_SILENT  = 1,
  LH_BIND    = 2,
  LH_REFRESH = 4
};
struct address_list *lookup_host (const char *, int);

void address_list_get_bounds (const struct address_list *, int *, int *);
const ip_address *address_list_address_at (const struct address_list *, int);
bool address_list_contains (const struct address_list *, const ip_address *);
void address_list_set_faulty (struct address_list *, int);
void address_list_set_connected (struct address_list *);
bool address_list_connected_p (const struct address_list *);
void address_list_release (struct address_list *);

const char *print_address (const ip_address *);
#ifdef ENABLE_IPV6
bool is_valid_ipv6_address (const char *, const char *);
#endif

bool is_valid_ip_address (const char *name);

bool accept_domain (struct url *);
bool sufmatch (const char **, const char *);

void host_cleanup (void);

#endif /* HOST_H */
