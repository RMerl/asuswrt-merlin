/* This program uses HP-UX-specific features to load and unload SOM
   shared libraries that it wasn't linked against (i.e., libraries
   that the loader doesn't automatically load along with the program
   itself).
   */

#include <stdio.h>
#include <dl.h>

int main ()
{
  shl_t  solib_handle;
  int  dummy;
  int  status;
  int  (*solib_main) (int);

  /* Load a shlib, with immediate binding of all symbols.

     Note that the pathname of the loaded shlib is assumed to be relative
     to the testsuite directory (from whence the tested GDB is run), not
     from dot/.
   */
  dummy = 1;  /* Put some code between shl_ calls... */
  solib_handle = shl_load ("gdb.base/solib1.sl", BIND_IMMEDIATE, 0);

  /* Find a function within the shlib, and call it. */
  status = shl_findsym (&solib_handle,
                        "solib_main",
                        TYPE_PROCEDURE,
                        (long *) &solib_main);
  status = (*solib_main) (dummy);

  /* Unload the shlib. */
  status = shl_unload (solib_handle);

  /* Load a different shlib, with deferred binding of all symbols. */
  dummy = 2;
  solib_handle = shl_load ("gdb.base/solib2.sl", BIND_DEFERRED, 0);

  /* Find a function within the shlib, and call it. */
  status = shl_findsym (&solib_handle,
                        "solib_main",
                        TYPE_PROCEDURE,
                        (long *) &solib_main);
  status = (*solib_main) (dummy);

  /* Unload the shlib. */
  status = shl_unload (solib_handle);

  /* Reload the first shlib again, with deferred symbol binding this time. */
  dummy = 3;
  solib_handle = shl_load ("gdb.base/solib1.sl", BIND_IMMEDIATE, 0);

  /* Unload it without trying to find any symbols in it. */
  status = shl_unload (solib_handle);

  /* All done. */
  dummy = -1;
  return 0;
}
