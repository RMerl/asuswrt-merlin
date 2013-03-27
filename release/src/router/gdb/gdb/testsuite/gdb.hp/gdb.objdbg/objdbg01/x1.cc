#include <stdio.h>
#include "x3.h"

extern void foo();

extern int acomm;

int main3()
{
  return 1;
}

int main2()
{
  return 0;
}

int main()
{
  acomm = 1;
  (new Info<PP>)->p(new PP);
  foo();

  return 0;
}
