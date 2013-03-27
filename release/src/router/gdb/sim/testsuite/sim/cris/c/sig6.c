/* Check that TRT happens at an non-abort non-caught signal, more than one thread.
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#xerror:
#output: Exiting pid 42 due to signal 7\n
#output: program stopped with signal 4.\n
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
  while (1)
    sched_yield ();
  return NULL;
}

int main (void)
{
  pthread_t th_a;
  if (pthread_create (&th_a, NULL, process, (void *) "a") == 0)
    kill (getpid (), SIGBUS);
  printf ("xyzzy\n");
  exit (0);
}
