/* Check that the syscalls implementing fdopen work trivially.
#output: This is the first line of this test.\npass\n
*/

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
    = "This is the first line of this test.\n";
  char buf[sizeof (tsttxt1)] = "";

  /* Write a line to stdout.  */
  f = fdopen (1, "w");
  if (f == NULL
      || fwrite (tsttxt1, 1, strlen (tsttxt1), f) != strlen (tsttxt1))
    perr ("fdopen or fwrite");

#if 0
  /* Unfortunately we can't get < /dev/null to the simulator with
     reasonable test-framework surgery.  */

  /* Try to read from stdin.  Expect EOF.  */
  f = fdopen (0, "r");
  if (f == NULL
      || fread (buf, 1, sizeof (buf), f) != 0
      || feof (f) == 0
      || ferror (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }
#endif

  printf ("pass\n");
  exit (0);
}
