/* Establishing and handling network connections.
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
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

#include "wget.h"

#include "exits.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/select.h>

#ifndef WINDOWS
# ifdef __VMS
#  include "vms_ip.h"
# else /* def __VMS */
#  include <netdb.h>
# endif /* def __VMS [else] */
# include <netinet/in.h>
# ifndef __BEOS__
#  include <arpa/inet.h>
# endif
#endif /* not WINDOWS */

#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "utils.h"
#include "host.h"
#include "connect.h"
#include "hash.h"

#include <stdint.h>

/* Define sockaddr_storage where unavailable (presumably on IPv4-only
   hosts).  */

#ifndef ENABLE_IPV6
# ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#  define sockaddr_storage sockaddr_in
# endif
#endif /* ENABLE_IPV6 */

/* Fill SA as per the data in IP and PORT.  SA shoult point to struct
   sockaddr_storage if ENABLE_IPV6 is defined, to struct sockaddr_in
   otherwise.  */

static void
sockaddr_set_data (struct sockaddr *sa, const ip_address *ip, int port)
{
  switch (ip->family)
    {
    case AF_INET:
      {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;
        xzero (*sin);
        sin->sin_family = AF_INET;
        sin->sin_port = htons (port);
        sin->sin_addr = ip->data.d4;
        break;
      }
#ifdef ENABLE_IPV6
    case AF_INET6:
      {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
        xzero (*sin6);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons (port);
        sin6->sin6_addr = ip->data.d6;
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
        sin6->sin6_scope_id = ip->ipv6_scope;
#endif
        break;
      }
#endif /* ENABLE_IPV6 */
    default:
      abort ();
    }
}

/* Get the data of SA, specifically the IP address and the port.  If
   you're not interested in one or the other information, pass NULL as
   the pointer.  */

static void
sockaddr_get_data (const struct sockaddr *sa, ip_address *ip, int *port)
{
  switch (sa->sa_family)
    {
    case AF_INET:
      {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;
        if (ip)
          {
            ip->family = AF_INET;
            ip->data.d4 = sin->sin_addr;
          }
        if (port)
          *port = ntohs (sin->sin_port);
        break;
      }
#ifdef ENABLE_IPV6
    case AF_INET6:
      {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
        if (ip)
          {
            ip->family = AF_INET6;
            ip->data.d6 = sin6->sin6_addr;
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
            ip->ipv6_scope = sin6->sin6_scope_id;
#endif
          }
        if (port)
          *port = ntohs (sin6->sin6_port);
        break;
      }
#endif
    default:
      abort ();
    }
}

/* Return the size of the sockaddr structure depending on its
   family.  */

static socklen_t
sockaddr_size (const struct sockaddr *sa)
{
  switch (sa->sa_family)
    {
    case AF_INET:
      return sizeof (struct sockaddr_in);
#ifdef ENABLE_IPV6
    case AF_INET6:
      return sizeof (struct sockaddr_in6);
#endif
    default:
      abort ();
    }
}

/* Resolve the bind address specified via --bind-address and store it
   to SA.  The resolved value is stored in a static variable and
   reused after the first invocation of this function.

   Returns true on success, false on failure.  */

static bool
resolve_bind_address (struct sockaddr *sa)
{
  struct address_list *al;

  /* Make sure this is called only once.  opt.bind_address doesn't
     change during a Wget run.  */
  static bool called, should_bind;
  static ip_address ip;
  if (called)
    {
      if (should_bind)
        sockaddr_set_data (sa, &ip, 0);
      return should_bind;
    }
  called = true;

  al = lookup_host (opt.bind_address, LH_BIND | LH_SILENT);
  if (!al)
    {
      /* #### We should be able to print the error message here. */
      logprintf (LOG_NOTQUIET,
                 _("%s: unable to resolve bind address %s; disabling bind.\n"),
                 exec_name, quote (opt.bind_address));
      should_bind = false;
      return false;
    }

  /* Pick the first address in the list and use it as bind address.
     Perhaps we should try multiple addresses in succession, but I
     don't think that's necessary in practice.  */
  ip = *address_list_address_at (al, 0);
  address_list_release (al);

  sockaddr_set_data (sa, &ip, 0);
  should_bind = true;
  return true;
}

