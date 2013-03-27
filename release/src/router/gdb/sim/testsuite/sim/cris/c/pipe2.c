/* Check that closing a pipe with a nonempty buffer works.
#notarget: cris*-*-elf
#output: got: a\ngot: b\nexit: 0\n
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
#include <string.h>
int pip[2];

int pipemax;

int
process (void *arg)
{
  char *s = arg;
  int lots = pipemax + 256;
  char *buf = malloc (lots);
  int ret;

  if (buf == NULL)
    abort ();

  *buf = *s;

  /* The first write should go straight through.  */
  if (write (pip[1], buf, 1) != 1)
    abort ();

  *buf = s[1];

  /* The second write may or may not be successful for the whole
     write, but should be successful for at least the pipemax part.
     As linux/limits.h clamps PIPE_BUF to 4096, but the page size is
     actually 8k, we can get away with that much.  There should be no
     error, though.  Doing this on host shows that for
     x86_64-unknown-linux-gnu (2.6.14-1.1656_FC4) pipemax * 10 can be
     successfully written, perhaps for similar reasons.  */
  ret = write (pip[1], buf, lots);
  if (ret < pipemax)
    {
      fprintf (stderr, "ret: %d, %s, %d\n", ret, strerror (errno), pipemax);
      fflush (0);
      abort ();
    }

  return 0;
}

int
main (void)
{
  int retcode;
  int pid;
  int st = 0;
  long stack[16384];
  char buf[1];

  /* We need to turn this off because we don't want (to have to model) a
     SIGPIPE resulting from the close.  */
  if (signal (SIGPIPE, SIG_IGN) != SIG_DFL)
    abort ();

  retcode = pipe (pip);

  if (retcode != 0)
    {
      fprintf (stderr, "Bad pipe %d\n", retcode);
      abort ();
    }

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

  pid = clone (process, (char *) stack + sizeof (stack) - 64,
	       (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
	       | SIGCHLD, "ab");
  if (pid <= 0)
    {
      fprintf (stderr, "Bad clone %d\n", pid);
      abort ();
    }

  while ((retcode = read (pip[0], buf, 1)) == 0)
    ;

  if (retcode != 1)
    {
      fprintf (stderr, "Bad read 1: %d\n", retcode);
      abort ();
    }

  printf ("got: %c\n", buf[0]);

  /* Need to read out something from the second write too before
     closing, or the writer can get EPIPE. */
  while ((retcode = read (pip[0], buf, 1)) == 0)
    ;

  if (retcode != 1)
    {
      fprintf (stderr, "Bad read 2: %d\n", retcode);
      abort ();
    }

  printf ("got: %c\n", buf[0]);

  if (close (pip[0]) != 0)
    {
      perror ("pip close");
      abort ();
    }

  retcode = waitpid (pid, &st, __WALL);

  if (retcode != pid || !WIFEXITED (st))
    {
      fprintf (stderr, "Bad wait %d:%d %x\n", pid, retcode, st);
      perror ("errno");
      abort ();
    }

  printf ("exit: %d\n", WEXITSTATUS (st));
  return 0;
}
