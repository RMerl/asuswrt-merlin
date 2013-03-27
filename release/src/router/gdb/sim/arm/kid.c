/*  kid.c -- ARMulator RDP/RDI interface:  ARM6 Instruction Emulator.
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

/*****************************************************************/
/* The child process continues here...                           */
/* It waits on a pipe from the parent and translates the RDP     */
/* messages into RDI calls to the ARMulator passing RDP replies  */
/* back up a pipe to the parent.                                 */
/*****************************************************************/

#include <sys/types.h>
#include <signal.h>

#include "armdefs.h"
#include "dbg_conf.h"
#include "dbg_hif.h"
#include "dbg_rdi.h"
#include "gdbhost.h"
#include "communicate.h"

/* The pipes between the two processes */
extern int mumkid[2];
extern int kidmum[2];

/* The maximum number of file descriptors */
extern int nfds;

/* The machine name */
#define MAXHOSTNAMELENGTH 64
extern char localhost[MAXHOSTNAMELENGTH + 1];

/* The socket number */
extern unsigned int socketnumber;

/* RDI interface */
extern const struct RDIProcVec armul_rdi;

static int MYrdp_level = 0;

static int rdi_state = 0;

/**************************************************************/
/* Signal handler that terminates excecution in the ARMulator */
/**************************************************************/
void
kid_handlesignal (int sig)
{
#ifdef DEBUG
  fprintf (stderr, "Terminate ARMulator excecution\n");
#endif
  if (sig != SIGUSR1)
    {
      fprintf (stderr, "Unsupported signal.\n");
      return;
    }
  armul_rdi.info (RDISignal_Stop, (unsigned long *) 0, (unsigned long *) 0);
}