struct cwt_context {
  int fd;
  const struct sockaddr *addr;
  socklen_t addrlen;
  int result;
};

static void
connect_with_timeout_callback (void *arg)
{
  struct cwt_context *ctx = (struct cwt_context *)arg;
  ctx->result = connect (ctx->fd, ctx->addr, ctx->addrlen);
}

/* Like connect, but specifies a timeout.  If connecting takes longer
   than TIMEOUT seconds, -1 is returned and errno is set to
   ETIMEDOUT.  */

static int
connect_with_timeout (int fd, const struct sockaddr *addr, socklen_t addrlen,
                      double timeout)
{
  struct cwt_context ctx;
  ctx.fd = fd;
  ctx.addr = addr;
  ctx.addrlen = addrlen;

  if (run_with_timeout (timeout, connect_with_timeout_callback, &ctx))
    {
      errno = ETIMEDOUT;
      return -1;
    }
  if (ctx.result == -1 && errno == EINTR)
    errno = ETIMEDOUT;
  return ctx.result;
}

/* Connect via TCP to the specified address and port.

   If PRINT is non-NULL, it is the host name to print that we're
   connecting to.  */

int
connect_to_ip (const ip_address *ip, int port, const char *print)
{
  struct sockaddr_storage ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  int sock;

  /* If PRINT is non-NULL, print the "Connecting to..." line, with
     PRINT being the host name we're connecting to.  */
  if (print)
    {
      const char *txt_addr = print_address (ip);
      if (0 != strcmp (print, txt_addr))
        {
          char *str = NULL, *name;

          if (opt.enable_iri && (name = idn_decode ((char *) print)) != NULL)
            {
              str = aprintf ("%s (%s)", name, print);
              xfree (name);
            }

          logprintf (LOG_VERBOSE, _("Connecting to %s|%s|:%d... "),
                     str ? str : escnonprint_uri (print), txt_addr, port);

          xfree (str);
        }
      else
        {
           if (ip->family == AF_INET)
               logprintf (LOG_VERBOSE, _("Connecting to %s:%d... "), txt_addr, port);
#ifdef ENABLE_IPV6
           else if (ip->family == AF_INET6)
               logprintf (LOG_VERBOSE, _("Connecting to [%s]:%d... "), txt_addr, port);
#endif
        }
    }

  /* Store the sockaddr info to SA.  */
  sockaddr_set_data (sa, ip, port);

  /* Create the socket of the family appropriate for the address.  */
  sock = socket (sa->sa_family, SOCK_STREAM, 0);
  if (sock < 0)
    goto err;

#if defined(ENABLE_IPV6) && defined(IPV6_V6ONLY)
  if (opt.ipv6_only) {
    int on = 1;
    /* In case of error, we will go on anyway... */
    int err = setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof (on));
    IF_DEBUG
      if (err < 0)
        DEBUGP (("Failed setting IPV6_V6ONLY: %s", strerror (errno)));
  }
#endif

  /* For very small rate limits, set the buffer size (and hence,
     hopefully, the kernel's TCP window size) to the per-second limit.
     That way we should never have to sleep for more than 1s between
     network reads.  */
  if (opt.limit_rate && opt.limit_rate < 8192)
    {
      int bufsize = opt.limit_rate;
      if (bufsize < 512)
        bufsize = 512;          /* avoid pathologically small values */
#ifdef SO_RCVBUF
      if (setsockopt (sock, SOL_SOCKET, SO_RCVBUF,
                  (void *) &bufsize, (socklen_t) sizeof (bufsize)))
        logprintf (LOG_NOTQUIET, _("setsockopt SO_RCVBUF failed: %s\n"),
                   strerror (errno));
#endif
      /* When we add limit_rate support for writing, which is useful
         for POST, we should also set SO_SNDBUF here.  */
    }

  if (opt.bind_address)
    {
      /* Bind the client side of the socket to the requested
         address.  */
      struct sockaddr_storage bind_ss;
      struct sockaddr *bind_sa = (struct sockaddr *)&bind_ss;
      if (resolve_bind_address (bind_sa))
        {
          if (bind (sock, bind_sa, sockaddr_size (bind_sa)) < 0)
            goto err;
        }
    }

  /* Connect the socket to the remote endpoint.  */
  if (connect_with_timeout (sock, sa, sockaddr_size (sa),
                            opt.connect_timeout) < 0)
    goto err;

  /* Success. */
  assert (sock >= 0);
  if (print)
    logprintf (LOG_VERBOSE, _("connected.\n"));
  DEBUGP (("Created socket %d.\n", sock));
  return sock;

 err:
  {
    /* Protect errno from possible modifications by close and
       logprintf.  */
    int save_errno = errno;
    if (sock >= 0)
      {
#ifdef WIN32
	/* If the connection timed out, fd_close will hang in Gnulib's
	   close_fd_maybe_socket, inside the call to WSAEnumNetworkEvents.  */
	if (errno != ETIMEDOUT)
#endif
	  fd_close (sock);
      }
    if (print)
      logprintf (LOG_NOTQUIET, _("failed: %s.\n"), strerror (errno));
    errno = save_errno;
    return -1;
  }
}

