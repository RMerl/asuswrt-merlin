/* New version of run front end support for simulators.
   Copyright (C) 1997, 2004, 2007 Free Software Foundation, Inc.

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

#include <signal.h>
#include "sim-main.h"

#include "bfd.h"

#ifdef HAVE_ENVIRON
extern char **environ;
#endif

#ifdef HAVE_UNISTD_H
/* For chdir.  */
#include <unistd.h>
#endif

static void usage (void);

extern host_callback default_callback;

static char *myname;

static SIM_DESC sd;

static RETSIGTYPE
cntrl_c (int sig)
{
  if (! sim_stop (sd))
    {
      fprintf (stderr, "Quit!\n");
      exit (1);
    }
}

int
main (int argc, char **argv)
{
  char *name;
  char **prog_argv = NULL;
  struct bfd *prog_bfd;
  enum sim_stop reason;
  int sigrc = 0;
  int single_step = 0;
  RETSIGTYPE (*prev_sigint) ();

  myname = argv[0] + strlen (argv[0]);
  while (myname > argv[0] && myname[-1] != '/')
    --myname;

  /* INTERNAL: When MYNAME is `step', single step the simulator
     instead of allowing it to run free.  The sole purpose of this
     HACK is to allow the sim_resume interface's step argument to be
     tested without having to build/run gdb. */
  if (strlen (myname) > 4 && strcmp (myname - 4, "step") == 0)
    {
      single_step = 1;
    }

  /* Create an instance of the simulator.  */
  default_callback.init (&default_callback);
  sd = sim_open (SIM_OPEN_STANDALONE, &default_callback, NULL, argv);
  if (sd == 0)
    exit (1);
  if (STATE_MAGIC (sd) != SIM_MAGIC_NUMBER)
    {
      fprintf (stderr, "Internal error - bad magic number in simulator struct\n");
      abort ();
    }

  /* We can't set the endianness in the callback structure until
     sim_config is called, which happens in sim_open.  */
  default_callback.target_endian
    = (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN
       ? BFD_ENDIAN_BIG : BFD_ENDIAN_LITTLE);

  /* Was there a program to run?  */
  prog_argv = STATE_PROG_ARGV (sd);
  prog_bfd = STATE_PROG_BFD (sd);
  if (prog_argv == NULL || *prog_argv == NULL)
    usage ();

  name = *prog_argv;

  /* For simulators that don't open prog during sim_open() */
  if (prog_bfd == NULL)
    {
      prog_bfd = bfd_openr (name, 0);
      if (prog_bfd == NULL)
	{
	  fprintf (stderr, "%s: can't open \"%s\": %s\n", 
		   myname, name, bfd_errmsg (bfd_get_error ()));
	  exit (1);
	}
      if (!bfd_check_format (prog_bfd, bfd_object)) 
	{
	  fprintf (stderr, "%s: \"%s\" is not an object file: %s\n",
		   myname, name, bfd_errmsg (bfd_get_error ()));
	  exit (1);
	}
    }

  if (STATE_VERBOSE_P (sd))
    printf ("%s %s\n", myname, name);

  /* Load the program into the simulator.  */
  if (sim_load (sd, name, prog_bfd, 0) == SIM_RC_FAIL)
    exit (1);

  /* Prepare the program for execution.  */
#ifdef HAVE_ENVIRON
  sim_create_inferior (sd, prog_bfd, prog_argv, environ);
#else
  sim_create_inferior (sd, prog_bfd, prog_argv, NULL);
#endif

  /* To accommodate relative file paths, chdir to sysroot now.  We
     mustn't do this until BFD has opened the program, else we wouldn't
     find the executable if it has a relative file path.  */
  if (simulator_sysroot[0] != '\0' && chdir (simulator_sysroot) < 0)
    {
      fprintf (stderr, "%s: can't change directory to \"%s\"\n",
	       myname, simulator_sysroot);
      exit (1);
    }

  /* Run/Step the program.  */
  if (single_step)
    {
      do
	{
	  prev_sigint = signal (SIGINT, cntrl_c);
	  sim_resume (sd, 1/*step*/, 0);
	  signal (SIGINT, prev_sigint);
	  sim_stop_reason (sd, &reason, &sigrc);

	  if ((reason == sim_stopped) &&
	      (sigrc == sim_signal_to_host (sd, SIM_SIGINT)))
	    break; /* exit on control-C */
	}
      /* remain on breakpoint or signals in oe mode*/
      while (((reason == sim_signalled) &&
	      (sigrc == sim_signal_to_host (sd, SIM_SIGTRAP))) ||
	     ((reason == sim_stopped) && 
	      (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)));
    }
  else 
    {
      do
	{
#if defined (HAVE_SIGACTION) && defined (SA_RESTART)
	  struct sigaction sa, osa;
	  sa.sa_handler = cntrl_c;
	  sigemptyset (&sa.sa_mask);
	  sa.sa_flags = 0;
	  sigaction (SIGINT, &sa, &osa);
	  prev_sigint = osa.sa_handler;
#else
	  prev_sigint = signal (SIGINT, cntrl_c);
#endif
	  sim_resume (sd, 0, sigrc);
	  signal (SIGINT, prev_sigint);
	  sim_stop_reason (sd, &reason, &sigrc);
	  
	  if ((reason == sim_stopped) &&
	      (sigrc == sim_signal_to_host (sd, SIM_SIGINT)))
	    break; /* exit on control-C */
	  
	  /* remain on signals in oe mode */
	} while ((reason == sim_stopped) &&
		 (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT));
      
    }
  /* Print any stats the simulator collected.  */
  if (STATE_VERBOSE_P (sd))
    sim_info (sd, 0);
  
  /* Shutdown the simulator.  */
  sim_close (sd, 0);
  
  /* If reason is sim_exited, then sigrc holds the exit code which we want
     to return.  If reason is sim_stopped or sim_signalled, then sigrc holds
     the signal that the simulator received; we want to return that to
     indicate failure.  */
  
  /* Why did we stop? */
  switch (reason)
    {
    case sim_signalled:
    case sim_stopped:
      if (sigrc != 0)
        fprintf (stderr, "program stopped with signal %d.\n", sigrc);
      break;

    case sim_exited:
      break;

    default:
      fprintf (stderr, "program in undefined state (%d:%d)\n", reason, sigrc);
      break;

    }

  return sigrc;
}

static void
usage ()
{
  fprintf (stderr, "Usage: %s [options] program [program args]\n", myname);
  fprintf (stderr, "Run `%s --help' for full list of options.\n", myname);
  exit (1);
}
