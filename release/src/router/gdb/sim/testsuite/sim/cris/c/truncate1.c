/* Check that the truncate syscall works trivially.
#notarget: cris*-*-elf
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PREFIX
#define PREFIX
#endif
int
main (void)
{
  FILE *f;
  const char fname[] = PREFIX "sk1test.dat";
  const char tsttxt1[]
    = "This is the first and only line of this file.\n";
  const char tsttxt2[] = "Now there is a second line.\n";
  char buf[sizeof (tsttxt1) + sizeof (tsttxt2) - 1] = "";

  f = fopen (fname, "w+");
  if (f == NULL
      || fwrite (tsttxt1, 1, strlen (tsttxt1), f) != strlen (tsttxt1)
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  if (truncate (fname, strlen(tsttxt1) - 10) != 0)
    {
      perror ("truncate");
      exit (1);
    }

  f = fopen (fname, "r");
  if (f == NULL
      || fread (buf, 1, sizeof (buf), f) != strlen (tsttxt1) - 10
      || strncmp (buf, tsttxt1, strlen (tsttxt1) - 10) != 0
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
