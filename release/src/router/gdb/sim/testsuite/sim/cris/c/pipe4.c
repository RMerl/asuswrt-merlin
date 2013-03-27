/* Check that TRT happens for pipe corner cases.
#notarget: cris*-*-elf
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
  char c;
  int pipemax;

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

  /* Writing to wrong end of pipe.  */
  if (write (pip[0], "argh", 1) != -1
      || errno != EBADF)
    err ("write pipe");

  errno = 0;

  /* Reading from wrong end of pipe.  */
  if (read (pip[1], &c, 1) != -1
      || errno != EBADF)
    err ("write pipe");

  errno = 0;

  if (close (pip[0]) != 0)
    err ("close");

  if (signal (SIGPIPE, SIG_IGN) != SIG_DFL)
    err ("signal");

  /* Writing to pipe with closed read end.  */
  if (write (pip[1], "argh", 1) != -1
      || errno != EPIPE)
    err ("write closed");

  printf ("pass\n");
  exit (0);
}
