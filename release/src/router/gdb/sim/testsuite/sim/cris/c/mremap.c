#include <stdio.h>
#include <stdlib.h>

/* Sanity check that system calls for realloc works.  Also tests a few
   more cases for mmap2 and munmap.  */

int main ()
{
  void *p1, *p2;

  if ((p1 = malloc (8100)) == NULL
      || (p1 = realloc (p1, 16300)) == NULL
      || (p1 = realloc (p1, 4000)) == NULL
      || (p1 = realloc (p1, 500)) == NULL
      || (p1 = realloc (p1, 1023*1024)) == NULL
      || (p1 = realloc (p1, 8191*1024)) == NULL
      || (p1 = realloc (p1, 512*1024)) == NULL
      || (p2 = malloc (1023*1024)) == NULL
      || (p1 = realloc (p1, 1023*1024)) == NULL
      || (p1 = realloc (p1, 8191*1024)) == NULL
      || (p1 = realloc (p1, 512*1024)) == NULL)
  {
    printf ("fail\n");
    exit (1);
  }

  free (p1);
  free (p2);
  printf ("pass\n");
  exit (0);
}
