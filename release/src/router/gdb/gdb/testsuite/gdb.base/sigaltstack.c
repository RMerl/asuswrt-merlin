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

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

enum level { MAIN, OUTER, INNER, LEAF, NR_LEVELS };

/* Levels completed flag.  */
volatile enum level level = NR_LEVELS;

void catcher (int signal);

void
thrower (enum level next_level, int sig, int itimer, int on_stack)
{
  level = next_level;
  /* Set up the signal handler.  */
  {
    struct sigaction act;
    memset (&act, 0, sizeof (act));
    act.sa_handler = catcher;
    act.sa_flags |= on_stack;
    sigaction (sig, &act, NULL);
  }
  /* Set up a one-off timer.  A timer, rather than SIGSEGV, is used as
     after a timer handler finishes the interrupted code can safely
     resume.  */
  {
    struct itimerval itime;
    memset (&itime, 0, sizeof (itime));
    itime.it_value.tv_usec = 250 * 1000;
    setitimer (itimer, &itime, NULL);
  }
  /* Wait.  */
  while (level != LEAF);
}

void
catcher (int signal)
{
  /* Find the next level.  */
  switch (level)
    {
    case MAIN:
      thrower (OUTER, SIGALRM, ITIMER_REAL, SA_ONSTACK);
      break;
    case OUTER:
      thrower (INNER, SIGVTALRM, ITIMER_VIRTUAL, SA_ONSTACK);
      break;
    case INNER:
      level = LEAF;
      return;
    }
}


main ()
{
  /* Set up the altstack.  */
  {
    static char stack[SIGSTKSZ * NR_LEVELS];
    stack_t alt;
    memset (&alt, 0, sizeof (alt));
    alt.ss_sp = stack;
    alt.ss_size = SIGSTKSZ;
    alt.ss_flags = 0;
    if (sigaltstack (&alt, NULL) < 0)
      {
	perror ("sigaltstack");
	exit (0);
      }
  }
  level = MAIN;
  catcher (0);
}
