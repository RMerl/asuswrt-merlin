/* Basic kill functionality test; suicide.
#xerror:
#output: program stopped with signal 6.\n
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
int
main (void)
{
  kill (getpid (), SIGABRT);
  printf ("undead\n");
  exit (1);
}
