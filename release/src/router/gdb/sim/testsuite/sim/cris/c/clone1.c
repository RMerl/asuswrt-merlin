/*
#notarget: cris*-*-elf
#output: got: a\nthen: bc\nexit: 0\n
*/

/* This is a very limited subset of what ex1.c does; we just check that
   thread creation (clone syscall) and pipe writes and reads work. */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int pip[2];

int
process (void *arg)
{
  char *s = arg;
  if (write (pip[1], s+2, 1) != 1) abort ();
  if (write (pip[1], s+1, 1) != 1) abort ();
  if (write (pip[1], s, 1) != 1) abort ();
  return 0;
}

int
main (void)
{
  int retcode;
  int pid;
  int st;
  long stack[16384];
  char buf[10] = {0};

  retcode = pipe (pip);

  if (retcode != 0)
    {
      fprintf (stderr, "Bad pipe %d\n", retcode);
      abort ();
    }

  pid = clone (process, (char *) stack + sizeof (stack) - 64,
	       (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
	       | SIGCHLD, "cba");
  if (pid <= 0)
    {
      fprintf (stderr, "Bad clone %d\n", pid);
      abort ();
    }

  if ((retcode = read (pip[0], buf, 1)) != 1)
    {
      fprintf (stderr, "Bad read 1: %d\n", retcode);
      abort ();
    }
  printf ("got: %c\n", buf[0]);
  retcode = read (pip[0], buf, 2);
  if (retcode == 1)
    {
      retcode = read (pip[0], buf+1, 1);
      if (retcode != 1)
	{
	  fprintf (stderr, "Bad read 1.5: %d\n", retcode);
	  abort ();
	}
      retcode = 2;
    }
  if (retcode != 2)
    {
      fprintf (stderr, "Bad read 2: %d\n", retcode);
      abort ();
    }

  printf ("then: %s\n", buf);
  retcode = wait4 (-1, &st, WNOHANG | __WCLONE, NULL);

  if (retcode != pid || !WIFEXITED (st))
    {
      fprintf (stderr, "Bad wait %d %x\n", retcode, st);
      abort ();
    }

  printf ("exit: %d\n", WEXITSTATUS (st));
  return 0;
}
