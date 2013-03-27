/*
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
int main (void)
{
  int Min = sched_get_priority_min (SCHED_OTHER);
  int Max = sched_get_priority_max (SCHED_OTHER);
  if (Min != 0 || Max != 0)
    {
      fprintf (stderr, "min: %d, max: %d\n", Min, Max);
      abort ();
    }
  printf ("pass\n");
  exit (0);
}
