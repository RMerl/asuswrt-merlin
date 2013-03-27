/*
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (void)
{
  struct sched_param sb;
  memset (&sb, -1, sizeof sb);
  if (sched_getparam (getpid (), &sb) != 0
      || sb.sched_priority != 0)
    abort ();
  printf ("pass\n");
  exit (0);
}
