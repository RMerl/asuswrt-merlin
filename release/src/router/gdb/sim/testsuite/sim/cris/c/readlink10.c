/* Check that odd cases of readlink work.
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
  if (readlink("/proc/42/exe", NULL, 4096) != -1
      || errno != EFAULT)
    abort ();

  printf ("pass\n");
  exit (0);
}
