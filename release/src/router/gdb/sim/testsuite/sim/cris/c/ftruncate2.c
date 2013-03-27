/*
#notarget: cris*-*-elf
*/

/* Check that we get a proper error indication if trying ftruncate on a
   fd that is a pipe descriptor.  */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
int main (void)
{
  int pip[2];

  if (pipe (pip) != 0)
  {
    perror ("pipe");
    abort ();
  }

  if (ftruncate (pip[0], 20) == 0 || errno != EINVAL)
    {
      perror ("ftruncate 1");
      abort ();
    }

  errno = 0;

  if (ftruncate (pip[1], 20) == 0 || errno != EINVAL)
    {
      perror ("ftruncate 2");
      abort ();
    }

  printf ("pass\n");

  exit (0);
}
