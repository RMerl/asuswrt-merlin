/*
#notarget: cris*-*-elf
*/

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

/* Like sig1.c, but using sigaction.  */

void
leave (int n, siginfo_t *info, void *x)
{
  abort ();
}

int
main (void)
{
  struct sigaction sa;
  sa.sa_sigaction = leave;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset (&sa.sa_mask);

  /* Check that the sigaction syscall (for signal) is interpreted, though
     possibly ignored.  */
  if (sigaction (SIGFPE, &sa, NULL) != 0)
    abort ();

  printf ("pass\n");
  exit (0);
}
