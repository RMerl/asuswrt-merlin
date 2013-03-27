/*
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
  char buf[1024];
  /* This depends on the test-setup, but it's unlikely that the program
     is passed as a symlink, so supposedly safe.   */
  if (readlink(argv[0], buf, sizeof (buf)) != -1 || errno != EINVAL)
    abort ();

  printf ("pass\n");
  exit (0);
}
