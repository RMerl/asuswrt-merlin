/* Check unimplemented-output for mprotect call.
#notarget: cris*-*-elf
#xerror:
#output: Unimplemented mprotect call (0x0, 0x2001, 0x4)\n
#output: program stopped with signal 4.\n
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>

int main (int argc, char *argv[])
{
  int err = mprotect (0, 8193, PROT_EXEC);
  if (err == -1 && errno == ENOSYS)
    printf ("ENOSYS\n");
  printf ("xyzzy\n");
  exit (0);
}
