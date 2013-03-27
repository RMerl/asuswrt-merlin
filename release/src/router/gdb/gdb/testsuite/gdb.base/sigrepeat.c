/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004, 2005, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

static volatile int done[2];
static volatile int repeats[2];
static int itimer[2] = { ITIMER_REAL, ITIMER_VIRTUAL };
static int alarm[2] = { SIGALRM, SIGVTALRM };

static void
handler (int sig)
{
  int sigi;
  switch (sig)
    {
    case SIGALRM: sigi = 0; break;
    case SIGVTALRM: sigi = 1; break;
    default: abort ();
    }
  if (repeats[sigi]++ > 3)
    {
      /* Hit with enough signals, cancel everything and get out.  */
      {
	struct itimerval itime;
	memset (&itime, 0, sizeof (itime));
	setitimer (itimer[sigi], &itime, NULL);
      }
      {
	struct sigaction action;
	memset (&action, 0, sizeof (action));
	action.sa_handler = SIG_IGN;
	sigaction (sig, &action, NULL);
      }
      done[sigi] = 1;
      return;
    }
  /* Set up a nested virtual timer.  */
  while (1)
    {
      /* Wait until a signal has become pending, that way when this
	 handler returns it will be immediatly delivered leading to
	 back-to-back signals.  */
      sigset_t set;
      sigemptyset (&set);
      if (sigpending (&set) < 0)
	{
	  perror ("sigrepeat");
	  abort ();
	}
      if (sigismember (&set, sig))
	break;
    }
} /* handler */

int
main ()
{
  int i;
  /* Set up the signal handler.  */
  for (i = 0; i < 2; i++)
    {
      struct sigaction action;
      memset (&action, 0, sizeof (action));
      action.sa_handler = handler;
      sigaction (alarm[i], &action, NULL);
    }

  /* Set up a rapidly repeating timers.  A timer, rather than SIGSEGV,
     is used as after a timer handler returns the interrupted code can
     safely resume.  The intent is for the program to swamp GDB with a
     backlog of pending signals.  */
  for (i = 0; i < 2; i++)
    {
      struct itimerval itime;
      memset (&itime, 0, sizeof (itime));
      itime.it_interval.tv_usec = 1;
      itime.it_value.tv_usec = 250 * 1000;
      setitimer (itimer[i], &itime, NULL);
    }

  /* Wait.  */
  while (!done[0] && !done[1]); /* infinite loop */
  return 0;
}
