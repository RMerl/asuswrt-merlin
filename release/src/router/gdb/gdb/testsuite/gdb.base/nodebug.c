#include <stdlib.h>
/* Test that things still (sort of) work when compiled without -g.  */

int dataglobal = 3;			/* Should go in global data */
static int datalocal = 4;		/* Should go in local data */
int bssglobal;				/* Should go in global bss */
static int bsslocal;			/* Should go in local bss */

#ifdef PROTOTYPES
int
inner (int x)
#else
int
inner (x)
     int x;
#endif
{
  return x + dataglobal + datalocal + bssglobal + bsslocal;
}

#ifdef PROTOTYPES
static short
middle (int x)
#else
static short
middle (x)
     int x;
#endif
{
  return 2 * inner (x);
}

#ifdef PROTOTYPES
short
top (int x)
#else
short
top (x)
     int x;
#endif
{
  return 2 * middle (x);
}

#ifdef PROTOTYPES
int
main (int argc, char **argv)
#else
int 
main (argc, argv)
     int argc;
     char **argv;
#endif
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  return top (argc);
}

int *x;

#ifdef PROTOTYPES
int array_index (char *arr, int i)
#else
int
array_index (arr, i)
     char *arr;
     int i;
#endif
{
  /* The basic concept is just "return arr[i];".  But call malloc so that gdb
     will be able to call functions.  */
  char retval;
  x = (int *) malloc (sizeof (int));
  *x = i;
  retval = arr[*x];
  free (x);
  return retval;
}
