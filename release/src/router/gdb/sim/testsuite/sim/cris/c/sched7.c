/* Check corner error case: specifying invalid PID.
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  if (sched_getscheduler (99) != -1
      || errno != ESRCH)
    abort ();
  printf ("pass\n");
  exit (0);
}