/* Connect via TCP to a remote host on the specified port.

   HOST is resolved as an Internet host name.  If HOST resolves to
   more than one IP address, they are tried in the order returned by
   DNS until connecting to one of them succeeds.  */

int
connect_to_host (const char *host, int port)
{
  int i, start, end;
  int sock;

  struct address_list *al = lookup_host (host, 0);

 retry:
  if (!al)
    {
      logprintf (LOG_NOTQUIET,
                 _("%s: unable to resolve host address %s\n"),
                 exec_name, quote (host));
      return E_HOST;
    }

  address_list_get_bounds (al, &start, &end);
  for (i = start; i < end; i++)
    {
      const ip_address *ip = address_list_address_at (al, i);
      sock = connect_to_ip (ip, port, host);
      if (sock >= 0)
        {
          /* Success. */
          address_list_set_connected (al);
          address_list_release (al);
          return sock;
        }

      /* The attempt to connect has failed.  Continue with the loop
         and try next address. */

      address_list_set_faulty (al, i);
    }

  /* Failed to connect to any of the addresses in AL. */

  if (address_list_connected_p (al))
    {
      /* We connected to AL before, but cannot do so now.  That might
         indicate that our DNS cache entry for HOST has expired.  */
      address_list_release (al);
      al = lookup_host (host, LH_REFRESH);
      goto retry;
    }
  address_list_release (al);

  return -1;
}

/* Create a socket, bind it to local interface BIND_ADDRESS on port
   *PORT, set up a listen backlog, and return the resulting socket, or
   -1 in case of error.

   BIND_ADDRESS is the address of the interface to bind to.  If it is
   NULL, the socket is bound to the default address.  PORT should
   point to the port number that will be used for the binding.  If
   that number is 0, the system will choose a suitable port, and the
   chosen value will be written to *PORT.

   Calling accept() on such a socket waits for and accepts incoming
   TCP connections.  */

int
bind_local (const ip_address *bind_address, int *port)
{
  int sock;
  struct sockaddr_storage ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;

  /* For setting options with setsockopt. */
  int setopt_val = 1;
  void *setopt_ptr = (void *)&setopt_val;
  socklen_t setopt_size = sizeof (setopt_val);

  sock = socket (bind_address->family, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;

#ifdef SO_REUSEADDR
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, setopt_ptr, setopt_size))
    logprintf (LOG_NOTQUIET, _("setsockopt SO_REUSEADDR failed: %s\n"),
               strerror (errno));
#endif

  xzero (ss);
  sockaddr_set_data (sa, bind_address, *port);
  if (bind (sock, sa, sockaddr_size (sa)) < 0)
    {
      fd_close (sock);
      return -1;
    }
  DEBUGP (("Local socket fd %d bound.\n", sock));

  /* If *PORT is 0, find out which port we've bound to.  */
  if (*port == 0)
    {
      socklen_t addrlen = sockaddr_size (sa);
      if (getsockname (sock, sa, &addrlen) < 0)
        {
          /* If we can't find out the socket's local address ("name"),
             something is seriously wrong with the socket, and it's
             unusable for us anyway because we must know the chosen
             port.  */
          fd_close (sock);
          return -1;
        }
      sockaddr_get_data (sa, NULL, port);
      DEBUGP (("binding to address %s using port %i.\n",
               print_address (bind_address), *port));
    }
  if (listen (sock, 1) < 0)
    {
      fd_close (sock);
      return -1;
    }
  return sock;
}

