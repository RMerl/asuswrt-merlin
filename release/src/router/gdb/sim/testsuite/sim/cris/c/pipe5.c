/* Check that TRT happens for pipe corner cases (for our definition of TRT).
#notarget: cris*-*-elf
#xerror:
#output: Terminating simulation due to writing pipe * from one single thread\n
#output: program stopped with signal 4.\n
*/
#include <stddef.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

void err (const char *s)
{
  perror (s);
  abort ();
}

int main (void)
{
  int pip[2];
  int pipemax;
  char *buf;

  if (pipe (pip) != 0)
    err ("pipe");

#ifdef PIPE_MAX
  pipemax = PIPE_MAX;
#else
  pipemax = fpathconf (pip[1], _PC_PIPE_BUF);
#endif

  if (pipemax <= 0)
    {
      fprintf (stderr, "Bad pipemax %d\n", pipemax);
      abort ();
    }

  /* Writing an inordinate amount to the pipe.  */
  buf = calloc (100 * pipemax, 1);
  if (buf == NULL)
    err ("calloc");

  /* The following doesn't trig on host; writing more than PIPE_MAX to a
     pipe with no reader makes the program hang.  Neither does it trig
     on target: we don't want to emulate the "hanging" (which would
     happen with *any* amount written to a pipe with no reader if we'd
     support it - but we don't).  Better to abort the simulation with a
     suitable message.  */
  if (write (pip[1], buf, 100 * pipemax) != -1
      || errno != EFBIG)
    err ("write mucho");

  printf ("pass\n");
  exit (0);
}
