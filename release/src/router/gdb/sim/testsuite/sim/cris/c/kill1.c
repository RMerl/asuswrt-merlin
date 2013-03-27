/* Basic kill functionality test; fail killing init.  Don't run as root.  */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
int
main (void)
{
  if (kill (1, SIGTERM) != -1
      || errno != EPERM)
    {
      printf ("fail\n");
      exit (1);
    }

  errno = 0;

  if (kill (1, SIGABRT) != -1
      || errno != EPERM)
    {
      printf ("fail\n");
      exit (1);
    }

  errno = 0;

  printf ("pass\n");
  exit (0);
}
