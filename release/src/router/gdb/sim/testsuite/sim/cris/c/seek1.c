/* Check that basic (ll|f)seek sim functionality works.  Also uses basic
   file open/write functionality.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{
  FILE *f;
  const char fname[] = "sk1test.dat";
  const char tsttxt[]
    = "A random line of text, used to test correct read, write and seek.\n";
  char buf[sizeof tsttxt] = "";

  f = fopen (fname, "w");
  if (f == NULL
      || fwrite (tsttxt, 1, strlen (tsttxt), f) != strlen (tsttxt)
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  /* Using "rb" to make this test similar to the use in genconf.c in
     GhostScript.  */
  f = fopen (fname, "rb");
  if (f == NULL
      || fseek (f, 0L, SEEK_END) != 0
      || ftell (f) != strlen (tsttxt))
    {
      printf ("fail\n");
      exit (1);
    }

  rewind (f);
  if (fread (buf, 1, strlen (tsttxt), f) != strlen (tsttxt)
      || strcmp (buf, tsttxt) != 0
      || fclose (f) != 0)
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
