/* This program is linked against SOM shared libraries, which the loader
   automatically loads along with the program itself).
   */

#include <stdio.h>
#ifdef PROTOTYPES
extern "C" int solib_main (int);

static int
solib_wrapper (int (*function)(int))
#else
extern int solib_main (int);

static int
solib_wrapper (function)
  int (*function)(int);
#endif
{
  return (*function)(100);
}


int main ()
{
  int  result;

  /* This is an indirect call to solib_main. */
  result = solib_wrapper (solib_main);
  return 0;
}
