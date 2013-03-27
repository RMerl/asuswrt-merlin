/*
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
  if (getcwd ((void *) -1, 4096) != NULL
      || errno != EFAULT)
    abort ();

  printf ("pass\n");
  exit (0);
}
