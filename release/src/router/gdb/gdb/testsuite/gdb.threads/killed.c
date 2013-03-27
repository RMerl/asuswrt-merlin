/* This testcase is part of GDB, the GNU debugger.

   Copyright 2002, 2004, 2007 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int pid;

void *
child_func (void *dummy)
{
  kill (pid, SIGKILL);
  exit (1);
}

int
main ()
{
  pthread_t child;

  pid = getpid ();
  pthread_create (&child, 0, child_func, 0);
  for (;;)
    sleep (10000);
}
