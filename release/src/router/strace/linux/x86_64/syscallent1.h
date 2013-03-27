/* Our second set comes from the i386 files.
   Only a couple of calls we cannot support without the i386 headers.  */

#define sys_oldstat printargs
#define sys_oldfstat printargs
#define sys_oldlstat printargs
#include "i386/syscallent.h"
