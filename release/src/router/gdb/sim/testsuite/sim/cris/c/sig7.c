/* Check unsupported case of sigaction syscall.
#notarget: cris*-*-elf
#xerror:
#output: Unimplemented rt_sigaction syscall (0x8, 0x3df*\n
#output: program stopped with signal 4.\n
*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

int
main (void)
{
  struct sigaction sa;
  int err;
  sa.sa_sigaction = NULL;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset (&sa.sa_mask);

  err = sigaction (SIGFPE, &sa, NULL);
  if (err == -1 && errno == ENOSYS)
    printf ("ENOSYS\n");

  printf ("xyzzy\n");
  exit (0);
}
