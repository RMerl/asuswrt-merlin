/* Check corner error case: specifying unimplemented resource.
#notarget: cris*-*-elf
*/
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int main (void)
{
  struct rlimit lim;
  memset (&lim, 0, sizeof lim);

  if (setrlimit (RLIMIT_NPROC, &lim) != -1
      || errno != EINVAL)
    abort ();
  printf ("pass\n");
  exit (0);
}
