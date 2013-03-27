#include <stdio.h>

extern void foo1();
extern void foo3();

struct foo_type;

int main()
{
  struct foo_type *x;

  printf("In main.\n");
  foo1();
  foo3();
  return 0;
}
