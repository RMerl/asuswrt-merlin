#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void
leave (int n)
{
  exit (0);
}

int
main (void)
{
  /* Check that the sigaction syscall (for signal) is interpreted, though
     possibly ignored.  */
  signal (SIGFPE, leave);

  printf ("pass\n");
  exit (0);
}
