/* Check that basic freopen functionality works.
#xfail: *-*-*
   Currently doesn't work, because syscall.c:cb_syscall case
   CB_SYS_write intercepts writes to fd 1 and 2.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{
  FILE *old_stderr;
  FILE *f;
  const char fname[] = "sk1test.dat";
  const char tsttxt[]
    = "A random line of text, used to test correct freopen etc.\n";
  char buf[sizeof tsttxt] = "";

  /* Like the freopen call in flex.  */
  old_stderr = freopen (fname, "w+", stderr);
  if (old_stderr == NULL
      || fwrite (tsttxt, 1, strlen (tsttxt), stderr) != strlen (tsttxt)
      || fclose (stderr) != 0)
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
