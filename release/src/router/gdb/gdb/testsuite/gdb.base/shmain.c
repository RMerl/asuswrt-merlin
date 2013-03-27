/* A test */

#include "ss.h"
#include <stdio.h>

extern int structarg(struct s);
extern int pstructarg(struct s*);
extern int shr1(int);
extern int shr2(int);
extern float sg;

int eglob;

struct {
 int a;  
 int b;
} s;

int g;

#ifdef PROTOTYPES
int local_structarg(struct s x)
#else
int local_structarg(x)
struct s x;
#endif
{
  return x.b;
}

#ifdef PROTOTYPES
int mainshr1(int g)
#else
int mainshr1(g)
int g;
#endif
{
  return 2*g;
}

int main()
{
  struct s y;
  g = 1;
  g = shr1(g);
  g = shr2(g);
  g = mainshr1(g);
  sg = 1.1;
  printf("address of sg is 0x%x\n", &sg);
  y.a = 3;
  y.b = 4;
  g = local_structarg(y);
  g = structarg(y);
  g = pstructarg(&y);
  return 0;
}
