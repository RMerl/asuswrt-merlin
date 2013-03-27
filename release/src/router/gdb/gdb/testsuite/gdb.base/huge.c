/*
 * Test GDB's ability to read a very large data object from target memory.
 */

#include <string.h>

/* A value that will produce a target data object large enough to
   crash GDB.  0x200000 is big enough on GNU/Linux, other systems may
   need a larger number.  */
#ifndef CRASH_GDB
#define CRASH_GDB 0x200000
#endif
static int a[CRASH_GDB], b[CRASH_GDB];

main()
{
  memcpy (a, b, sizeof (a));
  return 0;
}
