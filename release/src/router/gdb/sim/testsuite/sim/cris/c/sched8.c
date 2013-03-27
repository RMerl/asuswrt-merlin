/* Check corner error case: specifying invalid PID.
#notarget: cris*-*-elf
*/
#include <string.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  struct sched_param sb;
  memset (&sb, -1, sizeof sb);
  if (sched_getparam (99, &sb) != -1
      || errno != ESRCH)
    abort ();
  printf ("pass\n");
  exit (0);
}
