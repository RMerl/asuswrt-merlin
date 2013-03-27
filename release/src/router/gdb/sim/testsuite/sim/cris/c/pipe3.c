/* Check that TRT happens when error on pipe call.
#notarget: cris*-*-elf
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
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

  /* Check that TRT happens when error on pipe call.  */
  for (i = 0; i < filemax + 1; i++)
    {
      int pip[2];
      if (pipe (pip) != 0)
	{
	  /* Shouldn't happen too early.  */
	  if (i < filemax / 2 - 3 - 1)
	    {
	      fprintf (stderr, "i: %d\n", i);
	      abort ();
	    }
	  if (errno != EMFILE)
	    {
	      perror ("pipe");
	      abort ();
	    }
	  goto ok;
	}
    }
  abort ();

ok:
  printf ("pass\n");
  exit (0);
}
