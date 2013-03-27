/*
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char *argv[])
{
  char buf[1024];
  char buf2[1024];
  int err;

  /* This is a special feature handled in the simulator.  The "42"
     should be formed from getpid () if this was a real program.  */
  err = readlink ("/proc/42/exe", buf, sizeof (buf));
  if (err < 0)
    {
      if (err == -1 && errno == ENOSYS)
	printf ("ENOSYS\n");
      printf ("xyzzy\n");
      exit (0);
    }      

  /* Don't use an abort in the following; it might cause the printf to
     not make it all the way to output and make debugging more
     difficult.  */

  /* We assume the program is called with no path, so we might need to
     prepend it.  */
  if (getcwd (buf2, sizeof (buf2)) != buf2)
    {
      perror ("getcwd");
      exit (1);
    }

  if (argv[0][0] == '/')
    {
#ifdef SYSROOTED
      if (strchr (argv[0] + 1, '/') != NULL)
	{
	  printf ("%s != %s\n", argv[0], strrchr (argv[0] + 1, '/'));
	  exit (1);
	}
#endif
      if (strcmp (argv[0], buf) != 0)
	{
	  printf ("%s != %s\n", buf, argv[0]);
	  exit (1);
	}
    }
  else if (argv[0][0] != '.')
    {
      if (buf2[strlen (buf2) - 1] != '/')
	strcat (buf2, "/");
      strcat (buf2, argv[0]);
      if (strcmp (buf2, buf) != 0)
	{
	  printf ("%s != %s\n", buf, buf2);
	  exit (1);
	}
    }
  else
    {
      strcat (buf2, argv[0] + 1);
      if (strcmp (buf, buf2) != 0)
	{
	  printf ("%s != %s\n", buf, buf2);
	  exit (1);
	}
    }

  printf ("pass\n");
  exit (0);
}


