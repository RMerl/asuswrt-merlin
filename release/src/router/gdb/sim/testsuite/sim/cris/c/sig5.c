/* Check that TRT happens for an uncaught non-abort signal, single thread.
#xerror:
#output: Unimplemented signal: 7\n
#output: program stopped with signal 4.\n
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
int main (void)
{
  kill (getpid (), SIGBUS);
  printf ("xyzzy\n");
  exit (0);
}
