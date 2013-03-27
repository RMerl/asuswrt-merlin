/* Check that TRT happens for spurious sigreturn calls.  Multiple threads.
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#xerror:
#output: Invalid sigreturn syscall: no signal handler active (0x1, 0x2, 0x3, 0x4, 0x5, 0x6)\n
#output: program stopped with signal 4.\n
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <signal.h>
#include <errno.h>

static void *
process (void *arg)
{
  while (1)
    sched_yield ();
  return NULL;
}

int main (void)
{
  pthread_t th_a;
  if (pthread_create (&th_a, NULL, process, (void *) "a") == 0)
    {
      int err = syscall (SYS_sigreturn, 1, 2, 3, 4, 5, 6);
      if (err == -1 && errno == ENOSYS)
	printf ("ENOSYS\n");
    }
  printf ("xyzzy\n");
  exit (0);
}
