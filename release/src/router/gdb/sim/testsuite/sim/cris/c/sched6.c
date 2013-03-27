/*
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

int main (void)
{
  if (sched_yield () != 0)
    abort ();
  printf ("pass\n");
  exit (0);
}
