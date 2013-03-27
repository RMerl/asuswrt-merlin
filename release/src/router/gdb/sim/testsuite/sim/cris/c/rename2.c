/* Test some execution paths for error cases.
#cc: additional_flags=-Wl,--section-start=.startup=0x8000
   The linker option is for sake of newlib, where the default program
   layout starts at address 0.  We need to change the layout so
   there's no memory at 0, as all sim error checking is "lazy",
   depending on lack of memory mapping.  */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void err (const char *s)
{
  perror (s);
  abort ();
}

int main (int argc, char *argv[])
{
  /* Avoid getting files with random characters due to errors
     elsewhere.  */
  if (argc != 1
      || (argv[0][0] != '.' && argv[0][0] != '/' && argv[0][0] != 'r'))
    abort ();

  if (rename (argv[0], NULL) != -1
      || errno != EFAULT)
    err ("rename 1 ");

  errno = 0;

  if (rename (NULL, argv[0]) != -1
      || errno != EFAULT)
    err ("rename 2");

  printf ("pass\n");
  return 0;
}
