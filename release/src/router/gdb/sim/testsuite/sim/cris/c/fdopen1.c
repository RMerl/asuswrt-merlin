/* Check that the syscalls implementing fdopen work trivially.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

void
perr (const char *s)
{
  perror (s);
  exit (1);
}

int
main (void)
{
  FILE *f;
  int fd;
  const char fname[] = "sk1test.dat";
  const char tsttxt1[]
    = "This is the first and only line of this file.\n";
  char buf[sizeof (tsttxt1)] = "";

  fd = open (fname, O_WRONLY|O_TRUNC|O_CREAT, S_IRWXU);
  if (fd <= 0)
    perr ("open-w");

  f = fdopen (fd, "w");
  if (f == NULL
      || fwrite (tsttxt1, 1, strlen (tsttxt1), f) != strlen (tsttxt1))
    perr ("fdopen or fwrite");

  if (fclose (f) != 0)
    perr ("fclose");

  fd = open (fname, O_RDONLY);
  if (fd <= 0)
    perr ("open-r");

  f = fdopen (fd, "r");
  if (f == NULL
      || fread (buf, 1, sizeof (buf), f) != strlen (tsttxt1)
      || strcmp (buf, tsttxt1) != 0
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
