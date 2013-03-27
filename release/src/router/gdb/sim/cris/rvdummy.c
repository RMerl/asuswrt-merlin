/* Test-driver for the remote-virtual-component simulator framework
   for GDB, the GNU Debugger.

   Copyright 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

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

/* Avoid any problems whatsoever building this program if we're not
   also building hardware support.  */

#if !WITH_HW
int
main (int argc, char *argv[])
{
  return 2;
}
#else

#ifdef HAVE_CONFIG_H
#include "cconfig.h"
#include "tconfig.h"
#endif

#include "getopt.h"
#include "libiberty.h"

#define _GNU_SOURCE
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* Not guarded in dv-sockser.c, so why here.  */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

enum rv_command {
  RV_READ_CMD = 0,
  RV_WRITE_CMD = 1,
  RV_IRQ_CMD = 2,
  RV_MEM_RD_CMD = 3,
  RV_MEM_WR_CMD = 4,
  RV_MBOX_HANDLE_CMD = 5,
  RV_MBOX_PUT_CMD = 6,
  RV_WATCHDOG_CMD = 7
};

enum opts { OPT_PORT = 1, OPT_TIMEOUT, OPT_VERBOSE };

struct option longopts[] =
  {
    {"port", required_argument, NULL, OPT_PORT},
    {"timeout", required_argument, NULL, OPT_TIMEOUT},
    {"verbose", no_argument, NULL, OPT_VERBOSE},
    {NULL, 0, NULL, 0}
  };

int port = 10000;
time_t timeout = 30000;
char *progname = "(unknown)";
int verbose = 0;

/* Required forward-declarations.  */
static void handle_input_file (int, char *);

/* Set up a "server" listening to the port in PORT for a raw TCP
   connection.  Return a file descriptor for the connection or -1 on
   error.  */

int setupsocket (void)
{
  int s;
  socklen_t len;
  int reuse = 1;
  struct sockaddr_in sa_in;
  struct sockaddr_in from;

  len = sizeof (from);
  memset (&from, 0, len);
  memset (&sa_in, 0, sizeof (sa_in));

  s = socket (AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    return -1;

  if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) != 0)
    return -1;

  sa_in.sin_port = htons (port);
  sa_in.sin_family = AF_INET;

  if (bind (s, (struct sockaddr *) & sa_in, sizeof sa_in) < 0)
    return -1;

  if (listen (s, 1) < 0)
    return -1;

  return accept (s, (struct sockaddr *) &from, &len);
}

/* Basic host-to-little-endian 32-bit value.  Could use the BFD
   machinery, but let's avoid it for this only dependency.  */

static void
h2le32 (unsigned char *dest, unsigned int val)
{
  dest[0] = val & 255;
  dest[1] = (val >> 8) & 255;
  dest[2] = (val >> 16) & 255;
  dest[3] = (val >> 24) & 255;
}

/* Send a blob of data.  */

static void
send_output (int fd, unsigned char *buf, int nbytes)
{
  while (nbytes > 0)
    {
      ssize_t written = write (fd, buf, nbytes);
      if (written < 0)
	{
	  fprintf (stderr, "%s: write to socket failed: %s\n",
		  progname, strerror (errno));
	  exit (2);
	}
      nbytes -= written;
    }
}

/* Receive a blob of data, NBYTES large.  Compare to the first NCOMP
   bytes of BUF; if not a match, write error message to stderr and
   exit (2).  Else put it in buf.  */

static void
expect_input (int fd, unsigned char *buf, int nbytes, int ncomp)
{
  unsigned char byt;
  int i;

  for (i = 0; i < nbytes; i++)
    {
      int r;

      do
	{
	  errno = 0;
	  r = read (fd, &byt, 1);
	}
      while (r <= 0 && (r == 0 || errno == EAGAIN));

      if (r != 1)
	{
	  fprintf (stderr, "%s: read from socket failed: %s",
		  progname, strerror (errno));
	  exit (2);
	}

      if (i < ncomp && byt != buf[i])
	{
	  int j;
	  fprintf (stderr, "%s: unexpected input,\n ", progname);
	  if (i == 0)
	    fprintf (stderr, "nothing,");
	  else
	    for (j = 0; j < i; j++)
	      fprintf (stderr, "%02x", buf[j]);
	  fprintf (stderr, "\nthen %02x instead of %02x\n", byt, buf[i]);
	  exit (2);
	}
      else
	buf[i] = byt;
    }
}

