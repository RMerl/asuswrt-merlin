/* Check for proper pipe semantics at corner cases.
#notarget: cris*-*-elf
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

int main (void)
{
  int i;
  int filemax;

#ifdef OPEN_MAX
  filemax = OPEN_MAX;
#else
  filemax = sysconf (_SC_OPEN_MAX);
#endif

  if (filemax < 10)
    abort ();

  /* Check that pipes don't leak file descriptors.  */
  for (i = 0; i < filemax * 10; i++)
    {
      int pip[2];
      if (pipe (pip) != 0)
	{
	  perror ("pipe");
	  abort ();
	}

      if (close (pip[0]) != 0 || close (pip[1]) != 0)
	{
	  perror ("close");
	  abort ();
	}
    }
  printf ("pass\n");
  exit (0);
}
