#include <stdio.h>

int common1;
int common2;
int common3;
static int common4;

int data1 = 1;
int data2 = 2;
int data3 = 3;
static int data4 = 4;

int foo()
{
  common1 = 1;
  common2 = 2;
  common3 = 3;
  common4 = 4;

  return data1 + data2 + data3 + data4 + common1 + common2 + common3 + common4;
}
