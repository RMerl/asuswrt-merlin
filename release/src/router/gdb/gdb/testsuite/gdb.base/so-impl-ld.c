/* This program is linked against SOM shared libraries, which the loader
   automatically loads along with the program itself).
   */

#include <stdio.h>

#if defined(__cplusplus) || defined(__STDCPP__)
extern "C" int  solib_main (int  arg);
#else
int  solib_main (int  arg);
#endif

int main ()
{
  int  result;

  /* Call a shlib function. */
  result = solib_main (100);

  /* Call it again. */
  result = solib_main (result);
  return 0;
}
