#include <stdio.h>
#include "x.h"

template class Adder<int>;

int main()
{
  Adder<int> add;

  add.set(3);
  add.add(3);
  printf("In main: %d\n", add.get());

  return 0;
}
