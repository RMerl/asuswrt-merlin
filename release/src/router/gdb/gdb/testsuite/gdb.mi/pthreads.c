/* Pthreads test program.
   Copyright 1996, 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

   Written by Keith Seitz of Red Hat.
   Copied from gdb.threads/pthreads.c.
   Contributed by Red Hat.

   This file is part of GDB.

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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Under OSF 2.0 & 3.0 and HPUX 10, the second arg of pthread_create
   is prototyped to be just a "pthread_attr_t", while under Solaris it
   is a "pthread_attr_t *".  Arg! */

#if defined (__osf__) || defined (__hpux__)
#define PTHREAD_CREATE_ARG2(arg) arg
#define PTHREAD_CREATE_NULL_ARG2 null_attr
static pthread_attr_t null_attr;
#else
#define PTHREAD_CREATE_ARG2(arg) &arg
#define PTHREAD_CREATE_NULL_ARG2 NULL
#endif

void *
routine (void *arg)
{
  /* When gdb is running, it sets hidden breakpoints in the thread
     library.  The signals caused by these hidden breakpoints can
     cause system calls such as 'sleep' to return early.  Pay attention
     to the return value from 'sleep' to get the full sleep.  */
  int unslept = 9;
  while (unslept > 0)
    unslept = sleep (unslept);

  printf ("hello thread\n");
}

/* Marker function for the testsuite */
void
done_making_threads (void)
{
  /* Nothing */
}

void
create_thread (void)
{
  pthread_t tid;

  if (pthread_create (&tid, PTHREAD_CREATE_NULL_ARG2, routine, (void *) 0xfeedface))
    {
      perror ("pthread_create 1");
      exit (1);
    }
}

int
main (int argc, char *argv[])
{
  int i;

  /* Create a few threads */
  for (i = 0; i < 5; i++)
    create_thread ();
  done_making_threads ();

  printf ("hello\n");
  printf ("hello\n");
  return 0;
}

