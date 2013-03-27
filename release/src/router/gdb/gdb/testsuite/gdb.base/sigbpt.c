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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <signal.h>
#include <stdlib.h>
#include <string.h>

extern void
keeper (int sig)
{
}

volatile long v1 = 0;
volatile long v2 = 0;
volatile long v3 = 0;

extern long
bowler (void)
{
  /* Try to read address zero.  Do it in a slightly convoluted way so
     that more than one instruction is used.  */
  return *(char *) (v1 + v2 + v3);
}

int
main ()
{
  static volatile int i;

  struct sigaction act;
  memset (&act, 0, sizeof act);
  act.sa_handler = keeper;
  sigaction (SIGSEGV, &act, NULL);

  bowler ();
  return 0;
}
