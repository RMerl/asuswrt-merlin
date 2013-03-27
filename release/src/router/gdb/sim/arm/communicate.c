/*  communicate.c -- ARMulator RDP comms code:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

/**************************************************************************/
/* Functions to read and write characters or groups of characters         */
/* down sockets or pipes.  Those that return a value return -1 on failure */
/* and 0 on success.                                                      */
/**************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "armdefs.h"

/* The socket to the debugger */
int debugsock;

/* The maximum number of file descriptors */
extern int nfds;

/* The socket handle */
extern int sockethandle;

/* Read and Write routines down a pipe or socket */

/****************************************************************/
/* Read an individual character.                                */
/* All other read functions rely on this one.                   */
/* It waits 15 seconds until there is a character available: if */
/* no character is available, then it timeouts and returns -1.  */
/****************************************************************/
int
MYread_char (int sock, unsigned char *c)
{
  int i;
  fd_set readfds;
  struct timeval timeout = { 15, 0 };
  struct sockaddr_in isa;

retry:

  FD_ZERO (&readfds);
  FD_SET (sock, &readfds);

  i = select (nfds, &readfds, (fd_set *) 0, (fd_set *) 0, &timeout);

  if (i < 0)
    {
      perror ("select");
      exit (1);
    }

  if (!i)
    {
      fprintf (stderr, "read: Timeout\n");
      return -1;
    }

  if ((i = read (sock, c, 1)) < 1)
    {
      if (!i && sock == debugsock)
	{
	  fprintf (stderr, "Connection with debugger severed.\n");
	  /* This shouldn't be necessary for a detached armulator, but
	     the armulator cannot be cold started a second time, so
	     this is probably preferable to locking up.  */
	  return -1;
	  fprintf (stderr, "Waiting for connection from debugger...");
	  debugsock = accept (sockethandle, &isa, &i);
	  if (debugsock < 0)
	    {			/* Now we are in serious trouble... */
	      perror ("accept");
	      return -1;
	    }
	  fprintf (stderr, " done.\nConnection Established.\n");
	  sock = debugsock;
	  goto retry;
	}
      perror ("read");
      return -1;
    }

#ifdef DEBUG
  if (sock == debugsock)
    fprintf (stderr, "<%02x ", *c);
#endif

  return 0;
}

/****************************************************************/
/* Read an individual character.                                */
/* It waits until there is a character available. Returns -1 if */
/* an error occurs.                                             */
/****************************************************************/
int
MYread_charwait (int sock, unsigned char *c)
{
  int i;
  fd_set readfds;
  struct sockaddr_in isa;

retry:

  FD_ZERO (&readfds);
  FD_SET (sock, &readfds);

  i = select (nfds, &readfds,
	      (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0);

  if (i < 0)
    {
      perror ("select");
      exit (-1);
    }

  if ((i = read (sock, c, 1)) < 1)
    {
      if (!i && sock == debugsock)
	{
	  fprintf (stderr, "Connection with debugger severed.\n");
	  return -1;
	  fprintf (stderr, "Waiting for connection from debugger...");
	  debugsock = accept (sockethandle, &isa, &i);
	  if (debugsock < 0)
	    {			/* Now we are in serious trouble... */
	      perror ("accept");
	      return -1;
	    }
	  fprintf (stderr, " done.\nConnection Established.\n");
	  sock = debugsock;
	  goto retry;
	}
      perror ("read");
      return -1;
    }

#ifdef DEBUG
  if (sock == debugsock)
    fprintf (stderr, "<%02x ", *c);
#endif

  return 0;
}

void
MYwrite_char (int sock, unsigned char c)
{

  if (write (sock, &c, 1) < 1)
    perror ("write");
#ifdef DEBUG
  if (sock == debugsock)
    fprintf (stderr, ">%02x ", c);
#endif
}

int
MYread_word (int sock, ARMword * here)
{
  unsigned char a, b, c, d;

  if (MYread_char (sock, &a) < 0)
    return -1;
  if (MYread_char (sock, &b) < 0)
    return -1;
  if (MYread_char (sock, &c) < 0)
    return -1;
  if (MYread_char (sock, &d) < 0)
    return -1;
  *here = a | b << 8 | c << 16 | d << 24;
  return 0;
}

void
MYwrite_word (int sock, ARMword i)
{
  MYwrite_char (sock, i & 0xff);
  MYwrite_char (sock, (i & 0xff00) >> 8);
  MYwrite_char (sock, (i & 0xff0000) >> 16);
  MYwrite_char (sock, (i & 0xff000000) >> 24);
}

void
MYwrite_string (int sock, char *s)
{
  int i;
  for (i = 0; MYwrite_char (sock, s[i]), s[i]; i++);
}

int
MYread_FPword (int sock, char *putinhere)
{
  int i;
  for (i = 0; i < 16; i++)
    if (MYread_char (sock, &putinhere[i]) < 0)
      return -1;
  return 0;
}

void
MYwrite_FPword (int sock, char *fromhere)
{
  int i;
  for (i = 0; i < 16; i++)
    MYwrite_char (sock, fromhere[i]);
}

/* Takes n bytes from source and those n bytes */
/* down to dest */
int
passon (int source, int dest, int n)
{
  char *p;
  int i;

  p = (char *) malloc (n);
  if (!p)
    {
      perror ("Out of memory\n");
      exit (1);
    }
  if (n)
    {
      for (i = 0; i < n; i++)
	if (MYread_char (source, &p[i]) < 0)
	  return -1;

#ifdef DEBUG
      if (dest == debugsock)
	for (i = 0; i < n; i++)
	  fprintf (stderr, ")%02x ", (unsigned char) p[i]);
#endif

      write (dest, p, n);
    }
  free (p);
  return 0;
}
