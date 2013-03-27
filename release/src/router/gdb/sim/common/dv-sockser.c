/* Serial port emulation using sockets.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* FIXME: will obviously need to evolve.
   - connectionless sockets might be more appropriate.  */

#include "sim-main.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <signal.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#ifndef __CYGWIN32__
#include <netinet/tcp.h>
#endif

#include "sim-assert.h"
#include "sim-options.h"

#include "dv-sockser.h"

/* Get definitions for both O_NONBLOCK and O_NDELAY.  */

#ifndef O_NDELAY
#ifdef FNDELAY
#define O_NDELAY FNDELAY
#else /* ! defined (FNDELAY) */
#define O_NDELAY 0
#endif /* ! defined (FNDELAY) */
#endif /* ! defined (O_NDELAY) */

#ifndef O_NONBLOCK
#ifdef FNBLOCK
#define O_NONBLOCK FNBLOCK
#else /* ! defined (FNBLOCK) */
#define O_NONBLOCK 0
#endif /* ! defined (FNBLOCK) */
#endif /* ! defined (O_NONBLOCK) */


/* Compromise between eating cpu and properly busy-waiting.
   One could have an option to set this but for now that seems
   like featuritis.  */
#define DEFAULT_TIMEOUT 1000 /* microseconds */

/* FIXME: These should allocated at run time and kept with other simulator
   state (duh...).  Later.  */
const char * sockser_addr = NULL;
/* Timeout in microseconds during status flag computation.
   Setting this to zero achieves proper busy wait semantics but eats cpu.  */
static unsigned int sockser_timeout = DEFAULT_TIMEOUT;
static int sockser_listen_fd = -1;
static int sockser_fd = -1;

/* FIXME: use tree properties when they're ready.  */

typedef enum {
  OPTION_ADDR = OPTION_START
} SOCKSER_OPTIONS;

static DECLARE_OPTION_HANDLER (sockser_option_handler);

static const OPTION sockser_options[] =
{
  { { "sockser-addr", required_argument, NULL, OPTION_ADDR },
      '\0', "SOCKET ADDRESS", "Set serial emulation socket address",
      sockser_option_handler },
  { { NULL, no_argument, NULL, 0 }, '\0', NULL, NULL, NULL }
};

static SIM_RC
sockser_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
			char *arg, int is_command)
{
  switch (opt)
    {
    case OPTION_ADDR :
      sockser_addr = arg;
      break;
    }

  return SIM_RC_OK;
}

static SIM_RC
dv_sockser_init (SIM_DESC sd)
{
  struct hostent *hostent;
  struct sockaddr_in sockaddr;
  char hostname[100];
  const char *port_str;
  int tmp,port;

  if (STATE_ENVIRONMENT (sd) != OPERATING_ENVIRONMENT
      || sockser_addr == NULL)
    return SIM_RC_OK;

  if (*sockser_addr == '/')
    {
      /* support for these can come later */
      sim_io_eprintf (sd, "sockser init: unix domain sockets not supported: `%s'\n",
		      sockser_addr);
      return SIM_RC_FAIL;
    }

  port_str = strchr (sockser_addr, ':');
  if (!port_str)
    {
      sim_io_eprintf (sd, "sockser init: missing port number: `%s'\n",
		      sockser_addr);
      return SIM_RC_FAIL;
    }
  tmp = port_str - sockser_addr;
  if (tmp >= sizeof hostname)
    tmp = sizeof (hostname) - 1;
  strncpy (hostname, sockser_addr, tmp);
  hostname[tmp] = '\000';
  port = atoi (port_str + 1);

  hostent = gethostbyname (hostname);
  if (! hostent)
    {
      sim_io_eprintf (sd, "sockser init: unknown host: %s\n",
		      hostname);
      return SIM_RC_FAIL;
    }

  sockser_listen_fd = socket (PF_INET, SOCK_STREAM, 0);
  if (sockser_listen_fd < 0)
    {
      sim_io_eprintf (sd, "sockser init: unable to get socket: %s\n",
		      strerror (errno));
      return SIM_RC_FAIL;
    }

  sockaddr.sin_family = PF_INET;
  sockaddr.sin_port = htons(port);
  memcpy (&sockaddr.sin_addr.s_addr, hostent->h_addr,
	  sizeof (struct in_addr));

  tmp = 1;
  if (setsockopt (sockser_listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)& tmp, sizeof(tmp)) < 0)
    {
      sim_io_eprintf (sd, "sockser init: unable to set SO_REUSEADDR: %s\n",
		      strerror (errno));
    }
  if (bind (sockser_listen_fd, (struct sockaddr *) &sockaddr, sizeof (sockaddr)) < 0)
    {
      sim_io_eprintf (sd, "sockser init: unable to bind socket address: %s\n",
		      strerror (errno));
      close (sockser_listen_fd);
      sockser_listen_fd = -1;
      return SIM_RC_FAIL;
    }
  if (listen (sockser_listen_fd, 1) < 0)
    {
      sim_io_eprintf (sd, "sockser init: unable to set up listener: %s\n",
		      strerror (errno));
      close (sockser_listen_fd);
      sockser_listen_fd = -1;
      return SIM_RC_OK;
    }

  /* Handle writes to missing client -> SIGPIPE.
     ??? Need a central signal management module.  */
  {
    RETSIGTYPE (*orig) ();
    orig = signal (SIGPIPE, SIG_IGN);
    /* If a handler is already set up, don't mess with it.  */
    if (orig != SIG_DFL && orig != SIG_IGN)
      signal (SIGPIPE, orig);
  }

  return SIM_RC_OK;
}

