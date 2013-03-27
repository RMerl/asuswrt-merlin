/* pr 13484 */

#include <stdio.h>

int x;

void foo()
{
  x++;
  printf("This is foo\n");
}

int main()
{
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif
  foo();
  return 0;
}
