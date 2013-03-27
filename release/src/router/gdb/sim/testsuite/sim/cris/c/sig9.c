/* Check that TRT happens at an non-abort ignored signal, more than one thread.
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

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
  signal (SIGALRM, SIG_IGN);
  if (pthread_create (&th_a, NULL, process, (void *) "a") == 0)
    kill (getpid (), SIGALRM);
  retcode = pthread_join (th_a, &retval);
  if (retcode != 0 || retval != NULL)
    abort ();
  printf ("pass\n");
  exit (0);
}
