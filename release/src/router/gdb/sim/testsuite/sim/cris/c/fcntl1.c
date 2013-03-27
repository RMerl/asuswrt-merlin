/* Check that we get the expected message for unsupported fcntl calls.
#notarget: cris*-*-elf
#xerror:
#output: Unimplemented fcntl*
#output: program stopped with signal 4.\n
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main (void)
{
  int err = fcntl (1, 42);
  if (err == -1 && errno == ENOSYS)
    printf ("ENOSYS\n");
  printf ("xyzzy\n");
  exit (0);
}
