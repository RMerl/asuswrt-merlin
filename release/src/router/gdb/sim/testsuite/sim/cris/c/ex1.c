/* Compiler options:
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#output: Starting process a\naaaaaaaaStarting process b\nababbbbbbbbb

   The output will change depending on the exact syscall sequence per
   thread, so will change with glibc versions.  Prepare to modify; use
   the latest glibc.

   This file is from glibc/linuxthreads, with the difference that the
   number is 10, not 10000.  */

/* Creates two threads, one printing 10000 "a"s, the other printing
   10000 "b"s.
   Illustrates: thread creation, thread joining. */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "pthread.h"

static void *
process (void *arg)
{
  int i;
  fprintf (stderr, "Starting process %s\n", (char *) arg);
  for (i = 0; i < 10; i++)
    {
      write (1, (char *) arg, 1);
    }
  return NULL;
}

int
main (void)
{
  int retcode;
  pthread_t th_a, th_b;
  void *retval;

  retcode = pthread_create (&th_a, NULL, process, (void *) "a");
  if (retcode != 0)
    fprintf (stderr, "create a failed %d\n", retcode);
  retcode = pthread_create (&th_b, NULL, process, (void *) "b");
  if (retcode != 0)
    fprintf (stderr, "create b failed %d\n", retcode);
  retcode = pthread_join (th_a, &retval);
  if (retcode != 0)
    fprintf (stderr, "join a failed %d\n", retcode);
  retcode = pthread_join (th_b, &retval);
  if (retcode != 0)
    fprintf (stderr, "join b failed %d\n", retcode);
  return 0;
}
