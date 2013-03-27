/*  main.c -- top level of ARMulator:  ARM6 Instruction Emulator.
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

/**********************************************************************/
/* Forks the ARMulator and hangs on a socket passing on RDP messages  */
/* down a pipe to the ARMulator which translates them into RDI calls. */
/**********************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>

#include "armdefs.h"
#include "dbg_rdi.h"
#include "dbg_conf.h"

#define MAXHOSTNAMELENGTH 64

/* Read and write routines down sockets and pipes */

void MYread_chars (int sock, void *p, int n);
unsigned char MYread_char (int sock);
ARMword MYread_word (int sock);
void MYread_FPword (int sock, char *putinhere);

void MYwrite_word (int sock, ARMword i);
void MYwrite_string (int sock, char *s);
void MYwrite_FPword (int sock, char *fromhere);
void MYwrite_char (int sock, unsigned char c);

void passon (int source, int dest, int n);


/* Mother and child processes */
void parent (void);
void kid (void);

/* The child process id. */
pid_t child;

/* The socket to the debugger */
int debugsock;

/* The pipes between the two processes */
int mumkid[2];
int kidmum[2];

/* A pipe for handling SWI return values that goes straight from the */
/* parent to the ARMulator host interface, bypassing the childs RDP */
/* to RDI interpreter */
int DebuggerARMul[2];

/* The maximum number of file descriptors */
int nfds;

/* The socket handle */
int sockethandle;

/* The machine name */
char localhost[MAXHOSTNAMELENGTH + 1];

/* The socket number */
unsigned int socketnumber;

/**************************************************************/
/* Takes one argument: the socket number.                     */
/* Opens a socket to the debugger, and once opened spawns the */
/* ARMulator and sets up a couple of pipes.                   */
/**************************************************************/
int
main (int argc, char *argv[])
{
  int i;
  struct sockaddr_in devil, isa;
  struct hostent *hp;


  if (argc == 1)
    {
      fprintf (stderr, "No socket number\n");
      return 1;
    }

  sscanf (argv[1], "%d", &socketnumber);
  if (!socketnumber || socketnumber > 0xffff)
    {
      fprintf (stderr, "Invalid socket number: %d\n", socketnumber);
      return 1;
    }

  gethostname (localhost, MAXHOSTNAMELENGTH);
  hp = gethostbyname (localhost);
  if (!hp)
    {
      fprintf (stderr, "Cannot get local host info\n");
      return 1;
    }

  /* Open a socket */
  sockethandle = socket (hp->h_addrtype, SOCK_STREAM, 0);
  if (sockethandle < 0)
    {
      perror ("socket");
      return 1;
    }

  devil.sin_family = hp->h_addrtype;
  devil.sin_port = htons (socketnumber);
  devil.sin_addr.s_addr = 0;
  for (i = 0; i < sizeof (devil.sin_zero); i++)
    devil.sin_zero[i] = '\000';
  memcpy (&devil.sin_addr, hp->h_addr_list[0], hp->h_length);

  if (bind (sockethandle, &devil, sizeof (devil)) < 0)
    {
      perror ("bind");
      return 1;
    }

  /* May only accept one debugger at once */

  if (listen (sockethandle, 0))
    {
      perror ("listen");
      return 1;
    }

  fprintf (stderr, "Waiting for connection from debugger...");

  debugsock = accept (sockethandle, &isa, &i);
  if (debugsock < 0)
    {
      perror ("accept");
      return 1;
    }

  fprintf (stderr, " done.\nConnection Established.\n");

  nfds = getdtablesize ();

  if (pipe (mumkid))
    {
      perror ("pipe");
      return 1;
    }
  if (pipe (kidmum))
    {
      perror ("pipe");
      return 1;
    }

  if (pipe (DebuggerARMul))
    {
      perror ("pipe");
      return 1;
    }

#ifdef DEBUG
  fprintf (stderr, "Created pipes ok\n");
#endif

  child = fork ();

#ifdef DEBUG
  fprintf (stderr, "fork() ok\n");
#endif

  if (child == 0)
    kid ();
  if (child != -1)
    parent ();

  perror ("fork");
  return 1;
}
