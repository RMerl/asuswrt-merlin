/* Compiler options:
#cc: additional_flags=-pthread
#notarget: cris*-*-elf

   A sanity check for syscalls resulting from
   pthread_getschedparam and pthread_setschedparam.  */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int main (void)
{
  struct sched_param param;
  int policy;

  if (pthread_getschedparam (pthread_self (), &policy, &param) != 0
      || policy != SCHED_OTHER
      || param.sched_priority != 0)
    abort ();

  if (pthread_setschedparam (pthread_self (), SCHED_OTHER, &param) != 0
      || param.sched_priority != 0)
    abort ();

  printf ("pass\n");
  exit (0);
}
