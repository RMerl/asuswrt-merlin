/*  parent.c -- ARMulator RDP comms code:  ARM6 Instruction Emulator.
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
/* The Parent process continues here...                          */
/* It waits on the socket and passes on RDP messages down a pipe */
/* to the ARMulator RDP to RDI interpreter.                      */
/*****************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include "time.h"
#include "armdefs.h"
#include "dbg_rdi.h"
#include "communicate.h"

/* The socket to the debugger */
extern int debugsock;

/* The pipes between the two processes */
extern int mumkid[2];
extern int kidmum[2];

/* A pipe for handling SWI return values that goes straight from the */
/* parent to the ARMulator host interface, bypassing the child's RDP */
/* to RDI interpreter */
extern int DebuggerARMul[2];

/* The maximum number of file descriptors */
extern int nfds;

/* The child process id. */
extern pid_t child;

void
parent ()
{
  int i, j, k;
  unsigned char message, CPnum, exreturn;
  ARMword mask, nbytes, messagetype;
  unsigned char c, d;
  ARMword x, y;
  int virgin = 1;
  struct fd_set readfds;

#ifdef DEBUG
  fprintf (stderr, "parent ()...\n");
#endif

panic_error:

  if (!virgin)
    {
#ifdef DEBUG
      fprintf (stderr, "Arghh! What is going on?\n");
#endif
      kill (child, SIGHUP);
      MYwrite_char (debugsock, RDP_Reset);
    }

  virgin = 0;

  while (1)
    {

      /* Wait either for the ARMulator or the debugger */

      FD_ZERO (&readfds);
      FD_SET (kidmum[0], &readfds);	/* Wait for messages from ARMulator */
      FD_SET (debugsock, &readfds);	/* Wait for messages from debugger */

#ifdef DEBUG
      fprintf (stderr, "Waiting for ARMulator or debugger... ");
#endif

      while ((i = select (nfds, &readfds, (fd_set *) 0, (fd_set *) 0, 0)) < 0)
	{
	  perror ("select");
	}

#ifdef DEBUG
      fprintf (stderr, "(%d/2)", i);
#endif

      if (FD_ISSET (debugsock, &readfds))
	{
#ifdef DEBUG
	  fprintf (stderr, "->debugger\n");
#endif

	  /* Inside this rather large if statement with simply pass on a complete 
	     message to the ARMulator.  The reason we need to pass messages on one
	     at a time is that we have to know whether the message is an OSOpReply
	     or an info(stop), so that we can take different action in those
	     cases. */

	  if (MYread_char (debugsock, &message))
	    goto panic_error;

	  switch (message)
	    {
	    case RDP_Start:
	      /* Open and/or Initialise */
#ifdef DEBUG
	      fprintf (stderr, "RDP Open\n");
#endif
	      if (MYread_char (debugsock, &c))	/* type */
		goto panic_error;

	      if (MYread_word (debugsock, &x))	/* memory size */
		goto panic_error;

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_word (mumkid[1], x);
	      if (c & 0x2)
		{
		  passon (debugsock, mumkid[1], 1);	/* speed */
		}
	      break;

	    case RDP_End:
	      /* Close and Finalise */
#ifdef DEBUG
	      fprintf (stderr, "RDP Close\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      break;

	    case RDP_Read:
	      /* Read Memory Address */
#ifdef DEBUG
	      fprintf (stderr, "RDP Read Memory\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      if (passon (debugsock, mumkid[1], 4))
		goto panic_error;	/* address */
	      if (MYread_word (debugsock, &nbytes))
		goto panic_error;	/* nbytes */
	      MYwrite_word (mumkid[1], nbytes);
	      break;

	    case RDP_Write:
	      /* Write Memory Address */
#ifdef DEBUG
	      fprintf (stderr, "RDP Write Memory\n");
#endif
	      if (MYread_word (debugsock, &x))
		goto panic_error;	/* address */

	      if (MYread_word (debugsock, &y))
		goto panic_error;	/* nbytes */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_word (mumkid[1], x);
	      MYwrite_word (mumkid[1], y);
	      passon (debugsock, mumkid[1], y);	/* actual data */
	      break;

	    case RDP_CPUread:
	      /* Read CPU State */
#ifdef DEBUG
	      fprintf (stderr, "RDP Read CPU\n");
#endif
	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* mode */

	      if (MYread_word (debugsock, &mask))
		goto panic_error;	/* mask */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_word (mumkid[1], mask);
	      break;

	    case RDP_CPUwrite:
	      /* Write CPU State */
#ifdef DEBUG
	      fprintf (stderr, "RDP Write CPU\n");
#endif
	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* mode */

	      if (MYread_word (debugsock, &x))
		goto panic_error;	/* mask */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_word (mumkid[1], x);
	      for (k = 1, j = 0; k != 0x80000000; k *= 2, j++)
		if ((k & x) && passon (debugsock, mumkid[1], 4))
		  goto panic_error;
	      break;

	    case RDP_CPread:
	      /* Read Co-Processor State */
#ifdef DEBUG
	      fprintf (stderr, "RDP Read CP state\n");
#endif
	      if (MYread_char (debugsock, &CPnum))
		goto panic_error;

	      if (MYread_word (debugsock, &mask))
		goto panic_error;

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], CPnum);
	      MYwrite_word (mumkid[1], mask);
	      break;

	    case RDP_CPwrite:
	      /* Write Co-Processor State */
#ifdef DEBUG
	      fprintf (stderr, "RDP Write CP state\n");
#endif
	      if (MYread_char (debugsock, &CPnum))
		goto panic_error;

	      if (MYread_word (debugsock, &mask))
		goto panic_error;

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_char (mumkid[1], x);
	      for (k = 1, j = 0; k != 0x80000000; k *= 2, j++)
		if (k & x)
		  {
		    if ((c == 1 || c == 2) && k <= 128)
		      {
			/* FP register = 12 bytes + 4 bytes format */
			if (passon (debugsock, mumkid[1], 16))
			  goto panic_error;
		      }
		    else
		      {
			/* Normal register = 4 bytes */
			if (passon (debugsock, mumkid[1], 4))
			  goto panic_error;
		      }
		  }
	      break;

	    case RDP_SetBreak:
	      /* Set Breakpoint */
#ifdef DEBUG
	      fprintf (stderr, "RDP Set Breakpoint\n");
#endif
	      if (MYread_word (debugsock, &x))
		goto panic_error;	/* address */

	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* type */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_word (mumkid[1], x);
	      MYwrite_char (mumkid[1], c);
	      if (((c & 0xf) >= 5) && passon (debugsock, mumkid[1], 4))
		goto panic_error;	/* bound */
	      break;

	    case RDP_ClearBreak:
	      /* Clear Breakpoint */
#ifdef DEBUG
	      fprintf (stderr, "RDP Clear Breakpoint\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      if (passon (debugsock, mumkid[1], 4))
		goto panic_error;	/* point */
	      break;

	    case RDP_SetWatch:
	      /* Set Watchpoint */
#ifdef DEBUG
	      fprintf (stderr, "RDP Set Watchpoint\n");
#endif
	      if (MYread_word (debugsock, &x))
		goto panic_error;	/* address */

	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* type */

	      if (MYread_char (debugsock, &d))
		goto panic_error;	/* datatype */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_word (mumkid[1], x);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_char (mumkid[1], d);
	      if (((c & 0xf) >= 5) && passon (debugsock, mumkid[1], 4))
		goto panic_error;	/* bound */
	      break;

	    case RDP_ClearWatch:
	      /* Clear Watchpoint */
#ifdef DEBUG
	      fprintf (stderr, "RDP Clear Watchpoint\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      if (passon (debugsock, mumkid[1], 4))
		goto panic_error;	/* point */
	      break;

	    case RDP_Execute:
	      /* Excecute */
#ifdef DEBUG
	      fprintf (stderr, "RDP Execute\n");
#endif

	      /* LEAVE THIS ONE 'TIL LATER... */
	      /* NEED TO WORK THINGS OUT */

	      /* NO ASCYNCHROUS RUNNING */

	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* return */

	      /* Remember incase bit 7 is set and we have to send back a word */
	      exreturn = c;

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      break;

	    case RDP_Step:
	      /* Step */
#ifdef DEBUG
	      fprintf (stderr, "RDP Step\n");
#endif

	      if (MYread_char (debugsock, &c))
		goto panic_error;	/* return */

	      if (MYread_word (debugsock, &x))
		goto panic_error;	/* ninstr */

	      MYwrite_char (mumkid[1], message);
	      MYwrite_char (mumkid[1], c);
	      MYwrite_word (mumkid[1], x);
	      break;

	    case RDP_Info:
	      /* Info */
#ifdef DEBUG
	      fprintf (stderr, "RDP Info\n");
#endif
	      /* INFO TARGET, SET RDI LEVEL */
	      if (MYread_word (debugsock, &messagetype))
		goto panic_error;	/* info */

	      switch (messagetype)
		{
		case RDIInfo_Target:
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  break;

		case RDISet_RDILevel:
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  if (passon (debugsock, mumkid[1], 1))
		    goto panic_error;	/* argument */
		  break;

		case RDISet_Cmdline:
		  /* Got to pass on a string argument */
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  do
		    {
		      if (MYread_char (debugsock, &c))
			goto panic_error;

		      MYwrite_char (mumkid[1], c);
		    }
		  while (c);
		  break;

		case RDISignal_Stop:
		  kill (child, SIGUSR1);
		  MYwrite_char (debugsock, RDP_Return);
		  MYwrite_char (debugsock, RDIError_UserInterrupt);
		  break;

		case RDIVector_Catch:
		  MYread_word (debugsock, &x);
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  MYwrite_word (mumkid[1], x);
		  break;

		case RDIInfo_Step:
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  break;

		case RDIInfo_Points:
		  MYwrite_char (mumkid[1], message);
		  MYwrite_word (mumkid[1], messagetype);
		  break;

		default:
		  fprintf (stderr, "Unrecognized RDIInfo request %d\n",
			   messagetype);
		  goto panic_error;
		}
	      break;

	    case RDP_OSOpReply:
	      /* OS Operation Reply */
#ifdef DEBUG
	      fprintf (stderr, "RDP OS Reply\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      if (MYread_char (debugsock, &message))
		goto panic_error;
	      MYwrite_char (mumkid[1], message);
	      switch (message)
		{
		case 0:	/* return value i.e. nothing else. */
		  break;

		case 1:	/* returns a byte... */
		  if (MYread_char (debugsock, &c))
		    goto panic_error;

		  MYwrite_char (mumkid[1], c);
		  break;

		case 2:	/* returns a word... */
		  if (MYread_word (debugsock, &x))
		    goto panic_error;

		  MYwrite_word (mumkid[1], x);
		  break;
		}
	      break;

	    case RDP_Reset:
	      /* Reset */
#ifdef DEBUG
	      fprintf (stderr, "RDP Reset\n");
#endif
	      MYwrite_char (mumkid[1], message);
	      break;

	    default:
	      /* Hmm.. bad RDP operation */
	      fprintf (stderr, "RDP Bad RDP request (%d)\n", message);
	      MYwrite_char (debugsock, RDP_Return);
	      MYwrite_char (debugsock, RDIError_UnimplementedMessage);
	      break;
	    }
	}

      if (FD_ISSET (kidmum[0], &readfds))
	{
#ifdef DEBUG
	  fprintf (stderr, "->ARMulator\n");
#endif
	  /* Anything we get from the ARMulator has to go to the debugger... */
	  /* It is that simple! */

	  passon (kidmum[0], debugsock, 1);
	}
    }
}
