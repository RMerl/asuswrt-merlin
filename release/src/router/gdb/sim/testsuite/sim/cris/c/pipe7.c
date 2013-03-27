/* Check for proper pipe semantics at corner cases.
#notarget: cris*-*-elf
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main (void)
{
  if (pipe (NULL) != -1
      || errno != EFAULT)
  {
    perror ("pipe");
    abort ();
  }

  printf ("pass\n");
  exit (0);
}
