/* A small multi-threaded test case.

   Copyright 2004, 2007 Free Software Foundation, Inc.

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
#include <stdio.h>
#include <time.h>

void
cond_wait (pthread_cond_t *cond, pthread_mutex_t *mut)
{
  pthread_mutex_lock(mut);
  pthread_cond_wait (cond, mut);
  pthread_mutex_unlock (mut);
}

void
noreturn (void)
{
  pthread_mutex_t mut;
  pthread_cond_t cond;

  pthread_mutex_init (&mut, NULL);
  pthread_cond_init (&cond, NULL);

  /* Wait for a condition that will never be signaled, so we effectively
     block the thread here.  */
  cond_wait (&cond, &mut);
}

void *
forever_pthread (void *unused)
{
  noreturn ();
}

void
break_me (void)
{
  /* Just an anchor to help putting a breakpoint.  */
}

int
main (void)
{
  pthread_t forever;
  const struct timespec ts = { 0, 10000000 }; /* 0.01 sec */

  pthread_create (&forever, NULL, forever_pthread, NULL);
  for (;;)
    {
      nanosleep (&ts, NULL);
      break_me();
    }

  return 0;
}