/* Handle everything about a nil-terminated line of input.
   Call exit (2) on error with error text on stderr.  */

static void
handle_input (int fd, char *buf, char *fname, int lineno)
{
  int nbytes = 0;
  int n = -1;
  char *s = buf + 2;
  unsigned int data;
  static unsigned char bytes[1024];
  int i;

  memset (bytes, 0, sizeof bytes);
  lineno++;

  if (buf[1] != ',')
    goto syntax_error;

  switch (buf[0])
    {
      /* Comment characters and empty lines.  */
    case 0: case '!': case '#':
      break;

      /* Include another file.  */
    case '@':
      handle_input_file (fd, s);
      break;

      /* Raw input (to be expected).  */
    case 'i':
      do
	{
	  n = -1;
	  sscanf (s, "%02x%n", &data, &n);
	  s += n;
	  if (n > 0)
	    bytes[nbytes++] = data;
	}
      while (n > 0);
      expect_input (fd, bytes, nbytes, nbytes);
      if (verbose)
	{
	  printf ("i,");
	  for (i = 0; i < nbytes; i++)
	    printf ("%02x", bytes[i]);
	  printf ("\n");
	}
      break;

      /* Raw output (to be written).  */
    case 'o':
      do
	{
	  n = -1;
	  sscanf (s, "%02x%n", &data, &n);
	  if (n > 0)
	    {
	      s += n;
	      bytes[nbytes++] = data;
	    }
	}
      while (n > 0);
      if (*s != 0)
	goto syntax_error;
      send_output (fd, bytes, nbytes);
      if (verbose)
	{
	  printf ("o,");
	  for (i = 0; i < nbytes; i++)
	    printf ("%02x", bytes[i]);
	  printf ("\n");
	}
      break;

      /* Read a register.  */
    case 'r':
      {
	unsigned int addr;
	sscanf (s, "%x,%x%n", &addr, &data, &n);
	if (n < 0 || s[n] != 0)
	  goto syntax_error;
	bytes[0] = 11;
	bytes[1] = 0;
	bytes[2] = RV_READ_CMD;
	h2le32 (bytes + 3, addr);
	expect_input (fd, bytes, 11, 7);
	h2le32 (bytes + 7, data);
	send_output (fd, bytes, 11);
	if (verbose)
	  printf ("r,%x,%x\n", addr, data);
      }
      break;

      /* Write a register.  */
    case 'w':
      {
	unsigned int addr;
	sscanf (s, "%x,%x%n", &addr, &data, &n);
	if (n < 0 || s[n] != 0)
	  goto syntax_error;
	bytes[0] = 11;
	bytes[1] = 0;
	bytes[2] = RV_WRITE_CMD;
	h2le32 (bytes + 3, addr);
	h2le32 (bytes + 7, data);
	expect_input (fd, bytes, 11, 11);
	send_output (fd, bytes, 11);
	if (verbose)
	  printf ("w,%x,%x\n", addr, data);
      }
      break;

      /* Wait for some milliseconds.  */
    case 't':
      {
	int del = 0;
	struct timeval to;
	sscanf (s, "%d%n", &del, &n);
	if (n < 0 || s[n] != 0 || del == 0)
	  goto syntax_error;

	to.tv_sec = del / 1000;
	to.tv_usec = (del % 1000) * 1000;

	if (select (0, NULL, NULL, NULL, &to) != 0)
	  {
	    fprintf (stderr, "%s: problem waiting for %d ms:\n %s\n",
		     progname, del, strerror (errno));
	    exit (2);
	  }
	if (verbose)
	  printf ("t,%d\n", del);
      }
      break;

      /* Expect a watchdog command.  */
    case 'W':
      if (*s != 0)
	goto syntax_error;
      bytes[0] = 3;
      bytes[1] = 0;
      bytes[2] = RV_WATCHDOG_CMD;
      expect_input (fd, bytes, 3, 3);
      if (verbose)
	printf ("W\n");
      break;

      /* Send an IRQ notification.  */
    case 'I':
      sscanf (s, "%x%n", &data, &n);
      if (n < 0 || s[n] != 0)
	goto syntax_error;
      bytes[0] = 7;
      bytes[1] = 0;
      bytes[2] = RV_IRQ_CMD;
      h2le32 (bytes + 3, data);
      send_output (fd, bytes, 7);
      if (verbose)
	printf ("I,%x\n", data);
      break;

      /* DMA store (to CPU).  */
    case 's':
      {
	unsigned int addr;
	sscanf (s, "%x,%n", &addr, &n);

	if (n < 0 || s[n] == 0)
	  goto syntax_error;
	s += n;
	do
	  {
	    n = -1;
	    sscanf (s, "%02x%n", &data, &n);
	    if (n > 0)
	      {
		s += n;
		bytes[11 + nbytes++] = data;
	      }
	  }
	while (n > 0);

	if (*s != 0)
	  goto syntax_error;
	h2le32 (bytes, nbytes + 11);
	bytes[2] = RV_MEM_WR_CMD;
	h2le32 (bytes + 3, addr);
	h2le32 (bytes + 7, nbytes);
	send_output (fd, bytes, nbytes + 11);
	if (verbose)
	  {
	    printf ("s,%x,", addr);
	    for (i = 0; i < nbytes; i++)
	      printf ("%02x", bytes[i]);
	    printf ("\n");
	  }
      }
      break;

      /* DMA load (from CPU).  */
    case 'l':
      {
	unsigned int addr;
	sscanf (s, "%x,%n", &addr, &n);

	if (n < 0 || s[n] == 0)
	  goto syntax_error;
	s += n;
	do
	  {
	    n = -1;
	    sscanf (s, "%02x%n", &data, &n);
	    if (n > 0)
	      {
		s += n;
		bytes[11 + nbytes++] = data;
	      }
	  }
	while (n > 0);

	if (*s != 0)
	  goto syntax_error;
	h2le32 (bytes, nbytes + 11);
	bytes[0] = 11;
	bytes[1] = 0;
	bytes[2] = RV_MEM_RD_CMD;
	h2le32 (bytes + 3, addr);
	h2le32 (bytes + 7, nbytes);
	send_output (fd, bytes, 11);
	bytes[0] = (nbytes + 11) & 255;
	bytes[1] = ((nbytes + 11) >> 8) & 255;
	expect_input (fd, bytes, nbytes + 11, nbytes + 11);
	if (verbose)
	  {
	    printf ("l,%x,", addr);
	    for (i = 0; i < nbytes; i++)
	      printf ("%02x", bytes[i]);
	    printf ("\n");
	  }
      }
      break;

    syntax_error:
    default:
      fprintf (stderr, "%s: invalid command line in %s:%d:\n %s",
	       progname, fname, lineno, strerror (errno));
      exit (2);
    }
}

