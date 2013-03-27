/* Compiler options:
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#xerror:
#output: Unimplemented rt_sigprocmask syscall (0x3, 0x0, 0x3dff*\n
#output: program stopped with signal 4.\n

   Testing a signal handler corner case.  */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

static void *
process (void *arg)
{
  while (1)
    sched_yield ();
  return NULL;
}

int
main (void)
{
  int retcode;
  pthread_t th_a;
  void *retval;
  sigset_t sigs;

  if (sigemptyset (&sigs) != 0)
    abort ();

  retcode = pthread_create (&th_a, NULL, process, NULL);
  if (retcode != 0)
    abort ();

  /* An invalid parameter 1 should cause this to halt the simulator.  */
  retcode
    = pthread_sigmask (SIG_BLOCK + SIG_UNBLOCK + SIG_SETMASK, NULL, &sigs);
  /* Direct return of the error number; i.e. not using -1 and errno,
     is the actual documented behavior.  */
  if (retcode == ENOSYS)
    printf ("ENOSYS\n");

  printf ("xyzzy\n");
  return 0;
}
