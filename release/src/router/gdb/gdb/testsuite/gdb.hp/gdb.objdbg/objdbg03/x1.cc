#include <stdio.h>

extern int foo();
extern int common3;
extern int data3;
extern int common11;
extern int data11;

int main()
{
  common11 = 11;
  printf("In main: %d %d %d\n", data3, common3, foo(), common11, data11);

  return 0;
}
