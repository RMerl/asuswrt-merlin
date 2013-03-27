/* Compiler options:
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#output: abbb ok\n

   Testing a signal handler corner case.  */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

static void *
process (void *arg)
{
  write (2, "a", 1);
  write (2, "b", 1);
  write (2, "b", 1);
  write (2, "b", 1);
  return NULL;
}

int ok = 0;
volatile int done = 0;

void
sigusr1 (int signum)
{
  if (signum != SIGUSR1 || !ok)
    abort ();
  done = 1;
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

  if (signal (SIGUSR1, sigusr1) != SIG_DFL)
    abort ();
  if (pthread_sigmask (SIG_BLOCK, NULL, &sigs) != 0
      || sigaddset (&sigs, SIGUSR1) != 0
      || pthread_sigmask (SIG_BLOCK, &sigs, NULL) != 0)
    abort ();
  if (pthread_kill (pthread_self (), SIGUSR1) != 0
      || sched_yield () != 0
      || sched_yield () != 0
      || sched_yield () != 0)
    abort ();

  ok = 1;
  if (pthread_sigmask (SIG_UNBLOCK, NULL, &sigs) != 0
      || sigaddset (&sigs, SIGUSR1) != 0
      || pthread_sigmask (SIG_UNBLOCK, &sigs, NULL) != 0)
    abort ();

  if (!done)
    abort ();

  retcode = pthread_join (th_a, &retval);
  if (retcode != 0)
    abort ();
  fprintf (stderr, " ok\n");
  return 0;
}
