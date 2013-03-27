/* Check that TRT happens when we reach the #threads implementation limit.
#notarget: cris*-*-elf
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

int
process (void *arg)
{
  int i;

  for (i = 0; i < 500; i++)
    if (sched_yield ())
      abort ();

  return 0;
}

int
main (void)
{
  int pid;
  int i;
  int stacksize = 16384;

  for (i = 0; i < 1000; i++)
    {
      char *stack = malloc (stacksize);
      if (stack == NULL)
	abort ();

      pid = clone (process, (char *) stack + stacksize - 64,
		   (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
		   | SIGCHLD, "ab");
      if (pid <= 0)
	{
	  /* FIXME: Read sysconf instead of magic number.  */
	  if (i < 60)
	    {
	      fprintf (stderr, "Bad clone %d\n", pid);
	      abort ();
	    }

	  if (errno == EAGAIN)
	    {
	      printf ("pass\n");
	      exit (0);
	    }
	}
    }

  abort ();
}
