/* This test program is part of GDB, The GNU debugger.

   Copyright 2004, 2007 Free Software Foundation, Inc.

   Originally written by Jeff Johnston <jjohnstn@redhat.com>,
   contributed by Red Hat

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

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

sem_t semaphore;

void *
thread_function (void *arg)
{
  printf ("Thread executing\n");
  while (sem_wait (&semaphore) != 0)
    {
      if (errno != EINTR)
	{
	  perror ("thread_function");
	  return;
	}
    }
  return NULL;
}

int 
main (int argc, char **argv)
{
  pthread_attr_t attr;

  pthread_attr_init (&attr);
  pthread_attr_setstacksize (&attr, PTHREAD_STACK_MIN);

  if (sem_init (&semaphore, 0, 0) == -1)
    {
      perror ("semaphore");
      return -1;
    }


  /* Create a thread, wait for it to complete.  */
  {
    pthread_t thread;
    pthread_create (&thread, &attr, thread_function, NULL);
    sem_post (&semaphore);
    pthread_join (thread, NULL);
  }

  pthread_attr_destroy (&attr);
  return 0;
}
