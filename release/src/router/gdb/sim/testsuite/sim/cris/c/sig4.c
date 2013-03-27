/* Check that TRT happens at an abort (3) call, more than one thread.
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#xerror:
#output: Exiting pid 42 due to signal 6\n
#output: program stopped with signal 6.\n
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

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
    abort ();
  printf ("xyzzy\n");
  exit (0);
}
