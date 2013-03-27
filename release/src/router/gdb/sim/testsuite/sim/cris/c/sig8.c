/* Check that TRT happens for an ignored catchable signal, single thread.
#xerror:
#output: Unimplemented signal: 14\n
#output: program stopped with signal 4.\n

   Sure, it'd probably be better to support signals in single-thread too,
   but that's on an as-need basis, and I don't have a need for it yet.  */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
int main (void)
{
  signal (SIGALRM, SIG_IGN);
  kill (getpid (), SIGALRM);
  printf ("xyzzy\n");
  exit (0);
}
