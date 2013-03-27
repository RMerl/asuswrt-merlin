/* Check that lstat:ing an nonexistent file works as expected.
#notarget: cris*-*-elf
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main (void)
{
  struct stat buf;
  
  if (lstat ("nonexistent", &buf) == 0 || errno != ENOENT)
    abort ();
  printf ("pass\n");
  exit (0);
}
