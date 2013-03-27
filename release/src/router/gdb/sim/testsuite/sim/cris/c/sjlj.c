/* Check that setjmp and longjmp stand a chance to work; that the used machine
   primitives work in the simulator.  */

#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

extern void f (void);

int ok = 0;
jmp_buf b;

int
main ()
{
  int ret = setjmp (b);

  if (ret == 42)
    ok = 100;
  else if (ret == 0)
    f ();

  if (ok == 100)
    printf ("pass\n");
  else
    printf ("fail\n");
  exit (0);
}

void
f (void)
{
  longjmp (b, 42);
}
