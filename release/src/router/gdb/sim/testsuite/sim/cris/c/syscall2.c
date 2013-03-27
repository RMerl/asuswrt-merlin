/* Test unknown-syscall output.
#notarget: cris*-*-elf
#xerror:
#output: Unimplemented syscall: 0 (0x3, 0x2, 0x1, 0x4, 0x6, 0x5)\n
#output: program stopped with signal 4.\n
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main (void)
{
  int err;

  /* Check special case of number 0 syscall.  */
  err = syscall (0, 3, 2, 1, 4, 6, 5);
  if (err == -1 && errno == ENOSYS)
    printf ("ENOSYS\n");
  printf ("xyzzy\n");
  exit (0);
}