/* Like a call to accept(), but with the added check for timeout.

   In other words, accept a client connection on LOCAL_SOCK, and
   return the new socket used for communication with the client.
   LOCAL_SOCK should have been bound, e.g. using bind_local().

   The caller is blocked until a connection is established.  If no
   connection is established for opt.connect_timeout seconds, the
   function exits with an error status.  */

int
accept_connection (int local_sock)
{
  int sock;

  /* We don't need the values provided by accept, but accept
     apparently requires them to be present.  */
  struct sockaddr_storage ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t addrlen = sizeof (ss);

  if (opt.connect_timeout)
    {
      int test = select_fd (local_sock, opt.connect_timeout, WAIT_FOR_READ);
      if (test == 0)
        errno = ETIMEDOUT;
      if (test <= 0)
        return -1;
    }
  sock = accept (local_sock, sa, &addrlen);
  DEBUGP (("Accepted client at socket %d.\n", sock));
  return sock;
}

/* Get the IP address associated with the connection on FD and store
   it to IP.  Return true on success, false otherwise.

   If ENDPOINT is ENDPOINT_LOCAL, it returns the address of the local
   (client) side of the socket.  Else if ENDPOINT is ENDPOINT_PEER, it
   returns the address of the remote (peer's) side of the socket.  */

bool
socket_ip_address (int sock, ip_address *ip, int endpoint)
{
  struct sockaddr_storage storage;
  struct sockaddr *sockaddr = (struct sockaddr *) &storage;
  socklen_t addrlen = sizeof (storage);
  int ret;

  memset (sockaddr, 0, addrlen);
  if (endpoint == ENDPOINT_LOCAL)
    ret = getsockname (sock, sockaddr, &addrlen);
  else if (endpoint == ENDPOINT_PEER)
    ret = getpeername (sock, sockaddr, &addrlen);
  else
    abort ();
  if (ret < 0)
    return false;

  memset(ip, 0, sizeof(ip_address));
  ip->family = sockaddr->sa_family;
  switch (sockaddr->sa_family)
    {
#ifdef ENABLE_IPV6
    case AF_INET6:
      {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&storage;
        ip->data.d6 = sa6->sin6_addr;
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
        ip->ipv6_scope = sa6->sin6_scope_id;
#endif
        DEBUGP (("conaddr is: %s\n", print_address (ip)));
        return true;
      }
#endif
    case AF_INET:
      {
        struct sockaddr_in *sa = (struct sockaddr_in *)&storage;
        ip->data.d4 = sa->sin_addr;
        DEBUGP (("conaddr is: %s\n", print_address (ip)));
        return true;
      }
    default:
      abort ();
    }
}

/* Get the socket family of connection on FD and store
   Return family type on success, -1 otherwise.

   If ENDPOINT is ENDPOINT_LOCAL, it returns the sock family of the local
   (client) side of the socket.  Else if ENDPOINT is ENDPOINT_PEER, it
   returns the sock family of the remote (peer's) side of the socket.  */

int
socket_family (int sock, int endpoint)
{
  struct sockaddr_storage storage;
  struct sockaddr *sockaddr = (struct sockaddr *) &storage;
  socklen_t addrlen = sizeof (storage);
  int ret;

  memset (sockaddr, 0, addrlen);

  if (endpoint == ENDPOINT_LOCAL)
    ret = getsockname (sock, sockaddr, &addrlen);
  else if (endpoint == ENDPOINT_PEER)
    ret = getpeername (sock, sockaddr, &addrlen);
  else
    abort ();

  if (ret < 0)
    return -1;

  return sockaddr->sa_family;
}

/* Return true if the error from the connect code can be considered
   retryable.  Wget normally retries after errors, but the exception
   are the "unsupported protocol" type errors (possible on IPv4/IPv6
   dual family systems) and "connection refused".  */

