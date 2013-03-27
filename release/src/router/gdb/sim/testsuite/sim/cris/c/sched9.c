/* Check corner error case: specifying invalid scheduling policy.
#notarget: cris*-*-elf
*/

#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  if (sched_get_priority_min (-1) != -1
      || errno != EINVAL)
    abort ();

  errno = 0;

  if (sched_get_priority_max (-1) != -1
      || errno != EINVAL)
    abort ();

  printf ("pass\n");
  exit (0);
}
