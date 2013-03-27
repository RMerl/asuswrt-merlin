/* This testcase is part of GDB, the GNU debugger.

   Copyright 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

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
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *thread_function(void *arg); /* Pointer to function executed by each thread */

#define NUM 5

unsigned int args[NUM+1];

int main() {
    int res;
    pthread_t threads[NUM];
    void *thread_result;
    long i;

    for (i = 0; i < NUM; i++)
      {
	args[i] = 1;
	res = pthread_create(&threads[i],
		             NULL,
			     thread_function,
			     (void *) i);
      }

    /* schedlock.exp: last thread start.  */
    args[i] = 1;
    thread_function ((void *) i);

    exit(EXIT_SUCCESS);
}

void *thread_function(void *arg) {
    int my_number =  (long) arg;
    int *myp = (int *) &args[my_number];

    /* Don't run forever.  Run just short of it :)  */
    while (*myp > 0)
      {
	/* schedlock.exp: main loop.  */
	(*myp) ++;
      }

    pthread_exit(NULL);
}