bool
retryable_socket_connect_error (int err)
{
  /* Have to guard against some of these values not being defined.
     Cannot use a switch statement because some of the values might be
     equal.  */
  if (false
#ifdef EAFNOSUPPORT
      || err == EAFNOSUPPORT
#endif
#ifdef EPFNOSUPPORT
      || err == EPFNOSUPPORT
#endif
#ifdef ESOCKTNOSUPPORT          /* no, "sockt" is not a typo! */
      || err == ESOCKTNOSUPPORT
#endif
#ifdef EPROTONOSUPPORT
      || err == EPROTONOSUPPORT
#endif
#ifdef ENOPROTOOPT
      || err == ENOPROTOOPT
#endif
      /* Apparently, older versions of Linux and BSD used EINVAL
         instead of EAFNOSUPPORT and such.  */
      || err == EINVAL
      )
    return false;

  if (!opt.retry_connrefused)
    if (err == ECONNREFUSED
#ifdef ENETUNREACH
        || err == ENETUNREACH   /* network is unreachable */
#endif
#ifdef EHOSTUNREACH
        || err == EHOSTUNREACH  /* host is unreachable */
#endif
        )
      return false;

  return true;
}

/* Wait for a single descriptor to become available, timing out after
   MAXTIME seconds.  Returns 1 if FD is available, 0 for timeout and
   -1 for error.  The argument WAIT_FOR can be a combination of
   WAIT_FOR_READ and WAIT_FOR_WRITE.

   This is a mere convenience wrapper around the select call, and
   should be taken as such (for example, it doesn't implement Wget's
   0-timeout-means-no-timeout semantics.)  */

int
select_fd (int fd, double maxtime, int wait_for)
{
  fd_set fdset;
  fd_set *rd = NULL, *wr = NULL;
  struct timeval tmout;
  int result;

  if (fd >= FD_SETSIZE)
    {
      logprintf (LOG_NOTQUIET, _("Too many fds open.  Cannot use select on a fd >= %d\n"), FD_SETSIZE);
      exit (WGET_EXIT_GENERIC_ERROR);
    }
  FD_ZERO (&fdset);
  FD_SET (fd, &fdset);
  if (wait_for & WAIT_FOR_READ)
    rd = &fdset;
  if (wait_for & WAIT_FOR_WRITE)
    wr = &fdset;

  tmout.tv_sec = (long) maxtime;
  tmout.tv_usec = 1000000 * (maxtime - (long) maxtime);

  do
  {
    result = select (fd + 1, rd, wr, NULL, &tmout);
#ifdef WINDOWS
    /* gnulib select() converts blocking sockets to nonblocking in windows.
       wget uses blocking sockets so we must convert them back to blocking.  */
    set_windows_fd_as_blocking_socket (fd);
#endif
  }
  while (result < 0 && errno == EINTR);

  return result;
}

/* Return true iff the connection to the remote site established
   through SOCK is still open.

   Specifically, this function returns true if SOCK is not ready for
   reading.  This is because, when the connection closes, the socket
   is ready for reading because EOF is about to be delivered.  A side
   effect of this method is that sockets that have pending data are
   considered non-open.  This is actually a good thing for callers of
   this function, where such pending data can only be unwanted
   leftover from a previous request.  */

bool
test_socket_open (int sock)
{
  fd_set check_set;
  struct timeval to;
  int ret = 0;

  if (sock >= FD_SETSIZE)
    {
      logprintf (LOG_NOTQUIET, _("Too many fds open.  Cannot use select on a fd >= %d\n"), FD_SETSIZE);
      exit (WGET_EXIT_GENERIC_ERROR);
    }
  /* Check if we still have a valid (non-EOF) connection.  From Andrew
   * Maholski's code in the Unix Socket FAQ.  */

  FD_ZERO (&check_set);
  FD_SET (sock, &check_set);

  /* Wait one microsecond */
  to.tv_sec = 0;
  to.tv_usec = 1;

  ret = select (sock + 1, &check_set, NULL, NULL, &to);
#ifdef WINDOWS
/* gnulib select() converts blocking sockets to nonblocking in windows.
wget uses blocking sockets so we must convert them back to blocking
*/
  set_windows_fd_as_blocking_socket ( sock );
#endif

  if ( !ret )
    /* We got a timeout, it means we're still connected. */
    return true;
  else
    /* Read now would not wait, it means we have either pending data
       or EOF/error. */
    return false;
}

