/*
#notarget: cris*-*-elf
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  struct stat buf;

  /* From Linux, we get EFAULT.  The simulator sends us EINVAL.  */
  if (lstat (NULL, &buf) != -1
      || (errno != EINVAL && errno != EFAULT))
    {
      perror ("lstat 1");
      abort ();
    }

  printf ("pass\n");
  exit (0);
}
