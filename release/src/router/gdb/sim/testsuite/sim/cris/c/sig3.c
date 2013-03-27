/* Check that TRT happens at an abort (3) call, single thread.
#xerror:
#output: program stopped with signal 6.\n
*/

#include <stdlib.h>
#include <stdio.h>
int main (void)
{
  abort ();
  printf ("xyzzy\n");
  exit (0);
}