/* Basic socket operations, mostly EINTR wrappers.  */

static int
sock_read (int fd, char *buf, int bufsize)
{
  int res;
  do
    res = read (fd, buf, bufsize);
  while (res == -1 && errno == EINTR);
  return res;
}

static int
sock_write (int fd, char *buf, int bufsize)
{
  int res;
  do
    res = write (fd, buf, bufsize);
  while (res == -1 && errno == EINTR);
  return res;
}

static int
sock_poll (int fd, double timeout, int wait_for)
{
  return select_fd (fd, timeout, wait_for);
}

static int
sock_peek (int fd, char *buf, int bufsize)
{
  int res;
  do
    res = recv (fd, buf, bufsize, MSG_PEEK);
  while (res == -1 && errno == EINTR);
  return res;
}

static void
sock_close (int fd)
{
  close (fd);
  DEBUGP (("Closed fd %d\n", fd));
}
#undef read
#undef write
#undef close

/* Reading and writing from the network.  We build around the socket
   (file descriptor) API, but support "extended" operations for things
   that are not mere file descriptors under the hood, such as SSL
   sockets.

   That way the user code can call fd_read(fd, ...) and we'll run read
   or SSL_read or whatever is necessary.  */

static struct hash_table *transport_map;
static unsigned int transport_map_modified_tick;

struct transport_info {
  struct transport_implementation *imp;
  void *ctx;
};

/* Register the transport layer operations that will be used when
   reading, writing, and polling FD.

   This should be used for transport layers like SSL that piggyback on
   sockets.  FD should otherwise be a real socket, on which you can
   call getpeername, etc.  */

void
fd_register_transport (int fd, struct transport_implementation *imp, void *ctx)
{
  struct transport_info *info;

  /* The file descriptor must be non-negative to be registered.
     Negative values are ignored by fd_close(), and -1 cannot be used as
     hash key.  */
  assert (fd >= 0);

  info = xnew (struct transport_info);
  info->imp = imp;
  info->ctx = ctx;
  if (!transport_map)
    transport_map = hash_table_new (0, NULL, NULL);
  hash_table_put (transport_map, (void *)(intptr_t) fd, info);
  ++transport_map_modified_tick;
}

/* Return context of the transport registered with
   fd_register_transport.  This assumes fd_register_transport was
   previously called on FD.  */

void *
fd_transport_context (int fd)
{
  struct transport_info *info = hash_table_get (transport_map, (void *)(intptr_t) fd);
  return info ? info->ctx : NULL;
}

/* When fd_read/fd_write are called multiple times in a loop, they should
   remember the INFO pointer instead of fetching it every time.  It is
   not enough to compare FD to LAST_FD because FD might have been
   closed and reopened.  modified_tick ensures that changes to
   transport_map will not be unnoticed.

   This is a macro because we want the static storage variables to be
   per-function.  */

#define LAZY_RETRIEVE_INFO(info) do {                                   \
  static struct transport_info *last_info;                              \
  static int last_fd = -1;                                              \
  static unsigned int last_tick;                                        \
  if (!transport_map)                                                   \
    info = NULL;                                                        \
  else if (last_fd == fd && last_tick == transport_map_modified_tick)   \
    info = last_info;                                                   \
  else                                                                  \
    {                                                                   \
      info = hash_table_get (transport_map, (void *)(intptr_t) fd);     \
      last_fd = fd;                                                     \
      last_info = info;                                                 \
      last_tick = transport_map_modified_tick;                          \
    }                                                                   \
} while (0)

static bool
poll_internal (int fd, struct transport_info *info, int wf, double timeout)
{
  if (timeout == -1)
    timeout = opt.read_timeout;
  if (timeout)
    {
      int test;
      if (info && info->imp->poller)
        test = info->imp->poller (fd, timeout, wf, info->ctx);
      else
        test = sock_poll (fd, timeout, wf);
      if (test == 0)
        errno = ETIMEDOUT;
      if (test <= 0)
        return false;
    }
  return true;
}

