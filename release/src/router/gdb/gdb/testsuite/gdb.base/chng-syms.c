/*
 *   Test that GDB cleans up properly after errors that result when a 
 * breakpoint is reset. 
 */

/* VARIABLE is a macro defined on the compiler command line. */

#include <stdlib.h>

int VARIABLE = 42;

void stop_here ()
{
  VARIABLE *= 2;
}

int main ()
{
  stop_here ();
  exit (0);
}

