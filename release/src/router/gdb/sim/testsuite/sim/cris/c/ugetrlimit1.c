/* Check corner error case: specifying unimplemented resource.
#notarget: cris*-*-elf
*/

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  struct rlimit lim;

  if (getrlimit (RLIMIT_NPROC, &lim) != -1
      || errno != EINVAL)
    abort ();
  printf ("pass\n");
  exit (0);
}