/********************************************************************/
/* Waits on a pipe from the socket demon for RDP and                */
/* acts as an RDP to RDI interpreter on the front of the ARMulator. */
/********************************************************************/
void
kid ()
{
  char *p, *q;
  int i, j, k;
  long outofthebag;
  unsigned char c, d, message;
  ARMword x, y, z;
  struct sigaction action;
  PointHandle point;
  Dbg_ConfigBlock config;
  Dbg_HostosInterface hostif;
  struct Dbg_MCState *MCState;
  char command_line[256];
  struct fd_set readfds;

  /* Setup a signal handler for SIGUSR1 */
  action.sa_handler = kid_handlesignal;
  action.sa_mask = 0;
  action.sa_flags = 0;

  sigaction (SIGUSR1, &action, (struct sigaction *) 0);

  while (1)
    {
      /* Wait for ever */
      FD_ZERO (&readfds);
      FD_SET (mumkid[0], &readfds);

      i = select (nfds, &readfds,
		  (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0);

      if (i < 0)
	{
	  perror ("select");
	}

      if (read (mumkid[0], &message, 1) < 1)
	{
	  perror ("read");
	}

      switch (message)
	{
	case RDP_Start:
	  /* Open and/or Initialise */
	  BAG_newbag ();

	  MYread_char (mumkid[0], &c);	/* type */
	  MYread_word (mumkid[0], &x);	/* memorysize */
	  if (c & 0x2)
	    MYread_char (mumkid[0], &d);	/* speed */
	  config.processor = 0;
	  config.memorysize = x;
	  config.bytesex = (c & 0x4) ? RDISex_Big : RDISex_Little;
	  if (c & 0x8)
	    config.bytesex = RDISex_DontCare;

	  hostif.dbgprint = myprint;
	  hostif.dbgpause = mypause;
	  hostif.dbgarg = stdout;
	  hostif.writec = mywritec;
	  hostif.readc = myreadc;
	  hostif.write = mywrite;
	  hostif.gets = mygets;
	  hostif.reset = mypause;	/* do nothing */
	  hostif.resetarg = "Do I love resetting or what!\n";

	  if (rdi_state)
	    {
	      /* we have restarted, so kill off the existing run.  */
	      /* armul_rdi.close(); */
	    }
	  i = armul_rdi.open (c & 0x3, &config, &hostif, MCState);
	  rdi_state = 1;

	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);

	  x = ~0x4;
	  armul_rdi.info (RDIVector_Catch, &x, 0);

	  break;

	case RDP_End:
	  /* Close and Finalise */
	  i = armul_rdi.close ();
	  rdi_state = 0;
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_Read:
	  /* Read Memory Address */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_word (mumkid[0], &y);	/* nbytes */
	  p = (char *) malloc (y);
	  i = armul_rdi.read (x, p, (unsigned *) &y);
	  MYwrite_char (kidmum[1], RDP_Return);
	  for (k = 0; k < y; k++)
	    MYwrite_char (kidmum[1], p[k]);
	  free (p);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  if (i)
	    MYwrite_word (kidmum[1], y);	/* number of bytes sent without error */
	  break;

	case RDP_Write:
	  /* Write Memory Address */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_word (mumkid[0], &y);	/* nbytes */
	  p = (char *) malloc (y);
	  for (k = 0; k < y; k++)
	    MYread_char (mumkid[0], &p[k]);
	  i = armul_rdi.write (p, x, (unsigned *) &y);
	  free (p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  if (i)
	    MYwrite_word (kidmum[1], y);	/* number of bytes sent without error */
	  break;

	case RDP_CPUread:
	  /* Read CPU State */
	  MYread_char (mumkid[0], &c);	/* mode */
	  MYread_word (mumkid[0], &x);	/* mask */
	  p = (char *) malloc (4 * RDINumCPURegs);
	  i = armul_rdi.CPUread (c, x, (ARMword *) p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  for (k = 1, j = 0; k != 0x80000000; k *= 2)
	    if (k & x)
	      MYwrite_word (kidmum[1], ((ARMword *) p)[j++]);
	  free (p);
	  if (i)
	    MYwrite_char (kidmum[1], (unsigned char) j);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_CPUwrite:
	  /* Write CPU State */
	  MYread_char (mumkid[0], &c);	/* mode */
	  MYread_word (mumkid[0], &x);	/* mask */

	  p = (char *) malloc (4 * RDINumCPURegs);
	  for (k = 1, j = 0; k != 0x80000000; k *= 2)
	    if (k & x)
	      MYread_word (mumkid[0], &(((ARMword *) p)[j++]));
	  i = armul_rdi.CPUwrite (c, x, (ARMword *) p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  free (p);
	  break;

	case RDP_CPread:
	  /* Read Co-Processor State */
	  MYread_char (mumkid[0], &c);	/* CPnum */
	  MYread_word (mumkid[0], &x);	/* mask */
	  p = q = (char *) malloc (16 * RDINumCPRegs);
	  i = armul_rdi.CPread (c, x, (ARMword *) p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  for (k = 1, j = 0; k != 0x80000000; k *= 2, j++)
	    if (k & x)
	      {
		if ((c == 1 || c == 2) && k <= 128)
		  {
		    MYwrite_FPword (kidmum[1], q);
		    q += 16;
		  }
		else
		  {
		    MYwrite_word (kidmum[1], *q);
		    q += 4;
		  }
	      }
	  free (p);
	  if (i)
	    MYwrite_char (kidmum[1], (unsigned char) j);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_CPwrite:
	  /* Write Co-Processor State */
	  MYread_char (mumkid[0], &c);	/* CPnum */
	  MYread_word (mumkid[0], &x);	/* mask */
	  p = q = (char *) malloc (16 * RDINumCPURegs);
	  for (k = 1, j = 0; k != 0x80000000; k *= 2, j++)
	    if (k & x)
	      {
		if ((c == 1 || c == 2) && k <= 128)
		  {
		    MYread_FPword (kidmum[1], q);
		    q += 16;
		  }
		else
		  {
		    MYread_word (mumkid[0], (ARMword *) q);
		    q += 4;
		  }
	      }
	  i = armul_rdi.CPwrite (c, x, (ARMword *) p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  free (p);
	  break;

	case RDP_SetBreak:
	  /* Set Breakpoint */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_char (mumkid[0], &c);	/* type */
	  if ((c & 0xf) >= 5)
	    MYread_word (mumkid[0], &y);	/* bound */
	  i = armul_rdi.setbreak (x, c, y, &point);
	  if (!MYrdp_level)
	    BAG_putpair ((long) x, (long) point);
	  MYwrite_char (kidmum[1], RDP_Return);
	  if (MYrdp_level)
	    MYwrite_word (kidmum[1], point);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_ClearBreak:
	  /* Clear Breakpoint */
	  MYread_word (mumkid[0], &point);	/* PointHandle */
	  if (!MYrdp_level)
	    {
	      BAG_getsecond ((long) point, &outofthebag);	/* swap pointhandle for address */
	      BAG_killpair_byfirst (outofthebag);
	      point = outofthebag;
	    }
	  i = armul_rdi.clearbreak (point);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_SetWatch:
	  /* Set Watchpoint */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_char (mumkid[0], &c);	/* type */
	  MYread_char (mumkid[0], &d);	/* datatype */
	  if ((c & 0xf) >= 5)
	    MYread_word (mumkid[0], &y);	/* bound */
	  i = armul_rdi.setwatch (x, c, d, y, &point);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_word (kidmum[1], point);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_ClearWatch:
	  /* Clear Watchpoint */
	  MYread_word (mumkid[0], &point);	/* PointHandle */
	  i = armul_rdi.clearwatch (point);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_Execute:
	  /* Excecute */

	  MYread_char (mumkid[0], &c);	/* return */

#ifdef DEBUG
	  fprintf (stderr, "Starting execution\n");
#endif
	  i = armul_rdi.execute (&point);
#ifdef DEBUG
	  fprintf (stderr, "Completed execution\n");
#endif
	  MYwrite_char (kidmum[1], RDP_Return);
	  if (c & 0x80)
	    MYwrite_word (kidmum[1], point);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_Step:
	  /* Step */
	  MYread_char (mumkid[0], &c);	/* return */
	  MYread_word (mumkid[0], &x);	/* ninstr */
	  point = 0x87654321;
	  i = armul_rdi.step (x, &point);
	  MYwrite_char (kidmum[1], RDP_Return);
	  if (c & 0x80)
	    MYwrite_word (kidmum[1], point);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  break;

	case RDP_Info:
	  /* Info */
	  MYread_word (mumkid[0], &x);
	  switch (x)
	    {
	    case RDIInfo_Target:
	      i = armul_rdi.info (RDIInfo_Target, &y, &z);
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_word (kidmum[1], y);	/* Loads of info... */
	      MYwrite_word (kidmum[1], z);	/* Model */
	      MYwrite_char (kidmum[1], (unsigned char) i);
	      break;

	    case RDISet_RDILevel:
	      MYread_word (mumkid[0], &x);	/* arg1, debug level */
	      i = armul_rdi.info (RDISet_RDILevel, &x, 0);
	      if (i == RDIError_NoError)
		MYrdp_level = x;
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_char (kidmum[1], (unsigned char) i);
	      break;

	    case RDISet_Cmdline:
	      for (p = command_line; MYread_char (mumkid[0], p), *p; p++)
		;		/* String */
	      i = armul_rdi.info (RDISet_Cmdline,
				  (unsigned long *) command_line, 0);
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_char (kidmum[1], (unsigned char) i);
	      break;

	    case RDIInfo_Step:
	      i = armul_rdi.info (RDIInfo_Step, &x, 0);
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_word (kidmum[1], x);
	      MYwrite_char (kidmum[1], (unsigned char) i);
	      break;

	    case RDIVector_Catch:
	      MYread_word (mumkid[0], &x);
	      i = armul_rdi.info (RDIVector_Catch, &x, 0);
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_char (kidmum[1], i);
	      break;

	    case RDIInfo_Points:
	      i = armul_rdi.info (RDIInfo_Points, &x, 0);
	      MYwrite_char (kidmum[1], RDP_Return);
	      MYwrite_word (kidmum[1], x);
	      MYwrite_char (kidmum[1], (unsigned char) i);
	      break;

	    default:
	      fprintf (stderr, "Unsupported info code %d\n", x);
	      break;
	    }
	  break;

	case RDP_OSOpReply:
	  /* OS Operation Reply */
	  MYwrite_char (kidmum[1], RDP_Fatal);
	  break;

	case RDP_Reset:
	  /* Reset */
	  for (i = 0; i < 50; i++)
	    MYwrite_char (kidmum[1], RDP_Reset);
	  p = (char *) malloc (MAXHOSTNAMELENGTH + 5 + 20);
	  sprintf (p, "Running on %s:%d\n", localhost, socketnumber);
	  MYwrite_string (kidmum[1], p);
	  free (p);

	  break;
	default:
	  fprintf (stderr, "Oh dear: Something is seriously wrong :-(\n");
	  /* Hmm.. bad RDP operation */
	  break;
	}
    }
}


/* Handles memory read operations until an OS Operation Reply Message is */
/* encounterd. It then returns the byte info value (0, 1, or 2) and fills  */
/* in 'putinr0' with the data if appropriate. */
int
wait_for_osreply (ARMword * reply)
{
  char *p, *q;
  int i, j, k;
  unsigned char c, d, message;
  ARMword x, y, z;
  struct sigaction action;
  PointHandle point;
  Dbg_ConfigBlock config;
  Dbg_HostosInterface hostif;
  struct Dbg_MCState *MCState;
  char command_line[256];
  struct fd_set readfds;

#ifdef DEBUG
  fprintf (stderr, "wait_for_osreply ().\n");
#endif

  /* Setup a signal handler for SIGUSR1 */
  action.sa_handler = kid_handlesignal;
  action.sa_mask = 0;
  action.sa_flags = 0;

  sigaction (SIGUSR1, &action, (struct sigaction *) 0);

  while (1)
    {
      /* Wait for ever */
      FD_ZERO (&readfds);
      FD_SET (mumkid[0], &readfds);

      i = select (nfds, &readfds,
		  (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0);

      if (i < 0)
	{
	  perror ("select");
	}

      if (read (mumkid[0], &message, 1) < 1)
	{
	  perror ("read");
	}

      switch (message)
	{
	case RDP_Read:
	  /* Read Memory Address */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_word (mumkid[0], &y);	/* nbytes */
	  p = (char *) malloc (y);
	  i = armul_rdi.read (x, p, (unsigned *) &y);
	  MYwrite_char (kidmum[1], RDP_Return);
	  for (k = 0; k < y; k++)
	    MYwrite_char (kidmum[1], p[k]);
	  free (p);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  if (i)
	    MYwrite_word (kidmum[1], y);	/* number of bytes sent without error */
	  break;

	case RDP_Write:
	  /* Write Memory Address */
	  MYread_word (mumkid[0], &x);	/* address */
	  MYread_word (mumkid[0], &y);	/* nbytes */
	  p = (char *) malloc (y);
	  for (k = 0; k < y; k++)
	    MYread_char (mumkid[0], &p[k]);
	  i = armul_rdi.write (p, x, (unsigned *) &y);
	  free (p);
	  MYwrite_char (kidmum[1], RDP_Return);
	  MYwrite_char (kidmum[1], (unsigned char) i);
	  if (i)
	    MYwrite_word (kidmum[1], y);	/* number of bytes sent without error */
	  break;

	case RDP_OSOpReply:
	  /* OS Operation Reply */
	  MYread_char (mumkid[0], &c);
	  if (c == 1)
	    MYread_char (mumkid[0], (char *) reply);
	  if (c == 2)
	    MYread_word (mumkid[0], reply);
	  return c;
	  break;

	default:
	  fprintf (stderr,
		   "HELP! Unaccounted-for message during OS request. \n");
	  MYwrite_char (kidmum[1], RDP_Fatal);
	}
    }
}
