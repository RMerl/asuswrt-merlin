#include "ss.h"
#include <stdio.h>

typedef float f;

float sg = 5.5;
int sgi = 2;
static int sgs = 7;

#ifdef PROTOTYPES
int shr1(int x)
#else
int shr1(x)
int x;
#endif
{
  f mumble;
  int l;
  l = 1;
  {
    int l;
    l = 2;
  }
  mumble = 7.7;
  sg = 6.6;
  sgi++;
  sgs = 8;
  printf("address of sgs is 0x%x\n", &sgs);
  return 2*x;
}

#ifdef PROTOTYPES
static int shr1_local(int x)
#else
static int shr1_local(x)
int x;
#endif
{
  return 2*x;
}

#ifdef PROTOTYPES
int structarg(struct s x)
#else
int structarg(x)
struct s x;
#endif
{
  return x.a;
}

#ifdef PROTOTYPES
int pstructarg(struct s *x)
#else
int pstructarg(x)
struct s *x;
#endif
{
  return x->a;
}



