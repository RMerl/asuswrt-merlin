/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004, 2007 Free Software Foundation, Inc.

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
#include <string.h>
#include <signal.h>
#include <sys/time.h>

static volatile int done;

#ifdef SA_SIGINFO
static void /* HANDLER */
handler (int sig, siginfo_t *info, void *context)
{
  done = 1;
} /* handler */
#else
static void
handler (int sig)
{
  done = 1;
} /* handler */
#endif

main ()
{
  /* Set up the signal handler.  */
  {
    struct sigaction action;
    memset (&action, 0, sizeof (action));
#ifdef SA_SIGINFO
    action.sa_sigaction = handler;
    action.sa_flags |= SA_SIGINFO;
#else
    action.sa_handler = handler;
#endif
    sigaction (SIGVTALRM, &action, NULL);
  }

  /* Set up a one-off timer.  A timer, rather than SIGSEGV, is used as
     after a timer handler finishes the interrupted code can safely
     resume.  */
  {
    struct itimerval itime;
    memset (&itime, 0, sizeof (itime));
    itime.it_value.tv_usec = 250 * 1000;
    setitimer (ITIMER_VIRTUAL, &itime, NULL);
  }
  /* Wait.  */
  while (!done);
  return 0;
} /* main */
