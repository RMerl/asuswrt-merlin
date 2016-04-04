#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#include <malloc.h>
#endif

static void minfo(void)
{
#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
  struct mallinfo mi;
  mi = mallinfo();
  printf(" fordblks = %d\n", mi.fordblks);
  malloc_stats();
  printf("\n");
#endif
}

int
main (void)
{
  char *p;

  minfo();
  p = malloc(10);
  free(p);
  minfo();

  return EXIT_SUCCESS;
}

/* vim:ts=2:sw=2:et:
 */
