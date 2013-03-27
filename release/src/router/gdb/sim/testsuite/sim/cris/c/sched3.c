/*
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  struct sched_param sb;
  sb.sched_priority = 0;
  if (sched_setscheduler (getpid (), SCHED_OTHER, &sb) != 0
      || sb.sched_priority != 0)
    abort ();
  sb.sched_priority = 5;
  if (sched_setscheduler (getpid (), SCHED_OTHER, &sb) != -1
      || errno != EINVAL
      || sb.sched_priority != 5)
    abort ();
  printf ("pass\n");
  exit (0);
}
