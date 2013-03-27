#include <stdio.h>

int
main (int argc, char **argv)
{
  int i = 0;
  printf ("%d\n", argc);
  while (i < argc)
    printf ("%s\n", argv[i++]);

  return 0; /* set breakpoint here */
}
