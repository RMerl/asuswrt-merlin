#include <spe.h>
#include <stdio.h>

#define vector __attribute__((vector_size(8)))


vector int
vector_fun (vector int a, vector int b)
{
  vector int c;
  a = (vector int) __ev_create_s32 (2, 2);
  b = (vector int) __ev_create_s32 (3, 3);

  c = __ev_and (a, b);
  return c;
}
 
int
main ()
{
  vector int y; 
  vector int x; 
  vector int z; 
  int a;

  /* This line may look unnecessary but we do need it, because we want to
     have a line to do a next over (so that gdb refetches the registers)
     and we don't want the code to change any vector registers.
     The splat operations below modify the VRs,
     so we don't want to execute them yet.  */
  a = 9;
  x = (vector int) __ev_create_s32 (-2, -2);
  y = (vector int) __ev_create_s32 (1, 1);
	
  z = vector_fun (x, y);

  return 0;
}