/* Read no more than BUFSIZE bytes of data from FD, storing them to
   BUF.  If TIMEOUT is non-zero, the operation aborts if no data is
   received after that many seconds.  If TIMEOUT is -1, the value of
   opt.timeout is used for TIMEOUT.  */

int
fd_read (int fd, char *buf, int bufsize, double timeout)
{
  struct transport_info *info;
  LAZY_RETRIEVE_INFO (info);
  if (!poll_internal (fd, info, WAIT_FOR_READ, timeout))
    return -1;
  if (info && info->imp->reader)
    return info->imp->reader (fd, buf, bufsize, info->ctx);
  else
    return sock_read (fd, buf, bufsize);
}

/* Like fd_read, except it provides a "preview" of the data that will
   be read by subsequent calls to fd_read.  Specifically, it copies no
   more than BUFSIZE bytes of the currently available data to BUF and
   returns the number of bytes copied.  Return values and timeout
   semantics are the same as those of fd_read.

   CAVEAT: Do not assume that the first subsequent call to fd_read
   will retrieve the same amount of data.  Reading can return more or
   less data, depending on the TCP implementation and other
   circumstances.  However, barring an error, it can be expected that
   all the peeked data will eventually be read by fd_read.  */

int
fd_peek (int fd, char *buf, int bufsize, double timeout)
{
  struct transport_info *info;
  LAZY_RETRIEVE_INFO (info);
  if (!poll_internal (fd, info, WAIT_FOR_READ, timeout))
    return -1;
  if (info && info->imp->peeker)
    return info->imp->peeker (fd, buf, bufsize, info->ctx);
  else
    return sock_peek (fd, buf, bufsize);
}

/* Write the entire contents of BUF to FD.  If TIMEOUT is non-zero,
   the operation aborts if no data is received after that many
   seconds.  If TIMEOUT is -1, the value of opt.timeout is used for
   TIMEOUT.  */

int
fd_write (int fd, char *buf, int bufsize, double timeout)
{
  int res;
  struct transport_info *info;
  LAZY_RETRIEVE_INFO (info);

  /* `write' may write less than LEN bytes, thus the loop keeps trying
     it until all was written, or an error occurred.  */
  res = 0;
  while (bufsize > 0)
    {
      if (!poll_internal (fd, info, WAIT_FOR_WRITE, timeout))
        return -1;
      if (info && info->imp->writer)
        res = info->imp->writer (fd, buf, bufsize, info->ctx);
      else
        res = sock_write (fd, buf, bufsize);
      if (res <= 0)
        break;
      buf += res;
      bufsize -= res;
    }
  return res;
}

/* Report the most recent error(s) on FD.  This should only be called
   after fd_* functions, such as fd_read and fd_write, and only if
   they return a negative result.  For errors coming from other calls
   such as setsockopt or fopen, strerror should continue to be
   used.

   If the transport doesn't support error messages or doesn't supply
   one, strerror(errno) is returned.  The returned error message
   should not be used after fd_close has been called.  */

const char *
fd_errstr (int fd)
{
  /* Don't bother with LAZY_RETRIEVE_INFO, as this will only be called
     in case of error, never in a tight loop.  */
  struct transport_info *info = NULL;
  if (transport_map)
    info = hash_table_get (transport_map, (void *)(intptr_t) fd);

  if (info && info->imp->errstr)
    {
      const char *err = info->imp->errstr (fd, info->ctx);
      if (err)
        return err;
      /* else, fall through and print the system error. */
    }
  return strerror (errno);
}

/* Close the file descriptor FD.  */

void
fd_close (int fd)
{
  struct transport_info *info;
  if (fd < 0)
    return;

  /* Don't use LAZY_RETRIEVE_INFO because fd_close() is only called once
     per socket, so that particular optimization wouldn't work.  */
  info = NULL;
  if (transport_map)
    info = hash_table_get (transport_map, (void *)(intptr_t) fd);

  if (info && info->imp->closer)
    info->imp->closer (fd, info->ctx);
  else
    sock_close (fd);

  if (info)
    {
      hash_table_remove (transport_map, (void *)(intptr_t) fd);
      xfree (info);
      ++transport_map_modified_tick;
    }
}
