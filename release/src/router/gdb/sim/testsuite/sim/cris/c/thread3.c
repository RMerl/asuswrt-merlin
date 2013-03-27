/* Compiler options:
#cc: additional_flags=-pthread
#notarget: cris*-*-elf

   To test sched_yield in the presencs of threads.  Core from ex1.c.  */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

static void *
process (void *arg)
{
  int i;
  for (i = 0; i < 10; i++)
    {
      if (sched_yield () != 0)
	abort ();
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
    abort ();
  retcode = pthread_create (&th_b, NULL, process, (void *) "b");
  if (retcode != 0)
    abort ();
  retcode = pthread_join (th_a, &retval);
  if (retcode != 0)
    abort ();
  retcode = pthread_join (th_b, &retval);
  if (retcode != 0)
    abort ();
  printf ("pass\n");
  return 0;
}
