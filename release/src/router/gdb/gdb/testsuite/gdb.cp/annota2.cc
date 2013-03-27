#include <stdio.h>

class A {
public:
  int x;
  int y;
  int foo (int arg);
};


int A::foo (int arg)
{
  x += arg;
  return arg *2;
}

int main()
{
  A a;

  a.x = 0;
  a.x = 1;
  a.y = 2;

  printf ("a.x is %d\n", a.x);
  return 0;
}

