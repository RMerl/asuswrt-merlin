/* Test case for C-c sent to threads with pending signals.  Before I
   even get there, creating a thread and sending it a signal before it
   has a chance to run leads to an internal error in GDB.  We need to
   record that there's a pending SIGSTOP, so that we'll ignore it
   later, and pass the current signal back to the thread.

   The fork/vfork case has similar trouble but that's even harder
   to get around.  We may need to send a SIGCONT to cancel out the
   SIGSTOP.  Different kernels may do different things if the thread
   is stopped by ptrace and sent a SIGSTOP.  */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

/* Loop long enough for GDB to send a few signals of its own, but
   don't hang around eating CPU forever if something goes wrong during
   testing.  */
#define NSIGS 10000000

void
handler (int sig)
{
  ;
}

pthread_t main_thread;
pthread_t child_thread, child_thread_two;

void *
child_two (void *arg)
{
  int i;

  for (i = 0; i < NSIGS; i++)
    pthread_kill (child_thread, SIGUSR1);
}

void *
thread_function (void *arg)
{
  int i;

  for (i = 0; i < NSIGS; i++)
    pthread_kill (child_thread_two, SIGUSR2);
}

int main()
{
  int i;

  signal (SIGUSR1, handler);
  signal (SIGUSR2, handler);

  main_thread = pthread_self ();
  pthread_create (&child_thread, NULL, thread_function, NULL);
  pthread_create (&child_thread_two, NULL, child_two, NULL);

  for (i = 0; i < NSIGS; i++)
    pthread_kill (child_thread_two, SIGUSR1);

  pthread_join (child_thread, NULL);

  exit (0);
}