/* Loop over the contents of FNAME, using handle_input to parse each line.
   Errors to stderr, exit (2).  */

static void
handle_input_file (int fd, char *fname)
{
  static char buf[2048] = {0};
  int lineno = 0;
  FILE *f = fopen (fname, "r");

  if (f == NULL)
    {
      fprintf (stderr, "%s: problem opening %s: %s\n",
	       progname, fname, strerror (errno));
      exit (2);
    }

  /* Let's cut the buffer short, so we always get a newline.  */
  while (fgets (buf, sizeof (buf) - 1, f) != NULL)
    {
      buf[strlen (buf) - 1] = 0;
      lineno++;
      handle_input (fd, buf, fname, lineno);
    }

  fclose (f);
}

int
main (int argc, char *argv[])
{
  int optc;
  int fd;
  FILE *f;
  int i;

  progname = argv[0];
  while ((optc = getopt_long (argc, argv, "", longopts, NULL)) != -1)
    switch (optc)
      {
      case OPT_PORT:
	port = atoi (optarg);
	break;

      case OPT_TIMEOUT:
	timeout = (time_t) atoi (optarg);
	break;

      case OPT_VERBOSE:
	verbose = 1;
	break;
      }

  fd = setupsocket ();
  if (fd < 0)
    {
      fprintf (stderr, "%s: problem setting up the connection: %s\n",
	       progname, strerror (errno));
      exit (2);
    }

  for (i = optind; i < argc; i++)
    handle_input_file (fd, argv[i]);

  /* FIXME: option-controlled test for remaining input?  */
  close (fd);
  return 1;
}
#endif
