/* Compiler options:
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#output: abb ok\n

   Testing a pthread corner case.  Output will change with glibc
   releases.  */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

static void *
process (void *arg)
{
  int i;

  if (pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL) != 0)
    abort ();
  write (2, "a", 1);
  for (i = 0; i < 10; i++)
    {
      sched_yield ();
      pthread_testcancel ();
      write (2, "b", 1);
    }
  return NULL;
}

int
main (void)
{
  int retcode;
  pthread_t th_a;
  void *retval;

  retcode = pthread_create (&th_a, NULL, process, NULL);
  sched_yield ();
  sched_yield ();
  sched_yield ();
  sched_yield ();
  retcode = pthread_cancel (th_a);
  retcode = pthread_join (th_a, &retval);
  if (retcode != 0)
    abort ();
  fprintf (stderr, " ok\n");
  return 0;
}
