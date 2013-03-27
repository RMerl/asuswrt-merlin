/* Check for corner case: readlink of too-long name.
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

void bye (const char *s, int i)
{
  fprintf (stderr, "%s: %d\n", s, i);
  fflush (NULL);
  abort ();
}

int main (int argc, char *argv[])
{
  char *buf;
  char buf2[1024];
  int max, i;

  /* We assume this limit is what we see in the simulator as well.  */
#ifdef PATH_MAX
  max = PATH_MAX;
#else
  max = pathconf (argv[0], _PC_PATH_MAX);
#endif

  max *= 10;

  if (max <= 0)
    bye ("path_max", max);

  if ((buf = malloc (max + 1)) == NULL)
    bye ("malloc", 0);

  strcat (buf, argv[0]);

  if (rindex (buf, '/') == NULL)
    strcat (buf, "./");

  for (i = rindex (buf, '/') - buf + 1; i < max; i++)
    buf[i] = 'a';

  buf [i] = 0;

  i = readlink (buf, buf2, sizeof (buf2) - 1);
  if (i != -1)
     bye ("i", i);

  if (errno != ENAMETOOLONG)
    {
      perror (buf);
      bye ("errno", errno);
    }

  printf ("pass\n");
  exit (0);
}
