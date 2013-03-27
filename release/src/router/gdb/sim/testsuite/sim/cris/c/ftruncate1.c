/* Check that the ftruncate syscall works trivially.
#notarget: cris*-*-elf
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
  const char fname[] = "sk1test.dat";
  const char tsttxt1[]
    = "This is the first and only line of this file.\n";
  const char tsttxt2[] = "Now there is a second line.\n";
  char buf[sizeof (tsttxt1) + sizeof (tsttxt2) - 1] = "";

  f = fopen (fname, "w+");
  if (f == NULL
      || fwrite (tsttxt1, 1, strlen (tsttxt1), f) != strlen (tsttxt1))
    perr ("open or fwrite");

  if (fflush (f) != 0)
    perr ("fflush");

  if (ftruncate (fileno (f), strlen(tsttxt1) - 20) != 0)
    perr ("ftruncate");

  if (fclose (f) != 0)
    perr ("fclose");

  f = fopen (fname, "r");
  if (f == NULL
      || fread (buf, 1, sizeof (buf), f) != strlen (tsttxt1) - 20
      || strncmp (buf, tsttxt1, strlen (tsttxt1) - 20) != 0
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
