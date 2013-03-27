#include <stdio.h>

struct foo_type {
  int t1;
  int t2;
};

static void foo2()
{
  printf("In foo2.\n");
}

void foo1()
{
  printf("In foo1.\n");
  foo2();
}
