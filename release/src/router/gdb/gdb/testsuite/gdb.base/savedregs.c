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

extern int
callee (int param)
{
  return param * done + 1;
}

extern int
caller (int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
  return callee (a1 << a2 * a3 / a4 + a6 & a6 % a7 - a8) + done;
}

static void
catcher (int sig)
{
  done = 1;
} /* handler */

static void
thrower (void)
{
  *(char *)0 = 0;
}

main ()
{
  signal (SIGSEGV, catcher);
  thrower ();
}
