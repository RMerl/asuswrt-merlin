/* pr 13484 */

#include <stdio.h>

int x;

void bar()
{
  x--;
}

void foo()
{
  x++;
}

int main()
{
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif
  foo();
  bar();
  return 0;
}
