#include <string.h>

#define ARRSIZE 32
int intarray[ARRSIZE], intarray2[ARRSIZE];

struct teststruct {
  int a;
  int b;
  int c;
  int d;
  int e;
  int f;
  int g;
} intstruct, intstruct2;

void checkpoint1 ()
{
  /* intarray and teststruct have been initialized. */
}

void
zero_all ()
{
  memset ((char *) &intarray,   0, sizeof (intarray));
  memset ((char *) &intarray2,  0, sizeof (intarray2));
  memset ((char *) &intstruct,  0, sizeof (intstruct));
  memset ((char *) &intstruct2, 0, sizeof (intstruct2));
}

main()
{
  int i;

  for (i = 0; i < ARRSIZE; i++)
    intarray[i] = i+1;

  intstruct.a = 12 * 1;
  intstruct.b = 12 * 2;
  intstruct.c = 12 * 3;
  intstruct.d = 12 * 4;
  intstruct.e = 12 * 5;
  intstruct.f = 12 * 6;
  intstruct.g = 12 * 7;

  checkpoint1 ();
}
