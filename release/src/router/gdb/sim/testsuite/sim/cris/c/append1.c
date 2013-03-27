/* Check regression of a bug uncovered by the libio tFile test (old
   libstdc++, pre-gcc-3.x era), where appending to a file doesn't work.
   The default open-flags-mapping does not match Linux/CRIS, so a
   specific mapping is necessary.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
      || fwrite (tsttxt1, 1, strlen (tsttxt1), f) != strlen (tsttxt1)
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  f = fopen (fname, "a+");
  if (f == NULL
      || fwrite (tsttxt2, 1, strlen (tsttxt2), f) != strlen (tsttxt2)
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  f = fopen (fname, "r");
  if (f == NULL
      || fread (buf, 1, sizeof (buf), f) != sizeof (buf) - 1
      || strncmp (buf, tsttxt1, strlen (tsttxt1)) != 0
      || strncmp (buf + strlen (tsttxt1), tsttxt2, strlen (tsttxt2)) != 0
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
