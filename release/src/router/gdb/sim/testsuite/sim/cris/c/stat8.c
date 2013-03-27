/* For this test, we need to do the lstat syscall directly, or else
   glibc gets a SEGV.
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main (void)
{
  int ret;

  /* From Linux, we get EFAULT.  The simulator sends us EINVAL.  */
  ret = syscall (SYS_lstat64, ".", NULL);
  if (ret != -1 || (errno != EINVAL && errno != EFAULT))
    {
      perror ("lstat");
      abort ();
    }

  printf ("pass\n");
  exit (0);
}
