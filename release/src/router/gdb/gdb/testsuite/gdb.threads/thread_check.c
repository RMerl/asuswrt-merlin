/* Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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

   This file was written by Steve Munroe (sjmunroe@us.ibm.com). */

/* Test break points and single step on thread functions.  */

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define N       2

static void *
tf (void *arg)
{
    int n = (int) (long int) arg;
    char number[160];
    int unslept = 10;

    sprintf(number, "tf(%ld): begin", (long)arg);
    puts (number);

    while (unslept > 0)
        unslept = sleep(unslept);

    sprintf(number, "tf(%ld): end", (long)arg);
    puts (number);
    return NULL;
}

int main (int argc, char *argv[])
{
    int n;
    int unslept = 2;
    pthread_t th[N];

    for (n = 0; n < N; ++n)
    if (pthread_create (&th[n], NULL, tf, (void *) (long int) n) != 0)
    {
        while (unslept > 0)
           unslept = sleep(2);
        puts ("create failed");
        exit (1);
    }

    puts("after create");

    for (n = 0; n < N; ++n)
    if (pthread_join (th[n], NULL) != 0)
    {
        puts ("join failed");
        exit (1);
    }

    puts("after join");
    return 0;
}
