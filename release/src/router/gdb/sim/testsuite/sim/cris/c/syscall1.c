/* Test unknown-syscall output.
#notarget: cris*-*-elf
#xerror:
#output: Unimplemented syscall: 166 (0x1, 0x2, 0x3, 0x4, 0x5, 0x6)\n
#output: program stopped with signal 4.\n
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main (void)
{
  /* The number 166 is chosen because there's a gap for that number in
     the CRIS asm/unistd.h.  */
  int err = syscall (166, 1, 2, 3, 4, 5, 6);
  if (err == -1 && errno == ENOSYS)
    printf ("ENOSYS\n");
  printf ("xyzzy\n");
  exit (0);
}