static void
dv_sockser_uninstall (SIM_DESC sd)
{
  if (sockser_listen_fd != -1)
    {
      close (sockser_listen_fd);
      sockser_listen_fd = -1;
    }
  if (sockser_fd != -1)
    {
      close (sockser_fd);
      sockser_fd = -1;
    }
}

SIM_RC
dv_sockser_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sim_add_option_table (sd, NULL, sockser_options) != SIM_RC_OK)
    return SIM_RC_FAIL;
  sim_module_add_init_fn (sd, dv_sockser_init);
  sim_module_add_uninstall_fn (sd, dv_sockser_uninstall);
  return SIM_RC_OK;
}

static int
connected_p (SIM_DESC sd)
{
  int numfds,flags;
  struct timeval tv;
  fd_set readfds;
  struct sockaddr sockaddr;
  int addrlen;

  if (sockser_listen_fd == -1)
    return 0;

  if (sockser_fd >= 0)
    {
      /* FIXME: has client gone away? */
      return 1;
    }

  /* Not connected.  Connect with a client if there is one.  */

  FD_ZERO (&readfds);
  FD_SET (sockser_listen_fd, &readfds);

  /* ??? One can certainly argue this should be done differently,
     but for now this is sufficient.  */
  tv.tv_sec = 0;
  tv.tv_usec = sockser_timeout;

  numfds = select (sockser_listen_fd + 1, &readfds, 0, 0, &tv);
  if (numfds <= 0)
    return 0;

  addrlen = sizeof (sockaddr);
  sockser_fd = accept (sockser_listen_fd, &sockaddr, &addrlen);
  if (sockser_fd < 0)
    return 0;

  /* Set non-blocking i/o.  */
  flags = fcntl (sockser_fd, F_GETFL);
  flags |= O_NONBLOCK | O_NDELAY;
  if (fcntl (sockser_fd, F_SETFL, flags) == -1)
    {
      sim_io_eprintf (sd, "unable to set nonblocking i/o");
      close (sockser_fd);
      sockser_fd = -1;
      return 0;
    }
  return 1;
}

int
dv_sockser_status (SIM_DESC sd)
{
  int numrfds,numwfds,status;
  struct timeval tv;
  fd_set readfds,writefds;

  /* status to return if the socket isn't set up, or select fails */
  status = DV_SOCKSER_INPUT_EMPTY | DV_SOCKSER_OUTPUT_EMPTY;

  if (! connected_p (sd))
    return status;

  FD_ZERO (&readfds);
  FD_ZERO (&writefds);
  FD_SET (sockser_fd, &readfds);
  FD_SET (sockser_fd, &writefds);

  /* ??? One can certainly argue this should be done differently,
     but for now this is sufficient.  The read is done separately
     from the write to enforce the delay which we heuristically set to
     once every SOCKSER_TIMEOUT_FREQ tries.
     No, this isn't great for SMP situations, blah blah blah.  */

  {
    static int n;
#define SOCKSER_TIMEOUT_FREQ 42
    if (++n == SOCKSER_TIMEOUT_FREQ)
      n = 0;
    if (n == 0)
      {
	tv.tv_sec = 0;
	tv.tv_usec = sockser_timeout;
	numrfds = select (sockser_fd + 1, &readfds, 0, 0, &tv);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	numwfds = select (sockser_fd + 1, 0, &writefds, 0, &tv);
      }
    else /* do both selects at once */
      {
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	numrfds = numwfds = select (sockser_fd + 1, &readfds, &writefds, 0, &tv);
      }
  }

  status = 0;
  if (numrfds <= 0 || ! FD_ISSET (sockser_fd, &readfds))
    status |= DV_SOCKSER_INPUT_EMPTY;
  if (numwfds <= 0 || FD_ISSET (sockser_fd, &writefds))
    status |= DV_SOCKSER_OUTPUT_EMPTY;
  return status;
}

int
dv_sockser_write (SIM_DESC sd, unsigned char c)
{
  int n;

  if (! connected_p (sd))
    return -1;
  n = write (sockser_fd, &c, 1);
  if (n == -1)
    {
      if (errno == EPIPE)
	{
	  close (sockser_fd);
	  sockser_fd = -1;
	}
      return -1;
    }
  if (n != 1)
    return -1;
  return 1;
}

int
dv_sockser_read (SIM_DESC sd)
{
  unsigned char c;
  int n;

  if (! connected_p (sd))
    return -1;
  n = read (sockser_fd, &c, 1);
  /* ??? We're assuming semantics that may not be correct for all hosts.
     In particular (from cvssrc/src/server.c), this assumes that we are using
     BSD or POSIX nonblocking I/O.  System V nonblocking I/O returns zero if
     there is nothing to read.  */
  if (n == 0)
    {
      close (sockser_fd);
      sockser_fd = -1;
      return -1;
    }
  if (n != 1)
    return -1;
  return c;
}
