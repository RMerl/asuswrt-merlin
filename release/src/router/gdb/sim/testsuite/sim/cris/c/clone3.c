/* Check that exiting from a parent thread does not kill the child.
#notarget: cris*-*-elf
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

int
process (void *arg)
{
  int i;

  for (i = 0; i < 50; i++)
    if (sched_yield ())
      abort ();

  printf ("pass\n");
  return 0;
}

int
main (void)
{
  int pid;
  long stack[16384];

  pid = clone (process, (char *) stack + sizeof (stack) - 64,
	       (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
	       | SIGCHLD, "ab");
  if (pid <= 0)
    {
      fprintf (stderr, "Bad clone %d\n", pid);
      abort ();
    }

  exit (0);
}
