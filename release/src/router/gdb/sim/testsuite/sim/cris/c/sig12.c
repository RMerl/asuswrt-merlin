/* Check that TRT happens for a signal sent to a non-existent process/thread, more than one thread.
#cc: additional_flags=-pthread
#notarget: cris*-*-elf
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

static void *
process (void *arg)
{
  int i;
  for (i = 0; i < 100; i++)
    sched_yield ();
  return NULL;
}

int main (void)
{
  pthread_t th_a;
  int retcode;
  void *retval;

  if (pthread_create (&th_a, NULL, process, (void *) "a") != 0)
    abort ();
  if (kill (getpid () - 1, SIGBUS) != -1
      || errno != ESRCH
      || pthread_join (th_a, &retval) != 0)
    abort ();
  printf ("pass\n");
  exit (0);
}
